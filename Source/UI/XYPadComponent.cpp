// ==========================================
// UI/XYPadComponent.cpp
// ==========================================
#include "XYPadComponent.h"
#include "../PluginProcessor.h"

XYPadComponent::XYPadComponent(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    for (int i = 0; i < 3; ++i)
        trails[i].fill({ 0.5f, 0.5f });
    startTimerHz(60);
}

void XYPadComponent::timerCallback()
{
    for (int i = 0; i < 3; ++i) {
        trails[i][trailIdx[i]] = processor.getLfoPos(i);
        trailIdx[i] = (trailIdx[i] + 1) % 30;
    }

    // ===== Cutoffモード: XY位置をスライダーに書き戻し (UI同期) =====
    // lfoCutOn=true のフィルターのカットオフスライダーを
    // XY位置由来の値に追従させる（表示の同期）
    int xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();
    if (xyMode == 1)
    {
        auto mPos = processor.getLfoPos(0);
        const int cutoffAlgo = (int)processor.apvts.getRawParameterValue("cutoffAlgo")->load();

        float xyCutoff, xyRes;
        if (cutoffAlgo == 1)
        {
            // 案II: 相対モード (processBlock と同一計算)
            const float devX = (mPos.x - 0.5f) * 2.0f;
            const float devY = (0.5f - mPos.y) * 2.0f;
            xyCutoff = 632.0f * std::pow(2.0f, devX * 4.0f);
            xyRes    = 1.0f   * std::pow(2.0f, devY * 2.0f);
        }
        else if (cutoffAlgo == 2)
        {
            // 案III: ゾーン非対称
            const float devX = (mPos.x - 0.5f) * 2.0f;
            const float devY = (0.5f - mPos.y) * 2.0f;
            xyCutoff = 632.0f * std::pow(2.0f, (devX >= 0.0f ? devX * 5.0f : devX * 3.0f));
            xyRes    = 1.0f   * std::pow(2.0f, (devY >= 0.0f ? devY * 3.0f : devY * 2.0f));
        }
        else
        {
            // 案I: 絶対モード
            xyCutoff = 20.0f * std::pow(1000.0f, mPos.x);
            xyRes    = 0.1f + (1.0f - mPos.y) * 9.9f;
        }
        xyCutoff = juce::jlimit(20.0f, 20000.0f, xyCutoff);
        xyRes    = juce::jlimit(0.1f,  10.0f,    xyRes);

        const juce::String suffixes[4] = { "A", "B", "C", "D" };
        for (const auto& s : suffixes)
        {
            // lfoCutOn の場合、カットオフスライダーを XY X位置に同期
            if (processor.apvts.getRawParameterValue("lfoCut" + s)->load() > 0.5f)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(
                    processor.apvts.getParameter("cutoff" + s)))
                {
                    // 現在値との差が小さければスキップ（CPU節約）
                    if (std::abs(p->get() - xyCutoff) > 0.5f)
                        *p = juce::jlimit(p->range.start, p->range.end, xyCutoff);
                }
            }

            // lfoResOn の場合、Resスライダーを XY Y位置に同期
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

void XYPadComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1E272E));
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(juce::Colour(0xffD5DDE5));
    g.drawRoundedRectangle(b, 8.0f, 1.5f);

    int xyMode = (int)processor.apvts.getRawParameterValue("xyMode")->load();

    // コーナーラベル
    g.setColour(juce::Colours::white.withAlpha(xyMode == 1 ? 0.1f : 0.3f));
    g.setFont(14.0f);
    g.drawText("A", 10, 10, 20, 20, juce::Justification::centred);
    g.drawText("B", getWidth() - 30, 10, 20, 20, juce::Justification::centred);
    g.drawText("C", 10, getHeight() - 30, 20, 20, juce::Justification::centred);
    g.drawText("D", getWidth() - 30, getHeight() - 30, 20, 20, juce::Justification::centred);

    // Cutoffモード: 軸ラベル
    if (xyMode == 1)
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.setFont(10.0f);
        g.drawText("Cutoff ->",
            getWidth() / 2 - 35, getHeight() - 15, 70, 12,
            juce::Justification::centred);
        g.drawText("^ Reso",
            3, getHeight() / 2 - 25, 40, 12,
            juce::Justification::centredLeft);
    }

    // LFO トレイルと現在位置
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

        for (int t = 0; t < 30; ++t) {
            int   idx = (trailIdx[i] + t) % 30;
            auto  pt = trails[i][idx];
            float alpha = (float)t / 30.0f * 0.5f;
            g.setColour(colors[i].withAlpha(alpha));
            g.fillEllipse(pt.x * getWidth() - 3, pt.y * getHeight() - 3, 6, 6);
        }

        auto p = processor.getLfoPos(i);
        bool isWait = processor.isWaitingForRecord[i].load(std::memory_order_acquire);
        bool isDrag = processor.isRecordingDrag[i].load(std::memory_order_acquire);

        if (isWait) {
            if (isDrag) {
                g.setColour(colors[i].brighter(0.8f));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
            else {
                float alpha = 0.3f + 0.7f * std::abs(
                    std::sin(juce::Time::getMillisecondCounter() * 0.005f));
                g.setColour(colors[i].withAlpha(alpha));
                g.fillEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16);
            }
        }
        else {
            g.setColour(colors[i]);
            g.fillEllipse(p.x * getWidth() - 6, p.y * getHeight() - 6, 12, 12);
        }
        g.setColour(juce::Colours::white);
        g.drawEllipse(p.x * getWidth() - 8, p.y * getHeight() - 8, 16, 16, 1.0f);
    }
}

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
        float ww = (float)getWidth();
        float hh = (float)getHeight();
        for (int i = 0; i < 3; ++i) {
            int wave = (int)processor.apvts.getRawParameterValue(
                "lfo" + juce::String(i + 1) + "wave")->load();
            if (wave == 6) {
                auto  p = processor.getLfoPos(i);
                float px = p.x * ww, py = p.y * hh;
                if (std::hypot(e.x - px, e.y - py) < 15.0f) {
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
                float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
                float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
                processor.currentRecX[i].store(nx);
                processor.currentRecY[i].store(ny);
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
            float nx = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
            float ny = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
            processor.recBuffer[draggingLfoIndex][len] = { nx, ny };
            processor.recLength[draggingLfoIndex].store(len + 1);
            processor.currentRecX[draggingLfoIndex].store(nx);
            processor.currentRecY[draggingLfoIndex].store(ny);
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
    // → 描画枠の角丸外側でのクリックを受け付けない
    juce::Path p;
    p.addRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
    return p.contains((float)x, (float)y);
}

void XYPadComponent::updatePosition(const juce::MouseEvent& e)
{
    // jlimit により枠外ドラッグは枠端に吸着する
    // （マウスが枠外に出てもドットは枠の辺上に留まる）
    float x = juce::jlimit(0.0f, 1.0f, (float)e.x / getWidth());
    float y = juce::jlimit(0.0f, 1.0f, (float)e.y / getHeight());
    processor.apvts.getParameter("posX")->setValueNotifyingHost(x);
    processor.apvts.getParameter("posY")->setValueNotifyingHost(y);
}