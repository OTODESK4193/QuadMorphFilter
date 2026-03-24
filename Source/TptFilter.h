// ==========================================
// TptFilter.h
// ==========================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>

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

    void calcDigitalPrecisionCoeffs(float fc, float res);
    void updateZPlaneCoeffs(float fc, float res);

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
    int filterOrder = 2;

    float smoothedDigitalCutoff = 1000.0f;

    float g = 0.0f; float R = 0.0f; float h = 0.0f;
    float s1[8][2] = { {0.0f} }; float s2[8][2] = { {0.0f} };

    float ladderG = 0.0f; float ladderRes = 0.0f;
    float zdfState[8][2][4] = { { {0.0f} } };

    float srrPhase[2] = { 0.0f, 0.0f }; float srrHeld[2] = { 0.0f, 0.0f };
    float form_g[3] = { 0.0f }; float form_R[3] = { 0.0f }; float form_h[3] = { 0.0f };
    float form_s1[3][2] = { {0.0f} }; float form_s2[3][2] = { {0.0f} };
    float combBuffer[8][2][4096] = { {{0.0f}} }; int combWriteIdx[8][2] = { {0} };
    float sk_s1[8][2] = { {0.0f} }; float sk_s2[8][2] = { {0.0f} };
    float ap_s[16][2] = { {0.0f} }; float ap_out_prev[2] = { 0.0f };

    struct BiquadCoeffs { float g, R, h; };
    std::vector<BiquadCoeffs> dpCoeffs;
    float dp_s1[8][2] = { {0.0f} }; float dp_s2[8][2] = { {0.0f} };

    float lpgEnv[2] = { 0.0f, 0.0f };
    float lpgTriggerState[2] = { 0.0f, 0.0f };

    static const int numModalBands = 8;
    float modal_g[numModalBands] = { 0.0f };
    float modal_R[numModalBands] = { 0.0f };
    float modal_h[numModalBands] = { 0.0f };
    float modal_s1[numModalBands][2] = { {0.0f} };
    float modal_s2[numModalBands][2] = { {0.0f} };

    float wgBuffer[2][4096] = { {0.0f} };
    int wgWriteIdx[2] = { 0 };

    const float hilbertCoeffsA[4] = { 0.4794008656f, 0.8762184935f, 0.9765975895f, 0.9974992559f };
    const float hilbertCoeffsB[4] = { 0.1617584983f, 0.7330289323f, 0.9453497003f, 0.9905991567f };
    float hilbertStateA[4][2] = { {0.0f} };
    float hilbertStateB[4][2] = { {0.0f} };
    float bodePhase[2] = { 0.0f, 0.0f };

    std::vector<BiquadCoeffs> zplaneCoeffs;
    float zplane_s1[7][2] = { {0.0f} };
    float zplane_s2[7][2] = { {0.0f} };
    float zplaneTargetZ[7][2] = { {0.0f} };
    float zplaneCurrentZ[7][2] = { {0.0f} };

    float pa_s[8][2] = { {0.0f} };

    float aa_s1[4][2] = { {0.0f} };
    float aa_s2[4][2] = { {0.0f} };

    float rmsIn[2] = { 0.0f, 0.0f };
    float rmsOut[2] = { 0.0f, 0.0f };
    float agcGain[2] = { 1.0f, 1.0f };
};