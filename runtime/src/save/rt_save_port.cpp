// ============================================================================
// runtime/src/save/rt_save_port.cpp — RtSavePortInit()   Wave 5 lane C
// ============================================================================
//   BANISHED_RT_SAVE=0       leave the whole Save namespace on the DLL
//   BANISHED_RT_SAVELOAD=0   ... just Save::SaveLoad + Save::SavedState
//   BANISHED_RT_SAVELOAD_UPDATE=0  ... just Wave 13's six state machines,
//                            leaving Wave 5's 16 exports ported. This split
//                            is not decoration: it is the A/B that localised
//                            Wave 13's first defect to this file in one run,
//                            by showing the save completing in 61 frames with
//                            Wave 5 on and Wave 13 off -- the same 61 frames
//                            the all-DLL control took.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtSaveLoadRegister();       // saveload.cpp (Wave 5 lane C)
extern int RtSaveLoadUpdateRegister(); // saveload_update.cpp (Wave 13)

void RtSavePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_SAVE")) {
        printf("[RtPort] BANISHED_RT_SAVE=0 -- Save namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_SAVELOAD")) {
        n += RtSaveLoadRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVELOAD_UPDATE")) n += RtSaveLoadUpdateRegister();
    }
    printf("[RtPort] Save: %d exports registered\n", n);
}
