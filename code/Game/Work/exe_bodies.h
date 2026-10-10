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
namespace Object { template <class T> class InternalPtr; }
extern "C" void BuildStructure_SetupBuildActions(CitizenComponent** citizenRef, uint32_t priority, const Object::InternalPtr<ComponentEntity>* building, float* position, uint32_t taskParam, uint32_t altAnim);  // 0x1400c0690

// ---- buildstructure.cpp
class BuildStructure;
class Work;               // Work/Work.h
class GatherRawMaterials; // Work/GatherRawMaterials.h
extern "C" BuildStructure* BuildStructure_CreateInPlaceFull(void* mem, ComponentEntity** entityIBP, bool altAnim);  // 0x1400e59c0 (buildcomponent_state8.cpp, destroycomponent_states.cpp)
extern "C" BuildStructure* BuildStructure_ConstructForSite(ComponentEntity** buildEntityIBP);  // no retail address (standalone harness entry)
extern "C" int BuildStructure_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)
extern "C" bool BuildStructure_DepsReady();  // no retail address (standalone harness entry)

// ---- foresterwork.cpp
bool ForesterWork_ExecuteReady();  // no retail address (standalone harness entry)
extern "C" int ForesterWork_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)

// ---- gatherrawmaterials.cpp
extern "C" GatherRawMaterials* GatherRawMaterials_ConstructForSite(ComponentEntity* const* buildEntityIBP);  // no retail address (standalone harness entry)
extern "C" int GatherRawMaterials_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)
extern "C" bool GatherRawMaterials_DepsReady();  // no retail address (standalone harness entry)

// ---- gatherrawmaterials_ctor.cpp
class Work;   // Work/Work.h
extern "C" Work* GatherRawMaterials_Ctor_Impl(void* mem, ComponentEntity* const* buildEntityIBP);  // 0x1400ec710 (caller buildcomponent_state1.cpp)

// ---- harvestorchard_ctor.cpp
extern "C" Work* HarvestOrchard_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- pasturework.cpp
extern "C" Work* PastureWork_CreateForEntity(ComponentEntity* const* entityRefSlot);  // no retail address (local extern in pasturecomponent_states.cpp)

// ---- plantorchard_ctor.cpp
class ComponentDescription;
extern "C" Work* PlantOrchard_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP, ComponentDescription* treeType);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- tendarea_ctor.cpp
extern "C" Work* TendArea_CtorFromEntity(void* mem, ComponentEntity* const* entityIBP, int kind);  // no retail address (local extern in orchardcomponent_states.cpp)

// ---- work_act_clear.cpp
class NaturalResourceComponent;   // Component/NaturalResourceComponent.h
void WorkAct_HarvestAreaCtor_c1530(CitizenComponent* const* citizenRef, uint32_t param_2, int param_3, NaturalResourceComponent* const* targets, float* param_5, uint32_t param_6, uint32_t);  // 0x1400c1530

// ---- work_act_consume.cpp
void WorkAct_ConsumeProduceCtor_bef70(CitizenComponent* const* citizenRef, uint32_t param_2, ComponentEntity* const* entityRef, float* param_4, float param_5, ComponentDescription* param_6, ComponentDescription* rawMatDesc, uint32_t param_8, int* param_9, uint32_t param_10, uint32_t param_11, float param_12, uint32_t param_13);  // 0x1400bef70

// ---- work_act_hunt.cpp
class HuntComponent;
void WorkAct_HuntWorkCtor_bbbc0(CitizenComponent* const* citizenIBP, uint32_t workId, HuntComponent* const* huntRef, float* worldPos, float performWorkTime, char selectMoveTo);  // 0x1400bbbc0
class ComponentDescription;
void WorkAct_ShowToolCtor3_bdd20(CitizenComponent** citizenRef, uint32_t workId, ComponentDescription* plantType, uint32_t, float* destination, int loopCount, const int32_t* cells, uint32_t completeParam8, ComponentEntity** messageTarget, uint32_t* msgIdPtr10, uint64_t, const uint32_t* messageArgs);  // 0x1400bdd20

// ---- work_act_path.cpp
class Action;
namespace Object { template <class T> class InternalPtr; }
extern "C" ::Object::InternalPtr<Action>* WorkAct_GatherRawMatCtor_bf790(::Object::InternalPtr<Action>* ret, CitizenComponent* const* citizen, uint32_t pathType, ComponentEntity* const* src, float* srcPos, ComponentEntity* const* dst, float* dstPos, ComponentDescription* material, int matCount, int completionCode, uint8_t useAnimal, char twoWay, int flags, uint8_t param_14);  // 0x1400bf790

// ---- work_act_perform.cpp
void WorkAct_PerformWorkCopyCtor_ba9f0(CitizenComponent* const* citizenRef, uint32_t workId, float* pos3, float* pos4, float* pos5, ComponentEntity* const* srcRef6);  // 0x1400ba9f0
void WorkAct_PerformWorkCtor_bffd0(CitizenComponent* const* citizenRef, uint32_t workId, float* destination, int roadP4, uint32_t completeRoadP5, uint8_t roadByte6, uint32_t completeTaskP7, ComponentDescription* material, int loopCount9, float* pos10, ComponentEntity* const* srcRef11);  // 0x1400bffd0

