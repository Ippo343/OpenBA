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
void* ActCNR_FindComponent_3f60(uintptr_t entity, void* retIBP);  // 0x140003f60
void* ActCNR_FindComponent_c460(uintptr_t entity, void* retIBP);  // 0x14000c460
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
void ActCNR_SetGrowthProgress_120db0(int64_t self, float progress);  // 0x140120db0
uint32_t ActCNR_NR_GetVariantCount_1252f0(int64_t comp);  // 0x1401252f0
uint32_t ActCNR_NR_GetSubVariantCount_125320(int64_t comp, int variant);  // 0x140125320
void ActCNR_NR_SetVariant_1253d0(int64_t* comp, uint16_t variant, int16_t subVariant);  // 0x1401253d0
void ActCNR_SetAnimIndex_126440(int64_t self, uint32_t animIdx);  // 0x140126440
uint64_t ActCNR_SetupModelAtCell_126540(int64_t self, int cellIndex);  // 0x140126540
void ActCNR_RemoveNatRes_1260f0(int64_t self, void* citizenIBP, char harvestFlag);  // 0x1401260f0

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
void ActCt_ShowToolOnEntity_1173b0(void* entityV, int toolId);  // 0x1401173b0

// ---- action_base_hunt.cpp
void* ActHunt_FindComponent_5500(uintptr_t entity, void* retIBP);  // 0x140005500
void* ActHunt_FindComponent_27e0(uintptr_t entity, void* retIBP);  // 0x1400227e0
class HuntComponent;          // Component/HuntComponent.h
class ComponentEntity;        // Entity/ComponentEntity.h
template <class T> struct RawArray;   // Action/PathSearch.h
bool HuntIsReady(const HuntComponent* hunt);  // 0x140122a80
void QwordArraySetSize(RawArray<void*>* arr, int newCount, void* const* fill);  // 0x140003110
void MarkEntityForRemoval(ComponentEntity* entity);  // 0x1400c2ea0
void NotifyEntityChanged(ComponentEntity* entity);  // 0x1400c3470 (a second body of ComponentEntity::RefreshEntityFlags)
bool ActHunt_IsAnimalReady_2a80(void* self);  // 0x140122a80 B4 FORWARD -> HuntIsReady
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
void* ActMv_FindComponentFamily_12d0(uintptr_t entity, void* retIBP);  // 0x1400012d0
void ActMv_ExternalPtrSerialize_22970(void* extPtrSlot, void* serializer);  // 0x140022970
void ActMv_SetFacingDirection_a060(void* comp, void* vector3);  // 0x14010a060
void ActMv_UpdateFacing_c8d20(void* selfp);  // 0x1400c8d20
void ActMv_SetAnimation_a1f0(void* selfp, uint32_t animId, float p3, float p4);  // 0x14010a1f0
uint8_t ActMv_CitizenBreakCheck_116650(void* entityp);  // 0x140116650
void ActMv_SetTerrainCosts_e550(void* mapGrid, void* pathfinder, int profile, void* param4);  // 0x14009e550
int ActMv_CopyPathFromFinder_a6d50(void* pathfinder, void* slotIdPtr, void* pathArray, int maxNodes);  // 0x1400a6d50
void* ActMv_PathfinderQuery_9cfb0(void* gridp, void* outResultp, uint64_t flags, uint32_t destCell, uint32_t srcCell);  // 0x14009cfb0
void* ActMv_PathSetup_c8c10(void* selfp, void* worldPosp, void*, void* param4);  // 0x1400c8c10

