// src/game/Work/exe_bodies.h
//
// Declarations of the C bodies in src/game/Work/ that were reached through the address
// registry (RegisterExeAddr rows, the readers' ResolveAddr(Module::Exe, ...) / helper /
// X_ResolveFuncs / engine_funcs doors), or through a local extern prototype in the caller.
// B3R_RULES R5: every DIRECT body is declared in exactly one header, written from its
// DEFINITION (linkage included), and each defining .cpp includes this header so the
// compiler checks the pair. Readers call these by name through this header (R2).
// Making them members of real classes is B.4's work; this header is where it finds them.
//
// Grouped by defining file; the retail address is the trailing comment on each line.
// `uint` in a definition (a file-local typedef there) is spelled `unsigned int` here.
// The address -> name table is docs/history/endgame_b/b3r_session/data/P0a_declared.tsv.
#pragma once

#include <cstddef>
#include <cstdint>

// ---- build_actions.cpp
class CitizenComponent;
class ComponentEntity;
namespace Object { template <class T> class InternalPtr; }
extern "C" void BuildStructure_SetupBuildActions(CitizenComponent** citizenRef, uint32_t priority, const Object::InternalPtr<ComponentEntity>* building, float* position, uint32_t taskParam, uint32_t altAnim);  // 0x1400c0690  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1400e571f LEA RCX,[RBP+0x7f]), not an object

// ---- buildstructure.cpp
class BuildStructure;
class Work;               // Work/Work.h
class GatherRawMaterials; // Work/GatherRawMaterials.h
extern "C" BuildStructure* BuildStructure_ConstructForSite(ComponentEntity** buildEntityIBP);  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol
extern "C" int BuildStructure_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol
extern "C" bool BuildStructure_DepsReady();  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol

// ---- foresterwork.cpp
bool ForesterWork_ExecuteReady();  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol
extern "C" int ForesterWork_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol

// ---- gatherrawmaterials.cpp
extern "C" GatherRawMaterials* GatherRawMaterials_ConstructForSite(ComponentEntity* const* buildEntityIBP);  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol
extern "C" int GatherRawMaterials_ForceDrive(Work* self, CitizenComponent** citizenIBP);  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol
extern "C" bool GatherRawMaterials_DepsReady();  // no retail address (standalone harness entry)  // B8: harness / port-only -- standalone driver entry, no retail symbol

// ---- gatherrawmaterials_ctor.cpp
class Work;   // Work/Work.h

// ---- harvestorchard_ctor.cpp

// ---- pasturework.cpp
extern "C" Work* PastureWork_CreateForEntity(ComponentEntity* const* entityRefSlot);  // no retail address (local extern in pasturecomponent_states.cpp)  // B8: harness / port-only -- a constructor door with no retail symbol (pasturecomponent_states.cpp's work post)

// ---- plantorchard_ctor.cpp
class ComponentDescription;

// ---- tendarea_ctor.cpp

// ---- work_act_clear.cpp
class NaturalResourceComponent;   // Component/NaturalResourceComponent.h
void WorkAct_HarvestAreaCtor_c1530(CitizenComponent* const* citizenRef, uint32_t param_2, int param_3, NaturalResourceComponent* const* targets, float* param_5, uint32_t param_6, uint32_t);  // 0x1400c1530  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_act_consume.cpp
void WorkAct_ConsumeProduceCtor_bef70(CitizenComponent* const* citizenRef, uint32_t param_2, ComponentEntity* const* entityRef, float* param_4, float param_5, ComponentDescription* param_6, ComponentDescription* rawMatDesc, uint32_t param_8, int* param_9, uint32_t param_10, uint32_t param_11, float param_12, uint32_t param_13);  // 0x1400bef70  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_act_hunt.cpp
class HuntComponent;
void WorkAct_HuntWorkCtor_bbbc0(CitizenComponent* const* citizenIBP, uint32_t workId, HuntComponent* const* huntRef, float* worldPos, float performWorkTime, char selectMoveTo);  // 0x1400bbbc0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
class ComponentDescription;
void WorkAct_ShowToolCtor3_bdd20(CitizenComponent** citizenRef, uint32_t workId, ComponentDescription* plantType, uint32_t, float* destination, int loopCount, const int32_t* cells, uint32_t completeParam8, ComponentEntity** messageTarget, uint32_t* msgIdPtr10, uint64_t, const uint32_t* messageArgs);  // 0x1400bdd20  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_act_path.cpp
class Action;
namespace Object { template <class T> class InternalPtr; }
extern "C" ::Object::InternalPtr<Action>* WorkAct_GatherRawMatCtor_bf790(::Object::InternalPtr<Action>* ret, CitizenComponent* const* citizen, uint32_t pathType, ComponentEntity* const* src, float* srcPos, ComponentEntity* const* dst, float* dstPos, ComponentDescription* material, int matCount, int completionCode, uint8_t useAnimal, char twoWay, int flags, uint8_t param_14);  // 0x1400bf790  // B8: free in retail -- action factory (builds the Action and pushes it; first parameter its out slot), not an object

