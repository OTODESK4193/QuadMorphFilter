// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Ladder
{

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
        const float g = st.diode_g;
        const float G = g / (1.0f + g);
        const float G2 = G * G;
        const float G3 = G * G2;
        const float G4 = G * G3;

        float& s1 = st.zdfState[0][ch][0];
        float& s2 = st.zdfState[0][ch][1];
        float& s3 = st.zdfState[0][ch][2];
        float& s4 = st.zdfState[0][ch][3];

        // ===== Accent による k_max の制御 =====
        // slopeIdx: 0=Accent Off, 1=Accent Low, 2=Accent High
        float k_max = 4.2f;

        if (st.slopeIdx == 0)
        {
            k_max = 4.2f;   // Off: 基本値
        }
        else if (st.slopeIdx == 1)
        {
            k_max = 4.6f;   // Low: やや高い
        }
        else if (st.slopeIdx == 2)
        {
            k_max = 5.0f;   // High: より高い
        }

        float k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, k_max);
        k = juce::jlimit(0.0f, k_max, k);

        // ===== フィードバック計算 =====
        float inv1pg = 1.0f / (1.0f + g);
        float S1 = s1 * inv1pg;
        float S2 = s2 * inv1pg;
        float S3 = s3 * inv1pg;
        float S4 = s4 * inv1pg;
        float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;

        float fb = k * sigma;
        float denom = 1.0f + k * G4;
        float u = (x - fb) / denom;

        // ノイズ注入
        if (k > 3.5f)
            u += 1e-7f;

        // ===== 入力段での非線形 =====
        u = std::tanh(u);

        // ===== 4段 ZDF LP フォワードパス =====
        float v1 = (u - s1) * g * inv1pg;
        float y1 = v1 + s1;
        y1 = std::tanh(y1);
        s1 = s1 + 2.0f * v1;

        float v2 = (y1 - s2) * g * inv1pg;
        float y2 = v2 + s2;
        y2 = std::tanh(y2);
        s2 = s2 + 2.0f * v2;

        float v3 = (y2 - s3) * g * inv1pg;
        float y3 = v3 + s3;
        y3 = std::tanh(y3);
        s3 = s3 + 2.0f * v3;

        float v4 = (y3 - s4) * g * inv1pg;
        float y4 = v4 + s4;
        y4 = std::tanh(y4);
        s4 = s4 + 2.0f * v4;

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

                float S1 = s1_ / (1.0f + st.g);
                float S2 = s2_ / (1.0f + st.g);
                float S3 = s3_ / (1.0f + st.g);
                float S4 = s4_ / (1.0f + st.g);

                float sigma = st.ladderG * st.ladderG * st.ladderG * S1
                    + st.ladderG * st.ladderG * S2
                    + st.ladderG * S3 + S4;

                float u = out - st.ladderRes * sigma;

                if (st.ladderRes > 3.5f)
                    u += 1e-6f;

                if (m == 1)  u = std::tanh(u / (1.0f + st.ladderRes
                    * st.ladderG * st.ladderG
                    * st.ladderG * st.ladderG));
                else if (m == 12) u = std::tanh(u * 1.1f) / 1.1f;
                else if (m == 13) u = std::tanh(u * 1.5f) / 1.5f;
                else if (m == 15) u = u / (1.0f + std::abs(u * 0.5f));

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