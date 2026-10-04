#pragma once
// code/Game/Component/exe_bodies.h -- declarations of the C bodies defined under
// code/Game/Component/ that were reached through the address registry (a retail exe
// address registered with RegisterExeAddr and read back by ResolveAddr(Module::Exe, ...),
// a file-local helper, an X_ResolveFuncs or an engine_funcs global).
//
// B3R_RULES R5: every DIRECT body is declared in exactly one header, written from its
// DEFINITION (linkage, parameter and return types as defined), and the defining .cpp
// includes this header so the compiler checks the pair. Readers call the body by name
// through this header, never through a local extern prototype.
// Grouped by defining file; the retail address is the trailing comment on each line.
// Bodies already declared in another header are NOT repeated here (see
// docs/runtime_port/endgame_b/b3r_session/data/P0b_declared.tsv, which maps every
// address to its name and declaring header). Members are B.4's work, not this file's.
//
// Must compile when included first in an otherwise empty translation unit.

#include <cstdint>

class CitizenComponent;

// ---- ambient_externalptr_serialize.cpp
namespace Audio { class SoundEffect; }
namespace IO { class Serializer; }
extern "C" void AmbientExternalPtrSerialize_Reimpl(Audio::SoundEffect** field, IO::Serializer* serializer);  // 0x1400019a0

// ---- ambientemittercomponent.cpp
extern "C" void AmbientEmitterResize_Reimpl(void* arrayBase, int newCount);  // 0x140001770

// ---- ambientenvironmentdescription.cpp
extern "C" void AmbientArrayResize_Reimpl(void* arrayBase, int newCount);  // 0x140002540

// ---- attendancecomponent.cpp
void AttendanceComponent_ReleasePartnerSlot(void* workerP, char reroll);  // 0x140115a50
void AttendanceComponent_ReleaseLink1650(void* workerP);  // 0x140111650
void AttendanceComponent_ReleaseLink59b0(void* workerP);  // 0x1401159b0
void* AttendanceComponent_GetJobHostComponent(void* entity, void* outPtr);  // 0x140002e10 (returns outPtr)

// ---- buildcomponent.cpp
int BuildComponent_FindInQueue(void* arrayBase, void* entityPtr);  // 0x140017550

// ---- cemeterycomponent.cpp
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);  // 0x140002cc0

// ---- charactercomponent.cpp
class Action;
void Character_UpdateAnimation_Reimpl(void* arrayBase, Action** ptrRef, void* deltaTime);  // 0x1400c4f80

// ---- charmanager.cpp
extern "C" void CharMgr_sub_8660(float* mgrF, int idx);  // 0x140018660

// ---- citizen_assign_citizens.cpp
extern "C" void CitizenAssignCitizens(void* ctxV);  // 0x1400d43b0

// ---- citizen_assignworkplace.cpp
extern "C" void CitizenComponent_AssignWorkplace(void* selfV, void* workplaceIBP, int slotIndex);  // 0x1401163d0

// ---- citizen_employ_cascade.cpp
extern "C" void* CitizenEmploy_Strat_cbf0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);  // 0x14008cbf0
extern "C" void* CitizenEmploy_Strat_ca20(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);  // 0x14008ca20
extern "C" void* CitizenEmploy_Strat_c5f0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);  // 0x14008c5f0
extern "C" void* CitizenEmploy_Strat_c4b0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);  // 0x14008c4b0
extern "C" void* CitizenList_AssignWorker_c3d0(int64_t townMgr, void* out, int64_t* availPtr, int64_t profKey);  // 0x14008c3d0

// ---- citizen_lifeevent.cpp

// ---- citizen_lifeevent_handler.cpp

// ---- citizen_needsupdate.cpp

// ---- citizen_reassign_workers.cpp
extern "C" void PathGraph_ReassignWorkers(void* ctxV);  // 0x1400d62a0

