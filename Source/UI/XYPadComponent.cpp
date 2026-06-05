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
// TAB_HEIGHT を考慮して、タブの下のエリアを 0-1 にマップ
// ────────────────────────────────────────
namespace {
    constexpr float TAB_HEIGHT_NS = 35.0f;  // タブ高さ
    constexpr float MARGIN = 15.0f;         // タブ下の余白

    // ピクセル → 正規化 [0,1] (ラベル中心を 0/1 とする)
    inline juce::Point<float> toNorm(float px, float py, float w, float h)
    {
        const float iL = 20.0f, iR = w - 20.0f;
        const float iT = TAB_HEIGHT_NS + MARGIN;
        const float iB = h - 15.0f;
        return {
            juce::jlimit(0.0f, 1.0f, (px - iL) / (iR - iL)),
            juce::jlimit(0.0f, 1.0f, (py - iT) / (iB - iT))
        };
    }
    // 正規化 [0,1] → ピクセル (描画用)
    inline juce::Point<float> toPix(float nx, float ny, float w, float h)
    {
        const float iL = 20.0f, iR = w - 20.0f;
        const float iT = TAB_HEIGHT_NS + MARGIN;
        const float iB = h - 15.0f;
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


    repaint();
}

// ────────────────────────────────────────
// paint
// ────────────────────────────────────────
void XYPadComponent::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // ===== 背景と枠 =====
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xff444444));
    g.drawRoundedRectangle(b, 8.0f, 1.5f);

    // ===== タブ領域の背景 =====
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(0.0f, 0.0f, w, TAB_HEIGHT);
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(0.0f, TAB_HEIGHT, w, TAB_HEIGHT, 1.0f);

    // ===== タブボタンを描画 =====
    drawTabs(g);

    // コーナーラベル (中心がコーナー 100% 点、タブ下に移動)
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(14.0f);
    g.drawText("A", 10,         (int)TAB_HEIGHT + 10, 20, 20, juce::Justification::centred);
    g.drawText("B", (int)w-30,  (int)TAB_HEIGHT + 10, 20, 20, juce::Justification::centred);
    g.drawText("C", 10,         (int)h-30,             20, 20, juce::Justification::centred);
    g.drawText("D", (int)w-30,  (int)h-30,             20, 20, juce::Justification::centred);

    // ===== Recording グリッド描画（selectedLfoTab のみ） =====
    bool anyRecordingActive = false;
    if (recording[selectedLfoTab])
    {
        int wave = (int)processor.apvts.getRawParameterValue(
            "lfo" + juce::String(selectedLfoTab + 1) + "wave")->load();
        if (wave == 6)
        {
            anyRecordingActive = true;
        }
    }

    if (anyRecordingActive)
    {
        paintRecordingGrid(g);
        return;  // Recording モード時はグリッド表示のみ
    }

    // ===== 非Recording時：選択タブのLFOのみ表示 =====
    juce::Colour colors[] = {
        juce::Colour(0xff00bcd4),
        juce::Colour(0xffff66dd),
        juce::Colour(0xffff9900)
    };

    const int i = selectedLfoTab;

    if (processor.apvts.getRawParameterValue(
        "lfo" + juce::String(i + 1) + "en")->load() >= 0.5f)
    {
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
    // ===== タブクリック判定 =====
    int tabIndex = -1;
    if (e.y < TAB_HEIGHT && hitTestTab((float)e.x, (float)e.y, tabIndex))
    {
        // Recording 中は他のタブに切り替えられない
        if (isRecordingNow())
            return;

        selectedLfoTab = tabIndex;
        repaint();
        return;
    }

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

        // ピクセル記録（正しく正規化座標を使用）
        recordPixel(n.x, n.y);
    }

    repaint();
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto n = toNorm((float)e.x, (float)e.y,
                    (float)getWidth(), (float)getHeight());

    recordPixel(n.x, n.y);

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
// Recording 状態を確認
// ────────────────────────────────────────
bool XYPadComponent::isRecordingNow() const
{
    return recording[0] || recording[1] || recording[2];
}

// ────────────────────────────────────────
// タブクリック判定
// ────────────────────────────────────────
bool XYPadComponent::hitTestTab(float x, float y, int& tabIndex)
{
    const float w = (float)getWidth();
    const float tabW = w / 3.0f;

    if (x < 0.0f || x >= w || y < 0.0f || y >= TAB_HEIGHT)
        return false;

    tabIndex = (int)(x / tabW);
    return tabIndex >= 0 && tabIndex < 3;
}

