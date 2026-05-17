// ==========================================
// DSP/MorphEngine.h
// モーフィング・モジュレーション計算エンジン
//
// 責務:
//   - XYパッドの等パワークロスフェード計算
//   - LFOモジュレーション値の計算
//   - フィルターパラメータへのモジュレーション適用
//
// 完全にステートレス（内部状態を持たない）
// 全メソッドが static
// ==========================================
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class MorphEngine
{
public:
    // ===== 等パワークロスフェード (XYパッド) =====
    //
    // 数学的根拠:
    //   cos²θ + sin²θ = 1
    //   → どの位置でも wMix[0]² + wMix[1]² + wMix[2]² + wMix[3]² = 1
    //   → 音量一定を保証
    //
    // 戻り値: wMix[0]=A, [1]=B, [2]=C, [3]=D
    static std::array<float, 4> computeEqualPowerWMix(float x, float y);

    // ===== LFOモジュレーション値の計算 =====
    //
    // isRand1 = true  : Random 1 モード（フィルターごとに独立した値）
    // isRand1 = false : 通常モード（XY位置から導出）
    //
    // 戻り値: [-1.0, +1.0] の範囲のモジュレーション量 × 4フィルター分
    static std::array<float, 4> computeModulation(
        juce::Point<float>          pos,
        const std::array<float, 4>& mod4,
        bool                        isRand1);

    // ===== フィルターパラメータへのモジュレーション適用 =====
    //
    // カットオフ: ±4オクターブ (pow(2, 4*mod))
    // レゾナンス: ±2オクターブ (pow(2, 2*mod))
    static float applyFrequencyMod(float baseFreq, float modValue);
    static float applyResonanceMod(float baseRes, float modValue);

private:
    MorphEngine() = delete; // static-only クラス。インスタンス化禁止
};