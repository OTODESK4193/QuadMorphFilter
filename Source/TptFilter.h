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

    // 0: Clean SVF, 1: Moog Ladder, 2: Diode Ladder, 3: SEM, 4: Bitcrush/SRR, 5: Formant
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

    // SVF & SEM 係数
    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;
    float s1[8][2] = { {0.0f} };
    float s2[8][2] = { {0.0f} };

    // Moog & Diode Ladder 係数
    float ladderG = 0.0f;
    float ladderRes = 0.0f;
    float zdfState[8][2][4] = { { {0.0f} } };

    // Bitcrush / SRR 係数
    float srrPhase[2] = { 0.0f, 0.0f };
    float srrHeld[2] = { 0.0f, 0.0f };

    // 【追加】Formant (Vowel) 係数
    float form_g[3] = { 0.0f };
    float form_R[3] = { 0.0f };
    float form_h[3] = { 0.0f };
    float form_s1[3][2] = { {0.0f} };
    float form_s2[3][2] = { {0.0f} };

    // リアルタイムRMS・動的AGC用ステート
    float rmsIn[2] = { 0.0f, 0.0f };
    float rmsOut[2] = { 0.0f, 0.0f };
    float agcGain[2] = { 1.0f, 1.0f };
};