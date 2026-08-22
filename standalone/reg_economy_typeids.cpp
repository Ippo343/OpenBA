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


#include "resolve.h"
#include <cstdio>
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

// RemovalTool::CalcPath (clearance tool) type-id globals. All THREE are read only on
// the player removal path (RemovalTool::Startup + CalcPath 0x1401035e0 and its
// closure), which is not reached by autostart/default sa-run — so filling them
// un-dormants the removal path only. A/B the default build after adding.
//   0x1401c74a0 CreateRemovalDescription — RemovalTool::Startup scans the tool's
//                description entity for this component and stores it as removalDesc
//                (+0x38). CalcPath dereferences removalDesc throughout; with this
//                unfilled Startup finds nothing -> removalDesc null -> CalcPath faults.
//   0x1401c75f0 DestroyComponent — CalcPath's building-removal else-branch and the
//                StateMachine-kick branch (sub_4670 -> StateMachine_impl_dfa0) FindComponent
//                this on the target entity. Unfilled -> those branches match nothing (no-op).
//   0x1401c6860 BuildRoad (TypeInfo_BuildRoad) — RemovalTool_helper_c700 (road removal)
//                IsA/IsDerivedFrom-checks the resolved road component against this. Unfilled
//                -> road removal treats every tile as non-road (removeKind stays 0xff).
// 0x1401c73f8 CreateBridgeDescription — BridgeTool::Startup (0x1400fb7a0) scans the tool's
// description entity for this component and stores it as bridgeDesc (+0x38). EVERY bridge
// path reads it: ValidatePath (0x1400fb2d0) pulls its terrain masks from +0x20/+0x24/+0x2c/
// +0x30 and the clearance radius from +0x34, and CalcPath (0x1400f9cd0) pulls the per-size
// segment templates from +0x38[0..4]. Unregistered -> ResolveAddr returned 0 ->
// g_TypeId_CreateBridgeDescription stayed null -> Startup took its early-out and left
// bridgeDesc null -> ValidatePath bailed on its first guard, so every bridge drag reported
// invalid with length 0. Exactly the same silent-null class as the 74a0 removal fix below.
// Found 2026-07-26 by instrumenting CalcCost (desc=0000000000000000 on a live drag).
static uint32_t g_tid_CreateBridgeDescription  = 0;  // 0x1401c73f8

// 0x1401c7ac0 MultiObjectComponent — writer confirmed as MultiObjectComponent_RegisterType
// (0x14014f475). Read by FUN_140004740, the shared "find the owner's MultiObjectComponent"
// walk, and by BuildComponent_helper_8720 and BridgeTool::CalcPath. Unregistered, so the
// walk compared every component against a NULL type-id pointer, found nothing, and every
// caller silently took its no-MultiObject branch. The one that mattered:
// InteractComponent::Draw (0x140123110, the on-added slot) rebuilds the interact
// use-location list from those children, and FUN_140122b00 recurses over that list to find
// a tile a worker can stand on. A BRIDGE's own footprint is all water, so the bank
// end-pieces in that list are its only reachable access points — with the list empty,
// GatherRawMaterials bailed "no target pos" forever and a placed bridge was never built.
static uint32_t g_tid_MultiObjectComponent     = 0;  // 0x1401c7ac0

static uint32_t g_tid_CreateRemovalDescription = 0;  // 0x1401c74a0
static uint32_t g_tid_DestroyComponent         = 0;  // 0x1401c75f0
static uint32_t g_tid_BuildRoad                = 0;  // 0x1401c6860

// 0x1401c7430 CreateDragDescription — the "brush" description that the removal
// descriptor's target template (removalDesc+0x38) carries. RemovalTool_sub_48a0
// (0x1401048a0, the NATURAL-RESOURCE removability gate — despite the older
// "crop-field only" comment, FUN_1400052c0 finds a NaturalResourceComponent, id
// 0x1401c7b30, so this path is exactly how trees/stone/iron are cleared) scans the
// target template entity for this component into `matched`, then gates removability
// on `desc+0x20 flag && matched != null && cellEntity has a NaturalResourceComponent`.
// UNREGISTERED in standalone -> tool_base_path.cpp's s_pTid_7430 (and
// keystone_a_removalcalcpath.cpp's s_tid_7430) resolved to null -> sub_48a0's
// `matched` scan was skipped -> matched stayed null -> the gate failed for EVERY
// cell -> the clearance tool rendered every natural-resource tile red (not removable)
// and CalcPath never cloned/registered a ClearArea entity. CreateDragDescription is a
// registered factory type (factory_registrations.csv:250), so GetTypeIdByName resolves
// it. Fill un-dormants the player removal + drag-placement paths only (not reached by
// default sa-run); A/B the default build after adding.
static uint32_t g_tid_CreateDragDescription    = 0;  // 0x1401c7430

