// ==========================================
// UI/FilterVisualizer.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

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
};