// ==========================================
// DSP/FilterA_SVF_SIMD.cpp
// AVX2/SSE SIMD最適化版 SVF (Step 2)
// ==========================================
#include "FilterA_SVF_SIMD.h"
#include <algorithm>

// ==========================================
// Constructor
// ==========================================
FilterA_SVF_SIMD::FilterA_SVF_SIMD()
{
    sampleRate = 48000.0;

    for (int i = 0; i < numInstances; ++i)
    {
        smoothedCutoff[i].setCurrentAndTargetValue(1000.0f);
        smoothedResonance[i].setCurrentAndTargetValue(0.707f);
        k_arr[i] = 1.0f / 0.707f;
        currentType[i] = 0;
        nonlinearMode[i] = false;
        enabledFlags[i] = true;

        updateCoefficients(i, 1000.0f, 0.707f);
    }
}

// ==========================================
// Initialization
// ==========================================
void FilterA_SVF_SIMD::prepare(double newSampleRate, int /*blockSize*/)
{
    sampleRate = newSampleRate;

    const double rampTime = 0.020;

    for (int i = 0; i < numInstances; ++i)
    {
        smoothedCutoff[i].reset(sampleRate, rampTime);
        smoothedResonance[i].reset(sampleRate, rampTime);

        updateCoefficients(i,
            smoothedCutoff[i].getCurrentValue(),
            smoothedResonance[i].getCurrentValue());
    }

    reset();
}

void FilterA_SVF_SIMD::reset()
{
    for (int ch = 0; ch < maxChannels; ++ch)
    {
        for (int i = 0; i < numInstances; ++i)
        {
            v1[ch][i] = 0.0f;
            v2[ch][i] = 0.0f;
        }
    }
}

// ==========================================
// Parameter Setters
// ==========================================
void FilterA_SVF_SIMD::setCutoff(int instance, float newCutoffHz)
{
    if (instance < 0 || instance >= numInstances) return;
    smoothedCutoff[instance].setTargetValue(clamp(newCutoffHz, 20.0f, 20000.0f));
}

void FilterA_SVF_SIMD::setResonance(int instance, float newQ)
{
    if (instance < 0 || instance >= numInstances) return;
    smoothedResonance[instance].setTargetValue(clamp(newQ, 0.1f, 10.0f));
}

void FilterA_SVF_SIMD::setType(int instance, int type)
{
    if (instance < 0 || instance >= numInstances) return;
    currentType[instance] = clamp(type, 0, 3);
}

void FilterA_SVF_SIMD::setEnabled(int instance, bool enabled)
{
    if (instance < 0 || instance >= numInstances) return;
    enabledFlags[instance] = enabled;
}

// ==========================================
// Coefficient Calculation
// ==========================================
void FilterA_SVF_SIMD::updateCoefficients(int i, float fc, float Q)
{
    nonlinearMode[i] = (Q >= nrThreshold);

    k_arr[i] = 1.0f / juce::jmax(0.1f, Q);

    const float pi = juce::MathConstants<float>::pi;
    float normalizedFreq = (pi * fc) / static_cast<float>(sampleRate);
    g_arr[i] = fastTan(normalizedFreq);

    float denom = 1.0f + g_arr[i] * (g_arr[i] + k_arr[i]) + g_arr[i] * g_arr[i];
    h_arr[i] = (std::abs(denom) < 1e-10f) ? 1.0f : (1.0f / denom);
}

