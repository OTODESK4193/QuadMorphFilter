// ==========================================
// UI/XYPadComponent.h
// ==========================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>

// 前方宣言: .cpp で完全なインクルードを行う
class QuadMorphFilterAudioProcessor;

class XYPadComponent : public juce::Component,
    public juce::Timer
{
public:
    explicit XYPadComponent(QuadMorphFilterAudioProcessor& p);

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // 角丸矩形の内側のみをヒット判定エリアとする
    // → 描画枠外（コーナーの円弧外）でのクリックを無効化
    bool hitTest(int x, int y) override;

private:
    void updatePosition(const juce::MouseEvent& e);

    // ===== Recording モード関連 =====
    bool isRecordingMode() const;
    void startRecording();
    void recordPixel(float xNorm, float yNorm);
    void finishRecording();
    void paintRecordingGrid(juce::Graphics& g);

    QuadMorphFilterAudioProcessor& processor;
    int draggingLfoIndex = -1;

    // ===== タブ機能（LFO選択） =====
    int selectedLfoTab = 0;  // 0=LFO1, 1=LFO2, 2=LFO3
    static constexpr float TAB_HEIGHT = 35.0f;
    bool isRecordingNow() const;
    bool hitTestTab(float x, float y, int& tabIndex);
    void drawTabs(juce::Graphics& g);

    std::array<juce::Point<float>, 30> trails[3];
    int trailIdx[3] = { 0, 0, 0 };

    // ===== Recording グリッド状態（各LFOごと独立） =====
    static constexpr int GRID_SIZE = 16;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> pixelMap[3];
    bool recording[3] = { false, false, false };
    int recordingLength[3] = { 0, 0, 0 };
    float lastRecX[3] = { 0.0f, 0.0f, 0.0f };
    float lastRecY[3] = { 0.0f, 0.0f, 0.0f };

    // Q2: 通過ピクセルの順序リスト（ピクセル中心軌道用）
    std::vector<juce::Point<int>> pixelSeq[3];

    // Q1: ラウンドロビン録音ターゲット (0=LFO1, 1=LFO2, 2=LFO3)
    int nextRecordTarget = 0;
};