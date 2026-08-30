// standalone/reg_debugmenu.cpp
// Lane 3 wave — registers the 11 exe addresses in the built-in DebugUI /
// Game debug-cheat cluster (notes/wave_remaining/lane3_debug.txt).
//
// The bodies for all 11 addresses already existed as internal-linkage
// *_Impl() helpers in code/Game/Dialog/debugui.cpp (an earlier, unfinished
// pass ported the whole DebugUI panel — Slot4/7/8/9, the panel's own
// REGISTER_TYPE vtable, and every Slot9 button handler — but never called
// RegisterExeAddr for the individual retail addresses). This wave adds a
// thin non-static wrapper per address (same file, matching Ghidra's exact
// signature) and wires them here.
//
// Nothing in the current standalone call graph resolves these 11 addresses
// via ResolveAddr today (verified: engine_funcs.cpp resolves fn_Game_Spawn-
// Tornado / fn_DebugUI_OnDaySliderChanged / etc. as raw exe-address pointers,
// but nothing in the tree ever calls those fn_* pointers — the reimplemented
// DebugUI dialog calls the C++ *_Impl bodies directly instead). Registering
// them anyway is defensive: it means any future or currently-unknown .rdata
// caller of these retail addresses gets our reimplementation rather than
// EXEC-of-0, and it makes fn_Game_SpawnTornado() etc. (already resolved in
// engine_funcs.cpp) usable dead code instead of permanently-null dead code.
//
// SAFETY: all 11 addresses verified as true function ENTRYs in Ghidra
// (get_function_by_address Entry == address, no mid-function resolves).
//
// See notes/wave_remaining/LANE3_DEBUGMENU_REPORT.md for what each function
// does, what it needs to actually fire in standalone, and how to invoke it.

#include "resolve.h"
#include "Dialog/ui_panels.h"
#include <cstdint>

void RegisterDebugMenuBatch() {
    RegisterExeAddr(0x14005fc60, reinterpret_cast<uintptr_t>(&DebugUI_OnDiseaseComboChanged));
    RegisterExeAddr(0x14005fd20, reinterpret_cast<uintptr_t>(&Game_SpawnTornado));
    RegisterExeAddr(0x14005fe10, reinterpret_cast<uintptr_t>(&Game_InfectRandomCitizen));
    RegisterExeAddr(0x14005feb0, reinterpret_cast<uintptr_t>(&Game_StartRandomFire));
    RegisterExeAddr(0x14005ff30, reinterpret_cast<uintptr_t>(&DebugUI_TriggerDisasterOnBuilding));
    RegisterExeAddr(0x140060000, reinterpret_cast<uintptr_t>(&Game_KillAllCitizens));   // actually destroys raw materials — see report
    RegisterExeAddr(0x140060110, reinterpret_cast<uintptr_t>(&Game_TriggerDisasterByType));
    RegisterExeAddr(0x140060380, reinterpret_cast<uintptr_t>(&Game_SpawnCitizens));
    RegisterExeAddr(0x1400607e0, reinterpret_cast<uintptr_t>(&DebugUI_OnDaySliderChanged));
    RegisterExeAddr(0x140061530, reinterpret_cast<uintptr_t>(&DebugUI_WriteLightingColors));
    RegisterExeAddr(0x140061780, reinterpret_cast<uintptr_t>(&DebugUI_SetDayLighting));
}
