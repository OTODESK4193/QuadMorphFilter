// ==========================================
// PluginEditor.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <deque>
#include <vector>
#include "PluginProcessor.h"

class QuadMorphLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuadMorphLookAndFeel();

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class FilterVisualizer : public juce::Component, public juce::Timer
{
public:
    FilterVisualizer(QuadMorphFilterAudioProcessor& p) : processor(p) { startTimerHz(60); }
    void timerCallback() override { repaint(); }
    void paint(juce::Graphics& g) override;
private:
    QuadMorphFilterAudioProcessor& processor;
};

class XYPadComponent : public juce::Component, public juce::Timer
{
public:
    XYPadComponent(QuadMorphFilterAudioProcessor& p);
    void timerCallback() override;
    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
private:
    void updatePosition(const juce::MouseEvent& e);
    QuadMorphFilterAudioProcessor& processor;
    int draggingLfoIndex = -1;

    std::array<juce::Point<float>, 30> trails[3];
    int trailIdx[3] = { 0, 0, 0 };
};

class QuadMorphFilterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    QuadMorphFilterAudioProcessorEditor(QuadMorphFilterAudioProcessor&);
    ~QuadMorphFilterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    QuadMorphFilterAudioProcessor& audioProcessor;
    QuadMorphLookAndFeel customLookAndFeel;
    FilterVisualizer visualizer;
    XYPadComponent xyPad;

    struct FilterGroup {
        juce::TextButton enableButton;
        juce::ComboBox model, type, slope;
        juce::Label cutoffLabel, resLabel;
        juce::Slider cutoff, res;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cAtt, rAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAtt, tAtt, slAtt;
    };
    FilterGroup groupA, groupB, groupC, groupD;

    struct LfoGroup {
        juce::TextButton enableButton;
        juce::ComboBox wave, rateSync, boundCombo;
        juce::TextButton stepMode, syncToggle;
        juce::Slider rateFree, minSlider, maxSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eAtt, sAtt, syAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rfAtt, minAtt, maxAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wAtt, rsAtt, bAtt;
    };
    LfoGroup lfos[3];

    juce::Label masterGainLabel, dryWetLabel, ceilingLabel;
    juce::Slider masterGainSlider, dryWetSlider, ceilingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mgAtt, dwAtt, clAtt;

    void setupFilterGroup(FilterGroup& g, juce::String s, juce::String name);
    void setupLfoGroup(LfoGroup& g, int index, juce::String name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadMorphFilterAudioProcessorEditor)
};