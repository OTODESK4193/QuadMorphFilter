// ==========================================
// PluginEditor.cpp
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/ModelCapabilities.h"

QuadMorphFilterAudioProcessorEditor::QuadMorphFilterAudioProcessorEditor(
    QuadMorphFilterAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), visualizer(p), xyPad(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(visualizer);
    addAndMakeVisible(xyPad);

    setupFilterGroup(groupA, "A", "A");
    setupFilterGroup(groupB, "B", "B");
    setupFilterGroup(groupC, "C", "C");
    setupFilterGroup(groupD, "D", "D");

    setupLfoGroup(lfos[0], 1, "LFO 1 (Morph)");
    setupLfoGroup(lfos[1], 2, "LFO 2 (Cutoff)");
    setupLfoGroup(lfos[2], 3, "LFO 3 (Reso)");

    auto setupMaster = [&](juce::Label& l, juce::Slider& sl, juce::String txt,
        juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            l.setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff2980B9));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id, sl);
        };
    setupMaster(masterGainLabel, masterGainSlider, "Out Gain", "masterGain", mgAtt);
    setupMaster(dryWetLabel, dryWetSlider, "Dry/Wet", "dryWet", dwAtt);
    setupMaster(ceilingLabel, ceilingSlider, "Limit Ceil", "limiterCeiling", clAtt);

    xyModeLabel.setText("Mode", juce::dontSendNotification);
    xyModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(xyModeLabel);
    xyModeCombo.addItemList({ "Morph", "Cutoff" }, 1);
    addAndMakeVisible(xyModeCombo);
    xyModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "xyMode", xyModeCombo);

    osModeLabel.setText("OS", juce::dontSendNotification);
    osModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(osModeLabel);
    osModeCombo.addItemList({ "Off", "Auto", "2x", "4x" }, 1);
    addAndMakeVisible(osModeCombo);
    osModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "osMode", osModeCombo);

    lfoCutLabel.setText("LFO Cut", juce::dontSendNotification);
    lfoCutLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoCutLabel);

    lfoResLabel.setText("LFO Res", juce::dontSendNotification);
    lfoResLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(lfoResLabel);

    const juce::String filterNames[4] = { "A", "B", "C", "D" };
    const juce::String cutIds[4] = { "lfoCutA", "lfoCutB", "lfoCutC", "lfoCutD" };
    const juce::String resIds[4] = { "lfoResA", "lfoResB", "lfoResC", "lfoResD" };

    for (int i = 0; i < 4; ++i)
    {
        lfoCutBtn[i].setButtonText(filterNames[i]);
        lfoCutBtn[i].setClickingTogglesState(true);
        lfoCutBtn[i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xffC2185B));
        addAndMakeVisible(lfoCutBtn[i]);
        lfoCutAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, cutIds[i], lfoCutBtn[i]);

        lfoResBtn[i].setButtonText(filterNames[i]);
        lfoResBtn[i].setClickingTogglesState(true);
        lfoResBtn[i].setColour(juce::TextButton::textColourOnId, juce::Colour(0xffE65100));
        addAndMakeVisible(lfoResBtn[i]);
        lfoResAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, resIds[i], lfoResBtn[i]);
    }

    setSize(1000, 680);
}

QuadMorphFilterAudioProcessorEditor::~QuadMorphFilterAudioProcessorEditor()
{
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void QuadMorphFilterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffF0F4F8));
}

