// code/Game/Work/exe_bodies.h
//
// Declarations of the C bodies in code/Game/Work/ that were reached through the address
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

// ---- build_actions.cpp
class CitizenComponent;
class ComponentEntity;
extern "C" void BuildStructure_SetupBuildActions(CitizenComponent** citizenRef, uint32_t priority, ComponentEntity** building, float* position, uint32_t taskParam, uint32_t altAnim);  // 0x1400c0690

// ---- buildstructure.cpp
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim);  // 0x1400e59c0 (local extern in buildcomponent_state8.cpp, destroycomponent_states.cpp)
extern "C" void* BuildStructure_ConstructForSite(void* buildEntityIBP);  // no retail address (standalone harness entry)
extern "C" int BuildStructure_ForceDrive(void* self, void* citizenIBP);  // no retail address (standalone harness entry)
extern "C" bool BuildStructure_DepsReady();  // no retail address (standalone harness entry)

// ---- foresterwork.cpp
bool ForesterWork_ExecuteReady();  // no retail address (standalone harness entry)
extern "C" int ForesterWork_ForceDrive(void* self, void* citizenIBP);  // no retail address (standalone harness entry)
extern "C" void* ForesterWork_Factory(void);  // no retail address (standalone harness entry)

// ---- gatherrawmaterials.cpp
extern "C" void* GatherRawMaterials_ConstructForSite(void* buildEntityIBP);  // no retail address (standalone harness entry)
extern "C" int GatherRawMaterials_ForceDrive(void* self, void* citizenIBP);  // no retail address (standalone harness entry)
extern "C" bool GatherRawMaterials_DepsReady();  // no retail address (standalone harness entry)

// ---- gatherrawmaterials_ctor.cpp
extern "C" void* GatherRawMaterials_Ctor_Impl(void* mem, void* buildEntityIBP);  // 0x1400ec710 (local extern in buildcomponent_state1.cpp)

// ---- harvestorchard_ctor.cpp
extern "C" void* HarvestOrchard_CtorFromEntity(void* mem, void** entityIBP);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- pasturework.cpp
extern "C" void* PastureWork_CreateForEntity(void* entityRefSlot);  // no retail address (local extern in pasturecomponent_states.cpp)

// ---- plantorchard_ctor.cpp
class ComponentDescription;
extern "C" void* PlantOrchard_CtorFromEntity(void* mem, void** entityIBP, ComponentDescription* treeType);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- tendarea_ctor.cpp
extern "C" void* TendArea_CtorFromEntity(void* mem, void** entityIBP, int kind);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- work_act_clear.cpp
void WorkAct_HarvestAreaCtor_c1530(void* param_1, uint32_t param_2, int param_3, void** param_4, float* param_5, uint32_t param_6, uint32_t);  // 0x1400c1530

// ---- work_act_consume.cpp
void WorkAct_ConsumeProduceCtor_bef70(void* param_1, uint32_t param_2, void* param_3, float* param_4, float param_5, int64_t param_6, uint64_t param_7, uint32_t param_8, int* param_9, uint32_t param_10, uint32_t param_11, float param_12, uint32_t param_13);  // 0x1400bef70

// ---- work_act_hunt.cpp
void WorkAct_HuntWorkCtor_bbbc0(void* citizenIBP, uint32_t workId, int64_t* targetInnerPtr, float* worldPos, float performWorkTime, char selectMoveTo);  // 0x1400bbbc0
class ComponentDescription;
void WorkAct_ShowToolCtor3_bdd20(CitizenComponent** citizenRef, uint32_t workId, ComponentDescription* plantType, uint32_t, float* destination, int loopCount, const int32_t* cells, uint32_t completeParam8, ComponentEntity** messageTarget, uint32_t* msgIdPtr10, uint64_t, const uint32_t* messageArgs);  // 0x1400bdd20

// ---- work_act_path.cpp
extern "C" void* WorkAct_GatherRawMatCtor_bf790(void* retval, void* entity, uint32_t pathType, uint64_t* srcLoc, float* srcPos, void** dstLoc, float* dstPos, int64_t materialDesc, int matCount, int completionCode, uint8_t useAnimal, char twoWay, int flags, uint8_t param_14);  // 0x1400bf790

// ---- work_act_perform.cpp
void WorkAct_PerformWorkCopyCtor_ba9f0(void* citizenIBP, uint32_t workId, float* pos3, float* pos4, float* pos5, int64_t* srcEntity6);  // 0x1400ba9f0
void WorkAct_PerformWorkCtor_bffd0(void* citizenIBP, uint32_t workId, uint32_t* roadP3, int roadP4, uint32_t completeRoadP5, uint8_t roadByte6, uint32_t completeTaskP7, int64_t consumeArg8, int loopCount9, float* pos10, int64_t* srcEntity11);  // 0x1400bffd0

// ---- work_act_rawmat.cpp
void WorkAct_GatherWorkCtor_bb7b0(void* citizenIBP, uint32_t workId, int64_t* workEntity, float* worldPos, uint64_t rawMatP2, char flag6, float rawMatP3, uint32_t rawMatP8, uint32_t param9);  // 0x1400bb7b0
void WorkAct_FishWorkCtor_beae0(void* citizenIBP, uint32_t workId, int64_t* workEntity, float* position, float param5_heading, uint64_t rawMatP2, float rawMatP3, uint32_t param8, uint32_t param9);  // 0x1400beae0

// ---- work_base_accessor.cpp
void* WorkAcc_FindComponent_6330(uintptr_t compList, void* retOut);  // 0x140006330

