// ==========================================
// DSP/MorphEngine.h
// モーフィング・モジュレーション計算エンジン
// ==========================================
#pragma once

// juce::Point は juce_graphics に属するため
// juce_audio_processors 経由で全モジュールをインクルード
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

class MorphEngine
{
public:
    // ===== 等パワークロスフェード (XYパッド) =====
    static std::array<float, 4> computeEqualPowerWMix(float x, float y);

    // ===== LFOモジュレーション値の計算 =====
    static std::array<float, 4> computeModulation(
        juce::Point<float>          pos,
        const std::array<float, 4>& mod4,
        bool                        isRand1);

    // ===== フィルターパラメータへのモジュレーション適用 =====
    static float applyFrequencyMod(float baseFreq, float modValue);
    static float applyResonanceMod(float baseRes, float modValue);

private:
    MorphEngine() = delete;
};