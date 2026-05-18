// ==========================================
// PluginEditor.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"
#include "UI/QuadMorphLookAndFeel.h"
#include "UI/FilterVisualizer.h"
#include "UI/XYPadComponent.h"

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
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAtt, tAtt, slAtt;
    };
    FilterGroup groupA, groupB, groupC, groupD;

    struct LfoGroup {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync, boundCombo;
        juce::Slider     rateFree, minSlider, maxSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rfAtt, minAtt, maxAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt, bAtt;
    };
    LfoGroup lfos[3];

    juce::Label  masterGainLabel, dryWetLabel, ceilingLabel;
    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mgAtt, dwAtt, clAtt;

    // ===== 【新規】XY モード選択 =====
    juce::Label    xyModeLabel;
    juce::ComboBox xyModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> xyModeAtt;

    // ===== 【新規追加】ここに挿入 =====
    juce::Label    osModeLabel;
    juce::ComboBox osModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osModeAtt;


    // ===== 【新規】LFO Cut per-filter スイッチ (A/B/C/D) =====
    juce::Label      lfoCutLabel;
    juce::TextButton lfoCutBtn[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoCutAtt[4];

    // ===== 【新規】LFO Res per-filter スイッチ (A/B/C/D) =====
    juce::Label      lfoResLabel;
    juce::TextButton lfoResBtn[4];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoResAtt[4];

    void setupFilterGroup(FilterGroup& g, juce::String s, juce::String name);
    void setupLfoGroup(LfoGroup& g, int index, juce::String name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};