// ---- action_base_movedeps.cpp
class ModelComponent;         // Component/ModelComponent.h
namespace System { class Time; class Random; }
bool BreakTimeCheck(System::Time* time, const float* pair, System::Random* rng);  // 0x1400a9290
void PlayModelAnimation(ModelComponent* model, uint32_t animId, float p3, float p4, float p5);  // 0x1401250f0
bool ActMvDep_TimeCheck_a9290(void* timeV, float* pair, void* rng);  // 0x1400a9290
void ActMvDep_AnimHelper_50f0(uintptr_t self, uint32_t animId, float p3, float p4, float p5);  // 0x1401250f0

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
void ActRm_DynArrayPush4_44810(uintptr_t self, uint32_t value);  // 0x140044810
int ActRm_GetResourceCategory_88f90(void*, uintptr_t entity);  // 0x140088f90
void* ActRm_FindComponent_223c0(uintptr_t entity, void* retIBP);  // 0x1400223c0
void* ActRm_FindComp_013320(uintptr_t self, void* retIBP);  // 0x140013320
void ActRm_VectorClone_d9d0(int64_t* dst, int64_t* src);  // 0x14000d9d0
void ActRm_ArrayUpsert_58fd0(uintptr_t self, int64_t key, int value);  // 0x140058fd0
void ActRm_RegisterRawMaterial_88ef0(int64_t entityList, void* sourceIBP, char placeNear, int64_t descEntity, int quantity);  // 0x140088ef0
void ActRm_PositionEntity_9760(int64_t param_1, int cellIndex, float rotation);  // 0x140129760

// ---- action_child_ctors.cpp
void* ActChild_ShowToolCtor_42890(void* self, void* param_2, int toolType);  // 0x140042890
void* ActChild_RemoveNRCopyCtor_cadf0(void* self, uint64_t* param_2, uint64_t* param_3, uint8_t param_4);  // 0x1400cadf0
void* ActChild_AnimateNRCopyCtor_c57d0(void* self, uint64_t* param_2, uint32_t param_3, uint8_t param_4);  // 0x1400c57d0
void* ActChild_CreateNRCopyCtor_c6a20(void* self, uint64_t param_2, uint64_t* param_3, uint32_t param_4);  // 0x1400c6a20
class ComponentEntity;        // Entity/ComponentEntity.h
float HeadingToward(const float* worldPos, ComponentEntity* const* entityIBP);  // 0x1400c29a0
float ActChild_Entity29a0_c29a0(float* worldPos, void* param_2);  // 0x1400c29a0

// ---- action_child_ctors2.cpp
void* ActChild_CompleteRoadCopyCtor_c59d0(void* self, uint32_t p2, uint32_t p3, uint8_t p4);  // 0x1400c59d0
void* ActChild_HuntAnimalCopyCtor_c8400(void* self, uint64_t* p2, uint64_t* p3);  // 0x1400c8400
void* ActChild_DropOffRawCopyCtor_c7cd0(void* self, uint64_t* p2, uint64_t* p3, uint64_t p4, uint32_t p5, uint8_t p6);  // 0x1400c7cd0
void Entity_ConsumeListAppend_64b0(int64_t self, int64_t resType, int count);  // 0x1400c64b0
void* ActChild_ConsumeRawCopyCtor_c63b0(void* self, uint64_t* p2, int64_t p3, uint64_t, uint8_t p5);  // 0x1400c63b0
void ActChild_TransferToCarrier_ca230(int64_t self, int64_t resType, int count);  // 0x1400ca230
void* ActChild_PickupStoredCopyCtor_ca520(void* self, uint64_t* param_2, uint64_t* param_3, int64_t param_4, int param_5, uint8_t param_6);  // 0x1400ca520
void* ActChild_ConsumeRawCtor3_c6440(void* self, uint64_t* param_2);  // 0x1400c6440
void* Entity_FindConsumeProduce_b6f0(int64_t param_1, void* param_2);  // 0x14000b6f0
class ConsumeProduceComponent;   // Component/ConsumeProduceComponent.h
uint64_t PickConsumeRecipe(const ConsumeProduceComponent* self);  // 0x140119470
uint64_t WeightedRandomIndex_119470(int64_t param_1);  // 0x140119470

// ---- action_pf_astar.cpp
void Pf_WorkerApplyCmd_a6430(int64_t param_1, uint32_t* param_2);  // 0x1400a6430 -> PathfinderThread::ApplyCommand
void Pf_EnqueueCmd2_a6fe0(int64_t param_1, int cell, int terrainType);  // 0x1400a6fe0 -> PathfinderThread::SetCellTerrain
void Pf_EnqueueCmd4_a68b0(int64_t param_1, uint8_t diagonal, uint8_t manhattan, uint8_t respectBlocked);  // 0x1400a68b0 -> PathfinderThread::SetSearchFlags
void Pf_EnqueueCmd1_a6a30(int64_t param_1, int terrainType, int cost);  // 0x1400a6a30 -> PathfinderThread::SetStepCost
void Pf_EnqueueCmd0_a6b20(int64_t param_1, int mask);  // 0x1400a6b20 -> PathfinderThread::SetPassableTypes
int* Pf_EnqueueTradeRoute_a6c10(int64_t param_1, int* ticket, int start, int goal);  // 0x1400a6c10 -> PathfinderThread::RequestPath
void Pf_FloodFillRegion_9c170(int* param_1, uint32_t param_2, uint16_t param_3);  // 0x14009c170

