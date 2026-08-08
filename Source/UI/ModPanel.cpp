// ==========================================
// UI/ModPanel.cpp   （V1.1.0 新規）
// ==========================================
#include "ModPanel.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr int kEnW = 92;
    constexpr int kWaveW = 116;
    constexpr int kSmallW = 46;
    constexpr int kGap = 5;

    const juce::StringArray kWaveNames = {
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard",
        "Polygon", "Attractor Orbit"
    };

    const juce::StringArray kSyncRates = {
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
    };

    const char* const kLfoTitles[3] = { "1  MORPH", "2  CUTOFF", "3  RESO" };
}

// ==========================================================================
ModPanel::ModPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    // ---------------- LFO 1 / 2 / 3 ----------------
    for (int i = 0; i < 3; ++i)
    {
        auto& g = lfos[(size_t)i];
        const juce::String id = "lfo" + juce::String(i + 1);
        const auto accent = QMColors::lfoColour(i);

        styleToggle(g.enableButton, kLfoTitles[i], accent, id + "en", g.eAtt);
        styleWave(g.wave, id + "wave", g.wAtt);
        styleToggle(g.stepMode, "Step", accent, id + "step", g.sAtt);
        styleToggle(g.syncToggle, "Sync", accent, id + "sync", g.syAtt);
        styleRateSync(g.rateSync, id + "rateSync", g.rsAtt);

        styleBar(g.rateFree, "Rate", QMUI::Unit::Hz, accent, id + "rateFree", g.rfAtt);
        styleBar(g.minSlider, "Min", QMUI::Unit::Pct01, accent, id + "min", g.minAtt);
        styleBar(g.maxSlider, "Max", QMUI::Unit::Pct01, accent, id + "max", g.maxAtt);
        styleBar(g.phaseSlider, "Phase", QMUI::Unit::Deg, accent, id + "phase", g.phaseAtt);
        styleBar(g.fadeSlider, "Fade", QMUI::Unit::Sec, accent, id + "fade", g.fadeAtt);
        styleBar(g.spreadSlider, "Sprd", QMUI::Unit::Deg, accent, id + "spread", g.spreadAtt);

        g.syncToggle.onClick = [this] { resized(); };
        lastSyncState[i] = isSynced("lfo" + juce::String(i + 1) + "sync");
    }

    // ---------------- LFO 4（Rate 変調）----------------
    {
        const auto accent = QMColors::lfoColour(3);

        styleToggle(lfo4.enableButton, "4  RATE MOD", accent, "lfo4en", lfo4.eAtt);
        styleWave(lfo4.wave, "lfo4wave", lfo4.wAtt);
        styleToggle(lfo4.stepMode, "Step", accent, "lfo4step", lfo4.sAtt);
        styleToggle(lfo4.syncToggle, "Sync", accent, "lfo4sync", lfo4.syAtt);
        styleRateSync(lfo4.rateSync, "lfo4rateSync", lfo4.rsAtt);

        styleBar(lfo4.rateFree, "Rate", QMUI::Unit::Hz, accent, "lfo4rateFree", lfo4.rfAtt);
        styleBar(lfo4.depthSlider, "Depth", QMUI::Unit::Ratio, accent, "lfo4depth", lfo4.depthAtt);

        const char* const assignIds[3] = { "lfo4assignA", "lfo4assignB", "lfo4assignC" };
        const char* const assignText[3] = { "> LFO 1", "> LFO 2", "> LFO 3" };
        for (int i = 0; i < 3; ++i)
            styleToggle(lfo4.assign[i], assignText[i], QMColors::lfoColour(i),
                        assignIds[i], lfo4.assignAtt[i]);

        lfo4.syncToggle.onClick = [this] { resized(); };
        lastSyncState[3] = isSynced("lfo4sync");
    }

    // ---------------- LFO 5（Dry/Wet 変調）----------------
    {
        const auto accent = QMColors::lfoColour(4);

        styleToggle(lfo5.enableButton, "5  DRY/WET", accent, "lfo5en", lfo5.eAtt);
        styleWave(lfo5.wave, "lfo5wave", lfo5.wAtt);
        styleToggle(lfo5.stepMode, "Step", accent, "lfo5step", lfo5.sAtt);
        styleToggle(lfo5.syncToggle, "Sync", accent, "lfo5sync", lfo5.syAtt);
        styleRateSync(lfo5.rateSync, "lfo5rateSync", lfo5.rsAtt);

        styleBar(lfo5.rateFree, "Rate", QMUI::Unit::Hz, accent, "lfo5rateFree", lfo5.rfAtt);
        styleBar(lfo5.minSlider, "Min", QMUI::Unit::Pct, accent, "lfo5min", lfo5.minAtt);
        styleBar(lfo5.maxSlider, "Max", QMUI::Unit::Pct, accent, "lfo5max", lfo5.maxAtt);

        lfo5.syncToggle.onClick = [this] { resized(); };
        lastSyncState[4] = isSynced("lfo5sync");
    }

    // ---------------- Envelope Follower ----------------
    {
        const auto accent = QMColors::envColour();

        styleToggle(env.enableButton, "ENV FOLLOW", accent, "envFollowen", env.eAtt);
        styleToggle(env.invertButton, "Invert", accent, "envFollowinvert", env.invAtt);
        styleBar(env.depthSlider, "Depth", QMUI::Unit::Pct, accent, "envFollowdepth", env.depthAtt);
    }

    startTimerHz(8);
}

