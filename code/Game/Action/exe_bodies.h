// code/Game/Action/exe_bodies.h
//
// Declarations of the C bodies in code/Game/Action/ that were reached through the address
// registry (RegisterExeAddr rows, the readers' ResolveAddr(Module::Exe, ...) / helper /
// X_ResolveFuncs / engine_funcs doors), or through a local extern prototype in the caller.
// B3R_RULES R5: every DIRECT body is declared in exactly one header, written from its
// DEFINITION (linkage included), and each defining .cpp includes this header so the
// compiler checks the pair. Readers call these by name through this header (R2).
// Making them members of real classes is B.4's work; this header is where it finds them.
//
// Grouped by defining file; the retail address is the trailing comment on each line.
// `uint` in a definition (a file-local typedef there) is spelled `unsigned int` here.
// The address -> name table is docs/runtime_port/endgame_b/b3r_session/data/P0a_declared.tsv.
#pragma once

#include <cstddef>
#include <cstdint>

// ---- action_base_cnr.cpp
class Map;                    // Game/Map.h
class GrowthComponent;        // Component/GrowthComponent.h
class ModelComponent;         // Component/ModelComponent.h
class NaturalResourceComponent;   // Component/NaturalResourceComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
uint64_t CheckCellTerrain(const Map* map, int cellIndex);  // 0x14009cb00
void SetGrowthProgress(GrowthComponent* self, float progress);  // 0x140120db0
uint32_t ModelMeshGroupCount(const ModelComponent* model);  // 0x1401252f0
uint32_t ModelMeshCount(const ModelComponent* model, int group);  // 0x140125320
void ModelSelectMesh(ModelComponent* model, uint16_t variant, int16_t subVariant);  // 0x1401253d0
void NaturalResourceSetAnimation(NaturalResourceComponent* self, uint32_t animIdx);  // 0x140126440
uint64_t NaturalResourcePlaceAtCell(NaturalResourceComponent* self, int cellIndex);  // 0x140126540
void RemoveNaturalResourceCore(NaturalResourceComponent* self, ComponentEntity* const* citizenIBP, char harvestFlag);  // 0x1401260f0
uint64_t ActCNR_CheckTerrain_cb00(void* mapGrid, int cellIndex);  // 0x14009cb00
uint32_t ActCNR_NR_GetSubVariantCount_125320(int64_t comp, int variant);  // 0x140125320
void ActCNR_NR_SetVariant_1253d0(int64_t* comp, uint16_t variant, int16_t subVariant);  // 0x1401253d0

// ---- action_base_complete.cpp
class CitizenComponent;       // Component/CitizenComponent.h
class WorkScheduler;          // Game/WorkScheduler.h
bool TryCompleteTask(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx, ComponentEntity* const* targetRef, uint32_t taskParam);  // 0x1400f8760
void TaskReassign(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx, ComponentEntity* const* targetRef, uint32_t taskParam);  // 0x1400f8800
void TaskCleanup(WorkScheduler* sched, int taskIdx);  // 0x1400f8e30
void HideCitizenTool(CitizenComponent* citizen);  // 0x1401176e0
void ShowCitizenTool(CitizenComponent* citizen, int toolId);  // 0x1401173b0
void ActCt_TaskCleanup_8e30(void* selfV, int taskIdx);  // 0x1400f8e30
void ActCt_HideTool_1176e0(void* entityV);  // 0x1401176e0

// ---- action_base_hunt.cpp
class HuntComponent;          // Component/HuntComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
template <class T> struct RawArray;   // Action/PathSearch.h
bool HuntIsReady(const HuntComponent* hunt);  // 0x140122a80
void QwordArraySetSize(RawArray<void*>* arr, int newCount, void* const* fill);  // 0x140003110
void MarkEntityForRemoval(ComponentEntity* entity);  // 0x1400c2ea0
void NotifyEntityChanged(ComponentEntity* entity);  // 0x1400c3470 (a second body of ComponentEntity::RefreshEntityFlags)
void ActHunt_DynArraySetCount_3110(void* arrV, int newCount, void* valuePtr);  // 0x140003110 B4 FORWARD -> QwordArraySetSize
void ActHunt_MarkEntityRemoval_c2ea0(void* self);  // 0x1400c2ea0 B4 FORWARD -> MarkEntityForRemoval
void ActHunt_NotifyEntityChanged_c3470(void* self);  // 0x1400c3470 B4 FORWARD -> NotifyEntityChanged