// ---- action_pf_avl.cpp
extern "C" int64_t Pf_AvlRotateB_e450(int64_t* root, int64_t node);  // 0x14000e450
extern "C" int64_t Pf_AvlRotateB_e580(int64_t* root, int64_t node);  // 0x14000e580
extern "C" int64_t Pf_AvlRotateL_14c70(int64_t* root, int64_t node);  // 0x140014c70
extern "C" int64_t Pf_AvlRotateR_14da0(int64_t* root, int64_t node);  // 0x140014da0
extern "C" void Pf_AvlRebalanceB_13480(int64_t* root, int64_t* node);  // 0x140013480
extern "C" void Pf_AvlRebalanceA_a8c0(int64_t* root, int64_t* node);  // 0x14001a8c0
extern "C" uint64_t* Pf_AvlInsertA_a4f0(int64_t* param_1, uint64_t* param_2, uint32_t* param_3);  // 0x14001a4f0
extern "C" uint64_t* PathBST_Insert_25a10(int64_t* param_1, uint64_t* param_2, uint32_t* param_3);  // 0x140025a10
extern "C" void PathPairs_Clear_25b30(int64_t* param_1);  // 0x140025b30

// ---- action_pf_ctors.cpp
void Pf_ActStackPush_54e0(int64_t* param_1, void* param_2_);  // 0x1400c54e0
void* Pf_CompleteTaskCopyCtor_5da0(void* param_1, uint32_t param_2, uint64_t* param_3, uint64_t* param_4, uint32_t param_5);  // 0x1400c5da0
void* Pf_CreateRawMaterialCopyCtor_71a0(void* param_1, uint64_t param_2, float param_3, uint64_t* param_4, uint64_t* param_5, uint64_t param_6, uint32_t param_7, uint32_t param_8, uint8_t param_9, uint64_t* param_10, uint32_t param_11);  // 0x1400c71a0
void* Pf_MoveToCtor_9170(void* self, void* param_2, uint32_t pathType, int param_4, float* destination, int param_6, int param_7, float speed);  // 0x1400c9170
void* Pf_PerformWorkCtor7_9720(void* self, void* entity, float animIndex, uint32_t actionType, int64_t time, uint8_t param_6, int param_7);  // 0x1400c9720
void* Pf_SendComponentMessageCopyCtor_b1d0(void* param_1, uint64_t* param_2, uint32_t* param_3, uint32_t param_4, uint64_t* param_5, uint32_t param_6);  // 0x1400cb1d0
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
int Pf_RegionSpiral_9d0a0(int* param_1, int param_2);  // 0x14009d0a0
void Pf_QuadtreeSubdivide_18880(uintptr_t param_1);  // 0x140018880 B4 FORWARD -> CharacterGridWalker::Subdivide
uintptr_t Pf_QuadtreeInit_18dd0(uintptr_t param_1, float* param_2, uint32_t param_3, uint32_t param_4);  // 0x140018dd0 B4 FORWARD -> CharacterGridWalker::Init
class PopulationComponent;    // Component/PopulationComponent.h
int Pf_PastureTargetPick_53ca0(const PopulationComponent* population, int herdCell);  // 0x140053ca0

// ---- action_pf_pools.cpp
void Pf_BstFreeRecursive_d400(uintptr_t param_1, uint64_t* param_2);  // 0x14001d400
void Pf_ArrayResize18_d830(int64_t* param_1, int param_2);  // 0x14001d830
void Pf_FreeArray30_e490(uint64_t param_1, uintptr_t param_2, int param_3);  // 0x14001e490
void Pf_VectorResize14_ee40(int64_t* param_1, int param_2);  // 0x14001ee40
void Pf_VectorResize14Fill_f8d0(int64_t* param_1, int param_2, uint64_t* param_3);  // 0x14001f8d0
void Pf_VectorResize60_fa80(int64_t* param_1, int param_2, uint64_t* param_3);  // 0x14001fa80
int64_t* Pf_IntArrayClone_f9e0(int64_t* param_1, int64_t* param_2);  // 0x14000f9e0

