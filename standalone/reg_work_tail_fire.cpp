// standalone/reg_work_tail_fire.cpp
// Registration for the FireWork burning-entity spiral search
// (code/Game/Work/firework_search.cpp). Wired via RegisterWorkTailFireBatch().
//
// Closure-safe: every 0x140xxxxxx sub-callee of 0x1400881d0 / 0x140088730 is already
// ported + registered elsewhere (Entity_impl_2e20 122e20, FUN_140123070, Entity_impl_7fb0,
// CharacterComponent_impl_f3e0 == fn_MapGetTerrainPos, FUN_1400a00f0, BuildStructure_impl_f810,
// FUN_140132010, Entity_FindComponent_Position 2ed0, Random::Rand slot 0x1401512e8) — so
// these two are the only newly-registered addresses.
//
// Also fills the InteractComponent type-id global 0x1401c7900 — read by 81d0's child-type
// scan (and already read by several landed FindComponent_2ed0 callers that previously saw
// it null). Filled via GetTypeIdByName AFTER Engine::Startup (Object::Factory::Startup
// assigns the real CRC id only then), called from game_main.cpp alongside the other
// FillTypeIdGlobals_*. The 0.5f half-cell const at 0x140172114 is registered too so the
// terrain-position bias reads the exact image value instead of the 0.5f fallback.
//
// Mirrors reg_work_act.cpp + reg_action_child_ctors2.cpp.
#ifdef STANDALONE_EXE
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>

// Forward declarations of the ported helpers (code/Game/Work/firework_search.cpp).
extern void    FireWork_SpiralCell_8730(int64_t, int*, int, int, int, uint32_t,
                                        int*, int*, uint64_t*);                       // 0x140088730
extern int64_t FireWork_SpiralSearch_81d0(int64_t, uint32_t, int, float*, float*);   // 0x1400881d0

// Type-id global filled after Engine::Startup.
static uint32_t g_tid_InteractComponent = 0;   // 0x1401c7900

// 0.5f half-cell-centre constant (exe .rdata DAT_140172114, bit pattern 0x3f000000).
static const float g_DAT_140172114 = 0.5f;

extern "C" void FillTypeIdGlobals_worktail_fire() {
    g_tid_InteractComponent = ::Object::GetTypeIdByName("InteractComponent");
}

void RegisterWorkTailFireBatch() {
    RegisterExeAddr(0x140088730, reinterpret_cast<uintptr_t>(&FireWork_SpiralCell_8730));
    RegisterExeAddr(0x1400881d0, reinterpret_cast<uintptr_t>(&FireWork_SpiralSearch_81d0));
    // type-id global storage (value filled later by FillTypeIdGlobals_worktail_fire)
    RegisterExeAddr(0x1401c7900, reinterpret_cast<uintptr_t>(&g_tid_InteractComponent));
    // 0.5f half-cell const (read-only; safe to register unconditionally)
    RegisterExeAddr(0x140172114, reinterpret_cast<uintptr_t>(&g_DAT_140172114));
}
#endif // STANDALONE_EXE
