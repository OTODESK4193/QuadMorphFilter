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
        juce::TextButton soloButton;      // 【V1.1.0 追加】排他 Solo
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

    /** processor.soloFilter の状態を 4 つの S ボタンの点灯に反映する */
    void updateSoloButtons();

    /** LFO 変調後の実効 Cutoff / Res を求め、各スライダーの
        "qmModOn" / "qmModNorm" プロパティに書き込む。
        描画は QuadMorphLookAndFeel::drawLinearSlider が行う。 */
    void updateModulationIndicators();

    void timerCallback() override;

    /** 1 行分の各カラム矩形を返す（paint と resized で共有する） */
    struct RowLayout
    {
        juce::Rectangle<int> enable, solo, model, type, slope, cutoff, res, lfoCut, lfoRes;
    };
    RowLayout layoutRow(juce::Rectangle<int> row) const;

    QuadMorphFilterAudioProcessor& processor;

    std::array<Group, 4> groups;

    juce::ComboBox morphBlendCombo, cutoffAlgoCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> morphBlendAtt, cutoffAlgoAtt;

    // ===== 【V1.1.0 追加】XY Depth =====
    // Cutoff Algo で求めた Cutoff/Res を各フィルターへどれだけ適用するか。
    // 0% で従来どおり（Cutoff/Res ノブの値をそのまま使用）。
    // 宣言順に注意: アタッチメントはスライダーより後に宣言し、
    // 破棄が先に走るようにする（CLAUDE.md §3）。
    juce::Slider xyDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xyDepthAtt;

    /** MODEL コンボのどれかにマウスが乗っていれば、その選択中モデル番号
        （0..27）を返す。乗っていなければ -1。タイマーから毎フレーム確認する。 */
    int hoveredModelIndex() const;

    /** MODEL にマウスオーバー中のモデル番号（-1 = なし）。paint が参照する。 */
    int hoveredModel = -1;

    // 見出しの描画位置（resized で確定、paint で使用）
    juce::Rectangle<int> headerArea, columnArea, morphHeaderArea, morphRowArea;

    /** MORPH 行の下、モデル説明を表示する帯 */
    juce::Rectangle<int> descArea;
    std::array<juce::Rectangle<int>, 4> rowAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPanel)
};
