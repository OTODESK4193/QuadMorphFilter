// ==========================================
// UI/ModPanel.h   （V1.1.0 新規）
//
// MOD タブ。変調系をすべてここに集約する。
//   ・LFO 1 (Morph) / LFO 2 (Cutoff) / LFO 3 (Reso)
//   ・LFO 4 : 他 LFO の Rate を変調
//   ・LFO 5 : Dry/Wet レンジを変調
//   ・Envelope Follower
// ==========================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

#include "ColorPalette.h"
#include "UiCommon.h"

class QuadMorphFilterAudioProcessor;

class ModPanel : public juce::Component,
                 private juce::Timer
{
public:
    explicit ModPanel(QuadMorphFilterAudioProcessor& p);
    ~ModPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshTheme();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    /** LFO 1〜3 共通（Min / Max / Phase / Fade / Spread を持つ） */
    struct LfoGroup
    {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync;
        juce::Slider     rateFree, minSlider, maxSlider, phaseSlider, fadeSlider, spreadSlider;

        std::unique_ptr<ButtonAtt> eAtt, sAtt, syAtt;
        std::unique_ptr<ComboAtt>  wAtt, rsAtt;
        std::unique_ptr<SliderAtt> rfAtt, minAtt, maxAtt, phaseAtt, fadeAtt, spreadAtt;
    };

    /** LFO 4 : Rate 変調 */
    struct Lfo4Group
    {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync;
        juce::Slider     rateFree, depthSlider;
        juce::TextButton assign[3];

        std::unique_ptr<ButtonAtt> eAtt, sAtt, syAtt, assignAtt[3];
        std::unique_ptr<ComboAtt>  wAtt, rsAtt;
        std::unique_ptr<SliderAtt> rfAtt, depthAtt;
    };

    /** LFO 5 : Dry/Wet レンジ変調 */
    struct Lfo5Group
    {
        juce::TextButton enableButton, stepMode, syncToggle;
        juce::ComboBox   wave, rateSync;
        juce::Slider     rateFree, minSlider, maxSlider;

        std::unique_ptr<ButtonAtt> eAtt, sAtt, syAtt;
        std::unique_ptr<ComboAtt>  wAtt, rsAtt;
        std::unique_ptr<SliderAtt> rfAtt, minAtt, maxAtt;
    };

    struct EnvGroup
    {
        juce::TextButton enableButton, invertButton;
        juce::Slider     depthSlider;

        std::unique_ptr<ButtonAtt> eAtt, invAtt;
        std::unique_ptr<SliderAtt> depthAtt;
    };

    // ---- 生成ヘルパー ----
    void styleBar(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                  juce::Colour accent, const juce::String& paramId,
                  std::unique_ptr<SliderAtt>& att);
    void styleToggle(juce::TextButton& b, const juce::String& text, juce::Colour accent,
                     const juce::String& paramId, std::unique_ptr<ButtonAtt>& att);
    void styleWave(juce::ComboBox& c, const juce::String& paramId, std::unique_ptr<ComboAtt>& att);
    void styleRateSync(juce::ComboBox& c, const juce::String& paramId, std::unique_ptr<ComboAtt>& att);

    void timerCallback() override;

    /** 【V1.1.0 追加】LFO4 によるレート変調を LFO1/2/3 の表示へ反映する。
        Free モード : rateFree スライダーに変調帯とライブ位置を描かせる
        Sync モード : rateSync コンボの右に実効レート [Hz] を動的表示する
        値は LfoEngine が実際に位相を進めるのに使った最終レートなので、
        表示と実挙動がずれない。 */
    void updateRateModIndicators();

    /** Sync モードで表示する実効レート [Hz]（0 = 表示しない） */
    float effectiveRateHz[3] = { 0.0f, 0.0f, 0.0f };
    bool  rateModShown[3] = { false, false, false };
    bool isSynced(const juce::String& paramId) const;

    /** 行の共通カラム（Enable / Wave / Step / Sync / 可変 6 スロット） */
    struct RowLayout
    {
        juce::Rectangle<int> enable, wave, step, sync;
        std::array<juce::Rectangle<int>, 6> slot;
    };
    RowLayout layoutRow(juce::Rectangle<int> row) const;

    QuadMorphFilterAudioProcessor& processor;

    std::array<LfoGroup, 3> lfos;
    Lfo4Group lfo4;
    Lfo5Group lfo5;
    EnvGroup  env;

    juce::Rectangle<int> headerArea, columnArea, extraHeaderArea;
    std::array<juce::Rectangle<int>, 3> lfoRowAreas;
    juce::Rectangle<int> lfo4RowArea, lfo5RowArea, envRowArea;

    // Sync のオン/オフが変わったら Rate 表示（スライダー↔コンボ）を差し替える。
    // プリセット読み込みでも切り替わるため、クリック検知だけでなく監視もする。
    bool lastSyncState[5] = { true, true, true, false, false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModPanel)
};
