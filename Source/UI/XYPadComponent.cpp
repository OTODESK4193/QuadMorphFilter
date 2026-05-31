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

        // ※ lfoCutSrc/lfoResSrc は AudioParameterChoice (index 0=Off,1〜4=変調有効)
        //    getRawParameterValue は index をそのまま float で返す → > 0 で ON 判定
        const juce::String suffixes[4] = { "A", "B", "C", "D" };
        for (const auto& s : suffixes)
        {
            const int cutSrc = juce::roundToInt(
                processor.apvts.getRawParameterValue("lfoCutSrc" + s)->load());
            if (cutSrc > 0)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    processor.apvts.getParameter("cutoff" + s)))
                {
                    if (std::abs(p->get() - xyCutoff) > 0.5f)
                        *p = juce::jlimit(p->range.start, p->range.end, xyCutoff);
                }
            }
            const int resSrc = juce::roundToInt(
                processor.apvts.getRawParameterValue("lfoResSrc" + s)->load());
            if (resSrc > 0)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    processor.apvts.getParameter("res" + s)))
                {
                    if (std::abs(p->get() - xyRes) > 0.01f)
                        *p = juce::jlimit(p->range.start, p->range.end, xyRes);
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
    g.setColour(juce::Colour(0xff1E272E));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
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
        juce::Colour(0xff00D2D3),
        juce::Colour(0xffFF9FF3),
        juce::Colour(0xffFEECA1)
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

    // ===== 愚直な方法: LFO1 / LFO2 / LFO3 を個別に処理 =====
    // wave==6 のLFO は全て同時に録音開始（LFO番号に関わらず同じロジック）

    // LFO 1
    if (juce::roundToInt(processor.apvts.getRawParameterValue("lfo1wave")->load()) == 6)
    {
        recording[0] = true;
        recordingLength[0] = 0;
        std::fill(pixelMap[0].begin(), pixelMap[0].end(), 0);
        processor.recLength[0].store(0);
        processor.isWaitingForRecord[0].store(true,  std::memory_order_release);
        processor.isRecordingDrag[0].store   (false, std::memory_order_release);
    }

    // LFO 2
    if (juce::roundToInt(processor.apvts.getRawParameterValue("lfo2wave")->load()) == 6)
    {
        recording[1] = true;
        recordingLength[1] = 0;
        std::fill(pixelMap[1].begin(), pixelMap[1].end(), 0);
        processor.recLength[1].store(0);
        processor.isWaitingForRecord[1].store(true,  std::memory_order_release);
        processor.isRecordingDrag[1].store   (false, std::memory_order_release);
    }

    // LFO 3
    if (juce::roundToInt(processor.apvts.getRawParameterValue("lfo3wave")->load()) == 6)
    {
        recording[2] = true;
        recordingLength[2] = 0;
        std::fill(pixelMap[2].begin(), pixelMap[2].end(), 0);
        processor.recLength[2].store(0);
        processor.isWaitingForRecord[2].store(true,  std::memory_order_release);
        processor.isRecordingDrag[2].store   (false, std::memory_order_release);
    }
}

// ────────────────────────────────────────
// ピクセル記録（正規化座標）→ 各LFOの pixelMap に記録
// ────────────────────────────────────────
void XYPadComponent::recordPixel(float xNorm, float yNorm)
{
    // 全 Recording 中のLFOにピクセルを記録
    for (int i = 0; i < 3; ++i)
    {
        if (!recording[i]) continue;

        // グリッドサイズに正規化（0～1 → 0～15）
        int gx = (int)(xNorm * GRID_SIZE);
        int gy = (int)(yNorm * GRID_SIZE);

        gx = juce::jlimit(0, GRID_SIZE - 1, gx);
        gy = juce::jlimit(0, GRID_SIZE - 1, gy);

        int idx = gy * GRID_SIZE + gx;
        pixelMap[i][idx] = 255;
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

        // recBuffer から recordingData に直接コピー
        std::array<juce::Point<float>, 2048> buffer;
        int len = processor.recLength[i].load();

        if (len > 0 && len <= 2048)
        {
            std::copy(processor.recBuffer[i].begin(),
                      processor.recBuffer[i].begin() + len,
                      buffer.begin());
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

    // ステータス表示: 録音中は activeIdx、待機中は firstWave6 を使用
    const int displayIdx = (activeIdx >= 0) ? activeIdx : firstWave6;
    if (displayIdx >= 0)
    {
        const bool isRec = (activeIdx >= 0);
        g.setColour(isRec ? juce::Colours::red : juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText(
            (isRec ? "REC: " : "Ready: ") + juce::String("LFO ") + juce::String(displayIdx + 1),
            10, 5, 200, 20, juce::Justification::centredLeft);

        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(12.0f);
        g.drawText(
            "Frames: " + juce::String(recordingLength[displayIdx]),
            (int)w - 120, 5, 110, 20, juce::Justification::centredRight);
    }
}
