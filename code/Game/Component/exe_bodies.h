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
#include "Entity/CitizenBehavior.h"   // CitizenIbpList<T> (BuildComponent_FindInQueue)

class CitizenComponent;

// ---- ambient_externalptr_serialize.cpp
namespace Audio { class SoundEffect; }
namespace IO { class Serializer; }

// ---- ambientemittercomponent.cpp
struct AmbientEmitterSlot;   // Component/AmbientEmitterComponent.h (Collection.h: via Entity/CitizenBehavior.h)
template <> void Collection::Array<AmbientEmitterSlot>::SetSize(int newCount);  // 0x140001770 (the member, specialized: ambientemittercomponent.cpp)

// ---- ambientenvironmentdescription.cpp
struct AmbientEnvironmentBiome;   // Component/AmbientEnvironmentDescription.h
template <> void Collection::Array<AmbientEnvironmentBiome>::SetSize(int newCount);  // 0x140002540 (the member, specialized: ambientenvironmentdescription.cpp)

// ---- attendancecomponent.cpp
class WorkPlaceComponent; namespace Object { template <class T> class InternalPtr; }
Object::InternalPtr<WorkPlaceComponent>* AttendanceComponent_GetJobHostComponent(ComponentEntity* entity, Object::InternalPtr<WorkPlaceComponent>* outPtr);  // 0x140002e10 (returns outPtr)

// ---- buildcomponent.cpp
int BuildComponent_FindInQueue(const CitizenIbpList<BuildComponent>* list, const Object::InternalPtr<BuildComponent>* key);  // 0x140017550

// ---- cemeterycomponent.cpp
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);  // 0x140002cc0

// ---- charactercomponent.cpp
class Action;
namespace System { class Time; }
struct ActionStack;
void Character_UpdateAnimation_Reimpl(ActionStack* arrayBase, Action** ptrRef, const System::Time* deltaTime);  // 0x1400c4f80

// ---- citizen_assign_citizens.cpp
extern "C" void CitizenAssignCitizens(void* ctxV);  // 0x1400d43b0

// ---- citizen_assignworkplace.cpp

// ---- citizen_employ_cascade.cpp
class CitizenBehavior;   // Entity/CitizenBehavior.h
class Profession;        // Game/Profession.h
class ComponentEntity;   // Entity/ComponentEntity.h
extern "C" CitizenComponent** CitizenEmploy_Strat_cbf0(CitizenBehavior* townMgr, CitizenComponent** out, ComponentEntity** availPtr, Profession* profKey);  // 0x14008cbf0
extern "C" CitizenComponent** CitizenEmploy_Strat_ca20(CitizenBehavior* townMgr, CitizenComponent** out, ComponentEntity** availPtr, Profession* profKey);  // 0x14008ca20
extern "C" CitizenComponent** CitizenEmploy_Strat_c5f0(CitizenBehavior* townMgr, CitizenComponent** out, ComponentEntity** availPtr, Profession* profKey);  // 0x14008c5f0
extern "C" CitizenComponent** CitizenEmploy_Strat_c4b0(CitizenBehavior* townMgr, CitizenComponent** out, ComponentEntity** availPtr, Profession* profKey);  // 0x14008c4b0
extern "C" CitizenComponent** CitizenList_AssignWorker_c3d0(CitizenBehavior* townMgr, CitizenComponent** out, ComponentEntity** availPtr, Profession* profKey);  // 0x14008c3d0
extern "C" void CitizenList_AddWorker_91130(CitizenBehavior* townMgr, Profession* profession);  // 0x140091130
extern "C" void TownRegistry_RegisterResidence(ComponentEntity* houseEntityIBP);  // the append inside ResidenceComponent's state-2 enter 0x140129de0 (townMgr+0x20)
extern "C" int CitizenEmploy_SeatWorkers(CitizenBehavior* town, Profession* profKey, int count);  // no retail address: the headless driver of dd10's (0x14007dd10) inner loop

// ---- citizen_lifeevent.cpp

// ---- citizen_lifeevent_handler.cpp

// ---- citizen_needsupdate.cpp