// ---- citizen_updatehealth.cpp
extern "C" void* Map_RadiusQuery_92990(float* map, void** out, float* pos, float* radius, int typeId, void* outArr);  // 0x140092990
extern "C" void CitizenComp_Infect(void* self, void* disease);  // 0x140116b90

// ---- citizenai_actionctors.cpp
void CitizenAI_PerformWorkCtor4_c1070(void* param_1, int64_t* param_2, float* param_3, int param_4, uint8_t param_5, uint32_t param_6);  // 0x1400c1070
void CitizenAI_PerformWorkCtor5_bd070(void* param_1, uint64_t* param_2, float* param_3, int64_t param_4);  // 0x1400bd070
void CitizenAI_PerformWorkCtor6_bd420(void* param_1, int64_t* param_2, float* param_3, char param_4);  // 0x1400bd420
void CitizenAI_PerformWorkCtor8_bd790(void* param_1, uint64_t* param_2, float* param_3);  // 0x1400bd790
void CitizenAI_PerformWorkCtor10_bccb0(void* param_1, uint64_t* param_2, float* param_3, uint64_t param_4);  // 0x1400bccb0
void CitizenAI_PerformWorkCtor11_bc790(void* param_1, int64_t param_2, int64_t* param_3, float* param_4, int64_t* param_5, float* param_6);  // 0x1400bc790
void CitizenAI_Pathfinding_NavigateTo_bda50(void* param_1, int64_t* param_2, uint32_t* param_3, uint8_t );  // 0x1400bda50
void CitizenAI_Pathfinding_WalkTowards_bb490(void* param_1, uint32_t param_2, uint32_t* param_3, uint32_t param_4, float param_5, uint32_t param_6);  // 0x1400bb490
void CitizenAI_PlayAnimationCtor4_ba6b0(void* param_1, int64_t* param_2, uint32_t* param_3, float param_4, uint32_t param_5);  // 0x1400ba6b0

// ---- citizenai_executor.cpp
extern "C" void CitizenAI_ActionExecutor_c4c50(int64_t* param_1, void* param_2);  // 0x1400c4c50

// ---- citizenai_findhelpers.cpp
bool ResidenceCheck_2ac30(int64_t param_1);  // 0x14012ac30
int64_t* Entity_impl_ec70(int64_t param_1, int64_t* param_2, uint32_t param_3, float* param_4);  // 0x14008ec70
int64_t* CitizenAI_helper_e370(int64_t* param_1, int64_t* param_2, uint32_t param_3, int64_t* param_4, float* param_5, uint64_t , int64_t* param_7);  // 0x14008e370

// ---- citizenai_findtarget.cpp
class ComponentEntity;
class MarketLocationComponent;
namespace Object { template <class T> class InternalPtr; }
::Object::InternalPtr<MarketLocationComponent>* CitizenAI_sub_78c0(
    ComponentEntity* param_1, ::Object::InternalPtr<MarketLocationComponent>* param_2);  // 0x1400178c0
int64_t* CitizenAI_FindBestTarget_dd30(int64_t* param_1, int64_t* param_2, uint32_t param_3, uint32_t param_4, int64_t* param_5, float* param_6, uint32_t param_7);  // 0x14008dd30

// ---- citizenai_gather.cpp
extern "C" int Storage_GetResourceCount2(int64_t param_1);  // 0x14012ab00

// ---- citizenai_growup.cpp

// ---- citizenai_housing_idle.cpp
extern "C" void CitizenAI_Reg_helper_e360(int64_t* a, int64_t* b);  // 0x14000e360

// ---- citizenai_work_scheduler.cpp
extern "C" int CitizenAI_WorkScheduler_Insert(int* sched, void* pWorkIBP, uint32_t code, char fromExisting, int prio);  // 0x1400f88a0
extern "C" void CitizenAI_WorkScheduler_Post(uintptr_t workComp, uint32_t code, char flag, void* pWorkIBP);  // 0x14012ec60
extern "C" void CitizenAI_WorkScheduler_Rekey(uintptr_t sched, uint32_t workId, uint64_t oldKey, uint64_t newKey);  // 0x1400f8400

