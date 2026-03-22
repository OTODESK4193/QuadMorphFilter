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
    QuadMorphFilterAudioProcessor& processor;
};

// --- XY PAD コンポーネント (ABCD表示を維持) ---
class XYPadComponent : public juce::Component, public juce::Timer
{
public:
    XYPadComponent(QuadMorphFilterAudioProcessor& p) : processor(p) { startTimerHz(60); }
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override { updatePosition(e); }
    void mouseDrag(const juce::MouseEvent& e) override { updatePosition(e); }
private:
    void updatePosition(const juce::MouseEvent& e);
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
    XYPadComponent xyPad;

    struct FilterGroup {
        juce::TextButton enableButton; // 点灯式ボタン
        juce::ComboBox type;
        juce::Label cutoffLabel, resLabel;
        juce::Slider cutoff, res;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tAtt;
    };

    FilterGroup groupA, groupB, groupC, groupD;

    // LFOコントロール
    juce::Label lfoLabel;
    juce::Slider lfoRateSlider, lfoAmtSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoR_Att, lfoA_Att;

    void setupFilterGroup(FilterGroup& g, juce::String suffix, juce::String groupName);
    void layoutFilterGroup(FilterGroup& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};