// ---- citizen_reassign_workers.cpp
extern "C" void PathGraph_ReassignWorkers(void* ctxV);  // 0x1400d62a0

// ---- citizen_updatehealth.cpp
class CharacterGrid; class CharacterComponent; template <class T> struct RawArray;
class CitizenComponent; class Disease;

// ---- citizenai_actionctors.cpp
class ComponentEntity;
class ComponentDescription;
void CitizenAI_PerformWorkCtor4_c1070(CitizenComponent* const* citizenRef, ComponentEntity* const* targetRef, float* param_3, int param_4, uint8_t param_5, uint32_t param_6);  // 0x1400c1070
void CitizenAI_PerformWorkCtor5_bd070(CitizenComponent* const* citizenRef, ComponentEntity* const* srcRef, float* param_3, ComponentDescription* material);  // 0x1400bd070
void CitizenAI_PerformWorkCtor6_bd420(CitizenComponent* const* citizenRef, ComponentEntity* const* targetRef, float* param_3, char param_4);  // 0x1400bd420
void CitizenAI_PerformWorkCtor8_bd790(CitizenComponent* const* citizenRef, ComponentEntity* const* targetRef, float* param_3);  // 0x1400bd790
void CitizenAI_PerformWorkCtor10_bccb0(CitizenComponent* const* citizenRef, ComponentEntity* const* srcRef, float* param_3, ComponentDescription* material);  // 0x1400bccb0
void CitizenAI_PerformWorkCtor11_bc790(CitizenComponent* const* citizenRef, ComponentDescription* material, ComponentEntity* const* srcRef, float* param_4, ComponentEntity* const* dstRef, float* param_6);  // 0x1400bc790
void CitizenAI_Pathfinding_NavigateTo_bda50(CitizenComponent* const* citizenRef, ComponentEntity* const* targetRef, float* param_3, uint8_t );  // 0x1400bda50
void CitizenAI_Pathfinding_WalkTowards_bb490(CitizenComponent* const* citizenRef, uint32_t param_2, float* param_3, uint32_t param_4, float param_5, uint32_t param_6);  // 0x1400bb490
void CitizenAI_PlayAnimationCtor4_ba6b0(CitizenComponent* const* citizenRef, ComponentEntity* const* targetRef, float* param_3, float param_4, uint32_t param_5);  // 0x1400ba6b0

// ---- citizenai_executor.cpp
struct ActionStack;   // Action/Action.h
extern "C" void CitizenAI_ActionExecutor_c4c50(ActionStack* param_1, const System::Time* param_2);  // 0x1400c4c50

// ---- citizenai_findhelpers.cpp
class ResidenceComponent;
class CitizenBehavior;
ComponentEntity** Entity_impl_ec70(CitizenBehavior* list, ComponentEntity** found, uint32_t param_3, float* param_4);  // 0x14008ec70
ComponentEntity** CitizenAI_helper_e370(CitizenBehavior* list, ComponentEntity** found, uint32_t param_3, ComponentDescription** param_4, float* param_5, uint64_t , ComponentEntity** origP7);  // 0x14008e370

// ---- citizenai_findtarget.cpp
class ComponentEntity;
class MarketLocationComponent;
namespace Object { template <class T> class InternalPtr; }
::Object::InternalPtr<MarketLocationComponent>* CitizenAI_sub_78c0(
    ComponentEntity* param_1, ::Object::InternalPtr<MarketLocationComponent>* param_2);  // 0x1400178c0
CitizenBehavior* CitizenAI_FindBestTarget_dd30(CitizenBehavior* list, ComponentEntity** found, uint32_t param_3, uint32_t param_4, ComponentDescription** param_5, float* param_6, uint32_t param_7);  // 0x14008dd30

// ---- citizenai_gather.cpp
class Component;
extern "C" int Storage_GetResourceCount2(const Component* self);  // 0x14012ab00  // B8: stays free -- passed as a function pointer at citizenai_gather.cpp:521 (HideRenderWorkplaceMesh); retail calls 0x14012ab00 directly from HasSufficientResources2 (0x140115892), no pointer slot

// ---- citizenai_growup.cpp

