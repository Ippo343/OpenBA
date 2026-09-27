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
// The 0.5f half-cell const at 0x140172114 is registered so the terrain-position bias
// reads the exact image value instead of the 0.5f fallback.
// (ENDGAME B.3.9 TR: the type-id storage cell this file used to register and fill is
// gone; the readers use X::TypeId() of the class at the cell.)
//
// Mirrors reg_work_act.cpp + reg_action_child_ctors2.cpp.
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/InteractComponent.h"

// Forward declarations of the ported helpers (code/Game/Work/firework_search.cpp).
extern int64_t FireWork_SpiralSearch_81d0(int64_t, uint32_t, int, float*, float*);   // 0x1400881d0

// 0.5f half-cell-centre constant (exe .rdata DAT_140172114, bit pattern 0x3f000000).
static const float g_DAT_140172114 = 0.5f;

void RegisterWorkTailFireBatch() {
    RegisterExeAddr(0x1400881d0, reinterpret_cast<uintptr_t>(&FireWork_SpiralSearch_81d0));
    // 0.5f half-cell const (read-only; safe to register unconditionally)
    // 0x140172114 (0.5f) is registered by reg_citizen_needsupdate.cpp, which runs later.
    // Both copies hold the same verified value; duplicate removed 2026-08-30.
}
