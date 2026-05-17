// ==========================================
// DSP/FilterA_SVF.h (Phase 1c版 - 変更なし)
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

    void process(juce::AudioBuffer<float>& buffer);

    float getLastLowpassOutput(int channel = 0)  const { return lastLp[channel]; }
    float getLastBandpassOutput(int channel = 0) const { return lastBp[channel]; }
    float getLastHighpassOutput(int channel = 0) const { return lastHp[channel]; }

    bool isNonlinearActive() const { return nonlinearMode; }

private:
    static constexpr int subBlockSize = 16;
    static constexpr int maxChannels = 2;

    static constexpr float nrThreshold = 1.5f;
    static constexpr int   maxNRIter = 4;
    static constexpr float nrTolerance = 1e-6f;

    double sampleRate = 48000.0;
    int    currentType = 0;
    bool   nonlinearMode = false;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedResonance;

    float g = 0.0f;
    float h = 0.0f;
    float k = 1.0f / 0.707f;

    float v1[maxChannels] = { 0.0f, 0.0f };
    float v2[maxChannels] = { 0.0f, 0.0f };

    float lastLp[maxChannels] = { 0.0f, 0.0f };
    float lastBp[maxChannels] = { 0.0f, 0.0f };
    float lastHp[maxChannels] = { 0.0f, 0.0f };

    float lastComputedCutoff = -1.0f;
    float lastComputedQ = -1.0f;

    void  updateCoefficients(float fc, float Q);
    float processSampleLinear(float x, int ch);
    float processSampleNonlinear(float x, int ch);

    static float fastTan(float x);
    static float clamp(float value, float minVal, float maxVal);
};