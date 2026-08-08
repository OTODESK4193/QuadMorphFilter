// ==========================================
// UI/FilterPanel.cpp   （V1.1.0 新規）
// ==========================================
#include "FilterPanel.h"
#include "../PluginProcessor.h"
#include "../DSP/ModelCapabilities.h"

namespace
{
    // ---- 列幅（論理座標）----
    constexpr int kEnW = 30;
    constexpr int kModelW = 152;
    constexpr int kTypeW = 74;
    constexpr int kSlopeW = 84;
    constexpr int kSrcW = 54;

    constexpr int kGapS = 6;
    constexpr int kGapM = 8;

    // 0=Off, 1=+X, 2=+Y, 3=-X, 4=-Y
    const char* const kSrcLabels[] = { "--", "+X", "+Y", "-X", "-Y" };

    const char* const kFilterNames[4] = { "A", "B", "C", "D" };
}

// ==========================================================================
FilterPanel::FilterPanel(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    for (int i = 0; i < 4; ++i)
        setupGroup(groups[(size_t)i], i, kFilterNames[i]);

    // ---- MORPH ----
    morphBlendCombo.addItemList({ "Equal Power", "Linear", "Smoothstep", "Radial" }, 1);
    addAndMakeVisible(morphBlendCombo);
    morphBlendAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "morphBlend", morphBlendCombo);

    cutoffAlgoCombo.addItemList({ "Absolute", "Relative", "Zone" }, 1);
    addAndMakeVisible(cutoffAlgoCombo);
    cutoffAlgoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "cutoffAlgo", cutoffAlgoCombo);

    updateLfoSrcButtons();
    startTimerHz(20);
}

FilterPanel::~FilterPanel()
{
    stopTimer();   // Timer は必ずデストラクタ最優先で停止（CLAUDE.md §3）
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

void FilterPanel::timerCallback()
{
    if (isVisible())
        updateLfoSrcButtons();
}

// ==========================================================================
void FilterPanel::refreshTheme()
{
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

    const int fixed = kEnW + kModelW + kTypeW + kSlopeW + kSrcW * 2;
    const int gaps = kGapS * 5 + kGapM * 2;
    const int barW = juce::jmax(70, (r.getWidth() - fixed - gaps) / 2);

    L.enable = r.removeFromLeft(kEnW);    r.removeFromLeft(kGapS);
    L.model = r.removeFromLeft(kModelW);  r.removeFromLeft(kGapS);
    L.type = r.removeFromLeft(kTypeW);    r.removeFromLeft(kGapS);
    L.slope = r.removeFromLeft(kSlopeW);  r.removeFromLeft(kGapM);
    L.cutoff = r.removeFromLeft(barW);    r.removeFromLeft(kGapS);
    L.res = r.removeFromLeft(barW);       r.removeFromLeft(kGapM);
    L.lfoCut = r.removeFromLeft(kSrcW);   r.removeFromLeft(kGapS);
    L.lfoRes = r.removeFromLeft(kSrcW);

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
        g.model.setBounds(L.model.withSizeKeepingCentre(kModelW, QMUI::kCtrlH));
        g.type.setBounds(L.type.withSizeKeepingCentre(kTypeW, QMUI::kCtrlH));
        g.slope.setBounds(L.slope.withSizeKeepingCentre(kSlopeW, QMUI::kCtrlH));
        g.cutoff.setBounds(L.cutoff.withSizeKeepingCentre(L.cutoff.getWidth(), QMUI::kBarH));
        g.res.setBounds(L.res.withSizeKeepingCentre(L.res.getWidth(), QMUI::kBarH));
        g.lfoCutBtn.setBounds(L.lfoCut.withSizeKeepingCentre(kSrcW, QMUI::kCtrlH));
        g.lfoResBtn.setBounds(L.lfoRes.withSizeKeepingCentre(kSrcW, QMUI::kCtrlH));
    }

    area.removeFromTop(10);
    morphHeaderArea = area.removeFromTop(QMUI::kHeadH);
    area.removeFromTop(4);
    morphRowArea = area.removeFromTop(QMUI::kCtrlH);

    {
        auto r = morphRowArea;
        r.removeFromLeft(58);                                 // "BLEND" ラベル分
        morphBlendCombo.setBounds(r.removeFromLeft(130));
        r.removeFromLeft(30);
        r.removeFromLeft(84);                                 // "CUTOFF ALGO" ラベル分
        cutoffAlgoCombo.setBounds(r.removeFromLeft(120));
    }
}

// ==========================================================================
void FilterPanel::paint(juce::Graphics& g)
{
    QMUI::drawSectionHeader(g, "FILTER MATRIX", headerArea, QMColors::accentFilter);

    // ---- 列見出し ----
    {
        const auto L = layoutRow(columnArea);
        QMUI::drawColumnLabel(g, "ON", L.enable);
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

    // ---- MORPH ----
    QMUI::drawSectionHeader(g, "MORPH", morphHeaderArea, QMColors::accentMorph);

    g.setColour(QMColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("BLEND", morphRowArea.getX(), morphRowArea.getY(), 54, morphRowArea.getHeight(),
               juce::Justification::centredLeft, false);
    g.drawText("CUTOFF ALGO", morphRowArea.getX() + 58 + 130 + 30, morphRowArea.getY(), 80,
               morphRowArea.getHeight(), juce::Justification::centredLeft, false);
}
