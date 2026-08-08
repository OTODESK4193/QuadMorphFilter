// ==========================================
// UI/OutPanel.cpp   （V1.1.0 新規）
// ==========================================
#include "OutPanel.h"
#include "../PluginProcessor.h"

#ifndef QUADMORPH_VERSION
 #define QUADMORPH_VERSION "1.1.0"
#endif

// ==========================================================================
OutPanel::OutPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    styleBar(masterGainSlider, "Output Gain", QMUI::Unit::Db,
             QMColors::accentOut, "masterGain", mgAtt);
    styleBar(dryWetSlider, "Dry / Wet", QMUI::Unit::Pct,
             QMColors::accentMorph, "dryWet", dwAtt);
    styleBar(ceilingSlider, "Limiter Ceiling", QMUI::Unit::Db,
             QMColors::rose, "limiterCeiling", clAtt);

    osModeCombo.addItemList({ "Off", "Auto", "2x", "4x" }, 1);
    addAndMakeVisible(osModeCombo);
    osModeAtt = std::make_unique<ComboAtt>(processor.apvts, "osMode", osModeCombo);
}

void OutPanel::styleBar(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                        juce::Colour accent, const juce::String& paramId,
                        std::unique_ptr<SliderAtt>& att)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setName(name);
    s.getProperties().set("qmUnit", (int)unit);
    s.setColour(juce::Slider::thumbColourId, accent);
    addAndMakeVisible(s);
    att = std::make_unique<SliderAtt>(processor.apvts, paramId, s);
}

// ==========================================================================
void OutPanel::refreshTheme()
{
    masterGainSlider.setColour(juce::Slider::thumbColourId, QMColors::accentOut);
    dryWetSlider.setColour(juce::Slider::thumbColourId, QMColors::accentMorph);
    ceilingSlider.setColour(juce::Slider::thumbColourId, QMColors::rose);
    repaint();
}

// ==========================================================================
void OutPanel::resized()
{
    auto area = getLocalBounds().reduced(QMUI::kMargin, 0);

    // 出力段は「太めのバー」を縦に 3 本。数値はバー内に出るので余計な部品は不要。
    outHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(6);

    const int barW = juce::jmin(area.getWidth(), 620);

    auto placeBar = [&](juce::Slider& s, juce::Rectangle<int>& store)
    {
        auto row = area.removeFromTop(28);
        store = row.removeFromLeft(barW);
        s.setBounds(store.withSizeKeepingCentre(barW, 26));
        area.removeFromTop(8);
    };

    placeBar(masterGainSlider, gainArea);
    placeBar(dryWetSlider, dryWetArea);
    placeBar(ceilingSlider, ceilingArea);

    area.removeFromTop(10);
    qualityHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(6);

    osRowArea = area.removeFromTop(QMUI::kCtrlH);
    osModeCombo.setBounds(osRowArea.getX() + 150, osRowArea.getY(), 110, QMUI::kCtrlH);

    area.removeFromTop(12);
    infoArea = area.removeFromTop(juce::jmax(0, area.getHeight()));
}

// ==========================================================================
void OutPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "OUTPUT", outHeaderArea, QMColors::accentOut);
    QMUI::drawSectionHeader(g, "QUALITY", qualityHeaderArea, QMColors::accentFilter);

    g.setColour(QMColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("Oversampling", osRowArea.getX(), osRowArea.getY(), 140, osRowArea.getHeight(),
               juce::Justification::centredLeft, false);

    g.setColour(QMColors::textDim.withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
    g.drawText("Auto = raises the rate only when a filter needs it (CPU friendly)",
               osRowArea.getX() + 272, osRowArea.getY(), 420, osRowArea.getHeight(),
               juce::Justification::centredLeft, false);

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
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
        g.drawText("28 filter models  /  XY morphing  /  5 LFOs  /  envelope follower",
                   r.getX(), r.getY(), r.getWidth(), 13, juce::Justification::centredLeft, false);
    }
}