// 0x1401c3f78 AttendanceDescription — read by CitizenComp_sub_40f0 (0x1400440f0, the
// free-worker-slot / reachability gate shared by FindWorkplace 0x140111590,
// FindStorage 0x1401116f0, TryFindPartner 0x140111840 via sub_1930) and dereferenced
// UNGUARDED by attendanceui.cpp:51 (g_TypeId_AttendanceSubComp — same address).
// Previously unregistered -> ResolveAddr returned 0 -> sub_40f0's IsDerivedFrom got
// id 0 -> capacity 0 -> gate always false -> all three finders silently no-op'd
// (latent: their driver CitizenComponent::Update is not live-wired yet), and the
// AttendanceUI deref was a null-pointer crash-in-waiting. Audit 2026-07-19.
static uint32_t g_tid_AttendanceDescription    = 0;  // 0x1401c3f78

// 0x1401c3f40 AttendanceComponent — the id fn_FindComponent_Attendance searches by, and
// AttendanceComponent's own vtable slot 2 (0x140002650 = `MOV EAX,[0x1401c3f40]; RET`).
// The Description at 0x1401c3f78 above was registered in the 2026-07-19 audit and this
// one, 0x38 bytes earlier, was MISSED — so the finder ran with a null id pointer, matched
// nothing, and every caller got a null component back.
//
// Latent until 2026-08-15, when AttendanceUI was promoted out of UI_DIALOG_STUB: its
// Slot4 does `*(void**)(attendComp + 0x18)` exactly as retail does, so clicking the
// school read [0 + 0x18] and took an ACCESS_VIOLATION. Registering the storage is the
// upstream fix — the deref itself is faithful and stays unguarded.
static uint32_t g_tid_AttendanceComponent      = 0;  // 0x1401c3f40

// 0x1401c7d28 ResidenceDescription — read by CitizenComp_helper_a4f0's cast gate
// (citizencomponent.cpp:1495, education-vs-capacity check: null id -> res null ->
// eduLevel forced 0 -> gate always passes) and residencecomponent.cpp:102. Same
// silent-null class as the 3f78 fix above. Found by the 2026-07-19 final sweep.
static uint32_t g_tid_ResidenceDescription     = 0;  // 0x1401c7d28

// 0x1401c7d98 StateMachineComponent — the FindComponent slot used by
// FUN_140003ea0 (tool_base_path.cpp, keystone_a_removalcalcpath.cpp),
// fn_FindComponent_BuildState, and statemachinecomponent.cpp. Previously
// unregistered -> the SM pump 0x14011dfa0 (UpgradeUI click) got a null
// component and crashed reading state byte +0x52 (live 2026-07-20, first
// upgrade-button click). Same silent-null class as 3f78/7d28 above.
static uint32_t g_tid_StateMachineComponent    = 0;  // 0x1401c7d98

// 0x1401c7628 DestroyDescription — dereferenced by destroyui.cpp's
// CanRebuild (g_TypeId_DestroyComponent — misleading engine_funcs name, the
// slot is the DESCRIPTION's) and destroycomponent_states.cpp's e600 guard.
// Crashed DestroyUI_Slot7 live 2026-07-20 before the dialog-local fallback;
// registered centrally so every reader benefits.
static uint32_t g_tid_DestroyDescription       = 0;  // 0x1401c7628

// 0x1401c5d38 ToolbarDialog — the BaseTypeInfo built by RegisterType_ToolbarDialog
// (0x14014cfb5: BaseTypeInfo(&DWORD_1401c5d38, "ToolbarDialog", "UI::DialogInstance",
// 0, Factory_ToolbarDialog, ...)). Readers take the FIRST DWORD (the type id), so a
// plain uint32 storage is what the slot needs, exactly like the ids above.
//
// Read by ToolbarDialog::OnEvent case 5 (CenterOnPoint — the camera save-view
// buttons 1..5) to cast the parent dialog (self+0x28) before closing the sub-toolbar,
// and by ToolbarDialog_OnRemove / ToolbarDialog_Slot2.
//
// UNREGISTERED -> ResolveAddr returned 0 -> engine_funcs.cpp's
// g_TypeId_ToolbarDialogParent stayed NULL -> `*g_TypeId_ToolbarDialogParent`
// faulted. Crashed live 2026-08-20 on the first click of any camera save-view
// button: ACCESS_VIOLATION READ of 0 at ToolbarDialog_OnEvent+0x855, which
// disassembles to `MOV EDX,[RAX]` with RAX = the null global. The deref is faithful
// to retail (`IsDerivedFrom(pDVar6, DWORD_1401c5d38)`) and stays unguarded —
// registering the storage is the upstream fix.
//
// "ToolbarDialog" is an exe REGISTER_TYPE (ui_dialog_stub_factories.cpp:1446,
// UI_DIALOG_STUB(ToolbarDialog, "UI::DialogInstance", 0x108)), so
// Object::GetTypeIdByName resolves it — unlike the Runtime-registered UI *element*
// slots in reg_element_typeids.cpp.
static uint32_t g_tid_ToolbarDialog            = 0;  // 0x1401c5d38

