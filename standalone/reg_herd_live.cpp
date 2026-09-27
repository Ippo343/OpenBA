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
// HerdComponent / PopulationComponent are real classes (ENDGAME B.3.8): their
// vtable slots (Herd 6/7, Population 4/6/7) are the compiler's, and nothing read
// those addresses back, so their rows and thunks are gone. What is left here are
// the helper doors the ports resolve by address.
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

void RegisterHerdLive() {
    // Storage first — the ports below read these through ResolveAddr.
    RegisterExeAddr(0x140172110, reinterpret_cast<uintptr_t>(&g_f_HerdPopulateJitter));
    RegisterExeAddr(0x1401c47c8, reinterpret_cast<uintptr_t>(&g_tid_PopulationDescription));

    // HerdComponent_helper_7400 — place one spawned animal (transform + pose mesh
    // + behavior kick). Was the last unported leaf of the spawn path.
    RegisterExeAddr(0x140107400, reinterpret_cast<uintptr_t>(&Herd_Helper7400_Thunk));

    // HerdComponent::Populate — the per-herd initial seeding pass.
    RegisterExeAddr(0x140120f50, reinterpret_cast<uintptr_t>(&Herd_Populate_Thunk));
}
