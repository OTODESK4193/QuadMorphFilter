// ==========================================
// UI/FilterPanel.cpp   （V1.1.0 新規）
// ==========================================
#include "FilterPanel.h"
#include "../PluginProcessor.h"
#include "../DSP/ModelCapabilities.h"
#include "../DSP/MorphEngine.h"   // 変調インジケータで DSP と同じ式を使う
#include <cmath>

namespace
{
    // ---- 列幅（論理座標）----
    constexpr int kEnW = 30;
    constexpr int kSoloW = 24;   // 【V1.1.0 追加】Solo ボタン
    constexpr int kModelW = 152;
    constexpr int kTypeW = 74;
    constexpr int kSlopeW = 84;
    constexpr int kSrcW = 54;

    constexpr int kGapS = 6;
    constexpr int kGapM = 8;

    // ---- OUTPUT セクションのノブ ----
    constexpr int kKnobW = 118;   // 列幅
    constexpr int kKnobD = 66;    // ノブ直径
    constexpr int kLabelH = 14;   // 上の名前
    constexpr int kValueH = 16;   // 下の数値
    constexpr int kKnobGap = 22;  // 列間

    // 0=Off, 1=+X, 2=+Y, 3=-X, 4=-Y
    const char* const kSrcLabels[] = { "--", "+X", "+Y", "-X", "-Y" };

    const char* const kFilterNames[4] = { "A", "B", "C", "D" };

    // ------------------------------------------------------------------
    // モデル説明（MODEL コンボにマウスオーバーで MORPH 下に表示）
    //
    // ※ 表示文字列は英字のみ。juce::String(const char*) は ASCII 前提で、
    //    非 ASCII を渡すと assert する（144 行付近のコメントと同じ理由）。
    //
    // 並び順は g.model.addItemList() と完全に一致させること。
    // Res ノブの働きがモデルごとに違うため、通常のレゾナンス以外の
    // 役割を持つものは "Res =" として明記している。
    // ------------------------------------------------------------------
    const char* const kModelDesc[28] =
    {
        /*  0 */ "Clean SVF  -  Transparent state-variable filter. Neutral reference tone with no colouration.",
        /*  1 */ "Moog Ladder  -  4-pole transistor ladder solved with Newton-Raphson. Warm, thick, self-oscillates.",
        /*  2 */ "TB-303  -  Diode ladder with 8 Hz feedback high-pass. The classic acid squelch.",
        /*  3 */ "Oberheim SEM  -  2-pole multimode with soft band-pass saturation. Smooth and vocal.",
        /*  4 */ "Bitcrush  -  SVF followed by bit and sample-rate reduction.  Res = crush amount.",
        /*  5 */ "Vowel Filter  -  Three formant band-passes.  Cutoff sweeps through A-E-I-O-U vowels.",
        /*  6 */ "Comb Filter  -  Tuned delay line with feedback.  Cutoff = pitch, Res = feedback depth.",
        /*  7 */ "MS-20  -  Sallen-Key with asymmetric drive and a DC blocker. Screams when pushed.",
        /*  8 */ "Phaser  -  Cascaded all-pass stages producing moving notches.  Res = feedback.",
        /*  9 */ "Wavefolder  -  Pre-filter then antialiased sine folding.  Res = fold gain.",
        /* 10 */ "FDN Reverb  -  4-line feedback delay network with damping.  Res = decay time.",
        /* 11 */ "Phase Shift  -  Broadband all-pass. Alters phase while leaving the magnitude flat.",
        /* 12 */ "CEM3320  -  Curtis OTA ladder. Tighter and cleaner than the Moog, with a firm low end.",
        /* 13 */ "SSM2040  -  SSM ladder with asymmetric clipping. Aggressive, slightly gritty resonance.",
        /* 14 */ "CS-80  -  Yamaha-style multimode with gentle input drive. Silky and wide.",
        /* 15 */ "Roland Jupiter  -  Bright 4-pole IR3109-style ladder. Glassy top end.",
        /* 16 */ "EDP Wasp  -  CMOS-based filter. Raw, buzzy and deliberately dirty.",
        /* 17 */ "Butterworth  -  Maximally flat pass-band. The most neutral of the digital designs.",
        /* 18 */ "Chebyshev  -  Steeper roll-off in exchange for pass-band ripple.",
        /* 19 */ "Bessel  -  Linear phase, so transients keep their shape. Gentlest slope.",
        /* 20 */ "Elliptic  -  Steepest roll-off with a transmission zero.  Res shifts the notch.",
        /* 21 */ "Vactrol LPG  -  Low-pass gate. Level and brightness move together, Buchla style.",
        /* 22 */ "Modal Res  -  Eight tuned resonant bands. Struck-object and bell-like tones.",
        /* 23 */ "Waveguide  -  Delay loop with scattering.  Cutoff = pitch, Res = decay.",
        /* 24 */ "Bode Shifter  -  Hilbert frequency shifter.  Res = feedback, for metallic Shepard tones.",
        /* 25 */ "2D Morph  -  Seven biquads morphed by pole placement. Evolving spectral shapes.",
        /* 26 */ "Phased Array  -  Multi-stage all-pass stereo decorrelation. Widens without chorusing.",
        /* 27 */ "Nyquist AA  -  Transparent high-frequency limiter. Tames aliasing and harshness."
    };

