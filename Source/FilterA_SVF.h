// ==========================================
// FilterA_SVF.h (Phase 1c: Newton-Raphson 高Q値対応版)
// Zero-Delay Feedback State Variable Filter
// Based on Andy Simper's Linear Trapezoidal Integrator
//
// Phase 1b からの変更点:
//   - Q >= 1.5 で非線形モード（Newton-Raphson）に自動切り替え
//   - tanh フィードバックによる自然な飽和特性
//   - 高Q値でも安定した自己発振
// ==========================================

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <array>

class FilterA_SVF
{
public:
    FilterA_SVF();
    ~FilterA_SVF() = default;

    void prepare(double sampleRate, int blockSize);
    void reset();

    // ===== パラメータ設定（外部APIは Phase 1b と同じ）=====
    void setCutoff(float newCutoffHz);
    void setResonance(float newQ);
    void setType(int type);

    // ===== 処理 =====
    void process(juce::AudioBuffer<float>& buffer);

    // ===== モニタリング用 =====
    float getLastLowpassOutput(int channel = 0)  const { return lastLp[channel]; }
    float getLastBandpassOutput(int channel = 0) const { return lastBp[channel]; }
    float getLastHighpassOutput(int channel = 0) const { return lastHp[channel]; }

    // ===== デバッグ用: 現在のモードを確認 =====
    bool isNonlinearActive() const { return nonlinearMode; }

private:
    // ===== 定数 =====
    // サブブロックサイズ: 16サンプルに1回 tan() を計算
    static constexpr int subBlockSize = 16;

    // ステレオ対応
    static constexpr int maxChannels = 2;

    // Newton-Raphson パラメータ
    // nrThreshold: この Q 値以上で非線形モードに切り替え
    // maxNRIter:   最大反復回数（4回で音響用途には十分）
    // nrTolerance: 収束判定（delta がこれ以下で終了）
    static constexpr float nrThreshold = 1.5f;
    static constexpr int   maxNRIter = 4;
    static constexpr float nrTolerance = 1e-6f;

    // ===== 設定 =====
    double sampleRate = 48000.0;
    int    currentType = 0;        // 0=LP, 1=BP, 2=HP, 3=Notch
    bool   nonlinearMode = false;  // true: NR モード, false: 線形モード

    // ===== パラメータスムージング =====
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedResonance;

    // ===== フィルター係数 =====
    float g = 0.0f;           // tan(π·fc/fs) プリワーピング係数
    float h = 0.0f;           // 1 / (1 + g(g+k) + g²) 分母係数
    float k = 1.0f / 0.707f; // 1/Q ダンピング係数

    // ===== チャンネル別状態変数 =====
    float v1[maxChannels] = { 0.0f, 0.0f };  // 第1積分器（BP）
    float v2[maxChannels] = { 0.0f, 0.0f };  // 第2積分器（LP）

    // ===== 最終出力値（モニタリング用）=====
    float lastLp[maxChannels] = { 0.0f, 0.0f };
    float lastBp[maxChannels] = { 0.0f, 0.0f };
    float lastHp[maxChannels] = { 0.0f, 0.0f };

    // ===== 内部メソッド =====
    void  updateCoefficients(float fc, float Q);

    // 線形モード（Phase 1b と同一）
    float processSampleLinear(float x, int ch);

    // 非線形モード（Newton-Raphson + tanh フィードバック）
    float processSampleNonlinear(float x, int ch);

    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};