ModPanel::~ModPanel()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
}

// ==========================================================================
// 生成ヘルパー
// ==========================================================================
void ModPanel::styleBar(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
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

void ModPanel::styleToggle(juce::TextButton& b, const juce::String& text, juce::Colour accent,
                           const juce::String& paramId, std::unique_ptr<ButtonAtt>& att)
{
    b.setButtonText(text);
    b.setClickingTogglesState(true);
    b.setColour(juce::TextButton::textColourOnId, accent);
    b.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
    addAndMakeVisible(b);
    att = std::make_unique<ButtonAtt>(processor.apvts, paramId, b);
}

void ModPanel::styleWave(juce::ComboBox& c, const juce::String& paramId,
                         std::unique_ptr<ComboAtt>& att)
{
    c.addItemList(kWaveNames, 1);
    addAndMakeVisible(c);
    att = std::make_unique<ComboAtt>(processor.apvts, paramId, c);
}

void ModPanel::styleRateSync(juce::ComboBox& c, const juce::String& paramId,
                             std::unique_ptr<ComboAtt>& att)
{
    c.addItemList(kSyncRates, 1);
    c.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(c);
    att = std::make_unique<ComboAtt>(processor.apvts, paramId, c);
}

bool ModPanel::isSynced(const juce::String& paramId) const
{
    return processor.apvts.getRawParameterValue(paramId)->load() > 0.5f;
}

// ==========================================================================
// Sync の状態変化を監視して Rate 表示を差し替える。
// （プリセット読み込みなど、クリック以外で変わるケースを拾うため）
// ==========================================================================
void ModPanel::timerCallback()
{
    if (!isVisible()) return;

    static const char* const syncIds[5] = {
        "lfo1sync", "lfo2sync", "lfo3sync", "lfo4sync", "lfo5sync"
    };

    for (int i = 0; i < 5; ++i)
    {
        const bool now = isSynced(syncIds[i]);
        if (now != lastSyncState[i])
        {
            lastSyncState[i] = now;
            resized();
            return;
        }
    }
}

// ==========================================================================
void ModPanel::refreshTheme()
{
    for (int i = 0; i < 3; ++i)
    {
        auto& g = lfos[(size_t)i];
        const auto accent = QMColors::lfoColour(i);

        for (auto* b : { &g.enableButton, &g.stepMode, &g.syncToggle })
        {
            b->setColour(juce::TextButton::textColourOnId, accent);
            b->setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        }
        for (auto* s : { &g.rateFree, &g.minSlider, &g.maxSlider,
                         &g.phaseSlider, &g.fadeSlider, &g.spreadSlider })
            s->setColour(juce::Slider::thumbColourId, accent);
    }

    {
        const auto a4 = QMColors::lfoColour(3);
        for (auto* b : { &lfo4.enableButton, &lfo4.stepMode, &lfo4.syncToggle })
        {
            b->setColour(juce::TextButton::textColourOnId, a4);
            b->setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        }
        for (auto* s : { &lfo4.rateFree, &lfo4.depthSlider })
            s->setColour(juce::Slider::thumbColourId, a4);
        for (int i = 0; i < 3; ++i)
        {
            lfo4.assign[i].setColour(juce::TextButton::textColourOnId, QMColors::lfoColour(i));
            lfo4.assign[i].setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        }
    }

    {
        const auto a5 = QMColors::lfoColour(4);
        for (auto* b : { &lfo5.enableButton, &lfo5.stepMode, &lfo5.syncToggle })
        {
            b->setColour(juce::TextButton::textColourOnId, a5);
            b->setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        }
        for (auto* s : { &lfo5.rateFree, &lfo5.minSlider, &lfo5.maxSlider })
            s->setColour(juce::Slider::thumbColourId, a5);
    }

    {
        const auto ae = QMColors::envColour();
        for (auto* b : { &env.enableButton, &env.invertButton })
        {
            b->setColour(juce::TextButton::textColourOnId, ae);
            b->setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        }
        env.depthSlider.setColour(juce::Slider::thumbColourId, ae);
    }

    repaint();
}

// ==========================================================================
ModPanel::RowLayout ModPanel::layoutRow(juce::Rectangle<int> r) const
{
    RowLayout L;

    L.enable = r.removeFromLeft(kEnW);   r.removeFromLeft(kGap);
    L.wave = r.removeFromLeft(kWaveW);   r.removeFromLeft(kGap);
    L.step = r.removeFromLeft(kSmallW);  r.removeFromLeft(kGap);
    L.sync = r.removeFromLeft(kSmallW);  r.removeFromLeft(kGap);

    const int slotW = juce::jmax(60, (r.getWidth() - kGap * 5) / 6);
    for (int i = 0; i < 6; ++i)
    {
        L.slot[(size_t)i] = r.removeFromLeft(slotW);
        if (i < 5) r.removeFromLeft(kGap);
    }
    return L;
}

// ==========================================================================
void ModPanel::resized()
{
    auto area = getLocalBounds().reduced(QMUI::kMargin, 0);

    headerArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(2);
    columnArea = area.removeFromTop(12);
    area.removeFromTop(2);

    // Rate 欄をスライダー / コンボのどちらで見せるか
    auto placeRate = [](juce::Rectangle<int> slot, bool synced,
                        juce::ComboBox& combo, juce::Slider& slider)
    {
        combo.setVisible(synced);
        slider.setVisible(!synced);
        if (synced) combo.setBounds(slot.withSizeKeepingCentre(slot.getWidth(), QMUI::kCtrlH));
        else        slider.setBounds(slot.withSizeKeepingCentre(slot.getWidth(), QMUI::kBarH));
    };

    // ---------------- LFO 1 / 2 / 3 ----------------
    for (int i = 0; i < 3; ++i)
    {
        lfoRowAreas[(size_t)i] = area.removeFromTop(QMUI::kRowH);
        area.removeFromTop(QMUI::kRowGap);

        auto& g = lfos[(size_t)i];
        const auto L = layoutRow(lfoRowAreas[(size_t)i]);

        g.enableButton.setBounds(L.enable.withSizeKeepingCentre(kEnW, QMUI::kCtrlH));
        g.wave.setBounds(L.wave.withSizeKeepingCentre(kWaveW, QMUI::kCtrlH));
        g.stepMode.setBounds(L.step.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));
        g.syncToggle.setBounds(L.sync.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));

        placeRate(L.slot[0], isSynced("lfo" + juce::String(i + 1) + "sync"),
                  g.rateSync, g.rateFree);

        g.minSlider.setBounds(L.slot[1].withSizeKeepingCentre(L.slot[1].getWidth(), QMUI::kBarH));
        g.maxSlider.setBounds(L.slot[2].withSizeKeepingCentre(L.slot[2].getWidth(), QMUI::kBarH));
        g.phaseSlider.setBounds(L.slot[3].withSizeKeepingCentre(L.slot[3].getWidth(), QMUI::kBarH));
        g.fadeSlider.setBounds(L.slot[4].withSizeKeepingCentre(L.slot[4].getWidth(), QMUI::kBarH));
        g.spreadSlider.setBounds(L.slot[5].withSizeKeepingCentre(L.slot[5].getWidth(), QMUI::kBarH));
    }

    area.removeFromTop(8);
    extraHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(2);

    // ---------------- LFO 4 ----------------
    {
        lfo4RowArea = area.removeFromTop(QMUI::kRowH);
        area.removeFromTop(QMUI::kRowGap);

        const auto L = layoutRow(lfo4RowArea);
        lfo4.enableButton.setBounds(L.enable.withSizeKeepingCentre(kEnW, QMUI::kCtrlH));
        lfo4.wave.setBounds(L.wave.withSizeKeepingCentre(kWaveW, QMUI::kCtrlH));
        lfo4.stepMode.setBounds(L.step.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));
        lfo4.syncToggle.setBounds(L.sync.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));

        placeRate(L.slot[0], isSynced("lfo4sync"), lfo4.rateSync, lfo4.rateFree);
        lfo4.depthSlider.setBounds(L.slot[1].withSizeKeepingCentre(L.slot[1].getWidth(), QMUI::kBarH));

        // アサイン先ボタンはスロット 3〜5 に 1 つずつ
        for (int i = 0; i < 3; ++i)
        {
            const auto slot = L.slot[(size_t)(i + 3)];
            lfo4.assign[i].setBounds(slot.withSizeKeepingCentre(slot.getWidth(), QMUI::kCtrlH));
        }
    }

    // ---------------- LFO 5 ----------------
    {
        lfo5RowArea = area.removeFromTop(QMUI::kRowH);
        area.removeFromTop(QMUI::kRowGap);

        const auto L = layoutRow(lfo5RowArea);
        lfo5.enableButton.setBounds(L.enable.withSizeKeepingCentre(kEnW, QMUI::kCtrlH));
        lfo5.wave.setBounds(L.wave.withSizeKeepingCentre(kWaveW, QMUI::kCtrlH));
        lfo5.stepMode.setBounds(L.step.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));
        lfo5.syncToggle.setBounds(L.sync.withSizeKeepingCentre(kSmallW, QMUI::kCtrlH));

        placeRate(L.slot[0], isSynced("lfo5sync"), lfo5.rateSync, lfo5.rateFree);
        lfo5.minSlider.setBounds(L.slot[1].withSizeKeepingCentre(L.slot[1].getWidth(), QMUI::kBarH));
        lfo5.maxSlider.setBounds(L.slot[2].withSizeKeepingCentre(L.slot[2].getWidth(), QMUI::kBarH));
    }

    // ---------------- Envelope Follower ----------------
    {
        envRowArea = area.removeFromTop(QMUI::kRowH);

        const auto L = layoutRow(envRowArea);
        env.enableButton.setBounds(L.enable.withSizeKeepingCentre(kEnW, QMUI::kCtrlH));
        env.invertButton.setBounds(L.wave.withSizeKeepingCentre(kSmallW + 18, QMUI::kCtrlH)
                                       .withX(L.wave.getX()));

        // Depth は 2 スロット分の幅を使ってゆったり見せる
        auto depthArea = L.slot[0].getUnion(L.slot[1]);
        env.depthSlider.setBounds(depthArea.withSizeKeepingCentre(depthArea.getWidth(), QMUI::kBarH));
    }
}