void QuadMorphFilterAudioProcessorEditor::refreshFilterGroupControls(
    FilterGroup& g, const juce::String& suffix, int modelIdx)
{
    auto [maxSlope, hasLP, hasBP, hasHP, hasNotch] = getModelCaps(modelIdx);

    if (modelIdx == 2)
    {
        // ── TB-303: ComboBoxAttachment を破棄し直接書き込みに切り替え ──
        // ComboBoxAttachment はコンボ再構築後に内部マッピングがずれ、
        // "Accent: High"（position 2）が slopeIdx=2 として伝わらない問題を回避。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("Off",  1);   // position 0 → slopeIdx 0 (Accent: Off)
        g.slope.addItem("Low",  2);   // position 1 → slopeIdx 1 (Accent: Low)
        g.slope.addItem("High", 3);   // position 2 → slopeIdx 2 (Accent: High)

        // 現在の APVTS 値を読んでコンボに反映（選択をリセットしない）
        const int curAccent = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 3, curAccent + 1),
                              juce::dontSendNotification);

        // getSelectedItemIndex()（0 基準）をそのまま slopeIdx として APVTS へ書き込む
        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex(); // 0=Off, 1=Low, 2=High
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else if (modelIdx == 4)
    {
        // ── Bitcrush/SRR: Slope → フィルターステージ数 (Stages) ──
        // 数値の意味は通常 SVF と同じ（1/2/4/8段）だが、
        // Bitcrush 文脈では "Stages" と呼ぶ方が直感的。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("1x", 1);
        g.slope.addItem("2x", 2);
        g.slope.addItem("4x", 3);
        g.slope.addItem("8x", 4);

        const int curStages = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 4, curStages + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex();
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else if (modelIdx == 6)
    {
        // ── Comb Filter: Slope → カスケード段数 (Cascade) ──
        // 多段カスケードでコームの密度が上がり、フィジカルモデリング的な
        // 複雑なエコーパターンになる。"Cascade" の方が意味が伝わりやすい。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("1x", 1);
        g.slope.addItem("2x", 2);
        g.slope.addItem("4x", 3);
        g.slope.addItem("8x", 4);

        const int curCascade = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 4, curCascade + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex();
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else if (modelIdx == 9)
    {
        // ── Wavefolder: Slope → カスケード段数 (1x / 2x / 4x / 8x) ──
        // TB-303 / Vactrol と同じくアタッチメントを破棄して直接 onChange で書き込む。
        // slopeIdx 0=1段, 1=2段, 2=4段, 3=8段 のマッピングは既存 Slope と同一。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("1x", 1);   // slopeIdx 0 → 1 fold
        g.slope.addItem("2x", 2);   // slopeIdx 1 → 2 folds cascade
        g.slope.addItem("4x", 3);   // slopeIdx 2 → 4 folds cascade
        g.slope.addItem("8x", 4);   // slopeIdx 3 → 8 folds cascade

        const int curFolds = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 4, curFolds + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex(); // 0=1x, 1=2x, 2=4x, 3=8x
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else if (modelIdx == 23)
    {
        // ── Waveguide: Slope → 反射段数 (1x / 2x / 4x / 8x) ──
        // 多段反射でリバーブ的な音場密度が増す。combBuffer を流用（Model 6 と排他使用）。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("1x", 1);
        g.slope.addItem("2x", 2);
        g.slope.addItem("4x", 3);
        g.slope.addItem("8x", 4);

        const int curRefl = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 4, curRefl + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex();
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else if (modelIdx == 21)
    {
        // ── Vactrol LPG: Slope → アタック時間 (At_Lo=1ms / At_Mid=5ms / At_Hi=20ms) ──
        // TB-303 と同じく ComboBoxAttachment を破棄して直接 onChange ハンドラで書き込む。
        g.slAtt.reset();

        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("Fast", 1);   // slopeIdx 0 → 1ms  (高速応答)
        g.slope.addItem("Mid",  2);   // slopeIdx 1 → 5ms  (標準 Vactrol)
        g.slope.addItem("Slow", 3);   // slopeIdx 2 → 20ms (なめらかなスウェル)

        const int curAtk = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        g.slope.setSelectedId(juce::jlimit(1, 3, curAtk + 1),
                              juce::dontSendNotification);

        g.slope.onChange = [this, &g, sfx = suffix]()
        {
            const int idx = g.slope.getSelectedItemIndex(); // 0=Lo, 1=Mid, 2=Hi
            if (auto* p = audioProcessor.apvts.getParameter("slope" + sfx))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(idx)));
        };
    }
    else
    {
        // ── 非 TB-303: TB-303 用ハンドラを解除 ──
        g.slope.onChange = nullptr;

        // ── モデル切替のたびにスロープコンボを再構築 ──
        // setItemEnabled だけでは ComboBoxAttachment の sendInitialUpdate と競合し
        // 無効化が確実に反映されない場合がある。アタッチメントを先に破棄してから
        // アイテムを再構成し、最後にアタッチメントを再生成することで順序を確定する。
        const int curSlopeRaw = juce::roundToInt(
            audioProcessor.apvts.getRawParameterValue("slope" + suffix)->load());
        const int clampedSlope = juce::jlimit(0, maxSlope, curSlopeRaw);

        g.slAtt.reset();  // 先にアタッチメントを破棄（removeListener）
        g.slope.clear(juce::dontSendNotification);
        g.slope.addItem("12dB", 1);
        g.slope.addItem("24dB", 2);
        g.slope.addItem("48dB", 3);
        g.slope.addItem("96dB", 4);

        // maxSlope を超えるアイテムを無効化（アタッチメント生成前に設定）
        for (int i = 1; i <= 4; ++i)
            g.slope.setItemEnabled(i, (i - 1) <= maxSlope);

        // アタッチメント再生成（sendInitialUpdate は setItemEnabled 後に発火）
        g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            audioProcessor.apvts, "slope" + suffix, g.slope);

        // clamped 値で選択を確定（通知なし：アタッチメントが同期済みのため）
        g.slope.setSelectedId(clampedSlope + 1, juce::dontSendNotification);

        // APVTS 値がクランプにより変化した場合のみ書き戻す
        if (curSlopeRaw != clampedSlope)
        {
            if (auto* p = audioProcessor.apvts.getParameter("slope" + suffix))
                p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(clampedSlope)));
        }
    }

    g.type.setItemEnabled(1, hasLP);
    g.type.setItemEnabled(2, hasBP);
    g.type.setItemEnabled(3, hasHP);
    g.type.setItemEnabled(4, hasNotch);

    // ── Type コンボのラベルをモデルに応じて切り替え ──
    // Model 22 (Modal Resonator): 音楽的な LP/BP/HP/Notch 分類ではなく
    // 「どう混ぜるか」の操作モードなので Type0〜3 と表示する。
    // アイテム ID（1〜4）は APVTS マッピングに使用するため変えない。
    if (modelIdx == 23)
    {
        // Waveguide: Type0=Wet (共鳴音のみ), Type1=Mix (励振+共鳴ブレンド)
        // HP/Notch は ModelCapabilities で無効化済み → デフォルトテキストに戻しておく
        g.type.changeItemText(1, "Wet");
        g.type.changeItemText(2, "Mix");
        g.type.changeItemText(3, "HP");
        g.type.changeItemText(4, "Notch");
    }
    else if (modelIdx == 8)
    {
        // Phaser: フィードバック極性の2択
        // LP(+FB) = 正帰還 → 暖かく深いフェイザー音
        // BP(-FB) = 負帰還 → 薄くコールドなフェイザー音
        // HP/Notch は ModelCapabilities で無効化済み → デフォルトテキストに戻す
        g.type.changeItemText(1, "+FB");
        g.type.changeItemText(2, "-FB");
        g.type.changeItemText(3, "HP");
        g.type.changeItemText(4, "Notch");
    }
    else if (modelIdx == 11)
    {
        // Phase Shift: 段間周波数スプレッドパターン選択
        g.type.changeItemText(1, "Lin");    // 線形分散
        g.type.changeItemText(2, "Log");    // 対数分散（旧実装）
        g.type.changeItemText(3, "Mirror"); // 鏡像分散
        g.type.changeItemText(4, "Rand");   // 固定疑似乱数分散
    }
    else if (modelIdx == 24)
    {
        // Bode Freq Shifter: 上側帯域 / 下側帯域 の2択
        // HP/Notch は ModelCapabilities で無効化済み → デフォルトテキストに戻す
        g.type.changeItemText(1, "Up");     // LP = 上側帯域 (A*cos - B*sin)
        g.type.changeItemText(2, "Down");   // BP = 下側帯域 (A*cos + B*sin)
        g.type.changeItemText(3, "HP");
        g.type.changeItemText(4, "Notch");
    }
    else if (modelIdx == 26)
    {
        // Phased Array: 出力ミックスモード
        // LP/HP が有効 (ModelCapabilities 維持), Blend=LP, Wet=HP
        g.type.changeItemText(1, "Blend");  // LP = ドライ + mixedPhase
        g.type.changeItemText(2, "BP");     // BP: 無効 (デフォルト表記に戻す)
        g.type.changeItemText(3, "Wet");    // HP = mixedPhase のみ
        g.type.changeItemText(4, "Notch");  // Notch: 無効
    }
    else if (modelIdx == 22)
    {
        // Modal Resonator: 混合モードとして Type0〜3 で表示
        g.type.changeItemText(1, "Type0");
        g.type.changeItemText(2, "Type1");
        g.type.changeItemText(3, "Type2");
        g.type.changeItemText(4, "Type3");
    }
    else if (modelIdx == 4)
    {
        // Bitcrush/SRR: SVF はトーン整形フィルターとして機能するため
        // LP/HP/BP/Notch ではなくキャラクター名で表示する
        g.type.changeItemText(1, "Warm");    // LP  = 低域を保護した温かいクラッシュ
        g.type.changeItemText(2, "Focus");   // BP  = 帯域を絞ったクラッシュ
        g.type.changeItemText(3, "Bright");  // HP  = 高域成分を際立たせるクラッシュ
        g.type.changeItemText(4, "Hollow");  // Notch = ノッチ周波数を除いてクラッシュ
    }
    else if (modelIdx == 6)
    {
        // Comb Filter: LP/HP/BP/Notch は動作と一致しないため専用ラベルに変更
        // Type 0 = フィードバック正帰還 (Warm)
        // Type 1 = フィードバック負帰還 (Metal/フランジャー)
        // Type 2 = フィードフォワード正帰還 (Comb ノッチ)
        // Type 3 = フィードフォワード負帰還 (Phase/反転コーム)
        g.type.changeItemText(1, "Warm");
        g.type.changeItemText(2, "Metal");
        g.type.changeItemText(3, "Comb");
        g.type.changeItemText(4, "Phase");
    }
    else
    {
        g.type.changeItemText(1, "LP");
        g.type.changeItemText(2, "BP");
        g.type.changeItemText(3, "HP");
        g.type.changeItemText(4, "Notch");
    }

    int curType = g.type.getSelectedId();
    bool typeOk = (curType == 1 && hasLP)
        || (curType == 2 && hasBP)
        || (curType == 3 && hasHP)
        || (curType == 4 && hasNotch);

    if (!typeOk)
    {
        int fallback = hasLP ? 1 : hasBP ? 2 : hasHP ? 3 : 4;
        g.type.setSelectedId(fallback, juce::sendNotification);
        if (auto* p = audioProcessor.apvts.getParameter("type" + suffix))
            p->setValueNotifyingHost(p->convertTo0to1((float)(fallback - 1)));
    }

    if (modelIdx == 8)
    {
        // Phaser: Cutoff = AP ノッチ中心周波数 → "Freq", Res = フィードバック量 → "Depth"
        g.cutoffLabel.setText("Freq",     juce::dontSendNotification);
        g.resLabel.setText("Depth",       juce::dontSendNotification);
    }
    else if (modelIdx == 11)
    {
        // Phase Shift: Cutoff = 段スプレッド中心周波数 → "Center", Res = スプレッド幅 → "Spread"
        g.cutoffLabel.setText("Center",   juce::dontSendNotification);
        g.resLabel.setText("Spread",      juce::dontSendNotification);
    }
    else if (modelIdx == 24)
    {
        // Bode Freq Shifter: Cutoff = 周波数シフト量 (-1000〜+1000Hz) → "Shift"
        //                    Res    = フィードバック量 → "Fdbk"
        g.cutoffLabel.setText("Shift",    juce::dontSendNotification);
        g.resLabel.setText("Fdbk",        juce::dontSendNotification);
    }
    else if (modelIdx == 25)
    {
        // Z-Plane 2D Morph: Cutoff = 2D 空間 X 軸 → "X"
        //                   Res    = 2D 空間 Y 軸 → "Y"
        g.cutoffLabel.setText("X",        juce::dontSendNotification);
        g.resLabel.setText("Y",           juce::dontSendNotification);
    }
    else if (modelIdx == 26)
    {
        // Phased Array: Cutoff = AP ノッチ周波数 → "Freq"
        //               Res    = フィードバック深度 → "Depth"
        g.cutoffLabel.setText("Freq",     juce::dontSendNotification);
        g.resLabel.setText("Depth",       juce::dontSendNotification);
    }
    else if (modelIdx == 4)
    {
        // Bitcrush/SRR: Cutoff = SRR 周波数 → "Rate", Res = SVF 色味 → "Color"
        g.cutoffLabel.setText("Rate",     juce::dontSendNotification);
        g.resLabel.setText("Color",       juce::dontSendNotification);
    }
    else if (modelIdx == 6)
    {
        // Comb Filter: Cutoff = 基音周波数 → "Freq", Res = フィードバック係数 → "Feedback"
        g.cutoffLabel.setText("Freq",     juce::dontSendNotification);
        g.resLabel.setText("FB",           juce::dontSendNotification);
    }
    else if (modelIdx == 7)
    {
        // MS-20: Cutoff はそのまま, Res = 実機パネル刻印 "Peak" に合わせる
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Peak",        juce::dontSendNotification);
    }
    else if (modelIdx == 9)
    {
        // Wavefolder: Res = フォールド深度（Drive）
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Drive",       juce::dontSendNotification);
    }
    else if (modelIdx == 17)
    {
        // Butterworth: Reso = Q 倍率（カットオフ付近のピーク量）
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Peak",        juce::dontSendNotification);
    }
    else if (modelIdx == 18)
    {
        // Chebyshev: Reso = パスバンドリップル量
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Ripple",      juce::dontSendNotification);
    }
    else if (modelIdx == 19)
    {
        // Bessel: Reso = 位相線形度（低=線形, 高=急峻）
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Phase",       juce::dontSendNotification);
    }
    else if (modelIdx == 20)
    {
        // Elliptic: Reso = ストップバンドノッチ位置（低=Wide, 高=Steep）
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Stop",        juce::dontSendNotification);
    }
    else if (modelIdx == 21)
    {
        // Vactrol LPG: Cutoff=開口量, Res=リリース時間
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Rel",         juce::dontSendNotification);
    }
    else if (modelIdx == 23)
    {
        // Waveguide: Cutoff = 共鳴ピッチ（遅延長）→ "Tune"
        //            Res    = フィードバック量 = 音の持続時間 → "Decay"
        g.cutoffLabel.setText("Tune",     juce::dontSendNotification);
        g.resLabel.setText("Decay",       juce::dontSendNotification);
    }
    else
    {
        g.cutoffLabel.setText("Cut",      juce::dontSendNotification);
        g.resLabel.setText("Res",         juce::dontSendNotification);
    }
}

