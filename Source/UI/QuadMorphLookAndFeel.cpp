// ==========================================
// UI/QuadMorphLookAndFeel.cpp
// ==========================================
#include "QuadMorphLookAndFeel.h"

QuadMorphLookAndFeel::QuadMorphLookAndFeel()
{
    setColour(juce::Label::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff2C3E50));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xff2C3E50));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xffFFFFFF));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xff2C3E50));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xffE0E6ED));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff2C3E50));
}

void QuadMorphLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
    int width, int height,
    float sliderPos, float, float,
    const juce::Slider::SliderStyle,
    juce::Slider& slider)
{
    auto trackRect = juce::Rectangle<float>(static_cast<float>(x),
        (float)y + (float)height * 0.5f - 1.5f,
        (float)width, 3.0f);
    g.setColour(juce::Colour(0xffE0E6ED));
    g.fillRoundedRectangle(trackRect, 1.5f);

    auto activeColour = slider.findColour(juce::Slider::thumbColourId);
    if (activeColour.isTransparent()) activeColour = juce::Colour(0xff7F8C8D);

    g.setColour(activeColour.withAlpha(0.7f));
    g.fillRoundedRectangle(trackRect.withWidth(sliderPos - (float)x), 1.5f);

    g.setColour(activeColour);
    g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 6.0f, 12.0f, 12.0f);
}

void QuadMorphLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
    bool, int, int, int, int, juce::ComboBox&)
{
    juce::Rectangle<int> boxBounds(0, 0, width, height);
    g.setColour(juce::Colour(0xffFFFFFF));
    g.fillRoundedRectangle(boxBounds.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
    g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f), 4.0f, 1.0f);

    juce::Path path;
    float arrowX = width - 15.0f;
    float arrowY = height * 0.45f;
    path.addTriangle(arrowX, arrowY, arrowX + 8.0f, arrowY, arrowX + 4.0f, arrowY + 5.0f);
    g.setColour(juce::Colour(0xff95A5A6));
    g.fillPath(path);
}

void QuadMorphLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    bool isOn = button.getToggleState();

    auto baseColour = isOn ? button.findColour(juce::TextButton::textColourOnId).withAlpha(0.15f)
        : juce::Colour(0xffFFFFFF);
    auto borderColour = isOn ? button.findColour(juce::TextButton::textColourOnId)
        : juce::Colour(0xffD5DDE5);

    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds, 4.0f, 1.2f);
}