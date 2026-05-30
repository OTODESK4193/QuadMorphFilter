// ==========================================
// DSP/LfoEngine.h
// LFO状態管理・波形計算エンジン
//
// 責務:
//   - 3系統のLFO状態を保持・更新
//   - 19種類の波形を計算
//   - Boundary処理 (Clip / Bounce / Wrap)
//   - Recordingバッファの読み出し
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class LfoEngine
{
public:
    // ===== 録音状態をまとめた構造体 =====
    // PluginProcessor の public メンバへの参照をまとめて渡すための構造体
    // XYPadComponent からも参照される
    struct RecordingContext
    {
        std::array<juce::Point<float>, 2048>* buffers;   // recBuffer[3]
        std::atomic<int>* lengths;                      // recLength[3]
        std::atomic<bool>* isWaiting;                    // isWaitingForRecord[3]
        std::atomic<bool>* isDragging;                   // isRecordingDrag[3]
        std::atomic<float>* currentX;                     // currentRecX[3]
        std::atomic<float>* currentY;                     // currentRecY[3]
    };

    LfoEngine();

    void prepare(double newSampleRate);

    // ===== メイン処理 =====
    // processBlock から毎ブロック呼び出す
    void process(float dt,
        double bpm,
        float baseX,
        float baseY,
        juce::AudioProcessorValueTreeState& apvts,
        const RecordingContext& rec);

    // ===== 出力アクセサ =====
    juce::Point<float>   getPosition(int index) const;
    std::array<float, 4> getMod4(int index) const;

    // ===== Recording データセッター =====
    // XYGridComponent から呼び出される
    void setRecordingData(int index, const std::array<juce::Point<float>, 2048>& buffer, int len)
    {
        if (index < 0 || index >= 3) return;
        recordingData[index] = buffer;
        recordingLength[index] = len;
    }

private:
    // ===== LFO内部状態 =====
    struct LfoState
    {
        juce::Random rng;
        float phase = 0.0f;

        juce::Point<float>   currentRandom{ 0.5f, 0.5f };
        juce::Point<float>   nextRandom{ 0.5f, 0.5f };
        std::array<float, 4> currentRand1{ 0.5f, 0.5f, 0.5f, 0.5f };
        std::array<float, 4> nextRand1{ 0.5f, 0.5f, 0.5f, 0.5f };

        float bilX = 0.0f, bilY = 0.0f;
        float bilVx = 1.3f, bilVy = 1.7f;

        float smoothNx = 0.5f, smoothNy = 0.5f;
        float tNextNx = 0.5f, tNextNy = 0.5f;
    };

    double sampleRate = 48000.0;

    LfoState             states[3];
    juce::Point<float>   positions[3];
    std::array<float, 4> mod4[3];

    // ===== Recording バッファ =====
    std::array<juce::Point<float>, 2048> recordingData[3];
    int recordingLength[3] = { 0, 0, 0 };

    // ===== ヘルパー =====
    static float getSyncTime(int selection, double bpm);
    static float applyBound(float v, int mode);

    // 1つのLFOを処理する内部メソッド
    void processSingleLfo(int index,
        float dt,
        double bpm,
        float baseX,
        float baseY,
        juce::AudioProcessorValueTreeState& apvts,
        const RecordingContext& rec);
};