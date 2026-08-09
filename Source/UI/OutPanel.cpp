// ==========================================
// UI/OutPanel.cpp   （V1.1.0 新規 / ノブ化 改訂）
// ==========================================
#include "OutPanel.h"
#include "../PluginProcessor.h"
#include <cmath>   // std::abs（LFO5 インジケータの差分判定）

#ifndef QUADMORPH_VERSION
 #define QUADMORPH_VERSION "1.1.0"
#endif

namespace
{
    // ノブ 1 つ分の占有サイズ
    constexpr int kKnobW = 120;   // 列幅
    constexpr int kKnobD = 74;    // ノブ直径
    constexpr int kLabelH = 14;   // 上の名前
    constexpr int kValueH = 16;   // 下の数値
    constexpr int kKnobGap = 26;  // 列間
}

// ==========================================================================
OutPanel::OutPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    styleKnob(masterGainSlider, "OUTPUT GAIN", QMUI::Unit::Db,
              QMColors::accentOut, "masterGain", mgAtt);
    styleKnob(dryWetSlider, "DRY / WET", QMUI::Unit::Pct,
              QMColors::accentMorph, "dryWet", dwAtt);
    styleKnob(ceilingSlider, "CEILING", QMUI::Unit::Db,
              QMColors::rose, "limiterCeiling", clAtt);

    dryWetSlider.setTooltip("Wet amount. When LFO5 is enabled the ring shows its "
                            "range and the dot follows the live value.");

    osModeCombo.addItemList({ "Off", "Auto", "2x", "4x" }, 1);
    addAndMakeVisible(osModeCombo);
    osModeAtt = std::make_unique<ComboAtt>(processor.apvts, "osMode", osModeCombo);

    updateLfo5Indicator();
    startTimerHz(30);
}

OutPanel::~OutPanel()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
}

// ==========================================================================
void OutPanel::styleKnob(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                         juce::Colour accent, const juce::String& paramId,
                         std::unique_ptr<SliderAtt>& att)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                          juce::MathConstants<float>::pi * 2.8f, true);
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
// updateLfo5Indicator
// LFO5 は Dry/Wet を Min〜Max の範囲で振る。
// dryWet パラメータのレンジも 0〜100 なので、正規化はそのまま割るだけでよい。
// ==========================================================================
void OutPanel::updateLfo5Indicator()
{
    auto& apvts = processor.apvts;
    auto raw = [&apvts](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    const bool on = raw("lfo5en") > 0.5f;
    auto& props = dryWetSlider.getProperties();

    if (!on)
    {
        if ((bool)props.getWithDefault("qmModOn", false))
        {
            props.set("qmModOn", false);
            dryWetSlider.repaint();
        }
        return;
    }

    const float lo = juce::jlimit(0.0f, 1.0f, raw("lfo5min") / 100.0f);
    const float hi = juce::jlimit(0.0f, 1.0f, raw("lfo5max") / 100.0f);
    const float live = juce::jlimit(0.0f, 1.0f, processor.getLfo5Output());

    const double prev = props.getWithDefault("qmModNorm", -1.0);
    const bool wasOn = (bool)props.getWithDefault("qmModOn", false);

    props.set("qmModOn", true);
    props.set("qmModMin", (double)lo);
    props.set("qmModMax", (double)hi);

    // 位置が実質変わっていないフレームは repaint を省く
    if (!wasOn || std::abs((double)live - prev) > 0.0015)
    {
        props.set("qmModNorm", (double)live);
        props.set("qmModValue", (double)(live * 100.0f));   // 数値表示用（%）
        dryWetSlider.repaint();
    }
}

void OutPanel::timerCallback()
{
    if (isVisible())
        updateLfo5Indicator();
}

// ==========================================================================
void OutPanel::resized()
{
    auto area = getLocalBounds().reduced(QMUI::kMargin, 0);

    outHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(10);

    // ---- ノブ 3 つを横並び ----
    auto knobRow = area.removeFromTop(kLabelH + kKnobD + kValueH + 6);

    auto place = [&](juce::Slider& s, KnobSlot& slot)
    {
        auto col = knobRow.removeFromLeft(kKnobW);
        slot.label = col.removeFromTop(kLabelH);
        slot.knob = col.removeFromTop(kKnobD);
        col.removeFromTop(4);
        slot.value = col.removeFromTop(kValueH);

        s.setBounds(slot.knob.withSizeKeepingCentre(kKnobD, kKnobD));
        knobRow.removeFromLeft(kKnobGap);
    };

    place(masterGainSlider, gainSlot);
    place(dryWetSlider, dryWetSlot);
    place(ceilingSlider, ceilingSlot);

    area.removeFromTop(16);
    qualityHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(6);

    osRowArea = area.removeFromTop(QMUI::kCtrlH);
    osModeCombo.setBounds(osRowArea.getX() + 150, osRowArea.getY(), 110, QMUI::kCtrlH);

    area.removeFromTop(12);
    infoArea = area.removeFromTop(juce::jmax(0, area.getHeight()));
}

// ==========================================================================
void OutPanel::paintKnobText(juce::Graphics& g, const KnobSlot& slot,
                             const juce::Slider& s, juce::Colour accent) const
{
    // ---- 名前 ----
    g.setColour(QMColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(s.getName(), slot.label, juce::Justification::centred, false);

    // ---- 数値 ----
    // 変調中は実際に効いている値を出す（LookAndFeel 側と同じ考え方）。
    const auto& props = s.getProperties();
    const bool modOn = (bool)props.getWithDefault("qmModOn", false);
    const double shown = modOn
        ? (double)props.getWithDefault("qmModValue", s.getValue())
        : s.getValue();

    g.setColour(modOn ? accent.brighter(0.4f) : QMColors::text);
    g.setFont(QMFonts::mono(12.0f, true));
    g.drawText(QMUI::formatValue(shown, QMUI::unitOf(s)),
               slot.value, juce::Justification::centred, false);
}

// ==========================================================================
void OutPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "OUTPUT", outHeaderArea, QMColors::accentOut);
    QMUI::drawSectionHeader(g, "QUALITY", qualityHeaderArea, QMColors::accentFilter);

    paintKnobText(g, gainSlot, masterGainSlider, QMColors::accentOut);
    paintKnobText(g, dryWetSlot, dryWetSlider, QMColors::accentMorph);
    paintKnobText(g, ceilingSlot, ceilingSlider, QMColors::rose);

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