// 0x1401c49f8 TerraformDescription — read by TerraformComponent's build-complete
// deformation pass (exe 0x140056b50) to confirm the component's description (+0x18)
// really is a TerraformDescription before reading the floor/ceiling picking meshes at
// desc +0x20 / +0x28. Unregistered -> ResolveAddr 0 -> the IsDerivedFrom test is skipped
// and the pass bails, so the tunnel mouth never gets its heightmap carve.
static uint32_t g_tid_TerraformDescription     = 0;  // 0x1401c49f8

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
    RegisterExeAddr(0x1401c73f8, reinterpret_cast<uintptr_t>(&g_tid_CreateBridgeDescription));
    RegisterExeAddr(0x1401c7ac0, reinterpret_cast<uintptr_t>(&g_tid_MultiObjectComponent));
    RegisterExeAddr(0x1401c74a0, reinterpret_cast<uintptr_t>(&g_tid_CreateRemovalDescription));
    RegisterExeAddr(0x1401c75f0, reinterpret_cast<uintptr_t>(&g_tid_DestroyComponent));
    RegisterExeAddr(0x1401c6860, reinterpret_cast<uintptr_t>(&g_tid_BuildRoad));
    RegisterExeAddr(0x1401c7430, reinterpret_cast<uintptr_t>(&g_tid_CreateDragDescription));
    RegisterExeAddr(0x1401c3f78, reinterpret_cast<uintptr_t>(&g_tid_AttendanceDescription));
    RegisterExeAddr(0x1401c3f40, reinterpret_cast<uintptr_t>(&g_tid_AttendanceComponent));
    RegisterExeAddr(0x1401c7d28, reinterpret_cast<uintptr_t>(&g_tid_ResidenceDescription));
    RegisterExeAddr(0x1401c7d98, reinterpret_cast<uintptr_t>(&g_tid_StateMachineComponent));
    RegisterExeAddr(0x1401c7628, reinterpret_cast<uintptr_t>(&g_tid_DestroyDescription));
    RegisterExeAddr(0x1401c5d38, reinterpret_cast<uintptr_t>(&g_tid_ToolbarDialog));
    RegisterExeAddr(0x1401c49f8, reinterpret_cast<uintptr_t>(&g_tid_TerraformDescription));
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
    g_tid_CreateBridgeDescription  = ::Object::GetTypeIdByName("CreateBridgeDescription");
    g_tid_MultiObjectComponent     = ::Object::GetTypeIdByName("MultiObjectComponent");
    g_tid_CreateRemovalDescription = ::Object::GetTypeIdByName("CreateRemovalDescription");
    g_tid_DestroyComponent         = ::Object::GetTypeIdByName("DestroyComponent");
    g_tid_BuildRoad                = ::Object::GetTypeIdByName("BuildRoad");
    g_tid_CreateDragDescription    = ::Object::GetTypeIdByName("CreateDragDescription");
    g_tid_AttendanceDescription    = ::Object::GetTypeIdByName("AttendanceDescription");
    g_tid_AttendanceComponent      = ::Object::GetTypeIdByName("AttendanceComponent");
    g_tid_ResidenceDescription     = ::Object::GetTypeIdByName("ResidenceDescription");
    g_tid_StateMachineComponent    = ::Object::GetTypeIdByName("StateMachineComponent");
    g_tid_DestroyDescription       = ::Object::GetTypeIdByName("DestroyDescription");
    g_tid_ToolbarDialog            = ::Object::GetTypeIdByName("ToolbarDialog");
    g_tid_TerraformDescription     = ::Object::GetTypeIdByName("TerraformDescription");
    // Graduation check: a registered-but-ZERO slot is invisible to every
    // null-POINTER guard downstream (IsDerivedFrom would just match nothing and the
    // sub-toolbar would silently stay open). Say so loudly if the name ever moves.
    if (g_tid_ToolbarDialog == 0) {
        fprintf(stderr, "[TypeId] WARNING: GetTypeIdByName(\"ToolbarDialog\") == 0 — "
                        "camera save-view buttons will no-op their sub-dialog close\n");
    }
}