// ---- action_pf_region.cpp
void Pf_FlushDirtyChunks_8a90(int64_t param_1);  // 0x1400a8a90 (local extern in code/Game/Game/region_seed.cpp (reg_pf_region.cpp prototype registers nothing))
int Pf_RegionPathLength_8ba0(int* param_1, int param_2, uint32_t param_3, uint64_t param_4);  // 0x1400a8ba0
void Pf_RegionPairCacheInsert_5b30(uint64_t* param_1, int param_2, int param_3, uint32_t param_4);  // 0x1400a5b30

// ---- action_pf_road.cpp
void Pf_RoadQuad_cc10(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend);  // 0x1400acc10 B4 FORWARD -> BuildRoadQuad
int Pf_CellQuadRay_aa20(int64_t self, int x, int y, void* p4, float* p5, float* p6, float* p7, float* p8, uint32_t* p9);  // 0x1400aaa20 B4 FORWARD -> RayTestCell
float Pf_CellQuadRayWrap_0f90(int64_t self, float* param_2, uint32_t* param_3);  // 0x1400b0f90 B4 FORWARD -> TerrainHeightUnder
void Pf_RegionDirty4_b150(int64_t self, int x, int y);  // 0x1400ab150
void Pf_FineCellDirty_a8c90(int64_t self, int x, int y);  // 0x1400a8c90
float* InteractNodePosition(const InteractComponent* self, float* out, int kind, int index, float* heading);  // 0x140122e20
int PickReachableCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14004edb0
int EntityAnchorCell(ComponentEntity* const* entityIBP, float* outPos);  // 0x140087fb0
void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5);  // 0x140122e20 B4 FORWARD -> InteractNodePosition
int Pf_PickReachableCell_edb0(int64_t param_1, uint32_t param_2, float* param_3);  // 0x14004edb0 B4 FORWARD -> PickReachableCell
int Pf_EntityAnchorCell_7fb0(void* param_1IBP, float* param_2);  // 0x140087fb0 B4 FORWARD -> EntityAnchorCell
void Pf_RoadMeshRebuild_f370(int64_t param_1, uint32_t* param_2);  // 0x1400af370

// ---- action_pf_roots_drop.cpp
class AnimalDescription;      // Component/AnimalDescription.h
class CitizenComponent;       // Component/CitizenComponent.h
class DropOffRawMaterial;     // Action/DropOffRawMaterial.h
struct ActionStack;           // Action/Action.h
void DropRawMaterialOnGround(CitizenComponent* citizen, ComponentDescription* material, int count);  // 0x1400c78c0
void SpillAllStorage(DropOffRawMaterial* self);  // 0x1400c7570
uint64_t DropOffExec(DropOffRawMaterial* self, ActionStack* stack, StorageComponent* const* target, ComponentDescription* material);  // 0x1400c7620
void RemoveAnimalActions(CitizenComponent* const* citizenIBP, uint32_t task, float* destination, uint32_t createFlag, Component* const* animal, const AnimalDescription* description, uint32_t gate);  // 0x1400bbea0
void Pf_SpillAllStorage_7570(int64_t param_1);  // 0x1400c7570 B4 FORWARD -> SpillAllStorage
uint64_t Pf_DropOffExec_7620(int64_t param_1, int64_t* param_2, void* param_3, uint64_t param_4);  // 0x1400c7620 B4 FORWARD -> DropOffExec
void Pf_RemoveAnimalCtor_bbea0(void* param_1, uint32_t param_2, uint32_t* param_3, uint32_t param_4, void* param_5, int64_t param_6, uint32_t param_7);  // 0x1400bbea0 B4 FORWARD -> RemoveAnimalActions

