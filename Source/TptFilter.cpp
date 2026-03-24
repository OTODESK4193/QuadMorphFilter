// ==========================================
// TptFilter.cpp
// ==========================================
#include "TptFilter.h"
#include <cmath>
#include <random>
#include <algorithm>

TptFilter::TptFilter()
{
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    cutoff.setCurrentAndTargetValue(1000.0f);
    resonance.setCurrentAndTargetValue(0.707f);
    dpCoeffs.resize(8);
    zplaneCoeffs.resize(7);
}

void TptFilter::prepare(double newSampleRate, int /*samplesPerBlock*/, int numChannels)
{
    sampleRate = newSampleRate;
    maxChannels = juce::jmin(numChannels, 2);
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    smoothedDigitalCutoff = cutoff.getCurrentValue();
    reset();
}

void TptFilter::reset()
{
    for (int stage = 0; stage < 8; ++stage) {
        for (int ch = 0; ch < 2; ++ch) {
            s1[stage][ch] = 0.0f; s2[stage][ch] = 0.0f;
            sk_s1[stage][ch] = 0.0f; sk_s2[stage][ch] = 0.0f;
            dp_s1[stage][ch] = 0.0f; dp_s2[stage][ch] = 0.0f;
            for (int pole = 0; pole < 4; ++pole) zdfState[stage][ch][pole] = 0.0f;
            combWriteIdx[stage][ch] = 0;
            for (int i = 0; i < 4096; ++i) combBuffer[stage][ch][i] = 0.0f;
            pa_s[stage][ch] = 0.0f;
        }
    }
    for (int stage = 0; stage < 16; ++stage) {
        for (int ch = 0; ch < 2; ++ch) ap_s[stage][ch] = 0.0f;
    }
    for (int stage = 0; stage < 7; ++stage) {
        for (int ch = 0; ch < 2; ++ch) { zplane_s1[stage][ch] = 0.0f; zplane_s2[stage][ch] = 0.0f; }
        float theta = juce::MathConstants<float>::pi * (2.0f * stage + 1.0f) / 14.0f;
        zplaneTargetZ[stage][0] = -std::sin(theta); zplaneTargetZ[stage][1] = std::cos(theta);
        zplaneCurrentZ[stage][0] = zplaneTargetZ[stage][0]; zplaneCurrentZ[stage][1] = zplaneTargetZ[stage][1];
    }
    for (int ch = 0; ch < 2; ++ch) {
        rmsIn[ch] = 0.0f; rmsOut[ch] = 0.0f; agcGain[ch] = 1.0f;
        srrPhase[ch] = 0.0f; srrHeld[ch] = 0.0f; ap_out_prev[ch] = 0.0f;
        lpgEnv[ch] = 0.0f; lpgTriggerState[ch] = 0.0f; bodePhase[ch] = 0.0f; wgWriteIdx[ch] = 0;
        for (int i = 0; i < 4096; ++i) wgBuffer[ch][i] = 0.0f;
        for (int f = 0; f < 3; ++f) { form_s1[f][ch] = 0.0f; form_s2[f][ch] = 0.0f; }
        for (int b = 0; b < numModalBands; ++b) { modal_s1[b][ch] = 0.0f; modal_s2[b][ch] = 0.0f; }
        for (int h = 0; h < 4; ++h) { hilbertStateA[h][ch] = 0.0f; hilbertStateB[h][ch] = 0.0f; }
        for (int a = 0; a < 4; ++a) { aa_s1[a][ch] = 0.0f; aa_s2[a][ch] = 0.0f; }
    }
    lastCutoff = -1.0f; lastRes = -1.0f;
    smoothedDigitalCutoff = cutoff.getCurrentValue();
}

void TptFilter::setModel(int newModel) { filterModel = newModel; lastCutoff = -1.0f; }
void TptFilter::setCutoff(float newCutoff) { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff)); }
void TptFilter::setResonance(float newResonance) { resonance.setTargetValue(juce::jmax(0.1f, newResonance)); }
void TptFilter::setType(int newType) { filterType = newType; }

void TptFilter::setSlope(int index)
{
    slopeIdx = index;
    if (filterModel == 5 || filterModel == 10 || filterModel == 21 || filterModel == 22 || filterModel == 24 || filterModel == 25 || filterModel == 27) {
        currentStages = 1; filterOrder = 2;
    }
    else if (filterModel == 8 || filterModel == 11 || filterModel == 26) {
        currentStages = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 8 : 16; filterOrder = currentStages * 2;
    }
    else if (filterModel >= 17 && filterModel <= 20) {
        filterOrder = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 8 : 16; currentStages = filterOrder / 2;
    }
    else if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 6 || filterModel == 7 || filterModel == 9 || filterModel == 14 || filterModel == 16 || filterModel == 23) {
        currentStages = (index == 0) ? 1 : (index == 1) ? 2 : (index == 2) ? 4 : 8; filterOrder = currentStages * 2;
    }
    else {
        currentStages = (index == 0) ? 1 : (index == 1) ? 1 : (index == 2) ? 2 : 4; filterOrder = currentStages * 4;
    }

    scalerSVF = (currentStages > 1) ? std::pow(0.6f, std::log2((float)currentStages)) : 1.0f;
    scalerMoog = (currentStages > 1) ? std::pow(0.7f, std::log2((float)currentStages)) : 1.0f;
    scalerDiode = (currentStages > 1) ? std::pow(0.7f, std::log2((float)currentStages)) : 1.0f;
    lastCutoff = -1.0f;
}

