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
static uint32_t g_tid_StatusIconDescription = 0;  // 0x1401c7e40
// 0x1401c7cf0 ResidenceComponent — the worker-ROSTER controller (extends
// ControllerComponent, roster count @+0x58). Read by Entity_FindComponent_Storage
// (exe 0x140008090, called from CitizenComponent::Update's multi-worker section to
// read the workplace's roster capacity). With c7cf0 unfilled the finder compares
// every component's GetTypeId() against 0 -> never matches -> returns null -> the
// Update's unguarded `storage+0x58` read faults. (Also read by the residence/townhall
// UIs and the CitizenAI find helpers, which already keep their own self-contained copy.)
static uint32_t g_tid_ResidenceComponent  = 0;   // 0x1401c7cf0

// 0x1401c7740 ForesterComponent (extends ResourceProduceComponent) and
// 0x1401c7778 ForesterDescription (extends Description) — read by ForesterWork
// (exe 0x1400eaec0, the forester work-state decision tick). ForesterWork scans the
// workplace entity's component array for the ForesterComponent (c7740 GetTypeId match
// + IsDerivedFrom), then reads that component's description (+0x18) and checks it
// IsDerivedFrom ForesterDescription (c7778) to recover the produced-resource category
// at desc+0x28 (used to index the town stock-limit records at townMgr+0x170). With
// these unfilled the type-id compares are against 0 -> the scans never match -> the
// component/description collapse to null -> ForesterWork bails (or faults reading
// desc+0x28). Only un-dormants the Forester producer path (no ForesterLodge / employed
// forester exists on the default Easy town), so default-safe; A/B the default build.
static uint32_t g_tid_ForesterComponent   = 0;   // 0x1401c7740
static uint32_t g_tid_ForesterDescription = 0;   // 0x1401c7778

// Three more type-id globals ForesterWork::Execute (0x1400eaec0) derefs to find the work
// inputs; all UNREGISTERED in standalone (their ResolveAddr -> 0), so foresterwork.cpp's
// cached pointers were null and `*ptr` faulted (the RadiusDescription one was the line-519
// crash). Filling them lets ForesterWork resolve the workplace radius, the harvestable
// resource description, and the worker-capacity description:
//   0x1401c4870  RadiusDescription  -- the workplace work-radius (scanned on the work-area
//                sub-object; ForesterWork reads its radius/category at +0x28).
//   0x1401c7b68  NaturalResourceDescription  -- the harvestable resource (tree) description;
//                ForesterWork checks its harvest flags (+0x5c bits) before pushing a cut.
//   0x1401c80a8  WorkDescription  -- the worker-capacity description read by CalcMaxWorkers
//                (FUN_14012e5c0) to bound the number of simultaneous foresters.
// Only on the Forester producer path (no employed forester on the default Easy town), so
// default-safe; A/B the default build after filling.
static uint32_t g_tid_RadiusDescription   = 0;   // 0x1401c4870
static uint32_t g_tid_NaturalResourceDescription = 0;  // 0x1401c7b68
static uint32_t g_tid_WorkDescription      = 0;   // 0x1401c80a8

// 0x1401c8070 WorkComponent — the work-area's worker-capacity component. ForesterWork::Execute
// finds it via FUN_140003050 (reimplemented in work_base_reimpl.cpp, which reads this id) and
// reads its capacity at +0x30 (CalcMaxWorkers). UNREGISTERED in standalone -> the find scanned
// for id 0 -> returned a null IBP -> the original's unguarded `matDesc+0x30` read faulted. The
// lodge carries a WorkComponent (the Phase A dump's comp[9]); filling this lets the find match.
static uint32_t g_tid_WorkComponent        = 0;   // 0x1401c8070

// 0x1401c7e78 StorageComponent — read by the SHARED storage finder FUN_140006270
// (WorkBase_FindComponent_6270) and its callers across the eat/consume chain:
// EatFromStorage (0x1401152e0), Entity_impl_ef40 slot pick, the e370/dd30 mask
// fallbacks, ConsumeListAppend 0x1400c64b0 (reserve bump on the citizen's own
// inventory), cd80 accept-check, c560 flag test, QueryCount cf10. Unfilled, every
// one of those finds returned null and the retail-faithful unguarded derefs
// faulted the moment Brick M-D made town storage live (4 separate crash sites).
// Filling follows the same pattern as the 13 ids above; inert storage components
// found by newly-matching scans have zeroed bodies (count 0 / flags 0), so
// dormant readers see empty-storage no-ops. A/B the default build.
static uint32_t g_tid_StorageComponent     = 0;   // 0x1401c7e78

// 0x1401c7190 CitizenDescription — read (via g_TypeId_CitizenDescription) by
// CitizenBehavior_CollectStats to fetch maxHappiness/maxHealth from the first
// citizen's description for the town-hall graph percentage series. Null-guarded
// there (percentages just record 0 when unfilled), so filling is additive.
static uint32_t g_tid_CitizenDescription   = 0;   // 0x1401c7190

