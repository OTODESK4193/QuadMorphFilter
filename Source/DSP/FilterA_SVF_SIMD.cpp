// ==========================================
// DSP/FilterA_SVF_SIMD.cpp
// SIMD最適化準備版 SVF (Step 1: AoS基盤)
// ==========================================
#include "FilterA_SVF_SIMD.h"
#include <algorithm>

// ==========================================
// Constructor
// ==========================================
FilterA_SVF_SIMD::FilterA_SVF_SIMD()
{
    sampleRate = 48000.0;

    for (int i = 0; i < numInstances; ++i)
    {
        smoothedCutoff[i].setCurrentAndTargetValue(1000.0f);
        smoothedResonance[i].setCurrentAndTargetValue(0.707f);
        k[i] = 1.0f / 0.707f;
        currentType[i] = 0;
        nonlinearMode[i] = false;
        enabledFlags[i] = true;

        updateCoefficients(i, 1000.0f, 0.707f);
    }
}

// ==========================================
// Initialization
// ==========================================
void FilterA_SVF_SIMD::prepare(double newSampleRate, int /*blockSize*/)
{
    sampleRate = newSampleRate;

    const double rampTime = 0.020; // 20ms

    for (int i = 0; i < numInstances; ++i)
    {
        smoothedCutoff[i].reset(sampleRate, rampTime);
        smoothedResonance[i].reset(sampleRate, rampTime);

        updateCoefficients(i,
            smoothedCutoff[i].getCurrentValue(),
            smoothedResonance[i].getCurrentValue());
    }

    reset();
}

void FilterA_SVF_SIMD::reset()
{
    for (int i = 0; i < numInstances; ++i)
    {
        for (int ch = 0; ch < maxChannels; ++ch)
        {
            v1[i][ch] = 0.0f;
            v2[i][ch] = 0.0f;
            lastLp[i][ch] = 0.0f;
            lastBp[i][ch] = 0.0f;
            lastHp[i][ch] = 0.0f;
        }
    }
}

// ==========================================
// Parameter Setters
// ==========================================
void FilterA_SVF_SIMD::setCutoff(int instance, float newCutoffHz)
{
    if (instance < 0 || instance >= numInstances) return;
    smoothedCutoff[instance].setTargetValue(clamp(newCutoffHz, 20.0f, 20000.0f));
}

void FilterA_SVF_SIMD::setResonance(int instance, float newQ)
{
    if (instance < 0 || instance >= numInstances) return;
    smoothedResonance[instance].setTargetValue(clamp(newQ, 0.1f, 10.0f));
}

void FilterA_SVF_SIMD::setType(int instance, int type)
{
    if (instance < 0 || instance >= numInstances) return;
    currentType[instance] = clamp(type, 0, 3);
}

void FilterA_SVF_SIMD::setEnabled(int instance, bool enabled)
{
    if (instance < 0 || instance >= numInstances) return;
    enabledFlags[instance] = enabled;
}

// ==========================================
// Coefficient Calculation
// ==========================================
void FilterA_SVF_SIMD::updateCoefficients(int i, float fc, float Q)
{
    nonlinearMode[i] = (Q >= nrThreshold);

    k[i] = 1.0f / juce::jmax(0.1f, Q);

    const float pi = juce::MathConstants<float>::pi;
    float normalizedFreq = (pi * fc) / static_cast<float>(sampleRate);
    g[i] = fastTan(normalizedFreq);

    float denom = 1.0f + g[i] * (g[i] + k[i]) + g[i] * g[i];
    h[i] = (std::abs(denom) < 1e-10f) ? 1.0f : (1.0f / denom);
}