void TptFilter::calcDigitalPrecisionCoeffs(float fc, float res) {
    float maxSafeFreq = (float)sampleRate * 0.45f;
    float rippleDb = juce::jmap(res, 0.1f, 10.0f, 0.1f, 3.0f);
    float eps = std::sqrt(std::pow(10.0f, rippleDb / 10.0f) - 1.0f);

    for (int k = 0; k < currentStages; ++k) {
        float q = 0.707f; float freqScale = 1.0f;
        if (filterModel == 17) {
            float theta = juce::MathConstants<float>::pi * (2.0f * k + 1.0f) / (2.0f * filterOrder);
            q = std::max(0.5f, 1.0f / (2.0f * std::sin(theta)));
        }
        else if (filterModel == 18) {
            float theta = juce::MathConstants<float>::pi * (2.0f * k + 1.0f) / (2.0f * filterOrder);
            float a = 1.0f / filterOrder * std::asinh(1.0f / eps);
            float real_p = -std::sinh(a) * std::sin(theta); float imag_p = std::cosh(a) * std::cos(theta);
            float wn2 = real_p * real_p + imag_p * imag_p;
            freqScale = std::sqrt(wn2); q = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p));
        }
        else if (filterModel == 19) {
            float theta = juce::MathConstants<float>::pi * (2.0f * k + 1.0f) / (2.0f * filterOrder);
            q = std::max(0.5f, 1.0f / (2.0f * std::sin(theta)) * 0.577f); freqScale = 1.0f + (float)filterOrder * 0.1f;
        }
        else if (filterModel == 20) {
            float theta = juce::MathConstants<float>::pi * (2.0f * k + 1.0f) / (2.0f * filterOrder);
            float a = 1.0f / filterOrder * std::asinh(1.0f / (eps * 0.5f));
            float real_p = -std::sinh(a) * std::sin(theta); float imag_p = std::cosh(a) * std::cos(theta);
            float wn2 = real_p * real_p + imag_p * imag_p;
            freqScale = std::sqrt(wn2); q = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p) * 1.2f);
        }
        float sectionFreq = std::clamp(fc * freqScale, 20.0f, maxSafeFreq);
        float sectionWd = juce::MathConstants<float>::pi * sectionFreq / (float)sampleRate;
        dpCoeffs[k].g = std::tan(sectionWd); dpCoeffs[k].R = 1.0f / (2.0f * q);
        dpCoeffs[k].h = 1.0f / (1.0f + 2.0f * dpCoeffs[k].R * dpCoeffs[k].g + dpCoeffs[k].g * dpCoeffs[k].g);
    }
}

void TptFilter::updateZPlaneCoeffs(float fc, float res) {
    float maxSafeFreq = (float)sampleRate * 0.45f;
    // 【修正】std::mt19random_device のタイポを std::random_device に完全修正
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    float zplaneMorphSpeed = juce::jmap(res, 0.1f, 10.0f, 0.0001f, 0.05f);

    for (int k = 0; k < 7; ++k) {
        if (std::abs(fc - lastCutoff) > 500.0f) {
            zplaneTargetZ[k][0] = dis(gen) * 0.9f;
            zplaneTargetZ[k][1] = dis(gen) * 0.9f;
        }
        zplaneCurrentZ[k][0] += (zplaneTargetZ[k][0] - zplaneCurrentZ[k][0]) * zplaneMorphSpeed;
        zplaneCurrentZ[k][1] += (zplaneTargetZ[k][1] - zplaneCurrentZ[k][1]) * zplaneMorphSpeed;

        float real_p = zplaneCurrentZ[k][0]; float imag_p = zplaneCurrentZ[k][1];
        float wn2 = real_p * real_p + imag_p * imag_p;
        float freqScale = std::sqrt(wn2) + 0.1f;
        float q = std::max(0.5f, std::min(20.0f, std::sqrt(wn2) / (-2.0f * real_p + 1e-5f)));

        float sectionFreq = std::clamp(fc * freqScale, 20.0f, maxSafeFreq);
        float sectionWd = juce::MathConstants<float>::pi * sectionFreq / (float)sampleRate;
        zplaneCoeffs[k].g = std::tan(sectionWd); zplaneCoeffs[k].R = 1.0f / (2.0f * q);
        zplaneCoeffs[k].h = 1.0f / (1.0f + 2.0f * zplaneCoeffs[k].R * zplaneCoeffs[k].g + zplaneCoeffs[k].g * zplaneCoeffs[k].g);
    }
}