// ---- action_base_move.cpp
class CharacterComponent;     // Component/CharacterComponent.h
class CitizenComponent;       // Component/CitizenComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
namespace Math { class Vector3; }
CharacterComponent* FindCharacterComponent(const ComponentEntity* entity);  // 0x1400012d0
void SetFacingDirection(CharacterComponent* character, const Math::Vector3& dir);  // 0x14010a060
void SetAnimation(CharacterComponent* character, uint32_t animId, float p3, float p4);  // 0x14010a1f0
uint8_t CitizenBreakCheck(CitizenComponent* citizen);  // 0x140116650
class CharacterDescription;   // Component/CharacterDescription.h
class MoveTo;                 // Action/MoveTo.h
struct ActionStack;           // Action/Action.h
namespace IO { class Serializer; }
void ActMv_ExternalPtrSerialize_22970(CharacterDescription** slot, IO::Serializer* ser);  // 0x140022970
void ActMv_UpdateFacing_c8d20(MoveTo* self);  // 0x1400c8d20
void ActMv_SetAnimation_a1f0(void* selfp, uint32_t animId, float p3, float p4);  // 0x14010a1f0 B4 FORWARD: code/Game/Component/citizencomponent_lifecycle.cpp still calls this -> SetAnimation
bool ActMv_PathSetup_c8c10(MoveTo* self, const float* worldPos, void*, ActionStack* param4);  // 0x1400c8c10 (retail returns AL = 1)

// ---- action_base_movedeps.cpp
class ModelComponent;         // Component/ModelComponent.h
namespace System { class Time; class Random; }
bool BreakTimeCheck(System::Time* time, const float* pair, System::Random* rng);  // 0x1400a9290
void PlayModelAnimation(ModelComponent* model, uint32_t animId, float p3, float p4, float p5);  // 0x1401250f0

// ---- action_base_rawmat.cpp
class AutoPickupComponent;    // Component/AutoPickupComponent.h
class CitizenBehavior;        // Entity/CitizenBehavior.h
class Component;              // Component/Component.h
class ComponentDescription;   // Component/ComponentDescription.h
class ComponentEntity;        // Entity/ComponentEntity.h
class StorageComponent;       // Component/StorageComponent.h
class TrackerComponent;       // Component/TrackerComponent.h
void StorageSubtract(StorageComponent* storage, const ComponentDescription* material, int count);  // 0x14012d150
void StorageAdd(StorageComponent* storage, ComponentDescription* material, int count);  // 0x14012d000
void AutoPickupAppend(AutoPickupComponent* self, uint32_t value);  // 0x140044810
int ResourceCategory(const ComponentDescription* material);  // 0x140088f90
void TrackerUpsert(TrackerComponent* tracker, ComponentDescription* material, int value);  // 0x140058fd0
void RegisterRawMaterial(CitizenBehavior* town, ComponentEntity* const* source, char placeNear, ComponentDescription* material, int quantity);  // 0x140088ef0
void PositionRawMaterial(Component* self, int cellIndex, float rotation);  // 0x140129760
void ActRm_StorageSubtract_d150(uintptr_t storage, int64_t resType, int count);  // 0x14012d150
void ActRm_StorageAdd_d000(uintptr_t storage, int64_t resType, int count);  // 0x14012d000
void ActRm_ArrayResize16_da70(int64_t* arr, int newCount);  // 0x14000da70
int ActRm_GetResourceCategory_88f90(void*, uintptr_t entity);  // 0x140088f90
void ActRm_VectorClone_d9d0(int64_t* dst, int64_t* src);  // 0x14000d9d0

