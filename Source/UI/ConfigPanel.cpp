// ==========================================
// UI/ConfigPanel.cpp   （V1.1.0 / 旧 OutPanel を再構成）
// ==========================================
#include "ConfigPanel.h"
#include "../PluginProcessor.h"

#ifndef QUADMORPH_VERSION
 #define QUADMORPH_VERSION "1.1.0"
#endif

namespace
{
    constexpr int kLabelW = 92;    // 左端のラベル幅
    constexpr int kComboW = 150;
    constexpr int kBarW = 190;
    constexpr int kHintX = 260;    // 右側の補足テキスト開始位置
}

// ==========================================================================
ConfigPanel::ConfigPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    // ---- MORPH ----
    morphBlendCombo.addItemList({ "Equal Power", "Linear", "Smoothstep", "Radial" }, 1);
    addAndMakeVisible(morphBlendCombo);
    morphBlendAtt = std::make_unique<ComboAtt>(processor.apvts, "morphBlend", morphBlendCombo);
    QMUI::setInfo(morphBlendCombo,
        "MORPH BLEND  -  how the four filters are mixed as the XY position moves. "
        "Equal Power keeps the level steady across the pad. Linear is a plain bilinear "
        "crossfade and dips in the middle. Smoothstep pulls towards the corners. "
        "Radial makes the nearest corner dominant for a sharp four-way split.");

    cutoffAlgoCombo.addItemList({ "Absolute", "Relative", "Zone" }, 1);
    addAndMakeVisible(cutoffAlgoCombo);
    cutoffAlgoAtt = std::make_unique<ComboAtt>(processor.apvts, "cutoffAlgo", cutoffAlgoCombo);
    QMUI::setInfo(cutoffAlgoCombo,
        "CUTOFF ALGO  -  how the XY position maps onto cutoff and resonance. "
        "Absolute sweeps the full 20 Hz to 20 kHz range. Relative gives plus or minus "
        "four octaves around 632 Hz. Zone uses asymmetric ranges for the upper and "
        "lower halves. This has no effect until XY DEPTH is raised above zero.");

    xyDepthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    xyDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    xyDepthSlider.setName("XY Depth");
    xyDepthSlider.getProperties().set("qmUnit", (int)QMUI::Unit::Pct);
    xyDepthSlider.setColour(juce::Slider::thumbColourId, QMColors::accentMorph);
    addAndMakeVisible(xyDepthSlider);
    xyDepthAtt = std::make_unique<SliderAtt>(processor.apvts, "xyDepth", xyDepthSlider);
    QMUI::setInfo(xyDepthSlider,
        "XY DEPTH  -  how strongly the Cutoff Algo mapping drives every filter's cutoff "
        "and resonance. At 0% the Cut and Res sliders are used unchanged, so the sound "
        "matches V1.0.0. Raise it and moving the XY pad sweeps the filters as well as "
        "morphing between them.");

    // ---- DISPLAY ----
    themeCombo.addItemList(QMColors::getThemeNames(), 1);
    addAndMakeVisible(themeCombo);
    themeAtt = std::make_unique<ComboAtt>(processor.apvts, "colorTheme", themeCombo);
    QMUI::setInfo(themeCombo,
        "COLOR THEME  -  display only, it never touches the audio. The choice is stored "
        "with the plugin state, so a project reopens with the same look.");

    // ComboBoxAttachment は Listener 経由なので onChange は自由に使える
    themeCombo.onChange = [this]
    {
        if (onThemeChanged != nullptr)
            onThemeChanged(themeCombo.getSelectedItemIndex());
    };

    // ---- QUALITY ----
    osModeCombo.addItemList({ "Off", "Auto", "2x", "4x" }, 1);
    addAndMakeVisible(osModeCombo);
    osModeAtt = std::make_unique<ComboAtt>(processor.apvts, "osMode", osModeCombo);
    QMUI::setInfo(osModeCombo,
        "OVERSAMPLING  -  runs the filters at a higher internal rate to reduce aliasing "
        "from the nonlinear models. Auto raises the rate only for the models that need "
        "it, which is the best CPU trade-off. Higher settings add latency, which is "
        "reported to the host so delay compensation stays correct.");
}

// ==========================================================================
void ConfigPanel::refreshTheme()
{
    xyDepthSlider.setColour(juce::Slider::thumbColourId, QMColors::accentMorph);
    repaint();
}