void TptFilter::updateCoefficients()
{
    float currentCutoff = cutoff.getNextValue();
    float currentRes = resonance.getNextValue();

    float smoothCoef = 0.05f;
    smoothedDigitalCutoff = (1.0f - smoothCoef) * smoothedDigitalCutoff + smoothCoef * currentCutoff;

    if (std::abs(smoothedDigitalCutoff - lastCutoff) > 0.01f || std::abs(currentRes - lastRes) > 0.001f) {

        if (filterModel == 5) {
            float v = juce::jlimit(0.0f, 4.0f, juce::jmap(std::log10(smoothedDigitalCutoff), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
            int idx = (int)v; float frac = v - idx; if (idx >= 4) { idx = 3; frac = 1.0f; }
            float f1_map[5] = { 730.f, 270.f, 300.f, 530.f, 400.f }; float f2_map[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f }; float f3_map[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
            float f_arr[3] = { f1_map[idx] + (f1_map[idx + 1] - f1_map[idx]) * frac, f2_map[idx] + (f2_map[idx + 1] - f2_map[idx]) * frac, f3_map[idx] + (f3_map[idx + 1] - f3_map[idx]) * frac };
            for (int f = 0; f < 3; ++f) {
                float wd = juce::MathConstants<float>::pi * f_arr[f] / (float)sampleRate; form_g[f] = std::tan(wd); form_R[f] = 1.0f / (2.0f * currentRes); form_h[f] = 1.0f / (1.0f + 2.0f * form_R[f] * form_g[f] + form_g[f] * form_g[f]);
            }
        }
        else if (filterModel >= 17 && filterModel <= 20) {
            calcDigitalPrecisionCoeffs(smoothedDigitalCutoff, currentRes);
        }
        else if (filterModel == 22) {
            float baseFreq = std::clamp(smoothedDigitalCutoff, 20.0f, (float)sampleRate * 0.45f);
            float inharmonicity = juce::jmap(currentRes, 0.1f, 10.0f, 1.0f, 2.5f);
            for (int b = 0; b < numModalBands; ++b) {
                float bFreq = std::clamp(baseFreq * std::pow((float)(b + 1), inharmonicity), 20.0f, (float)sampleRate * 0.45f);
                float wd = juce::MathConstants<float>::pi * bFreq / (float)sampleRate;
                modal_g[b] = std::tan(wd);
                float q = 50.0f / std::sqrt((float)(b + 1));
                modal_R[b] = 1.0f / (2.0f * q);
                modal_h[b] = 1.0f / (1.0f + 2.0f * modal_R[b] * modal_g[b] + modal_g[b] * modal_g[b]);
            }
        }
        else if (filterModel == 25) {
            updateZPlaneCoeffs(smoothedDigitalCutoff, currentRes);
        }
        else {
            float maxSafeFreq = (float)sampleRate * 0.45f;
            float safeCutoff = std::clamp(smoothedDigitalCutoff, 20.0f, maxSafeFreq);
            float wd = juce::MathConstants<float>::pi * safeCutoff / (float)sampleRate;
            g = std::tan(wd); ladderG = g / (1.0f + g);

            if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 6 || filterModel == 7 || filterModel == 9 || filterModel == 11 || filterModel == 14 || filterModel == 16 || filterModel == 21 || filterModel == 23 || filterModel == 26 || filterModel == 27) {
                float adjustedRes = currentRes * ((filterModel != 7 && filterModel != 11 && filterModel != 14 && filterModel != 21) ? scalerSVF : 1.0f);
                R = 1.0f / (2.0f * adjustedRes); h = 1.0f / (1.0f + 2.0f * R * g + g * g);
            }
            else if (filterModel == 1 || filterModel == 12 || filterModel == 13 || filterModel == 15) {
                float r_scale = (filterModel == 13) ? 5.0f : 4.0f;
                ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, r_scale) * scalerMoog;
            }
            else if (filterModel == 2) {
                ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, 15.0f) * scalerDiode;
            }
        }
        lastCutoff = smoothedDigitalCutoff; lastRes = currentRes;
    }
}

void TptFilter::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    float* writePointers[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch) writePointers[ch] = buffer.getWritePointer(ch);
    for (int i = 0; i < numSamples; ++i) {
        updateCoefficients();
        for (int ch = 0; ch < numChannels; ++ch) writePointers[ch][i] = processSample(ch, writePointers[ch][i]);
    }
}

