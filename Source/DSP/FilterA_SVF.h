// ==========================================
// DSP/FilterA_SVF_SIMD.h
// AVX2 SIMD最適化版 SVF (Step 2)
//
// 設計:
//   - 4インスタンス分の状態を SoA レイアウトで保持
//   - __m128 (SSE) による4並列処理（4floatをパック）
//   - 線形モード: 完全 SIMD 並列
//   - 非線形モード (Q>=1.5): tanh が SIMD で難しいため
//     インスタンスごとにスカラフォールバック
//
// なぜ __m128 か:
//   __m256 は 8 並列だが、ここでは 4 インスタンス分しか必要ない
//   SSE は AVX2 より枯れていて確実に動作する
//   将来のステレオ × 4 = 8並列化への布石でもある
// ==========================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <array>

// SIMD intrinsics
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif

class FilterA_SVF_SIMD
{
public:
    // ===== 定数 =====
    static constexpr int numInstances = 4;
    static constexpr int maxChannels = 2;
    static constexpr int subBlockSize = 16;

    static constexpr float nrThreshold = 1.5f;
    static constexpr int   maxNRIter = 4;
    static constexpr float nrTolerance = 1e-6f;

    FilterA_SVF_SIMD();
    ~FilterA_SVF_SIMD() = default;

    void prepare(double sampleRate, int blockSize);
    void reset();

    void setCutoff(int instance, float newCutoffHz);
    void setResonance(int instance, float newQ);
    void setType(int instance, int type);
    void setEnabled(int instance, bool enabled);

    void processBuffer(const juce::AudioBuffer<float>& input,
        std::array<juce::AudioBuffer<float>, 4>& outputs);

    bool isInstanceEnabled(int instance) const { return enabledFlags[instance]; }
    bool isNonlinearActive(int instance) const { return nonlinearMode[instance]; }

private:
    // ===== 設定 =====
    double sampleRate = 48000.0;

    // ===== パラメータスムージング =====
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff[numInstances];
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedResonance[numInstances];

    // ===== SIMD用係数 (SoA) =====
    // 4インスタンス分の係数を1つの __m128 にパック
    // [0]=A, [1]=B, [2]=C, [3]=D
    alignas(16) float g_arr[numInstances] = { 0.0f, 0.0f, 0.0f, 0.0f };
    alignas(16) float h_arr[numInstances] = { 0.0f, 0.0f, 0.0f, 0.0f };
    alignas(16) float k_arr[numInstances] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // ===== 状態変数 (SoA per channel) =====
    // v1[channel][instance] のレイアウト
    // 1チャンネル分の4インスタンス状態が1つの __m128 にパックされる
    alignas(16) float v1[maxChannels][numInstances] = { { 0.0f } };
    alignas(16) float v2[maxChannels][numInstances] = { { 0.0f } };

    // ===== モード =====
    bool nonlinearMode[numInstances] = { false, false, false, false };
    int  currentType[numInstances] = { 0, 0, 0, 0 };
    bool enabledFlags[numInstances] = { true, true, true, true };

    // ===== 内部メソッド =====
    void updateCoefficients(int instance, float fc, float Q);

    // ===== SIMD並列処理（線形モード）=====
    // 4インスタンス × 1チャンネル × 1サンプルを1回で処理
    // input4: 4インスタンスへの入力（同じソース信号がコピーされる場合もある）
    // 結果: 4インスタンス分の出力（type別）を out4[4] に格納
    void processSampleLinear_SIMD(int channel,
        const float input,
        float out4[numInstances]);

    // ===== スカラ処理（非線形モード - フォールバック）=====
    float processSampleNonlinear_Scalar(int instance, int ch, float x);

    // 1チャンネル分のバッファを4インスタンス分処理
    void processChannel(int ch,
        const float* inData,
        float* outData_A,
        float* outData_B,
        float* outData_C,
        float* outData_D,
        int          numSamples);

    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};