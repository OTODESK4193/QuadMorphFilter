// ==========================================
// FilterA_SVF.h (修正版)
// Zero-Delay Feedback State Variable Filter
// Based on Andy Simper's Linear Trapezoidal Integrator
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

    void setCutoff(float newCutoffHz);
    void setResonance(float newQ);
    void setType(int type);

    float processSample(float inputSample);
    void process(juce::AudioBuffer<float>& buffer);

    float getLastLowpassOutput() const { return lastLp; }
    float getLastBandpassOutput() const { return lastBp; }
    float getLastHighpassOutput() const { return lastHp; }

private:
    double sampleRate = 48000.0;
    int currentType = 0;

    float cutoffHz = 1000.0f;
    float resQ = 0.707f;
    float k = 1.0f / 0.707f;

    float g = 0.0f;
    float h = 0.0f;
    float g2 = 0.0f;

    float v1 = 0.0f;
    float v2 = 0.0f;

    float lastLp = 0.0f;
    float lastBp = 0.0f;
    float lastHp = 0.0f;

    // ===== 【修正】係数キャッシング用 =====
    float lastComputedCutoff = -1.0f;
    float lastComputedQ = -1.0f;

    void updateCoefficients();
    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};