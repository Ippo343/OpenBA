// ============================================================================
// runtime/src/input/rt_input_port.cpp — RtInputPortInit()  Wave 8 lane D
// ============================================================================
// Kill switches (PLAN.md §4.1 decision 3; unset = ON):
//   BANISHED_RT_INPUT=0   leave the whole Input namespace on the DLL
//
// Mirrors runtime/src/video/rt_video_port.cpp. This lane is a single file
// (input.cpp), so there is only one namespace-level switch, not a per-lane
// one — a future lane splitting KeyMap/Device/Configuration further can add
// BANISHED_RT_INPUT_KEYMAP etc. the way Wave 6's Video did.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtInputRegister();  // input.cpp

void RtInputPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_INPUT")) {
        printf("[RtPort] BANISHED_RT_INPUT=0 -- Input namespace port disabled\n");
        return;
    }
    int n = RtInputRegister();
    printf("[RtPort] Input: %d exports registered\n", n);
}
