// ==========================================
// UI/QuadMorphLookAndFeel.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class QuadMorphLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuadMorphLookAndFeel();

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH,
        juce::ComboBox& box) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
};