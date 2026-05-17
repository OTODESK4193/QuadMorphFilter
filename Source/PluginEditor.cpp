// ==========================================
// PluginEditor.cpp (軽量化版)
// UI の実装は UI/ 各ファイルに移動済み
// ここは Editor の構築・レイアウトのみ
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

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

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g,
    juce::String s,
    juce::String name)
{
    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00D2D3));
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
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00D2D3));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id + s, sl);
        };
    setup(g.cutoffLabel, g.cutoff, "Cut", "cutoff", g.cAtt);
    setup(g.resLabel, g.res, "Res", "res", g.rAtt);
}

void QuadMorphFilterAudioProcessorEditor::setupLfoGroup(LfoGroup& g,
    int idx,
    juce::String name)
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
    auto b = getLocalBounds().reduced(15);
    auto top = b.removeFromTop(300);

    visualizer.setBounds(top.removeFromLeft(top.getWidth() * 0.7f).reduced(5));
    xyPad.setBounds(top.reduced(5));

    b.removeFromTop(10);

    for (auto* g : { &groupA, &groupB, &groupC, &groupD })
    {
        auto r = b.removeFromTop(38).reduced(5, 2);
        g->enableButton.setBounds(r.removeFromLeft(60).reduced(0, 5));
        g->model.setBounds(r.removeFromLeft(115).reduced(2, 5));
        g->type.setBounds(r.removeFromLeft(60).reduced(2, 5));
        g->slope.setBounds(r.removeFromLeft(85).reduced(2, 5));
        auto cutArea = r.removeFromLeft(r.getWidth() / 2).reduced(5, 0);
        auto resArea = r.reduced(5, 0);
        g->cutoffLabel.setBounds(cutArea.removeFromLeft(30));
        g->cutoff.setBounds(cutArea);
        g->resLabel.setBounds(resArea.removeFromLeft(30));
        g->res.setBounds(resArea);
    }

    b.removeFromTop(10);

    for (int i = 0; i < 3; ++i)
    {
        auto r = b.removeFromTop(38).reduced(5, 2);
        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));
        lfos[i].wave.setBounds(r.removeFromLeft(120).withSizeKeepingCentre(115, 22));
        lfos[i].boundCombo.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 22));
        lfos[i].stepMode.setBounds(r.removeFromLeft(50).reduced(2, 5));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(50).reduced(2, 5));

        auto remainingWidth = r.getWidth();
        auto rateArea = r.removeFromLeft(remainingWidth / 3);
        auto minArea = r.removeFromLeft(remainingWidth / 3);
        auto maxArea = r;

        bool isSynced = audioProcessor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "sync")->load() > 0.5f;
        if (isSynced) {
            lfos[i].rateSync.setBounds(rateArea.withSizeKeepingCentre(
                rateArea.getWidth() - 5, 22));
            lfos[i].rateFree.setVisible(false);
            lfos[i].rateSync.setVisible(true);
        }
        else {
            lfos[i].rateFree.setBounds(rateArea.reduced(2, 8));
            lfos[i].rateFree.setVisible(true);
            lfos[i].rateSync.setVisible(false);
        }
        lfos[i].minSlider.setBounds(minArea.reduced(2, 8));
        lfos[i].maxSlider.setBounds(maxArea.reduced(2, 8));
    }

    b.removeFromTop(15);
    auto masterArea = b.removeFromTop(38).reduced(5, 2);
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