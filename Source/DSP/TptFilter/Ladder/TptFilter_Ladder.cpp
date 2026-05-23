// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>
namespace TptFilter_Ladder
{
    // ============================================================
    // ADAA (Antiderivative Antialiasing) ヘルパー
    //
    // 【原理】
    //   非線形関数 f(x) = tanh(x) のエイリアスを低減するため、
    //   連続信号の区間積分を近似する。
    //
    //   f の不定積分: F(x) = ∫tanh(x)dx = log(cosh(x))
    //
    //   一次 ADAA:
    //     |x - x_prev| > ε の場合:
    //       y = (F(x) - F(x_prev)) / (x - x_prev)   [区間平均]
    //     |x - x_prev| ≤ ε の場合:
    //       y = tanh((x + x_prev) / 2)               [中点フォールバック]
    //
    // 【数値安定化】
    //   log(cosh(x)) = |x| + log(1 + exp(-2|x|)) - log(2)
    //   |x| > 12 では exp(-24) < float 精度のため近似値を使用。
    // ============================================================

    static inline float logCosh(float x) noexcept
    {
        const float ax = std::abs(x);
        if (ax > 12.0f)
            return ax - 0.6931471806f;  // log(cosh(x)) ≈ |x| - log(2)
        return ax + std::log1p(std::exp(-2.0f * ax)) - 0.6931471806f;
    }

    // tanh の一次 ADAA（前サンプル入力 x_prev を渡す）
    static inline float tanhAdaa(float x, float x_prev) noexcept
    {
        constexpr float kEps = 1e-4f;
        const float delta = x - x_prev;
        if (std::abs(delta) > kEps)
            return (logCosh(x) - logCosh(x_prev)) / delta;
        return std::tanh(0.5f * (x + x_prev));
    }

    static inline float tpt1HPF(float x, float& s, float g)
    {
        float v = (x - s) * g / (1.0f + g);
        float lp = v + s;
        s = lp + v;
        return x - lp;
    }
    void updateCoeffs(TptFilterState& st)
    {
        if (st.filterModel != 2) return;
        const float fc = st.smoothedDigitalCutoff;
        const float fs = (float)st.sampleRate;
        float safeFc = std::clamp(fc, 20.0f, fs * 0.45f);
        st.diode_g = std::tan(juce::MathConstants<float>::pi * safeFc / fs);
        st.diode_h = std::tan(juce::MathConstants<float>::pi * 8.0f / fs);
    }
    // ========================================
    // 【高精度版】ダイオード特性関数
    // Koren の近似式を使用
    // y = tanh(x) で実装済み
    // ========================================
    static inline float diodeSat(float x, float strength = 1.0f)
    {
        // strength: Accent によって非線形強度を変更
        // 0.8f (Accent Off) → 1.0f (Low) → 1.2f (High)
        return std::tanh(x * strength);
    }
    static float processDiodeLadder(int ch, float x, TptFilterState& st)
    {
        const float g      = st.diode_g;
        const float inv1pg = 1.0f / (1.0f + g);
        const float G      = g * inv1pg;   // = g/(1+g)
        const float G2     = G * G;
        const float G3     = G * G2;
        const float G4     = G * G3;

        float& s1 = st.zdfState[0][ch][0];
        float& s2 = st.zdfState[0][ch][1];
        float& s3 = st.zdfState[0][ch][2];
        float& s4 = st.zdfState[0][ch][3];

        // ===== Accent による k_max の制御 =====
        // slopeIdx: 0=Accent Off, 1=Accent Low, 2=Accent High
        const float k_max = (st.slopeIdx == 0) ? 4.2f
                          : (st.slopeIdx == 1) ? 4.6f
                          :                      5.0f;
        float k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, k_max);
        k = juce::jlimit(0.0f, k_max, k);

        // ===== 正規化状態 Si = si/(1+g) =====
        const float S1 = s1 * inv1pg;
        const float S2 = s2 * inv1pg;
        const float S3 = s3 * inv1pg;
        const float S4 = s4 * inv1pg;
        const float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;

