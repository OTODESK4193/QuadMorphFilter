// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    float y0dB = juce::jmap(0.0f, 40.0f, -60.0f, 0.0f, h);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine((int)y0dB, 0.0f, w);
    g.setFont(10.0f);
    g.drawText("0dB", 2, (int)y0dB - 12, 30, 10, juce::Justification::left);

    g.setColour(juce::Colours::white.withAlpha(0.15f));
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
    auto cPos = processor.getLfoPos(1);
    auto rPos = processor.getLfoPos(2);

    bool lfo1_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo2wave")->load() == 3) && (processor.apvts.getRawParameterValue("lfo2en")->load() > 0.5f);
    bool lfo2_isRand1 = ((int)processor.apvts.getRawParameterValue("lfo3wave")->load() == 3) && (processor.apvts.getRawParameterValue("lfo3en")->load() > 0.5f);

    auto lfo1Mod4 = processor.getLfoMod4(1);
    auto lfo2Mod4 = processor.getLfoMod4(2);

    auto getM = [](juce::Point<float> p, const std::array<float, 4>& m4, bool isRand1) -> std::array<float, 4> {
        std::array<float, 4> res;
        if (isRand1) { for (int i = 0; i < 4; ++i) res[i] = m4[i] * 2.0f - 1.0f; }
        else { res[0] = p.x * 2.0f - 1.0f; res[1] = p.y * 2.0f - 1.0f; res[2] = (1.0f - p.x) * 2.0f - 1.0f; res[3] = (1.0f - p.y) * 2.0f - 1.0f; }
        return res;
        };

    std::array<float, 4> cM = getM(cPos, lfo1Mod4, lfo1_isRand1);
    std::array<float, 4> rM = getM(rPos, lfo2Mod4, lfo2_isRand1);

    int wInt = (int)w;
    std::vector<float> rawMag(wInt + 1, 0.0f);

    // Step 1: 元の解像度（1px刻み）で生のMagnitudeを計算
    for (int px = 0; px <= wInt; ++px) {
        float freq = 20.0f * std::pow(1000.0f, (float)px / w);

        auto calc = [&](juce::String s, int idx) -> float {
            if (processor.apvts.getRawParameterValue("enable" + s)->load() < 0.5f) return 0.0f;

            float baseCutoff = processor.apvts.getRawParameterValue("cutoff" + s)->load();
            float fc = baseCutoff * std::pow(2.0f, 4.0f * cM[idx]);
            float baseRes = processor.apvts.getRawParameterValue("res" + s)->load();
            float res = baseRes * std::pow(2.0f, 2.0f * rM[idx]);

            int modelIdx = (int)processor.apvts.getRawParameterValue("model" + s)->load();
            int slopeIdx = (int)processor.apvts.getRawParameterValue("slope" + s)->load();
            int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();

            float freqLimit = juce::jlimit(20.0f, 20000.0f, fc);
            float w_norm = freq / freqLimit;
            float w2 = w_norm * w_norm;
            float mag = 1.0f;

            if (modelIdx == 0 || modelIdx == 3 || modelIdx == 4) {
                int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
                float adjustedRes = res;
                if (stages > 1) adjustedRes = res * std::pow(0.6f, std::log2((float)stages));
                float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
                float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w_norm * d, 2.0f));
                float m = 1.0f / den;
                if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
                mag = std::pow(m, stages);
                if (modelIdx == 0 || modelIdx == 4) mag *= (1.0f + res * 0.1f);
            }
            else if (modelIdx == 1) {
                int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
                float r_moog = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 4.0f);
                if (stages > 1) r_moog *= std::pow(0.7f, std::log2((float)stages));
                float real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_moog;
                float imag_p = 4.0f * w_norm * (1.0f - w2);
                float den2 = real_p * real_p + imag_p * imag_p;
                float m = 1.0f / std::sqrt(den2);
                if (slopeIdx == 0) {
                    if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
                }
                else {
                    if (t == 1) m *= w2; else if (t == 2) m *= w2 * w2; else if (t == 3) m *= std::abs(1.0f - w2 * w2);
                }
                mag = std::pow(m, stages);
                mag *= (1.0f + 0.5f * r_moog);
            }
            else if (modelIdx == 2) {
                int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 1 : (slopeIdx == 2) ? 2 : 4;
                float r_diode = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, 15.0f);
                if (stages > 1) r_diode *= std::pow(0.7f, std::log2((float)stages));
                float real_p = std::pow(1.0f - w2, 2.0f) - 3.5f * w2 + r_diode;
                float imag_p = 3.5f * w_norm * (1.0f - w2);
                float den2 = real_p * real_p + imag_p * imag_p;
                float m = 1.0f / std::sqrt(den2);
                if (slopeIdx == 0) {
                    if (t == 1) m *= w_norm; else if (t == 2) m *= w2; else if (t == 3) m *= std::abs(1.0f - w2);
                }
                else {
                    if (t == 1) m *= w2 * w_norm; else if (t == 2) m *= w2 * w2; else if (t == 3) m *= std::abs(1.0f - w2 * w_norm);
                }
                mag = std::pow(m, stages);
                mag *= (1.0f + 0.2f * r_diode);
            }
            else if (modelIdx == 5) { // 【追加】Formant Visualizer
                float v = juce::jmap(std::log10(freqLimit), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f);
                v = juce::jlimit(0.0f, 4.0f, v);
                int idx_v = (int)v; float frac = v - idx_v;
                if (idx_v >= 4) { idx_v = 3; frac = 1.0f; }
                float f1_m[5] = { 730.f, 270.f, 300.f, 530.f, 400.f };
                float f2_m[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f };
                float f3_m[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
                float f_arr[3] = { f1_m[idx_v] + (f1_m[idx_v + 1] - f1_m[idx_v]) * frac, f2_m[idx_v] + (f2_m[idx_v + 1] - f2_m[idx_v]) * frac, f3_m[idx_v] + (f3_m[idx_v + 1] - f3_m[idx_v]) * frac };

                float mag_sum = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
                int stages = (slopeIdx == 0) ? 1 : (slopeIdx == 1) ? 2 : (slopeIdx == 2) ? 4 : 8;
                for (int f = 0; f < 3; ++f) {
                    float w_f = freq / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
                    float d_f = 1.0f / juce::jlimit(0.1f, 10.0f, res);
                    float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
                    if (t == 1) m_f *= w_f; else if (t == 2) m_f *= w_f * w_f; else if (t == 3) m_f *= std::abs(1.0f - w_f * w_f);
                    mag_sum += m_f * gains[f];
                }
                mag = std::pow(mag_sum, stages) * (1.0f + res * 0.1f);
            }
            return static_cast<float>(mag);
            };

        auto mPos = processor.getLfoPos(0); float x = mPos.x; float y = mPos.y;
        rawMag[px] = (calc("A", 0) * (1 - x) * (1 - y)) + (calc("B", 1) * x * (1 - y)) + (calc("C", 2) * (1 - x) * y) + (calc("D", 3) * x * y);
    }

    // Step 2: 【流体スムージング】 移動平均フィルターで角を美しく溶かす
    juce::Path path;
    int smoothRadius = 8; // 溶かす幅（プロ品質の有機的なカーブを生成）
    for (int px = 0; px <= wInt; ++px) {
        float sum = 0.0f; int count = 0;
        for (int k = -smoothRadius; k <= smoothRadius; ++k) {
            int idx = px + k;
            if (idx >= 0 && idx <= wInt) { sum += rawMag[idx]; count++; }
        }
        float smoothedMag = sum / (float)count;

        float db = 20.0f * std::log10(smoothedMag + 1e-5f);
        float yPos = juce::jmap(db, 40.0f, -60.0f, 0.0f, h);

        if (px == 0) path.startNewSubPath((float)px, yPos); else path.lineTo((float)px, yPos);
    }

    g.strokePath(path, juce::PathStrokeType(2.0f));
}

