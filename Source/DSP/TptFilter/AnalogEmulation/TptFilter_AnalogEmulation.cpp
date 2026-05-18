// ==========================================
// TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.cpp
// Model 5,21,22,27
// ==========================================
#include "TptFilter_AnalogEmulation.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_AnalogEmulation
{

    void updateCoeffs(TptFilterState& st)
    {
        const float fc = st.smoothedDigitalCutoff;
        const float res = st.currentResVal;

        // ----- Model 5: Formant -----
        if (st.filterModel == 5)
        {
            float v = juce::jlimit(0.0f, 4.0f,
                juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
            int   idx = (int)v;
            float frac = v - idx;
            if (idx >= 4) { idx = 3; frac = 1.0f; }

            float f1_map[5] = { 730.f, 270.f, 300.f, 530.f, 400.f };
            float f2_map[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f };
            float f3_map[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
            float f_arr[3] = {
                f1_map[idx] + (f1_map[idx + 1] - f1_map[idx]) * frac,
                f2_map[idx] + (f2_map[idx + 1] - f2_map[idx]) * frac,
                f3_map[idx] + (f3_map[idx + 1] - f3_map[idx]) * frac
            };
            for (int f = 0; f < 3; ++f)
            {
                float wd = juce::MathConstants<float>::pi * f_arr[f] / (float)st.sampleRate;
                st.form_g[f] = std::tan(wd);
                st.form_R[f] = 1.0f / (2.0f * res);
                st.form_h[f] = 1.0f / (1.0f + 2.0f * st.form_R[f] * st.form_g[f]
                    + st.form_g[f] * st.form_g[f]);
            }
        }
        // ----- Model 22: Modal Resonator -----
        else if (st.filterModel == 22)
        {
            float baseFreq = std::clamp(fc, 20.0f, (float)st.sampleRate * 0.45f);
            float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
            for (int b = 0; b < TptFilterState::numModalBands; ++b)
            {
                float bFreq = std::clamp(baseFreq * std::pow((float)(b + 1), inharmonicity),
                    20.0f, (float)st.sampleRate * 0.45f);
                float wd = juce::MathConstants<float>::pi * bFreq / (float)st.sampleRate;
                st.modal_g[b] = std::tan(wd);
                float q = 50.0f / std::sqrt((float)(b + 1));
                st.modal_R[b] = 1.0f / (2.0f * q);
                st.modal_h[b] = 1.0f / (1.0f + 2.0f * st.modal_R[b] * st.modal_g[b]
                    + st.modal_g[b] * st.modal_g[b]);
            }
        }
    }

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 5: Formant (Vowel) -----
        if (m == 5)
        {
            float out_formant = 0.0f;
            float gains[3] = { 1.0f, 0.5f, 0.2f };
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float stage_in = out;
                out_formant = 0.0f;
                for (int f = 0; f < 3; ++f)
                {
                    float hp = (stage_in
                        - (2.0f * st.form_R[f] + st.form_g[f]) * st.form_s1[f][ch]
                        - st.form_s2[f][ch]) * st.form_h[f];
                    float bp = st.form_g[f] * hp + st.form_s1[f][ch];
                    float lp = st.form_g[f] * bp + st.form_s2[f][ch];
                    st.form_s1[f][ch] = st.form_g[f] * hp + bp;
                    st.form_s2[f][ch] = st.form_g[f] * bp + lp;

                    float bandOut = bp;
                    if (st.filterType == 0) bandOut = lp;
                    else if (st.filterType == 1) bandOut = bp;
                    else if (st.filterType == 2) bandOut = hp;
                    else                         bandOut = lp + hp;
                    out_formant += bandOut * gains[f];
                }
                out = out_formant;
            }
        }
        // ----- Model 21: Vactrol LPG -----
        else if (m == 21)
        {
            float targetEnv = std::log10(st.smoothedDigitalCutoff / 20.0f) / 3.0f;
            float currentEnv = st.lpgEnv[ch];
            float attackCoef = 1.0f - std::exp(-1.0f / (0.005f * (float)st.sampleRate));
            float releaseTime = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.05f, 2.0f);
            float releaseCoef = 1.0f - std::exp(-1.0f / (releaseTime * (float)st.sampleRate));
            float coef = (targetEnv > currentEnv) ? attackCoef : releaseCoef;
            st.lpgEnv[ch] = currentEnv + coef * (targetEnv - currentEnv);

            float lpgFc = juce::jlimit(20.0f, 15000.0f,
                20.0f * std::pow(1000.0f, st.lpgEnv[ch]));
            float lpgWd = juce::MathConstants<float>::pi * lpgFc / (float)st.sampleRate;
            float lpgG = std::tan(lpgWd);
            float lpgR = 1.0f / (2.0f * 0.707f);
            float lpgH = 1.0f / (1.0f + 2.0f * lpgR * lpgG + lpgG * lpgG);

            float hp = (out - (2.0f * lpgR + lpgG) * st.s1[0][ch] - st.s2[0][ch]) * lpgH;
            float bp = lpgG * hp + st.s1[0][ch];
            float lp = lpgG * bp + st.s2[0][ch];
            st.s1[0][ch] = lpgG * hp + bp;
            st.s2[0][ch] = lpgG * bp + lp;
            out = lp * (st.lpgEnv[ch] * st.lpgEnv[ch] + 0.001f);
        }
        // ----- Model 22: Modal Resonator -----
        else if (m == 22)
        {
            float modalMix = 0.0f;
            for (int b = 0; b < TptFilterState::numModalBands; ++b)
            {
                float hp = (out - (2.0f * st.modal_R[b] + st.modal_g[b]) * st.modal_s1[b][ch]
                    - st.modal_s2[b][ch]) * st.modal_h[b];
                float bp = st.modal_g[b] * hp + st.modal_s1[b][ch];
                float lp = st.modal_g[b] * bp + st.modal_s2[b][ch];
                st.modal_s1[b][ch] = st.modal_g[b] * hp + bp;
                st.modal_s2[b][ch] = st.modal_g[b] * bp + lp;
                modalMix += bp * (1.0f / std::sqrt((float)(b + 1)));
            }
            if (st.filterType == 0) out = modalMix;
            else if (st.filterType == 1) out = out * 0.5f + modalMix;
            else if (st.filterType == 2) out = out * 0.5f - modalMix;
            else                         out = std::tanh(modalMix * 2.0f);
        }
        // ----- Model 27: Nyquist Anti-alias -----
        else if (m == 27)
        {
            float aa_g = st.g;
            float aa_R = 1.0f / (2.0f * 1.5f);
            float aa_h = 1.0f / (1.0f + 2.0f * aa_R * aa_g + aa_g * aa_g);
            for (int stage = 0; stage < 4; ++stage)
            {
                float hp = (out - (2.0f * aa_R + aa_g) * st.aa_s1[stage][ch]
                    - st.aa_s2[stage][ch]) * aa_h;
                float bp = aa_g * hp + st.aa_s1[stage][ch];
                float lp = aa_g * bp + st.aa_s2[stage][ch];
                st.aa_s1[stage][ch] = aa_g * hp + bp;
                st.aa_s2[stage][ch] = aa_g * bp + lp;
                out = lp;
            }
        }

        return out;
    }

} // namespace TptFilter_AnalogEmulation