// ---- citizenaicomponent.cpp
void* CitizenComp_Entity_FindComponent_Storage(int64_t entity, void* outIBP);  // 0x140008090
int CitizenComp_StorageComponent_QueryCount(int64_t self);  // 0x14012cf10
int CitizenComp_Storage_GetResourceCount(int64_t self);  // 0x14012a9d0

// ---- citizencomponent.cpp
void CitizenComp_sub_1fa0(int64_t selfV, void* param2);  // 0x140111fa0
int64_t* CitizenComp_sub_ffc0(int64_t param_1, int64_t* param_2);  // 0x14010ffc0
int64_t* CitizenComp_sub_0470(int64_t param_1, int64_t* param_2);  // 0x140110470
void CitizenComp_helper_1b90(int64_t selfV);  // 0x140111b90
int64_t* CitizenComp_Entity_impl_ef40(void* param_1, int64_t* param_2, void* param_3, int param_4);  // 0x14008ef40
void CitizenComp_AssignHousing(CitizenComponent* param_1, int64_t* param_2);  // 0x140111ee0
char CitizenComp_TryEnrolInSchool(CitizenComponent* param_1, int64_t* param_2);  // 0x140111840

// ---- citizencomponent_lifecycle.cpp
void CitizenComp_JoinRoster_68a0(void* selfV, void* param2V);  // 0x1401168a0
void CitizenComp_SortByProfession_02a0(void* arrV, int n, void* );  // 0x1400302a0
void CitizenComp_DeferredInit_117080(int64_t selfV, void* pos, uint32_t gender, char isElder, float age, char forceEmploy, int64_t* parentIBP, int64_t spawnKey);  // 0x140117080

// ---- cropfieldcomponent.cpp
struct CropSlot;
struct CropSlotArray;
namespace IO { class Serializer; }
void CropFieldComponent_ResizeEntityList(CropSlotArray* arr, int newSize);  // 0x1400335b0 Array<CropSlot>::SetSize
void CropFieldComponent_SerializeEntityList(IO::Serializer& serializer, int count, CropSlot* entry);  // 0x1400336c0

// ---- cropfieldcomponent_states.cpp
extern "C" float CropFieldComponent_Maturity(void* comp);  // 0x14011b410  B4 FORWARD: CropFieldComponent::Maturity (cropui_promote.cpp)

// ---- decalcomponent.cpp
void DecalComponent_TerrainDecalRemove(void* terrainDecalSys, int decalId);  // 0x1400b0d00

// ---- destroycomponent_state6.cpp
extern "C" void* Destroy_FindDecalComponent_b630(uintptr_t entity, void* retIBP);  // 0x14000b630

// ---- destroycomponent_workerrelease.cpp
extern "C" void WorkPlaceComponent_SetMaxWorkers(int64_t wpc, int newMax);  // 0x14012fb00
extern "C" void WorkPlaceComponent_EnterBuilderProfession(int64_t wpc);  // 0x14012f520

// ---- entity_finders_disaster.cpp
extern "C" void* Entity_FindComponent_Fire_5840(uintptr_t entity, void* retIBP);  // 0x140005840
extern "C" void* Entity_FindComponent_Citizen_9790(uintptr_t entity, void* retIBP);  // 0x140009790

// ---- floorcomponent.cpp
void* FloorComponent_FindResData(void* owner, void* outIBP);  // 0x140004680
void FloorComponent_SubF390(void* selfV);  // 0x14011f390  B4 FORWARD: FloorComponent::PlaceFloor (multiobjectcomponent.cpp)

// ---- frustumoutlineelement.cpp
extern "C" void FrustumOutline_ProjectCorners(float* out);  // 0x14006c5b0