void QuadMorphFilterAudioProcessorEditor::setupFilterGroup(FilterGroup& g,
    juce::String s,
    juce::String name)
{
    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff00ACC1));
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "enable" + s, g.enableButton);

    // モデル名: 括弧を削除しコンパクト化（全28モデル、ID=1〜28に対応）
    g.model.addItemList({
        "Clean SVF",    "Moog Ladder",  "TB-303",       "Oberheim SEM", "Bitcrush",
        "Vowel Filter", "Comb Filter",  "MS-20",        "Phaser",       "Wavefolder",
        "FDN Reverb",   "Phase Shift",
        "CEM3320",      "SSM2040",      "CS-80",        "Roland Jupiter", "EDP Wasp",
        "Butterworth",  "Chebyshev",    "Bessel",       "Elliptic",
        "Vactrol LPG",  "Modal Res",    "Waveguide",    "Bode Shifter",
        "Z-Plane",      "Phased Array", "Nyquist AA"
        }, 1);
    addAndMakeVisible(g.model);
    g.mAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "model" + s, g.model);

    g.type.addItemList({ "LP", "BP", "HP", "Notch" }, 1);
    addAndMakeVisible(g.type);
    g.tAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "type" + s, g.type);

    g.slope.addItemList({ "12dB", "24dB", "48dB", "96dB" }, 1);
    addAndMakeVisible(g.slope);
    g.slAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "slope" + s, g.slope);

    auto setup = [&](juce::Label& l, juce::Slider& sl, juce::String txt, juce::String id,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att)
        {
            l.setText(txt, juce::dontSendNotification);
            addAndMakeVisible(l);
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 50, 18);
            sl.setColour(juce::Slider::thumbColourId, juce::Colour(0xff00ACC1));
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, id + s, sl);
        };
    setup(g.cutoffLabel, g.cutoff, "Cut", "cutoff", g.cAtt);
    setup(g.resLabel, g.res, "Res", "res", g.rAtt);

    g.model.onChange = [this, &g, s]()
        {
            int modelIdx = g.model.getSelectedId() - 1;
            refreshFilterGroupControls(g, s, modelIdx);
        };

    int initialModel = (int)audioProcessor.apvts.getRawParameterValue("model" + s)->load();
    refreshFilterGroupControls(g, s, initialModel);
}

