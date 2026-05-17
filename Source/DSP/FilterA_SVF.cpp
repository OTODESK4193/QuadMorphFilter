// ==========================================
// DSP/FilterA_SVF.cpp (Phase 1c版 - 変更なし)
// ==========================================
#include "FilterA_SVF.h"
#include <algorithm>

FilterA_SVF::FilterA_SVF()
{
    sampleRate = 48000.0;
    currentType = 0;
    nonlinearMode = false;

    smoothedCutoff.setCurrentAndTargetValue(1000.0f);
    smoothedResonance.setCurrentAndTargetValue(0.707f);

    k = 1.0f / 0.707f;
    updateCoefficients(1000.0f, 0.707f);
}

void FilterA_SVF::prepare(double newSampleRate, int /*blockSize*/)
{
    sampleRate = newSampleRate;

    const double rampTime = 0.020;
    smoothedCutoff.reset(sampleRate, rampTime);
    smoothedResonance.reset(sampleRate, rampTime);

    lastComputedCutoff = -1.0f;
    lastComputedQ = -1.0f;

    updateCoefficients(
        smoothedCutoff.getCurrentValue(),
        smoothedResonance.getCurrentValue());

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

void FilterA_SVF::setCutoff(float newCutoffHz)
{
    smoothedCutoff.setTargetValue(clamp(newCutoffHz, 20.0f, 20000.0f));
}

void FilterA_SVF::setResonance(float newQ)
{
    smoothedResonance.setTargetValue(clamp(newQ, 0.1f, 10.0f));
}

void FilterA_SVF::setType(int type)
{
    currentType = clamp(type, 0, 3);
}

void FilterA_SVF::updateCoefficients(float fc, float Q)
{
    nonlinearMode = (Q >= nrThreshold);

    k = 1.0f / juce::jmax(0.1f, Q);

    const float pi = juce::MathConstants<float>::pi;
    float normalizedFreq = (pi * fc) / static_cast<float>(sampleRate);
    g = fastTan(normalizedFreq);

    float denom = 1.0f + g * (g + k) + g * g;
    h = (std::abs(denom) < 1e-10f) ? 1.0f : (1.0f / denom);
}

float FilterA_SVF::processSampleLinear(float x, int ch)
{
    if (std::abs(v1[ch]) < 1e-15f) v1[ch] = 0.0f;
    if (std::abs(v2[ch]) < 1e-15f) v2[ch] = 0.0f;

    float v3 = x - v2[ch] - k * v1[ch];
    float v1_new = v1[ch] + g * h * v3;
    float v2_new = v2[ch] + g * v1_new;

    v1[ch] = v1_new;
    v2[ch] = v2_new;

    lastLp[ch] = v2[ch];
    lastBp[ch] = v1[ch];
    lastHp[ch] = x - k * v1[ch] - v2[ch];
    float notch = x - k * v1[ch];

    float output = 0.0f;
    switch (currentType)
    {
    case 0:  output = lastLp[ch]; break;
    case 1:  output = lastBp[ch]; break;
    case 2:  output = lastHp[ch]; break;
    case 3:  output = notch;      break;
    default: output = lastLp[ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

float FilterA_SVF::processSampleNonlinear(float x, int ch)
{
    if (std::abs(v1[ch]) < 1e-15f) v1[ch] = 0.0f;
    if (std::abs(v2[ch]) < 1e-15f) v2[ch] = 0.0f;

    float y = v1[ch];

    for (int iter = 0; iter < maxNRIter; ++iter)
    {
        float tanhY = std::tanh(y);
        float sech2Y = 1.0f - tanhY * tanhY;

        float F = y - v1[ch] - g * h * (x - v2[ch] - k * tanhY);
        float dF = 1.0f + g * h * k * sech2Y;

        float delta = F / dF;
        y -= delta;

        if (std::abs(delta) < nrTolerance)
            break;
    }

    v1[ch] = y;
    v2[ch] = v2[ch] + g * y;

    float tanhV1 = std::tanh(v1[ch]);

    lastLp[ch] = v2[ch];
    lastBp[ch] = v1[ch];
    lastHp[ch] = x - k * tanhV1 - v2[ch];
    float notch = x - k * tanhV1;

    float output = 0.0f;
    switch (currentType)
    {
    case 0:  output = lastLp[ch]; break;
    case 1:  output = lastBp[ch]; break;
    case 2:  output = lastHp[ch]; break;
    case 3:  output = notch;      break;
    default: output = lastLp[ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

void FilterA_SVF::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    int samplePos = 0;

    while (samplePos < numSamples)
    {
        const int blockSize = juce::jmin(subBlockSize, numSamples - samplePos);

        const float currentCutoff = smoothedCutoff.skip(blockSize);
        const float currentRes = smoothedResonance.skip(blockSize);

        updateCoefficients(currentCutoff, currentRes);

        const bool useNonlinear = nonlinearMode;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch, samplePos);

            for (int n = 0; n < blockSize; ++n)
            {
                if (useNonlinear)
                    data[n] = processSampleNonlinear(data[n], ch);
                else
                    data[n] = processSampleLinear(data[n], ch);
            }
        }

        samplePos += blockSize;
    }
}

float FilterA_SVF::fastTan(float x)
{
    if (x < -1.5f || x > 1.5f)
        return std::tan(x);

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