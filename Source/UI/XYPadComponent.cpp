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
    if (e.mods.isRightButtonDown())
    {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                processor.isWaitingForRecord[i].store(false);
                processor.isRecordingDrag[i].store(false);
                draggingLfoIndex = -1;
                return;
            }
        }
        const float ww = (float)getWidth();
        const float hh = (float)getHeight();
        for (int i = 0; i < 3; ++i) {
            int wave = (int)processor.apvts.getRawParameterValue(
                "lfo" + juce::String(i + 1) + "wave")->load();
            if (wave == 6) {
                auto  p  = processor.getLfoPos(i);
                auto  px = toPix(p.x, p.y, ww, hh);
                if (std::hypot(e.x - px.x, e.y - px.y) < 15.0f) {
                    processor.recLength[i].store(0);
                    processor.isWaitingForRecord[i].store(true);
                    return;
                }
            }
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i) {
            if (processor.isWaitingForRecord[i].load()) {
                draggingLfoIndex = i;
                processor.isRecordingDrag[i].store(true);
                auto n = toNorm((float)e.x, (float)e.y,
                                (float)getWidth(), (float)getHeight());
                processor.currentRecX[i].store(n.x);
                processor.currentRecY[i].store(n.y);
                return;
            }
        }
    }
    updatePosition(e);
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingLfoIndex != -1 && processor.isRecordingDrag[draggingLfoIndex].load())
    {
        int len = processor.recLength[draggingLfoIndex].load();
        if (len < 2048) {
            auto n = toNorm((float)e.x, (float)e.y,
                            (float)getWidth(), (float)getHeight());
            processor.recBuffer[draggingLfoIndex][len] = { n.x, n.y };
            processor.recLength[draggingLfoIndex].store(len + 1);
            processor.currentRecX[draggingLfoIndex].store(n.x);
            processor.currentRecY[draggingLfoIndex].store(n.y);
        }
        return;
    }
    updatePosition(e);
}

void XYPadComponent::mouseUp(const juce::MouseEvent&)
{
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