// ==========================================================================
void ConfigPanel::resized()
{
    auto area = getLocalBounds().reduced(QMUI::kMargin, 0);

    // ---- MORPH ----
    morphHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(8);

    morphRow1 = area.removeFromTop(QMUI::kCtrlH);
    {
        auto r = morphRow1;
        r.removeFromLeft(kLabelW);
        morphBlendCombo.setBounds(r.removeFromLeft(kComboW));
    }
    area.removeFromTop(8);

    morphRow2 = area.removeFromTop(QMUI::kCtrlH);
    {
        auto r = morphRow2;
        r.removeFromLeft(kLabelW);
        cutoffAlgoCombo.setBounds(r.removeFromLeft(kComboW));
        r.removeFromLeft(24);
        r.removeFromLeft(74);   // "XY DEPTH" ラベル分
        xyDepthSlider.setBounds(r.removeFromLeft(kBarW)
                                    .withSizeKeepingCentre(kBarW, QMUI::kBarH));
    }

    // ---- DISPLAY ----
    area.removeFromTop(18);
    displayHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(8);

    displayRowArea = area.removeFromTop(QMUI::kCtrlH);
    {
        auto r = displayRowArea;
        r.removeFromLeft(kLabelW);
        themeCombo.setBounds(r.removeFromLeft(kComboW));
    }

    // ---- QUALITY ----
    area.removeFromTop(18);
    qualityHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(8);

    osRowArea = area.removeFromTop(QMUI::kCtrlH);
    {
        auto r = osRowArea;
        r.removeFromLeft(kLabelW);
        osModeCombo.setBounds(r.removeFromLeft(kComboW));
    }

    area.removeFromTop(16);
    infoArea = area.removeFromTop(juce::jmax(0, area.getHeight()));
}

// ==========================================================================
void ConfigPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "MORPH", morphHeaderArea, QMColors::accentMorph);
    QMUI::drawSectionHeader(g, "DISPLAY", displayHeaderArea, QMColors::accentFilter);
    QMUI::drawSectionHeader(g, "QUALITY", qualityHeaderArea, QMColors::accentOut);

    auto rowLabel = [&g](const juce::String& t, juce::Rectangle<int> row)
    {
        g.setColour(QMColors::textDim);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(t, row.getX(), row.getY(), kLabelW - 6, row.getHeight(),
                   juce::Justification::centredLeft, false);
    };

    auto rowHint = [&g](const juce::String& t, juce::Rectangle<int> row, int x)
    {
        g.setColour(QMColors::textDim.withAlpha(0.7f));
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText(t, row.getX() + x, row.getY(), row.getWidth() - x, row.getHeight(),
                   juce::Justification::centredLeft, false);
    };

    rowLabel("BLEND", morphRow1);
    rowHint("How the four filters are mixed across the XY pad", morphRow1, kHintX);

    rowLabel("CUTOFF ALGO", morphRow2);
    g.setColour(QMColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("XY DEPTH", morphRow2.getX() + kLabelW + kComboW + 24, morphRow2.getY(),
               70, morphRow2.getHeight(), juce::Justification::centredLeft, false);

    rowLabel("THEME", displayRowArea);
    rowHint("Display only  -  never affects the audio", displayRowArea, kHintX);

    rowLabel("OVERSAMPLING", osRowArea);
    rowHint("Auto = raises the rate only when a filter needs it (CPU friendly)",
            osRowArea, kHintX);

    // ---- 情報 ----
    if (!infoArea.isEmpty())
    {
        auto r = infoArea;

        g.setColour(QMColors::panelLine.withAlpha(0.4f));
        g.fillRect(r.getX(), r.getY(), r.getWidth(), 1);
        r.removeFromTop(10);

        g.setColour(QMColors::text.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("Quad-Morph Filter  V" QUADMORPH_VERSION "   /   OTODESK",
                   r.getX(), r.getY(), r.getWidth(), 14, juce::Justification::centredLeft, false);

        r.removeFromTop(16);
        g.setColour(QMColors::textDim.withAlpha(0.8f));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("28 filter models  /  XY morphing  /  5 LFOs  /  envelope follower",
                   r.getX(), r.getY(), r.getWidth(), 13, juce::Justification::centredLeft, false);
    }
}
