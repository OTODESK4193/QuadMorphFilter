// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizer: 3Ç¬ÇÃLFOåãâ ÇìùçáÇµÇƒï`âÊ ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));

    auto w = (float)getWidth();
    auto h = (float)getHeight();

    // Åyí«â¡Åzé¸îgêîÉOÉäÉbÉhÅi100, 500, 1k, 5k, 10k HzÅjÇÃï`âÊ
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.setFont(10.0f);
    float freqs[] = { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };
    juce::String labels[] = { "100Hz", "500Hz", "1kHz", "5kHz", "10kHz" };

    for (int i = 0; i < 5; ++i) {
        // ëŒêîÉXÉPÅ[ÉãÇÃXç¿ïWãtéZ: x = w * log10(freq / 20.0) / 3.0
        float x = w * std::log10(freqs[i] / 20.0f) / 3.0f;
        g.drawVerticalLine((int)x, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawText(labels[i], (int)x + 2, (int)h - 15, 40, 10, juce::Justification::left);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
    }

    g.setColour(juce::Colours::cyan);

    auto mPos = processor.getLfoPos(0); // Morph LFO
    auto cPos = processor.getLfoPos(1); // Cutoff LFO
    auto rPos = processor.getLfoPos(2); // Reso LFO

    float dx = cPos.x - 0.5f; float dy = cPos.y - 0.5f;
    float cutoffMod = std::sqrt(dx * dx + dy * dy) / 0.707f;
    float rx = rPos.x - 0.5f; float ry = rPos.y - 0.5f;
    float resMod = std::sqrt(rx * rx + ry * ry) / 0.707f;

    juce::Path path;

    for (int px = 0; px <= (int)w; ++px) {
        float freq = 20.0f * std::pow(1000.0f, px / w);
        auto calc = [&](juce::String s) {
            if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f;
            float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load() * (1.0f + cutoffMod * 2.0f);
            float res = processor.apvts.getRawParameterValue("res" + s)->load() * (1.0f + resMod * 3.0f);
            int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
            float r = freq / juce::jlimit(20.0f, 20000.0f, fc);
            float r2 = r * r; float d = 1.0f / juce::jlimit(0.1f, 10.0f, res);
            float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
            float mag = 1.0f / den;
            if (t == 1) mag *= r; if (t == 2) mag *= r2; if (t == 3) mag *= std::abs(1.0f - r2);
            return mag;
            };
        float x = mPos.x; float y = mPos.y;
        float mag = (calc("A") * (1 - x) * (1 - y)) + (calc("B") * x * (1 - y)) + (calc("C") * (1 - x) * y) + (calc("D") * x * y);
        float yPos = h - (std::log10(mag + 1.0f) * 1.5f * h * 0.4f + h * 0.1f);
        if (px == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)px, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

// --- XYPad: 3Ç¬ÇÃÉhÉbÉgÇêFï™ÇØÇµÇƒï`âÊ ---
void XYPadComponent::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.fillRoundedRectangle(b, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.2f)); g.drawRoundedRectangle(b, 10.0f, 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.4f)); g.setFont(14.0f);
    g.drawText("A", 10, 10, 20, 20, juce::Justification::centred);
    g.drawText("B", getWidth() - 30, 10, 20, 20, juce::Justification::centred);
    g.drawText("C", 10, getHeight() - 30, 20, 20, juce::Justification::centred);
    g.drawText("D", getWidth() - 30, getHeight() - 30, 20, 20, juce::Justification::centred);

    juce::Colour colors[] = { juce::Colours::cyan, juce::Colours::magenta, juce::Colours::yellow };
    for (int i = 0; i < 3; ++i) {
        auto p = processor.getLfoPos(i);
        g.setColour(colors[i]);
        g.fillEllipse(p.x * getWidth() - 6, p.y * getHeight() - 6, 12, 12);
        g.setColour(juce::Colours::white);
        g.drawEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16, 1.0f);
    }
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e) {
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
}