// ---- growthcomponent.cpp
void GrowthComponent_ModifyFlags(void* comp, uint32_t orMask, uint32_t andNotMask);  // 0x140118950  B4 FORWARD: Component::ModifyFlags (animalcomponent.cpp)
void GrowthComponent_ResetGrowthState(void* entityPtr);  // 0x140126780  B4 FORWARD: NaturalResourceComponent::ResetGrowthState (tornadocomponent.cpp)

// ---- happinesscomponent.cpp

// ---- herdcomponent.cpp

// ---- interactcomponent.cpp
void InteractComponent_FindWorkplaceComponent(uintptr_t entity, void* retBuf);  // 0x140004740

// ---- keystone_a_map.cpp

// ---- keystone_a_restree.cpp
extern "C" void* ResidenceComponent_impl_7230(int64_t* param_1, void** param_2, uint64_t* param_3, int64_t* param_4);  // 0x140017230

// ---- mapcomponent.cpp
extern "C" void MapTerrainClear_Reimpl(void* gridPtr, int value, int x, int y, int w, int h);  // 0x1400a4d30
extern "C" void MapComponent_DetachOccupancy_4750(void* selfV);  // 0x140124750

// ---- modelcomponent.cpp
extern "C" void ModelComponentNotify_Reimpl(void* entity, int param);  // 0x1400c2b70

// ---- naturalresourcecomponent.cpp
void NaturalResourceComponent_FindComponentByType(void* entity, void* outIBP);  // 0x140005200

// ---- nomadcomponent.cpp
void NomadComp_FUN_140103480(int64_t param_1, void* param_2);  // 0x140103480

// ---- orchardcomponent.cpp
void OrchardComponent_ValidateRefs(void* selfV);  // 0x140051830 B4 FORWARD to OrchardComponent::ValidateRefs

// ---- pasturecomponent.cpp
void PastureComponent_AnimalSetBounds(void* selfP, short* outX, short* outY, short* outW, short* outH, int size);  // 0x140105e20 B4 FORWARD to AnimalComponent::GetWanderBox
uint64_t PastureComponent_ComputeCapacity(uintptr_t self);  // 0x140128570 B4 FORWARD to PastureComponent::ComputeCapacity

// ---- pasturecomponent_spawn.cpp
extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP);  // 0x140128670
extern "C" int Pasture_FillAllPastures();  // 0x140060290

// ---- pasturecomponent_states.cpp
extern "C" uint64_t MapGrid_StampCellRefs_9f770(void* grid, int x, int y, int w, int h, void* ref);  // 0x14009f770

// ---- rawmaterialcomponent.cpp
void RawMaterialComponent_RegisterRawMaterial(void* worldMgr, void* entityChain);  // 0x140091e60 B4 FORWARD: 4 outside caller(s) -> CitizenBehavior::RegisterRawMaterial

// ---- residencecomponent.cpp
void ResidenceComponent_ResizeResidentArray(void* arrayFieldPtr, int newCount);  // 0x140037ad0 B4 FORWARD: 1 outside caller (Game/starttown.cpp) -> ResidenceComponent::SetFamilyCount
void ResidenceComponent_ResidenceInit(void* selfP);  // 0x140129ef0 B4 FORWARD: 1 outside caller (Game/starttown.cpp) -> ResidenceComponent::EvictResidents
bool CitizenComp_helper_a4f0(int64_t self, int param2);  // 0x14012a4f0 B4 FORWARD: 1 outside caller (standalone/death_harness.cpp) -> ResidenceComponent::IsFamilyFull (moved from citizencomponent.cpp)

// ---- statusiconcomponent.cpp
void CitizenComp_FUN_14012c3f0(int64_t* param_1, uint8_t flagIndex, char value);  // 0x14012c3f0 B4 FORWARD: outside callers in 16 files -> StatusIconComponent::SetFlag (moved from citizencomponent.cpp)