// ────────────────────────────────────────
// タブボタンを描画
// ────────────────────────────────────────
void XYPadComponent::drawTabs(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float tabW = w / 3.0f;

    for (int i = 0; i < 3; ++i)
    {
        float px = i * tabW;
        bool isSelected = (i == selectedLfoTab);
        bool isRecording = recording[i];

        // タブ背景
        if (isSelected)
        {
            g.setColour(juce::Colour(0xff2a2a2a));  // 選択中は明るく
        }
        else if (isRecordingNow() && !isRecording)
        {
            g.setColour(juce::Colour(0xff0d0d0d).withAlpha(0.5f));  // 他は暗くグレーアウト
        }
        else
        {
            g.setColour(juce::Colour(0xff1a1a1a));  // 通常
        }
        g.fillRect(px, 0.0f, tabW, TAB_HEIGHT);

        // タブ境界
        g.setColour(juce::Colour(0xff444444));
        if (i < 2) g.drawLine(px + tabW, 0.0f, px + tabW, TAB_HEIGHT, 1.0f);

        // タブテキスト
        juce::String tabLabel = "LFO " + juce::String(i + 1);
        if (isRecording)
            tabLabel += " REC";

        juce::Colour textColor = juce::Colours::white;
        if (isRecordingNow() && !isRecording)
            textColor = juce::Colours::grey.withAlpha(0.4f);
        else if (isRecording)
            textColor = juce::Colours::red;

        g.setColour(textColor);
        g.setFont(13.0f);
        g.drawText(tabLabel, (int)px, 0, (int)tabW, (int)TAB_HEIGHT,
                   juce::Justification::centred);

        // 選択中のタブに下線
        if (isSelected)
        {
            g.setColour(juce::Colours::cyan);
            g.fillRect(px, TAB_HEIGHT - 2.0f, tabW, 2.0f);
        }
    }
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
// Recording 開始（selectedLfoTab のLFOのみ）
// ────────────────────────────────────────
void XYPadComponent::startRecording()
{
    // 既に録音中なら何もしない
    if (recording[selectedLfoTab]) return;

    // selectedLfoTab のLFOが wave == 6 かをチェック
    int wave = juce::roundToInt(processor.apvts.getRawParameterValue(
        "lfo" + juce::String(selectedLfoTab + 1) + "wave")->load());

    if (wave != 6)
        return;  // wave == 6 ではない場合は Recording しない

    // 選択タブのLFOのみを Recording 対象にする
    int idx = selectedLfoTab;
    recording[idx] = true;
    recordingLength[idx] = 0;
    pixelSeq[idx].clear();
    std::fill(pixelMap[idx].begin(), pixelMap[idx].end(), 0);
    processor.recLength[idx].store(0);
    processor.isWaitingForRecord[idx].store(true,  std::memory_order_release);
    processor.isRecordingDrag[idx].store   (false, std::memory_order_release);
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
// Recording グリッド描画（selectedLfoTab のみ）
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

    // タブ領域を描画（paintRecordingGrid内でも必要）
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(0.0f, 0.0f, w, TAB_HEIGHT);
    g.setColour(juce::Colour(0xff333333));
    g.drawLine(0.0f, TAB_HEIGHT, w, TAB_HEIGHT, 1.0f);
    drawTabs(g);

    // グリッド配置（XYPad 内に 16×16 グリッド、タブの下から十分な余白を作る）
    const float gridStartY = TAB_HEIGHT + 15.0f;  // 余白を広げた
    const float gridHeight = h - gridStartY - 15.0f;
    const float cellW = (w - 40.0f) / GRID_SIZE;
    const float cellH = gridHeight / GRID_SIZE;
    const float gridStartX = 20.0f;

    // selectedLfoTab のLFOのグリッドのみ描画
    juce::Colour lfoColors[] = {
        juce::Colour(0xff00D2D3),  // LFO 1
        juce::Colour(0xffFF9FF3),  // LFO 2
        juce::Colour(0xffFEECA1)   // LFO 3
    };

    const int lfo = selectedLfoTab;

    // グリッドセル描画
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

    // ステータス表示は削除（タブで既に表示されているため不要）
}
