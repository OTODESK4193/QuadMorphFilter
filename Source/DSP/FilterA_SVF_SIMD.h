// ==========================================
// DSP/FilterA_SVF_SIMD.h
// SIMD最適化準備版 SVF (Step 1: AoS基盤)
//
// 設計思想:
//   - 4インスタンス分（A/B/C/D）の状態を一括管理
//   - 将来の SoA + AVX2 化に備えた構造
//   - Step 1 では既存 FilterA_SVF と同じスカラー処理
//   - Step 2 で SIMD 命令に置き換え
//
// 既存 FilterA_SVF との違い:
//   - 4インスタンス分のフィルターを1つのクラスで管理
//   - メモリレイアウトが SIMD 化を意識した配置
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <array>

class FilterA_SVF_SIMD
{
public:
    // ===== 定数 =====
    static constexpr int numInstances = 4;   // A, B, C, D の4フィルター
    static constexpr int maxChannels = 2;   // ステレオ
    static constexpr int subBlockSize = 16;  // サブブロックサイズ

    // Newton-Raphson 設定
    static constexpr float nrThreshold = 1.5f;
    static constexpr int   maxNRIter = 4;
    static constexpr float nrTolerance = 1e-6f;

    FilterA_SVF_SIMD();
    ~FilterA_SVF_SIMD() = default;

    // ===== 初期化 =====
    void prepare(double sampleRate, int blockSize);
    void reset();

    // ===== パラメータ設定（インスタンスごと）=====
    void setCutoff(int instance, float newCutoffHz);
    void setResonance(int instance, float newQ);
    void setType(int instance, int type);
    void setEnabled(int instance, bool enabled);

    // ===== バッファ処理 =====
    // 入力バッファ(buffer) を フィルター i 用のバッファ(outputs[i]) にコピーしてフィルター適用
    void processBuffer(const juce::AudioBuffer<float>& input,
        std::array<juce::AudioBuffer<float>, 4>& outputs);

    // ===== モニタリング =====
    bool isInstanceEnabled(int instance) const { return enabledFlags[instance]; }
    bool isNonlinearActive(int instance) const { return nonlinearMode[instance]; }

private:
    // ===== 設定 =====
    double sampleRate = 48000.0;

    // ===== パラメータスムージング（インスタンスごと）=====
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff[numInstances];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedResonance[numInstances];

    // ===== フィルター状態 =====
    // 【SoA 準備】将来 SIMD 化したときに連続メモリアクセスできるレイアウト
    //
    // 現在: [instance][channel] の AoS的だがフラット
    // 将来: [instance] が SIMD レーンになる
    alignas(32) float g[numInstances] = { 0.0f };
    alignas(32) float h[numInstances] = { 0.0f };
    alignas(32) float k[numInstances] = { 0.0f };
    alignas(32) float v1[numInstances][maxChannels] = { { 0.0f } };
    alignas(32) float v2[numInstances][maxChannels] = { { 0.0f } };

    // ===== モード判定 =====
    bool nonlinearMode[numInstances] = { false, false, false, false };
    int  currentType[numInstances] = { 0, 0, 0, 0 };
    bool enabledFlags[numInstances] = { true, true, true, true };

    // ===== モニタリング =====
    float lastLp[numInstances][maxChannels] = { { 0.0f } };
    float lastBp[numInstances][maxChannels] = { { 0.0f } };
    float lastHp[numInstances][maxChannels] = { { 0.0f } };

    // ===== 内部メソッド =====
    void updateCoefficients(int instance, float fc, float Q);

    // 1サンプル処理（線形モード）
    float processSampleLinear(int instance, int ch, float x);

    // 1サンプル処理（非線形モード - Newton-Raphson）
    float processSampleNonlinear(int instance, int ch, float x);

    // 1インスタンス・1チャンネル分のバッファ処理
    void processInstanceChannel(int instance, int ch,
        const float* inData,
        float* outData,
        int          numSamples);

    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};