// ---- storagecomponent.cpp
int StorageComponent_QueryAvailable(void* storageComp, int64_t materialEntity);  // 0x14012ce70 B4 FORWARD: 1 outside caller (Work/buildroad.cpp) -> StorageComponent::QueryAvailable
void StorageComponent_DynArrayResize(void* arrayBase, int newCount);  // 0x14000f090

// ---- toolbardescription.cpp
extern "C" void ToolbarDescription_StringArrayResize(void* hdr, unsigned newCount);  // 0x140028860

// ---- tradecomponent.cpp
// B.4 COMP-f: Setup is TradeComponent::Setup; the array helpers are in tradecomponent_funcs.h.

// ---- tradecomponent_livestock.cpp
extern "C" void TradeComponent_SpawnPurchasedLivestock(void* selfV, void* tmpl, unsigned int qty);  // 0x14005ace0 B4 FORWARD -> TradeComponent::SpawnPurchasedLivestock (Dialog/tradeui_checkout.cpp)

// ---- wellcomponent.cpp
// B.4 COMP-f: 0x140091ba0 is CitizenBehavior::UnregisterWell.

// ---- workcomponent.cpp
bool WorkComponent_CheckWorkSlot(void* selfP);  // 0x14012ead0 B4 FORWARD -> WorkComponent::IsWorkPosted (outside callers)
void WorkComponent_RegisterWorkSlot(void* selfP);  // 0x14012eb50 B4 FORWARD -> WorkComponent::CancelWork (outside callers)

// ---- workplacecomponent.cpp
void WorkPlaceComponent_SetAccepting(int64_t comp, bool accepting);  // 0x14012fc00 B4 FORWARD -> WorkPlaceComponent::SetAccepting (outside callers)
void WorkPlaceComponent_OnCreateInit(void* selfP);  // 0x14012ee60 B4 FORWARD -> WorkPlaceComponent::ReleaseWorkers (destroycomponent_workerrelease.cpp)
extern "C" void WorkPlaceComponent_LeaveBuilderProfession(void* selfP);  // 0x14012f670 B4 FORWARD -> WorkPlaceComponent::LeaveBuilderProfession (destroycomponent_states.cpp)

// ---- zonecomponent.cpp
// B.4 COMP-f: 0x140088e20 is CitizenBehavior::AddZoneEntity.

// ============================================================
// Moved from engine/funcs/engine_funcs.h's "TO MOVE: Component" section (B3R G-CompD).
// (engine_funcs.h is gone since B3R W2-EZ: its includers include this header directly.)
// ============================================================

// ---- storagecomponent.cpp
// StorageComponent::QueryCount(storageComp) -> int
// FUN_14012cf10: iterates +0x28 item array (count at +0x30). Each slot is 0x10 bytes:
//   [0..7] = ptr to "item" struct (has +0x28=component ptr array, +0x30=comp count)
//   [8..11] = item quantity count
// For each item: scan its component array for ResourceType (type ID check at vtable+0x10,
// then IsDerivedFrom). Accumulate ResourceType.quantity(+0x48) * item.count.
int fn_StorageComponent_QueryCount(void* storageComp);  // 0x14012cf10 B4 FORWARD: outside callers in ~25 files -> StorageComponent::QueryCount

// ---- workcomponent.cpp
// FUN_14012e5c0: Calculate the STARTING job count of a WorkComponent (DAT_1401c8070) —
// i.e. the initial value of WorkComponent+0x30, the number of worker SLOTS the site opens
// with. It is NOT the maximum the player can reach: the jobCount spinner raises +0x30 up
// to WorkDescription+0x2c (_maxWorkerCount) via SetMaxWorkers (0x14012fb00).
// param_1+0x18 = the WorkDescription (checks IsDerivedFrom at DAT_1401c80a8).
// WorkDescription layout: +0x20 _allowCountChange, +0x24 _defaultWorkers,
//   +0x28 _minWorkerCount, +0x2c _maxWorkerCount, +0x30 _maxArea, +0x34 _scaleWorkersWithSize.
// If _scaleWorkersWithSize (+0x34) == 0: return _defaultWorkers (+0x24) flat.
// Else: trunc(w*h / _maxArea * _defaultWorkers + 1) clamped to [1, _defaultWorkers].
// Used as fn_StorageCountQuery in workplaceui, fn_CalcWorkerSlots in professionui.
int fn_StorageCountQuery(void* component);  // 0x14012e5c0 B4 FORWARD -> WorkComponent::CalcMaxWorkers (NOT the registered body: that is
                                            // WorkComponent_CalcMaxWorkers, tutorial_base.cpp; R5, two bodies)