// ==========================================
// SIMD並列処理: 4インスタンス × 1サンプル を同時に
//
// 数式:
//   v3 = x - v2 - k*v1
//   v1_new = v1 + g*h*v3
//   v2_new = v2 + g*v1_new
//
// これを 4要素ベクトル演算で一括処理
// ==========================================
void FilterA_SVF_SIMD::processSampleLinear_SIMD(int ch,
    const float input,
    float out4[numInstances])
{
    // ===== 4インスタンスの状態を SIMD レジスタにロード =====
    __m128 vec_v1 = _mm_load_ps(v1[ch]);       // {v1_A, v1_B, v1_C, v1_D}
    __m128 vec_v2 = _mm_load_ps(v2[ch]);       // {v2_A, v2_B, v2_C, v2_D}
    __m128 vec_g = _mm_load_ps(g_arr);        // {g_A, g_B, g_C, g_D}
    __m128 vec_h = _mm_load_ps(h_arr);        // {h_A, h_B, h_C, h_D}
    __m128 vec_k = _mm_load_ps(k_arr);        // {k_A, k_B, k_C, k_D}

    // 入力を4要素にブロードキャスト
    __m128 vec_x = _mm_set1_ps(input);

    // ===== v3 = x - v2 - k*v1 =====
    // FMA: -(k * v1) + (x - v2) と分解
    __m128 vec_kv1 = _mm_mul_ps(vec_k, vec_v1);
    __m128 vec_xv2 = _mm_sub_ps(vec_x, vec_v2);
    __m128 vec_v3 = _mm_sub_ps(vec_xv2, vec_kv1);

    // ===== v1_new = v1 + g*h*v3 =====
    __m128 vec_gh = _mm_mul_ps(vec_g, vec_h);
    __m128 vec_ghv3 = _mm_mul_ps(vec_gh, vec_v3);
    __m128 vec_v1_new = _mm_add_ps(vec_v1, vec_ghv3);

    // ===== v2_new = v2 + g*v1_new =====
    __m128 vec_gv1n = _mm_mul_ps(vec_g, vec_v1_new);
    __m128 vec_v2_new = _mm_add_ps(vec_v2, vec_gv1n);

    // ===== デノーマル保護 (絶対値 < 1e-15 を 0 に) =====
    // 単純な閾値比較で代替
    __m128 vec_thresh = _mm_set1_ps(1e-15f);
    __m128 vec_v1_abs = _mm_andnot_ps(_mm_set1_ps(-0.0f), vec_v1_new);
    __m128 vec_v2_abs = _mm_andnot_ps(_mm_set1_ps(-0.0f), vec_v2_new);
    __m128 vec_v1_mask = _mm_cmpge_ps(vec_v1_abs, vec_thresh);
    __m128 vec_v2_mask = _mm_cmpge_ps(vec_v2_abs, vec_thresh);
    vec_v1_new = _mm_and_ps(vec_v1_new, vec_v1_mask);
    vec_v2_new = _mm_and_ps(vec_v2_new, vec_v2_mask);

    // ===== 状態を書き戻し =====
    _mm_store_ps(v1[ch], vec_v1_new);
    _mm_store_ps(v2[ch], vec_v2_new);

    // ===== 各 type の出力を計算 =====
    // 4要素のスカラとして取り出して、type別に出力選択
    alignas(16) float lp_arr[4];
    alignas(16) float bp_arr[4];
    alignas(16) float hp_arr[4];
    alignas(16) float notch_arr[4];

    _mm_store_ps(lp_arr, vec_v2_new);                              // LP = v2
    _mm_store_ps(bp_arr, vec_v1_new);                              // BP = v1

    // HP = x - k*v1 - v2
    __m128 vec_kv1n = _mm_mul_ps(vec_k, vec_v1_new);
    __m128 vec_hp = _mm_sub_ps(_mm_sub_ps(vec_x, vec_kv1n), vec_v2_new);
    _mm_store_ps(hp_arr, vec_hp);

    // Notch = x - k*v1
    __m128 vec_notch = _mm_sub_ps(vec_x, vec_kv1n);
    _mm_store_ps(notch_arr, vec_notch);

    // ===== type ごとに出力を選択 =====
    for (int i = 0; i < numInstances; ++i)
    {
        float output = 0.0f;
        switch (currentType[i])
        {
        case 0:  output = lp_arr[i];    break;
        case 1:  output = bp_arr[i];    break;
        case 2:  output = hp_arr[i];    break;
        case 3:  output = notch_arr[i]; break;
        default: output = lp_arr[i];    break;
        }

        // NaN/Inf チェック
        if (!std::isfinite(output))
        {
            output = 0.0f;
            v1[ch][i] = 0.0f;
            v2[ch][i] = 0.0f;
        }

        out4[i] = output;
    }
}

// ==========================================
// 非線形モード (スカラフォールバック)
// tanh は SIMD で実装すると精度に問題が出やすいため
// 高Q時のみ個別インスタンスごとに従来のスカラ処理を行う
// ==========================================
float FilterA_SVF_SIMD::processSampleNonlinear_Scalar(int i, int ch, float x)
{
    if (std::abs(v1[ch][i]) < 1e-15f) v1[ch][i] = 0.0f;
    if (std::abs(v2[ch][i]) < 1e-15f) v2[ch][i] = 0.0f;

    float y = v1[ch][i];

    for (int iter = 0; iter < maxNRIter; ++iter)
    {
        float tanhY = std::tanh(y);
        float sech2Y = 1.0f - tanhY * tanhY;

        float F = y - v1[ch][i] - g_arr[i] * h_arr[i]
            * (x - v2[ch][i] - k_arr[i] * tanhY);
        float dF = 1.0f + g_arr[i] * h_arr[i] * k_arr[i] * sech2Y;

        float delta = F / dF;
        y -= delta;

        if (std::abs(delta) < nrTolerance)
            break;
    }

    v1[ch][i] = y;
    v2[ch][i] = v2[ch][i] + g_arr[i] * y;

    float tanhV1 = std::tanh(v1[ch][i]);

    float lp = v2[ch][i];
    float bp = v1[ch][i];
    float hp = x - k_arr[i] * tanhV1 - v2[ch][i];
    float notch = x - k_arr[i] * tanhV1;

    float output = 0.0f;
    switch (currentType[i])
    {
    case 0:  output = lp;    break;
    case 1:  output = bp;    break;
    case 2:  output = hp;    break;
    case 3:  output = notch; break;
    default: output = lp;    break;
    }

    if (!std::isfinite(output))
    {
        v1[ch][i] = 0.0f;
        v2[ch][i] = 0.0f;
        output = 0.0f;
    }

    return output;
}

