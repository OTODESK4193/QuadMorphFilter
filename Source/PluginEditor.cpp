#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- Visualizerの描画ロジック ---
void FilterVisualizer::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black.withAlpha(0.8f)); // カーブを見やすく
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
    // 座標によるモーフィング計算
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

// --- Editorの実装 ---
QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    addAndMakeVisible(visualizer);

    // 【修正】3つの改善点を反映して FilterA~D をセットアップ
    setupFilterGroup(groupA, "A", "Filter A (Top-Left)");
    setupFilterGroup(groupB, "B", "Filter B (Top-Right)");
    setupFilterGroup(groupC, "C", "Filter C (Bottom-Left)");
    setupFilterGroup(groupD, "D", "Filter D (Bottom-Right)");

    // 全体コントロール (XY, LFO) も同様のスタイルで
    auto setupGlobal = [&](juce::Label& l, juce::Slider& s, juce::String text, juce::String id, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att) {
        l.setText(text, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(l);

        s.setSliderStyle(juce::Slider::LinearHorizontal); // 水平スライダー
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15); // 数値ボックスあり
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, s);
        };
    setupGlobal(posXLabel, posXSlider, "X Pos", "posX", xAtt);
    setupGlobal(posYLabel, posYSlider, "Y Pos", "posY", yAtt);
    setupGlobal(lfoRateLabel, lfoRateSlider, "LFO Rate", "lfoRate", lfoR_Att);
    setupGlobal(lfoAmtLabel, lfoAmtSlider, "LFO Amount", "lfoAmount", lfoA_Att);

    // 要素が増えたため、画面サイズを広げました
    setSize(900, 700);
}

// フィルターグループの初期化関数
void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g, juce::String s, juce::String groupName) {
    // 改善点3: グループ名を太字で表示
    g.groupLabel.setText(groupName, juce::dontSendNotification);
    g.groupLabel.setJustificationType(juce::Justification::centred);
    g.groupLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    addAndMakeVisible(g.groupLabel);

    // 改善点2: 「名称」「数値ボックス」「スライダー」で統一
    g.cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    addAndMakeVisible(g.cutoffLabel);
    g.cutoff.setSliderStyle(juce::Slider::LinearHorizontal); // 改善点1: スライダー
    g.cutoff.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(g.cutoff);

    g.resLabel.setText("Resonance", juce::dontSendNotification);
    addAndMakeVisible(g.resLabel);
    g.res.setSliderStyle(juce::Slider::LinearHorizontal);
    g.res.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(g.res);

    // ComboBoxも配置
    g.type.addItemList({ "LowPass", "BandPass", "HighPass", "Notch" }, 1);
    addAndMakeVisible(g.type);

    // APVTSとの接続
    g.cAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff" + s, g.cutoff);
    g.rAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "res" + s, g.res);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type" + s, g.type);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g) {
    // 背景を暗くしてコントロールを目立たせる
    g.fillAll(juce::Colours::grey.withAlpha(0.2f));
}

// --- 【修正】複雑なレイアウトを再構築 ---
void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // 分成上中下三部分
    auto topArea = bounds.removeFromTop(200);
    auto bottomArea = bounds.removeFromBottom(200);
    auto middleArea = bounds; // 剩余部分是Visualizer

    // 上部：FilterA和FilterB
    layoutFilterGroup(groupA, topArea.removeFromLeft(300).reduced(10));
    layoutFilterGroup(groupB, topArea.removeFromRight(300).reduced(10));

    // 中部：Visualizer
    visualizer.setBounds(middleArea.reduced(20));

    // 下部：Global Control 和 FilterC/FilterD
    auto globalControlAreaHeight = 100;
    auto globalControlArea = bottomArea.removeFromTop(globalControlAreaHeight).reduced(10);

    // 全局控制（XY pad, LFO）
    auto xyArea = globalControlArea.removeFromLeft(globalControlArea.getWidth() / 2).reduced(10);
    auto lfoArea = globalControlArea.reduced(10);

    posXLabel.setBounds(xyArea.removeFromTop(20));
    posXSlider.setBounds(xyArea.removeFromTop(30));
    posYLabel.setBounds(xyArea.removeFromTop(20));
    posYSlider.setBounds(xyArea.removeFromTop(30));

    lfoRateLabel.setBounds(lfoArea.removeFromTop(20));
    lfoRateSlider.setBounds(lfoArea.removeFromTop(30));
    lfoAmtLabel.setBounds(lfoArea.removeFromTop(20));
    lfoAmtSlider.setBounds(lfoArea.removeFromTop(30));

    // FilterC和FilterD
    layoutFilterGroup(groupC, bottomArea.removeFromLeft(300).reduced(10));
    layoutFilterGroup(groupD, bottomArea.removeFromRight(300).reduced(10));
}

// --- 【追加】FilterGroupの内部レイアウト関数 (名称、数値、スライダーのセット) ---
void QuadMorphFilterAudioProcessorEditor::layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> bounds)
{
    g.groupLabel.setBounds(bounds.removeFromTop(30));

    auto rowHeight = 40;
    auto labelWidth = 80;

    // Cutoff行
    auto cutoffRow = bounds.removeFromTop(rowHeight).reduced(2);
    g.cutoffLabel.setBounds(cutoffRow.removeFromLeft(labelWidth));
    g.cutoff.setBounds(cutoffRow);

    // Resonance行
    auto resRow = bounds.removeFromTop(rowHeight).reduced(2);
    g.resLabel.setBounds(resRow.removeFromLeft(labelWidth));
    g.res.setBounds(resRow);

    // Type行
    g.type.setBounds(bounds.removeFromTop(rowHeight).reduced(2).withSizeKeepingCentre(100, 24));
}