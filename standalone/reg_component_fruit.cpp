// Component registration wave — batch fruit (Phase 1 FruitComponent bring-up).
// Registers the exe addresses FruitComponent's live path needs that are not
// already registered: two .rdata float constants used by the season-month math,
// plus the FruitDescription type-id .rdata global (DAT_1401c4288) read by
// FruitComponent_Update's IsDerivedFrom(_unk18, *typeId).
//
// Already-registered (NOT re-registered here):
//   * 0x1400052c0  FUN_1400052c0 sibling NaturalResourceComponent finder
//                  (reg_work_base.cpp WorkBase_FindComponent_52c0) — called by
//                  FruitComponent_Startup.
//   * 0x1401c7b30  NaturalResourceComponent type-id (reg_component_nature.cpp
//                  tid_NaturalResourceComponent) — read inside FUN_1400052c0.
//   * 0x1401c3dd8  game-world singleton global (core, registered for growth/pop).
//   * 0x140172140 (0.98 g_fUVScale), 0x1401721d0 (32.0), 0x140172234 (300.0),
//     0x140172288 (-10.0), 0x1401722b0 (fabs mask) — all reg_component_nature.cpp.
//   * 0x14017214c (1.0) — gameupdateloop_load.cpp.
//   * fn_IsDerivedFrom / fn_InternalBasePtr_CheckPtr / fn_Time_AsSeconds are
//     engine_funcs globals resolved elsewhere.
//
// The integrator wires RegisterComponentBatch_fruit() into address_registry.cpp.
// (ENDGAME B.3.9 TR: the type-id storage cell this file used to register and fill is
// gone; the readers use X::TypeId() of the class at the cell.)
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>
#include "Component/FruitComponent.h"
#include "Component/FruitDescription.h"

// ------------------------------------------------------------------

// Backing storage for the fruitcomponent .rdata constant-pool entries not already
// provided by reg_component_nature.cpp. Exact bit patterns verified in Ghidra:
//   0x14017225c = 0x45610000 = 3600.0f  (SECS_PER_YEAR, season month math)
//   0x1401721b8 = 0x41400000 = 12.0f    (MONTHS_PER_YEAR, season month math)
static const uint32_t k_rdata_17225c = 0x45610000u;  // 3600.0f
static const uint32_t k_rdata_1721b8 = 0x41400000u;  // 12.0f

void RegisterComponentBatch_fruit() {
    // .rdata float constant-pool entries read by fruitcomponent.cpp
    // (FruitComponent_Update season-month math) via ResolveAddr(Module::Exe, addr).
    RegisterExeAddr(0x14017225c, reinterpret_cast<uintptr_t>(&k_rdata_17225c));  // 3600.0 secs/year
    RegisterExeAddr(0x1401721b8, reinterpret_cast<uintptr_t>(&k_rdata_1721b8));  // 12.0   months/year

}
