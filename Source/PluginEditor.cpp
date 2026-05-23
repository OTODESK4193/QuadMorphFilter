// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/ModelCapabilities.h"

QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(
    QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);

    setupFilterGroup(groupA, "A", "Filter A");
    setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C");
    setupFilterGroup(groupD, "D", "Filter D");

    setupLfoGroup(lfos[0], 1, "LFO 1 (Morph)");
    setupLfoGroup(lfos[1], 2, "LFO 2 (Cutoff)");
    setupLfoGroup(lfos[2], 3, "LFO 3 (Reso)");

    auto setupMaster = [&](juce::Label& l, juce::Slider& sl, juce::String txt,
        juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff2980B9));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id, sl);
        };
    setupMaster(masterGainLabel, masterGainSlider, "Out Gain", "masterGain", mgAtt);
    setupMaster(dryWetLabel, dryWetSlider, "Dry/Wet", "dryWet", dwAtt);
    setupMaster(ceilingLabel, ceilingSlider, "Limit Ceil", "limiterCeiling", clAtt);

    xyModeLabel.setText("Mode", juce::dontSendNotification);
    xyModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(xyModeLabel);
    xyModeCombo.addItemList({ "Morph", "Cutoff" }, 1);
    addAndMakeVisible(xyModeCombo);
    xyModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "xyMode", xyModeCombo);

    osModeLabel.setText("OS", juce::dontSendNotification);
    osModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(osModeLabel);
    osModeCombo.addItemList({ "Off", "Auto", "2x", "4x" }, 1);
    addAndMakeVisible(osModeCombo);
    osModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "osMode", osModeCombo);

    lfoCutLabel.setText("LFO Cut", juce::dontSendNotification);
    lfoCutLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoCutLabel);

    lfoResLabel.setText("LFO Res", juce::dontSendNotification);
    lfoResLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoResLabel);

    const juce::String filterNames[4] = { "A", "B", "C", "D" };
    const juce::String cutIds[4] = { "lfoCutA", "lfoCutB", "lfoCutC", "lfoCutD" };
    const juce::String resIds[4] = { "lfoResA", "lfoResB", "lfoResC", "lfoResD" };

    for (int i = 0; i < 4; ++i)
    {
        lfoCutBtn[i].setButtonText(filterNames[i]);
        lfoCutBtn[i].setClickingTogglesState(true);
        lfoCutBtn[i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xffC2185B));
        addAndMakeVisible(lfoCutBtn[i]);
        lfoCutAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, cutIds[i], lfoCutBtn[i]);

        lfoResBtn[i].setButtonText(filterNames[i]);
        lfoResBtn[i].setClickingTogglesState(true);
        lfoResBtn[i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xffE65100));
        addAndMakeVisible(lfoResBtn[i]);
        lfoResAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, resIds[i], lfoResBtn[i]);
    }

    setSize(1000, 680);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffF0F4F8));
}

