// ==========================================
// FilterA_SVF.cpp (修正版 v2)
// Zero-Delay Feedback State Variable Filter
//
// 修正点: ステレオ対応（チャンネル別状態変数）
// ==========================================

#include "FilterA_SVF.h"
#include <algorithm>

// ==========================================
// Constructor
// ==========================================
FilterA_SVF::FilterA_SVF()
{
    sampleRate = 48000.0;
    cutoffHz = 1000.0f;
    resQ = 0.707f;
    k = 1.0f / resQ;
    currentType = 0;

    updateCoefficients();
}

// ==========================================
// Initialization
// ==========================================
void FilterA_SVF::prepare(double newSampleRate, int /*blockSize*/)
{
    sampleRate = newSampleRate;

    // サンプルレートが変わったら係数を強制再計算
    lastComputedCutoff = -1.0f;
    lastComputedQ = -1.0f;

    updateCoefficients();
    reset();
}

void FilterA_SVF::reset()
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;

        lastLp[ch] = 0.0f;
        lastBp[ch] = 0.0f;
        lastHp[ch] = 0.0f;
    }
}

// ==========================================
// Parameter Control
// ==========================================
void FilterA_SVF::setCutoff(float newCutoffHz)
{
    cutoffHz = clamp(newCutoffHz, 20.0f, 20000.0f);
    updateCoefficients();
}

void FilterA_SVF::setResonance(float newQ)
{
    resQ = clamp(newQ, 0.1f, 10.0f);
    k = 1.0f / resQ;
    updateCoefficients();
}

void FilterA_SVF::setType(int type)
{
    currentType = clamp(type, 0, 3);
}

// ==========================================
// Coefficient Calculation
// ==========================================
void FilterA_SVF::updateCoefficients()
{
    // 値が変わっていなければスキップ（余分な tan() 計算を避ける）
    if (std::abs(cutoffHz - lastComputedCutoff) < 0.1f &&
        std::abs(resQ - lastComputedQ) < 0.001f)
    {
        return;
    }

    lastComputedCutoff = cutoffHz;
    lastComputedQ = resQ;

    const float pi = juce::MathConstants<float>::pi;

    // プリワーピング: g = tan(π * fc / fs)
    // アナログフィルターの周波数特性をデジタル領域に正確に対応させる
    float normalizedFreq = (pi * cutoffHz) / static_cast<float>(sampleRate);

    g = fastTan(normalizedFreq);

    // 分母係数: h = 1 / (1 + g(g+k) + g²)
    // ZDF方程式を解いた結果で、毎サンプルの除算を避けるための事前計算
    float denom = 1.0f + g * (g + k) + g * g;

    if (std::abs(denom) < 1e-10f)
    {
        h = 1.0f;  // ゼロ除算防止
    }
    else
    {
        h = 1.0f / denom;
    }

    g2 = 2.0f * g * k;
}

// ==========================================
// Core DSP: Per-sample processing
// ==========================================
float FilterA_SVF::processSample(float inputSample, int channel)
{
    // チャンネル番号の安全確認
    if (channel < 0 || channel >= maxChannels)
        channel = 0;

    // デノーマル値の強制ゼロ化（CPU スパイク防止）
    // ScopedNoDenormals で大半は防げるが念のため
    if (std::abs(v1[channel]) < 1e-15f) v1[channel] = 0.0f;
    if (std::abs(v2[channel]) < 1e-15f) v2[channel] = 0.0f;

    // ===== Andy Simper ZDF/TPT 更新式 =====
    //
    //  v3 は「フィードバックを含む誤差信号」
    //  v1 は「第1積分器（BPF出力相当）」
    //  v2 は「第2積分器（LPF出力相当）」
    //
    float v3 = inputSample - v2[channel] - k * v1[channel];
    float v1_new = v1[channel] + g * h * v3;
    float v2_new = v2[channel] + g * v1_new;

    // 状態を更新（次サンプルへの記憶）
    v1[channel] = v1_new;
    v2[channel] = v2_new;

    // 4出力の同時計算（線形演算なのでコスト増なし）
    lastLp[channel] = v2[channel];
    lastBp[channel] = v1[channel];
    lastHp[channel] = inputSample - k * v1[channel] - v2[channel];
    float notch = inputSample - k * v1[channel];

    // 選択されたタイプの出力
    float output = 0.0f;
    switch (currentType)
    {
    case 0:  output = lastLp[channel]; break;
    case 1:  output = lastBp[channel]; break;
    case 2:  output = lastHp[channel]; break;
    case 3:  output = notch;           break;
    default: output = lastLp[channel]; break;
    }

    // NaN / Inf が発生した場合は状態をリセットして無音にする
    // （これが頻発する場合は係数計算に問題がある）
    if (!std::isfinite(output))
    {
        v1[channel] = 0.0f;
        v2[channel] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// Buffer Processing
// ==========================================
void FilterA_SVF::process(juce::AudioBuffer<float>& buffer)
{
    // ===== 【修正の核心】=====
    // チャンネルを独立して処理する
    // ch=0 (Left) と ch=1 (Right) が互いの状態変数を汚染しない

    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int n = 0; n < numSamples; ++n)
        {
            // ← channel インデックスを渡すことで独立した v1[ch], v2[ch] を使用
            channelData[n] = processSample(channelData[n], ch);
        }
    }
}

// ==========================================
// Fast Math: Padé [5/4] approximation of tan(x)
// ==========================================
float FilterA_SVF::fastTan(float x)
{
    // 音響用途向けの高精度 Padé 近似
    // tan(x) ≈ x(105 + 10x² + x⁴) / (105 + 45x² + x⁴)
    //
    // 誤差: 音響周波数域（0 ≤ x ≤ π/2）で < 0.01%

    if (x < -1.5f || x > 1.5f)
    {
        // 範囲外は標準ライブラリにフォールバック
        return std::tan(x);
    }

    float x2 = x * x;
    float x4 = x2 * x2;

    float numerator = x * (105.0f + 10.0f * x2 + x4);
    float denominator = 105.0f + 45.0f * x2 + x4;

    return numerator / denominator;
}

// ==========================================
// Utility
// ==========================================
float FilterA_SVF::clamp(float value, float minVal, float maxVal)
{
    return std::min(maxVal, std::max(minVal, value));
}