XYPadComponent::XYPadComponent(QuadMorphFilterAudioProcessor& p) : processor(p) {
    for (int i = 0; i < 3; ++i) trails[i].fill({ 0.5f, 0.5f });
    startTimerHz(60);
}

void XYPadComponent::timerCallback() {
    for (int i = 0; i < 3; ++i) { trails[i][trailIdx[i]] = processor.getLfoPos(i); trailIdx[i] = (trailIdx[i] + 1) % 30; }
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

        for (int t = 0; t < 30; ++t) {
            int idx = (trailIdx[i] + t) % 30; auto pt = trails[i][idx]; float alpha = (float)t / 30.0f * 0.4f;
            g.setColour(colors[i].withAlpha(alpha)); g.fillEllipse(pt.x * getWidth() - 3, pt.y * getHeight() - 3, 6, 6);
        }

        auto p = processor.getLfoPos(i);
        bool isWait = processor.isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = processor.isRecordingDrag[i].load(std::memory_order_acquire);

        if (isWait) {
            if (isDrag) { g.setColour(colors[i].brighter(0.8f)); g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16); }
            else { float alpha = 0.3f + 0.7f * std::abs(std::sin(juce::Time::getMillisecondCounter() * 0.005f)); g.setColour(colors[i].withAlpha(alpha)); g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16); }
        }
        else {
            g.setColour(colors[i]); g.fillEllipse(p.x * getWidth() - 6, p.y * getHeight() - 6, 12, 12);
        }
        g.setColour(juce::Colours::white); g.drawEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16, 1.0f);
    }
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e) {
    if (e.mods.isRightButtonDown()) {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) { processor.isWaitingForRecord[i].store(false); processor.isRecordingDrag[i].store(false); draggingLfoIndex = -1; return; }
        }
        float w = (float)getWidth(); float h = (float)getHeight();
        for (int i = 0; i < 3; ++i) {
            int wave = (int)processor.apvts.getRawParameterValue("lfo" + juce::String(i + 1) + "wave")->load();
            if (wave == 6) {
                auto p = processor.getLfoPos(i); float px = p.x * w; float py = p.y * h;
                if (std::hypot(e.x - px, e.y - py) < 15.0f) { processor.recLength[i].store(0); processor.isWaitingForRecord[i].store(true); return; }
            }
        }
    }
    else {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                draggingLfoIndex = i; processor.isRecordingDrag[i].store(true);
                float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth()); float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
                processor.currentRecX[i].store(nx); processor.currentRecY[i].store(ny); return;
            }
        }
    }
    updatePosition(e);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e) {
    if (draggingLfoIndex != -1 && processor.isRecordingDrag[draggingLfoIndex].load()) {
        int len = processor.recLength[draggingLfoIndex].load();
        if (len < 2048) {
            float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth()); float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
            processor.recBuffer[draggingLfoIndex][len] = { nx, ny }; processor.recLength[draggingLfoIndex].store(len + 1);
            processor.currentRecX[draggingLfoIndex].store(nx); processor.currentRecY[draggingLfoIndex].store(ny);
        }
        return;
    }
    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent& e) {
    if (draggingLfoIndex != -1) { processor.isRecordingDrag[draggingLfoIndex].store(false); draggingLfoIndex = -1; }
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e) {
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth()); float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x); processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
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

    // 【追加】Formant を選択肢に追加
    g.model.addItemList({ "Clean SVF", "Moog Ladder", "Diode (TB-303)", "SEM (Oberheim)", "Bitcrush / SRR", "Formant (Vowel)" }, 1); addAndMakeVisible(g.model);
    g.mAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "model" + s, g.model);

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
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard", "Polygon", "Attractor Orbit"
    };
    g.wave.addItemList(waveTypes, 1); addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "wave", g.wave);

    g.boundCombo.addItemList({ "Clip", "Bounce", "Wrap" }, 1); addAndMakeVisible(g.boundCombo);
    g.bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "bound", g.boundCombo);

    g.stepMode.setButtonText("Step"); g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]); addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync"); g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]); g.syncToggle.onClick = [this] { resized(); };
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id + "sync", g.syncToggle);

    juce::StringArray syncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };
    g.rateSync.addItemList(syncRates, 1); addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id + "rateSync", g.rateSync);

    g.rateFree.setSliderStyle(juce::Slider::LinearHorizontal); g.rateFree.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.rateFree);
    g.rfAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "rateFree", g.rateFree);

    g.minSlider.setSliderStyle(juce::Slider::LinearHorizontal); g.minSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
    addAndMakeVisible(g.minSlider);
    g.minAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id + "min", g.minSlider);

    g.maxSlider.setSliderStyle(juce::Slider::LinearHorizontal); g.maxSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
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

    b.removeFromTop(15);
    for (int i = 0; i < 3; ++i) {
        auto r = b.removeFromTop(40).reduced(5, 2);
        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 5));

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