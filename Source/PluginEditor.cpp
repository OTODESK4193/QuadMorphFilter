// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.setFont(10.0f);
    float freqs[] = { 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f };
    juce::String labels[] = { "100Hz", "500Hz", "1kHz", "5kHz", "10kHz" };

    for (int i = 0; i < 5; ++i) {
        float x = w * std::log10(freqs[i] / 20.0f) / 3.0f;
        g.drawVerticalLine((int)x, 0.0f, h);
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawText(labels[i], (int)x + 2, (int)h - 15, 40, 10, juce::Justification::left);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
    }

    g.setColour(juce::Colours::cyan);
    auto mPos = processor.getLfoPos(0);
    auto cPos = processor.getLfoPos(1);
    auto rPos = processor.getLfoPos(2);

    float dx = cPos.x - 0.5f; float dy = cPos.y - 0.5f;
    float cutoffMod = std::sqrt(dx * dx + dy * dy) / 0.707f;
    float rx = rPos.x - 0.5f; float ry = rPos.y - 0.5f;
    float resMod = std::sqrt(rx * rx + ry * ry) / 0.707f;

    bool lfo1_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo2wave")->load() == 3) && (processor.apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo3wave")->load() == 3) && (processor.apvts.getRawParameterValue("lfo3en")->load() > 0.5f);

    auto lfo1Mod4 = processor.getLfoMod4(1);
    auto lfo2Mod4 = processor.getLfoMod4(2);

    juce::Path path;
    for (int px = 0; px <= (int)w; ++px) {
        float freq = 20.0f * std::pow(1000.0f, px / w);

        auto calc = [&](juce::String s, int idx) -> float {
            if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f;

            float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load() * (1.0f + cutoffMod * 2.0f);
            if (lfo1_isRand1) fc = 20.0f * std::pow(1000.0f, lfo1Mod4[idx]);

            float res = processor.apvts.getRawParameterValue("res" + s)->load() * (1.0f + resMod * 3.0f);
            if (lfo2_isRand1) res = 0.1f * std::pow(100.0f, lfo2Mod4[idx]);

            int slopeIdx = (int)processor.apvts.getRawParameterValue("slope" + s)->load();
            int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;

            float adjustedRes = res;
            if (stages > 1) adjustedRes = res * std::pow(0.6f, std::log2((float)stages));

            int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
            float r = freq / juce::jlimit(20.0f, 20000.0f, fc);
            float r2 = r * r; float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
            float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
            float mag = 1.0f / den;

            if (t == 1) mag *= r; if (t == 2) mag *= r2; if (t == 3) mag *= std::abs(1.0f - r2);
            return static_cast<float>(std::pow((double)mag, (double)stages));
            };

        float x = mPos.x; float y = mPos.y;
        float mag = (calc("A", 0) * (1 - x) * (1 - y)) + (calc("B", 1) * x * (1 - y)) + (calc("C", 2) * (1 - x) * y) + (calc("D", 3) * x * y);
        float yPos = h - (std::log10(mag + 1.0f) * 1.5f * h * 0.4f + h * 0.1f);
        if (px == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)px, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

XYPadComponent::XYPadComponent(QuadMorphFilterAudioProcessor& p) : processor(p) {
    for (int i = 0; i < 3; ++i) trails[i].fill({ 0.5f, 0.5f });
    startTimerHz(60);
}

void XYPadComponent::timerCallback() {
    // Åyí«â¡ÅzVisual TrailsÇÃécëúãLò^
    for (int i = 0; i < 3; ++i) {
        trails[i][trailIdx[i]] = processor.getLfoPos(i);
        trailIdx[i] = (trailIdx[i] + 1) % 30;
    }
    repaint();
}

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
        if (processor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "en")->load() < 0.5f) continue;

        // Åyí«â¡ÅzVisual TrailsÇÃï`âÊ
        for (int t = 0; t < 30; ++t) {
            int idx = (trailIdx[i] + t) % 30;
            auto pt = trails[i][idx];
            float alpha = (float)t / 30.0f * 0.4f;
            g.setColour(colors[i].withAlpha(alpha));
            g.fillEllipse(pt.x * getWidth() - 3, pt.y * getHeight() - 3, 6, 6);
        }

        auto p = processor.getLfoPos(i);
        bool isWait = processor.isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = processor.isRecordingDrag[i].load(std::memory_order_acquire);

        if (isWait) {
            if (isDrag) {
                g.setColour(colors[i].brighter(0.8f));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
            else {
                float alpha = 0.3f + 0.7f * std::abs(std::sin(juce::Time::getMillisecondCounter() * 0.005f));
                g.setColour(colors[i].withAlpha(alpha));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
        }
        else {
            g.setColour(colors[i]);
            g.fillEllipse(p.x * getWidth() - 6, p.y * getHeight() - 6, 12, 12);
        }
        g.setColour(juce::Colours::white);
        g.drawEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16, 1.0f);
    }
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                processor.isWaitingForRecord[i].store(false, std::memory_order_release);
                processor.isRecordingDrag[i].store(false, std::memory_order_release);
                draggingLfoIndex = -1;
                return;
            }
        }
        float w = (float)getWidth(); float h = (float)getHeight();
        for (int i = 0; i < 3; ++i) {
            int wave = (int)processor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "wave")->load();
            if (wave == 6) { // Recordingîgå`
                auto p = processor.getLfoPos(i);
                float px = p.x * w; float py = p.y * h;
                if (std::hypot(e.x - px, e.y - py) < 15.0f) {
                    processor.recLength[i].store(0, std::memory_order_release);
                    processor.isWaitingForRecord[i].store(true, std::memory_order_release);
                    return;
                }
            }
        }
    }
    else {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load(std::memory_order_acquire)) {
                draggingLfoIndex = i;
                processor.isRecordingDrag[i].store(true, std::memory_order_release);
                float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
                float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
                processor.currentRecX[i].store(nx, std::memory_order_release);
                processor.currentRecY[i].store(ny, std::memory_order_release);
                return;
            }
        }
    }
    updatePosition(e);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e) {
    if (draggingLfoIndex != -1 && processor.isRecordingDrag[draggingLfoIndex].load(std::memory_order_acquire)) {
        int len = processor.recLength[draggingLfoIndex].load(std::memory_order_acquire);
        if (len < 2048) {
            float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
            float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
            processor.recBuffer[draggingLfoIndex][len] = { nx, ny };
            processor.recLength[draggingLfoIndex].store(len + 1, std::memory_order_release);
            processor.currentRecX[draggingLfoIndex].store(nx, std::memory_order_release);
            processor.currentRecY[draggingLfoIndex].store(ny, std::memory_order_release);
        }
        return;
    }
    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent& e) {
    if (draggingLfoIndex != -1) {
        processor.isRecordingDrag[draggingLfoIndex].store(false, std::memory_order_release);
        draggingLfoIndex = -1;
    }
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e) {
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
}

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

    g.slope.addItemList({ "12dB", "24dB", "48dB", "96dB" }, 1); addAndMakeVisible(g.slope);
    g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "slope" + s, g.slope);

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

    juce::StringArray waveTypes = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Lorenz Attractor", "Double Pendulum", "Random Walk",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard", "Polygon", "Attractor Orbit"
    };
    g.wave.addItemList(waveTypes, 1);
    addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "wave", g.wave);

    // Åyí«â¡ÅzBoundary ComboBox ÇÃ UI
    g.boundCombo.addItemList({ "Clip", "Bounce", "Wrap" }, 1);
    addAndMakeVisible(g.boundCombo);
    g.bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "bound", g.boundCombo);

    g.stepMode.setButtonText("Step"); g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync"); g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    g.syncToggle.onClick = [this] { resized(); };
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "sync", g.syncToggle);

    juce::StringArray syncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };
    g.rateSync.addItemList(syncRates, 1);
    addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "rateSync", g.rateSync);

    g.rateFree.setSliderStyle(juce::Slider::LinearHorizontal); g.rateFree.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.rateFree);
    g.rfAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "rateFree", g.rateFree);

    g.minSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    g.minSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.minSlider);
    g.minAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "min", g.minSlider);

    g.maxSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    g.maxSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.maxSlider);
    g.maxAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "max", g.maxSlider);
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
        g->enableButton.setBounds(r.removeFromLeft(90).reduced(0, 5));

        g->type.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 24));
        g->slope.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 24));

        auto cA = r.removeFromLeft(r.getWidth() / 2).reduced(10, 0);
        g->cutoffLabel.setBounds(cA.removeFromLeft(35)); g->cutoff.setBounds(cA);
        g->resLabel.setBounds(r.removeFromLeft(35)); g->res.setBounds(r);
    }

    b.removeFromTop(15);
    for (int i = 0; i < 3; ++i) {
        auto r = b.removeFromTop(40).reduced(5, 2);
        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));

        // ÅyèCê≥ÅzêVê›ÇµÇΩBoundaryÉRÉìÉ{É{ÉbÉNÉXÇä‹ÇﬂÇƒÉåÉCÉAÉEÉgÇìôï™
        lfos[i].wave.setBounds(r.removeFromLeft(120).withSizeKeepingCentre(115, 22));
        lfos[i].boundCombo.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 22));

        lfos[i].stepMode.setBounds(r.removeFromLeft(50).reduced(2, 5));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(50).reduced(2, 5));

        auto remainingWidth = r.getWidth();
        auto rateArea = r.removeFromLeft(remainingWidth / 3);
        auto minArea = r.removeFromLeft(remainingWidth / 3);
        auto maxArea = r;

        if (audioProcessor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "sync")->load() > 0.5f) {
            lfos[i].rateSync.setBounds(rateArea.withSizeKeepingCentre(rateArea.getWidth() - 5, 22));
            lfos[i].rateFree.setVisible(false); lfos[i].rateSync.setVisible(true);
        }
        else {
            lfos[i].rateFree.setBounds(rateArea.reduced(2, 8));
            lfos[i].rateFree.setVisible(true); lfos[i].rateSync.setVisible(false);
        }

        lfos[i].minSlider.setBounds(minArea.reduced(2, 8));
        lfos[i].maxSlider.setBounds(maxArea.reduced(2, 8));
    }
}