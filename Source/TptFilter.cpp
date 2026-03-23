// ==========================================
// TptFilter.cpp
// ==========================================
#include "TptFilter.h"
#include <cmath>

TptFilter::TptFilter()
{
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    cutoff.setCurrentAndTargetValue(1000.0f);
    resonance.setCurrentAndTargetValue(0.707f);
}

void TptFilter::prepare(double newSampleRate, int /*samplesPerBlock*/, int numChannels)
{
    sampleRate = newSampleRate;
    maxChannels = juce::jmin(numChannels, 2);
    cutoff.reset(sampleRate, 0.01);
    resonance.reset(sampleRate, 0.01);
    reset();
}

void TptFilter::reset()
{
    for (int stage = 0; stage < 8; ++stage) {
        for (int ch = 0; ch < 2; ++ch) {
            s1[stage][ch] = 0.0f;
            s2[stage][ch] = 0.0f;
            for (int pole = 0; pole < 4; ++pole) {
                zdfState[stage][ch][pole] = 0.0f;
            }
        }
    }
    for (int ch = 0; ch < 2; ++ch) {
        rmsIn[ch] = 0.0f;
        rmsOut[ch] = 0.0f;
        agcGain[ch] = 1.0f;
        srrPhase[ch] = 0.0f;
        srrHeld[ch] = 0.0f;
    }
}

void TptFilter::setModel(int newModel) { filterModel = newModel; }
void TptFilter::setCutoff(float newCutoff) { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff)); }
void TptFilter::setResonance(float newResonance) { resonance.setTargetValue(juce::jmax(0.1f, newResonance)); }
void TptFilter::setType(int newType) { filterType = newType; }

void TptFilter::setSlope(int index)
{
    slopeIdx = index;
    if (filterModel == 0 || filterModel == 3 || filterModel == 4) { // SVF base
        currentStages = (index == 0) ? 1 : (index == 1) ? 2 : (index == 2) ? 4 : 8;
    }
    else { // Ladder base
        currentStages = (index == 0) ? 1 : (index == 1) ? 1 : (index == 2) ? 2 : 4;
    }
}

void TptFilter::updateCoefficients()
{
    float currentCutoff = cutoff.getNextValue();
    float currentRes = resonance.getNextValue();

    float wd = juce::MathConstants<float>::pi * currentCutoff / (float)sampleRate;
    g = std::tan(wd);
    ladderG = g / (1.0f + g);

    if (filterModel == 0 || filterModel == 3 || filterModel == 4) {
        float adjustedRes = currentRes;
        if (currentStages > 1) adjustedRes = currentRes * std::pow(0.6f, std::log2((float)currentStages));
        R = 1.0f / (2.0f * adjustedRes);
        h = 1.0f / (1.0f + 2.0f * R * g + g * g);
    }
    else if (filterModel == 1) { // Moog
        ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, 4.0f);
        if (currentStages > 1) ladderRes *= std::pow(0.7f, std::log2((float)currentStages));
    }
    else if (filterModel == 2) { // Diode
        ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, 15.0f);
        if (currentStages > 1) ladderRes *= std::pow(0.7f, std::log2((float)currentStages));
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
        for (int ch = 0; ch < numChannels; ++ch) {
            writePointers[ch][i] = processSample(ch, writePointers[ch][i]);
        }
    }
}

