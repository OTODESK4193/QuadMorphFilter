#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"

// --- 視覚化コンポーネント (前回から継続) ---
class FilterVisualizer : public juce::Component, public juce::Timer
{
public:
    FilterVisualizer(QuadMorphFilterAudioProcessor& p) : processor(p) { startTimerHz(60); }
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics& g) override;
private:
    float getMorphedMagnitude(float freq);
    QuadMorphFilterAudioProcessor& processor;
};

// --- メインエディター (今回の主役) ---
class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor&);
    ~QuadMorphFilterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    QuadMorphFilterAudioProcessor& audioProcessor;
    FilterVisualizer visualizer;

    // --- 【修正】改善点を反映した FilterGroup 構造体 ---
    struct FilterGroup {
        juce::Label groupLabel;     // "Filter A (Top-Left)" とか
        juce::Label cutoffLabel;    // "Cutoff"
        juce::Label resLabel;       // "Resonance"
        juce::Slider cutoff;        // 水平スライダー
        juce::Slider res;           // 水平スライダー
        juce::ComboBox type;        // タイプ
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tAtt;
    };

    FilterGroup groupA, groupB, groupC, groupD;

    // --- 【追加】グローバルコントロールのラベル ---
    juce::Label posXLabel, posYLabel, lfoRateLabel, lfoAmtLabel;
    juce::Slider posXSlider, posYSlider, lfoRateSlider, lfoAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAtt, yAtt, lfoR_Att, lfoA_Att;

    // 内部レイアウト用の関数
    void setupFilterGroup(FilterGroup& g, juce::String suffix, juce::String groupName);
    void layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};