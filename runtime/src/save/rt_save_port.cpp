// ============================================================================
// runtime/src/save/rt_save_port.cpp — RtSavePortInit()   Wave 5 lane C
// ============================================================================
//   BANISHED_RT_SAVE=0       leave the whole Save namespace on the DLL
//   BANISHED_RT_SAVELOAD=0   ... just Save::SaveLoad + Save::SavedState
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
        n += RtSaveLoadUpdateRegister();
    }
    printf("[RtPort] Save: %d exports registered\n", n);
}
