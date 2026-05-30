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
    // ===== ブレンドアルゴリズム (morphBlend パラメータで選択) =====

    // 0: 等パワークロスフェード — sin/cos、常に wA²+wB²+wC²+wD²=1
    static std::array<float, 4> computeEqualPowerWMix(float x, float y);

    // 1: 線形バイリニア — シンプルな比例ブレンド (中央で各0.25)
    static std::array<float, 4> computeLinearWMix(float x, float y);

    // 2: スムーズステップ等パワー — コーナーへの「引き寄せ感」が強い
    static std::array<float, 4> computeSmoothstepWMix(float x, float y);

    // 3: ラジアル (逆距離二乗) — 最も近いコーナーが支配的、境界で急峻な切替
    static std::array<float, 4> computeRadialWMix(float x, float y);

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