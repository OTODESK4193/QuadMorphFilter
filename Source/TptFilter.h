// ==========================================
// TptFilter.h
// ==========================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class TptFilter
{
public:
    TptFilter();
    ~TptFilter() = default;

    void prepare(double newSampleRate, int samplesPerBlock, int numChannels);
    void reset();

    void setModel(int newModel);
    void setCutoff(float newCutoff);
    void setResonance(float newResonance);
    void setType(int newType);
    void setSlope(int slopeIndex);

    void process(juce::AudioBuffer<float>& buffer);
    float getMagnitudeForFrequency(float frequency) const;

private:
    float processSample(int channel, float x);
    void updateCoefficients();

    double sampleRate = 44100.0;
    int maxChannels = 2;

    juce::SmoothedValue<float> cutoff;
    juce::SmoothedValue<float> resonance;

    int filterModel = 0;
    int filterType = 0;
    int slopeIdx = 0;
    int currentStages = 1;

    // SVF係数
    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;
    float s1[8][2] = { {0.0f} };
    float s2[8][2] = { {0.0f} };

    // Moog Ladder係数
    float moogG = 0.0f;
    float moogRes = 0.0f;
    float zdfState[8][2][4] = { { {0.0f} } };

    // 【追加】リアルタイムRMS・オートゲインコントロール（動的AGC）用ステート
    float rmsIn[2] = { 0.0f, 0.0f };
    float rmsOut[2] = { 0.0f, 0.0f };
    float agcGain[2] = { 1.0f, 1.0f };
};