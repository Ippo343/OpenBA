// Animal spawn wave — HerdComponent live bring-up (deer herds).
// OWNED BY the herd bring-up lane: put every RegisterExeAddr / helper this lane
// needs HERE (plus code in code/Game/Component/herdcomponent.cpp and, if needed,
// code/Game/Component/populationcomponent.cpp). Shared type-id globals are already
// published by reg_component_animal.cpp — do NOT re-register them.
// Hook is already wired: address_registry.cpp calls RegisterHerdLive() after
// RegisterComponentBatch_animal().
//
// Retail herd-creation chain (all verified in Ghidra, Application-x64.exe):
//   StartTown simple pool spawns the "PopulationDeer" template entity
//     -> ComponentEntity_CtorFromDescription -> StartupComponents
//        -> PopulationComponent vtable slot 4 = 0x140054220
//           (Ghidra calls it PopulationComponent_Serialize; it is the Startup)
//           -> builds the habitability grid, then per herd-grid cell:
//              instantiate popDesc+0x50 (the herd template entity),
//              point its HerdComponent at us, roll a size, and
//              -> HerdComponent_Populate 0x140120f50
//                 (Ghidra: PopulationComponent_helper_0f50 — it is a
//                  HerdComponent method, param_1 is the herd)
//                 -> HerdComponent_StartMigration 0x140122340
//                 -> per animal: HerdComponent_helper_7400 0x140107400
//                    + HerdComponent_impl_e510 0x14012e510 + RegisterEntity
//   From then on HerdComponent::Update (slot 9) grazes/migrates and grows the
//   herd through AdjustSize.
//
// The vtable slot wiring itself lives in the COMPONENT_LIVE_FACTORY_* macros in
// herdcomponent.cpp / populationcomponent.cpp — registering an address here only
// makes it reachable for OTHER callers that still go through ResolveAddr.
//
// Gate: BANISHED_LIVE_HERD or BANISHED_LIVE_POPULATION. Both halves of the chain
// need the same helper bodies, and the useful recipe turns on both anyway.
#include "resolve.h"
#include "Component/component_factory_base.h"
#include "Component/herdcomponent_funcs.h"
#include "Component/populationcomponent_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>

// ---- Storage for .rdata cells this lane is the first to need ----------------

// DAT_140172110 = 0.4f — the sub-tile jitter HerdComponent_Populate adds to each
// animal's spawn position (verified from retail .rdata: 0x3ECCCCCD).
static float    g_f_HerdPopulateJitter    = 0.4f;   // 0x140172110
// DWORD_1401c47c8 = "PopulationDescription" type id (PopulationDescription_Register
// @0x14014b5e5). PopulationComponent::Startup reads it to validate its +0x18.
// Filled lazily on first use by PopulationTypeId() in populationcomponent.cpp, so
// this file only has to provide the storage (no ordering dependency on a Fill*
// pass in address_registry.cpp).
static uint32_t g_tid_PopulationDescription = 0;    // 0x1401c47c8

// ---- Trampolines matching the retail calling shapes -------------------------

static void Herd_Helper7400_Thunk(void* animalComp, void* pos, float yaw, char pose) {
    HerdComponent_Helper7400(reinterpret_cast<int64_t>(animalComp), pos, yaw, pose);
}
static void Herd_Populate_Thunk(void* herdComp, int count) {
    HerdComponent_Populate(reinterpret_cast<int64_t>(herdComp), count);
}
static void Herd_OnAdd_Thunk(void* self)    { HerdComponent_OnAdd(self); }
static void Herd_OnRemove_Thunk(void* self) { HerdComponent_OnRemove(self); }
static void Pop_Startup_Thunk(void* self) {
    PopulationComponent_Startup(reinterpret_cast<PopulationComponent*>(self));
}
static void Pop_OnAdd_Thunk(void* self)    { PopulationComponent_OnAdd(self); }
static void Pop_OnRemove_Thunk(void* self) { PopulationComponent_OnRemove(self); }

static bool HerdWaveEnabled() {
    return ::ComponentFactory::LiveEnabled("BANISHED_LIVE_HERD")
        || ::ComponentFactory::LiveEnabled("BANISHED_LIVE_POPULATION");
}

void RegisterHerdLive() {
    if (!HerdWaveEnabled()) return;

    // Storage first — the ports below read these through ResolveAddr.
    RegisterExeAddr(0x140172110, reinterpret_cast<uintptr_t>(&g_f_HerdPopulateJitter));
    RegisterExeAddr(0x1401c47c8, reinterpret_cast<uintptr_t>(&g_tid_PopulationDescription));

    // HerdComponent_helper_7400 — place one spawned animal (transform + pose mesh
    // + behavior kick). Was the last unported leaf of the spawn path.
    RegisterExeAddr(0x140107400, reinterpret_cast<uintptr_t>(&Herd_Helper7400_Thunk));

    // HerdComponent::Populate — the per-herd initial seeding pass.
    RegisterExeAddr(0x140120f50, reinterpret_cast<uintptr_t>(&Herd_Populate_Thunk));

    // HerdComponent vtable slots 6 / 7.
    RegisterExeAddr(0x140121f40, reinterpret_cast<uintptr_t>(&Herd_OnAdd_Thunk));
    RegisterExeAddr(0x140121e20, reinterpret_cast<uintptr_t>(&Herd_OnRemove_Thunk));

    // PopulationComponent vtable slot 4 — the herd-creation entry point.
    RegisterExeAddr(0x140054220, reinterpret_cast<uintptr_t>(&Pop_Startup_Thunk));

    // PopulationComponent vtable slots 6 / 7 — the TownManager population-registry
    // append/erase at (GameWorld+0xF0)+0xC0. That array is what DebugUI::Slot4 walks
    // to build the debug panel's "deer population" combo; with slot 6 never installed
    // it stayed empty and the combo had no rows.
    RegisterExeAddr(0x1400541c0, reinterpret_cast<uintptr_t>(&Pop_OnAdd_Thunk));
    RegisterExeAddr(0x140054180, reinterpret_cast<uintptr_t>(&Pop_OnRemove_Thunk));
}