float TptFilter::processSample(int channel, float x)
{
    float comp = 1.0f;
    if (filterModel == 0 || filterModel == 4) comp = 1.0f + resonance.getCurrentValue() * 0.1f;
    else if (filterModel == 1) comp = 1.0f + 0.5f * ladderRes;
    else if (filterModel == 2) comp = 1.0f + 0.2f * ladderRes;
    x *= comp;

    // 【追加】Bitcrusher / SRR プロセッシング (入力信号を先に破壊する)
    if (filterModel == 4) {
        float targetSR = cutoff.getCurrentValue();
        float phaseInc = targetSR / sampleRate;
        srrPhase[channel] += phaseInc;

        if (srrPhase[channel] >= 1.0f) {
            srrPhase[channel] -= std::floor(srrPhase[channel]);
            // Resonance (0.1 ~ 10.0) を Bit Depth (16bit ~ 2bit) へマッピング
            float bits = juce::jmap(resonance.getCurrentValue(), 0.1f, 10.0f, 16.0f, 2.0f);
            float steps = std::pow(2.0f, bits);
            srrHeld[channel] = std::round(x * steps) / steps;
        }
        x = srrHeld[channel];
    }

    float envCoefIn = 0.005f;
    rmsIn[channel] = (1.0f - envCoefIn) * rmsIn[channel] + envCoefIn * (x * x);

    float out = x;

    if (filterModel == 0 || filterModel == 4) { // Clean SVF & SRR(破壊後の信号をフィルタリング)
        for (int stage = 0; stage < currentStages; ++stage) {
            float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel];
            float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = g * hp + bp;
            s2[stage][channel] = g * bp + lp;

            if (filterType == 0) out = lp;
            else if (filterType == 1) out = bp;
            else if (filterType == 2) out = hp;
            else out = lp + hp;
        }
    }
    else if (filterModel == 1) { // Moog Ladder
        for (int stage = 0; stage < currentStages; ++stage) {
            float s1_ = zdfState[stage][channel][0]; float s2_ = zdfState[stage][channel][1];
            float s3_ = zdfState[stage][channel][2]; float s4_ = zdfState[stage][channel][3];

            float S1 = s1_ / (1.0f + g); float S2 = s2_ / (1.0f + g);
            float S3 = s3_ / (1.0f + g); float S4 = s4_ / (1.0f + g);

            float sigma = ladderG * ladderG * ladderG * S1 + ladderG * ladderG * S2 + ladderG * S3 + S4;
            float u = (out - ladderRes * sigma) / (1.0f + ladderRes * ladderG * ladderG * ladderG * ladderG);
            u = std::tanh(u);

            float v1 = (u - s1_) * ladderG; float y1 = v1 + s1_; zdfState[stage][channel][0] = s1_ + 2.0f * v1;
            float v2 = (y1 - s2_) * ladderG; float y2 = v2 + s2_; zdfState[stage][channel][1] = s2_ + 2.0f * v2;
            float v3 = (y2 - s3_) * ladderG; float y3 = v3 + s3_; zdfState[stage][channel][2] = s3_ + 2.0f * v3;
            float v4 = (y3 - s4_) * ladderG; float y4 = v4 + s4_; zdfState[stage][channel][3] = s4_ + 2.0f * v4;

            if (slopeIdx == 0) {
                if (filterType == 0) out = y2;
                else if (filterType == 1) out = 2.0f * (y1 - y2);
                else if (filterType == 2) out = out - 2.0f * y1 + y2;
                else out = y2 + (out - 2.0f * y1 + y2);
            }
            else {
                if (filterType == 0) out = y4;
                else if (filterType == 1) out = 4.0f * (y2 - y4);
                else if (filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
                else out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4);
            }
        }
    }
    else if (filterModel == 2) { // Diode Ladder
        for (int stage = 0; stage < currentStages; ++stage) {
            float fb = std::tanh(out - ladderRes * zdfState[stage][channel][3]);
            float v1 = (fb - zdfState[stage][channel][0]) * ladderG; float y1 = v1 + zdfState[stage][channel][0]; zdfState[stage][channel][0] = y1 + v1; y1 = std::tanh(y1);
            float v2 = (y1 - zdfState[stage][channel][1]) * ladderG; float y2 = v2 + zdfState[stage][channel][1]; zdfState[stage][channel][1] = y2 + v2; y2 = std::tanh(y2);
            float v3 = (y2 - zdfState[stage][channel][2]) * ladderG; float y3 = v3 + zdfState[stage][channel][2]; zdfState[stage][channel][2] = y3 + v3; y3 = std::tanh(y3);
            float v4 = (y3 - zdfState[stage][channel][3]) * ladderG; float y4 = v4 + zdfState[stage][channel][3]; zdfState[stage][channel][3] = y4 + v4;
            if (y4 > 0.0f) y4 *= 1.1f; y4 = std::tanh(y4);

            if (slopeIdx == 0) {
                if (filterType == 0) out = y2; else if (filterType == 1) out = y1 - y2; else if (filterType == 2) out = out - y1; else out = y2 + (out - y1);
            }
            else {
                if (filterType == 0) out = y4; else if (filterType == 1) out = y2 - y4; else if (filterType == 2) out = out - y2; else out = y4 + (out - y2);
            }
        }
    }
    else if (filterModel == 3) { // SEM
        for (int stage = 0; stage < currentStages; ++stage) {
            float drivenOut = std::tanh(out * 1.2f);
            float hp = (drivenOut - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
            float bp = g * hp + s1[stage][channel];
            float lp = g * bp + s2[stage][channel];
            s1[stage][channel] = std::tanh(g * hp + bp);
            s2[stage][channel] = std::tanh(g * bp + lp);

            if (filterType == 0) out = lp; else if (filterType == 1) out = bp; else if (filterType == 2) out = hp; else out = lp + hp;
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

    if (filterModel == 0 || filterModel == 3 || filterModel == 4) {
        float adjustedRes = res;
        if (currentStages > 1) adjustedRes = res * std::pow(0.6f, std::log2((float)currentStages));
        float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, currentStages);
    }
    else {
        float r_scale = (filterModel == 1) ? 4.0f : 15.0f;
        float r_val = juce::jmap(res, 0.1f, 10.0f, 0.0f, r_scale);
        if (currentStages > 1) r_val *= std::pow(0.7f, std::log2((float)currentStages));

        float real_p, imag_p;
        if (filterModel == 1) {
            real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_val;
            imag_p = 4.0f * w * (1.0f - w2);
        }
        else {
            real_p = std::pow(1.0f - w2, 2.0f) - 3.5f * w2 + r_val;
            imag_p = 3.5f * w * (1.0f - w2);
        }

        float den2 = real_p * real_p + imag_p * imag_p;
        mag = 1.0f / std::sqrt(den2);

        if (slopeIdx == 0) {
            if (filterType == 1) mag *= w; else if (filterType == 2) mag *= w2; else if (filterType == 3) mag *= std::abs(1.0f - w2);
        }
        else {
            if (filterModel == 1) {
                if (filterType == 1) mag *= w2; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w2);
            }
            else {
                if (filterType == 1) mag *= w2 * w; else if (filterType == 2) mag *= w2 * w2; else if (filterType == 3) mag *= std::abs(1.0f - w2 * w);
            }
        }
        return std::pow(mag, currentStages);
    }
}