void QuadMorphFilterAudioProcessorEditor::setupLfoGroup(LfoGroup& g, int idx, juce::String name)
{
    juce::String id = "lfo" + juce::String(idx);
    juce::Colour lfoCols[] = {
        juce::Colour(0xff00D2D3),
        juce::Colour(0xffFF9FF3),
        juce::Colour(0xffFEECA1)
    };

    g.enableButton.setButtonText(name);
    g.enableButton.setClickingTogglesState(true);
    g.enableButton.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.enableButton);
    g.eAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "en", g.enableButton);

    g.wave.addItemList({
        "Sine", "SAW", "Pulse", "Random 1", "Random 2", "Noise", "Recording",
        "Smooth Noise", "Spirograph", "Harmonic Swarm", "3D Torus Knot",
        "Lissajous", "Spiral", "Star", "Rose", "Lemniscate", "Billiard",
        "Polygon", "Attractor Orbit"
        }, 1);
    addAndMakeVisible(g.wave);
    g.wAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "wave", g.wave);

    g.boundCombo.addItemList({ "Clip", "Bounce", "Wrap" }, 1);
    addAndMakeVisible(g.boundCombo);
    g.bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "bound", g.boundCombo);

    g.stepMode.setButtonText("Step");
    g.stepMode.setClickingTogglesState(true);
    g.stepMode.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    addAndMakeVisible(g.stepMode);
    g.sAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "step", g.stepMode);

    g.syncToggle.setButtonText("Sync");
    g.syncToggle.setClickingTogglesState(true);
    g.syncToggle.setColour(juce::TextButton::textColourOnId, lfoCols[idx - 1]);
    g.syncToggle.onClick = [this] { resized(); };
    addAndMakeVisible(g.syncToggle);
    g.syAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, id + "sync", g.syncToggle);

    g.rateSync.addItemList({
        "8/1", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64",
        "1/1D", "1/2D", "1/4D", "1/8D", "1/16D", "1/32D",
        "1/1T", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T"
        }, 1);
    addAndMakeVisible(g.rateSync);
    g.rsAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, id + "rateSync", g.rateSync);

    auto setupSlider = [&](juce::Slider& sl,
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
        juce::String paramId)
        {
            sl.setSliderStyle(juce::Slider::LinearHorizontal);
            sl.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 45, 18);
            sl.setColour(juce::Slider::thumbColourId, lfoCols[idx - 1]);
            addAndMakeVisible(sl);
            att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.apvts, paramId, sl);
        };
    setupSlider(g.rateFree, g.rfAtt, id + "rateFree");
    setupSlider(g.minSlider, g.minAtt, id + "min");
    setupSlider(g.maxSlider, g.maxAtt, id + "max");
}