void QuadMorphFilterAudioProcessorEditor::refreshFilterGroupControls(
    FilterGroup& g, const juce::String& suffix, int modelIdx)
{
    auto [maxSlope, hasLP, hasBP, hasHP, hasNotch] = getModelCaps(modelIdx);

    if (modelIdx == 2)
    {
        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("Accent: Off", 1);
        g.slope.addItem("Accent: Low", 2);
        g.slope.addItem("Accent: High", 3);

        if (g.slope.getSelectedId() < 1 || g.slope.getSelectedId() > 3)
            g.slope.setSelectedId(1, juce::sendNotification);
    }
    else
    {
        if (g.slope.getNumItems() < 4 ||
            g.slope.getItemText(0) == "Accent: Off")
        {
            g.slope.clear(juce::dontSendNotification);
            g.slope.addItem("12dB", 1);
            g.slope.addItem("24dB", 2);
            g.slope.addItem("48dB", 3);
            g.slope.addItem("96dB", 4);

            // ✅ 【修正】デフォルト値を 12dB（ID=1）に設定
            g.slope.setSelectedId(1, juce::sendNotification);
        }

        for (int i = 1; i <= 4; ++i)
            g.slope.setItemEnabled(i, (i - 1) <= maxSlope);

        int curSlope = g.slope.getSelectedId();
        if (curSlope - 1 > maxSlope)
        {
            g.slope.setSelectedId(maxSlope + 1, juce::sendNotification);
            if (auto* p = audioProcessor.apvts.getParameter("slope" + suffix))
                p->setValueNotifyingHost(p->convertTo0to1((float)maxSlope));
        }
    }

    g.type.setItemEnabled(1, hasLP);
    g.type.setItemEnabled(2, hasBP);
    g.type.setItemEnabled(3, hasHP);
    g.type.setItemEnabled(4, hasNotch);

    int curType = g.type.getSelectedId();
    bool typeOk = (curType == 1 && hasLP)
        || (curType == 2 && hasBP)
        || (curType == 3 && hasHP)
        || (curType == 4 && hasNotch);

    if (!typeOk)
    {
        int fallback = hasLP ? 1 : hasBP ? 2 : hasHP ? 3 : 4;
        g.type.setSelectedId(fallback, juce::sendNotification);
        if (auto* p = audioProcessor.apvts.getParameter("type" + suffix))
            p->setValueNotifyingHost(p->convertTo0to1((float)(fallback - 1)));
    }

    if (modelIdx == 4)
    {
        g.cutoffLabel.setText("SRR", juce::dontSendNotification);
        g.resLabel.setText("Bits", juce::dontSendNotification);
    }
    else
    {
        g.cutoffLabel.setText("Cut", juce::dontSendNotification);
        g.resLabel.setText("Res", juce::dontSendNotification);
    }
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g,
    juce::String s,
    juce::String name)
{
    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00ACC1));
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "enable" + s, g.enableButton);

    g.model.addItemList({
        "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR",
        "Formant (Vowel)", "Comb Filter", "MS-20 (Screaming)", "All-Pass Phaser", "Wavefolder",
        "Reverb (Metallic)", "Kilo All-Pass",
        "Prophet (Curtis)", "SSM 2040", "CS-80 (Yamaha)", "Jupiter (Roland)", "EDP Wasp (CMOS)",
        "Butterworth (Flat)", "Chebyshev (Ripple)", "Bessel (Phase)", "Elliptic (Notch)",
        "Vactrol LPG", "Modal Resonator", "Waveguide Mesh", "Bode Freq Shifter",
        "Z-Plane (2D Morph)", "Phased Array", "Nyquist Anti-alias"
        }, 1);
    addAndMakeVisible(g.model);
    g.mAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "model" + s, g.model);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "type" + s, g.type);

    g.slope.addItemList({ "12dB", "24dB", "48dB", "96dB" }, 1);
    addAndMakeVisible(g.slope);
    g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "slope" + s, g.slope);

    auto setup = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 55, 18);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ACC1));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id + s, sl);
        };
    setup(g.cutoffLabel, g.cutoff, "Cut", "cutoff", g.cAtt);
    setup(g.resLabel, g.res, "Res", "res", g.rAtt);

    g.model.onChange = [this, &g, s]()
        {
            int modelIdx = g.model.getSelectedId() - 1;
            refreshFilterGroupControls(g, s, modelIdx);
        };

    int initialModel = (int)audioProcessor.apvts.getRawParameterValue("model" + s)->load();
    refreshFilterGroupControls(g, s, initialModel);
}

void QuadMorphFilterAudioProcessorEditor::setupLfoGroup(LfoGroup& g, int idx, juce::String name)
{
    juce::String id = "lfo" + juce::String(idx);
    juce::Colour lfoCols[] = {
        juce::Colour(0xff00D2D3),
        juce::Colour(0xffFF9FF3),
        juce::Colour(0xffFEECA1)
    };

    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "en", g.enableButton);

    g.wave.addItemList({
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard",
        "Polygon", "Attractor Orbit"
        }, 1);
    addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "wave", g.wave);

    g.boundCombo.addItemList({ "Clip", "Bounce", "Wrap" }, 1);
    addAndMakeVisible(g.boundCombo);
    g.bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "bound", g.boundCombo);

    g.stepMode.setButtonText("Step");
    g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync");
    g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    g.syncToggle.onClick = [this] { resized(); };
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "sync", g.syncToggle);

    g.rateSync.addItemList({
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
        }, 1);
    addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "rateSync", g.rateSync);

    auto setupSlider = [&](juce::Slider& sl,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
        juce::String paramId)
        {
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
            sl.setColour(juce::Slider::thumbColourId, lfoCols[idx - 1]);
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, paramId, sl);
        };
    setupSlider(g.rateFree, g.rfAtt, id + "rateFree");
    setupSlider(g.minSlider, g.minAtt, id + "min");
    setupSlider(g.maxSlider, g.maxAtt, id + "max");
}

