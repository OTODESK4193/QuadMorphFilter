// ==========================================
// UI/ConfigPanel.h   （V1.1.0 / 旧 OutPanel を再構成）
//
// CONFIG タブ。曲づくり中に頻繁には触らない設定をまとめる。
//   ・MORPH   : Blend / Cutoff Algo / XY Depth（FILTER タブから移設）
//   ・DISPLAY : カラーテーマ（ヘッダーから移設）
//   ・QUALITY : オーバーサンプリング
//   ・プラグイン情報
//
// 出力段（Gain / Dry-Wet / Ceiling）は FILTER タブへ移した。
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

#include "ColorPalette.h"
#include "UiCommon.h"

class QuadMorphFilterAudioProcessor;

class ConfigPanel : public juce::Component
{
public:
    explicit ConfigPanel(QuadMorphFilterAudioProcessor& p);
    ~ConfigPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshTheme();

    /** テーマが変わったときにエディタ側へ通知する（色の貼り直し用）。
        ヘッダーにあった themeCombo をここへ移したため、
        エディタが直接 onChange を持てなくなった分をコールバックで補う。 */
    std::function<void(int)> onThemeChanged;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    QuadMorphFilterAudioProcessor& processor;

    // ---- MORPH ----
    juce::ComboBox morphBlendCombo, cutoffAlgoCombo;
    std::unique_ptr<ComboAtt> morphBlendAtt, cutoffAlgoAtt;
    juce::Slider xyDepthSlider;
    std::unique_ptr<SliderAtt> xyDepthAtt;

    // ---- DISPLAY ----
    juce::ComboBox themeCombo;
    std::unique_ptr<ComboAtt> themeAtt;

    // ---- QUALITY ----
    juce::ComboBox osModeCombo;
    std::unique_ptr<ComboAtt> osModeAtt;

    juce::Rectangle<int> morphHeaderArea, morphRow1, morphRow2;
    juce::Rectangle<int> displayHeaderArea, displayRowArea;
    juce::Rectangle<int> qualityHeaderArea, osRowArea, infoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConfigPanel)
};
