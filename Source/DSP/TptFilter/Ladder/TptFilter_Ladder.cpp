// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Ladder
{

    // ==========================================
    // TPT 1次ハイパスフィルター
    // Stinchcombe: 8Hz ACカップリング特性
    // ==========================================
    static inline float tpt1HPF(float x, float& s, float g)
    {
        float v = (x - s) * g / (1.0f + g);
        float lp = v + s;
        float hp = x - lp;
        s = lp + v;
        return hp;
    }

    // ==========================================
    // ハードクリッパー
    // Wurtz: ソフトクリッパーだとチューニングがドリフトする
    // ==========================================
    static inline float hardClip(float x, float threshold = 1.0f)
    {
        return std::clamp(x, -threshold, threshold);
    }

    // ==========================================
    // updateCoeffs（TB-303 専用）
    // ==========================================
    void updateCoeffs(TptFilterState& st)
    {
        if (st.filterModel != 2) return;

        const float fc = st.smoothedDigitalCutoff;
        const float fs = (float)st.sampleRate;

        float safeFc = std::clamp(fc, 20.0f, fs * 0.45f);
        st.diode_g = std::tan(juce::MathConstants<float>::pi * safeFc / fs);

        // 8Hz HPF（固定）: Stinchcombe の ACカップリング
        const float hpfFc = 8.0f;
        st.diode_h = std::tan(juce::MathConstants<float>::pi * hpfFc / fs);
    }

    // ==========================================
    // TB-303 Diode Ladder
    //
    // 【修正済み】resonance スケール
    //   Before: k = jmap(0.1~10, 0~17) × 0.25 = 実効 0~4.25
    //           → res=2.35 で発振閾値（k_eff≈1.0）を超えてしまう
    //   After:  k = jmap(0.1~10, 0~4.0) を直接使用
    //           → 自己発振は res≈9〜10 付近で発生
    //
    // 4段 ZDF LP + 8Hz HPF フィードバック
    // ==========================================
    static float processDiodeLadder(int ch, float x, TptFilterState& st)
    {
        const float g = st.diode_g;

        float& s1 = st.zdfState[0][ch][0];
        float& s2 = st.zdfState[0][ch][1];
        float& s3 = st.zdfState[0][ch][2];
        float& s4 = st.zdfState[0][ch][3];

        // ===== 【置き換え】=====
            // k=4.0 は理論閾値（4段×各1/√2 = 1/4）だが
            // 数値精度と8Hz HPFの損失で届かない → 4.2 に上げる
        float k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 4.2f);

        // 8Hz HPF フィードバック（Stinchcombe: アシッドうねりの本質）
        float fb = tpt1HPF(k * s4, st.diodeHpfS[ch], st.diode_h);

        // ハードクリッパー（Wurtz: チューニング安定性のため必須）
        float x_in = hardClip(x - fb, 1.0f);

        // 4段 ZDF LP
        float inv1pg = 1.0f / (1.0f + g);

        float v1 = (x_in - s1) * g * inv1pg;
        float y1 = v1 + s1;
        s1 += 2.0f * v1;

        float v2 = (y1 - s2) * g * inv1pg;
        float y2 = v2 + s2;
        s2 += 2.0f * v2;

        float v3 = (y2 - s3) * g * inv1pg;
        float y3 = v3 + s3;
        s3 += 2.0f * v3;

        float v4 = (y3 - s4) * g * inv1pg;
        float y4 = v4 + s4;
        s4 += 2.0f * v4;

        // 出力選択: slopeIdx で 12dB(y2) vs 24dB(y4) を切り替え
        float out = 0.0f;
        if (st.slopeIdx == 0)
            out = y2; // 12dB: 2段目タップ
        else
            out = y4; // 24dB: 4段目タップ（TB-303 本来の出力）

        return out;
    }

    // ==========================================
    // processSample
    // ==========================================
    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 2: TB-303 Diode Ladder -----
        if (m == 2)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
                out = processDiodeLadder(ch, out, st);
            return out;
        }

        // ----- Model 1,12,13,15: Moog Ladder 系 -----
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
                if (m == 1)  u = std::tanh(u / (1.0f + st.ladderRes
                    * st.ladderG * st.ladderG * st.ladderG * st.ladderG));
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

                // slopeIdx で出力タップを切り替え
                if (st.slopeIdx == 0) {
                    // 12dB: y2 タップ
                    if (st.filterType == 0) out = y2;
                    else if (st.filterType == 1) out = 2.0f * (y1 - y2);
                    else if (st.filterType == 2) out = out - 2.0f * y1 + y2;
                    else                         out = y2 + (out - 2.0f * y1 + y2);
                }
                else {
                    // 24dB+: y4 タップ
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