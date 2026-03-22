#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"

// 視覚化コンポーネント
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

// メインエディター
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

    // --- 全パラメーターのUI定義 ---
    struct FilterGroup {
        juce::Slider cutoff, res;
        juce::ComboBox type;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tAtt;
    };

    FilterGroup groupA, groupB, groupC, groupD;

    juce::Slider posXSlider, posYSlider, lfoRateSlider, lfoAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAtt, yAtt, rAtt, aAtt;

    void setupFilterGroup(FilterGroup& g, juce::String suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};