// ---- work_act_perform.cpp
void WorkAct_PerformWorkCopyCtor_ba9f0(CitizenComponent* const* citizenRef, uint32_t workId, float* pos3, float* pos4, float* pos5, ComponentEntity* const* srcRef6);  // 0x1400ba9f0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
void WorkAct_PerformWorkCtor_bffd0(CitizenComponent* const* citizenRef, uint32_t workId, float* destination, int roadP4, uint32_t completeRoadP5, uint8_t roadByte6, uint32_t completeTaskP7, ComponentDescription* material, int loopCount9, float* pos10, ComponentEntity* const* srcRef11);  // 0x1400bffd0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_act_rawmat.cpp
void WorkAct_GatherWorkCtor_bb7b0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* workRef, float* worldPos, ComponentDescription* rawMatDesc, char flag6, float rawMatP3, uint32_t rawMatP8, uint32_t param9);  // 0x1400bb7b0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
void WorkAct_FishWorkCtor_beae0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* workRef, float* position, float param5_heading, ComponentDescription* rawMatDesc, float rawMatP3, uint32_t param8, uint32_t param9);  // 0x1400beae0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_base_accessor.cpp
class RawMaterialDescription;

// ---- work_base_actionctor.cpp
class NaturalResourceComponent;
void WorkAct_PerformWorkCtor_c1eb0(CitizenComponent* const* citizenRef, uint32_t workId, NaturalResourceComponent* const* targetRef, float* worldPos, int, uint8_t zeroByte);  // 0x1400c1eb0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
class PickupRawMaterial;
void WorkAct_PerformWorkCtor9_c0ad0(CitizenComponent* const* citizenRef, uint32_t workId, ComponentEntity* const* targetIBP, float* worldPos, int cellIdx, uint8_t cancelFirst);  // 0x1400c0ad0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
void WorkAct_PlayAnimationCtor_c2490(CitizenComponent* const* citizenRef, uint32_t priority, uint32_t citizenIndex);  // 0x1400c2490  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
void WorkAct_GrazingWorkCtor_bb0a0(CitizenComponent* const* citizenRef, uint32_t priority, ComponentEntity* const* workEntityIBP, uint32_t, float* targetPos, float* srcPos, uint32_t typeId, uint32_t flag8, uint32_t cellIdx9);  // 0x1400bb0a0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object
void WorkAct_PlantWorkCtor_be6e0(CitizenComponent* const* citizenRef, uint32_t priority, ComponentDescription* plantDesc, uint32_t, float* position, uint32_t createParam6, uint32_t completeParam7, ComponentEntity* const* parentRef, uint32_t* msgIdPtr9, uint64_t, uint32_t smParam11);  // 0x1400be6e0  // B8: free in retail -- action factory: RCX is a stack InternalPtr slot (verified for the family on 0x1400bffd0: 0x1400e43c3 LEA RCX,[RBP-0x1]), not an object

// ---- work_base_bst.cpp
// 0x140021a30 WorkBST_RegisterWork_1a30 (with 0x140021b40, its leaf allocation) and 0x1400296b0
// WorkBST_BST_Serialize_296b0: templates over the node type, Work/KeyBst.h (B9), as are
// 0x14001a6e0 WorkBST_BSTRemove_a6e0 / 0x140025520 WorkBST_BSTFindRemove_25520 (B8 D5).
#include "Work/KeyBst.h"