float TptFilter::processSample(int channel, float x)
{
    float comp = 1.0f;
    if (filterModel == 0 || filterModel == 4 || filterModel == 5 || filterModel == 6 || filterModel == 9 || filterModel == 11 || filterModel == 16 || filterModel == 26) comp = 1.0f + resonance.getCurrentValue() * 0.1f;
    else if (filterModel == 1 || filterModel == 12 || filterModel == 13 || filterModel == 15) comp = 1.0f + 0.5f * ladderRes;
    else if (filterModel == 2) comp = 1.0f + 0.2f * ladderRes;
    else if (filterModel == 7) comp = 1.0f + resonance.getCurrentValue() * 0.15f;
    x *= comp;

    if (filterModel == 4) {
        float targetSR = cutoff.getCurrentValue(); float phaseInc = targetSR / sampleRate; srrPhase[channel] += phaseInc;
        if (srrPhase[channel] >= 1.0f) {
            srrPhase[channel] -= std::floor(srrPhase[channel]); float bits = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 16.0f, 2.0f);
            float steps = std::pow(2.0f, bits); srrHeld[channel] = std::round(x * steps) / steps;
        }
        x = srrHeld[channel];
    }

    float envCoefIn = 0.005f;
    rmsIn[channel] = (1.0f - envCoefIn) * rmsIn[channel] + envCoefIn * (x * x);
    float out = x;

    if (filterModel == 0 || filterModel == 4) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp; s2[stage][channel] = g * bp + lp;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 1 || filterModel == 12 || filterModel == 13 || filterModel == 15) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float s1_ = zdfState[stage][channel][0]; float s2_ = zdfState[stage][channel][1]; float s3_ = zdfState[stage][channel][2]; float s4_ = zdfState[stage][channel][3];
            float S1 = s1_ / (1.0f + g); float S2 = s2_ / (1.0f + g); float S3 = s3_ / (1.0f + g); float S4 = s4_ / (1.0f + g);
            float sigma = ladderG * ladderG * ladderG * S1 + ladderG * ladderG * S2 + ladderG * S3 + S4;
            float u = out - ladderRes * sigma;
            if (filterModel == 1) u = std::tanh(u / (1.0f + ladderRes * ladderG * ladderG * ladderG * ladderG));
            else if (filterModel == 12) u = std::tanh(u * 1.1f) / 1.1f;
            else if (filterModel == 13) u = std::tanh(u * 1.5f) / 1.5f;
            else if (filterModel == 15) u = u / (1.0f + std::abs(u * 0.5f));
            float v1 = (u - s1_) * ladderG; float y1 = v1 + s1_; if (filterModel == 15) y1 = std::tanh(y1); zdfState[stage][channel][0] = s1_ + 2.0f * v1;
            float v2 = (y1 - s2_) * ladderG; float y2 = v2 + s2_; if (filterModel == 15) y2 = std::tanh(y2); zdfState[stage][channel][1] = s2_ + 2.0f * v2;
            float v3 = (y2 - s3_) * ladderG; float y3 = v3 + s3_; if (filterModel == 15) y3 = std::tanh(y3); zdfState[stage][channel][2] = s3_ + 2.0f * v3;
            float v4 = (y3 - s4_) * ladderG; float y4 = v4 + s4_; if (filterModel == 15) y4 = std::tanh(y4); zdfState[stage][channel][3] = s4_ + 2.0f * v4;
            if (slopeIdx == 0) { if (filterType == 0) out = y2; else if (filterType == 1) out = 2.0f * (y1 - y2); else if (filterType == 2) out = out - 2.0f * y1 + y2; else out = y2 + (out - 2.0f * y1 + y2); }
            else { if (filterType == 0) out = y4; else if (filterType == 1) out = 4.0f * (y2 - y4); else if (filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4; else out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4); }
        }
    }
    else if (filterModel == 2) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float fb = std::tanh(out - ladderRes * zdfState[stage][channel][3]);
            float G_h = ladderG * 0.5f;
            float v1 = (fb - zdfState[stage][channel][0]) * ladderG; float y1 = v1 + zdfState[stage][channel][0]; zdfState[stage][channel][0] = y1 + v1 - G_h * zdfState[stage][channel][1]; y1 = std::tanh(y1);
            float v2 = (y1 - zdfState[stage][channel][1]) * ladderG; float y2 = v2 + zdfState[stage][channel][1]; zdfState[stage][channel][1] = y2 + v2 - G_h * zdfState[stage][channel][2]; y2 = std::tanh(y2);
            float v3 = (y2 - zdfState[stage][channel][2]) * ladderG; float y3 = v3 + zdfState[stage][channel][2]; zdfState[stage][channel][2] = y3 + v3 - G_h * zdfState[stage][channel][3]; y3 = std::tanh(y3);
            float v4 = (y3 - zdfState[stage][channel][3]) * ladderG; float y4 = v4 + zdfState[stage][channel][3]; zdfState[stage][channel][3] = y4 + v4;
            if (y4 > 0.0f) y4 *= 1.2f; y4 = std::tanh(y4);
            if (slopeIdx == 0) { if (filterType == 0) out = y2; else if (filterType == 1) out = y1 - y2; else if (filterType == 2) out = out - y1; else out = y2 + (out - y1); }
            else { if (filterType == 0) out = y4; else if (filterType == 1) out = y2 - y4; else if (filterType == 2) out = out - y2; else out = y4 + (out - y2); }
        }
    }
    else if (filterModel == 3 || filterModel == 14) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float drivenOut = (filterModel == 14) ? out : std::tanh(out * 1.2f);
            float hp = (drivenOut - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            if (filterModel == 14) { s1[stage][channel] = g * hp + bp; s2[stage][channel] = g * bp + lp; }
            else { s1[stage][channel] = std::tanh(g * hp + bp); s2[stage][channel] = std::tanh(g * bp + lp); }
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 5) {
        float out_formant = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
        for (int stage = 0; stage < currentStages; ++stage) {
            float stage_in = out; out_formant = 0.0f;
            for (int f = 0; f < 3; ++f) {
                float hp = (stage_in - (2.0f * form_R[f] + form_g[f]) * form_s1[f][channel] - form_s2[f][channel]) * form_h[f];
                float bp = form_g[f] * hp + form_s1[f][channel]; float lp = form_g[f] * bp + form_s2[f][channel];
                form_s1[f][channel] = form_g[f] * hp + bp; form_s2[f][channel] = form_g[f] * bp + lp;
                float bandOut = bp; if (filterType == 0) bandOut = lp; else if (filterType == 1) bandOut = bp; else if (filterType == 2) bandOut = hp; else bandOut = lp + hp;
                out_formant += bandOut * gains[f];
            }
            out = out_formant;
        }
    }
    else if (filterModel == 6) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float delaySamples = sampleRate / juce::jlimit(20.0f, 20000.0f, cutoff.getCurrentValue());
            int dInt = (int)delaySamples; float dFrac = delaySamples - dInt;
            int readIdx1 = combWriteIdx[stage][channel] - dInt; if (readIdx1 < 0) readIdx1 += 4096;
            int readIdx2 = readIdx1 - 1; if (readIdx2 < 0) readIdx2 += 4096;
            float delayed = combBuffer[stage][channel][readIdx1] + dFrac * (combBuffer[stage][channel][readIdx2] - combBuffer[stage][channel][readIdx1]);
            float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f); if (filterType == 1 || filterType == 3) fb = -fb;
            float combOut = 0.0f;
            if (filterType == 0 || filterType == 1) { float toBuffer = std::tanh(out + delayed * fb); combBuffer[stage][channel][combWriteIdx[stage][channel]] = toBuffer; combOut = toBuffer; }
            else { combBuffer[stage][channel][combWriteIdx[stage][channel]] = out; combOut = out + delayed * fb; }
            combWriteIdx[stage][channel] = (combWriteIdx[stage][channel] + 1) % 4096; out = combOut;
        }
    }
    else if (filterModel == 7) {
        float ms_k = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 2.5f);
        for (int stage = 0; stage < currentStages; ++stage) {
            float fb = sk_s2[stage][channel] * ms_k; if (fb > 0.0f) fb = std::tanh(fb * 1.5f); else fb = std::tanh(fb * 0.8f);
            float v1 = (out - fb - sk_s1[stage][channel]) * g / (1.0f + g); float y1 = v1 + sk_s1[stage][channel]; sk_s1[stage][channel] = y1 + v1;
            float v2 = (y1 - sk_s2[stage][channel]) * g / (1.0f + g); float y2 = v2 + sk_s2[stage][channel]; sk_s2[stage][channel] = y2 + v2;
            float lp = y2; float hp = out - y1; float bp = y1 - y2;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 8) {
        float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f); if (filterType == 1 || filterType == 3) fb = -fb;
        float in_ap = std::tanh(out + fb * ap_out_prev[channel]);
        for (int stage = 0; stage < currentStages; ++stage) {
            float v = (in_ap - ap_s[stage][channel]) * ladderG; float lp = v + ap_s[stage][channel];
            ap_s[stage][channel] = lp + v; in_ap = 2.0f * lp - in_ap;
        }
        ap_out_prev[channel] = in_ap; out = (out + in_ap) * 0.5f;
    }
    else if (filterModel == 9) {
        float fold_gain = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 1.0f, 10.0f);
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp; s2[stage][channel] = g * bp + lp;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
        out = std::sin(out * fold_gain);
    }
    else if (filterModel == 10) {
        float ms = juce::jmap(cutoff.getCurrentValue(), 20.0f, 20000.0f, 50.0f, 0.5f);
        float baseSamples = (ms / 1000.0f) * sampleRate;
        float d1 = juce::jlimit(1.0f, 4095.0f, baseSamples * 1.000f); float d2 = juce::jlimit(1.0f, 4095.0f, baseSamples * 1.313f); float d_ap = juce::jlimit(1.0f, 4095.0f, baseSamples * 0.471f);
        float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f);
        auto readDelay = [&](int stage, float delay) { int dInt = (int)delay; float dFrac = delay - dInt; int r1 = combWriteIdx[stage][channel] - dInt; if (r1 < 0) r1 += 4096; int r2 = r1 - 1; if (r2 < 0) r2 += 4096; return combBuffer[stage][channel][r1] + dFrac * (combBuffer[stage][channel][r2] - combBuffer[stage][channel][r1]); };
        float c1_out = readDelay(0, d1); float c2_out = readDelay(1, d2);
        combBuffer[0][channel][combWriteIdx[0][channel]] = std::tanh(out + c1_out * fb); combBuffer[1][channel][combWriteIdx[1][channel]] = std::tanh(out + c2_out * fb);
        float mixed = (c1_out + c2_out) * 0.707f;
        float ap_out = readDelay(2, d_ap);
        combBuffer[2][channel][combWriteIdx[2][channel]] = std::tanh(mixed + ap_out * 0.5f); float reverb_out = ap_out - mixed * 0.5f;
        for (int s = 0; s < 3; ++s) combWriteIdx[s][channel] = (combWriteIdx[s][channel] + 1) % 4096;
        if (filterType == 0) out = reverb_out; else if (filterType == 1) out = (out + reverb_out) * 0.5f; else if (filterType == 2) out = out - reverb_out * 0.5f; else out = std::sin(reverb_out * 3.0f);
    }
    else if (filterModel == 11) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp; s2[stage][channel] = g * bp + lp;
            out = lp - 2.0f * R * bp + hp;
        }
    }
    else if (filterModel == 16) {
        auto waspClip = [](float v) { return v / (1.0f + std::abs(v * 2.5f)); };
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel]; float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = waspClip(g * hp + bp); s2[stage][channel] = waspClip(g * bp + lp);
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
        out = std::clamp(out * 1.5f, -1.0f, 1.0f);
    }
    else if (filterModel >= 17 && filterModel <= 20) {
        for (int stage = 0; stage < currentStages; ++stage) {
            float cur_g = dpCoeffs[stage].g; float cur_R = dpCoeffs[stage].R; float cur_h = dpCoeffs[stage].h;
            float hp = (out - (2.0f * cur_R + cur_g) * dp_s1[stage][channel] - dp_s2[stage][channel]) * cur_h;
            float bp = cur_g * hp + dp_s1[stage][channel]; float lp = cur_g * bp + dp_s2[stage][channel];
            dp_s1[stage][channel] = cur_g * hp + bp; dp_s2[stage][channel] = cur_g * bp + lp;
            if (filterModel == 20) { out = lp + hp * 0.5f; }
            else { if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp; }
        }
    }
    else if (filterModel == 21) {
        float currentTrigger = smoothedDigitalCutoff / 20000.0f;
        if (currentTrigger > lpgTriggerState[channel] + 0.05f) { lpgEnv[channel] += 0.5f; if (lpgEnv[channel] > 1.0f) lpgEnv[channel] = 1.0f; }
        lpgTriggerState[channel] = currentTrigger;
        float decay = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0005f, 0.00001f);
        lpgEnv[channel] *= (1.0f - decay);
        float lpgFc = juce::jlimit(20.0f, 15000.0f, 20.0f * std::pow(1000.0f, lpgEnv[channel]));
        float lpgWd = juce::MathConstants<float>::pi * lpgFc / (float)sampleRate;
        float lpgG = std::tan(lpgWd); float lpgR = 1.0f / (2.0f * 0.707f); float lpgH = 1.0f / (1.0f + 2.0f * lpgR * lpgG + lpgG * lpgG);
        float hp = (out - (2.0f * lpgR + lpgG) * s1[0][channel] - s2[0][channel]) * lpgH;
        float bp = lpgG * hp + s1[0][channel]; float lp = lpgG * bp + s2[0][channel];
        s1[0][channel] = lpgG * hp + bp; s2[0][channel] = lpgG * bp + lp;
        out = lp * (lpgEnv[channel] * lpgEnv[channel]);
    }
    else if (filterModel == 22) {
        float modalMix = 0.0f;
        for (int b = 0; b < numModalBands; ++b) {
            float hp = (out - (2.0f * modal_R[b] + modal_g[b]) * modal_s1[b][channel] - modal_s2[b][channel]) * modal_h[b];
            float bp = modal_g[b] * hp + modal_s1[b][channel]; float lp = modal_g[b] * bp + modal_s2[b][channel];
            modal_s1[b][channel] = modal_g[b] * hp + bp; modal_s2[b][channel] = modal_g[b] * bp + lp;
            modalMix += bp * (1.0f / std::sqrt((float)(b + 1)));
        }
        if (filterType == 0) out = modalMix; else if (filterType == 1) out = out * 0.5f + modalMix; else if (filterType == 2) out = out * 0.5f - modalMix; else out = std::tanh(modalMix * 2.0f);
    }
    else if (filterModel == 23) {
        float delaySamples = sampleRate / juce::jlimit(20.0f, 20000.0f, smoothedDigitalCutoff);
        int dInt = (int)delaySamples; float dFrac = delaySamples - dInt;
        int r1 = wgWriteIdx[channel] - dInt; if (r1 < 0) r1 += 4096;
        int r2 = r1 - 1; if (r2 < 0) r2 += 4096;
        float wgDelayed = wgBuffer[channel][r1] + dFrac * (wgBuffer[channel][r2] - wgBuffer[channel][r1]);
        float hp = (wgDelayed - (2.0f * R + g) * s1[0][channel] - s2[0][channel]) * h;
        float bp = g * hp + s1[0][channel]; float wgScattered = g * bp + s2[0][channel];
        s1[0][channel] = g * hp + bp; s2[0][channel] = g * bp + wgScattered;
        float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.99f);
        float excitation = out + wgScattered * fb;
        wgBuffer[channel][wgWriteIdx[channel]] = std::tanh(excitation);
        wgWriteIdx[channel] = (wgWriteIdx[channel] + 1) % 4096;
        if (filterType == 0) out = wgScattered; else out = out * 0.5f + wgScattered * 0.5f;
    }
    else if (filterModel == 24) {
        float in_A = out; float in_B = out;
        for (int i = 0; i < 4; ++i) {
            float vA = (in_A - hilbertStateA[i][channel]) * hilbertCoeffsA[i];
            float outA = vA + hilbertStateA[i][channel]; hilbertStateA[i][channel] = outA + vA; in_A = 2.0f * outA - in_A;
            float vB = (in_B - hilbertStateB[i][channel]) * hilbertCoeffsB[i];
            float outB = vB + hilbertStateB[i][channel]; hilbertStateB[i][channel] = outB + vB; in_B = 2.0f * outB - in_B;
        }
        float shiftHz = juce::jmap(smoothedDigitalCutoff, 20.0f, 20000.0f, -1000.0f, 1000.0f);
        float phaseInc = juce::MathConstants<float>::twoPi * shiftHz / (float)sampleRate;
        bodePhase[channel] += phaseInc;
        if (bodePhase[channel] > juce::MathConstants<float>::twoPi) bodePhase[channel] -= juce::MathConstants<float>::twoPi;
        if (bodePhase[channel] < 0.0f) bodePhase[channel] += juce::MathConstants<float>::twoPi;
        float oscCos = std::cos(bodePhase[channel]); float oscSin = std::sin(bodePhase[channel]);
        float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.95f);
        float shifted = 0.0f;
        if (filterType == 0 || filterType == 2) shifted = in_A * oscCos - in_B * oscSin;
        else shifted = in_A * oscCos + in_B * oscSin;
        out = std::tanh(shifted * (1.0f + fb));
    }
    else if (filterModel == 25) {
        for (int stage = 0; stage < 7; ++stage) {
            float cur_g = zplaneCoeffs[stage].g; float cur_R = zplaneCoeffs[stage].R; float cur_h = zplaneCoeffs[stage].h;
            float hp = (out - (2.0f * cur_R + cur_g) * zplane_s1[stage][channel] - zplane_s2[stage][channel]) * cur_h;
            float bp = cur_g * hp + zplane_s1[stage][channel]; float lp = cur_g * bp + zplane_s2[stage][channel];
            zplane_s1[stage][channel] = cur_g * hp + bp; zplane_s2[stage][channel] = cur_g * bp + lp;
            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
        }
    }
    else if (filterModel == 26) {
        float mixedPhase = 0.0f; float in_ap = out;
        for (int stage = 0; stage < currentStages; ++stage) {
            float fb = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 0.0f, 0.8f);
            if (stage % 2 != 0) fb = -fb;
            float v = (in_ap - pa_s[stage][channel]) * ladderG; float lp = v + pa_s[stage][channel];
            pa_s[stage][channel] = lp + v; float stage_out = 2.0f * lp - in_ap;
            mixedPhase += stage_out * ((float)(stage + 1) / (float)currentStages);
            in_ap = std::tanh(stage_out * 1.1f);
        }
        if (filterType == 0) out = (out + mixedPhase) * 0.3f; else out = mixedPhase * 0.3f;
    }
    else if (filterModel == 27) {
        float aa_g = g; float aa_R = 1.0f / (2.0f * 1.5f); float aa_h = 1.0f / (1.0f + 2.0f * aa_R * aa_g + aa_g * aa_g);
        for (int stage = 0; stage < 4; ++stage) {
            float hp = (out - (2.0f * aa_R + aa_g) * aa_s1[stage][channel] - aa_s2[stage][channel]) * aa_h;
            float bp = aa_g * hp + aa_s1[stage][channel]; float lp = aa_g * bp + aa_s2[stage][channel];
            aa_s1[stage][channel] = aa_g * hp + bp; aa_s2[stage][channel] = aa_g * bp + lp;
            out = lp;
        }
    }

    float envCoefOut = 0.005f;
    rmsOut[channel] = (1.0f - envCoefOut) * rmsOut[channel] + envCoefOut * (out * out);

    float targetGain = 1.0f;
    if (rmsOut[channel] > 1e-8f) {
        targetGain = std::sqrt((rmsIn[channel] + 1e-8f) / (rmsOut[channel] + 1e-8f));
        targetGain = juce::jlimit(0.1f, 15.0f, targetGain);
    }
    agcGain[channel] = (1.0f - 0.0005f) * agcGain[channel] + 0.0005f * targetGain;

    return out * agcGain[channel];
}

