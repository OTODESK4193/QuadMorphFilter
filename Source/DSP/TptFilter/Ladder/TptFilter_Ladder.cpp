// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
//
// 【根本修正】TB-303 自己発振
//   Before: diodePrevY4（前サンプル）でフィードバック
//           → 1サンプル遅延で位相がずれ発振不可
//   After:  Moogと同じZDF代数解（遅延ゼロ）
//           → k=4.0で正しく自己発振
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Ladder
{

    // ==========================================
    // TPT 1次ハイパスフィルター（8Hz固定）
    // TB-303 の ACカップリング特性
    // ==========================================
    static inline float tpt1HPF(float x, float& s, float g)
    {
        float v = (x - s) * g / (1.0f + g);
        float lp = v + s;
        s = lp + v;
        return x - lp;
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

        // 8Hz HPF（固定）: TB-303のACカップリング
        st.diode_h = std::tan(juce::MathConstants<float>::pi * 8.0f / fs);
    }

    // ==========================================
    // TB-303 Diode Ladder
    //
    // 【正しいZDF実装】Moogと同じ代数解法を使用
    //
    //   y4 = (G^4 * x + sigma) / (1 + k * G^4)
    //   u  = (x - k * sigma)  / (1 + k * G^4)
    //
    // これにより:
    //   - 1サンプル遅延ゼロ
    //   - k≈4で正しく自己発振
    //   - Moogとの違い: 8Hz HPFを入力に適用
    //                   異なるソフトサチュレーション
    // ==========================================
    static float processDiodeLadder(int ch, float x, TptFilterState& st)
    {
        const float g = st.diode_g;
        const float G = g / (1.0f + g);
        const float G2 = G * G;
        const float G3 = G * G2;
        const float G4 = G * G3;

        // ===== Step 1: 8Hz HPF を入力に適用 =====
        // TB-303のACカップリング特性
        // これが「アシッドうねり」の物理的根拠
        float x_hp = tpt1HPF(x, st.diodeHpfS[ch], st.diode_h);

        // ===== Step 2: ZDF代数解 =====
        // k = 0〜4.0 （k=4.0付近で自己発振）
        float k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 4.0f);

        float s1 = st.zdfState[0][ch][0];
        float s2 = st.zdfState[0][ch][1];
        float s3 = st.zdfState[0][ch][2];
        float s4 = st.zdfState[0][ch][3];

        // 状態変数の寄与
        float inv1pg = 1.0f / (1.0f + g);
        float S1 = s1 * inv1pg;
        float S2 = s2 * inv1pg;
        float S3 = s3 * inv1pg;
        float S4 = s4 * inv1pg;

        float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;

        // 分母（常に正 → 数値安定）
        float denom = 1.0f + k * G4;
        float u = (x_hp - k * sigma) / denom;

        // ===== Step 3: TB-303 ソフトサチュレーション =====
        // Moogの強いtanhより柔らかい特性
        // 自己発振振幅を自然に制限する
        u = std::tanh(u * 0.9f) / 0.9f;

        // ===== Step 4: 4段 ZDF LP フォワードパス =====
        // y_i = G*(x_i - s_i) + s_i
        // s_i_new = 2*y_i - s_i_old
        float y1 = G * (u - s1) + s1;  s1 = 2.0f * y1 - s1;
        float y2 = G * (y1 - s2) + s2;  s2 = 2.0f * y2 - s2;
        float y3 = G * (y2 - s3) + s3;  s3 = 2.0f * y3 - s3;
        float y4 = G * (y3 - s4) + s4;  s4 = 2.0f * y4 - s4;

        st.zdfState[0][ch][0] = s1;
        st.zdfState[0][ch][1] = s2;
        st.zdfState[0][ch][2] = s3;
        st.zdfState[0][ch][3] = s4;

        // diodePrevY4は互換性のため保持
        st.diodePrevY4[ch] = y4;

        // 12dB: y2タップ, 24dB: y4タップ
        return (st.slopeIdx == 0) ? y2 : y4;
    }

    // ==========================================
    // processSample
    // ==========================================
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