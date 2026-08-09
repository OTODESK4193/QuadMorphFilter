// ==========================================
// UI/ModPanel.cpp   （V1.1.0 新規）
// ==========================================
#include "ModPanel.h"
#include "../PluginProcessor.h"
#include <cmath>   // std::abs（レート変調インジケータの差分判定）

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

    // kWaveNames 内の "Recording" の項目 ID（addItemList は 1 始まり）。
    // LFO4 / LFO5 では選択不可にする。
    constexpr int kRecordingItemId = 7;
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

        // 【V1.1.0】LFO4 に Recording は不要。
        // 項目を削除するとコンボの添字が APVTS の選択肢とずれてしまうため、
        // 並びはそのままに選択だけを無効化する（保存済みの状態も壊れない）。
        lfo4.wave.setItemEnabled(kRecordingItemId, false);
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

        // LFO5 も Recording は不要（XY 軌跡を持たない 1 次元の LFO のため）
        lfo5.wave.setItemEnabled(kRecordingItemId, false);
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

    // パラメータ ID から説明を引く。ID 末尾で判別できるので
    // 呼び出し側に説明文を書かせずに済む。
    if (paramId.endsWith("rateFree"))
        QMUI::setInfo(s, "RATE (FREE)  -  LFO speed in Hz, independent of the host tempo. "
                         "When LFO4 modulates this LFO the bar shows the range it sweeps "
                         "and the bright tip follows the live rate.");
    else if (paramId.endsWith("min"))
        QMUI::setInfo(s, "MIN  -  lower end of the LFO output range. Setting Min above Max "
                         "runs the LFO backwards, which is a quick way to invert it.");
    else if (paramId.endsWith("max"))
        QMUI::setInfo(s, "MAX  -  upper end of the LFO output range.");
    else if (paramId.endsWith("phase"))
        QMUI::setInfo(s, "PHASE  -  start offset in degrees. Use it to run two LFOs of the "
                         "same shape out of step with each other.");
    else if (paramId.endsWith("fade"))
        QMUI::setInfo(s, "FADE IN  -  time for the LFO to reach full depth after it is "
                         "enabled. Leave at 0 ms for an immediate start.");
    else if (paramId.endsWith("spread"))
        QMUI::setInfo(s, "FILTER SPREAD  -  phase offset applied per filter, so A, B, C and D "
                         "rise in sequence rather than together. Works on the periodic "
                         "waveforms only.");
    else if (paramId.endsWith("depth"))
        QMUI::setInfo(s, "DEPTH  -  modulation amount in octaves. At 4x the target LFO can "
                         "run up to sixteen times faster or slower.");

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

    if (paramId.endsWith("step"))
        QMUI::setInfo(b, "STEP  -  quantises the LFO output into steps instead of a smooth "
                         "sweep, for sample-and-hold style movement.");
    else if (paramId.endsWith("sync"))
        QMUI::setInfo(b, "SYNC  -  locks the LFO to the host tempo and swaps the Rate control "
                         "for a note-division selector.");
    else if (paramId.startsWith("lfo4assign"))
        QMUI::setInfo(b, "ASSIGN  -  send LFO4's rate modulation to this LFO. Several targets "
                         "can be active at once.");
    else if (paramId == "lfo1en")
        QMUI::setInfo(b, "LFO1 MORPH  -  moves the XY morph position automatically. "
                         "The pad dot follows it and the filter mix changes with it.");
    else if (paramId == "lfo2en")
        QMUI::setInfo(b, "LFO2 CUTOFF  -  modulation source for filter cutoff. Assign it per "
                         "filter with the LFO2 buttons on the FILTER tab.");
    else if (paramId == "lfo3en")
        QMUI::setInfo(b, "LFO3 RESO  -  modulation source for filter resonance. Assign it per "
                         "filter with the LFO3 buttons on the FILTER tab.");
    else if (paramId == "lfo4en")
        QMUI::setInfo(b, "LFO4 RATE MOD  -  modulates the speed of LFO1, LFO2 and LFO3. "
                         "Pick the targets with the buttons on the right.");
    else if (paramId == "lfo5en")
        QMUI::setInfo(b, "LFO5 DRY/WET  -  sweeps the Dry/Wet balance between Min and Max. "
                         "The Dry/Wet knob on the FILTER tab shows the range as a ring.");
    else if (paramId == "envFollowen")
        QMUI::setInfo(b, "ENVELOPE FOLLOWER  -  the input level drives Filter A's cutoff, so "
                         "the filter opens as the signal gets louder.");
    else if (paramId == "envFollowinvert")
        QMUI::setInfo(b, "INVERT  -  flips the envelope so a louder input closes the filter "
                         "instead of opening it.");

    addAndMakeVisible(b);
    att = std::make_unique<ButtonAtt>(processor.apvts, paramId, b);
}

void ModPanel::styleWave(juce::ComboBox& c, const juce::String& paramId,
                         std::unique_ptr<ComboAtt>& att)
{
    c.addItemList(kWaveNames, 1);
    QMUI::setInfo(c,
        "WAVEFORM  -  the shape this LFO traces. The first few are the classic "
        "shapes; Random 1 gives each of the four filters its own independent value, "
        "Recording plays back a path you draw on the XY pad, and the rest are "
        "two-dimensional curves that move X and Y together.");
    addAndMakeVisible(c);
    att = std::make_unique<ComboAtt>(processor.apvts, paramId, c);
}

