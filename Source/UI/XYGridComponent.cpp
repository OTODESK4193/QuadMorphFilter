// ==========================================
// UI/XYGridComponent.cpp
// 16×16 ピクセルグリッド描画実装
// ==========================================
#include "XYGridComponent.h"
#include "../PluginProcessor.h"

XYGridComponent::XYGridComponent(QuadMorphFilterAudioProcessor& p)
    : processor(p)
{
    setSize(GRID_PIXEL_WIDTH, GRID_PIXEL_HEIGHT);
    startTimer(60);  // 60Hz 更新
}

XYGridComponent::~XYGridComponent()
{
    stopTimer();
}

// ==========================================
// paint
// ==========================================
void XYGridComponent::paint(juce::Graphics& g)
{
    // 背景
    g.setColour(juce::Colour(0xff1E272E));
    g.fillRect(getLocalBounds());

    // グリッド線
    g.setColour(juce::Colour(0xff444444));
    for (int i = 0; i <= GRID_SIZE; ++i) {
        int pos = i * CELL_SIZE;
        g.drawLine((float)pos, 0.0f, (float)pos, (float)GRID_PIXEL_HEIGHT, 1.0f);
        g.drawLine(0.0f, (float)pos, (float)GRID_PIXEL_WIDTH, (float)pos, 1.0f);
    }

    // ピクセルセル描画
    for (int y = 0; y < GRID_SIZE; ++y) {
        for (int x = 0; x < GRID_SIZE; ++x) {
            if (pixelMap[y * GRID_SIZE + x] > 128) {
                // 塗りつぶし
                int px = x * CELL_SIZE;
                int py = y * CELL_SIZE;
                g.setColour(juce::Colour(0xff00FF00).withAlpha(0.8f));
                g.fillRect(px + 1, py + 1, CELL_SIZE - 2, CELL_SIZE - 2);
            }
        }
    }

    // Recording ステータス表示
    g.setColour(recording ? juce::Colours::red : juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText(
        recording ? "REC (Recording...)" : "Ready to record",
        10, 5, 200, 20,
        juce::Justification::centredLeft
    );

    // フレーム数表示
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(10.0f);
    g.drawText(
        "Frames: " + juce::String(recordingLength),
        GRID_PIXEL_WIDTH - 120, 5, 110, 20,
        juce::Justification::centredRight
    );
}

// ==========================================
// mouseDown
// ==========================================
void XYGridComponent::mouseDown(const juce::MouseEvent& e)
{
    recording = true;
    recordingLength = 0;
    lastFrameIndex = -1;
    std::fill(std::begin(pixelMap), std::end(pixelMap), 0);

    recordPixel(e.x, e.y);
    repaint();
}

// ==========================================
// mouseDrag
// ==========================================
void XYGridComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!recording) return;

    auto [gx, gy] = mouseToGridCoord(e);
    if (gx >= 0 && gx < GRID_SIZE && gy >= 0 && gy < GRID_SIZE) {
        recordPixel(gx, gy);
    }

    repaint();
}

// ==========================================
// mouseUp
// ==========================================
void XYGridComponent::mouseUp(const juce::MouseEvent& e)
{
    recording = false;

    // 記録データを取得
    std::array<juce::Point<float>, 2048> buffer;
    int len = 0;
    getRecordingData(buffer, len);

    // PluginProcessor 経由で LfoEngine に反映
    // ※ 注意: XYGridComponent を複数の LFO に対応させる場合は、
    //    lfoIndex を引数で受け取る必要があります。
    //    現在は固定で LFO 1 (Morph) に対応。
    processor.setLfoRecordingData(0, buffer, len);  // LFO 1 (Morph)

    repaint();
}

// ==========================================
// timerCallback
// ==========================================
void XYGridComponent::timerCallback()
{
    // 毎60Hz で UI 更新（フレーム数表示の更新等）
    repaint();
}

// ==========================================
// clear
// ==========================================
void XYGridComponent::clear()
{
    std::fill(std::begin(pixelMap), std::end(pixelMap), 0);
    recordingLength = 0;
    lastFrameIndex = -1;
    recording = false;
    repaint();
}

// ==========================================
// getRecordingData
// ==========================================
void XYGridComponent::getRecordingData(
    std::array<juce::Point<float>, 2048>& buffer,
    int& len)
{
    len = 0;

    // 左上から右下へスキャン → バッファに記録
    // デフォルト: Z-order (行優先)
    for (int y = 0; y < GRID_SIZE && len < 2048; ++y) {
        for (int x = 0; x < GRID_SIZE && len < 2048; ++x) {
            if (pixelMap[y * GRID_SIZE + x] > 128) {
                buffer[len++] = {
                    x / (GRID_SIZE - 1.0f),  // [0, 1]
                    y / (GRID_SIZE - 1.0f)
                };
            }
        }
    }

    recordingLength = len;
}

// ==========================================
// Helper: recordPixel
// ==========================================
void XYGridComponent::recordPixel(int gridX, int gridY)
{
    if (gridX < 0 || gridX >= GRID_SIZE || gridY < 0 || gridY >= GRID_SIZE) {
        return;
    }

    int idx = gridY * GRID_SIZE + gridX;
    pixelMap[idx] = 255;  // 塗りつぶし

    // フレーム記録（重複チェック）
    if (lastFrameIndex != idx) {
        lastFrameIndex = idx;
        // フレーム数はカウント（重複なし）
    }
}

// ==========================================
// Helper: mouseToGridCoord
// ==========================================
std::pair<int, int> XYGridComponent::mouseToGridCoord(const juce::MouseEvent& e) const
{
    int gx = e.x / CELL_SIZE;
    int gy = e.y / CELL_SIZE;
    return { gx, gy };
}