void QuadMorphFilterAudioProcessorEditor::resized()
{
    const int pad = 15;
    auto b = getLocalBounds().reduced(pad);
    const int totalW = b.getWidth();

    const int visW = (int)(totalW * 0.7f);
    const int rightX = b.getX() + visW;
    const int rightW = b.getRight() - rightX;

    {
        auto top = b.removeFromTop(300);
        visualizer.setBounds(top.removeFromLeft(visW).reduced(5));
        xyPad.setBounds(top.reduced(5));
    }
    b.removeFromTop(4);

    {
        int ctrlY = b.getY();
        const int rowH = 24;
        const int gap = 2;

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            auto modeHalf = area.removeFromLeft(area.getWidth() / 2 - 4);
            xyModeLabel.setBounds(modeHalf.removeFromLeft(42).reduced(0, 1));
            xyModeCombo.setBounds(modeHalf);
            area.removeFromLeft(8);
            osModeLabel.setBounds(area.removeFromLeft(24).reduced(0, 1));
            osModeCombo.setBounds(area);
            ctrlY += rowH + gap;
        }

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            lfoCutLabel.setBounds(area.removeFromLeft(52).reduced(0, 1));
            int bw = area.getWidth() / 4;
            for (int i = 0; i < 4; ++i)
                lfoCutBtn[i].setBounds(area.removeFromLeft(bw).reduced(2, 1));
            ctrlY += rowH + gap;
        }

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            lfoResLabel.setBounds(area.removeFromLeft(52).reduced(0, 1));
            int bw = area.getWidth() / 4;
            for (int i = 0; i < 4; ++i)
                lfoResBtn[i].setBounds(area.removeFromLeft(bw).reduced(2, 1));
        }
    }

    const int sliderW = 155;
    for (auto* g : { &groupA, &groupB, &groupC, &groupD })
    {
        auto r = b.removeFromTop(28).reduced(5, 2);

        g->enableButton.setBounds(r.removeFromLeft(60).reduced(0, 2));
        g->model.setBounds(r.removeFromLeft(115).reduced(2, 2));
        g->type.setBounds(r.removeFromLeft(60).reduced(2, 2));
        g->slope.setBounds(r.removeFromLeft(85).reduced(2, 2));

        auto cutArea = r.removeFromLeft(sliderW).reduced(2, 0);
        g->cutoffLabel.setBounds(cutArea.removeFromLeft(28));
        g->cutoff.setBounds(cutArea);

        r.removeFromLeft(6);

        auto resArea = r.removeFromLeft(sliderW).reduced(2, 0);
        g->resLabel.setBounds(resArea.removeFromLeft(28));
        g->res.setBounds(resArea);
    }
    b.removeFromTop(6);

    for (int i = 0; i < 3; ++i)
    {
        auto r = b.removeFromTop(28).reduced(5, 2);

        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 2));
        lfos[i].wave.setBounds(r.removeFromLeft(120).withSizeKeepingCentre(115, 20));
        lfos[i].boundCombo.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 20));
        lfos[i].stepMode.setBounds(r.removeFromLeft(50).reduced(2, 2));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(50).reduced(2, 2));

        auto remainW = r.getWidth();
        auto rateArea = r.removeFromLeft(remainW / 3);
        auto minArea = r.removeFromLeft(remainW / 3);
        auto maxArea = r;

        bool isSynced = audioProcessor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "sync")->load() > 0.5f;
        if (isSynced) {
            lfos[i].rateSync.setBounds(
                rateArea.withSizeKeepingCentre(rateArea.getWidth() - 5, 20));
            lfos[i].rateFree.setVisible(false);
            lfos[i].rateSync.setVisible(true);
        }
        else {
            lfos[i].rateFree.setBounds(rateArea.reduced(2, 5));
            lfos[i].rateFree.setVisible(true);
            lfos[i].rateSync.setVisible(false);
        }
        lfos[i].minSlider.setBounds(minArea.reduced(2, 5));
        lfos[i].maxSlider.setBounds(maxArea.reduced(2, 5));
    }

    b.removeFromTop(10);

    auto masterArea = b.removeFromTop(28).reduced(5, 2);
    auto cellW = masterArea.getWidth() / 3;

    auto gainRect = masterArea.removeFromLeft(cellW);
    masterGainLabel.setBounds(gainRect.removeFromLeft(60));
    masterGainSlider.setBounds(gainRect);

    auto dwRect = masterArea.removeFromLeft(cellW);
    dryWetLabel.setBounds(dwRect.removeFromLeft(60));
    dryWetSlider.setBounds(dwRect);

    ceilingLabel.setBounds(masterArea.removeFromLeft(60));
    ceilingSlider.setBounds(masterArea);
}