// --- Editorñ{ëÃ ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    addAndMakeVisible(visualizer); addAndMakeVisible(xyPad);
    setupFilterGroup(groupA, "A", "Filter A"); setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C"); setupFilterGroup(groupD, "D", "Filter D");
    setupLfoGroup(lfos[0], 1, "LFO 1 (Morph)"); setupLfoGroup(lfos[1], 2, "LFO 2 (Cutoff)"); setupLfoGroup(lfos[2], 3, "LFO 3 (Reso)");

    setSize(1000, 850);
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String name) {
    g.enableButton.setButtonText(name); g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colours::cyan);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "enable" + s, g.enableButton);
    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1); addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);

    auto setup = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(txt, juce::dontSendNotification); addAndMakeVisible(l);
        sl.setSliderStyle(juce::Slider::LinearHorizontal); sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
        addAndMakeVisible(sl); att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + s, sl);
        };
    setup(g.cutoffLabel, g.cutoff, "Cut", "cutoff", g.cAtt); setup(g.resLabel, g.res, "Res", "res", g.rAtt);
}

void QuadMorphFilterAudioProcessorEditor::setupLfoGroup(LfoGroup& g, int idx, juce::String name) {
    juce::String id = "lfo" + juce::String(idx);
    juce::Colour lfoCols[] = { juce::Colours::cyan, juce::Colours::magenta, juce::Colours::yellow };

    g.enableButton.setButtonText(name); g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "en", g.enableButton);

    g.wave.addItemList({ "Sine", "SAW", "Pulse", "Random", "Noise" }, 1); addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "wave", g.wave);

    // ÅyèCê≥ÅzTextButtonÇ…ÇÊÇÈè¡ìîéÆÉgÉOÉãê›íË
    g.stepMode.setButtonText("Step"); g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync"); g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    g.syncToggle.onClick = [this] { resized(); }; // SyncêÿÇËë÷Ç¶éûÇ…ÉåÉCÉAÉEÉgÇçƒåvéZ
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "sync", g.syncToggle);

    g.rateSync.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D", "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T" }, 1);
    addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "rateSync", g.rateSync);

    g.rateFree.setSliderStyle(juce::Slider::LinearHorizontal); g.rateFree.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.rateFree);
    g.rfAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "rateFree", g.rateFree);

    g.amt.setSliderStyle(juce::Slider::LinearHorizontal); g.amt.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.amt);
    g.aAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "amt", g.amt);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkgrey.darker(0.9f)); }

void QuadMorphFilterAudioProcessorEditor::resized() {
    auto b = getLocalBounds().reduced(15);
    auto top = b.removeFromTop(300);
    visualizer.setBounds(top.removeFromLeft(top.getWidth() * 0.7f).reduced(5));
    xyPad.setBounds(top.reduced(5));

    b.removeFromTop(10);
    for (auto* g : { &groupA, &groupB, &groupC, &groupD }) {
        auto r = b.removeFromTop(40).reduced(5, 2);
        g->enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));
        g->type.setBounds(r.removeFromLeft(80).withSizeKeepingCentre(70, 24));
        auto cA = r.removeFromLeft(r.getWidth() / 2).reduced(10, 0);
        g->cutoffLabel.setBounds(cA.removeFromLeft(35)); g->cutoff.setBounds(cA);
        g->resLabel.setBounds(r.removeFromLeft(35)); g->res.setBounds(r);
    }

    b.removeFromTop(15);
    for (int i = 0; i < 3; ++i) {
        auto r = b.removeFromTop(40).reduced(5, 2);
        lfos[i].enableButton.setBounds(r.removeFromLeft(120).reduced(0, 5));
        lfos[i].wave.setBounds(r.removeFromLeft(80).withSizeKeepingCentre(75, 22));

        // ÅyèCê≥ÅzTextButtonÇÃó]îíí≤êÆ
        lfos[i].stepMode.setBounds(r.removeFromLeft(55).reduced(2, 5));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(55).reduced(2, 5));

        // ÅyèCê≥ÅzRate Free/Sync Ç∆ Amount ÇécÇËÇÃïùÇ≈50:50Ç…ï™äÑîzíu
        auto remainingWidth = r.getWidth();
        auto rateArea = r.removeFromLeft(remainingWidth / 2);
        auto amtArea = r; // écÇËÇÃîºï™

        if (audioProcessor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "sync")->load() > 0.5f) {
            lfos[i].rateSync.setBounds(rateArea.withSizeKeepingCentre(rateArea.getWidth() - 10, 22));
            lfos[i].rateFree.setVisible(false); lfos[i].rateSync.setVisible(true);
        }
        else {
            lfos[i].rateFree.setBounds(rateArea.reduced(5, 8));
            lfos[i].rateFree.setVisible(true); lfos[i].rateSync.setVisible(false);
        }
        lfos[i].amt.setBounds(amtArea.reduced(5, 8));
    }
}