// ---- componentdescription.cpp
// ResolveExternalByName (FUN_14001b1d0 in exe) — REPLACED with inline C++
// Resolves and caches an External object by filename.
// Input: nameString points to an ExternalRef struct:
//   +0x00: String (8 bytes) — resource name
//   +0x08: int32  — cached generation counter (compared against ExternalFactory+0x08)
//   +0x10: void*  — cached External* result
// If the generation counter matches the factory's, the cached result is still valid.
// Otherwise, looks up the External by converting the String to a Filename and calling
// ExternalFactory::Find. If found and passes IsDerivedFrom(ComponentDescription::TypeId()),
// caches the result; otherwise caches nullptr. Body: code/Game/Component/componentdescription.cpp.
void ResolveExternalByName_Impl(void* nameString);  // 0x14001b1d0

// ---- keystone_a_map.cpp (B3R G-Game: read by code/Game/Game/gamegrid_serialize.cpp)
int CreateNaturalResource_helper_e2c0(int* param_1, int param_2);  // 0x14009e2c0

// ---- inline bodies (moved from engine/funcs/engine_funcs.h, its "TO MOVE: Component"
// section; no .cpp. B3R Leftovers. Storage_AddItem, the third body there, had no caller
// and was deleted.)
#include "Entity/exe_bodies.h"   // fn_FindComponent_Work (read by fn_CitizenComponent_HasVacancy)

// ---- inline body (no .cpp)
// Alias used by professionui.cpp
inline int fn_CalcWorkerSlots(void* storageLocComp) {
    return fn_StorageCountQuery(storageLocComp);
}

// ---- inline body (no .cpp)
// CitizenComponent_HasVacancy (0x140017050) — WorkPlaceComponent roster (+0x60) < seat
// capacity (its StorageLocationComponent/WorkComponent +0x30, lazily CalcMaxWorkers'd).
// Reimplemented INLINE: it was ResolveAddr(Module::Exe,0x140017050), which is NULL in
// standalone (that exe address isn't registered), and citizencomponent.cpp:1176/1193 call
// it UNGUARDED — so once a placed building reaches construction and a citizen evaluates
// switching workplaces, the null call crashed (VCALL-of-0). Byte-faithful to 0x140017050
// (FUN_140003050 = fn_FindComponent_Work, FUN_14012e5c0 = fn_StorageCountQuery), plus
// one SA safety guard the original lacks: a null storage-loc component → "no vacancy"
// rather than dereferencing null+0x30. Works in both DLL and standalone builds.
inline bool fn_CitizenComponent_HasVacancy(uintptr_t workplaceComp) {
    int roster = *reinterpret_cast<int*>(workplaceComp + 0x60);
    uint64_t slComp = 0;
    fn_FindComponent_Work(*reinterpret_cast<void**>(workplaceComp + 0x10), &slComp);
    if (slComp == 0) return false;
    uintptr_t sl = static_cast<uintptr_t>(slComp);
    if (*reinterpret_cast<int*>(sl + 0x30) == 0) {
        int cap = fn_StorageCountQuery(reinterpret_cast<void*>(sl));
        *reinterpret_cast<int*>(sl + 0x30) = cap;
    }
    return roster < *reinterpret_cast<int*>(sl + 0x30);
}
