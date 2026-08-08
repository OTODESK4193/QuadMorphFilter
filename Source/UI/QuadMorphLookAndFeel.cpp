// ==========================================
// UI/QuadMorphLookAndFeel.cpp   （V1.1.0 全面改訂）
// ==========================================
#include "QuadMorphLookAndFeel.h"
#include "UiCommon.h"

QuadMorphLookAndFeel::QuadMorphLookAndFeel()
{
    refreshColours();
}

void QuadMorphLookAndFeel::refreshColours()
{
    setColour(juce::Label::textColourId, QMColors::text);
    setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

    setColour(juce::Slider::textBoxTextColourId, QMColors::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::thumbColourId, QMColors::accentFilter);

    setColour(juce::ComboBox::backgroundColourId, QMColors::track);
    setColour(juce::ComboBox::textColourId, QMColors::text);
    setColour(juce::ComboBox::arrowColourId, QMColors::textDim);
    setColour(juce::ComboBox::outlineColourId, QMColors::panelLine);
    setColour(juce::ComboBox::focusedOutlineColourId, QMColors::accentFilter);

    setColour(juce::TextButton::buttonColourId, QMColors::track);
    setColour(juce::TextButton::textColourOffId, QMColors::textDim);
    setColour(juce::TextButton::textColourOnId, QMColors::accentFilter);

    setColour(juce::PopupMenu::backgroundColourId, QMColors::panel);
    setColour(juce::PopupMenu::textColourId, QMColors::text);
    setColour(juce::PopupMenu::headerTextColourId, QMColors::textDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, QMColors::track.brighter(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, QMColors::text);

    setColour(juce::TooltipWindow::backgroundColourId, QMColors::panel);
    setColour(juce::TooltipWindow::textColourId, QMColors::text);
    setColour(juce::TooltipWindow::outlineColourId, QMColors::panelLine);
}

// ==========================================================================
// バー型スライダー
//   名前を左、値を右にバー内へ直接描く。
//   文字は「塗られた部分」と「未塗り部分」でクリップを分けて 2 回描くので、
//   明るい塗りの上に明るい文字が乗って読めなくなることがない。
// ==========================================================================
void QuadMorphLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y,
                                            int width, int height,
                                            float, float, float,
                                            juce::Slider::SliderStyle,
                                            juce::Slider& slider)
{
    const auto b = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
    const float radius = juce::jmin(5.0f, b.getHeight() * 0.28f);

    auto accent = slider.findColour(juce::Slider::thumbColourId);
    if (accent.isTransparent()) accent = QMColors::accentFilter;
    if (!slider.isEnabled()) accent = QMColors::textDim;

    // --- 溝 ---
    g.setColour(QMColors::track);
    g.fillRoundedRectangle(b, radius);

    // --- 塗り ---
    const float prop = juce::jlimit(0.0f, 1.0f,
        (float)slider.valueToProportionOfLength(slider.getValue()));
    const float fillW = b.getWidth() * prop;

    if (fillW > 1.0f)
    {
        // 溝と同じ角丸で描いてから塗り幅でクリップする。
        // 幅の狭いときに角丸の直径ぶん膨らんで見えるのを防ぐため。
        g.saveState();
        g.reduceClipRegion(b.withWidth(fillW).getSmallestIntegerContainer());
        g.setColour(accent.withAlpha(slider.isEnabled() ? 0.62f : 0.28f));
        g.fillRoundedRectangle(b, radius);
        g.restoreState();
    }

    // --- 現在位置のライン（ハンドル代わり）---
    if (slider.isEnabled() && fillW > 1.5f)
    {
        const float hx = juce::jlimit(b.getX() + 1.0f, b.getRight() - 2.0f,
                                      b.getX() + fillW - 1.0f);
        g.setColour(accent.brighter(0.35f));
        g.fillRect(hx, b.getY() + 1.5f, 1.6f, b.getHeight() - 3.0f);
    }

    // --- 枠 ---
    g.setColour(QMColors::panelLine.withAlpha(0.35f));
    g.drawRoundedRectangle(b.reduced(0.5f), radius, 1.0f);

    // --- 文字（名前 / 値）---
    const auto textArea = b.reduced(7.0f, 0.0f);
    const juce::String nameTxt = slider.getName();
    const juce::String valTxt = QMUI::formatValue(slider.getValue(), QMUI::unitOf(slider));
    const float fontH = juce::jlimit(9.5f, 12.5f, b.getHeight() * 0.55f);

    auto drawPair = [&](juce::Colour nameCol, juce::Colour valCol)
    {
        g.setFont(juce::Font(juce::FontOptions(fontH, juce::Font::bold)));
        if (nameTxt.isNotEmpty())
        {
            g.setColour(nameCol);
            g.drawText(nameTxt, textArea, juce::Justification::centredLeft, false);
        }
        g.setColour(valCol);
        g.drawText(valTxt, textArea, juce::Justification::centredRight, false);
    };

    // 塗られている領域 → 背景色の文字
    if (fillW > 0.5f)
    {
        g.saveState();
        g.reduceClipRegion(b.withWidth(fillW).getSmallestIntegerContainer());
        drawPair(QMColors::bg.withAlpha(0.72f), QMColors::bg);
        g.restoreState();
    }

    // 塗られていない領域 → 明るい文字
    if (fillW < b.getWidth() - 0.5f)
    {
        g.saveState();
        g.reduceClipRegion(b.withTrimmedLeft(fillW).getSmallestIntegerContainer());
        drawPair(QMColors::textDim, QMColors::text);
        g.restoreState();
    }
}

