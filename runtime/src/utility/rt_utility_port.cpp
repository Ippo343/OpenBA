// ============================================================================
// runtime/src/utility/rt_utility_port.cpp — RtUtilityPortInit()  Wave 8 lane E
// ============================================================================
//   BANISHED_RT_UTILITY=0   leave Utility::PerlinNoise::Generate on the DLL
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtUtilityRegister();   // perlin.cpp

void RtUtilityPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_UTILITY")) {
        printf("[RtPort] BANISHED_RT_UTILITY=0 -- Utility namespace port disabled\n");
        return;
    }
    int n = RtUtilityRegister();
    printf("[RtPort] Utility: %d exports registered\n", n);
}
