// standalone/reg_economy_typeids.cpp
//
// Type-id .rdata globals needed to make BUILDING component-finds resolve in the
// standalone exe, so the citizen work/economy loop can validate + path to a workplace.
//
// In standalone the .rdata type-id globals are 0xFFFFFFFF (unfilled) unless we both
// (a) register the global's address -> a storage uint32_t and (b) fill that storage
// from GetTypeIdByName AFTER Engine::Startup (Object::Factory::Startup assigns the real
// CRC ids only then; InvokeAllRegistrations is too early). Exe reimpls that read these
// globals via ResolveAddr(Module::Exe, 0x1401cXXXX) then get the real CRC and their
// IsDerivedFrom / GetTypeId-keyed FindComponent scans match.
//
// These are SHARED globals — filling them un-dormants every reader (not just the citizen
// path). Add ids here only as the economy path needs them, and A/B the DEFAULT build
// (envs off) after each addition to catch world-build/render regressions.
//
//   0x1401c7938  InteractDescription  -- read by FUN_140123070 (Pf_RecursiveChildSum,
//                position-count of a building's InteractComponent) via Entity_impl_7fb0;
//                with it null, 7fb0 can't resolve a workplace's cell -> CheckWorkplaceValid
//                fails -> State_Work can't proceed. (InteractComponent 0x1401c7900 is
//                already registered+filled by RegisterWorkTailFireBatch.)
//
//   0x1401c79e0  MapComponent  -- read by FUN_140004680 (FloorComponent_FindResData,
//                the "find MapComponent on owner" scan). Entity_impl_7fb0 (workplace anchor
//                cell) tries PATH 1 (InteractComponent work-positions) first; a StorageBarn
//                genuinely has none (it's storage, not a production workplace), so PATH 1
//                returns 0 and 7fb0 falls to PATH 2, which finds the building's MapComponent
//                and reads its packed tile coords (+0x28 tileX / +0x2a tileY / +0x2c fpW /
//                +0x2e fpH -- written at spawn by starttown.cpp) to compute the building's
//                center cell. With c79e0 unfilled, FloorComponent_FindResData scans for
//                type-id 0, matches nothing, returns null -> PATH 2 fails too -> 7fb0 = -1
//                -> CheckWorkplaceValid invalid -> State_Work can't path to the workplace.
//
//   0x1401c80e0  WorkPlaceComponent  -- the occupant-ROSTER controller. AssignHousing
//                (FUN_140111ee0) finds the house's WorkPlaceComponent via FUN_140002e10
//                (a GetTypeId==c80e0 scan over the residence entity's component array) and
//                rosters the citizen into its +0x58/+0x60 worker list. With c80e0 unfilled
//                the scan matches nothing -> a null building IBP is handed to the attach
//                (FUN_14012fa20) -> read of comp+0x60 faults. Needed so the founding-housing
//                pass can drive the FULL faithful AssignHousing (houses track occupants).
//
//   0x1401c8118  WorkPlaceDescription  -- read by the attach/detach (FUN_14012fa20 /
//                FUN_14012f7e0) to validate the WorkPlaceComponent's description before the
//                "occupied" mesh visual: `IsDerivedFrom(comp+0x18, c8118) ? desc : null` then
//                `jobSlot == desc+0x20`. With c8118 unfilled the IsDerivedFrom returns false,
//                desc collapses to null, and `*(desc+0x20)` faults (read of 0x20). Needed so
//                the attach's ModelKick(mode 2)/MeshFlag(mode 4) "occupied" visual runs.
//
//   0x1401c7e08  StatusIconComponent  -- the building's renderable status-icon mesh that
//                the attach's "occupied" toggle acts on. The attach finds it via FUN_140002f90
//                (Entity_FindComponent_MeshInstance, a c7e08 scan over the house entity) and
//                passes it to FUN_14012c3f0, which toggles the mesh visibility (Scene
//                Add/RemoveObject + MeshInstance::Update). With c7e08 unfilled the find
//                returns a null component and FUN_14012c3f0 faults reading mesh+0x10 (READ of
//                0x10). The same global is the SHARED MeshInstance/StatusIcon finder used by
//                the Work classes (FUN_140002f90) — those paths need an employed citizen, not
//                reached in the default Easy town, so A/B the default build after filling.

#ifdef STANDALONE_EXE

#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>

static uint32_t g_tid_InteractDescription = 0;   // 0x1401c7938
static uint32_t g_tid_MapComponent        = 0;   // 0x1401c79e0
static uint32_t g_tid_WorkPlaceComponent  = 0;   // 0x1401c80e0
static uint32_t g_tid_WorkPlaceDescription = 0;  // 0x1401c8118
static uint32_t g_tid_StatusIconComponent = 0;   // 0x1401c7e08

// Register the .rdata global storage (value filled later by FillEconomyTypeIdGlobals).
void RegisterEconomyTypeIds() {
    RegisterExeAddr(0x1401c7938, reinterpret_cast<uintptr_t>(&g_tid_InteractDescription));
    RegisterExeAddr(0x1401c79e0, reinterpret_cast<uintptr_t>(&g_tid_MapComponent));
    RegisterExeAddr(0x1401c80e0, reinterpret_cast<uintptr_t>(&g_tid_WorkPlaceComponent));
    RegisterExeAddr(0x1401c8118, reinterpret_cast<uintptr_t>(&g_tid_WorkPlaceDescription));
    RegisterExeAddr(0x1401c7e08, reinterpret_cast<uintptr_t>(&g_tid_StatusIconComponent));
}

// Fill from the real-CRC type tree — call AFTER Engine::Startup.
extern "C" void FillEconomyTypeIdGlobals() {
    g_tid_InteractDescription  = ::Object::GetTypeIdByName("InteractDescription");
    g_tid_MapComponent         = ::Object::GetTypeIdByName("MapComponent");
    g_tid_WorkPlaceComponent   = ::Object::GetTypeIdByName("WorkPlaceComponent");
    g_tid_WorkPlaceDescription = ::Object::GetTypeIdByName("WorkPlaceDescription");
    g_tid_StatusIconComponent  = ::Object::GetTypeIdByName("StatusIconComponent");
}

#endif // STANDALONE_EXE