// ---- action_child_ctors.cpp
void* ActChild_ShowToolCtor_42890(void* self, void* param_2, int toolType);  // 0x140042890
class ComponentEntity;        // Entity/ComponentEntity.h
float HeadingToward(const float* worldPos, ComponentEntity* const* entityIBP);  // 0x1400c29a0
float ActChild_Entity29a0_c29a0(float* worldPos, void* param_2);  // 0x1400c29a0

// ---- action_child_ctors2.cpp
class ConsumeProduceComponent;   // Component/ConsumeProduceComponent.h
uint64_t PickConsumeRecipe(const ConsumeProduceComponent* self);  // 0x140119470

// ---- action_pf_astar.cpp

// ---- action_pf_avl.cpp
extern "C" void Pf_AvlRebalanceA_a8c0(int64_t* root, int64_t* node);  // 0x14001a8c0
struct PathGraph;
extern "C" void PathPairs_Clear_25b30(PathGraph* pg);  // 0x140025b30

// ---- action_pf_ctors.cpp
void Pf_ActStackPush_54e0(int64_t* param_1, void* param_2_);  // 0x1400c54e0
void* Pf_PerformWorkCtor7_9720(void* self, void* entity, float animIndex, uint32_t actionType, int64_t time, uint8_t param_6, int param_7);  // 0x1400c9720
class Component;              // Component/Component.h
class ComponentDescription;   // Component/ComponentDescription.h
class ComponentEntity;        // Entity/ComponentEntity.h
class InteractComponent;      // Component/InteractComponent.h
class StorageComponent;       // Component/StorageComponent.h
template <class T> struct RawArray;   // Action/PathSearch.h
void Pf_SetFlagRefreshUpdate_107260(Component* self);  // 0x140107260
int InteractNodeCount(const InteractComponent* self, int kind);  // 0x140123070
int Pf_RecursiveChildSum_123070(int64_t param_1, int param_2);  // 0x140123070 B4 FORWARD -> InteractNodeCount
bool StorageAcceptsMaterial(const StorageComponent* storage, const ComponentDescription* material);  // 0x14012c560
void Pf_StorageDropOff_116760(Component* self, ComponentDescription* material, int count);  // 0x140116760
int Pf_StorageCountDelta_1167c0(Component* self, const ComponentDescription* material);  // 0x1401167c0

// ---- action_pf_geom.cpp
int Pf_RayTriIntersect_aa810(void*, const float* P, const float* D, const float* maxT, const float* V0, const float* V1, const float* V2, float* outT, float* outNormal);  // 0x1400aa810
int Pf_TileShapeLut_ae180(void*, char p2, char p3, char p4, char p5, char p6, char p7, char p8, char p9);  // 0x1400ae180
class PopulationComponent;    // Component/PopulationComponent.h
int Pf_PastureTargetPick_53ca0(const PopulationComponent* population, int herdCell);  // 0x140053ca0

// ---- action_pf_pools.cpp
struct GeometryBatchVertex;   // Game/WorldGeometryElements.h
void Pf_VectorResize14_ee40(RawArray<GeometryBatchVertex>* arr, int newCount);  // 0x14001ee40
void Pf_VectorResize14Fill_f8d0(RawArray<GeometryBatchVertex>* arr, int newCount, const GeometryBatchVertex* fill);  // 0x14001f8d0
struct GeometryBatch;   // Game/WorldGeometryElements.h
void Pf_VectorResize60_fa80(RawArray<GeometryBatch>* arr, int newSize, const GeometryBatch* fill);  // 0x14001fa80

// ---- action_pf_region.cpp

// ---- action_pf_road.cpp
void Pf_RoadQuad_cc10(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend);  // 0x1400acc10 B4 FORWARD -> BuildRoadQuad
class WorldGeometry;
int Pf_CellQuadRay_aa20(WorldGeometry* geo, int x, int y, const float* dir, const float* origin, const float* maxT, float* outHit, float* outT, float* outNormal);  // 0x1400aaa20 (body: RayTestCell, action_pf_road.cpp)
float Pf_CellQuadRayWrap_0f90(WorldGeometry* geo, const float* point, float* outNormal);  // 0x1400b0f90 (body: TerrainHeightUnder, action_pf_road.cpp)
float* InteractNodePosition(const InteractComponent* self, float* out, int kind, int index, float* heading);  // 0x140122e20
int PickReachableCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14004edb0
int EntityAnchorCell(ComponentEntity* const* entityIBP, float* outPos);  // 0x140087fb0
void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5);  // 0x140122e20 B4 FORWARD -> InteractNodePosition
int Pf_EntityAnchorCell_7fb0(void* param_1IBP, float* param_2);  // 0x140087fb0 B4 FORWARD -> EntityAnchorCell

