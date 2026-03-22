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

    void setCutoff(float newCutoff);
    void setResonance(float newResonance);
    void setType(int newType);

    // 【追加】Slope（12/24/48/96 dB/oct）の設定
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
    int filterType = 0;
    int currentStages = 1; // 12dB=1, 24dB=2, 48dB=4, 96dB=8

    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;

    // 【追加】多段カスケード用の固定長配列（最大8段 = 96dB/oct）
    float s1[8][2] = { {0.0f} };
    float s2[8][2] = { {0.0f} };
};