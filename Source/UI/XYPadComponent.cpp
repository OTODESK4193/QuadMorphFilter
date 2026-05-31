// ==========================================
// UI/XYPadComponent.cpp
// ==========================================
//
// 【座標系の設計】
//   ラベル A/B/C/D の視覚的中心をコーナー 100% 点として使用する。
//   各ラベルの中心は以下の位置に描画される:
//     A: (20, 20)         B: (width-20, 20)
//     C: (20, height-20)  D: (width-20, height-20)
//
//   正規化 [0,1] ↔ ピクセル 変換:
//     toNorm: px → (px - 20) / (width - 40),  py → (py - 20) / (height - 40)
//     toPix:  nx → 20 + nx * (width - 40),     ny → 20 + ny * (height - 40)
//
//   この設計により「ラベルの場所でそのフィルターが 100%」になる。
//   旧実装: px/getWidth() を使用していたため、ラベル位置で 88-99% 止まりだった。
// ==========================================
#include "XYPadComponent.h"
#include "../PluginProcessor.h"

// ────────────────────────────────────────
// 座標変換ヘルパー (インスタンスメソッド内で使用)
// ────────────────────────────────────────
namespace {
    // ピクセル → 正規化 [0,1] (ラベル中心を 0/1 とする)
    inline juce::Point<float> toNorm(float px, float py, float w, float h)
    {
        const float iL = 20.0f, iR = w - 20.0f;
        const float iT = 20.0f, iB = h - 20.0f;
        return {
            juce::jlimit(0.0f, 1.0f, (px - iL) / (iR - iL)),
            juce::jlimit(0.0f, 1.0f, (py - iT) / (iB - iT))
        };
    }
    // 正規化 [0,1] → ピクセル (描画用)
    inline juce::Point<float> toPix(float nx, float ny, float w, float h)
    {
        const float iL = 20.0f, iR = w - 20.0f;
        const float iT = 20.0f, iB = h - 20.0f;
        return { iL + nx * (iR - iL), iT + ny * (iB - iT) };
    }
}

// ────────────────────────────────────────
// コンストラクタ
// ────────────────────────────────────────
XYPadComponent::XYPadComponent(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    for (int i = 0; i < 3; ++i)
        trails[i].fill({ 0.5f, 0.5f });
    startTimerHz(60);
}

