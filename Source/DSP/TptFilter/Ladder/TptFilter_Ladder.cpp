// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
// Model 1,2,12,13,15
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>

namespace TptFilter_Ladder
{

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

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

                float v1 = (u - s1_) * st.ladderG; float y1 = v1 + s1_;
                if (m == 15) y1 = std::tanh(y1);
                st.zdfState[stage][ch][0] = s1_ + 2.0f * v1;

                float v2 = (y1 - s2_) * st.ladderG; float y2 = v2 + s2_;
                if (m == 15) y2 = std::tanh(y2);
                st.zdfState[stage][ch][1] = s2_ + 2.0f * v2;

                float v3 = (y2 - s3_) * st.ladderG; float y3 = v3 + s3_;
                if (m == 15) y3 = std::tanh(y3);
                st.zdfState[stage][ch][2] = s3_ + 2.0f * v3;

                float v4 = (y3 - s4_) * st.ladderG; float y4 = v4 + s4_;
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
        // ----- Model 2: Diode TB-303 -----
        else if (m == 2)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float fb = std::tanh(out - st.ladderRes * st.zdfState[stage][ch][3]);
                float G_h = st.ladderG * 0.5f;

                float v1 = (fb - st.zdfState[stage][ch][0]) * st.ladderG;
                float y1 = v1 + st.zdfState[stage][ch][0];
                st.zdfState[stage][ch][0] = y1 + v1 - G_h * st.zdfState[stage][ch][1];
                y1 = std::tanh(y1);

                float v2 = (y1 - st.zdfState[stage][ch][1]) * st.ladderG;
                float y2 = v2 + st.zdfState[stage][ch][1];
                st.zdfState[stage][ch][1] = y2 + v2 - G_h * st.zdfState[stage][ch][2];
                y2 = std::tanh(y2);

                float v3 = (y2 - st.zdfState[stage][ch][2]) * st.ladderG;
                float y3 = v3 + st.zdfState[stage][ch][2];
                st.zdfState[stage][ch][2] = y3 + v3 - G_h * st.zdfState[stage][ch][3];
                y3 = std::tanh(y3);

                float v4 = (y3 - st.zdfState[stage][ch][3]) * st.ladderG;
                float y4 = v4 + st.zdfState[stage][ch][3];
                st.zdfState[stage][ch][3] = y4 + v4;
                if (y4 > 0.0f) y4 *= 1.2f;
                y4 = std::tanh(y4);

                if (st.slopeIdx == 0) {
                    if (st.filterType == 0) out = y2;
                    else if (st.filterType == 1) out = y1 - y2;
                    else if (st.filterType == 2) out = out - y1;
                    else                         out = y2 + (out - y1);
                }
                else {
                    if (st.filterType == 0) out = y4;
                    else if (st.filterType == 1) out = y2 - y4;
                    else if (st.filterType == 2) out = out - y2;
                    else                         out = y4 + (out - y2);
                }
            }
        }

        return out;
    }

} // namespace TptFilter_Ladder