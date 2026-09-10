// ============================================================================
// runtime/src/save/rt_save_port.cpp — RtSavePortInit()   Wave 5 lane C
// ============================================================================
//   BANISHED_RT_SAVE=0       leave the whole Save namespace on the DLL
//   BANISHED_RT_SAVELOAD=0   ... just Save::SaveLoad + Save::SavedState
//   BANISHED_RT_SETUPLOAD=0  ... just Wave 14 Save::SavedState::SetupLoad
//   BANISHED_RT_SAVE_METADATA=0     ... just Wave 14's SaveMetaData.
//   BANISHED_RT_LOAD_METADATA=0     ... just Wave 14's LoadMetaData
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
extern int RtSaveSetupLoadRegister();  // savedstate_setupload.cpp (Wave 14)
extern int RtSaveSaveMetaDataRegister();// saveload_meta.cpp   (Wave 14, lane SAVEMETA)
extern int RtSaveLoadMetaDataRegister();// saveload_loadmeta.cpp (Wave 14, lane LOADMETA)
extern int RtSaveLoadDrawRegister();   // saveload_draw.cpp (Wave 14 lane DRAW)

void RtSavePortInit() {
    if (!RtWaveEnabled("BANISHED_RT_SAVE")) {
        printf("[RtPort] BANISHED_RT_SAVE=0 -- Save namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_SAVELOAD")) {
        n += RtSaveLoadRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVELOAD_UPDATE")) n += RtSaveLoadUpdateRegister();
        // Wave 14: ?SetupLoad@SavedState@Save@@. Gated with Wave 5's block
        // rather than with Wave 13's, because its three callers are Wave 13
        // bodies and a SavedState method is Wave 5's subject. Its OWN kill
        // switch exists so a crash anywhere on the save path can be A/B'd
        // against this one registration without taking Wave 13 out with it.
        if (RtWaveEnabled("BANISHED_RT_SETUPLOAD")) n += RtSaveSetupLoadRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVE_METADATA"))  n += RtSaveSaveMetaDataRegister();
        // Wave 14 lane LOADMETA. Own kill switch: LoadMetaData is the callee
        // UpdateList state 4 suppresses under the predictive oracle, so leaving
        // it on the DLL while Wave 13 stays ported is a real A/B, not a
        // hypothetical one.
        if (RtWaveEnabled("BANISHED_RT_LOAD_METADATA"))  n += RtSaveLoadMetaDataRegister();
        // Wave 14 lane DRAW gets its own switch for the same A/B reason the
        // UPDATE switch exists: Draw is the only Save body that touches the
        // GPU, so "the save hangs" and "the thumbnail is wrong" have to be
        // separable from Wave 5's and Wave 13's bodies in one run.
        if (RtWaveEnabled("BANISHED_RT_SAVELOAD_DRAW")) n += RtSaveLoadDrawRegister();
    }
    printf("[RtPort] Save: %d exports registered\n", n);
}
