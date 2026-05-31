// ==========================================
// UI/FilterVisualizer.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// 前方宣言: .cpp で完全なインクルードを行う
class QuadMorphFilterAudioProcessor;

class FilterVisualizer : public juce::Component,
    public juce::Timer
{
public:
    explicit FilterVisualizer(QuadMorphFilterAudioProcessor& p);
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics& g) override;

private:
    QuadMorphFilterAudioProcessor& processor;

    // paint() 内アロケーション禁止 (CLAUDE.md §4) のためメンバーに移行。
    // FilterVisualizer の幅は最大 ~700px なので 1024 で十分。
    std::array<float, 1024> rawMag {};
};