// ---- work_base_actionctor.cpp
void WorkAct_PerformWorkCtor_c1eb0(void* citizenIBP, uint32_t workId, void* targetIBP, float* worldPos, int, uint8_t zeroByte);  // 0x1400c1eb0
void* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, void* p2, void* p3);  // 0x1400c9b70
void WorkAct_PerformWorkCtor9_c0ad0(void* citizenIBP, uint32_t workId, void* targetIBP, float* worldPos, int cellIdx, uint8_t cancelFirst);  // 0x1400c0ad0
void WorkAct_PlayAnimationCtor_c2490(void* citizenIBP, uint32_t priority, uint32_t citizenIndex);  // 0x1400c2490
void WorkAct_GrazingWorkCtor_bb0a0(void* citizenIBP, uint32_t priority, void* workEntityIBP, uint32_t, float* targetPos, float* srcPos, uint32_t typeId, uint32_t flag8, uint32_t cellIdx9);  // 0x1400bb0a0
void WorkAct_PlantWorkCtor_be6e0(void* citizenIBP, uint32_t priority, void* workEntityInner, uint32_t, float* position, uint32_t createParam6, uint32_t completeParam7, int64_t* condPtr8, uint32_t* msgIdPtr9, uint64_t, uint32_t smParam11);  // 0x1400be6e0

// ---- work_base_bst.cpp
void* WorkBST_RegisterWork_1a30(uintptr_t* root, void* outResult, void* keyIBP);  // 0x140021a30
void WorkBST_BSTRemove_a6e0(uintptr_t* root, void* nodeArg);  // 0x14001a6e0
void WorkBST_BSTFindRemove_25520(uintptr_t* root, void* keyIBP);  // 0x140025520
void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer);  // 0x1400296b0

// ---- work_base_cellbst.cpp
void* WorkCellBST_Insert_9a40(uintptr_t* root, void* outResult, void* keyRec);  // 0x140029a40
void WorkCellBST_Remove_ca60(uintptr_t* root, void* nodeArg);  // 0x14001ca60
void WorkCellBST_Serialize_9de0(uintptr_t* root, void* serializer);  // 0x140029de0

// ---- work_base_grid.cpp
int WorkGrid_FindCell_9ccf0(int* mapGrid, int srcCell, int destCell, uint32_t passFlags);  // 0x14009ccf0
uint64_t WorkGrid_TryAssignWorkplace_f8620(uintptr_t mgr, void* citizenRef, int cellId, char flag);  // 0x1400f8620

// ---- work_base_impl2.cpp
float WorkImpl_Sqrtf_132010(float param_1);  // 0x140132010

// ---- work_base_reimpl.cpp
void* WorkBase_FindComponent_2ed0(uintptr_t entity, void* retIBP);  // 0x140002ed0
void* WorkBase_FindComponent_6270(uintptr_t entity, void* retIBP);  // 0x140006270
void WorkBase_TreeNodeFree_14c30(void* root, void* node);  // 0x140014c30
void WorkBase_TreeNodeFree_e320(void* root, void* node);  // 0x14000e320
void WorkBase_TaskListCleanup_c5400(void* self);  // 0x1400c5400
int WorkBase_CalcDistance_8830(int* mapGrid, uint32_t cellA, uint32_t cellB);  // 0x140008830
uint64_t WorkBase_ValidateCell_a00f0(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);  // 0x1400a00f0

// ---- work_base_walkpos.cpp
bool WorkImpl_WalkablePos_122be0(uintptr_t param_1, uintptr_t param_2, float* param_3, float* param_4, int* param_5);  // 0x140122be0
uint8_t WorkImpl_WalkablePosRecursive_122b00(uintptr_t param_1, int param_2, float* param_3, float* param_4, int* param_5);  // 0x140122b00
uint64_t WorkImpl_BuildStructureStep_6ef0(uintptr_t param_1, void* param_2IBP, uint32_t param_3, uint32_t param_4, void* param_5Time);  // 0x1400f6ef0 B4 FORWARD: 5 outside callers (Work::StepProgress)

// ---- work_leaf_clear.cpp
class CropFieldComponent;
void WorkLeaf_PlantArea_b390(CropFieldComponent* crop);  // 0x14011b390 (a CropFieldComponent member; its header is COMP-c's)
void WorkLeaf_BstRemoveByIBP_298a0(void* treeRootSlot, void* keyIBP);  // 0x1400298a0
void* WorkLeaf_BstInsertByIBP_29920(void* treeRootSlot, void* outNode, void* insIBP);  // 0x140029920
void WorkLeaf_BstSerializeIBPpair_29c10(void* treeRootSlot, void* ser);  // 0x140029c10
void WorkLeaf_SortTilesByDistance_9d7c0(int* mapGrid, void* tileArray, int refCell);  // 0x14009d7c0
void WorkLeaf_GetTilesInArea_9fc00(int* grid, int x, int y, int w, int h, void* outArr, char collectEmpty);  // 0x14009fc00

// ---- work_leaf_gather.cpp
int WorkLeaf_FreeStorageCapacity_116540(intptr_t self);  // 0x140116540
void* WorkLeaf_FindGrowthDescription_c3c0(intptr_t entity, void* retOut);  // 0x14000c3c0
long long WorkLeaf_FootprintOwnerCheck_120c30(intptr_t self, int col, int row);  // 0x140120c30

// ---- work_leaf_hunt.cpp
void* WorkLeaf_FindComp_63d0(uintptr_t entity, void** retval);  // 0x1400063d0
bool WorkLeaf_ConsumeProduce_93a0(uintptr_t self);  // 0x1401193a0
int WorkLeaf_Pasture_8350(uintptr_t self);  // 0x140128350
void WorkLeaf_ConsumeProduce_b310(uintptr_t self);  // 0x14012b310

// ---- work_leaf_market.cpp
void WorkLeaf_MarketTreeCleanup_2af40(void* ctx, void* node);  // 0x14002af40