// ---- citizenai_housing_idle.cpp
// The AVL rebalance walk over any 0x30 node (links +0x10 / +0x18 / +0x20, int16 balance /
// height +0x28 / +0x2A; one retail body for every such tree, B13); instantiated per node type
// in citizenai_housing_idle.cpp.
template <class Node> void CitizenAI_Reg_helper_e360(Node** rootSlot, Node** walk);  // 0x14000e360

// ---- citizenai_work_scheduler.cpp
class WorkScheduler; class WorkComponent; class Work; class Profession;
extern "C" int CitizenAI_WorkScheduler_Insert(WorkScheduler* sched, Object::InternalPtr<Work>* pWorkIBP, uint32_t code, char fromExisting, int prio);  // 0x1400f88a0
extern "C" void CitizenAI_WorkScheduler_Rekey(WorkScheduler* sched, uint32_t workId, Profession* oldKey, Profession* newKey);  // 0x1400f8400

// ---- citizenaicomponent.cpp
class StorageComponent;
int CitizenComp_Storage_GetResourceCount(const Component* self);  // 0x14012a9d0  // B8: stays free -- passed as a function pointer at citizenai_gather.cpp:423 (HideRenderWorkplaceMesh); retail calls 0x14012a9d0 directly from HasSufficientResources (0x1401157c2), no pointer slot

// ---- citizencomponent.cpp
ComponentDescription** CitizenComp_Entity_impl_ef40(const void* param_1, ComponentDescription** param_2, ComponentEntity* const* param_3, int param_4);  // 0x14008ef40 (param_1 unused)

// ---- citizencomponent_lifecycle.cpp

void CitizenComp_SortByProfession_02a0(ComponentEntity** array, int n, void* );  // 0x1400302a0
namespace Math { class Vector3; }

// ---- cropfieldcomponent.cpp
struct CropSlot;
namespace IO { class Serializer; }
void CropFieldComponent_SerializeEntityList(IO::Serializer& serializer, int count, CropSlot* entry);  // 0x1400336c0  // B8: stays free -- not an Array<T> member (B8 RT2 reading): Collection::Array has no Serialize by design (Collection.h, hazard 6) and IO::Serializer declares only the scalar-array overloads, so there is no instantiation to move it onto; retail 0x1400336c0 is `TEST EDX,EDX; JLE` then per 0x20 slot int, int, InternalBasePtr + IsDerivedFrom(ComponentEntity) when non-null, InternalBasePtr + IsDerivedFrom(NaturalResourceComponent) when non-null, bool: the body as written

// ---- cropfieldcomponent_states.cpp
class CropFieldComponent;

// ---- decalcomponent.cpp
class WorldGeometry;
void DecalComponent_TerrainDecalRemove(WorldGeometry* terrainDecalSys, int decalId);  // 0x1400b0d00

// ---- destroycomponent_workerrelease.cpp
// B4 X3-COMP: WorkPlaceComponent_SetMaxWorkers (0x14012fb00, its one caller calls the
// member now) and WorkPlaceComponent_EnterBuilderProfession (0x14012f520, no caller):
// deleted.

// ---- entity_finders_disaster.cpp
// B4 X-G1-2b: deleted with its file. Entity_FindComponent_Fire_5840 (0x140005840),
// _Citizen_9790 (0x140009790) and _AmbientEmitter_2280 (0x140002280) had no caller
// left (the tornado reads ComponentEntity::FindComponent<T>()); the AmbientEmitter
// finder's warning (its OnAdded 0x140042e10 is not ported, so +0x38 stays null and
// 0x14012a710's unguarded read faults) is in git history at that file.

// ---- floorcomponent.cpp
class MapComponent;
MapComponent** FloorComponent_FindResData(ComponentEntity* owner, MapComponent** outIBP);  // 0x140004680 (returns outIBP)

// ---- frustumoutlineelement.cpp
extern "C" void FrustumOutline_ProjectCorners(float* out);  // 0x14006c5b0

// ---- growthcomponent.cpp

// ---- happinesscomponent.cpp

// ---- herdcomponent.cpp

// ---- interactcomponent.cpp

// ---- keystone_a_map.cpp

