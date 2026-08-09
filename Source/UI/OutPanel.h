// ==========================================
// UI/OutPanel.h   （V1.1.0 新規）
//
// OUT タブ。出力段（Gain / Dry-Wet / Limiter Ceiling）と
// オーバーサンプリング設定、プラグイン情報をまとめる。
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

#include "ColorPalette.h"
#include "UiCommon.h"

class QuadMorphFilterAudioProcessor;

class OutPanel : public juce::Component,
                 private juce::Timer          // LFO5 変調インジケータの更新用
{
public:
    explicit OutPanel(QuadMorphFilterAudioProcessor& p);
    ~OutPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshTheme();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    /** 【V1.1.0】バー型からアーク式ノブへ変更。
        名前と値はノブの上下に paint() で描くので TextBox は使わない。 */
    void styleKnob(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                   juce::Colour accent, const juce::String& paramId,
                   std::unique_ptr<SliderAtt>& att);

    /** LFO5 の変調レンジと現在位置を Dry/Wet ノブへ書き込む */
    void updateLfo5Indicator();

    void timerCallback() override;

    /** ノブ 1 つ分の描画（名前・ノブ・値）に使う矩形 */
    struct KnobSlot
    {
        juce::Rectangle<int> label;   // 上: 名前
        juce::Rectangle<int> knob;    // 中: ノブ本体
        juce::Rectangle<int> value;   // 下: 数値
    };

    void paintKnobText(juce::Graphics& g, const KnobSlot& slot,
                       const juce::Slider& s, juce::Colour accent) const;

    QuadMorphFilterAudioProcessor& processor;

    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<SliderAtt> mgAtt, dwAtt, clAtt;

    juce::ComboBox osModeCombo;
    std::unique_ptr<ComboAtt> osModeAtt;

    KnobSlot gainSlot, dryWetSlot, ceilingSlot;

    juce::Rectangle<int> outHeaderArea;
    juce::Rectangle<int> qualityHeaderArea, osRowArea, infoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutPanel)
};
