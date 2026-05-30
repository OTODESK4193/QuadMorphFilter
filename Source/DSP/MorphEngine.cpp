// ==========================================
// DSP/MorphEngine.cpp
// ==========================================
#include "MorphEngine.h"
#include <cmath>

// =============================================
// ブレンドアルゴリズム実装
// =============================================

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

std::array<float, 4> MorphEngine::computeLinearWMix(float x, float y)
{
    // バイリニア補間: 各コーナーへの距離の線形積
    // 中央 (0.5, 0.5) で wA=wB=wC=wD=0.25（-12dB）
    // 等パワーより中央での音量感が低いが、倍音変化がダイレクト
    return { (1.0f - x) * (1.0f - y),
              x         * (1.0f - y),
             (1.0f - x) * y,
              x         * y };
}

std::array<float, 4> MorphEngine::computeSmoothstepWMix(float x, float y)
{
    // スムーズステップ適用後に等パワー計算
    // コーナー付近に「引き寄せ感」が生まれ、意図したフィルターに素早く収束
    // sx = 3x²-2x³ (smoothstep S字カーブ)
    const float sx = x * x * (3.0f - 2.0f * x);
    const float sy = y * y * (3.0f - 2.0f * y);
    return computeEqualPowerWMix(sx, sy);
}

std::array<float, 4> MorphEngine::computeRadialWMix(float x, float y)
{
    // 逆距離二乗: 最も近いコーナーが支配的で境界は急峻に切り替わる
    // 対角線上でも2つのフィルターのみが存在感を持ち「明確な4分割」感
    constexpr float eps = 1e-4f;
    const float wA = 1.0f / (x*x         + y*y         + eps);
    const float wB = 1.0f / ((1-x)*(1-x) + y*y         + eps);
    const float wC = 1.0f / (x*x         + (1-y)*(1-y) + eps);
    const float wD = 1.0f / ((1-x)*(1-x) + (1-y)*(1-y) + eps);
    const float sum = wA + wB + wC + wD;
    return { wA / sum, wB / sum, wC / sum, wD / sum };
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