void ModPanel::styleRateSync(juce::ComboBox& c, const juce::String& paramId,
                             std::unique_ptr<ComboAtt>& att)
{
    c.addItemList(kSyncRates, 1);
    c.setJustificationType(juce::Justification::centred);
    QMUI::setInfo(c,
        "RATE (SYNC)  -  the note division this LFO runs at, locked to the host "
        "tempo. D is dotted and T is a triplet. When LFO4 modulates this LFO the "
        "reading shows the division actually being played, plus its rate in Hz.");
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

    updateRateModIndicators();
}

// ==========================================================================
// updateRateModIndicators
// LFO4 は LFO1/2/3 のレートを 2^(waveX * depth) 倍する。
// 変調が掛かっている LFO について、実際に使われているレートを表示に反映する。
//
//   Free モード : rateFree スライダーへ変調帯とライブ位置を渡す
//                 （描画は QuadMorphLookAndFeel::drawLinearSlider）
//   Sync モード : 音符の刻みは離散値なのでコンボの選択自体は動かせない。
//                 代わりに実効レート [Hz] をコンボの隣に動的表示する。
//                 （コンボの選択を書き換えるとパラメータが動いてしまい、
//                   オートメーションや保存内容を壊すため採らない）
// ==========================================================================
void ModPanel::updateRateModIndicators()
{
    auto& apvts = processor.apvts;

    for (int i = 0; i < 3; ++i)
    {
        auto& grp = lfos[(size_t)i];
        const juce::String id = "lfo" + juce::String(i + 1);

        // 変調表示を出す条件は 3 つとも満たすこと。
        //   ・その LFO 自体が有効
        //   ・LFO4 が有効
        //   ・LFO4 のアサイン先になっている
        // どれか欠けていれば変調は掛かっていないので帯も数値も出さない。
        const bool enabled = apvts.getRawParameterValue(id + "en")->load() > 0.5f;
        const bool lfo4On = apvts.getRawParameterValue("lfo4en")->load() > 0.5f;
        const bool modOn = enabled && lfo4On && processor.isRateModulated(i);

        const float rateHz = processor.getEffectiveLfoRate(i);
        const bool synced = isSynced(id + "sync");

        // ---- Sync モード: コンボの表示を動的に差し替え + 実効レート表示 ----
        const bool showText = modOn && synced;
        const int  effIdx = showText ? processor.getEffectiveSyncIndex(i) : -1;

        if (showText != rateModShown[i]
            || effIdx != effectiveSyncIdx[i]
            || (showText && std::abs(rateHz - effectiveRateHz[i]) > 0.005f))
        {
            rateModShown[i] = showText;
            effectiveSyncIdx[i] = effIdx;
            effectiveRateHz[i] = rateHz;
            repaint(grp.rateSync.getBounds().expanded(90, 2));
        }

        // ---- Free モード: スライダーの変調帯 ----
        auto& sl = grp.rateFree;
        auto& props = sl.getProperties();

        if (!modOn || synced)
        {
            if ((bool)props.getWithDefault("qmModOn", false))
            {
                props.set("qmModOn", false);
                sl.repaint();
            }
            continue;
        }

        auto* prm = apvts.getParameter(id + "rateFree");
        if (prm == nullptr) continue;

        const float shown = juce::jlimit(0.01f, 20.0f, rateHz);
        const double norm = (double)prm->convertTo0to1(shown);
        const double prev = props.getWithDefault("qmModNorm", -1.0);
        const bool wasOn = (bool)props.getWithDefault("qmModOn", false);

        props.set("qmModOn", true);

        if (!wasOn || std::abs(norm - prev) > 0.0015)
        {
            props.set("qmModNorm", norm);
            props.set("qmModValue", (double)shown);
            sl.repaint();
        }
    }
}

// ==========================================================================
// paintOverChildren
// LFO4 でレート変調が掛かっている LFO の Sync コンボに、
// 「実際に鳴っている刻み」を上書き表示する。
//
// コンボの選択そのものを書き換えないのは意図的で、
// パラメータを動かすとオートメーションと保存内容が壊れるため。
// ここでは子の描画が終わったあとに文字を重ねるだけなので、
// ドロップダウンを開けば本来の選択がそのまま出る。
// ==========================================================================
void ModPanel::paintOverChildren(juce::Graphics& g)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!rateModShown[i]) continue;

        const auto cb = lfos[(size_t)i].rateSync.getBounds();
        if (cb.isEmpty() || !lfos[(size_t)i].rateSync.isVisible()) continue;

        const auto accent = QMColors::lfoColour(3);   // LFO4 の色

        // ---- 刻み表示の差し替え ----
        const int effIdx = effectiveSyncIdx[i];
        if (effIdx >= 0 && effIdx < kSyncRates.size())
        {
            // 元の文字を隠してから描き直す（背景はコンボと同じ色）
            auto inner = cb.reduced(2, 2).withTrimmedRight(18);
            g.setColour(QMColors::track);
            g.fillRoundedRectangle(inner.toFloat(), 3.0f);

            g.setColour(accent);
            g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
            g.drawText(kSyncRates[effIdx], inner.reduced(6, 0),
                       juce::Justification::centredLeft, false);
        }

        // ---- 実効レート [Hz] ----
        g.setColour(accent.withAlpha(0.95f));
        g.setFont(QMFonts::mono(10.0f, true));
        g.drawText(juce::String(effectiveRateHz[i], 2) + " Hz",
                   cb.getRight() + 4, cb.getY(), 62, cb.getHeight(),
                   juce::Justification::centredLeft, false);
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
