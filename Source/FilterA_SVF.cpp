// ==========================================
// FilterA_SVF.cpp (修正版)
// Zero-Delay Feedback State Variable Filter Implementation
// ==========================================

#include "FilterA_SVF.h"
#include <algorithm>

FilterA_SVF::FilterA_SVF()
{
    sampleRate = 48000.0;
    cutoffHz = 1000.0f;
    resQ = 0.707f;
    k = 1.0f / resQ;
    currentType = 0;

    updateCoefficients();
}

void FilterA_SVF::prepare(double newSampleRate, int blockSize)
{
    sampleRate = newSampleRate;
    updateCoefficients();
    reset();
}

void FilterA_SVF::reset()
{
    v1 = 0.0f;
    v2 = 0.0f;
    lastLp = 0.0f;
    lastBp = 0.0f;
    lastHp = 0.0f;
}

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

// ===== 【修正】updateCoefficients: キャッシング追加 =====
void FilterA_SVF::updateCoefficients()
{
    // キャッシュチェック（値が変わらなければ再計算しない）
    if (std::abs(cutoffHz - lastComputedCutoff) < 0.1f &&
        std::abs(resQ - lastComputedQ) < 0.001f)
    {
        return;
    }

    lastComputedCutoff = cutoffHz;
    lastComputedQ = resQ;

    const float pi = juce::MathConstants<float>::pi;
    float normalizedFreq = (pi * cutoffHz) / static_cast<float>(sampleRate);

    g = fastTan(normalizedFreq);

    float denom = 1.0f + g * (g + k) + g * g;

    if (std::abs(denom) < 1e-10f)
    {
        h = 1.0f;
    }
    else
    {
        h = 1.0f / denom;
    }

    g2 = 2.0f * g * k;
}

// ===== 【修正】processSample: デノーマル保護 + 数値安定性 =====
float FilterA_SVF::processSample(float inputSample)
{
    // デノーマル値の強制ゼロ化（CPU スパイク防止）
    if (std::abs(v1) < 1e-10f) v1 = 0.0f;
    if (std::abs(v2) < 1e-10f) v2 = 0.0f;

    // Zero-Delay Feedback topology
    float v3 = inputSample - v2 - k * v1;

    float v1_input = v1 + g * h * v3;
    float v2_input = v2 + g * v1_input;

    v1 = v1_input;
    v2 = v2_input;

    // 4つの出力タイプを計算
    lastLp = v2;
    lastBp = v1;
    lastHp = inputSample - k * v1 - v2;
    float notch = inputSample - k * v1;

    float output = 0.0f;
    switch (currentType)
    {
    case 0:
        output = lastLp;
        break;
    case 1:
        output = lastBp;
        break;
    case 2:
        output = lastHp;
        break;
    case 3:
        output = notch;
        break;
    default:
        output = lastLp;
        break;
    }

    // NaN/Inf チェック（異常値排除）
    if (!std::isfinite(output))
    {
        output = 0.0f;
    }

    return output;
}

void FilterA_SVF::process(juce::AudioBuffer<float>& buffer)
{
    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int n = 0; n < numSamples; ++n)
        {
            channelData[n] = processSample(channelData[n]);
        }
    }
}

// ===== 【修正】fastTan: 高精度 Padé [5/4] 近似 =====
float FilterA_SVF::fastTan(float x)
{
    // 高精度 Padé 有理関数近似
    // tan(x) ≈ x(105 + 10x² + x⁴) / (105 + 45x² + x⁴)

    if (x < -1.5f || x > 1.5f)
    {
        return std::tan(x);
    }

    float x2 = x * x;
    float x4 = x2 * x2;

    float numerator = x * (105.0f + 10.0f * x2 + x4);
    float denominator = 105.0f + 45.0f * x2 + x4;

    return numerator / denominator;
}

float FilterA_SVF::clamp(float value, float minVal, float maxVal)
{
    return std::min(maxVal, std::max(minVal, value));
}