    // Res ノブの役割（モデルごとに意味が変わるため個別に用意）
    const char* const kResDesc[28] =
    {
        /*  0 */ "resonance peak at the cutoff.",
        /*  1 */ "ladder feedback. Self-oscillates near the top.",
        /*  2 */ "ladder feedback. The classic acid squelch lives high up here.",
        /*  3 */ "resonance, with the band-pass path saturating as it rises.",
        /*  4 */ "bit depth and sample-rate crushing amount.",
        /*  5 */ "formant sharpness, how vocal the vowel sounds.",
        /*  6 */ "comb feedback, which sets how long the tuned delay rings.",
        /*  7 */ "resonance. High values push the filter into its screaming range.",
        /*  8 */ "all-pass feedback, deepening the notches.",
        /*  9 */ "fold gain. More folds means more added harmonics.",
        /* 10 */ "reverb decay time.",
        /* 11 */ "how far the phase spreads across the spectrum.",
        /* 12 */ "resonance. Tighter and cleaner than the Moog ladder.",
        /* 13 */ "resonance, with asymmetric clipping adding grit as it rises.",
        /* 14 */ "resonance with gentle input drive.",
        /* 15 */ "resonance. Bright and glassy at the top.",
        /* 16 */ "resonance. Gets raw and buzzy quickly.",
        /* 17 */ "pass-band peaking.",
        /* 18 */ "pass-band ripple depth. More ripple means a steeper roll-off.",
        /* 19 */ "phase character. Bessel stays close to linear phase.",
        /* 20 */ "stop-band notch position.",
        /* 21 */ "release time of the low-pass gate.",
        /* 22 */ "how sharply the eight tuned bands ring.",
        /* 23 */ "decay time of the waveguide loop.",
        /* 24 */ "feedback. Raising it stacks shifts into metallic Shepard tones.",
        /* 25 */ "the Y coordinate of the pole-placement morph.",
        /* 26 */ "depth of the all-pass stereo decorrelation.",
        /* 27 */ "how hard the high-frequency limiter clamps."
    };
}

// ==========================================================================
FilterPanel::FilterPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    for (int i = 0; i < 4; ++i)
        setupGroup(groups[(size_t)i], i, kFilterNames[i]);

    // ---- OUTPUT（旧 OUT タブから移設）----
    // 表示文字列は英字のみ（juce::String(const char*) は ASCII 前提）。
    styleKnob(masterGainSlider, "OUTPUT GAIN", QMUI::Unit::Db, QMColors::accentOut,
              "masterGain", mgAtt,
              "OUTPUT GAIN  -  final level after the filters and the dry/wet mix, "
              "applied before the limiter. Use it to match bypassed and processed levels.");

    styleKnob(dryWetSlider, "DRY / WET", QMUI::Unit::Pct, QMColors::accentMorph,
              "dryWet", dwAtt,
              "DRY / WET  -  balance between the untouched input and the filtered signal, "
              "using an equal-power curve so the level stays steady. When LFO5 is enabled "
              "the ring shows its range and the dot follows the live value.");

    styleKnob(ceilingSlider, "CEILING", QMUI::Unit::Db, QMColors::rose,
              "limiterCeiling", clAtt,
              "LIMITER CEILING  -  the output never exceeds this level. Resonant filters "
              "can add a lot of gain, so this is the safety net that keeps peaks in check.");

    updateLfoSrcButtons();
    updateSoloButtons();

    // 30Hz。LFO 変調インジケータを滑らかに動かすため 20 → 30 に引き上げた。
    startTimerHz(30);
}

FilterPanel::~FilterPanel()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
}

// ==========================================================================
void FilterPanel::styleKnob(juce::Slider& s, const juce::String& name, QMUI::Unit unit,
                            juce::Colour accent, const juce::String& paramId,
                            std::unique_ptr<SliderAtt>& att, const juce::String& info)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                          juce::MathConstants<float>::pi * 2.8f, true);
    s.setName(name);
    s.getProperties().set("qmUnit", (int)unit);
    s.setColour(juce::Slider::thumbColourId, accent);
    QMUI::setInfo(s, info);
    addAndMakeVisible(s);
    att = std::make_unique<SliderAtt>(processor.apvts, paramId, s);
}