// ────────────────────────────────────────
// timerCallback: UI 同期
// ────────────────────────────────────────
void XYPadComponent::timerCallback()
{
    // 各LFOで wave が 6 でなくなったら recording フラグをリセット（軽量版）
    // ※ wave チェックは recording[i] が true のときのみ実行
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;

        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "wave")->load();
        if (wave != 6)
        {
            recording[i] = false;
            recordingLength[i] = 0;
            std::fill(pixelMap[i].begin(), pixelMap[i].end(), 0);
        }
    }

    for (int i = 0; i < 3; ++i) {
        trails[i][trailIdx[i]] = processor.getLfoPos(i);
        trailIdx[i] = (trailIdx[i] + 1) % 30;
    }

    // ===== Cutoffモード: XY位置をスライダーに書き戻し (UI同期) =====
    int xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();
    if (xyMode == 1)
    {
        auto mPos = processor.getLfoPos(0);
        const int cutoffAlgo = (int)processor.apvts.getRawParameterValue("cutoffAlgo")->load();

        float xyCutoff, xyRes;
        if (cutoffAlgo == 1)
        {
            const float devX = (mPos.x - 0.5f) * 2.0f;
            const float devY = (0.5f - mPos.y) * 2.0f;
            xyCutoff = 632.0f * std::pow(2.0f, devX * 4.0f);
            xyRes    = 1.0f   * std::pow(2.0f, devY * 2.0f);
        }
        else if (cutoffAlgo == 2)
        {
            const float devX = (mPos.x - 0.5f) * 2.0f;
            const float devY = (0.5f - mPos.y) * 2.0f;
            xyCutoff = 632.0f * std::pow(2.0f, (devX >= 0.0f ? devX * 5.0f : devX * 3.0f));
            xyRes    = 1.0f   * std::pow(2.0f, (devY >= 0.0f ? devY * 3.0f : devY * 2.0f));
        }
        else
        {
            xyCutoff = 20.0f * std::pow(1000.0f, mPos.x);
            xyRes    = 0.1f + (1.0f - mPos.y) * 9.9f;
        }
        xyCutoff = juce::jlimit(20.0f, 20000.0f, xyCutoff);
        xyRes    = juce::jlimit(0.1f,  10.0f,    xyRes);

        // ===== LFO2/3 変調値をフィルターごとに計算 =====
        // LFO2 → Cutoff 変調、LFO3 → Reso 変調
        // Cutoffモードでは xyCutoff/xyRes を基準に LFO2/3 が ±変調するため、
        // スライダーにはフィルターごとの実際の変調済み値を表示する。
        // (A=+X, C=−X に設定していれば A と C で値が異なって見える)
        auto lfo2Pos     = processor.getLfoPos(1);
        auto lfo2Mod4    = processor.getLfoMod4(1);
        bool lfo2En      = processor.apvts.getRawParameterValue("lfo2en")->load() > 0.5f;
        bool lfo2IsRand1 = (juce::roundToInt(processor.apvts.getRawParameterValue("lfo2wave")->load()) == 3) && lfo2En;
        auto cM          = MorphEngine::computeModulation(lfo2Pos, lfo2Mod4, lfo2IsRand1);

        auto lfo3Pos     = processor.getLfoPos(2);
        auto lfo3Mod4    = processor.getLfoMod4(2);
        bool lfo3En      = processor.apvts.getRawParameterValue("lfo3en")->load() > 0.5f;
        bool lfo3IsRand1 = (juce::roundToInt(processor.apvts.getRawParameterValue("lfo3wave")->load()) == 3) && lfo3En;
        auto rM          = MorphEngine::computeModulation(lfo3Pos, lfo3Mod4, lfo3IsRand1);

        const juce::String suffixes[4] = { "A", "B", "C", "D" };
        for (const auto& s : suffixes)
        {
            const int cutSrc = juce::roundToInt(
                processor.apvts.getRawParameterValue("lfoCutSrc" + s)->load());
            if (cutSrc > 0)
            {
                // フィルターごとの変調ソースで実際のカットオフを計算
                const int cutModIdx = cutSrc - 1;  // 0=+X,1=+Y,2=-X,3=-Y
                float actualCutoff  = lfo2En
                    ? MorphEngine::applyFrequencyMod(xyCutoff, cM[cutModIdx])
                    : xyCutoff;
                actualCutoff = juce::jlimit(20.0f, 20000.0f, actualCutoff);

                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                        processor.apvts.getParameter("cutoff" + s)))
                {
                    if (std::abs(p->get() - actualCutoff) > 0.5f)
                        *p = juce::jlimit(p->range.start, p->range.end, actualCutoff);
                }
            }

            const int resSrc = juce::roundToInt(
                processor.apvts.getRawParameterValue("lfoResSrc" + s)->load());
            if (resSrc > 0)
            {
                const int resModIdx = resSrc - 1;
                float actualRes     = lfo3En
                    ? MorphEngine::applyResonanceMod(xyRes, rM[resModIdx])
                    : xyRes;
                actualRes = juce::jlimit(0.1f, 10.0f, actualRes);

                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                        processor.apvts.getParameter("res" + s)))
                {
                    if (std::abs(p->get() - actualRes) > 0.01f)
                        *p = juce::jlimit(p->range.start, p->range.end, actualRes);
                }
            }
        }
    }

    repaint();
}

