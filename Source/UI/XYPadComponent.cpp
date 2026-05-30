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
    // Recording モード check：wave が 6 でなくなったら recording フラグをリセット
    if (recording && recordingLfoIndex >= 0)
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(recordingLfoIndex + 1) + "wave")->load();
        if (wave != 6)
        {
            recording = false;
            recordingLfoIndex = -1;
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

        const juce::String suffixes[4] = { "A", "B", "C", "D" };
        for (const auto& s : suffixes)
        {
            if (processor.apvts.getRawParameterValue("lfoCut" + s)->load() > 0.5f)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    processor.apvts.getParameter("cutoff" + s)))
                {
                    if (std::abs(p->get() - xyCutoff) > 0.5f)
                        *p = juce::jlimit(p->range.start, p->range.end, xyCutoff);
                }
            }
            if (processor.apvts.getRawParameterValue("lfoRes" + s)->load() > 0.5f)
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

    // ===== Recording グリッド描画（現在Recording中のLFOのみ） =====
    if (recordingLfoIndex >= 0 && recording)
    {
        paintRecordingGrid(g);
        return;  // Recording 中はグリッド表示のみ、トレイル非表示
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
    // Recording 中なら新規 Recording を拒否
    if (recording && recordingLfoIndex >= 0) {
        updatePosition(e);
        return;
    }

    // Recording モード時：グリッド Recording を開始
    for (int i = 0; i < 3; ++i)
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "wave")->load();
        if (wave == 6)  // Recording モード
        {
            recordingLfoIndex = i;
            recording = true;
            recordingLength = 0;
            std::fill(pixelMap.begin(), pixelMap.end(), 0);

            // recBuffer もクリア（既存ロジックとの連携用）
            processor.recLength[i].store(0);

            recordPixel((float)e.x / getWidth(), (float)e.y / getHeight());

            // recBuffer にも記録開始
            auto n = toNorm((float)e.x, (float)e.y,
                            (float)getWidth(), (float)getHeight());
            processor.recBuffer[i][0] = { n.x, n.y };
            processor.recLength[i].store(1);
            processor.currentRecX[i].store(n.x);
            processor.currentRecY[i].store(n.y);

            // 補間用の前回位置を初期化
            lastRecX = n.x;
            lastRecY = n.y;

            repaint();
            return;
        }
    }

    // 通常の XYPad ドラッグ
    updatePosition(e);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (recordingLfoIndex >= 0 && recording)
    {
        auto n = toNorm((float)e.x, (float)e.y,
                        (float)getWidth(), (float)getHeight());

        recordPixel((float)e.x / getWidth(), (float)e.y / getHeight());

        // recBuffer に線形補間でポイントを複数記録（滑らかさ向上）
        int len = processor.recLength[recordingLfoIndex].load();
        if (len < 2048)
        {
            // 前回位置から現在位置への距離を計算
            float dx = n.x - lastRecX;
            float dy = n.y - lastRecY;
            float dist = std::hypot(dx, dy);

            // 距離に応じて補間ポイント数を決定（最大5ポイント）
            int interpolationSteps = (int)std::min(5.0f, std::max(1.0f, dist * 10.0f));

            for (int step = 1; step <= interpolationSteps && len < 2048; ++step)
            {
                float t = (float)step / interpolationSteps;
                float px = lastRecX + dx * t;
                float py = lastRecY + dy * t;

                processor.recBuffer[recordingLfoIndex][len] = { px, py };
                len++;
            }

            processor.recLength[recordingLfoIndex].store(len);
            processor.currentRecX[recordingLfoIndex].store(n.x);
            processor.currentRecY[recordingLfoIndex].store(n.y);

            lastRecX = n.x;
            lastRecY = n.y;
        }

        repaint();
        return;
    }

    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent&)
{
    if (recordingLfoIndex >= 0 && recording)
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
// Recording 開始
// ────────────────────────────────────────
void XYPadComponent::startRecording()
{
    // 現在 Recording モードの LFO を特定
    for (int i = 0; i < 3; ++i)
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(i + 1) + "wave")->load();
        if (wave == 6)
        {
            recordingLfoIndex = i;
            recording = true;
            recordingLength = 0;
            std::fill(pixelMap.begin(), pixelMap.end(), 0);
            return;
        }
    }
}

// ────────────────────────────────────────
// ピクセル記録（正規化座標）
// ────────────────────────────────────────
void XYPadComponent::recordPixel(float xNorm, float yNorm)
{
    if (!recording || recordingLfoIndex < 0) return;

    // グリッドサイズに正規化（0～1 → 0～15）
    int gx = (int)(xNorm * GRID_SIZE);
    int gy = (int)(yNorm * GRID_SIZE);  // Y軸反転なし（finishRecording で反転）

    gx = juce::jlimit(0, GRID_SIZE - 1, gx);
    gy = juce::jlimit(0, GRID_SIZE - 1, gy);

    int idx = gy * GRID_SIZE + gx;
    pixelMap[idx] = 255;
}

// ────────────────────────────────────────
// Recording 完了・データ反映
// ────────────────────────────────────────
void XYPadComponent::finishRecording()
{
    if (!recording || recordingLfoIndex < 0) return;

    recording = false;

    // recBuffer から recordingData に直接コピー
    // recBuffer にはマウスドラッグで記録した連続的なポイントが格納されている
    std::array<juce::Point<float>, 2048> buffer;
    int len = processor.recLength[recordingLfoIndex].load();

    if (len > 0 && len <= 2048)
    {
        std::copy(processor.recBuffer[recordingLfoIndex].begin(),
                  processor.recBuffer[recordingLfoIndex].begin() + len,
                  buffer.begin());
    }

    recordingLength = len;

    // LfoEngine に Recording データを反映
    processor.setLfoRecordingData(recordingLfoIndex, buffer, len);
    recordingLfoIndex = -1;
}

// ────────────────────────────────────────
// Recording グリッド描画
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

    // グリッドセル描画
    for (int y = 0; y < GRID_SIZE; ++y)
    {
        for (int x = 0; x < GRID_SIZE; ++x)
        {
            float px = gridStartX + x * cellW;
            float py = gridStartY + y * cellH;

            // グリッド線
            g.setColour(juce::Colour(0xff444444));
            g.drawRect(px, py, cellW, cellH, 0.5f);

            // ピクセル塗りつぶし
            if (pixelMap[y * GRID_SIZE + x] > 128)
            {
                g.setColour(juce::Colour(0xff00FF00).withAlpha(0.8f));
                g.fillRect(px + 1.0f, py + 1.0f, cellW - 2.0f, cellH - 2.0f);
            }
        }
    }

    // Recording ステータス表示
    g.setColour(recording ? juce::Colours::red : juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText(
        recording ? "REC (Recording...)" : "Ready to record",
        10, 5, 200, 20,
        juce::Justification::centredLeft);

    // フレーム数表示
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(12.0f);
    g.drawText(
        "Frames: " + juce::String(recordingLength),
        (int)w - 120, 5, 110, 20,
        juce::Justification::centredRight);
}
