#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"

// --- 視覚化コンポーネント ---
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

// --- メインエディター ---
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

    struct FilterGroup {
        juce::Label groupLabel;
        juce::Label cutoffLabel;
        juce::Label resLabel;
        juce::Slider cutoff;
        juce::Slider res;
        juce::ComboBox type;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tAtt;
    };

    FilterGroup groupA, groupB, groupC, groupD;

    juce::Label posXLabel, posYLabel, lfoRateLabel, lfoAmtLabel;
    juce::Slider posXSlider, posYSlider, lfoRateSlider, lfoAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAtt, yAtt, lfoR_Att, lfoA_Att;

    void setupFilterGroup(FilterGroup& g, juce::String suffix, juce::String groupName);
    void layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};