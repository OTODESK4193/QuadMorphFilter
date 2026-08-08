// ==========================================
// UI/FilterPanel.h   （V1.1.0 新規）
//
// FILTER タブ。フィルター A/B/C/D の 4 行と、
// 各行に対する LFO→Cut / LFO→Res の割り当て、
// および MORPH（Blend / Cutoff Algo）をまとめる。
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

#include "ColorPalette.h"
#include "UiCommon.h"

class QuadMorphFilterAudioProcessor;

class FilterPanel : public juce::Component,
                    private juce::Timer
{
public:
    explicit FilterPanel(QuadMorphFilterAudioProcessor& p);
    ~FilterPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** テーマ切替後に色を貼り直す */
    void refreshTheme();

private:
    // ------------------------------------------------------------------
    struct Group
    {
        juce::TextButton enableButton;
        juce::ComboBox   model, type, slope;
        juce::Slider     cutoff, res;
        juce::TextButton lfoCutBtn, lfoResBtn;

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAtt, tAtt, slAtt;
    };

    void setupGroup(Group& g, int index, const juce::String& suffix);
    void refreshGroupControls(Group& g, const juce::String& suffix, int modelIdx);
    void updateLfoSrcButtons();
    void timerCallback() override;

    /** 1 行分の各カラム矩形を返す（paint と resized で共有する） */
    struct RowLayout
    {
        juce::Rectangle<int> enable, model, type, slope, cutoff, res, lfoCut, lfoRes;
    };
    RowLayout layoutRow(juce::Rectangle<int> row) const;

    QuadMorphFilterAudioProcessor& processor;

    std::array<Group, 4> groups;

    juce::ComboBox morphBlendCombo, cutoffAlgoCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> morphBlendAtt, cutoffAlgoAtt;

    // 見出しの描画位置（resized で確定、paint で使用）
    juce::Rectangle<int> headerArea, columnArea, morphHeaderArea, morphRowArea;
    std::array<juce::Rectangle<int>, 4> rowAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};
