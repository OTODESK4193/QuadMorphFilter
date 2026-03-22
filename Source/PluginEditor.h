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

// --- XY PAD (3つのドットを描画) ---
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

    // フィルターグループ構造体
    struct FilterGroup {
        juce::TextButton enableButton;
        juce::ComboBox type;
        juce::Label cutoffLabel, resLabel;
        juce::Slider cutoff, res;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tAtt;
    };
    FilterGroup groupA, groupB, groupC, groupD;

    // LFOグループ構造体
    struct LfoGroup {
        juce::TextButton enableButton;
        juce::ComboBox wave, rateSync;
        juce::ToggleButton stepMode, syncToggle;
        juce::Slider rateFree, amt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rfAtt, aAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt;
    };
    LfoGroup lfos[3];

    void setupFilterGroup(FilterGroup& g, juce::String s, juce::String name);
    void setupLfoGroup(LfoGroup& g, int index, juce::String name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};