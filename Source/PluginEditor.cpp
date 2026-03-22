#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizer: 描画スケーリングの改善 ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::cyan);

    auto currentPos = processor.getCurrentXY();
    float x = currentPos.getX();
    float y = currentPos.getY();

    auto w = (float)getWidth(); auto h = (float)getHeight();
    juce::Path path;

    for (int px = 0; px <= (int)w; ++px) {
        float freq = 20.0f * std::pow(1000.0f, px / w);

        auto calc = [&](juce::String s) {
            float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load();
            float res = processor.apvts.getRawParameterValue("res" + s)->load();
            int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
            float r = freq / fc; float r2 = r * r; float d = 1.0f / res;
            float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
            float mag = 1.0f / den;
            if (t == 1) mag = r * mag; // BP
            if (t == 2) mag = r2 * mag; // HP
            if (t == 3) mag = std::abs(1.0f - r2) * mag; // Notch
            return mag;
            };

        float mag = (calc("A") * (1 - x) * (1 - y)) + (calc("B") * x * (1 - y)) + (calc("C") * (1 - x) * y) + (calc("D") * x * y);

        // 【修正】高Resonance時でも枠に収めるためのスケーリング (非線形圧縮)
        // 単純な乗算ではなく、対数的な圧縮を加えることでピークを抑え込みます
        float scaledMag = std::log10(mag + 1.0f) * 1.5f;
        float yPos = h - (scaledMag * h * 0.4f + h * 0.1f);

        yPos = juce::jlimit(2.0f, h - 2.0f, yPos); // 上下の枠を突き抜けないガード

        if (px == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)px, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

// --- XYPadComponent: ガイドラベルの追加 ---
void XYPadComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds, 10.0f, 2.0f);

    // 【追加】四隅のガイドテキスト
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(12.0f);
    g.drawText("A", bounds.removeFromTop(20).removeFromLeft(20), juce::Justification::centred);
    g.drawText("B", getLocalBounds().removeFromTop(20).removeFromRight(20).toFloat(), juce::Justification::centred);
    g.drawText("C", getLocalBounds().removeFromBottom(20).removeFromLeft(20).toFloat(), juce::Justification::centred);
    g.drawText("D", getLocalBounds().removeFromBottom(20).removeFromRight(20).toFloat(), juce::Justification::centred);

    auto currentPos = processor.getCurrentXY();
    float dotX = currentPos.getX() * getWidth();
    float dotY = currentPos.getY() * getHeight();

    g.setColour(juce::Colours::cyan);
    g.fillEllipse(dotX - 5, dotY - 5, 10, 10);
    g.setColour(juce::Colours::white);
    g.drawEllipse(dotX - 7, dotY - 7, 14, 14, 1.0f);
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e) {
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
    repaint();
}

// --- Editor コンストラクタ / resized (変更なし) ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);
    setupFilterGroup(groupA, "A", "Filter A"); setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C"); setupFilterGroup(groupD, "D", "Filter D");

    auto setupGlobal = [&](juce::Label& l, juce::Slider& s, juce::String text, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(text, juce::dontSendNotification); addAndMakeVisible(l);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 15); addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupGlobal(lfoRateLabel, lfoRateSlider, "LFO Rate", "lfoRate", lfoR_Att);
    setupGlobal(lfoAmtLabel, lfoAmtSlider, "LFO Amt", "lfoAmount", lfoA_Att);
    setSize(900, 800);
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String groupName) {
    g.groupLabel.setText(groupName, juce::dontSendNotification);
    g.groupLabel.setFont(juce::Font(18.0f, juce::Font::bold)); addAndMakeVisible(g.groupLabel);
    g.cutoffLabel.setText("Cut", juce::dontSendNotification); addAndMakeVisible(g.cutoffLabel);
    g.cutoff.setSliderStyle(juce::Slider::LinearHorizontal);
    g.cutoff.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20); addAndMakeVisible(g.cutoff);
    g.resLabel.setText("Res", juce::dontSendNotification); addAndMakeVisible(g.resLabel);
    g.res.setSliderStyle(juce::Slider::LinearHorizontal);
    g.res.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20); addAndMakeVisible(g.res);
    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1); addAndMakeVisible(g.type);
    g.cAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff" + s, g.cutoff);
    g.rAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "res" + s, g.res);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}
void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colours::darkgrey.darker()); }

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    const int groupHeight = 150;
    auto topArea = bounds.removeFromTop(groupHeight);
    layoutFilterGroup(groupA, topArea.removeFromLeft(bounds.getWidth() / 2).reduced(10));
    layoutFilterGroup(groupB, topArea.reduced(10));
    auto bottomArea = bounds.removeFromBottom(groupHeight);
    layoutFilterGroup(groupC, bottomArea.removeFromLeft(bounds.getWidth() / 2).reduced(10));
    layoutFilterGroup(groupD, bottomArea.reduced(10));
    auto middleArea = bounds.reduced(10);
    visualizer.setBounds(middleArea.removeFromLeft(middleArea.getWidth() * 0.6f).reduced(5));
    auto rightArea = middleArea;
    xyPad.setBounds(rightArea.removeFromTop(rightArea.getWidth()).reduced(5));
    lfoRateLabel.setBounds(rightArea.removeFromTop(20));
    lfoRateSlider.setBounds(rightArea.removeFromTop(40));
    lfoAmtLabel.setBounds(rightArea.removeFromTop(20));
    lfoAmtSlider.setBounds(rightArea.removeFromTop(40));
}

void QuadMorphFilterAudioProcessorEditor::layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> b) {
    auto header = b.removeFromTop(30);
    g.groupLabel.setBounds(header.removeFromLeft(80));
    g.type.setBounds(header.removeFromLeft(80).withSizeKeepingCentre(70, 22));
    auto row1 = b.removeFromTop(45).reduced(2);
    g.cutoffLabel.setBounds(row1.removeFromLeft(40)); g.cutoff.setBounds(row1);
    auto row2 = b.removeFromTop(45).reduced(2);
    g.resLabel.setBounds(row2.removeFromLeft(40)); g.res.setBounds(row2);
}