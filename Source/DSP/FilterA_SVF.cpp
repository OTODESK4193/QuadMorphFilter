// ==========================================
// FilterA_SVF.cpp (Phase 1c: Newton-Raphson 高Q値対応版)
// ==========================================

#include "FilterA_SVF.h"
#include <algorithm>

// ==========================================
// Constructor
// ==========================================
FilterA_SVF::FilterA_SVF()
{
    sampleRate = 48000.0;
    currentType = 0;
    nonlinearMode = false;

    smoothedCutoff.setCurrentAndTargetValue(1000.0f);
    smoothedResonance.setCurrentAndTargetValue(0.707f);

    k = 1.0f / 0.707f;
    updateCoefficients(1000.0f, 0.707f);
}

// ==========================================
// Initialization
// ==========================================
void FilterA_SVF::prepare(double newSampleRate, int /*blockSize*/)
{
    sampleRate = newSampleRate;

    // ランプタイム 20ms: UI 操作・オートメーション時のジッパーノイズ防止
    const double rampTime = 0.020;
    smoothedCutoff.reset(sampleRate, rampTime);
    smoothedResonance.reset(sampleRate, rampTime);

    updateCoefficients(
        smoothedCutoff.getCurrentValue(),
        smoothedResonance.getCurrentValue()
    );

    reset();
}

void FilterA_SVF::reset()
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;

        lastLp[ch] = 0.0f;
        lastBp[ch] = 0.0f;
        lastHp[ch] = 0.0f;
    }
}

// ==========================================
// Parameter Control
// ==========================================
void FilterA_SVF::setCutoff(float newCutoffHz)
{
    smoothedCutoff.setTargetValue(clamp(newCutoffHz, 20.0f, 20000.0f));
}

void FilterA_SVF::setResonance(float newQ)
{
    smoothedResonance.setTargetValue(clamp(newQ, 0.1f, 10.0f));
}

void FilterA_SVF::setType(int type)
{
    currentType = clamp(type, 0, 3);
}

// ==========================================
// Coefficient Calculation
// ==========================================
void FilterA_SVF::updateCoefficients(float fc, float Q)
{
    // Q 値に基づいてモードを判定
    // Q >= 1.5 でアナログ的な飽和が必要になる領域
    nonlinearMode = (Q >= nrThreshold);

    // ダンピング係数: k = 1/Q
    k = 1.0f / juce::jmax(0.1f, Q);

    // プリワーピング: g = tan(π·fc/fs)
    const float pi = juce::MathConstants<float>::pi;
    float normalizedFreq = (pi * fc) / static_cast<float>(sampleRate);
    g = fastTan(normalizedFreq);

    // 分母係数: h = 1 / (1 + g(g+k) + g²)
    // 非線形モードでも h は NR のウォームスタート精度向上に使用
    float denom = 1.0f + g * (g + k) + g * g;
    h = (std::abs(denom) < 1e-10f) ? 1.0f : (1.0f / denom);
}