// ---- action_pf_roots_drop.cpp
class AnimalDescription;      // Component/AnimalDescription.h
class CitizenComponent;       // Component/CitizenComponent.h
class DropOffRawMaterial;     // Action/DropOffRawMaterial.h
struct ActionStack;           // Action/Action.h
void DropRawMaterialOnGround(CitizenComponent* citizen, ComponentDescription* material, int count);  // 0x1400c78c0
void SpillAllStorage(DropOffRawMaterial* self);  // 0x1400c7570
uint64_t DropOffExec(DropOffRawMaterial* self, ActionStack* stack, StorageComponent* const* target, ComponentDescription* material);  // 0x1400c7620
void RemoveAnimalActions(CitizenComponent* const* citizenIBP, uint32_t task, float* destination, uint32_t createFlag, Component* const* animal, const AnimalDescription* description, uint32_t gate);  // 0x1400bbea0

// ---- action_pf_roots_misc.cpp
class CharacterComponent;     // Component/CharacterComponent.h
class CharacterGrid;          // Component/CharacterGrid.h
class HerdComponent;          // Component/HerdComponent.h
namespace Math { class Scalar; }
void QueryNearestCharacter(const CharacterGrid* grid, CharacterComponent** best, const float* point, const Math::Scalar* radius, int typeId, CharacterComponent* const* exclude);  // 0x140092260
void HerdStartMigration(HerdComponent* herd);  // 0x140122340

// ---- action_pf_roots_road.cpp
void Pf_ZoneRefresh_9c480(int* param_1);  // 0x14009c480

// ---- action_pf_roots_route.cpp
bool StorageCanAdd(StorageComponent* storage, const ComponentDescription* material, int count);  // 0x14012cd80
int FindBestAdjacentCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14012d5d0
ComponentEntity** PickBestDropTarget(const RawArray<ComponentEntity*>* lists, ComponentEntity** best, uint32_t goalCell, const ComponentDescription* material, int count, char useCapacity, float* outPos, ComponentEntity* const* exclude);  // 0x14008d590
// B4 FORWARD C spellings (defined in action_pf_ctors.cpp)
int Pf_IsDstReachable_f810(int* param_1, uint32_t param_2, uint32_t param_3, char param_4);  // 0x14009f810 -> Map::PathDistance
uint32_t Pf_StorageCanAccept_cd80(int64_t param_1, int64_t param_2, int param_3);  // 0x14012cd80 -> StorageCanAdd

// ---- keystone_a_zone.cpp
class PathfinderThread;       // Game/PathfinderThread.h
namespace Object { template <class T> class InternalPtr; }
namespace System { class String; }
int RoadTool_helper_6e30(PathfinderThread* pf, int start, int goal, int passableTypes, int32_t* path, int maxNodes);  // 0x1400a6e30
void ZoneComponent_sub_d360(Map* map, const Object::InternalPtr<ComponentEntity>* owner, int x, int y, unsigned int w, int h, const System::String* footprint, int rotation);  // 0x14009d360
int RoadTool_sub_ce00(Map* map, unsigned int passableTypes, unsigned int start, uint64_t goal, void* unused5, int32_t* path, uint8_t manhattan, uint8_t respectBlocked);  // 0x14009ce00

// ---- region_graph_build.cpp

// ---- region_paircache.cpp
struct RegionPairCache;   // Action/PathSearch.h
extern "C" RegionPairCache* Pf_RegionPairCache_Build();  // no retail address (the tail of 0x14009e8c0; caller code/Game/Game/region_seed.cpp)
void Pf_RegionPairCacheTick_5d10(int64_t* param_1, const int64_t* param_2);  // 0x1400a5d10