// ==========================================================================
// updateLfo5Indicator
// LFO5 は Dry/Wet を Min〜Max の範囲で振る。dryWet パラメータのレンジも
// 0〜100 なので、正規化は 100 で割るだけでよい。
// ==========================================================================
void FilterPanel::updateLfo5Indicator()
{
    auto& apvts = processor.apvts;
    auto raw = [&apvts](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    auto& props = dryWetSlider.getProperties();
    const bool on = raw("lfo5en") > 0.5f;

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

    if (!wasOn || std::abs((double)live - prev) > 0.0015)
    {
        props.set("qmModNorm", (double)live);
        props.set("qmModValue", (double)(live * 100.0f));
        dryWetSlider.repaint();
        repaint(dryWetSlot.value);   // 下の数値表示も追随させる
    }
}

// ==========================================================================
// updateInfoText
// マウス直下のコントロールから "qmInfo" を拾って Info 欄を更新する。
// 各コントロール側の配線は不要で、setInfo() してあるものが自動的に対象になる。
// ==========================================================================
void FilterPanel::updateInfoText()
{
    const auto info = QMUI::findInfoUnderMouse();

    if (info != currentInfo)
    {
        currentInfo = info;
        repaint(infoArea);
    }
}

// ==========================================================================
void FilterPanel::setupGroup(Group& g, int index, const juce::String& s)
{
    const auto accent = QMColors::filterColour(index);

    // ---- Enable ----
    g.enableButton.setButtonText(s);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, accent);
    g.enableButton.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
    g.enableButton.setTooltip("Filter " + s + " on / off");
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "enable" + s, g.enableButton);

    // ---- Solo（排他）----
    // APVTS ではなく processor.soloFilter（atomic）を直接操作する。
    // 一時的なモニタリング機能なので保存もオートメーションもしない。
    g.soloButton.setButtonText("S");
    g.soloButton.setClickingTogglesState(false);   // 状態は soloFilter 側で管理
    // 点灯色はそのフィルター固有の色。カーブの塗り色と一致するので
    // 「どのフィルターを Solo しているか」が一目で分かる。
    g.soloButton.setColour(juce::TextButton::textColourOnId, accent);
    g.soloButton.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
    g.soloButton.setTooltip("Solo filter " + s + "  -  hear and display this filter only. "
                            "Click again to release.");
    addAndMakeVisible(g.soloButton);

    g.soloButton.onClick = [this, index]()
    {
        // 同じボタンをもう一度押したら解除、違うボタンなら乗り換え
        const int current = processor.soloFilter.load();
        processor.soloFilter.store(current == index ? -1 : index);
        updateSoloButtons();
        repaint();
    };

    // ---- Model ----
    g.model.addItemList({
        "Clean SVF",    "Moog Ladder",  "TB-303",       "Oberheim SEM", "Bitcrush",
        "Vowel Filter", "Comb Filter",  "MS-20",        "Phaser",       "Wavefolder",
        "FDN Reverb",   "Phase Shift",
        "CEM3320",      "SSM2040",      "CS-80",        "Roland Jupiter", "EDP Wasp",
        "Butterworth",  "Chebyshev",    "Bessel",       "Elliptic",
        "Vactrol LPG",  "Modal Res",    "Waveguide",    "Bode Shifter",
        "2D Morph",     "Phased Array", "Nyquist AA"
        }, 1);
    addAndMakeVisible(g.model);
    g.mAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "model" + s, g.model);

    // ---- Type / Slope ----
    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "type" + s, g.type);

    g.slope.addItemList({ "12dB", "24dB", "48dB", "96dB" }, 1);
    addAndMakeVisible(g.slope);
    g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "slope" + s, g.slope);

    // ---- Cutoff / Res（バー型スライダー：名前と値はバー内に描かれる）----
    auto setupBar = [&](juce::Slider& sl, const juce::String& name, QMUI::Unit unit,
                        juce::Colour col, const juce::String& paramId,
                        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
    {
        sl.setSliderStyle(juce::Slider::LinearHorizontal);
        sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sl.setName(name);
        sl.getProperties().set("qmUnit", (int)unit);
        sl.setColour(juce::Slider::thumbColourId, col);
        addAndMakeVisible(sl);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, paramId, sl);
    };
    setupBar(g.cutoff, "Cut", QMUI::Unit::Hz, accent, "cutoff" + s, g.cAtt);
    setupBar(g.res, "Res", QMUI::Unit::Raw, QMColors::filterResColour(index), "res" + s, g.rAtt);

    // ---- LFO 割り当て（クリックのたびに Off→+X→+Y→-X→-Y をサイクル）----
    auto setupSrc = [&](juce::TextButton& b, const juce::String& paramId, juce::Colour col,
                        const juce::String& tip)
    {
        b.setButtonText("--");
        b.setClickingTogglesState(false);   // トグル状態は手動管理
        b.setColour(juce::TextButton::textColourOnId, col);
        b.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        b.setTooltip(tip);
        addAndMakeVisible(b);

        b.onClick = [this, paramId]()
        {
            if (auto* prm = dynamic_cast<juce::AudioParameterChoice*>(
                    processor.apvts.getParameter(paramId)))
            {
                const int next = (prm->getIndex() + 1) % 5;
                prm->setValueNotifyingHost(prm->convertTo0to1((float)next));
            }
            updateLfoSrcButtons();   // タイマーを待たずに即時反映
        };
    };
    // ※ juce::String(const char*) は ASCII 前提（非 ASCII は assert する）ため、
    //    ツールチップ等の表示文字列は英字で書く。日本語はコメント側に残す。
    setupSrc(g.lfoCutBtn, "lfoCutSrc" + s, QMColors::lfoColour(1),
             "LFO 2 axis that modulates Filter " + s + " cutoff  (Off / +X / +Y / -X / -Y)");
    setupSrc(g.lfoResBtn, "lfoResSrc" + s, QMColors::lfoColour(2),
             "LFO 3 axis that modulates Filter " + s + " resonance  (Off / +X / +Y / -X / -Y)");

    // ---- Info（マウスオーバーで右下の Info 欄に出る説明）----
    QMUI::setInfo(g.enableButton,
        "FILTER " + s + " ON / OFF  -  disabled filters are skipped entirely, "
        "so turning one off also saves CPU.");
    QMUI::setInfo(g.soloButton,
        "SOLO FILTER " + s + "  -  hear and display this filter on its own. "
        "Solo overrides the on/off switch the way a mixer does, and only one filter "
        "can be soloed at a time. It is not saved with the project.");
    QMUI::setInfo(g.type,
        "RESPONSE TYPE  -  low-pass, band-pass, high-pass or notch. "
        "Models that only support some of these grey out the rest.");
    QMUI::setInfo(g.slope,
        "SLOPE  -  how many filter stages are cascaded. Steeper slopes cut harder "
        "past the cutoff and cost more CPU. Some models repurpose this control.");
    QMUI::setInfo(g.cutoff,
        "CUTOFF  -  the corner frequency, 20 Hz to 20 kHz. On delay-based models "
        "such as Comb and Waveguide this sets the pitch instead.");
    QMUI::setInfo(g.lfoCutBtn,
        "LFO2 to CUTOFF  -  pick which axis of LFO2 modulates this filter's cutoff. "
        "Click to cycle Off, +X, +Y, -X, -Y. The slider then shows the modulation range.");
    QMUI::setInfo(g.lfoResBtn,
        "LFO3 to RESONANCE  -  pick which axis of LFO3 modulates this filter's "
        "resonance. Click to cycle Off, +X, +Y, -X, -Y.");

    // ---- モデル変更時にコントロール構成を更新 ----
    g.model.onChange = [this, &g, s]()
    {
        refreshGroupControls(g, s, g.model.getSelectedId() - 1);
    };

    const int initialModel = (int)processor.apvts.getRawParameterValue("model" + s)->load();
    refreshGroupControls(g, s, initialModel);
}