// ==========================================
// 線形モード（Phase 1b と同一）
// Q < 1.5 のとき使用
// ==========================================
float FilterA_SVF::processSampleLinear(float x, int ch)
{
    // デノーマル保護
    if (std::abs(v1[ch]) < 1e-15f) v1[ch] = 0.0f;
    if (std::abs(v2[ch]) < 1e-15f) v2[ch] = 0.0f;

    // ZDF/TPT 更新式（直接解）
    float v3 = x - v2[ch] - k * v1[ch];
    float v1_new = v1[ch] + g * h * v3;
    float v2_new = v2[ch] + g * v1_new;

    v1[ch] = v1_new;
    v2[ch] = v2_new;

    lastLp[ch] = v2[ch];
    lastBp[ch] = v1[ch];
    lastHp[ch] = x - k * v1[ch] - v2[ch];
    float notch = x - k * v1[ch];

    float output = 0.0f;
    switch (currentType)
    {
    case 0:  output = lastLp[ch]; break;
    case 1:  output = lastBp[ch]; break;
    case 2:  output = lastHp[ch]; break;
    case 3:  output = notch;      break;
    default: output = lastLp[ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// 非線形モード（Newton-Raphson）
// Q >= 1.5 のとき使用
//
// 解くべき陰関数:
//   F(y) = y - v1 - g*h*(x - v2 - k*tanh(y)) = 0
//   y = v1_new（新しい BP 状態変数）
//
// 微分:
//   F'(y) = 1 + g*h*k*(1 - tanh²(y))
//         = 1 + g*h*k*sech²(y)
//
// F'(y) は常に 1 以上 → 収束が数学的に保証される
// ==========================================
float FilterA_SVF::processSampleNonlinear(float x, int ch)
{
    // デノーマル保護
    if (std::abs(v1[ch]) < 1e-15f) v1[ch] = 0.0f;
    if (std::abs(v2[ch]) < 1e-15f) v2[ch] = 0.0f;

    // ===== Newton-Raphson 反復 =====
    //
    // ウォームスタート: 前サンプルの v1 から開始
    // → 音声信号は連続的なので、前サンプルからの変化は小さい
    // → 通常 1〜2 回の反復で収束する
    float y = v1[ch];

    for (int iter = 0; iter < maxNRIter; ++iter)
    {
        // tanh(y) と sech²(y) = 1 - tanh²(y) を計算
        float tanhY = std::tanh(y);
        float sech2Y = 1.0f - tanhY * tanhY;

        // F(y):  解くべき方程式の残差
        // F'(y): ヤコビアン（常に 1 以上）
        float F = y - v1[ch] - g * h * (x - v2[ch] - k * tanhY);
        float dF = 1.0f + g * h * k * sech2Y;

        // Newton ステップ: delta = F/F'
        float delta = F / dF;
        y -= delta;

        // 収束判定: 変化量が許容値以下なら終了
        if (std::abs(delta) < nrTolerance)
            break;
    }

    // ===== 状態変数を更新 =====
    v1[ch] = y;
    v2[ch] = v2[ch] + g * y;  // 第2積分器は線形更新

    // ===== 出力計算 =====
    // HP と Notch の計算では tanh(v1) を使用
    // （フィードバックと整合性を保つため）
    float tanhV1 = std::tanh(v1[ch]);

    lastLp[ch] = v2[ch];
    lastBp[ch] = v1[ch];
    lastHp[ch] = x - k * tanhV1 - v2[ch];
    float notch = x - k * tanhV1;

    float output = 0.0f;
    switch (currentType)
    {
    case 0:  output = lastLp[ch]; break;
    case 1:  output = lastBp[ch]; break;
    case 2:  output = lastHp[ch]; break;
    case 3:  output = notch;      break;
    default: output = lastLp[ch]; break;
    }

    if (!std::isfinite(output))
    {
        v1[ch] = 0.0f;
        v2[ch] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// Buffer Processing（サブブロック処理）
// ==========================================
void FilterA_SVF::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    int samplePos = 0;

    while (samplePos < numSamples)
    {
        // サブブロックサイズを計算（端数対応）
        const int blockSize = juce::jmin(subBlockSize, numSamples - samplePos);

        // SmoothedValue を blockSize 分進めて現在値を取得
        const float currentCutoff = smoothedCutoff.skip(blockSize);
        const float currentRes = smoothedResonance.skip(blockSize);

        // 係数を更新（nonlinearMode も同時に決定される）
        updateCoefficients(currentCutoff, currentRes);

        // サブブロック内でのモードを固定
        // （サンプル単位でモードが切り替わることを防ぐ）
        const bool useNonlinear = nonlinearMode;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch, samplePos);

            for (int n = 0; n < blockSize; ++n)
            {
                // Q 値に基づいてモードを切り替え
                if (useNonlinear)
                    data[n] = processSampleNonlinear(data[n], ch);
                else
                    data[n] = processSampleLinear(data[n], ch);
            }
        }

        samplePos += blockSize;
    }
}

// ==========================================
// Fast Math: Padé [5/4] tan(x) 近似
// ==========================================
float FilterA_SVF::fastTan(float x)
{
    if (x < -1.5f || x > 1.5f)
        return std::tan(x);

    float x2 = x * x;
    float x4 = x2 * x2;

    float numerator = x * (105.0f + 10.0f * x2 + x4);
    float denominator = 105.0f + 45.0f * x2 + x4;

    return numerator / denominator;
}

// ==========================================
// Utility
// ==========================================
float FilterA_SVF::clamp(float value, float minVal, float maxVal)
{
    return std::min(maxVal, std::max(minVal, value));
}