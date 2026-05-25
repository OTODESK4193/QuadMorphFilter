// ==========================================
// TptFilter/SVF/TptFilter_SVF.cpp
// Model 0,3,4,6,7,9,14,16,23
// ==========================================
#include "TptFilter_SVF.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_SVF
{

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 0, 4: Clean SVF / Bitcrush (SVF part) -----
        if (m == 0 || m == 4)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }
        // ----- Model 3: SEM (Oberheim) - OTA 非線形モデル -----
        // 実機解析:
        //   Oberheim SEM の VCF は OTA ベース 2 極 SVF。
        //   飽和は OTA のテール電流（共鳴 BP フィードバック経路）で発生。
        //   インテグレータはキャパシタ → 線形。
        //   入力への直接 tanh は回路的に誤り（廃止）。
        //
        // 数学モデル（半陰的 ZDF）:
        //   bp_sat = tanh(s1)             ← OTA 飽和
        //   hp = (in - 2R·bp_sat - g·s1 - s2) / (1 + g²)
        //   bp = g·hp + s1
        //   lp = g·bp + s2
        //   s1_new = g·hp + bp            ← 線形インテグレータ更新
        //   s2_new = g·bp + lp
        //   notch  = lp + hp              ← = in - 2R·bp_sat（SEM 固有）
        else if (m == 3)
        {
            const float inv1g2 = 1.0f / (1.0f + st.g * st.g);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                const float bp_sat = std::tanh(st.s1[stage][ch]);
                float hp = (out
                            - 2.0f * st.R * bp_sat
                            - st.g * st.s1[stage][ch]
                            - st.s2[stage][ch]) * inv1g2;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp; // = in - 2R·bp_sat
            }
        }
        // ----- Model 14: CS-80 (Yamaha) - 線形 SVF -----
        else if (m == 14)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }
        // ----- Model 6: Comb Filter -----
        else if (m == 6)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float delaySamples = (float)st.sampleRate
                    / juce::jlimit(20.0f, 20000.0f, st.currentCutoffVal);
                int   dInt = (int)delaySamples;
                float dFrac = delaySamples - dInt;
                int readIdx1 = (st.combWriteIdx[stage][ch] - dInt) & 16383;
                int readIdx2 = (readIdx1 - 1) & 16383;
                float eta = (1.0f - dFrac) / (1.0f + dFrac);
                float delayed = st.combBuffer[stage][ch][readIdx2]
                    + eta * (st.combBuffer[stage][ch][readIdx1]
                        - st.comb_ap_state[stage][ch]);
                st.comb_ap_state[stage][ch] = delayed;

                float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.95f);
                if (st.filterType == 1 || st.filterType == 3) fb = -fb;

                float combOut = 0.0f;
                if (st.filterType == 0 || st.filterType == 1) {
                    float toBuffer = std::tanh(out + delayed * fb);
                    st.combBuffer[stage][ch][st.combWriteIdx[stage][ch]] = toBuffer;
                    combOut = toBuffer;
                }
                else {
                    st.combBuffer[stage][ch][st.combWriteIdx[stage][ch]] = out;
                    combOut = out + delayed * fb;
                }
                st.combWriteIdx[stage][ch] = (st.combWriteIdx[stage][ch] + 1) & 16383;
                out = combOut;
            }
        }
        // ----- Model 7: MS-20 Screaming -----
        else if (m == 7)
        {
            float ms_k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 2.5f);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float fb = st.sk_s2[stage][ch] * ms_k;
                fb = (fb > 0.0f) ? std::tanh(fb * 1.5f) : std::tanh(fb * 0.8f);
                float v1 = (out - fb - st.sk_s1[stage][ch]) * st.g / (1.0f + st.g);
                float y1 = v1 + st.sk_s1[stage][ch];
                st.sk_s1[stage][ch] = y1 + v1;
                float v2 = (y1 - st.sk_s2[stage][ch]) * st.g / (1.0f + st.g);
                float y2 = v2 + st.sk_s2[stage][ch];
                st.sk_s2[stage][ch] = y2 + v2;
                float lp = y2, hp = out - y1, bp = y1 - y2;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }
        // ----- Model 9: Wavefolder -----
        else if (m == 9)
        {
            float fold_gain = juce::jmap(st.currentResVal, 0.1f, 10.0f, 1.0f, 10.0f);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
            out = std::sin(out * fold_gain);
        }
        // ----- Model 16: EDP Wasp CMOS -----
        else if (m == 16)
        {
            auto waspClip = [](float v) { return v / (1.0f + std::abs(v * 2.5f)); };
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = waspClip(st.g * hp + bp);
                st.s2[stage][ch] = waspClip(st.g * bp + lp);
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
            out = std::clamp(out * 1.5f, -1.0f, 1.0f);
        }
        // ----- Model 23: Waveguide Mesh -----
        else if (m == 23)
        {
            float delaySamples = (float)st.sampleRate
                / juce::jlimit(20.0f, 20000.0f, st.smoothedDigitalCutoff);
            int   dInt = (int)delaySamples;
            float dFrac = delaySamples - dInt;
            int r1 = (st.wgWriteIdx[ch] - dInt) & 16383;
            int r2 = (r1 - 1) & 16383;
            float eta = (1.0f - dFrac) / (1.0f + dFrac);
            float wgDelayed = st.wgBuffer[ch][r2]
                + eta * (st.wgBuffer[ch][r1] - st.wg_ap_state[ch]);
            st.wg_ap_state[ch] = wgDelayed;

            float hp = (wgDelayed - (2.0f * st.R + st.g) * st.s1[0][ch]
                - st.s2[0][ch]) * st.h;
            float bp = st.g * hp + st.s1[0][ch];
            float wgScattered = st.g * bp + st.s2[0][ch];
            st.s1[0][ch] = st.g * hp + bp;
            st.s2[0][ch] = st.g * bp + wgScattered;

            float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.99f);
            float excitation = out + wgScattered * fb;
            st.wgBuffer[ch][st.wgWriteIdx[ch]] = std::tanh(excitation);
            st.wgWriteIdx[ch] = (st.wgWriteIdx[ch] + 1) & 16383;

            out = (st.filterType == 0) ? wgScattered : (out * 0.5f + wgScattered * 0.5f);
        }

        return out;
    }

} // namespace TptFilter_SVF