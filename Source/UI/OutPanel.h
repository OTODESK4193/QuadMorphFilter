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

class OutPanel : public juce::Component
{
public:
    explicit OutPanel(QuadMorphFilterAudioProcessor& p);
    ~OutPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshTheme();

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void styleBar(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                  juce::Colour accent, const juce::String& paramId,
                  std::unique_ptr<SliderAtt>& att);

    QuadMorphFilterAudioProcessor& processor;

    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<SliderAtt> mgAtt, dwAtt, clAtt;

    juce::ComboBox osModeCombo;
    std::unique_ptr<ComboAtt> osModeAtt;

    juce::Rectangle<int> outHeaderArea, gainArea, dryWetArea, ceilingArea;
    juce::Rectangle<int> qualityHeaderArea, osRowArea, infoArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutPanel)
};