// ==========================================================================
// モデルごとのコントロール構成更新
//   （V1.0.0 の PluginEditor::refreshFilterGroupControls をそのまま移設。
//     ラベル部品を廃止したので、名称はスライダーの setName() で反映する）
// ==========================================================================
void FilterPanel::refreshGroupControls(Group& g, const juce::String& suffix, int modelIdx)
{
    auto [maxSlope, hasLP, hasBP, hasHP, hasNotch] = getModelCaps(modelIdx);

    // ---- Slope コンボ：モデル別に意味が変わる ----
    auto rebuildSlopeDirect = [&](const juce::StringArray& items, int maxIndex)
    {
        // ComboBoxAttachment はコンボ再構築後に内部マッピングがずれるため、
        // アタッチメントを破棄して onChange で直接 APVTS へ書き込む。
        g.slAtt.reset();
        g.slope.clear(juce::dontSendNotification);
        for (int i = 0; i < items.size(); ++i)
            g.slope.addItem(items[i], i + 1);

        const int cur = juce::roundToInt(
            processor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, maxIndex + 1, cur + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex();
            if (idx < 0) return;
            if (auto* prm = processor.apvts.getParameter("slope" + sfx))
                prm->setValueNotifyingHost(prm->convertTo0to1((float)idx));
        };
    };

    if (modelIdx == 2)                       // TB-303: Accent
        rebuildSlopeDirect({ "Off", "Low", "High" }, 2);
    else if (modelIdx == 4)                  // Bitcrush: Stages
        rebuildSlopeDirect({ "1x", "2x", "4x", "8x" }, 3);
    else if (modelIdx == 6)                  // Comb: Cascade
        rebuildSlopeDirect({ "1x", "2x", "4x", "8x" }, 3);
    else if (modelIdx == 9)                  // Wavefolder: Folds
        rebuildSlopeDirect({ "1x", "2x", "4x", "8x" }, 3);
    else if (modelIdx == 10)                 // FDN Reverb: Space
        rebuildSlopeDirect({ "Room", "Hall", "Cave", "Plate" }, 3);
    else if (modelIdx == 21)                 // Vactrol LPG: Attack
        rebuildSlopeDirect({ "Fast", "Mid", "Slow" }, 2);
    else if (modelIdx == 23)                 // Waveguide: Reflections
        rebuildSlopeDirect({ "1x", "2x", "4x", "8x" }, 3);
    else
    {
        // ---- 通常の dB/oct スロープ ----
        g.slope.onChange = nullptr;

        const int curSlopeRaw = juce::roundToInt(
            processor.apvts.getRawParameterValue("slope" + suffix)->load());
        const int clampedSlope = juce::jlimit(0, maxSlope, curSlopeRaw);

        g.slAtt.reset();                     // 先にアタッチメントを破棄
        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("12dB", 1);
        g.slope.addItem("24dB", 2);
        g.slope.addItem("48dB", 3);
        g.slope.addItem("96dB", 4);

        for (int i = 1; i <= 4; ++i)
            g.slope.setItemEnabled(i, (i - 1) <= maxSlope);

        g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.apvts, "slope" + suffix, g.slope);

        g.slope.setSelectedId(clampedSlope + 1, juce::dontSendNotification);

        if (curSlopeRaw != clampedSlope)
            if (auto* prm = processor.apvts.getParameter("slope" + suffix))
                prm->setValueNotifyingHost(prm->convertTo0to1((float)clampedSlope));
    }

    // ---- Type コンボ ----
    g.type.setItemEnabled(1, hasLP);
    g.type.setItemEnabled(2, hasBP);
    g.type.setItemEnabled(3, hasHP);
    g.type.setItemEnabled(4, hasNotch);

    auto setTypeTexts = [&](const char* a, const char* b, const char* c, const char* d)
    {
        g.type.changeItemText(1, a);
        g.type.changeItemText(2, b);
        g.type.changeItemText(3, c);
        g.type.changeItemText(4, d);
    };

    switch (modelIdx)
    {
        case 23: setTypeTexts("Wet", "Mix", "HP", "Notch");            break; // Waveguide
        case 10: setTypeTexts("Dark", "Mid", "Air", "Open");           break; // FDN Reverb
        case 8:  setTypeTexts("+FB", "-FB", "HP", "Notch");            break; // Phaser
        case 11: setTypeTexts("Lin", "Log", "Mirror", "Rand");         break; // Phase Shift
        case 24: setTypeTexts("Up", "Down", "HP", "Notch");            break; // Bode Shifter
        case 26: setTypeTexts("Blend", "BP", "Wet", "Notch");          break; // Phased Array
        case 22: setTypeTexts("Type0", "Type1", "Type2", "Type3");     break; // Modal Res
        case 4:  setTypeTexts("Warm", "Focus", "Bright", "Hollow");    break; // Bitcrush
        case 6:  setTypeTexts("Warm", "Metal", "Comb", "Phase");       break; // Comb
        default: setTypeTexts("LP", "BP", "HP", "Notch");              break;
    }

    const int curType = g.type.getSelectedId();
    const bool typeOk = (curType == 1 && hasLP)
        || (curType == 2 && hasBP)
        || (curType == 3 && hasHP)
        || (curType == 4 && hasNotch);

    if (!typeOk)
    {
        const int fallback = hasLP ? 1 : hasBP ? 2 : hasHP ? 3 : 4;
        g.type.setSelectedId(fallback, juce::sendNotification);
        if (auto* prm = processor.apvts.getParameter("type" + suffix))
            prm->setValueNotifyingHost(prm->convertTo0to1((float)(fallback - 1)));
    }

    // ---- Cutoff / Res のラベルと単位 ----
    //   モデルによって Cutoff / Res の意味が変わるので、名称と表示単位を差し替える。
    const char* cutName = "Cut";
    const char* resName = "Res";

    switch (modelIdx)
    {
        case 10: cutName = "Cut";    resName = "Decay";  break; // FDN Reverb
        case 8:  cutName = "Freq";   resName = "Depth";  break; // Phaser
        case 11: cutName = "Center"; resName = "Spread"; break; // Phase Shift
        case 24: cutName = "Shift";  resName = "Fdbk";   break; // Bode Shifter
        case 25: cutName = "X";      resName = "Y";      break; // 2D Morph
        case 26: cutName = "Freq";   resName = "Depth";  break; // Phased Array
        case 4:  cutName = "Rate";   resName = "Color";  break; // Bitcrush
        case 6:  cutName = "Freq";   resName = "FB";     break; // Comb
        case 7:  cutName = "Cut";    resName = "Peak";   break; // MS-20
        case 9:  cutName = "Cut";    resName = "Drive";  break; // Wavefolder
        case 17: cutName = "Cut";    resName = "Peak";   break; // Butterworth
        case 18: cutName = "Cut";    resName = "Ripple"; break; // Chebyshev
        case 19: cutName = "Cut";    resName = "Phase";  break; // Bessel
        case 20: cutName = "Cut";    resName = "Stop";   break; // Elliptic
        case 21: cutName = "Cut";    resName = "Rel";    break; // Vactrol LPG
        case 23: cutName = "Tune";   resName = "Decay";  break; // Waveguide
        default: break;
    }

    g.cutoff.setName(cutName);
    g.res.setName(resName);

    // ---- Info: 選択中モデルの説明をコンボへ持たせる ----
    // Info 欄はマウス直下のコントロールから "qmInfo" を拾うだけなので、
    // ここで内容を差し替えておけば自動的に表示が切り替わる。
    QMUI::setInfo(g.model, kModelDesc[juce::jlimit(0, 27, modelIdx)]);

    // Res はモデルごとに役割が変わるので、名前に合わせて説明も差し替える
    QMUI::setInfo(g.res,
        juce::String(resName) + "  -  " + kResDesc[juce::jlimit(0, 27, modelIdx)]);

    g.cutoff.repaint();
    g.res.repaint();
}