// ==========================================================================
// ComboBox
// ==========================================================================
void QuadMorphLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                        bool, int, int, int, int,
                                        juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height);
    const float radius = 4.0f;

    g.setColour(box.isEnabled() ? QMColors::track : QMColors::track.withAlpha(0.5f));
    g.fillRoundedRectangle(b, radius);

    g.setColour(box.hasKeyboardFocus(true) ? QMColors::accentFilter.withAlpha(0.6f)
                                           : QMColors::panelLine.withAlpha(0.45f));
    g.drawRoundedRectangle(b.reduced(0.5f), radius, 1.0f);

    // 右端の三角
    const float ax = b.getRight() - 14.0f;
    const float ay = b.getCentreY() - 1.5f;
    juce::Path arrow;
    arrow.addTriangle(ax, ay, ax + 7.0f, ay, ax + 3.5f, ay + 4.5f);
    g.setColour(box.isEnabled() ? QMColors::textDim : QMColors::textDim.withAlpha(0.4f));
    g.fillPath(arrow);
}

void QuadMorphLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setMinimumHorizontalScale(0.7f);
}

// ==========================================================================
// ボタン
//   通常  : 角丸のピル。ON でアクセント色の淡い塗り + 枠
//   タブ  : getProperties()["qmTab"] = true。フラット + 下線
// ==========================================================================
void QuadMorphLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour&,
                                                bool isHighlighted, bool isDown)
{
    const bool isTab = (bool)button.getProperties().getWithDefault("qmTab", false);
    const bool isOn = button.getToggleState();
    const auto accent = button.findColour(juce::TextButton::textColourOnId);
    auto b = button.getLocalBounds().toFloat();

    if (isTab)
    {
        if (isOn)
        {
            g.setColour(QMColors::panel);
            g.fillRoundedRectangle(b, 5.0f);
        }
        else if (isHighlighted)
        {
            g.setColour(QMColors::text.withAlpha(0.05f));
            g.fillRoundedRectangle(b, 5.0f);
        }

        if (isOn)
        {
            g.setColour(accent);
            g.fillRoundedRectangle(b.getX() + 8.0f, b.getBottom() - 2.5f,
                                   b.getWidth() - 16.0f, 2.5f, 1.25f);
        }
        return;
    }

    b = b.reduced(1.0f);
    const float radius = juce::jmin(5.0f, b.getHeight() * 0.3f);

    g.setColour(isOn ? accent.withAlpha(0.26f) : QMColors::track);
    g.fillRoundedRectangle(b, radius);

    if (isHighlighted || isDown)
    {
        g.setColour(QMColors::text.withAlpha(isDown ? 0.12f : 0.06f));
        g.fillRoundedRectangle(b, radius);
    }

    g.setColour(isOn ? accent.withAlpha(0.9f) : QMColors::panelLine.withAlpha(0.45f));
    g.drawRoundedRectangle(b.reduced(0.5f), radius, isOn ? 1.2f : 1.0f);
}

void QuadMorphLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool, bool)
{
    const bool isOn = button.getToggleState();
    const auto onCol = button.findColour(juce::TextButton::textColourOnId);
    const auto offCol = button.findColour(juce::TextButton::textColourOffId);

    g.setFont(getTextButtonFont(button, button.getHeight()));
    g.setColour((isOn ? onCol : offCol)
                    .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.45f));

    g.drawText(button.getButtonText(), button.getLocalBounds().reduced(3, 0),
               juce::Justification::centred, false);
}

// ==========================================================================
// フォント
// ==========================================================================
juce::Font QuadMorphLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(juce::FontOptions(11.5f, juce::Font::bold));
}

juce::Font QuadMorphLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(juce::FontOptions(11.5f, juce::Font::bold));
}

juce::Font QuadMorphLookAndFeel::getPopupMenuFont()
{
    return juce::Font(juce::FontOptions(13.5f, juce::Font::plain));
}

juce::Font QuadMorphLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(juce::jlimit(9.5f, 12.5f, (float)buttonHeight * 0.48f),
                                        juce::Font::bold));
}
