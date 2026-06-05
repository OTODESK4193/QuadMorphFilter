// ==========================================
// UI/FilterVisualizer.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// 前方宣言: .cpp で完全なインクルードを行う
class QuadMorphFilterAudioProcessor;
class QuadMorphFilterAudioProcessorEditor;

class FilterVisualizer : public juce::Component,
    public juce::Timer
{
public:
    explicit FilterVisualizer(QuadMorphFilterAudioProcessor& p, QuadMorphFilterAudioProcessorEditor* editor = nullptr);
    ~FilterVisualizer() override;
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void setEditorPointer(QuadMorphFilterAudioProcessorEditor* ed) { editor = ed; }

private:
    QuadMorphFilterAudioProcessor& processor;
    QuadMorphFilterAudioProcessorEditor* editor = nullptr;

    // paint() 内アロケーション禁止 (CLAUDE.md §4) のためメンバーに移行。
    // FilterVisualizer の幅は最大 ~700px なので 1024 で十分。
    std::array<float, 1024> rawMag {};

    // Background color randomization
    juce::Colour bgColour = juce::Colour(0xff2a2a2a);  // Default dark color
    void randomizeBackgroundColour();
    void resetBackgroundColour();
    bool hitTestEButton(int x, int y) const;
    void notifyBackgroundColourChanged();
};