// ────────────────────────────────────────
// paint
// ────────────────────────────────────────
void XYPadComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff444444));
    g.drawRoundedRectangle(b, 8.0f, 1.5f);

    int xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();

    // コーナーラベル (中心がコーナー 100% 点)
    g.setColour(juce::Colours::white.withAlpha(xyMode == 1 ? 0.1f : 0.3f));
    g.setFont(14.0f);
    g.drawText("A", 10,         10,              20, 20, juce::Justification::centred);
    g.drawText("B", (int)w-30,  10,              20, 20, juce::Justification::centred);
    g.drawText("C", 10,         (int)h-30,       20, 20, juce::Justification::centred);
    g.drawText("D", (int)w-30,  (int)h-30,       20, 20, juce::Justification::centred);

    // Cutoffモード: 軸ラベル
    if (xyMode == 1)
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(10.0f);
        g.drawText("Cutoff ->",
            (int)w/2 - 35, (int)h - 15, 70, 12, juce::Justification::centred);
        g.drawText("^ Reso",
            3, (int)h/2 - 25, 40, 12, juce::Justification::centredLeft);
    }

    // ===== Recording グリッド描画（wave=="6" かつ recording[i]==true のLFOのみ） =====
    // recording[i]==true は mouseDown で設定され、mouseUp/finishRecording() で false になる
    // つまり、実際にドラッグ中のLFOだけがグリッド表示される
    bool anyRecordingActive = false;
    for (int i = 0; i < 3; ++i)
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "wave")->load();
        if (wave == 6 && recording[i])  // wave==6 かつ recording[i]==true
        {
            anyRecordingActive = true;
            break;
        }
    }

    if (anyRecordingActive)
    {
        paintRecordingGrid(g);
        return;  // Recording モード時はグリッド表示のみ
    }

    // LFO トレイルと現在位置 (toPix で正規化 → ピクセル変換)
    juce::Colour colors[] = {
        juce::Colour(0xff00bcd4),
        juce::Colour(0xff4dd0e1),
        juce::Colour(0xffff9900)
    };

    for (int i = 0; i < 3; ++i)
    {
        if (processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "en")->load() < 0.5f)
            continue;

        // トレイル描画
        for (int t = 0; t < 30; ++t) {
            int  idx = (trailIdx[i] + t) % 30;
            auto pt  = trails[i][idx];
            auto px  = toPix(pt.x, pt.y, w, h);
            float alpha = (float)t / 30.0f * 0.5f;
            g.setColour(colors[i].withAlpha(alpha));
            g.fillEllipse(px.x - 3.0f, px.y - 3.0f, 6.0f, 6.0f);
        }

        // 現在位置ドット
        auto pos = processor.getLfoPos(i);
        auto px  = toPix(pos.x, pos.y, w, h);
        bool isWait = processor.isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = processor.isRecordingDrag[i].load(std::memory_order_acquire);

        if (isWait) {
            if (isDrag) {
                g.setColour(colors[i].brighter(0.8f));
                g.fillEllipse(px.x - 8.0f, px.y - 8.0f, 16.0f, 16.0f);
            }
            else {
                float alpha = 0.3f + 0.7f * std::abs(
                    std::sin(juce::Time::getMillisecondCounter() * 0.005f));
                g.setColour(colors[i].withAlpha(alpha));
                g.fillEllipse(px.x - 8.0f, px.y - 8.0f, 16.0f, 16.0f);
            }
        }
        else {
            g.setColour(colors[i]);
            g.fillEllipse(px.x - 6.0f, px.y - 6.0f, 12.0f, 12.0f);
        }
        g.setColour(juce::Colours::white);
        g.drawEllipse(px.x - 8.0f, px.y - 8.0f, 16.0f, 16.0f, 1.0f);
    }
}

