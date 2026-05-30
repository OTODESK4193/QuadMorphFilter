// ==========================================
// UI/XYGridComponent.h
// 16×16 ピクセルグリッド描画コンポーネント
// Recording LFO 用
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// 前方宣言
class QuadMorphFilterAudioProcessor;

class XYGridComponent : public juce::Component,
                       public juce::Timer
{
public:
    explicit XYGridComponent(QuadMorphFilterAudioProcessor& p);
    ~XYGridComponent() override;

    // ===== Component オーバーライド =====
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void timerCallback() override;

    // ===== Grid 制御 =====
    void clear();
    void getRecordingData(std::array<juce::Point<float>, 2048>& buffer, int& len);
    int getRecordingLength() const { return recordingLength; }

    // ===== Status =====
    bool isRecording() const { return recording; }

private:
    // ===== Grid パラメータ =====
    static constexpr int GRID_SIZE = 16;
    static constexpr int CELL_SIZE = 20;  // ピクセル
    static constexpr int GRID_PIXEL_WIDTH = GRID_SIZE * CELL_SIZE;   // 320px
    static constexpr int GRID_PIXEL_HEIGHT = GRID_SIZE * CELL_SIZE;  // 320px

    // ===== State =====
    uint8_t pixelMap[GRID_SIZE * GRID_SIZE] = {};
    bool recording = false;
    int recordingLength = 0;
    int lastFrameIndex = -1;  // 最後に記録したフレーム

    QuadMorphFilterAudioProcessor& processor;

    // ===== Helper =====
    void paintGridCell(juce::Graphics& g, int gridX, int gridY);
    void recordPixel(int gridX, int gridY);
    std::pair<int, int> mouseToGridCoord(const juce::MouseEvent& e) const;
};