        // ============================================================
        // Newton-Raphson 陰的ソルバー (TB-303 Diode Ladder)
        //
        // 【明示的 ZDF の問題】
        //   sigma を線形帰還として使用し、各段の tanh を無視した近似。
        //   自己発振域（k > 4.0）では誤差が特に顕著。
        //
        // 【NR が解く方程式】
        //   u_pre + k · y4_nl(tanh(u_pre)) = x
        //
        //   y4_nl は 4 段 ZDF を tanh 付きで通した非線形出力:
        //     u_sat = tanh(u_pre)
        //     lp_i  = G · y_{i-1} + S_i   (y_0 := u_sat)
        //     y_i   = tanh(lp_i)
        //
        // 【連鎖律による微分 f'】
        //   dy4/du_pre = G⁴ · ∏ sech²(各段)
        //              = G⁴ · (1-u_sat²)·(1-y1²)·(1-y2²)·(1-y3²)·(1-y4²)
        //
        //   f'(u_pre) = 1 + k · dy4/du_pre
        //
        // 初期推定値 = 明示的 ZDF の解
        // 3 回反復で通常 10⁻⁶ 以下に収束する。
        // ============================================================

        // 初期推定値（明示的 ZDF 近似）
        float u_pre = (x - k * sigma) / (1.0f + k * G4);
        if (k > 3.5f) u_pre += 1e-7f;  // 自己発振安定化ノイズ

        // NR 反復
        for (int nr = 0; nr < 3; ++nr)
        {
            const float u_sat = std::tanh(u_pre);
            const float y1    = std::tanh(G * u_sat + S1);
            const float y2    = std::tanh(G * y1    + S2);
            const float y3    = std::tanh(G * y2    + S3);
            const float y4    = std::tanh(G * y3    + S4);

            const float f = u_pre + k * y4 - x;

            // 連鎖律: dy4/du_pre = G⁴ · ∏ sech²
            const float dchain = G4
                * (1.0f - u_sat * u_sat)
                * (1.0f - y1   * y1)
                * (1.0f - y2   * y2)
                * (1.0f - y3   * y3)
                * (1.0f - y4   * y4);

            const float f_prime = 1.0f + k * dchain;
            if (std::abs(f_prime) < 1e-10f) break;
            u_pre -= f / f_prime;
        }

        // ===== NR 収束値で状態を更新（ADAA 適用・1 回のみ）=====
        //
        // 【ADAA 適用方針】
        //   入力段 tanh（u_pre → u_sat）のみ ADAA を適用する。
        //
        //   内部 ZDF ステージ（lp1〜lp4）への ADAA は適用しない。
        //   理由: 再帰フィードバックループ内部で ADAA（≈ハーフサンプル遅延）を
        //   多段適用すると、小信号線形解析でリップルが発生し
        //   フィルター本来の周波数特性が損なわれる。
        //   非線形エイリアス低減の主要効果は入力段 ADAA のみで得られる。
        //
        // 入力段 tanh: ADAA 適用
        const float u_sat = tanhAdaa(u_pre, st.diodeAdaaPrevInput[ch]);
        st.diodeAdaaPrevInput[ch] = u_pre;

        // 各 ZDF 段: 標準 tanh（フィルター特性を保全）
        const float v1 = (u_sat - s1) * G;
        const float y1 = std::tanh(v1 + s1);
        s1 += 2.0f * v1;

        const float v2 = (y1 - s2) * G;
        const float y2 = std::tanh(v2 + s2);
        s2 += 2.0f * v2;

        const float v3 = (y2 - s3) * G;
        const float y3 = std::tanh(v3 + s3);
        s3 += 2.0f * v3;

        const float v4 = (y3 - s4) * G;
        const float y4 = std::tanh(v4 + s4);
        s4 += 2.0f * v4;