// ==========================================
// processSampleLinear
// ==========================================
float FilterA_SVF_SIMD::processSampleLinear(int i, int ch, float x)
{
    // デノーマル保護
    if (std::abs(v1[i][ch]) < 1e-15f) v1[i][ch] = 0.0f;
    if (std::abs(v2[i][ch]) < 1e-15f) v2[i][ch] = 0.0f;

    // ZDF/TPT 更新式
    float v3 = x - v2[i][ch] - k[i] * v1[i][ch];
    float v1_new = v1[i][ch] + g[i] * h[i] * v3;
    float v2_new = v2[i][ch] + g[i] * v1_new;

    v1[i][ch] = v1_new;
    v2[i][ch] = v2_new;

    lastLp[i][ch] = v2[i][ch];
    lastBp[i][ch] = v1[i][ch];
    lastHp[i][ch] = x - k[i] * v1[i][ch] - v2[i][ch];
    float notch = x - k[i] * v1[i][ch];

    float output = 0.0f;
    switch (currentType[i])
    {
    case 0:  output = lastLp[i][ch]; break;
    case 1:  output = lastBp[i][ch]; break;
    case 2:  output = lastHp[i][ch]; break;
    case 3:  output = notch;         break;
    default: output = lastLp[i][ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[i][ch] = 0.0f;
        v2[i][ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// processSampleNonlinear (Newton-Raphson)
// ==========================================
float FilterA_SVF_SIMD::processSampleNonlinear(int i, int ch, float x)
{
    if (std::abs(v1[i][ch]) < 1e-15f) v1[i][ch] = 0.0f;
    if (std::abs(v2[i][ch]) < 1e-15f) v2[i][ch] = 0.0f;

    // Newton-Raphson 反復
    float y = v1[i][ch];

    for (int iter = 0; iter < maxNRIter; ++iter)
    {
        float tanhY = std::tanh(y);
        float sech2Y = 1.0f - tanhY * tanhY;

        float F = y - v1[i][ch] - g[i] * h[i] * (x - v2[i][ch] - k[i] * tanhY);
        float dF = 1.0f + g[i] * h[i] * k[i] * sech2Y;

        float delta = F / dF;
        y -= delta;

        if (std::abs(delta) < nrTolerance)
            break;
    }

    v1[i][ch] = y;
    v2[i][ch] = v2[i][ch] + g[i] * y;

    float tanhV1 = std::tanh(v1[i][ch]);

    lastLp[i][ch] = v2[i][ch];
    lastBp[i][ch] = v1[i][ch];
    lastHp[i][ch] = x - k[i] * tanhV1 - v2[i][ch];
    float notch = x - k[i] * tanhV1;

    float output = 0.0f;
    switch (currentType[i])
    {
    case 0:  output = lastLp[i][ch]; break;
    case 1:  output = lastBp[i][ch]; break;
    case 2:  output = lastHp[i][ch]; break;
    case 3:  output = notch;         break;
    default: output = lastLp[i][ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[i][ch] = 0.0f;
        v2[i][ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// processInstanceChannel
// 1インスタンス・1チャンネル分のバッファを処理
// サブブロック処理付き
// ==========================================
void FilterA_SVF_SIMD::processInstanceChannel(int i, int ch,
    const float* inData,
    float* outData,
    int          numSamples)
{
    int samplePos = 0;

    while (samplePos < numSamples)
    {
        const int blockSize = juce::jmin(subBlockSize, numSamples - samplePos);

        // サブブロックごとに係数を更新
        const float currentCutoff = smoothedCutoff[i].skip(blockSize);
        const float currentRes = smoothedResonance[i].skip(blockSize);

        updateCoefficients(i, currentCutoff, currentRes);

        const bool useNonlinear = nonlinearMode[i];

        for (int n = 0; n < blockSize; ++n)
        {
            float input = inData[samplePos + n];

            if (useNonlinear)
                outData[samplePos + n] = processSampleNonlinear(i, ch, input);
            else
                outData[samplePos + n] = processSampleLinear(i, ch, input);
        }

        samplePos += blockSize;
    }
}

// ==========================================
// processBuffer
// 4インスタンスを順番に処理して4つの出力バッファに格納
// ==========================================
void FilterA_SVF_SIMD::processBuffer(const juce::AudioBuffer<float>& input,
    std::array<juce::AudioBuffer<float>, 4>& outputs)
{
    const int numChannels = juce::jmin(input.getNumChannels(), maxChannels);
    const int numSamples = input.getNumSamples();

    for (int i = 0; i < numInstances; ++i)
    {
        if (!enabledFlags[i])
        {
            outputs[i].clear();
            continue;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* inData = input.getReadPointer(ch);
            float* outData = outputs[i].getWritePointer(ch);

            processInstanceChannel(i, ch, inData, outData, numSamples);
        }
    }
}

// ==========================================
// Helper: Padé tan(x) approximation
// ==========================================
float FilterA_SVF_SIMD::fastTan(float x)
{
    if (x < -1.5f || x > 1.5f)
        return std::tan(x);

    float x2 = x * x;
    float x4 = x2 * x2;

    float numerator = x * (105.0f + 10.0f * x2 + x4);
    float denominator = 105.0f + 45.0f * x2 + x4;

    return numerator / denominator;
}

// ==========================================
// Helper: clamp
// ==========================================
float FilterA_SVF_SIMD::clamp(float value, float minVal, float maxVal)
{
    return std::min(maxVal, std::max(minVal, value));
}