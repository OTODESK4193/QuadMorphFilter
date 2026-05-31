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

class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             private juce::Timer
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
        juce::ComboBox   wave, rateSync;
        juce::Slider     rateFree, minSlider, maxSlider;
        juce::Slider     phaseSlider, fadeSlider, spreadSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rfAtt, minAtt, maxAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   phaseAtt, fadeAtt, spreadAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt, bAtt;
    };
    LfoGroup lfos[3];

    // ===== LFO4: Rate Modulation 専用 =====
    struct LFO4Group {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync;
        juce::Slider     rateFree, depthSlider;
        juce::Slider     phaseSlider, fadeSlider, spreadSlider;
        // ===== アサイン先ボタン =====
        juce::TextButton assignLFO1, assignLFO2, assignLFO3;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   rfAtt, depthAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   phaseAtt, fadeAtt, spreadAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   assignAtt1, assignAtt2, assignAtt3;
    };
    LFO4Group lfo4;

    juce::Label  masterGainLabel, dryWetLabel, ceilingLabel;
    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mgAtt, dwAtt, clAtt;

    // ===== 【新規追加】ここに挿入 =====
    juce::Label    osModeLabel;
    juce::ComboBox osModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> osModeAtt;


    // ===== Morph ブレンド / Cutoff アルゴリズム 選択コンボ =====
    juce::Label    morphBlendLabel;
    juce::ComboBox morphBlendCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> morphBlendAtt;

    juce::Label    cutoffAlgoLabel;
    juce::ComboBox cutoffAlgoCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cutoffAlgoAtt;

    // ===== LFO Cut/Res per-filter サイクルボタン (A/B/C/D) =====
    // Off → +X → +Y → -X → -Y → Off を押すたびにサイクル
    // AudioParameterChoice (5択) を使用。ButtonAttachment は使用しない。
    juce::Label      lfoCutLabel;
    juce::TextButton lfoCutBtn[4];

    juce::Label      lfoResLabel;
    juce::TextButton lfoResBtn[4];

    // ===== LFO セクションタイトル行 =====
    // LFO | Wave | Step | Sync | Rate | Min | Max | Phase | Fade | Spread
    juce::Label lfoTitleLabels[10];
    juce::Label lfo4TitleLabel;  // LFO4 セクションタイトル

    // ===== 既存コード: private セクション末尾 =====
    void setupFilterGroup(FilterGroup& g, juce::String s, juce::String name);
    void setupLfoGroup(LfoGroup& g, int index, juce::String name);
    void refreshFilterGroupControls(FilterGroup& g, const juce::String& suffix, int modelIdx);

    // ===== LFO Cut/Res ボタン外観の定期同期 (30Hz) =====
    void timerCallback() override;
    void updateLfoCutResButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};