// ---- work_base_cellbst.cpp
void* WorkCellBST_Insert_9a40(uintptr_t* root, void* outResult, void* keyRec);  // 0x140029a40  // B9: stays free -- measured with the 8-byte family (5738a31c7: typed out slots SAME, the template done): the one body 0x140029a40 serves element types that name their key / value differently -- WorkCellAssignment {cell, citizen} (ClearArea / PlantArea / PlantOrchard), HarvestArea / HarvestOrchard::Claim {slot, citizen}, and the scheduler's raw WorkSchedulerIdNode {key, work} trees fed anonymous {id, pad, work} records (citizenai_work_scheduler.cpp); a template over the node type needs one key / value spelling, i.e. a per-element accessor (a helper) or one element type, which the tree lacks
// 0x14001ca60 WorkCellBST_Remove_ca60: the template over the node type, Work/CellBst.h (B7 LX).
#include "Work/CellBst.h"
namespace IO { class Serializer; }
void WorkCellBST_Serialize_9de0(uintptr_t* root, IO::Serializer* ser);  // 0x140029de0  // B9: stays free -- its load path inserts through 0x140029a40 (stays free, above: the element types' key / value names differ), its save path reads WorkCellAssignment's {cell, citizen} for every set it is handed
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
class WorkScheduler;   // Game/WorkScheduler.h

// ---- work_base_impl2.cpp
float WorkImpl_Sqrtf_132010(float param_1);  // 0x140132010  // B8: free in retail -- the CRT sqrtf (0x140132010): the float in XMM0, no object

// ---- work_base_reimpl.cpp
// retail 0x140014c30 / 0x14000e320 -- the recursive post-order node free of the exe's Set /
// Map instances: MSVC folded one body per link layout (left +0x10 / right +0x18, and +0x18 /
// +0x20) over every node type (ICF). One template over (header, node): RCX the header (its
// root slot, forwarded unread), RDX the node; each caller instantiates it on its own node
// type, whose `left` / `right` give the offsets. [[gnu::noipa]]: retail's body is out of
// line and recursive (0x140014c49 / 0x140014c55 CALL itself, MemFree after); without it GCC
// unrolls the recursion into the callers (inline) or drops the unread header argument
// (.isra clone, noinline alone), and the instantiation would not be retail's 3-call body.
#include "system/MemoryAllocator.h"
template <class Root, class Node>
[[gnu::noipa]] void WorkBase_TreeNodeFree(Root* root, Node* node) {
    if (node) {
        WorkBase_TreeNodeFree(root, node->left);
        WorkBase_TreeNodeFree(root, node->right);
        System::MemoryAllocator::MemFree(node);
    }
}
struct ActionStack;

// ---- work_base_walkpos.cpp
#include "Component/InteractDescription.h"   // InteractDescription::NodeArray
class InteractComponent;

// ---- work_leaf_clear.cpp
class CropFieldComponent;
#include "Work/Work.h"   // WorkEntityAssignment, WorkEntitySetNode (the 0x140029920 out slot)
void WorkLeaf_BstRemoveByIBP_298a0(Collection::Set<WorkEntityAssignment>* set, ComponentEntity* const* key);  // 0x1400298a0  // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased
WorkEntitySetNode** WorkLeaf_BstInsertByIBP_29920(Collection::Set<WorkEntityAssignment>* set, WorkEntitySetNode** outNode, const WorkEntityAssignment* ins);  // 0x140029920  // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased
void WorkLeaf_BstSerializeIBPpair_29c10(Collection::Set<WorkEntityAssignment>* set, IO::Serializer* ser);  // 0x140029c10  // B8: stays free -- already typed on its one instantiation (Collection::Set<WorkEntityAssignment>, ClearArea::_entities); nothing erased
struct GridTile;   // Work/GridTile.h
template <class T> struct RawArray;   // Action/PathSearch.h

// ---- work_leaf_gather.cpp
class Component;
class GrowthDescription;

// ---- work_leaf_hunt.cpp
class ConsumeProduceComponent;
class PastureComponent;
class ResourceProduceComponent;

// ---- work_leaf_market.cpp (WorkLeaf_MarketTreeCleanup_2af40: Work/MarketWork.h)
