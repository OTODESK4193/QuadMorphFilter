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

    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;

    float s1[2] = { 0.0f, 0.0f };
    float s2[2] = { 0.0f, 0.0f };
};