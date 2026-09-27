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
extern int RtSaveLoadUpdateRegister(); // saveload_update.cpp (Wave 13)
extern int RtSaveSetupLoadRegister();  // savedstate_setupload.cpp (Wave 14)
extern int RtSaveSaveMetaDataRegister();// saveload_meta.cpp   (Wave 14, lane SAVEMETA)
extern int RtSaveLoadMetaDataRegister();// saveload_loadmeta.cpp (Wave 14, lane LOADMETA)
extern int RtSaveLoadDrawRegister();   // saveload_draw.cpp (Wave 14 lane DRAW)
// Wave 24 lane INTERNAL8. The two UNEXPORTED bodies UpdateList reaches by
// address: the qsort comparator (0x1800c3db0) and SaveInfo::SetThumbnail
// (0x1800c3de0). Own switch, nested under SAVELOAD for the same reason DRAW
// is: SetThumbnail is the only one of the two that touches the GPU, and the
// comparator decides the ORDER the player sees in the save list, so "the list
// is in the wrong order" and "the thumbnail is wrong" must be separable from
// Wave 13's UpdateList itself.
extern int RtSaveInfoRegister();       // saveload_saveinfo.cpp (Wave 24 INTERNAL8)

void RtSavePortInit() {
    // ENDGAME B.3.9 ENG (B39_RULES D7): the minted storage row IS the class
    // static from here on (name door, address door, manifest publish).
    RtMintRedirectStorage("?_singleton@SaveLoad@Save@@0PEAV12@EA", &::Save::SaveLoad::_singleton);
    int n = 0;
    n += RtSaveLoadRegister();
    n += RtSaveLoadUpdateRegister();
    n += RtSaveSetupLoadRegister();      // savedstate_setupload.cpp
    n += RtSaveSaveMetaDataRegister();
    n += RtSaveLoadMetaDataRegister();
    n += RtSaveLoadDrawRegister();
    n += RtSaveInfoRegister();
    printf("[RtPort] Save: %d exports registered\n", n);
}
