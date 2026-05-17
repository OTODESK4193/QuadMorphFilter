// ==========================================
// PluginEditor.h (軽量化版)
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"
#include "UI/QuadMorphLookAndFeel.h"  // ← UI/ から
#include "UI/FilterVisualizer.h"       // ← UI/ から
#include "UI/XYPadComponent.h"         // ← UI/ から

class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor&);
    ~QuadMorphFilterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    QuadMorphFilterAudioProcessor& audioProcessor;
    QuadMorphLookAndFeel customLookAndFeel;
    FilterVisualizer     visualizer;
    XYPadComponent       xyPad;

    struct FilterGroup {
        juce::TextButton enableButton;
        juce::ComboBox   model, type, slope;
        juce::Label      cutoffLabel, resLabel;
        juce::Slider     cutoff, res;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAtt, tAtt, slAtt;
    };
    FilterGroup groupA, groupB, groupC, groupD;

    struct LfoGroup {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync, boundCombo;
        juce::Slider     rateFree, minSlider, maxSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  rfAtt, minAtt, maxAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt, bAtt;
    };
    LfoGroup lfos[3];

    juce::Label  masterGainLabel, dryWetLabel, ceilingLabel;
    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mgAtt, dwAtt, clAtt;

    void setupFilterGroup(FilterGroup& g, juce::String s, juce::String name);
    void setupLfoGroup(LfoGroup& g, int index, juce::String name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};