// ---- work_act_rawmat.cpp
void WorkAct_GatherWorkCtor_bb7b0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* workRef, float* worldPos, ComponentDescription* rawMatDesc, char flag6, float rawMatP3, uint32_t rawMatP8, uint32_t param9);  // 0x1400bb7b0
void WorkAct_FishWorkCtor_beae0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* workRef, float* position, float param5_heading, ComponentDescription* rawMatDesc, float rawMatP3, uint32_t param8, uint32_t param9);  // 0x1400beae0

// ---- work_base_accessor.cpp
class RawMaterialDescription;
RawMaterialDescription** WorkAcc_FindComponent_6330(ComponentDescription* desc, RawMaterialDescription** retOut);  // 0x140006330

// ---- work_base_actionctor.cpp
class NaturalResourceComponent;
void WorkAct_PerformWorkCtor_c1eb0(CitizenComponent* const* citizenRef, uint32_t workId, NaturalResourceComponent* const* targetRef, float* worldPos, int, uint8_t zeroByte);  // 0x1400c1eb0
class PickupRawMaterial;
PickupRawMaterial* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, CitizenComponent* const* p2, ComponentEntity* const* p3);  // 0x1400c9b70
void WorkAct_PerformWorkCtor9_c0ad0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* targetIBP, float* worldPos, int cellIdx, uint8_t cancelFirst);  // 0x1400c0ad0
void WorkAct_PlayAnimationCtor_c2490(CitizenComponent* const* citizenRef, uint32_t priority, uint32_t citizenIndex);  // 0x1400c2490
void WorkAct_GrazingWorkCtor_bb0a0(CitizenComponent* const* citizenRef, uint32_t priority, ComponentEntity* const* workEntityIBP, uint32_t, float* targetPos, float* srcPos, uint32_t typeId, uint32_t flag8, uint32_t cellIdx9);  // 0x1400bb0a0
void WorkAct_PlantWorkCtor_be6e0(CitizenComponent* const* citizenRef, uint32_t priority, ComponentDescription* plantDesc, uint32_t, float* position, uint32_t createParam6, uint32_t completeParam7, ComponentEntity* const* parentRef, uint32_t* msgIdPtr9, uint64_t, uint32_t smParam11);  // 0x1400be6e0

// ---- work_base_bst.cpp
void* WorkBST_RegisterWork_1a30(uintptr_t* root, void* outResult, void* keyIBP);  // 0x140021a30
void WorkBST_BSTRemove_a6e0(uintptr_t* root, void* nodeArg);  // 0x14001a6e0
void WorkBST_BSTFindRemove_25520(uintptr_t* root, void* keyIBP);  // 0x140025520
namespace IO { class Serializer; }
void WorkBST_BST_Serialize_296b0(uintptr_t* root, IO::Serializer* serializer);  // 0x1400296b0
// The typed entry points a Work leaf's real `Collection::Set<T*>` member is handed to: one
// retail body serves every 8-byte-key set (identical-code folding), so the Set's {root,
// count} header is given to the ONE erased core here, once per entry point.
#include "collection/Collection.h"
template <class T>
inline void* WorkBST_RegisterWork_1a30(Collection::Set<T*>* set, void* outResult, void* keyIBP) {
    return WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(set), outResult, keyIBP);  // CAST-OK: elem_stride
}
template <class T>
inline void WorkBST_BSTRemove_a6e0(Collection::Set<T*>* set, typename Collection::Set<T*>::Node* node) {
    WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(set), node);  // CAST-OK: elem_stride
}
template <class T>
inline void WorkBST_BSTFindRemove_25520(Collection::Set<T*>* set, void* keyIBP) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(set), keyIBP);  // CAST-OK: elem_stride
}
template <class T>
inline void WorkBST_BST_Serialize_296b0(Collection::Set<T*>* set, IO::Serializer* serializer) {
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(set), serializer);  // CAST-OK: elem_stride
}
// A function-local set held as its raw {root, count} header (Action/PathAvl.h's
// AvlTreeHead: no ctor, no dtor), e.g. MarketWork::Persist's version-0 migration set.
template <class Node> struct AvlTreeHead;   // Action/PathAvl.h
template <class Node>
inline void WorkBST_BST_Serialize_296b0(AvlTreeHead<Node>* head, IO::Serializer* serializer) {
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(head), serializer);  // CAST-OK: elem_stride
}
// The same core unlinking a node of a raw {root, count} tree (WorkScheduler's queue-order tree).
template <class Node>
inline void WorkBST_BSTRemove_a6e0(AvlTreeHead<Node>* head, Node* node) {
    WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(head), node);  // CAST-OK: elem_stride
}