float TptFilter::getMagnitudeForFrequency(float frequency) const
{
    float fc = cutoff.getCurrentValue();
    float res = resonance.getCurrentValue();
    float w = frequency / juce::jlimit(20.0f, 20000.0f, fc);
    float w2 = w * w;
    float mag = 1.0f;

    if (filterModel == 0 || filterModel == 3 || filterModel == 4 || filterModel == 9 || filterModel == 14 || filterModel == 16) {
        float adjustedRes = res * ((filterModel != 14) ? scalerSVF : 1.0f);
        float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, currentStages);
    }
    else if (filterModel >= 17 && filterModel <= 20) {
        float mag_total = 1.0f;
        for (int k = 0; k < currentStages; ++k) {
            float cur_g = dpCoeffs[k].g; float cur_R = dpCoeffs[k].R;
            float stage_fc = std::atan(cur_g) * sampleRate / juce::MathConstants<float>::pi;
            float stage_w = frequency / juce::jlimit(20.0f, 20000.0f, stage_fc);
            float stage_w2 = stage_w * stage_w; float stage_d = 2.0f * cur_R;
            float den = std::sqrt(std::pow(1.0f - stage_w2, 2.0f) + std::pow(stage_w * stage_d, 2.0f));
            float m = 1.0f / den;
            if (filterModel == 20) { m = std::abs(1.0f - stage_w2 * 0.5f) / den; }
            else { if (filterType == 1) m *= stage_w; else if (filterType == 2) m *= stage_w2; else if (filterType == 3) m *= std::abs(1.0f - stage_w2); }
            mag_total *= m;
        }
        return mag_total;
    }
    else if (filterModel == 11) {
        float d = 1.0f / juce::jlimit(0.1f, 10.0f, res);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        float bp_mag = (1.0f / den) * w;
        mag = 1.0f + std::pow(bp_mag, 1.2f) * res * ((float)currentStages * 0.1f);
        return mag;
    }
    else if (filterModel == 10) {
        float ms = juce::jmap(fc, 20.0f, 20000.0f, 50.0f, 0.5f);
        float baseD = (ms / 1000.0f);
        float wD1 = 2.0f * juce::MathConstants<float>::pi * frequency * (baseD * 1.000f);
        float wD2 = 2.0f * juce::MathConstants<float>::pi * frequency * (baseD * 1.313f);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        float m1 = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD1));
        float m2 = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD2));
        mag = (m1 + m2) * 0.5f;
        return mag;
    }
    else if (filterModel == 5) {
        float v = juce::jlimit(0.0f, 4.0f, juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
        int idx = (int)v; float frac = v - idx; if (idx >= 4) { idx = 3; frac = 1.0f; }
        float f1_m[5] = { 730.f, 270.f, 300.f, 530.f, 400.f }; float f2_m[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f }; float f3_m[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
        float f_arr[3] = { f1_m[idx] + (f1_m[idx + 1] - f1_m[idx]) * frac, f2_m[idx] + (f2_m[idx + 1] - f2_m[idx]) * frac, f3_m[idx] + (f3_m[idx + 1] - f3_m[idx]) * frac };
        float mag_sum = 0.0f; float gains[3] = { 1.0f, 0.5f, 0.2f };
        for (int f = 0; f < 3; ++f) {
            float w_f = frequency / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
            float d_f = 1.0f / juce::jlimit(0.1f, 10.0f, res);
            float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
            if (filterType == 1) m_f *= w_f; else if (filterType == 2) m_f *= w_f * w_f; else if (filterType == 3) m_f *= std::abs(1.0f - w_f * w_f);
            mag_sum += m_f * gains[f];
        }
        return mag_sum;
    }
    else if (filterModel == 6) {
        float delaySamples = sampleRate / juce::jlimit(20.0f, 20000.0f, fc);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f); if (filterType == 1 || filterType == 3) fb = -fb;
        float wD = 2.0f * juce::MathConstants<float>::pi * frequency * (delaySamples / sampleRate);
        float m = (filterType == 0 || filterType == 1) ? (1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD))) : std::sqrt(1.0f + fb * fb + 2.0f * fb * std::cos(wD));
        return std::pow(m, currentStages);
    }
    else if (filterModel == 7) {
        float ms_res = juce::jmap(res, 0.1f, 10.0f, 0.0f, 2.5f);
        float d = 1.0f / (ms_res + 0.1f);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, currentStages);
    }
    else if (filterModel == 8) {
        float phi = -2.0f * std::atan(frequency / juce::jlimit(20.0f, 20000.0f, fc));
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        if (filterType == 1 || filterType == 3) fb = -fb;
        float c_ap = std::cos(currentStages * phi);
        float s_ap = std::sin(currentStages * phi);
        float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
        float real_yap = (c_ap - fb) / den2;
        float imag_yap = s_ap / den2;
        float real_out = 0.5f * (1.0f + real_yap);
        float imag_out = 0.5f * imag_yap;
        return std::sqrt(real_out * real_out + imag_out * imag_out);
    }
    else if (filterModel == 21) {
        float lpgFc = juce::jlimit(20.0f, 15000.0f, 20.0f * std::pow(1000.0f, lpgEnv[0]));
        float d = 1.0f / 0.707f; float w_lpg = frequency / lpgFc;
        mag = (1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f) + std::pow(w_lpg * d, 2.0f))) * lpgEnv[0];
        return mag;
    }
    else if (filterModel == 22) {
        float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
        float mag_sum = 0.0f;
        for (int b = 0; b < numModalBands; ++b) {
            float bFreq = std::clamp(fc * std::pow((float)(b + 1), inharmonicity), 20.0f, (float)sampleRate * 0.45f);
            float stage_w = frequency / bFreq; float q = 50.0f / std::sqrt((float)(b + 1)); float d = 1.0f / q;
            mag_sum += (1.0f / std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f) + std::pow(stage_w * d, 2.0f))) * stage_w * (1.0f / std::sqrt((float)(b + 1)));
        }
        return mag_sum;
    }
    else if (filterModel == 24) {
        float shiftHz = juce::jmap(fc, 20.0f, 20000.0f, -1000.0f, 1000.0f);
        if (std::abs(frequency - (1000.0f + shiftHz)) < 100.0f) return 5.0f;
        return 1.0f;
    }
    else if (filterModel == 25) {
        float mag_total = 1.0f;
        for (int k = 0; k < 7; ++k) {
            float cur_g = zplaneCoeffs[k].g; float cur_R = zplaneCoeffs[k].R;
            float stage_fc = std::atan(cur_g) * sampleRate / juce::MathConstants<float>::pi;
            float stage_w = frequency / juce::jlimit(20.0f, 20000.0f, stage_fc);
            float den = std::sqrt(std::pow(1.0f - stage_w * stage_w, 2.0f) + std::pow(stage_w * 2.0f * cur_R, 2.0f));
            mag_total *= (1.0f / den);
        }
        return mag_total;
    }
    else if (filterModel == 26) {
        float phi = -2.0f * std::atan(w);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.8f);
        float c_ap = std::cos(currentStages * phi); float s_ap = std::sin(currentStages * phi);
        float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
        float real_yap = (c_ap - fb) / den2; float imag_yap = s_ap / den2;
        return std::sqrt(real_yap * real_yap + imag_yap * imag_yap);
    }
    else if (filterModel == 27) {
        float d = 1.0f / 1.5f;
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        return std::pow(1.0f / den, 4.0f);
    }
    else {
        float r_scale = (filterModel == 13) ? 5.0f : (filterModel == 2) ? 15.0f : 4.0f;
        float r_val = juce::jmap(res, 0.1f, 10.0f, 0.0f, r_scale) * ((filterModel == 2) ? scalerDiode : scalerMoog);
        float real_p = std::pow(1.0f - w2, 2.0f) - ((filterModel == 2) ? 3.5f : 4.0f) * w2 + r_val;
        float imag_p = ((filterModel == 2) ? 3.5f : 4.0f) * w * (1.0f - w2);
        mag = 1.0f / std::sqrt(real_p * real_p + imag_p * imag_p);
        if (slopeIdx == 0) {
            if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        }
        else {
            if (filterModel == 1 || filterModel == 12 || filterModel == 13 || filterModel == 15) {
                if (filterType == 1) mag *= w2; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w2);
            }
            else {
                if (filterType == 1) mag *= w2 * w; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w);
            }
        }
        return std::pow(mag, currentStages);
    }
}