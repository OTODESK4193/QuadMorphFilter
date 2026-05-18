// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.h
// Model 1,2,12,13,15
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_Ladder
{
    float processSample(int channel, float x, TptFilterState& st);
    inline void updateCoeffs(TptFilterState&) {}
}