void QuadMorphFilterAudioProcessorEditor::resized()
{
    const int pad = 15;
    auto b = getLocalBounds().reduced(pad);
    const int totalW = b.getWidth();

    const int visW = (int)(totalW * 0.7f);
    const int rightX = b.getX() + visW;
    const int rightW = b.getRight() - rightX;

    {
        auto top = b.removeFromTop(300);
        visualizer.setBounds(top.removeFromLeft(visW).reduced(5));
        xyPad.setBounds(top.reduced(5));
    }
    b.removeFromTop(4);

    {
        int ctrlY = b.getY();
        const int rowH = 24;
        const int gap = 2;

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            auto modeHalf = area.removeFromLeft(area.getWidth() / 2 - 4);
            xyModeLabel.setBounds(modeHalf.removeFromLeft(42).reduced(0, 1));
            xyModeCombo.setBounds(modeHalf);
            area.removeFromLeft(8);
            osModeLabel.setBounds(area.removeFromLeft(24).reduced(0, 1));
            osModeCombo.setBounds(area);
            ctrlY += rowH + gap;
        }

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            lfoCutLabel.setBounds(area.removeFromLeft(52).reduced(0, 1));
            int bw = area.getWidth() / 4;
            for (int i = 0; i < 4; ++i)
                lfoCutBtn[i].setBounds(area.removeFromLeft(bw).reduced(2, 1));
            ctrlY += rowH + gap;
        }

        {
            auto area = juce::Rectangle<int>(rightX, ctrlY, rightW, rowH).reduced(4, 1);
            lfoResLabel.setBounds(area.removeFromLeft(52).reduced(0, 1));
            int bw = area.getWidth() / 4;
            for (int i = 0; i < 4; ++i)
                lfoResBtn[i].setBounds(area.removeFromLeft(bw).reduced(2, 1));
        }
    }

    // ── フィルター行レイアウト（等幅スライダー設計）──
    //
    // Cutoff・Reso スライダーを等幅にするため、固定消費を除いた残り幅を 2 等分する。
    //
    //   固定消費: enable(28) + model(145) + type(68) + slope(80)
    //             + cutoffLabel(34) + gap(4) + resLabel(55)  = 414px
    //
    //   type  68px: "Hollow"/"Bright" (6文字) まで表示可能
    //   cutoffLabel 34px: "Rate"/"Freq" (4文字) が余裕で表示可能
    //   resLabel 55px: 最長 "Ripple" (6文字) まで — "Feedback"→"FB"、"Stopband"→"Stop" に省略
    //
    //   sliderAreaW = (visW - 10 - 414) / 2
    //   行全体 = 414 + 2*sliderAreaW = visW - 10 → FilterVisualizer 右端に揃う

    const int fixedConsumed = 28 + 145 + 68 + 80 + 34 + 4 + 55;   // = 414
    const int sliderAreaW   = std::max(80, (visW - 10 - fixedConsumed) / 2);

    for (auto* g : { &groupA, &groupB, &groupC, &groupD })
    {
        auto r = b.removeFromTop(28).reduced(5, 2);

        g->enableButton.setBounds(r.removeFromLeft(28).reduced(0, 2));
        g->model.setBounds(r.removeFromLeft(145).reduced(2, 2));
        g->type.setBounds(r.removeFromLeft(68).reduced(2, 2));   // 55→68: "Hollow"対応
        g->slope.setBounds(r.removeFromLeft(80).reduced(2, 2));

        // Cutoff: label(34) + slider(sliderAreaW)
        auto cutArea = r.removeFromLeft(34 + sliderAreaW).reduced(2, 0);
        g->cutoffLabel.setBounds(cutArea.removeFromLeft(34));
        g->cutoff.setBounds(cutArea);

        r.removeFromLeft(4);

        // Reso: label(55) + slider(sliderAreaW) — Cutoff と同じ幅
        auto resArea = r.removeFromLeft(55 + sliderAreaW).reduced(2, 0);
        g->resLabel.setBounds(resArea.removeFromLeft(55));
        g->res.setBounds(resArea);
    }
    b.removeFromTop(6);

    for (int i = 0; i < 3; ++i)
    {
        auto r = b.removeFromTop(28).reduced(5, 2);

        lfos[i].enableButton.setBounds(r.removeFromLeft(100).reduced(0, 2));
        lfos[i].wave.setBounds(r.removeFromLeft(120).withSizeKeepingCentre(115, 20));
        lfos[i].boundCombo.setBounds(r.removeFromLeft(70).withSizeKeepingCentre(65, 20));
        lfos[i].stepMode.setBounds(r.removeFromLeft(50).reduced(2, 2));
        lfos[i].syncToggle.setBounds(r.removeFromLeft(50).reduced(2, 2));

        auto remainW = r.getWidth();
        auto rateArea = r.removeFromLeft(remainW / 3);
        auto minArea = r.removeFromLeft(remainW / 3);
        auto maxArea = r;

        bool isSynced = audioProcessor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "sync")->load() > 0.5f;
        if (isSynced) {
            lfos[i].rateSync.setBounds(
                rateArea.withSizeKeepingCentre(rateArea.getWidth() - 5, 20));
            lfos[i].rateFree.setVisible(false);
            lfos[i].rateSync.setVisible(true);
        }
        else {
            lfos[i].rateFree.setBounds(rateArea.reduced(2, 5));
            lfos[i].rateFree.setVisible(true);
            lfos[i].rateSync.setVisible(false);
        }
        lfos[i].minSlider.setBounds(minArea.reduced(2, 5));
        lfos[i].maxSlider.setBounds(maxArea.reduced(2, 5));
    }

    b.removeFromTop(10);

    auto masterArea = b.removeFromTop(28).reduced(5, 2);
    auto cellW = masterArea.getWidth() / 3;

    auto gainRect = masterArea.removeFromLeft(cellW);
    masterGainLabel.setBounds(gainRect.removeFromLeft(60));
    masterGainSlider.setBounds(gainRect);

    auto dwRect = masterArea.removeFromLeft(cellW);
    dryWetLabel.setBounds(dwRect.removeFromLeft(60));
    dryWetSlider.setBounds(dwRect);

    ceilingLabel.setBounds(masterArea.removeFromLeft(60));
    ceilingSlider.setBounds(masterArea);
}