// ────────────────────────────────────────
// マウスイベント
// ────────────────────────────────────────
void XYPadComponent::mouseDown(const juce::MouseEvent& e)
{
    // 通常の XYPad ドラッグ
    updatePosition(e);

    // Recording モード時（wave=="6"）：Recording を開始
    startRecording();

    // Recording 中のLFOについてのみポイント記録開始
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;

        // recBuffer にも記録開始（ピクセルは mouseDrag で塗りつぶし）
        auto n = toNorm((float)e.x, (float)e.y,
                        (float)getWidth(), (float)getHeight());
        processor.recBuffer[i][0] = { n.x, n.y };
        processor.recLength[i].store(1);
        processor.currentRecX[i].store(n.x);
        processor.currentRecY[i].store(n.y);

        // 補間用の前回位置を初期化
        lastRecX[i] = n.x;
        lastRecY[i] = n.y;

        // 【修正】ドラッグ中フラグ → LFO がマウス現在位置をリアルタイム表示
        processor.isRecordingDrag[i].store(true, std::memory_order_release);

        // ピクセル記録
        recordPixel((float)e.x / getWidth(), (float)e.y / getHeight());
    }

    repaint();
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto n = toNorm((float)e.x, (float)e.y,
                    (float)getWidth(), (float)getHeight());

    recordPixel((float)e.x / getWidth(), (float)e.y / getHeight());

    // Recording 中の各LFOに線形補間でポイント記録
    bool isRecording = false;
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;
        isRecording = true;

        int len = processor.recLength[i].load();
        if (len < 2048)
        {
            // 前回位置から現在位置への距離を計算
            float dx = n.x - lastRecX[i];
            float dy = n.y - lastRecY[i];
            float dist = std::hypot(dx, dy);

            // 距離に応じて補間ポイント数を決定（最大5ポイント）
            int interpolationSteps = (int)std::min(5.0f, std::max(1.0f, dist * 10.0f));

            for (int step = 1; step <= interpolationSteps && len < 2048; ++step)
            {
                float t = (float)step / interpolationSteps;
                float px = lastRecX[i] + dx * t;
                float py = lastRecY[i] + dy * t;

                processor.recBuffer[i][len] = { px, py };
                len++;
            }

            processor.recLength[i].store(len);
            processor.currentRecX[i].store(n.x);
            processor.currentRecY[i].store(n.y);

            lastRecX[i] = n.x;
            lastRecY[i] = n.y;
        }
    }

    if (isRecording)
    {
        repaint();
        return;
    }

    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent&)
{
    bool anyRecording = false;
    for (int i = 0; i < 3; ++i)
    {
        if (recording[i])
        {
            anyRecording = true;
            break;
        }
    }

    if (anyRecording)
    {
        finishRecording();
        repaint();
        return;
    }

    if (draggingLfoIndex != -1) {
        processor.isRecordingDrag[draggingLfoIndex].store(false);
        draggingLfoIndex = -1;
    }
}

void XYPadComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    // ダブルクリックで XY パッドを中央 (0.5, 0.5) にリセット
    processor.apvts.getParameter("posX")->setValueNotifyingHost(0.5f);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(0.5f);
}

bool XYPadComponent::hitTest(int x, int y)
{
    // 角丸矩形 (radius=8) の内側のみをヒット判定エリアとする
    juce::Path p;
    p.addRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    return p.contains((float)x, (float)y);
}

// ────────────────────────────────────────
// updatePosition: マウス座標を正規化して APVTS に書き込む
// ────────────────────────────────────────
void XYPadComponent::updatePosition(const juce::MouseEvent& e)
{
    // ラベル中心座標系 (A/C 中心 x=20 → 0, B/D 中心 x=w-20 → 1 等) で正規化
    // 枠外ドラッグは枠端に吸着 (jlimit により)
    auto n = toNorm((float)e.x, (float)e.y,
                    (float)getWidth(), (float)getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(n.x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(n.y);
}

// ────────────────────────────────────────
// Recording モード検出
// ────────────────────────────────────────
bool XYPadComponent::isRecordingMode() const
{
    for (int i = 0; i < 3; ++i)
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "wave")->load();
        if (wave == 6)  // Custom/Recording モード
            return true;
    }
    return false;
}

