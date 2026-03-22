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
        }
    }
}

void TptFilter::setCutoff(float newCutoff) { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff)); }
void TptFilter::setResonance(float newResonance) { resonance.setTargetValue(juce::jmax(0.1f, newResonance)); }
void TptFilter::setType(int newType) { filterType = newType; }

// 【追加】Slopeインデックスから内部の段数を設定
void TptFilter::setSlope(int slopeIndex)
{
    switch (slopeIndex) {
    case 0: currentStages = 1; break; // 12 dB/oct
    case 1: currentStages = 2; break; // 24 dB/oct
    case 2: currentStages = 4; break; // 48 dB/oct
    case 3: currentStages = 8; break; // 96 dB/oct
    default: currentStages = 1; break;
    }
}

void TptFilter::updateCoefficients()
{
    float currentCutoff = cutoff.getNextValue();
    float currentRes = resonance.getNextValue();

    // 【追加】多段カスケード時のレゾナンス発振を防ぐための安全補正（段数に応じてQ値をマイルドにする）
    float adjustedRes = currentRes;
    if (currentStages > 1) {
        adjustedRes = currentRes * std::pow(0.6f, std::log2((float)currentStages));
    }

    float wd = juce::MathConstants<float>::pi * currentCutoff / (float)sampleRate;
    g = std::tan(wd);
    R = 1.0f / (2.0f * adjustedRes);
    h = 1.0f / (1.0f + 2.0f * R * g + g * g);
}

void TptFilter::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);

    const float* readPointers[2] = { nullptr, nullptr };
    float* writePointers[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch) {
        readPointers[ch] = buffer.getReadPointer(ch);
        writePointers[ch] = buffer.getWritePointer(ch);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        updateCoefficients();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float x = readPointers[ch][i];
            writePointers[ch][i] = processSample(ch, x);
        }
    }
}

// 【修正】指定された段数(Slope)だけ直列にSVF処理を反復（カスケード）
float TptFilter::processSample(int channel, float x)
{
    float out = x;
    for (int stage = 0; stage < currentStages; ++stage) {
        float hp = (out - (2.0f * R + g) * s1[stage][channel] - s2[stage][channel]) * h;
        float bp = g * hp + s1[stage][channel];
        float lp = g * bp + s2[stage][channel];
        s1[stage][channel] = g * hp + bp;
        s2[stage][channel] = g * bp + lp;

        switch (filterType) {
        case 0: out = lp; break;
        case 1: out = bp; break;
        case 2: out = hp; break;
        case 3: out = lp + hp; break; // Notch
        default: out = lp; break;
        }
    }
    return out;
}

float TptFilter::getMagnitudeForFrequency(float frequency) const
{
    float fc = cutoff.getCurrentValue();
    float res = resonance.getCurrentValue();

    float adjustedRes = res;
    if (currentStages > 1) {
        adjustedRes = res * std::pow(0.6f, std::log2((float)currentStages));
    }

    float r = frequency / fc;
    float r2 = r * r;
    float damping = 1.0f / adjustedRes;

    float denominator = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * damping, 2.0f));
    float mag = 1.0f;

    switch (filterType) {
    case 0: mag = 1.0f / denominator; break;
    case 1: mag = r / denominator; break;
    case 2: mag = r2 / denominator; break;
    case 3: mag = std::abs(1.0f - r2) / denominator; break;
    default: mag = 1.0f / denominator; break;
    }

    // 【追加】Slope段数分だけマグニチュードを乗算し、急峻なカーブをVisualizerに反映
    return std::pow(mag, currentStages);
}