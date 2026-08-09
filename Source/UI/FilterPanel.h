// ==========================================
// UI/FilterPanel.h   （V1.1.0 / レイアウト再構成）
//
// FILTER タブ。
//   ・FILTER MATRIX : A/B/C/D の 4 行（On / Solo / Model / Type / Slope /
//                     Cutoff / Res / LFO2・LFO3 割り当て）
//   ・OUTPUT        : Gain / Dry-Wet / Ceiling のノブ 3 つ（旧 OUT タブから移設）
//   ・INFO          : ノブの右の余白。マウス直下のコントロールの説明を 3 行で出す
//
// MORPH セクションは CONFIG タブへ移した。
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
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // ------------------------------------------------------------------
    struct Group
    {
        juce::TextButton enableButton;
        juce::TextButton soloButton;      // 排他 Solo
        juce::ComboBox   model, type, slope;
        juce::Slider     cutoff, res;
        juce::TextButton lfoCutBtn, lfoResBtn;

        std::unique_ptr<ButtonAtt> eAtt;
        std::unique_ptr<SliderAtt> cAtt, rAtt;
        std::unique_ptr<ComboAtt>  mAtt, tAtt, slAtt;
    };

    void setupGroup(Group& g, int index, const juce::String& suffix);
    void refreshGroupControls(Group& g, const juce::String& suffix, int modelIdx);
    void updateLfoSrcButtons();

    /** processor.soloFilter の状態を 4 つの S ボタンの点灯に反映する */
    void updateSoloButtons();

    /** LFO 変調後の実効 Cutoff / Res を求め、各スライダーの
        "qmModOn" / "qmModNorm" プロパティに書き込む。 */
    void updateModulationIndicators();

    /** LFO5 の変調レンジと現在位置を Dry/Wet ノブへ書き込む */
    void updateLfo5Indicator();

    /** マウス直下のコントロールの説明を拾って Info 欄を更新する */
    void updateInfoText();

    void timerCallback() override;

    /** 出力ノブ 1 つ分（名前・ノブ・数値）の矩形 */
    struct KnobSlot
    {
        juce::Rectangle<int> label, knob, value;
    };

    void styleKnob(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                   juce::Colour accent, const juce::String& paramId,
                   std::unique_ptr<SliderAtt>& att, const juce::String& info);

    void paintKnobText(juce::Graphics& g, const KnobSlot& slot,
                       const juce::Slider& s, juce::Colour accent) const;

    /** 1 行分の各カラム矩形を返す（paint と resized で共有する） */
    struct RowLayout
    {
        juce::Rectangle<int> enable, solo, model, type, slope, cutoff, res, lfoCut, lfoRes;
    };
    RowLayout layoutRow(juce::Rectangle<int> row) const;

    QuadMorphFilterAudioProcessor& processor;

    std::array<Group, 4> groups;

    // ---- 出力段（旧 OUT タブから移設）----
    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<SliderAtt> mgAtt, dwAtt, clAtt;
    KnobSlot gainSlot, dryWetSlot, ceilingSlot;

    // ---- Info 欄 ----
    juce::String currentInfo;
    juce::Rectangle<int> infoArea;

    // 見出しの描画位置（resized で確定、paint で使用）
    juce::Rectangle<int> headerArea, columnArea, outHeaderArea;
    std::array<juce::Rectangle<int>, 4> rowAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};