        // ===== 8Hz HPF =====
        float& hpf_s = st.diodeHpfS[ch];
        float hpf_out = tpt1HPF(y4, hpf_s, st.diode_h);
        hpf_out *= 0.95f;
        return hpf_out;
    }
    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;
        if (m == 2)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
                out = processDiodeLadder(ch, out, st);
            return out;
        }
        if (m == 1 || m == 12 || m == 13 || m == 15)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float s1_ = st.zdfState[stage][ch][0];
                float s2_ = st.zdfState[stage][ch][1];
                float s3_ = st.zdfState[stage][ch][2];
                float s4_ = st.zdfState[stage][ch][3];
                const float inv1pg = 1.0f / (1.0f + st.g);
                float S1 = s1_ * inv1pg;
                float S2 = s2_ * inv1pg;
                float S3 = s3_ * inv1pg;
                float S4 = s4_ * inv1pg;
                const float G  = st.ladderG;  // = g/(1+g)
                const float G2 = G * G;
                const float G3 = G * G2;
                const float G4 = G * G3;
                float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;
                float c = out - st.ladderRes * sigma;  // c = x_in - k·sigma
                float u;
                if (m == 1)
                {
                    // ============================================================
                    // Newton-Raphson 陰的ソルバー (Moog Ladder)
                    //
                    // 【明示的 ZDF の問題】
                    //   sigma は状態の線形結合で帰還を近似しており、
                    //   入力 tanh の非線形性を無視した近似。
                    //   高レゾナンスで誤差が顕著になる。
                    //
                    // 【NR が解く方程式】
                    //   u_hat + k·G⁴·tanh(u_hat · inv_norm) = c
                    //   （ここで inv_norm = 1/(1+k·G⁴)、c = x_in - k·sigma）
                    //
                    // 【微分（f'）】
                    //   f'(u_hat) = 1 + k·G⁴·inv_norm·sech²(u_hat·inv_norm)
                    //
                    // 初期推定値 = 明示的 ZDF の解（tanh ≈ x 線形近似）
                    // 3回反復で通常 10⁻⁶ 以下に収束する。
                    // ============================================================
                    const float inv_norm = 1.0f / (1.0f + st.ladderRes * G4);
                    float u_hat = c;
                    if (st.ladderRes > 3.5f) u_hat += 1e-6f;

                    for (int nr = 0; nr < 3; ++nr)
                    {
                        const float t     = std::tanh(u_hat * inv_norm);
                        const float sech2 = 1.0f - t * t;
                        const float f_val = u_hat + st.ladderRes * G4 * t - c;
                        const float f_prime = 1.0f
                            + st.ladderRes * G4 * inv_norm * sech2;
                        if (std::abs(f_prime) < 1e-10f) break;
                        u_hat -= f_val / f_prime;
                    }
                    // ADAA: 入力 tanh に適用
                    const float tanh_input = u_hat * inv_norm;
                    u = tanhAdaa(tanh_input, st.moogAdaaPrevInput[stage][ch]);
                    st.moogAdaaPrevInput[stage][ch] = tanh_input;
                }
                else
                {
                    // model 12 / 13 / 15: 明示的 ZDF（既存）
                    float u_pre = c;
                    if (st.ladderRes > 3.5f) u_pre += 1e-6f;
                    if      (m == 12) u = std::tanh(u_pre * 1.1f) / 1.1f;
                    else if (m == 13) u = std::tanh(u_pre * 1.5f) / 1.5f;
                    else              u = u_pre / (1.0f + std::abs(u_pre * 0.5f));
                }
                float v1 = (u - s1_) * st.ladderG;
                float y1 = v1 + s1_;
                if (m == 15) y1 = std::tanh(y1);
                st.zdfState[stage][ch][0] = s1_ + 2.0f * v1;
                float v2 = (y1 - s2_) * st.ladderG;
                float y2 = v2 + s2_;
                if (m == 15) y2 = std::tanh(y2);
                st.zdfState[stage][ch][1] = s2_ + 2.0f * v2;
                float v3 = (y2 - s3_) * st.ladderG;
                float y3 = v3 + s3_;
                if (m == 15) y3 = std::tanh(y3);
                st.zdfState[stage][ch][2] = s3_ + 2.0f * v3;
                float v4 = (y3 - s4_) * st.ladderG;
                float y4 = v4 + s4_;
                if (m == 15) y4 = std::tanh(y4);
                st.zdfState[stage][ch][3] = s4_ + 2.0f * v4;
                if (st.slopeIdx == 0) {
                    if (st.filterType == 0) out = y2;
                    else if (st.filterType == 1) out = 2.0f * (y1 - y2);
                    else if (st.filterType == 2) out = out - 2.0f * y1 + y2;
                    else                         out = y2 + (out - 2.0f * y1 + y2);
                }
                else {
                    if (st.filterType == 0) out = y4;
                    else if (st.filterType == 1) out = 4.0f * (y2 - y4);
                    else if (st.filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
                    else                         out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4);
                }
            }
        }
        return out;
    }
} // namespace TptFilter_Ladder