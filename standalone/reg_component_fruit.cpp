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
//                  g_tid_NaturalResourceComponent) — read inside FUN_1400052c0.
//   * 0x1401c3dd8  game-world singleton global (core, registered for growth/pop).
//   * 0x140172140 (0.98 g_fUVScale), 0x1401721d0 (32.0), 0x140172234 (300.0),
//     0x140172288 (-10.0), 0x1401722b0 (fabs mask) — all reg_component_nature.cpp.
//   * 0x14017214c (1.0) — gameupdateloop_load.cpp.
//   * fn_IsDerivedFrom / fn_InternalBasePtr_CheckPtr / fn_Time_AsSeconds are
//     engine_funcs globals resolved elsewhere.
//
// The integrator wires RegisterComponentBatch_fruit() into address_registry.cpp and
// FillComponentTypeIdGlobals_fruit() into game_main.cpp (after Engine::Startup).
#include "resolve.h"
#include "Object/factory_registration.h"   // Object::GetTypeIdByName
#include <cstdint>

// ------------------------------------------------------------------
// FruitDescription type-id .rdata global (DAT_1401c4288). In the original exe this
// is .data filled by FruitDescription_Register (0x14014aca5,
// Object::BaseTypeInfo at &DAT_1401c4288 with name "FruitDescription"). In
// standalone WE provide the storage (registered below at AddressRegistryInit) and
// FILL it after Engine::Startup via Object::GetTypeIdByName("FruitDescription") —
// GetTypeIdByName returns the real CRC id only once Object::Factory::Startup has
// run (0xFFFFFFFF sentinel earlier). FruitComponent_Update validates its
// description (_unk18) with IsDerivedFrom(_, *DAT_1401c4288); if this global is 0
// the check fails -> resData=null -> the live Update early-returns (no ripening).
// So filling it is REQUIRED for the live path, not optional.
static uint32_t g_tid_FruitDescription = 0;  // 0x1401c4288

// FruitComponent type-id .rdata global (DAT_1401c4250, writer FruitComponent_RegisterType
// 0x14014ac40 per factory_registrations.csv). This slot was registered NOWHERE, so the
// shared sibling finder FUN_14000a0b0 — which scans an entity for *DAT_1401c4250 — missed on
// EVERY entity forever. Found 2026-08-10 while porting OrchardComponent::OnMessage: msg 0
// binds the fruit component of a freshly planted tree into the orchard slot at +0x18, and
// msg 2 (the growth boost) drives that component's Update; with a null id the slot bind
// silently downgrades to state 2 ("planted but unusable") and the boost is a no-op.
// Same class of defect as the GrowthComponent slot c77b0 — see reg_growth_typeid.cpp.
static uint32_t g_tid_FruitComponent = 0;    // 0x1401c4250

extern "C" void FillComponentTypeIdGlobals_fruit() {
    g_tid_FruitDescription = ::Object::GetTypeIdByName("FruitDescription");
    g_tid_FruitComponent   = ::Object::GetTypeIdByName("FruitComponent");
}

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

    // FruitDescription type-ID global (filled after Engine::Startup — see above).
    RegisterExeAddr(0x1401c4288, reinterpret_cast<uintptr_t>(&g_tid_FruitDescription));
    RegisterExeAddr(0x1401c4250, reinterpret_cast<uintptr_t>(&g_tid_FruitComponent));
}