// ---- action_pf_roots_misc.cpp
class CharacterComponent;     // Component/CharacterComponent.h
class CharacterGrid;          // Component/CharacterGrid.h
class HerdComponent;          // Component/HerdComponent.h
namespace Math { class Scalar; }
void QueryNearestCharacter(const CharacterGrid* grid, CharacterComponent** best, const float* point, const Math::Scalar* radius, int typeId, CharacterComponent* const* exclude);  // 0x140092260
void HerdStartMigration(HerdComponent* herd);  // 0x140122340
int Pf_FindPlacementCell_bd40(int* param_1, int param_2, char param_3, int64_t* param_4, uint32_t param_5);  // 0x14009bd40 B4 FORWARD -> Map::FindPlacementCell
int64_t* Pf_SpatialProximityQuery_92260(float* param_1, int64_t* param_2, float* param_3, void* param_4, int param_5, void* param_6);  // 0x140092260 B4 FORWARD -> QueryNearestCharacter
void Pf_HerdStartMigration_122340(void* param_1);  // 0x140122340 B4 FORWARD -> HerdStartMigration

// ---- action_pf_roots_road.cpp
void Pf_ZoneRefresh_9c480(int* param_1);  // 0x14009c480
void Pf_SetTileRoadFlag_9f2f0(int* param_1, int param_2, unsigned int param_3);  // 0x14009f2f0
void Pf_RefreshRegionHeight_9e0e0(unsigned int* param_1, int param_2, int param_3, int param_4, int param_5);  // 0x14009e0e0
void Pf_FinalizeRoadTile_9c640(unsigned int* param_1, int param_2, uint8_t param_3, unsigned int param_4);  // 0x14009c640

// ---- action_pf_roots_route.cpp
bool StorageCanAdd(StorageComponent* storage, const ComponentDescription* material, int count);  // 0x14012cd80
int FindBestAdjacentCell(const Component* target, uint32_t goalCell, float* outPos);  // 0x14012d5d0
ComponentEntity** PickBestDropTarget(const RawArray<ComponentEntity*>* lists, ComponentEntity** best, uint32_t goalCell, const ComponentDescription* material, int count, char useCapacity, float* outPos, ComponentEntity* const* exclude);  // 0x14008d590
// B4 FORWARD C spellings (defined in action_pf_ctors.cpp)
int Pf_IsDstReachable_f810(int* param_1, uint32_t param_2, uint32_t param_3, char param_4);  // 0x14009f810 -> Map::PathDistance
uint32_t Pf_StorageCanAccept_cd80(int64_t param_1, int64_t param_2, int param_3);  // 0x14012cd80 -> StorageCanAdd
int Pf_FindBestAdjacentCell_d5d0(int64_t param_1, uint32_t param_2, float* param_3, int64_t param_4);  // 0x14012d5d0 -> FindBestAdjacentCell
int64_t* Pf_PickBestDropTarget_8d590(int64_t** param_1, int64_t* param_2, uint32_t param_3, uint64_t param_4, int param_5, char param_6, uint32_t* param_7, void* param_8);  // 0x14008d590 -> PickBestDropTarget

// ---- keystone_a_zone.cpp
int RoadTool_helper_6e30(int64_t param_1, unsigned int param_2, uint64_t param_3, void* param_4, int64_t param_5, int param_6);  // 0x1400a6e30
void ZoneComponent_sub_d360(int* param_1, int64_t* param_2, int param_3, int param_4, unsigned int param_5, int param_6, int64_t* param_7, int param_8);  // 0x14009d360
int RoadTool_sub_ce00(int64_t param_1, unsigned int param_2, unsigned int param_3, uint64_t param_4, void* param_5, int64_t param_6, uint8_t param_7, uint8_t param_8);  // 0x14009ce00

// ---- region_graph_build.cpp
int* Pf_RegionGraphCtor_8670(int* param_1, int64_t param_2, int64_t param_3, int64_t param_4, uint32_t param_5, int param_6);  // 0x1400a8670 (local extern in code/Game/Game/region_seed.cpp (reg_region_graph.cpp prototype registers nothing))

// ---- region_paircache.cpp
extern "C" void* Pf_RegionPairCache_Build();  // no retail address (local extern in code/Game/Game/region_seed.cpp)
void Pf_RegionPairCacheTick_5d10(int64_t* param_1, const int64_t* param_2);  // 0x1400a5d10