// ────────────────────────────────────────
// Recording 開始（wave=="6" のLFO）
// ────────────────────────────────────────
void XYPadComponent::startRecording()
{
    // 既に録音中なら何もしない
    if (recording[0] || recording[1] || recording[2]) return;

    // ===== ラウンドロビン方式: nextRecordTarget から順に wave==6 のLFOを探す =====
    // クリックのたびに LFO1→LFO2→LFO3→LFO1... とターゲットが切り替わる。
    // → LFO1 を再録音したいときはクリックを繰り返すだけでよい。
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        int idx = (nextRecordTarget + attempt) % 3;
        int wave = juce::roundToInt(processor.apvts.getRawParameterValue(
            "lfo" + juce::String(idx + 1) + "wave")->load());

        if (wave == 6)
        {
            recording[idx] = true;
            recordingLength[idx] = 0;
            pixelSeq[idx].clear();                              // Q2: ピクセル順序をリセット
            std::fill(pixelMap[idx].begin(), pixelMap[idx].end(), 0);
            processor.recLength[idx].store(0);
            processor.isWaitingForRecord[idx].store(true,  std::memory_order_release);
            processor.isRecordingDrag[idx].store   (false, std::memory_order_release);
            nextRecordTarget = (idx + 1) % 3;                  // 次回ターゲットを進める
            return;  // 1クリック = 1LFOのみ録音
        }
    }
}

// ────────────────────────────────────────
// ピクセル記録（正規化座標）→ 各LFOの pixelMap に記録
// ────────────────────────────────────────
void XYPadComponent::recordPixel(float xNorm, float yNorm)
{
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;

        int gx = juce::jlimit(0, GRID_SIZE - 1, (int)(xNorm * GRID_SIZE));
        int gy = juce::jlimit(0, GRID_SIZE - 1, (int)(yNorm * GRID_SIZE));

        pixelMap[i][gy * GRID_SIZE + gx] = 255;

        // Q2: 通過ピクセルを順序付きで記録（2048上限）
        // 同じピクセルを連続で通っても重複を除いてよりコンパクトなシーケンスにする
        if ((int)pixelSeq[i].size() < 2048)
        {
            // 直前と同じセルなら重複追加しない（1ピクセル内でのマウス微振動対策）
            if (pixelSeq[i].empty() || pixelSeq[i].back() != juce::Point<int>{gx, gy})
                pixelSeq[i].push_back({gx, gy});
        }
    }
}

// ────────────────────────────────────────
// Recording 完了・データ反映
// ────────────────────────────────────────
void XYPadComponent::finishRecording()
{
    // 各LFOで recording フラグが立っていたら Recording 完了
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;

        recording[i] = false;

        // ===== Q2: ピクセル中心座標からバッファを構築 =====
        // 通過ピクセルの中心を正規化座標 [0,1] に変換してLFOデータとして使用。
        // これにより、Step OFF = Hermite補間でスムーズ、Step ON = ステップ再生 になる。
        std::array<juce::Point<float>, 2048> buffer;
        int len = 0;
        {
            const auto& seq = pixelSeq[i];
            const int seqSize = juce::jmin((int)seq.size(), 2048);
            for (int k = 0; k < seqSize; ++k)
            {
                // ピクセルセル中心 → [0, 1] 正規化
                float cx = (seq[k].x + 0.5f) / (float)GRID_SIZE;
                float cy = (seq[k].y + 0.5f) / (float)GRID_SIZE;
                buffer[len++] = { cx, cy };
            }
        }

        recordingLength[i] = len;

        // LfoEngine に Recording データを反映
        processor.setLfoRecordingData(i, buffer, len);

        // 【修正①】録音完了 → LfoEngine の isWaiting を解除（再生モードへ）
        processor.isWaitingForRecord[i].store(false, std::memory_order_release);
        processor.isRecordingDrag[i].store   (false, std::memory_order_release);

        // 【修正②】フェーズを先頭にリセット → 録音した軌跡が必ず先頭から再生される
        processor.resetLfoPhase(i);
    }
}

