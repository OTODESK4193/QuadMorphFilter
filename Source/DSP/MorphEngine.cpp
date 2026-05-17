// ==========================================
// DSP/MorphEngine.cpp
// ==========================================
#include "MorphEngine.h"
#include <cmath>

std::array<float, 4> MorphEngine::computeEqualPowerWMix(float x, float y)
{
    const float halfPi = juce::MathConstants<float>::halfPi;
    float angleX = x * halfPi;
    float angleY = y * halfPi;

    float cosX = std::cos(angleX);
    float sinX = std::sin(angleX);
    float cosY = std::cos(angleY);
    float sinY = std::sin(angleY);

    return { cosX * cosY,
             sinX * cosY,
             cosX * sinY,
             sinX * sinY };
}

std::array<float, 4> MorphEngine::computeModulation(
    juce::Point<float>          pos,
    const std::array<float, 4>& mod4,
    bool                        isRand1)
{
    std::array<float, 4> result;

    if (isRand1)
    {
        for (int i = 0; i < 4; ++i)
            result[i] = mod4[i] * 2.0f - 1.0f;
    }
    else
    {
        result[0] = pos.x * 2.0f - 1.0f;
        result[1] = pos.y * 2.0f - 1.0f;
        result[2] = (1.0f - pos.x) * 2.0f - 1.0f;
        result[3] = (1.0f - pos.y) * 2.0f - 1.0f;
    }

    return result;
}

float MorphEngine::applyFrequencyMod(float baseFreq, float modValue)
{
    return baseFreq * std::pow(2.0f, 4.0f * modValue);
}

float MorphEngine::applyResonanceMod(float baseRes, float modValue)
{
    return baseRes * std::pow(2.0f, 2.0f * modValue);
}