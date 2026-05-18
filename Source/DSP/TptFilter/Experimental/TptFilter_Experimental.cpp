// ==========================================
// TptFilter/Experimental/TptFilter_Experimental.cpp
// Model 10: Reverb Metallic (4x4 FDN)
// ==========================================
#include "TptFilter_Experimental.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Experimental
{

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int ch = channel;

        float ms = juce::jmap(st.smoothedDigitalCutoff, 20.0f, 20000.0f, 50.0f, 0.5f);
        float baseSamples = (ms / 1000.0f) * (float)st.sampleRate;
        float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.98f);

        float d_out[4];
        for (int i = 0; i < 4; ++i)
        {
            float delaySamples = juce::jlimit(1.0f, 16383.0f,
                baseSamples * st.fdnDelayTimes[i]);
            int   dInt = (int)delaySamples;
            float dFrac = delaySamples - dInt;
            int r1 = (st.fdnWriteIdx[i][ch] - dInt) & 16383;
            int r2 = (r1 - 1) & 16383;
            float eta = (1.0f - dFrac) / (1.0f + dFrac);
            d_out[i] = st.fdnBuffer[i][ch][r2]
                + eta * (st.fdnBuffer[i][ch][r1] - st.fdn_ap_state[i][ch]);
            st.fdn_ap_state[i][ch] = d_out[i];
        }

        float m0 = 0.5f * (d_out[0] + d_out[1] + d_out[2] + d_out[3]);
        float m1 = 0.5f * (d_out[0] - d_out[1] + d_out[2] - d_out[3]);
        float m2 = 0.5f * (d_out[0] + d_out[1] - d_out[2] - d_out[3]);
        float m3 = 0.5f * (d_out[0] - d_out[1] - d_out[2] + d_out[3]);

        st.fdnBuffer[0][ch][st.fdnWriteIdx[0][ch]] = std::tanh(out + m0 * fb);
        st.fdnBuffer[1][ch][st.fdnWriteIdx[1][ch]] = std::tanh(out + m1 * fb);
        st.fdnBuffer[2][ch][st.fdnWriteIdx[2][ch]] = std::tanh(out + m2 * fb);
        st.fdnBuffer[3][ch][st.fdnWriteIdx[3][ch]] = std::tanh(out + m3 * fb);

        for (int s = 0; s < 4; ++s)
            st.fdnWriteIdx[s][ch] = (st.fdnWriteIdx[s][ch] + 1) & 16383;

        float reverb_out = m0;
        if (st.filterType == 0) out = reverb_out;
        else if (st.filterType == 1) out = (out + reverb_out) * 0.5f;
        else if (st.filterType == 2) out = out - reverb_out * 0.5f;
        else                         out = std::sin(reverb_out * 3.0f);

        return out;
    }

} // namespace TptFilter_Experimental