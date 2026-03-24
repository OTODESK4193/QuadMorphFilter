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

    // 0: Clean SVF, 1: Moog, 2: Diode, 3: SEM, 4: Bitcrush, 5: Formant, 6: Comb, 7: MS-20, 8: Phaser, 9: Wavefolder, 10: Reverb, 11: Kilo AP
    // 【Wave 1 追加】12: Prophet(Curtis), 13: SSM2040, 14: CS-80, 15: Jupiter, 16: EDP Wasp
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

    float lastCutoff = -1.0f;
    float lastRes = -1.0f;
    float scalerSVF = 1.0f;
    float scalerMoog = 1.0f;
    float scalerDiode = 1.0f;

    int filterModel = 0;
    int filterType = 0;
    int slopeIdx = 0;
    int currentStages = 1;

    // SVF & SEM & CS-80 & Wasp 係数
    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;
    float s1[8][2] = { {0.0f} };
    float s2[8][2] = { {0.0f} };

    // Ladder 系係数 (Moog, Diode, Prophet, SSM, Jupiter)
    float ladderG = 0.0f;
    float ladderRes = 0.0f;
    float zdfState[8][2][4] = { { {0.0f} } };

    float srrPhase[2] = { 0.0f, 0.0f };
    float srrHeld[2] = { 0.0f, 0.0f };

    float form_g[3] = { 0.0f };
    float form_R[3] = { 0.0f };
    float form_h[3] = { 0.0f };
    float form_s1[3][2] = { {0.0f} };
    float form_s2[3][2] = { {0.0f} };

    float combBuffer[8][2][4096] = { {{0.0f}} };
    int combWriteIdx[8][2] = { {0} };

    float sk_s1[8][2] = { {0.0f} };
    float sk_s2[8][2] = { {0.0f} };

    float ap_s[16][2] = { {0.0f} };
    float ap_out_prev[2] = { 0.0f };

    float rmsIn[2] = { 0.0f, 0.0f };
    float rmsOut[2] = { 0.0f, 0.0f };
    float agcGain[2] = { 1.0f, 1.0f };
};