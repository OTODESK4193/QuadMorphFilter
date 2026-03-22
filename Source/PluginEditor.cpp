#include "PluginProcessor.h"
#include "PluginEditor.h"

QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p)
{
    // カーブ描画コンポーネントを追加
    addAndMakeVisible(visualizer);

    // 【追加】Cutoffスライダーの設定 (つまみ型: RotaryHorizontalVerticalDrag)
    cutoffSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    cutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 50, 20); // テキストボックスなし
    addAndMakeVisible(cutoffSlider);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    cutoffLabel.attachToComponent(&cutoffSlider, false);

    // 【追加】Resonanceスライダーの設定
    resonanceSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    resonanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 50, 20);
    addAndMakeVisible(resonanceSlider);
    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    resonanceLabel.setJustificationType(juce::Justification::centred);
    resonanceLabel.attachToComponent(&resonanceSlider, false);

    // 【追加】Filter Typeコンボボックスの設定
    typeComboBox.addItemList({ "LowPass", "BandPass", "HighPass", "Notch" }, 1);
    addAndMakeVisible(typeComboBox);
    typeLabel.setText("Filter Type", juce::dontSendNotification);
    typeLabel.attachToComponent(&typeComboBox, false);

    // 【重要】APVTSとUIコンポーネントをアタッチメントで接続（最重要）
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "cutoff", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "resonance", resonanceSlider);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "type", typeComboBox);

    // 画面の初期サイズ
    setSize(600, 400);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor() {}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // 背景を黒で塗りつぶし
    g.fillAll(juce::Colours::black);
}

void QuadMorphFilterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // --- 1. 画面領域の分割 ---
    // 下部にスライダー領域を確保（高さ100px）
    auto sliderAreaHeight = 100;
    auto sliderArea = bounds.removeFromBottom(sliderAreaHeight);

    // --- 2. 各コンポーネントの配置 ---
    // 上部のカーブ描画領域を配置（周囲に余白を持たせる）
    visualizer.setBounds(bounds.reduced(10));

    // 下部のスライダー領域を等分割（3つ分）
    auto numSliders = 3;
    auto sliderWidth = sliderArea.getWidth() / numSliders;

    // スライダーとコンボボックスを配置
    cutoffSlider.setBounds(sliderArea.removeFromLeft(sliderWidth).reduced(10));
    resonanceSlider.setBounds(sliderArea.removeFromLeft(sliderWidth).reduced(10));

    // コンボボックスはサイズを固定して中央に配置
    typeComboBox.setBounds(sliderArea.removeFromLeft(sliderWidth).withSizeKeepingCentre(100, 20));
}