// ---- keystone_a_restree.cpp
// ResidenceComponent_impl_7230 (0x140017230): Entity/CitizenBehavior.h (InsertCitizenBucket<Key>)

// ---- mapcomponent.cpp
class Minimap;   // Game/Minimap.h
extern "C" void MapTerrainClear_Reimpl(Minimap* gridPtr, int value, int x, int y, int w, int h);  // 0x1400a4d30
class MapComponent;

// ---- modelcomponent.cpp

// ---- naturalresourcecomponent.cpp

// ---- nomadcomponent.cpp

// ---- orchardcomponent.cpp
// B.4 COMP-6: OrchardComponent_ValidateRefs had no caller left: deleted.

// ---- pasturecomponent.cpp

// ---- pasturecomponent_spawn.cpp
class PastureComponent;
class AnimalComponent;
extern "C" int Pasture_FillAllPastures();  // 0x140060290
class MapGenerator;   // Game/MapGenerator.h
extern "C" int StartTown_SpawnStartingHerd(MapGenerator* sc, const float* centre, ComponentDescription* tmpl);  // 0x1400a0b10

// ---- pasturecomponent_states.cpp
class Map;   // Game/Map.h
extern "C" uint64_t MapGrid_StampCellRefs_9f770(Map* grid, int x, int y, int w, int h, ComponentDescription* ref);  // 0x14009f770 (callers: orchard / pasture states only)

// ---- rawmaterialcomponent.cpp

// ---- residencecomponent.cpp
// B.4 COMP-6: the three forwarders (SetFamilyCount / EvictResidents / IsFamilyFull) had no caller left: deleted.

// ---- statusiconcomponent.cpp

// ---- storagecomponent.cpp

// ---- toolbardescription.cpp
#include "collection/Collection.h"   // Collection::Array (the resize's header)
namespace System { class String; }

// ---- tradecomponent.cpp
// B.4 COMP-f: Setup is TradeComponent::Setup; the array helpers are in tradecomponent_funcs.h.

// ---- tradecomponent_livestock.cpp

// ---- wellcomponent.cpp
// B.4 COMP-f: 0x140091ba0 is CitizenBehavior::UnregisterWell.

// ---- workcomponent.cpp
// B4 X3-COMP: WorkComponent_CheckWorkSlot (0x14012ead0), _RegisterWorkSlot (0x14012eb50) and
// fn_StorageCountQuery / fn_CalcWorkerSlots (0x14012e5c0): their callers call IsWorkPosted /
// CancelWork / CalcMaxWorkers; deleted.

// ---- workplacecomponent.cpp
class WorkPlaceComponent;

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
// (WorkComponent::CalcMaxWorkers; NOT the registered body, WorkComponent_CalcMaxWorkers in
// tutorial_base.cpp: R5, two bodies.)

// ---- componentdescription.cpp
// ResolveExternalByName (FUN_14001b1d0 in exe) — REPLACED with inline C++
// Resolves and caches an External object by filename.
// Input: the by-name record (Object/NamedExternalRef.h): name, generation, resolved.
// If the generation counter matches the factory's, the cached result is still valid.
// Otherwise, looks up the External by converting the String to a Filename and calling
// ExternalFactory::Find. If found and passes IsDerivedFrom(ComponentDescription::TypeId()),
// caches the result; otherwise caches nullptr. Body: code/Game/Component/componentdescription.cpp.
struct NamedExternalRef;
void ResolveExternalByName_Impl(NamedExternalRef* ref);  // 0x14001b1d0

// ---- keystone_a_map.cpp (B3R G-Game: read by code/Game/Game/gamegrid_serialize.cpp)
int CreateNaturalResource_helper_e2c0(Map* map, int cellIndex);  // 0x14009e2c0

// ---- inline bodies (moved from engine/funcs/engine_funcs.h, its "TO MOVE: Component"
// section; no .cpp. B3R Leftovers. Storage_AddItem, the third body there, had no caller
// and was deleted.)
#include "Entity/exe_bodies.h"   // fn_FindComponent_Work (read by citizencomponent.cpp's fn_CitizenComponent_HasVacancy)