// ---- work_base_cellbst.cpp
void* WorkCellBST_Insert_9a40(uintptr_t* root, void* outResult, void* keyRec);  // 0x140029a40
// 0x14001ca60 WorkCellBST_Remove_ca60: the template over the node type, Work/CellBst.h (B7 LX).
#include "Work/CellBst.h"
namespace IO { class Serializer; }
void WorkCellBST_Serialize_9de0(uintptr_t* root, IO::Serializer* ser);  // 0x140029de0
// The typed entry points an area work's real `Collection::Set<T>` member (a 0x10-byte
// {int key, pointer} element, 0x30-byte node) is handed to: one retail body per entry serves
// every such set (ICF), so the Set's {root, count} header is given to the ONE erased core here.
template <class T>
inline void* WorkCellBST_Insert_9a40(Collection::Set<T>* set, void* outResult, void* keyRec) {
    static_assert(sizeof(T) == 0x10, "the cell core's element is 0x10 bytes");
    return WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>(set), outResult, keyRec);  // CAST-OK: elem_stride
}
// The work scheduler's {root, count} trees (Game/WorkScheduler.h: the id map at
// scheduler +0x30 and every bucket's entries) are handed to the same two cores.
struct WorkSchedulerTree;   // Game/WorkScheduler.h
inline void* WorkCellBST_Insert_9a40(WorkSchedulerTree* tree, void* outResult, void* keyRec) {
    return WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>(tree), outResult, keyRec);   // (the typed wrapper's one hand-off into the erased core)  // CAST-OK: elem_stride
}
template <class T>
inline void WorkCellBST_Serialize_9de0(Collection::Set<T>* set, IO::Serializer* ser) {
    static_assert(sizeof(T) == 0x10, "the cell core's element is 0x10 bytes");
    WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>(set), ser);  // CAST-OK: elem_stride
}

// ---- work_base_grid.cpp
class Map;   // Game/Map.h
int WorkGrid_FindCell_9ccf0(Map* map, int srcCell, int destCell, uint32_t passFlags);  // 0x14009ccf0
class WorkScheduler;   // Game/WorkScheduler.h
uint64_t WorkGrid_TryAssignWorkplace_f8620(WorkScheduler* mgr, CitizenComponent** citizenRef, int cellId, char flag);  // 0x1400f8620

// ---- work_base_impl2.cpp
float WorkImpl_Sqrtf_132010(float param_1);  // 0x140132010

// ---- work_base_reimpl.cpp
void WorkBase_TreeNodeFree_14c30(void* root, void* node);  // 0x140014c30
void WorkBase_TreeNodeFree_e320(void* root, void* node);  // 0x14000e320
struct ActionStack;
int WorkBase_CalcDistance_8830(Map* map, uint32_t cellA, uint32_t cellB);  // 0x140008830
uint64_t WorkBase_ValidateCell_a00f0(Map* map, uint32_t pathType, uint32_t destCell, int srcCell);  // 0x1400a00f0

// ---- work_base_walkpos.cpp
#include "Component/InteractDescription.h"   // InteractDescription::NodeArray
class InteractComponent;
bool WorkImpl_WalkablePos_122be0(InteractComponent* interact, const InteractDescription::NodeArray* nodes, float* param_3, float* param_4, int* param_5);  // 0x140122be0 B8: stays free -- DIFF: as InteractComponent::FindWalkablePos, CitizenAIComponent::State_Work (citizenai_handlers3.cpp:326) loses its post-call `if (posComp != nullptr)` arm (GCC: `this` is non-null; 17 -> 16 calls). The callee derefs RCX (+0x10) first, so a null never passed the call; movable if that fold is accepted

// ---- work_leaf_clear.cpp
class CropFieldComponent;
#include "Work/Work.h"   // WorkEntityAssignment, WorkEntitySetNode (the 0x140029920 out slot)
void WorkLeaf_BstRemoveByIBP_298a0(Collection::Set<WorkEntityAssignment>* set, ComponentEntity* const* key);  // 0x1400298a0
WorkEntitySetNode** WorkLeaf_BstInsertByIBP_29920(Collection::Set<WorkEntityAssignment>* set, WorkEntitySetNode** outNode, const WorkEntityAssignment* ins);  // 0x140029920
void WorkLeaf_BstSerializeIBPpair_29c10(Collection::Set<WorkEntityAssignment>* set, IO::Serializer* ser);  // 0x140029c10
struct GridTile;   // Work/GridTile.h
template <class T> struct RawArray;   // Action/PathSearch.h
void WorkLeaf_SortTilesByDistance_9d7c0(const Map* map, RawArray<GridTile>* tiles, int refCell);  // 0x14009d7c0
void WorkLeaf_GetTilesInArea_9fc00(const Map* map, int x, int y, int w, int h, RawArray<GridTile>* outArr, char collectEmpty);  // 0x14009fc00

// ---- work_leaf_gather.cpp
class Component;
class GrowthDescription;
GrowthDescription** WorkLeaf_FindGrowthDescription_c3c0(const ComponentDescription* desc, GrowthDescription** retOut);  // 0x14000c3c0

// ---- work_leaf_hunt.cpp
class ConsumeProduceComponent;
class PastureComponent;
class ResourceProduceComponent;

// ---- work_leaf_market.cpp (WorkLeaf_MarketTreeCleanup_2af40: Work/MarketWork.h)
