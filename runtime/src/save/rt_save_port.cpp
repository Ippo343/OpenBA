// ============================================================================
// runtime/src/save/rt_save_port.cpp — RtSavePortInit()   Wave 5 lane C
// ============================================================================
//   the old DLL-era switch       leave the whole Save namespace on the DLL
//   the old DLL-era switch   ... just Save::SaveLoad + Save::SavedState
//   the old DLL-era switch  ... just Wave 14 Save::SavedState::SetupLoad
//   the old DLL-era switch     ... just Wave 14's SaveMetaData.
//   the old DLL-era switch     ... just Wave 14's LoadMetaData
//   the old DLL-era switch  ... just Wave 13's six state machines,
//                            leaving Wave 5's 16 exports ported. This split
//                            is not decoration: it is the A/B that localised
//                            Wave 13's first defect to this file in one run,
//                            by showing the save completing in 61 frames with
//                            Wave 5 on and Wave 13 off -- the same 61 frames
//                            the all-DLL control took.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include "../../funcs/rt_port.h"
#include "../../types/save/SaveLoad.h"   // ENDGAME B.3.9 ENG: the storage redirect
#include "saveload_internal.h"

// ============================================================================
// the old DLL-era switch — the SUBJECT the pre-init slot audit needs
// (Wave 15 lane ORDER)
extern int RtSaveLoadRegister();       // saveload.cpp (Wave 5 lane C)

void RtSavePortInit() {
    // ENDGAME B.3.9 ENG (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish).
    int n = 0;
    n += RtSaveLoadRegister();
    printf("[RtPort] Save: %d exports registered\n", n);
}