// ==========================================================================
void ModPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "MODULATION  /  LFO 1-3", headerArea, QMColors::accentMod);

    // ---- 列見出し ----
    {
        const auto L = layoutRow(columnArea);
        QMUI::drawColumnLabel(g, "LFO", L.enable);
        QMUI::drawColumnLabel(g, "WAVEFORM", L.wave);
        QMUI::drawColumnLabel(g, "STEP", L.step);
        QMUI::drawColumnLabel(g, "SYNC", L.sync);
        QMUI::drawColumnLabel(g, "RATE", L.slot[0]);
        QMUI::drawColumnLabel(g, "MIN", L.slot[1]);
        QMUI::drawColumnLabel(g, "MAX", L.slot[2]);
        QMUI::drawColumnLabel(g, "PHASE", L.slot[3]);
        QMUI::drawColumnLabel(g, "FADE IN", L.slot[4]);
        QMUI::drawColumnLabel(g, "SPREAD", L.slot[5]);
    }

    // ---- 行の背景 + 左端アクセント ----
    auto paintRow = [&g](juce::Rectangle<int> row, juce::Colour accent, bool on)
    {
        if (row.isEmpty()) return;
        g.setColour(QMColors::text.withAlpha(on ? 0.035f : 0.015f));
        g.fillRoundedRectangle(row.toFloat().expanded(4.0f, 0.0f), 5.0f);
        g.setColour(accent.withAlpha(on ? 0.85f : 0.20f));
        g.fillRoundedRectangle((float)row.getX() - 6.0f, (float)row.getY() + 5.0f,
                               2.5f, (float)row.getHeight() - 10.0f, 1.25f);
    };

    auto isOn = [this](const char* id)
    {
        return processor.apvts.getRawParameterValue(id)->load() > 0.5f;
    };

    static const char* const enIds[3] = { "lfo1en", "lfo2en", "lfo3en" };
    for (int i = 0; i < 3; ++i)
        paintRow(lfoRowAreas[(size_t)i], QMColors::lfoColour(i), isOn(enIds[i]));

    QMUI::drawSectionHeader(g, "EXTRA MODULATION", extraHeaderArea, QMColors::accentMod);

    paintRow(lfo4RowArea, QMColors::lfoColour(3), isOn("lfo4en"));
    paintRow(lfo5RowArea, QMColors::lfoColour(4), isOn("lfo5en"));
    paintRow(envRowArea, QMColors::envColour(), isOn("envFollowen"));

    // ---- 補足説明（薄く）----
    g.setColour(QMColors::textDim.withAlpha(0.75f));
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::plain)));

    auto note = [&](juce::Rectangle<int> row, const juce::String& text)
    {
        if (row.isEmpty()) return;
        const auto L = layoutRow(row);
        auto r = juce::Rectangle<int>(L.slot[2].getX(), row.getY(),
                                      row.getRight() - L.slot[2].getX(), row.getHeight());
        g.drawText(text, r, juce::Justification::centredRight, false);
    };

    note(lfo5RowArea, "sweeps Dry/Wet between Min and Max");
    note(envRowArea, "input level drives Filter A cutoff");
}
