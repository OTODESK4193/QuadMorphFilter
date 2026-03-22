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
    for (int i = 0; i < 2; ++i) { s1[i] = 0.0f; s2[i] = 0.0f; }
}

void TptFilter::setCutoff(float newCutoff) { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff)); }
void TptFilter::setResonance(float newResonance) { resonance.setTargetValue(juce::jmax(0.1f, newResonance)); }
void TptFilter::setType(int newType) { filterType = newType; }

void TptFilter::updateCoefficients()
{
    float currentCutoff = cutoff.getNextValue();
    float currentRes = resonance.getNextValue();
    float wd = juce::MathConstants<float>::pi * currentCutoff / (float)sampleRate;
    g = std::tan(wd);
    R = 1.0f / (2.0f * currentRes);
    h = 1.0f / (1.0f + 2.0f * R * g + g * g);
}

void TptFilter::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);

    // 【最適化】ポインタをループ外で取得し、アクセスオーバーヘッドを削減
    const float* readPointers[2] = { nullptr, nullptr };
    float* writePointers[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch) {
        readPointers[ch] = buffer.getReadPointer(ch);
        writePointers[ch] = buffer.getWritePointer(ch);
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // 係数の更新（SmoothedValueのステップを進めるためサンプル単位で実行）
        updateCoefficients();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float x = readPointers[ch][i];
            writePointers[ch][i] = processSample(ch, x);
        }
    }
}

float TptFilter::processSample(int channel, float x)
{
    float hp = (x - (2.0f * R + g) * s1[channel] - s2[channel]) * h;
    float bp = g * hp + s1[channel];
    float lp = g * bp + s2[channel];
    s1[channel] = g * hp + bp;
    s2[channel] = g * bp + lp;

    switch (filterType) {
    case 0: return lp; case 1: return bp; case 2: return hp; case 3: return lp + hp; default: return lp;
    }
}

float TptFilter::getMagnitudeForFrequency(float frequency) const
{
    float fc = cutoff.getCurrentValue();
    float res = resonance.getCurrentValue();
    float r = frequency / fc;
    float r2 = r * r;
    float damping = 1.0f / res;

    float denominator = std::sqrt(std::pow(1.0f - r2, 2.0f) + std::pow(r * damping, 2.0f));

    switch (filterType) {
    case 0: return 1.0f / denominator;                  // LP
    case 1: return r / denominator;                     // BP
    case 2: return r2 / denominator;                    // HP
    case 3: return std::abs(1.0f - r2) / denominator;   // Notch
    default: return 1.0f / denominator;
    }
}