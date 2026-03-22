#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include "PluginProcessor.h"

// フィルターカーブ描画コンポーネント（前回実装のまま）
class FilterVisualizer : public juce::Component, public juce::Timer
{
public:
    FilterVisualizer(QuadMorphFilterAudioProcessor& p) : processor(p) { startTimerHz(60); }

    void timerCallback() override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        // 描画ロジックは前回と同じ（image_0.pngのようなシアンのカーブを描画）
        g.fillAll(juce::Colours::black.withAlpha(0.5f));
        g.setColour(juce::Colours::cyan);

        juce::Path path;
        auto bounds = getLocalBounds().toFloat();
        auto w = bounds.getWidth();
        auto h = bounds.getHeight();

        // 20Hz ~ 20kHz を対数スケールで描画
        for (int x = 0; x <= (int)w; ++x)
        {
            float freq = 20.0f * std::pow(1000.0f, x / w);
            float mag = calculateMagnitude(freq);

            // 表示位置の調整
            float y = h - (mag * h * 0.4f + h * 0.2f);
            y = juce::jlimit(0.0f, h, y);

            if (x == 0) path.startNewSubPath(0, y);
            else path.lineTo((float)x, y);
        }
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    float calculateMagnitude(float freq)
    {
        float fc = processor.apvts.getRawParameterValue("cutoff")->load();
        float res = processor.apvts.getRawParameterValue("resonance")->load();
        int type = (int)processor.apvts.getRawParameterValue("type")->load();

        float r = freq / fc;
        float r2 = r * r;
        float damping = 1.0f / res;
        float den = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * damping, 2.0f));

        if (type == 0) return 1.0f / den; // LP
        if (type == 1) return r / den;    // BP
        if (type == 2) return r2 / den;   // HP
        return std::abs(1.0f - r2) / den; // Notch
    }

private:
    QuadMorphFilterAudioProcessor& processor;
};

// プラグイン・エディター本体
class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor&);
    ~QuadMorphFilterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    QuadMorphFilterAudioProcessor& audioProcessor;

    // カーブ描画コンポーネント
    FilterVisualizer visualizer;

    // 【追加】スライダーとコンボボックス、およびラベル
    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;

    juce::Slider resonanceSlider;
    juce::Label resonanceLabel;

    juce::ComboBox typeComboBox;
    juce::Label typeLabel;

    // 【追加】APVTSとのアタッチメント（SliderAttachment / ComboBoxAttachment）
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};