// ────────────────────────────────────────
// Recording グリッド描画（各LFO独立）
// ────────────────────────────────────────
void XYPadComponent::paintRecordingGrid(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // 背景
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1E272E));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
    g.drawRoundedRectangle(b, 8.0f, 1.5f);

    // グリッド配置（XYPad 内に 16×16 グリッド）
    const float cellW = (w - 40.0f) / GRID_SIZE;
    const float cellH = (h - 40.0f) / GRID_SIZE;
    const float gridStartX = 20.0f;
    const float gridStartY = 20.0f;

    // wave=="6" のLFOをすべて描画（recording フラグ不問）
    juce::Colour lfoColors[] = {
        juce::Colour(0xff00D2D3),  // LFO 1
        juce::Colour(0xffFF9FF3),  // LFO 2
        juce::Colour(0xffFEECA1)   // LFO 3
    };

    // ステータス表示用インデックスを決定
    // activeIdx: 実際に録音中のLFO（REC表示）
    // firstWave6: 最初の wave==6 LFO（Ready表示）
    int activeIdx    = -1;
    int firstWave6   = -1;

    for (int lfo = 0; lfo < 3; ++lfo)
    {
        int wave = juce::roundToInt(processor.apvts.getRawParameterValue(
            "lfo" + juce::String(lfo + 1) + "wave")->load());
        if (wave != 6) continue;

        if (firstWave6 < 0) firstWave6 = lfo;
        if (recording[lfo] && activeIdx < 0) activeIdx = lfo;

        // グリッドセル描画（各LFO別色）
        for (int y = 0; y < GRID_SIZE; ++y)
        {
            for (int x = 0; x < GRID_SIZE; ++x)
            {
                float px = gridStartX + x * cellW;
                float py = gridStartY + y * cellH;

                g.setColour(juce::Colour(0xff444444));
                g.drawRect(px, py, cellW, cellH, 0.5f);

                if (pixelMap[lfo][y * GRID_SIZE + x] > 128)
                {
                    g.setColour(lfoColors[lfo].withAlpha(0.8f));
                    g.fillRect(px + 1.0f, py + 1.0f, cellW - 2.0f, cellH - 2.0f);
                }
            }
        }
    }

    // ステータス表示: 録音中の全LFO番号を列挙
    // 例: "REC: LFO 1 2" "Ready: LFO 3"
    {
        const bool isRec = (activeIdx >= 0);
        juce::String lfoNums;
        int maxFrames = 0;

        if (isRec)
        {
            // 実際に録音中のLFO番号を列挙
            for (int lfo = 0; lfo < 3; ++lfo)
            {
                if (recording[lfo])
                {
                    lfoNums += juce::String(lfo + 1) + " ";
                    maxFrames = juce::jmax(maxFrames, recordingLength[lfo]);
                }
            }
        }
        else if (firstWave6 >= 0)
        {
            // 待機中: wave==6 のLFO番号を列挙
            for (int lfo = 0; lfo < 3; ++lfo)
            {
                int wave = juce::roundToInt(processor.apvts.getRawParameterValue(
                    "lfo" + juce::String(lfo + 1) + "wave")->load());
                if (wave == 6) lfoNums += juce::String(lfo + 1) + " ";
            }
        }

        if (lfoNums.isNotEmpty())
        {
            g.setColour(isRec ? juce::Colours::red : juce::Colours::grey);
            g.setFont(14.0f);
            g.drawText(
                (isRec ? "REC: LFO " : "Ready: LFO ") + lfoNums.trim(),
                10, 5, 200, 20, juce::Justification::centredLeft);

            if (isRec)
            {
                g.setColour(juce::Colours::white.withAlpha(0.6f));
                g.setFont(12.0f);
                g.drawText("Frames: " + juce::String(maxFrames),
                    (int)w - 120, 5, 110, 20, juce::Justification::centredRight);
            }
        }
    }
}
