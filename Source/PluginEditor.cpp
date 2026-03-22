#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizer描画 (変更なし) ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f));
    g.setColour(juce::Colours::cyan);
    juce::Path path;
    auto w = (float)getWidth(); auto h = (float)getHeight();
    for (int x = 0; x <= (int)w; ++x) {
        float freq = 20.0f * std::pow(1000.0f, x / w);
        float mag = getMorphedMagnitude(freq);
        float yPos = h - (mag * h * 0.4f + h * 0.2f);
        if (x == 0) path.startNewSubPath(0, yPos); else path.lineTo((float)x, yPos);
    }
    g.strokePath(path, juce::PathStrokeType(2.0f));
}

float FilterVisualizer::getMorphedMagnitude(float freq) {
    float x = processor.apvts.getRawParameterValue("posX")->load();
    float y = processor.apvts.getRawParameterValue("posY")->load();
    auto calc = [&](juce::String s) {
        float fc = processor.apvts.getRawParameterValue("cutoff" + s)->load();
        float res = processor.apvts.getRawParameterValue("res" + s)->load();
        int t = (int)processor.apvts.getRawParameterValue("type" + s)->load();
        float r = freq / fc; float r2 = r * r; float d = 1.0f / res;
        float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * d, 2.0f));
        if (t == 0) return 1.0f / den; if (t == 1) return r / den; if (t == 2) return r2 / den;
        return std::abs(1.0f - r2) / den;
        };
    return (calc("A") * (1 - x) * (1 - y)) + (calc("B") * x * (1 - y)) + (calc("C") * (1 - x) * y) + (calc("D") * x * y);
}

// --- Editor実装 ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    addAndMakeVisible(visualizer);

    // Filter A-D (名称から説明を削除)
    setupFilterGroup(groupA, "A", "Filter A");
    setupFilterGroup(groupB, "B", "Filter B");
    setupFilterGroup(groupC, "C", "Filter C");
    setupFilterGroup(groupD, "D", "Filter D");

    auto setupGlobal = [&](juce::Label& l, juce::Slider& s, juce::String text, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupGlobal(posXLabel, posXSlider, "X Pos", "posX", xAtt);
    setupGlobal(posYLabel, posYSlider, "Y Pos", "posY", yAtt);
    setupGlobal(lfoRateLabel, lfoRateSlider, "LFO Rate", "lfoRate", lfoR_Att);
    setupGlobal(lfoAmtLabel, lfoAmtSlider, "LFO Amount", "lfoAmount", lfoA_Att);

    setSize(900, 800); // 重なりを防ぐため高さを確保
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String groupName) {
    g.groupLabel.setText(groupName, juce::dontSendNotification);
    g.groupLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(g.groupLabel);

    g.cutoffLabel.setText("Cut", juce::dontSendNotification);
    addAndMakeVisible(g.cutoffLabel);
    g.cutoff.setSliderStyle(juce::Slider::LinearHorizontal);
    g.cutoff.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20); // 横に数値を配置
    addAndMakeVisible(g.cutoff);

    g.resLabel.setText("Res", juce::dontSendNotification);
    addAndMakeVisible(g.resLabel);
    g.res.setSliderStyle(juce::Slider::LinearHorizontal);
    g.res.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    addAndMakeVisible(g.res);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.type);

    g.cAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff" + s, g.cutoff);
    g.rAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "res" + s, g.res);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey.darker());
}

// --- レイアウト修正 ---
void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    const int groupHeight = 150;
    const int globalHeight = 180;

    // 1. 最上段: Filter A & B
    auto topArea = bounds.removeFromTop(groupHeight);
    layoutFilterGroup(groupA, topArea.removeFromLeft(bounds.getWidth() / 2).reduced(10));
    layoutFilterGroup(groupB, topArea.reduced(10));

    // 2. 最下段: Filter C & D
    auto bottomArea = bounds.removeFromBottom(groupHeight);
    layoutFilterGroup(groupC, bottomArea.removeFromLeft(bounds.getWidth() / 2).reduced(10));
    layoutFilterGroup(groupD, bottomArea.reduced(10));

    // 3. 下から2段目: Global Controls (XY/LFO)
    auto globalArea = bounds.removeFromBottom(globalHeight).reduced(10);
    auto leftGlobal = globalArea.removeFromLeft(globalArea.getWidth() / 2).reduced(10);
    auto rightGlobal = globalArea.reduced(10);

    posXLabel.setBounds(leftGlobal.removeFromTop(20));
    posXSlider.setBounds(leftGlobal.removeFromTop(50));
    posYLabel.setBounds(leftGlobal.removeFromTop(20));
    posYSlider.setBounds(leftGlobal.removeFromTop(50));

    lfoRateLabel.setBounds(rightGlobal.removeFromTop(20));
    lfoRateSlider.setBounds(rightGlobal.removeFromTop(50));
    lfoAmtLabel.setBounds(rightGlobal.removeFromTop(20));
    lfoAmtSlider.setBounds(rightGlobal.removeFromTop(50));

    // 4. 残った中央: Visualizer
    visualizer.setBounds(bounds.reduced(10));
}

void QuadMorphFilterAudioProcessorEditor::layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> b)
{
    auto header = b.removeFromTop(30);
    g.groupLabel.setBounds(header.removeFromLeft(80)); // 名称
    g.type.setBounds(header.removeFromLeft(80).withSizeKeepingCentre(70, 22)); // すぐ右にType

    const int labelW = 40;
    auto row1 = b.removeFromTop(45).reduced(2);
    g.cutoffLabel.setBounds(row1.removeFromLeft(labelW));
    g.cutoff.setBounds(row1);

    auto row2 = b.removeFromTop(45).reduced(2);
    g.resLabel.setBounds(row2.removeFromLeft(labelW));
    g.res.setBounds(row2);
}