// ==========================================================================
void FilterPanel::updateLfoSrcButtons()
{
    for (int i = 0; i < 4; ++i)
    {
        const juce::String s = kFilterNames[i];
        auto& g = groups[(size_t)i];

        // AudioParameterChoice は getRawParameterValue がインデックス値を返す
        const int cutState = juce::jlimit(0, 4, juce::roundToInt(
            processor.apvts.getRawParameterValue("lfoCutSrc" + s)->load()));
        const int resState = juce::jlimit(0, 4, juce::roundToInt(
            processor.apvts.getRawParameterValue("lfoResSrc" + s)->load()));

        if (g.lfoCutBtn.getButtonText() != kSrcLabels[cutState])
            g.lfoCutBtn.setButtonText(kSrcLabels[cutState]);
        g.lfoCutBtn.setToggleState(cutState > 0, juce::dontSendNotification);

        if (g.lfoResBtn.getButtonText() != kSrcLabels[resState])
            g.lfoResBtn.setButtonText(kSrcLabels[resState]);
        g.lfoResBtn.setToggleState(resState > 0, juce::dontSendNotification);
    }
}

// ==========================================================================
// updateModulationIndicators
// LFO2 / LFO3 による変調後の実効値を、DSP と同じ式で求めてスライダーに渡す。
// （MorphEngine の関数をそのまま使うので、表示と実音がずれない）
// ==========================================================================
void FilterPanel::updateModulationIndicators()
{
    auto& apvts = processor.apvts;
    auto raw = [&apvts](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    // LFO2 = Cutoff 変調 / LFO3 = Reso 変調
    const bool cutIsRand1 = ((int)raw("lfo2wave") == 3) && (raw("lfo2en") > 0.5f);
    const bool resIsRand1 = ((int)raw("lfo3wave") == 3) && (raw("lfo3en") > 0.5f);

    const auto cM = MorphEngine::computeModulation(
        processor.getLfoPos(1), processor.getLfoMod4(1), cutIsRand1);
    const auto rM = MorphEngine::computeModulation(
        processor.getLfoPos(2), processor.getLfoMod4(2), resIsRand1);

    for (int i = 0; i < 4; ++i)
    {
        const juce::String s = kFilterNames[i];
        auto& grp = groups[(size_t)i];

        const int cutSrc = juce::jlimit(0, 4, juce::roundToInt(raw("lfoCutSrc" + s)));
        const int resSrc = juce::jlimit(0, 4, juce::roundToInt(raw("lfoResSrc" + s)));

        // ---- Cutoff ----
        auto applyTo = [&](juce::Slider& sl, const juce::String& paramId,
                           int src, float modValue, float octaves,
                           float lo, float hi)
        {
            if (src <= 0)
            {
                // 変調なし → インジケータを消す
                if ((bool)sl.getProperties().getWithDefault("qmModOn", false))
                {
                    sl.getProperties().set("qmModOn", false);
                    sl.repaint();
                }
                return;
            }

            auto* prm = apvts.getParameter(paramId);
            if (prm == nullptr) return;

            const float base = raw(paramId);
            const float modulated = juce::jlimit(lo, hi,
                base * std::pow(2.0f, octaves * modValue));

            const double norm = (double)prm->convertTo0to1(modulated);
            const double prev = sl.getProperties().getWithDefault("qmModNorm", -1.0);
            const bool   wasOn = (bool)sl.getProperties().getWithDefault("qmModOn", false);

            sl.getProperties().set("qmModOn", true);

            // 位置が実質変わっていないときは repaint を省く（描画負荷の抑制）。
            // ただし OFF → ON に切り替わった瞬間は必ず描き直す。
            if (!wasOn || std::abs(norm - prev) > 0.0015)
            {
                sl.getProperties().set("qmModNorm", norm);
                sl.getProperties().set("qmModValue", (double)modulated);
                sl.repaint();
            }
        };

        applyTo(grp.cutoff, "cutoff" + s, cutSrc,
                cM[(size_t)juce::jmax(0, cutSrc - 1)], 4.0f, 20.0f, 20000.0f);
        applyTo(grp.res, "res" + s, resSrc,
                rM[(size_t)juce::jmax(0, resSrc - 1)], 2.0f, 0.1f, 10.0f);
    }
}

// ==========================================================================
void FilterPanel::updateSoloButtons()
{
    const int solo = processor.soloFilter.load();

    for (int i = 0; i < 4; ++i)
        groups[(size_t)i].soloButton.setToggleState(solo == i, juce::dontSendNotification);
}

// ==========================================================================
void FilterPanel::timerCallback()
{
    if (!isVisible())
        return;

    updateLfoSrcButtons();
    updateSoloButtons();
    updateModulationIndicators();
    updateLfo5Indicator();
    updateInfoText();
}

// ==========================================================================
void FilterPanel::refreshTheme()
{
    masterGainSlider.setColour(juce::Slider::thumbColourId, QMColors::accentOut);
    dryWetSlider.setColour(juce::Slider::thumbColourId, QMColors::accentMorph);
    ceilingSlider.setColour(juce::Slider::thumbColourId, QMColors::rose);

    for (int i = 0; i < 4; ++i)
    {
        auto& g = groups[(size_t)i];
        const auto accent = QMColors::filterColour(i);

        g.enableButton.setColour(juce::TextButton::textColourOnId, accent);
        g.enableButton.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        g.cutoff.setColour(juce::Slider::thumbColourId, accent);
        g.res.setColour(juce::Slider::thumbColourId, QMColors::filterResColour(i));

        g.lfoCutBtn.setColour(juce::TextButton::textColourOnId, QMColors::lfoColour(1));
        g.lfoCutBtn.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
        g.lfoResBtn.setColour(juce::TextButton::textColourOnId, QMColors::lfoColour(2));
        g.lfoResBtn.setColour(juce::TextButton::textColourOffId, QMColors::textDim);
    }
    repaint();
}

// ==========================================================================
FilterPanel::RowLayout FilterPanel::layoutRow(juce::Rectangle<int> r) const
{
    RowLayout L;

    // 左から: ON / S / MODEL / TYPE / SLOPE
    L.enable = r.removeFromLeft(kEnW);    r.removeFromLeft(kGapS);
    L.solo = r.removeFromLeft(kSoloW);    r.removeFromLeft(kGapS);
    L.model = r.removeFromLeft(kModelW);  r.removeFromLeft(kGapS);
    L.type = r.removeFromLeft(kTypeW);    r.removeFromLeft(kGapS);
    L.slope = r.removeFromLeft(kSlopeW);  r.removeFromLeft(kGapM);

    // LFO 列は右端から確保する。
    // 【V1.1.0】バーを 3/4 に縮めた分の余白が中央に生まれるが、
    // 右詰めにしておけば LFO2 / LFO3 の列位置がウィンドウ幅によらず安定する。
    L.lfoRes = r.removeFromRight(kSrcW);  r.removeFromRight(kGapS);
    L.lfoCut = r.removeFromRight(kSrcW);  r.removeFromRight(kGapM);

    // 残り領域を Cutoff / Res の 2 本で均等に分ける。
    // 【V1.1.0】以前は 3/4 に縮めていたが、"Cut 20.0k" のように
    // 名前と値の両方をバー内に描くため幅が足りず、文字が重なっていた。
    // MORPH セクションを CONFIG タブへ移して行に余裕ができたので、
    // 余りを丸ごと 2 等分して十分な余白を確保する。
    const int barGap = kGapM;
    const int barW = juce::jmax(120, (r.getWidth() - barGap) / 2);

    L.cutoff = r.removeFromLeft(barW);    r.removeFromLeft(barGap);
    L.res = r.removeFromLeft(barW);

    return L;
}

// ==========================================================================
void FilterPanel::resized()
{
    auto area = getLocalBounds().reduced(QMUI::kMargin, 0);

    headerArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(2);
    columnArea = area.removeFromTop(12);
    area.removeFromTop(2);

    for (int i = 0; i < 4; ++i)
    {
        rowAreas[(size_t)i] = area.removeFromTop(QMUI::kRowH);
        area.removeFromTop(QMUI::kRowGap);

        auto& g = groups[(size_t)i];
        const auto L = layoutRow(rowAreas[(size_t)i]);

        g.enableButton.setBounds(L.enable.withSizeKeepingCentre(kEnW, QMUI::kCtrlH));
        g.soloButton.setBounds(L.solo.withSizeKeepingCentre(kSoloW, QMUI::kCtrlH));
        g.model.setBounds(L.model.withSizeKeepingCentre(kModelW, QMUI::kCtrlH));
        g.type.setBounds(L.type.withSizeKeepingCentre(kTypeW, QMUI::kCtrlH));
        g.slope.setBounds(L.slope.withSizeKeepingCentre(kSlopeW, QMUI::kCtrlH));
        g.cutoff.setBounds(L.cutoff.withSizeKeepingCentre(L.cutoff.getWidth(), QMUI::kBarH));
        g.res.setBounds(L.res.withSizeKeepingCentre(L.res.getWidth(), QMUI::kBarH));
        g.lfoCutBtn.setBounds(L.lfoCut.withSizeKeepingCentre(kSrcW, QMUI::kCtrlH));
        g.lfoResBtn.setBounds(L.lfoRes.withSizeKeepingCentre(kSrcW, QMUI::kCtrlH));
    }

    // ======================================================================
    // OUTPUT セクション（旧 OUT タブから移設）
    //   左にノブ 3 つ、右の余白に Info 欄を置く。
    // ======================================================================
    area.removeFromTop(12);
    outHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(8);

    auto outRow = area.removeFromTop(juce::jmax(0,
        juce::jmin(kLabelH + kKnobD + kValueH + 6, area.getHeight())));

    auto knobArea = outRow.removeFromLeft(kKnobW * 3 + kKnobGap * 2);

    auto place = [&](juce::Slider& s, KnobSlot& slot)
    {
        auto col = knobArea.removeFromLeft(kKnobW);
        slot.label = col.removeFromTop(kLabelH);
        slot.knob = col.removeFromTop(kKnobD);
        col.removeFromTop(4);
        slot.value = col.removeFromTop(juce::jmax(0, juce::jmin(kValueH, col.getHeight())));

        s.setBounds(slot.knob.withSizeKeepingCentre(kKnobD, kKnobD));
        knobArea.removeFromLeft(kKnobGap);
    };

    place(masterGainSlider, gainSlot);
    place(dryWetSlider, dryWetSlot);
    place(ceilingSlider, ceilingSlot);

    // ---- Info 欄（ノブの右の余白すべて）----
    outRow.removeFromLeft(18);
    infoArea = outRow;
}

// ==========================================================================
void FilterPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "FILTER MATRIX", headerArea, QMColors::accentFilter);

    // ---- 列見出し ----
    {
        const auto L = layoutRow(columnArea);
        QMUI::drawColumnLabel(g, "ON", L.enable);
        QMUI::drawColumnLabel(g, "S", L.solo);
        QMUI::drawColumnLabel(g, "MODEL", L.model, juce::Justification::centredLeft);
        QMUI::drawColumnLabel(g, "TYPE", L.type);
        QMUI::drawColumnLabel(g, "SLOPE", L.slope);
        QMUI::drawColumnLabel(g, "CUTOFF", L.cutoff, juce::Justification::centredLeft);
        QMUI::drawColumnLabel(g, "RESONANCE", L.res, juce::Justification::centredLeft);
        QMUI::drawColumnLabel(g, "LFO2", L.lfoCut);
        QMUI::drawColumnLabel(g, "LFO3", L.lfoRes);
    }

    // ---- 各行のカード背景 + 左端のアクセントバー ----
    for (int i = 0; i < 4; ++i)
    {
        const auto row = rowAreas[(size_t)i];
        if (row.isEmpty()) continue;

        const bool on = processor.apvts.getRawParameterValue(
            juce::String("enable") + kFilterNames[i])->load() > 0.5f;

        g.setColour(QMColors::text.withAlpha(on ? 0.035f : 0.015f));
        g.fillRoundedRectangle(row.toFloat().expanded(4.0f, 0.0f), 5.0f);

        g.setColour(QMColors::filterColour(i).withAlpha(on ? 0.85f : 0.20f));
        g.fillRoundedRectangle((float)row.getX() - 6.0f, (float)row.getY() + 5.0f,
                               2.5f, (float)row.getHeight() - 10.0f, 1.25f);
    }

    // ---- OUTPUT ----
    QMUI::drawSectionHeader(g, "OUTPUT", outHeaderArea, QMColors::accentOut);

    paintKnobText(g, gainSlot, masterGainSlider, QMColors::accentOut);
    paintKnobText(g, dryWetSlot, dryWetSlider, QMColors::accentMorph);
    paintKnobText(g, ceilingSlot, ceilingSlider, QMColors::rose);

    // ---- INFO ----
    // マウス直下のコントロールの説明を 3 行で表示する。
    // 何にも乗っていないときは使い方のヒントを薄く出しておく。
    if (!infoArea.isEmpty())
    {
        const auto box = infoArea.toFloat();

        g.setColour(QMColors::panel.withAlpha(0.50f));
        g.fillRoundedRectangle(box, 6.0f);
        g.setColour(QMColors::panelLine.withAlpha(0.30f));
        g.drawRoundedRectangle(box.reduced(0.5f), 6.0f, 1.0f);

        const bool hasInfo = currentInfo.isNotEmpty();

        g.setColour((hasInfo ? QMColors::accentFilter : QMColors::textDim).withAlpha(0.85f));
        g.fillRoundedRectangle(box.getX() + 1.5f, box.getY() + 6.0f,
                               2.5f, box.getHeight() - 12.0f, 1.25f);

        auto textArea = infoArea.reduced(14, 8);

        g.setColour(QMColors::textDim.withAlpha(0.75f));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("INFO", textArea.removeFromTop(11),
                   juce::Justification::topLeft, false);

        textArea.removeFromTop(2);

        // 3 行分。読みやすいよう従来より大きめのフォントにする。
        g.setColour(hasInfo ? QMColors::text.withAlpha(0.92f)
                            : QMColors::textDim.withAlpha(0.60f));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawFittedText(hasInfo
                             ? currentInfo
                             : "Hover any control to see what it does.",
                         textArea, juce::Justification::topLeft, 3, 1.0f);
    }
}

// ==========================================================================
void FilterPanel::paintKnobText(juce::Graphics& g, const KnobSlot& slot,
                                const juce::Slider& s, juce::Colour accent) const
{
    g.setColour(QMColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText(s.getName(), slot.label, juce::Justification::centred, false);

    // 変調中は実際に効いている値を出す（LookAndFeel 側と同じ考え方）
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