// ==========================================
// processChannel
// 1チャンネル分のバッファを処理
// 4インスタンスを同時に処理
// ==========================================
void FilterA_SVF_SIMD::processChannel(int ch,
    const float* inData,
    float* outData_A,
    float* outData_B,
    float* outData_C,
    float* outData_D,
    int          numSamples)
{
    int samplePos = 0;
    float* outArr[numInstances] = { outData_A, outData_B, outData_C, outData_D };

    while (samplePos < numSamples)
    {
        const int blockSize = juce::jmin(subBlockSize, numSamples - samplePos);

        // ===== サブブロックごとに係数を更新 =====
        for (int i = 0; i < numInstances; ++i)
        {
            const float currentCutoff = smoothedCutoff[i].skip(blockSize);
            const float currentRes = smoothedResonance[i].skip(blockSize);
            updateCoefficients(i, currentCutoff, currentRes);
        }

        // ===== 全インスタンスが線形か判定 =====
        // 全て線形 → SIMD で4並列処理
        // 1つでも非線形 → スカラ処理（安全策）
        bool allLinear = true;
        for (int i = 0; i < numInstances; ++i)
        {
            if (nonlinearMode[i] && enabledFlags[i])
            {
                allLinear = false;
                break;
            }
        }

        // ===== サブブロック内のサンプル処理 =====
        for (int n = 0; n < blockSize; ++n)
        {
            float input = inData[samplePos + n];

            if (allLinear)
            {
                // SIMD 並列処理
                float out4[numInstances];
                processSampleLinear_SIMD(ch, input, out4);

                for (int i = 0; i < numInstances; ++i)
                {
                    outArr[i][samplePos + n] = enabledFlags[i] ? out4[i] : 0.0f;
                }
            }
            else
            {
                // スカラ処理 (Newton-Raphson)
                for (int i = 0; i < numInstances; ++i)
                {
                    if (!enabledFlags[i])
                    {
                        outArr[i][samplePos + n] = 0.0f;
                        continue;
                    }

                    if (nonlinearMode[i])
                        outArr[i][samplePos + n] = processSampleNonlinear_Scalar(i, ch, input);
                    else
                    {
                        // 線形だが他が非線形なので一旦SIMDは使わずスカラで
                        // SIMDと同じ計算をインラインで
                        float v3 = input - v2[ch][i] - k_arr[i] * v1[ch][i];
                        float v1_new = v1[ch][i] + g_arr[i] * h_arr[i] * v3;
                        float v2_new = v2[ch][i] + g_arr[i] * v1_new;

                        v1[ch][i] = v1_new;
                        v2[ch][i] = v2_new;

                        float lp = v2[ch][i];
                        float bp = v1[ch][i];
                        float hp = input - k_arr[i] * v1[ch][i] - v2[ch][i];
                        float notch = input - k_arr[i] * v1[ch][i];

                        float output = 0.0f;
                        switch (currentType[i])
                        {
                        case 0:  output = lp;    break;
                        case 1:  output = bp;    break;
                        case 2:  output = hp;    break;
                        case 3:  output = notch; break;
                        default: output = lp;    break;
                        }
                        outArr[i][samplePos + n] = std::isfinite(output) ? output : 0.0f;
                    }
                }
            }
        }

        samplePos += blockSize;
    }
}

// ==========================================
// processBuffer
// メインのバッファ処理エントリポイント
// ==========================================
void FilterA_SVF_SIMD::processBuffer(const juce::AudioBuffer<float>& input,
    std::array<juce::AudioBuffer<float>, 4>& outputs)
{
    const int numChannels = juce::jmin(input.getNumChannels(), maxChannels);
    const int numSamples = input.getNumSamples();

    // 無効インスタンスのバッファをクリア
    for (int i = 0; i < numInstances; ++i)
    {
        if (!enabledFlags[i])
            outputs[i].clear();
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* inData = input.getReadPointer(ch);
        float* outData_A = outputs[0].getWritePointer(ch);
        float* outData_B = outputs[1].getWritePointer(ch);
        float* outData_C = outputs[2].getWritePointer(ch);
        float* outData_D = outputs[3].getWritePointer(ch);

        processChannel(ch, inData, outData_A, outData_B, outData_C, outData_D, numSamples);
    }
}

// ==========================================
// Helpers
// ==========================================
float FilterA_SVF_SIMD::fastTan(float x)
{
    if (x < -1.5f || x > 1.5f)
        return std::tan(x);

    float x2 = x * x;
    float x4 = x2 * x2;

    float numerator = x * (105.0f + 10.0f * x2 + x4);
    float denominator = 105.0f + 45.0f * x2 + x4;

    return numerator / denominator;
}

float FilterA_SVF_SIMD::clamp(float value, float minVal, float maxVal)
{
    return std::min(maxVal, std::max(minVal, value));
}