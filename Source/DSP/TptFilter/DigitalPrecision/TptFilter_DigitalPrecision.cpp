// ==========================================
// TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.cpp
// Model 17,18,19,20
// ==========================================
#include "TptFilter_DigitalPrecision.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_DigitalPrecision
{

    void updateCoeffs(TptFilterState& st)
    {
        const float fc = st.smoothedDigitalCutoff;
        const float res = st.currentResVal;
        float maxSafeFreq = (float)st.sampleRate * 0.45f;

        float rippleDb = juce::jmap(res, 0.1f, 10.0f, 0.1f, 3.0f);
        float eps = std::sqrt(std::pow(10.0f, rippleDb / 10.0f) - 1.0f);

        for (int k = 0; k < st.currentStages; ++k)
        {
            float q = 0.707f;
            float freqScale = 1.0f;
            const int m = st.filterModel;

            if (m == 17)
            {
                float theta = juce::MathConstants<float>::pi
                    * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                q = std::max(0.5f, 1.0f / (2.0f * std::sin(theta)));
            }
            else if (m == 18)
            {
                float theta = juce::MathConstants<float>::pi
                    * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float a = 1.0f / st.filterOrder * std::asinh(1.0f / eps);
                float real_p = -std::sinh(a) * std::sin(theta);
                float imag_p = std::cosh(a) * std::cos(theta);
                float wn2 = real_p * real_p + imag_p * imag_p;
                freqScale = std::sqrt(wn2);
                q = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p));
            }
            else if (m == 19)
            {
                float theta = juce::MathConstants<float>::pi
                    * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                q = std::max(0.5f, 1.0f / (2.0f * std::sin(theta)) * 0.577f);
                freqScale = 1.0f + (float)st.filterOrder * 0.1f;
            }
            else if (m == 20)
            {
                float theta = juce::MathConstants<float>::pi
                    * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float a = 1.0f / st.filterOrder * std::asinh(1.0f / (eps * 0.5f));
                float real_p = -std::sinh(a) * std::sin(theta);
                float imag_p = std::cosh(a) * std::cos(theta);
                float wn2 = real_p * real_p + imag_p * imag_p;
                freqScale = std::sqrt(wn2);
                q = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p) * 1.2f);
            }

            float sectionFreq = std::clamp(fc * freqScale, 20.0f, maxSafeFreq);
            float sectionWd = juce::MathConstants<float>::pi * sectionFreq
                / (float)st.sampleRate;

            if ((int)st.dpCoeffs.size() <= k)
                st.dpCoeffs.resize(k + 1);

            st.dpCoeffs[k].g = std::tan(sectionWd);
            st.dpCoeffs[k].R = 1.0f / (2.0f * q);
            st.dpCoeffs[k].h = 1.0f / (1.0f + 2.0f * st.dpCoeffs[k].R * st.dpCoeffs[k].g
                + st.dpCoeffs[k].g * st.dpCoeffs[k].g);
        }
    }

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int ch = channel;

        for (int stage = 0; stage < st.currentStages; ++stage)
        {
            if (stage >= (int)st.dpCoeffs.size()) break;
            const float cur_g = st.dpCoeffs[stage].g;
            const float cur_R = st.dpCoeffs[stage].R;
            const float cur_h = st.dpCoeffs[stage].h;

            float hp = (out - (2.0f * cur_R + cur_g) * st.dp_s1[stage][ch]
                - st.dp_s2[stage][ch]) * cur_h;
            float bp = cur_g * hp + st.dp_s1[stage][ch];
            float lp = cur_g * bp + st.dp_s2[stage][ch];
            st.dp_s1[stage][ch] = cur_g * hp + bp;
            st.dp_s2[stage][ch] = cur_g * bp + lp;

            if (st.filterModel == 20) {
                out = lp + hp * 0.5f;
            }
            else {
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }

        return out;
    }

} // namespace TptFilter_DigitalPrecision