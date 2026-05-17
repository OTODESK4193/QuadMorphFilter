// ==========================================
// DSP/MorphEngine.cpp
// ==========================================
#include "MorphEngine.h"
#include <cmath>

std::array<float, 4> MorphEngine::computeEqualPowerWMix(float x, float y)
{
    // x, y を [0,1] から角度 [0, π/2] に変換
    const float halfPi = juce::MathConstants<float>::halfPi;
    float angleX = x * halfPi;
    float angleY = y * halfPi;

    float cosX = std::cos(angleX);
    float sinX = std::sin(angleX);
    float cosY = std::cos(angleY);
    float sinY = std::sin(angleY);

    // 2D 等パワーミックス
    // A(左上)=cosX*cosY, B(右上)=sinX*cosY
    // C(左下)=cosX*sinY, D(右下)=sinX*sinY
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
        // Random 1 モード: mod4 の値をそのまま [-1,+1] に変換
        for (int i = 0; i < 4; ++i)
            result[i] = mod4[i] * 2.0f - 1.0f;
    }
    else
    {
        // 通常モード: XY位置から4フィルター分を導出
        // A: pos.x, B: pos.y, C: 1-pos.x, D: 1-pos.y
        // (XYパッドの対角が互いに逆方向にモジュレートされる)
        result[0] = pos.x * 2.0f - 1.0f;
        result[1] = pos.y * 2.0f - 1.0f;
        result[2] = (1.0f - pos.x) * 2.0f - 1.0f;
        result[3] = (1.0f - pos.y) * 2.0f - 1.0f;
    }

    return result;
}

float MorphEngine::applyFrequencyMod(float baseFreq, float modValue)
{
    // ±4オクターブのモジュレーション
    // modValue = -1.0 → /16 (4オクターブ下)
    // modValue =  0.0 → ×1  (変化なし)
    // modValue = +1.0 → ×16 (4オクターブ上)
    return baseFreq * std::pow(2.0f, 4.0f * modValue);
}

float MorphEngine::applyResonanceMod(float baseRes, float modValue)
{
    // ±2オクターブのモジュレーション
    return baseRes * std::pow(2.0f, 2.0f * modValue);
}