// 0x1401c7c80 RawMaterialComponent — read by ClearArea::Execute Phase 3 (cleararea.cpp,
// mislabelled there as g_TypeId_NaturalResourceDesc) to find a stored-raw-material
// component on a cleared tile before dispatching a GatherWork. UNREGISTERED in
// standalone -> the `*g_TypeId_NaturalResourceDesc` deref faulted the moment the
// ClearAreaComponent harvest state machine (BANISHED_LIVE_CLEARCTRL) first drove a
// citizen through ClearArea::Execute. Trees carry a NaturalResourceComponent, not a
// RawMaterialComponent, so the newly-matching find simply returns null there and the
// phase no-ops — the fill just stops the crash. A/B the default build.
static uint32_t g_tid_RawMaterialComponent = 0;   // 0x1401c7c80

// Register the .rdata global storage (value filled later by FillEconomyTypeIdGlobals).
void RegisterEconomyTypeIds() {
    RegisterExeAddr(0x1401c7938, reinterpret_cast<uintptr_t>(&g_tid_InteractDescription));
    RegisterExeAddr(0x1401c79e0, reinterpret_cast<uintptr_t>(&g_tid_MapComponent));
    RegisterExeAddr(0x1401c80e0, reinterpret_cast<uintptr_t>(&g_tid_WorkPlaceComponent));
    RegisterExeAddr(0x1401c8118, reinterpret_cast<uintptr_t>(&g_tid_WorkPlaceDescription));
    RegisterExeAddr(0x1401c7e08, reinterpret_cast<uintptr_t>(&g_tid_StatusIconComponent));
    RegisterExeAddr(0x1401c7e40, reinterpret_cast<uintptr_t>(&g_tid_StatusIconDescription));
    RegisterExeAddr(0x1401c7cf0, reinterpret_cast<uintptr_t>(&g_tid_ResidenceComponent));
    RegisterExeAddr(0x1401c7740, reinterpret_cast<uintptr_t>(&g_tid_ForesterComponent));
    RegisterExeAddr(0x1401c7778, reinterpret_cast<uintptr_t>(&g_tid_ForesterDescription));
    RegisterExeAddr(0x1401c4870, reinterpret_cast<uintptr_t>(&g_tid_RadiusDescription));
    RegisterExeAddr(0x1401c7b68, reinterpret_cast<uintptr_t>(&g_tid_NaturalResourceDescription));
    RegisterExeAddr(0x1401c80a8, reinterpret_cast<uintptr_t>(&g_tid_WorkDescription));
    RegisterExeAddr(0x1401c8070, reinterpret_cast<uintptr_t>(&g_tid_WorkComponent));
    RegisterExeAddr(0x1401c7e78, reinterpret_cast<uintptr_t>(&g_tid_StorageComponent));
    RegisterExeAddr(0x1401c7190, reinterpret_cast<uintptr_t>(&g_tid_CitizenDescription));
    RegisterExeAddr(0x1401c7c80, reinterpret_cast<uintptr_t>(&g_tid_RawMaterialComponent));
}

// Fill from the real-CRC type tree — call AFTER Engine::Startup.
extern "C" void FillEconomyTypeIdGlobals() {
    g_tid_InteractDescription  = ::Object::GetTypeIdByName("InteractDescription");
    g_tid_MapComponent         = ::Object::GetTypeIdByName("MapComponent");
    g_tid_WorkPlaceComponent   = ::Object::GetTypeIdByName("WorkPlaceComponent");
    g_tid_WorkPlaceDescription = ::Object::GetTypeIdByName("WorkPlaceDescription");
    g_tid_StatusIconComponent  = ::Object::GetTypeIdByName("StatusIconComponent");
    g_tid_StatusIconDescription = ::Object::GetTypeIdByName("StatusIconDescription");
    g_tid_ResidenceComponent   = ::Object::GetTypeIdByName("ResidenceComponent");
    g_tid_ForesterComponent    = ::Object::GetTypeIdByName("ForesterComponent");
    g_tid_ForesterDescription  = ::Object::GetTypeIdByName("ForesterDescription");
    g_tid_RadiusDescription    = ::Object::GetTypeIdByName("RadiusDescription");
    g_tid_NaturalResourceDescription = ::Object::GetTypeIdByName("NaturalResourceDescription");
    g_tid_WorkDescription      = ::Object::GetTypeIdByName("WorkDescription");
    g_tid_WorkComponent        = ::Object::GetTypeIdByName("WorkComponent");
    g_tid_StorageComponent     = ::Object::GetTypeIdByName("StorageComponent");
    g_tid_CitizenDescription   = ::Object::GetTypeIdByName("CitizenDescription");
    g_tid_RawMaterialComponent = ::Object::GetTypeIdByName("RawMaterialComponent");
}

#endif // STANDALONE_EXE
