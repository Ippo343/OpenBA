// code/Game/Action/action_base_complete.cpp
// Wave 3 (Action) — CompleteTask / ShowTool exe sub-helpers (closure-safe subset).
//
// The CompleteTask / ShowTool action .cpp files call them by name through
// Action/exe_bodies.h (B3R); the registration (RegisterActionCompleteBatch in
// standalone/reg_action_complete.cpp) stays for readers other blocks still have.
//
// Wired (closure-safe): CompleteTask BST helpers 0x1400f8760 / 0x1400f8800 / 0x1400f8e30
// and ShowTool 0x1401173b0 / 0x1401176e0. Each is a 1:1 port of the decompiled exe
// helper (Ghidra); do NOT "improve" behavior.
//
// DEFERRED (NOT in this file): DropOffRawMaterial transfer helpers 0x1400c7570 /
// 0x1400c7620 — their closure pulls in unregistered code sub-callees (the deep
// create-raw-material spawn FUN_1400c78c0, MoveTo action ctor FUN_1400c9170, the
#include "Action/exe_bodies.h"
// task-list append FUN_1400c54e0, and an allocator dispatch slot). Drafted in
// notes/wave3_drafts/action_base_complete.cpp.draft; wire once that closure lands.
//
// B3R: every dispatch-slot and engine_funcs door this file read is a direct call now:
// the IsDerivedFrom door is Object::IsDerivedFromNullSafe, the Time ctor slot
// 0x140152b00 is RtSystem_Time_Ctor, the BST removes are the Work bodies by name.
//

// The runtime type headers come FIRST: they reach runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen (through resolve.h and this file's own
// <windows.h> below).  ENDGAME B.3.5 batch K1.
#include "graphics/Geometry.h"
#include "graphics/Mesh.h"
#include "graphics/GraphicsMesh.h"
#include "graphics/Scene.h"
#include "system/Value.h"       // System::Time, for Geometry::Update's argument
#include "object/External.h"
#include "object/TypeInfo.h"
#include "graphics/Camera.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Work/exe_bodies.h"      // WorkCellBST_Remove_ca60, WorkBST_BSTRemove_a6e0
#include "Game/exe_constants.h"  // kRdata_14017214c
#include "Component/ModelDescription.h"         // ENDGAME B.3.9 CB: 0x1401c7a88 cell class
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 CB: 0x1401c7cb8 cell class
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "scene_watch.h"
#include <windows.h>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Work/Work.h"         // B.3.x W0: Work slots 6/7/8/10 by name
#include "Action/PathAvl.h"      // AvlNode28 (the order tree)
#include "Component/CharacterComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CitizenDescription.h"
#include "Component/ComponentDescription.h"
#include "Component/ModelComponent.h"
#include "Component/StorageComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/WorkScheduler.h"
#include "Game/WorkSchedulerNodes.h"   // WorkSchedulerIdNode / CountNode / BucketNode

// ============================================================
// CompleteTask task-manager BST helpers
// ============================================================
// The work scheduler (GameUpdateLoop::m_tutorial, +0x118) holds an intrusive binary search
// tree of task nodes rooted at trees[0] (+0x30). Node layout (verified in disassembly):
//   +0x00 int   key (task index)
//   +0x08 IBP   the Work (CheckPtr'd before use)
//   +0x18 ptr   left child
//   +0x20 ptr   right child
// Each helper walks the tree to the node whose key == taskIdx, then invokes a Work slot
// with copies of the two ref args.

namespace {
// The id-tree node and the +0x120 bucket-tree node are Game/WorkSchedulerNodes.h's.
using WorkIdNode     = WorkSchedulerIdNode;
using WorkBucketNode = WorkSchedulerBucketNode;

static WorkIdNode* IdRoot(const WorkSchedulerTree& tree) {
    return tree.root;
}

// Walk the id tree; return the node whose key == taskIdx, else null.
static WorkIdNode* FindTaskNode(const WorkScheduler* sched, int taskIdx) {
    WorkIdNode* node = IdRoot(sched->trees[0]);
    while (node != nullptr) {
        int key = node->key;
        if (taskIdx < key) {
            node = node->left;
        } else if (key < taskIdx) {
            node = node->right;
        } else {
            return node;  // exact match
        }
    }
    return nullptr;
}

} // namespace

// Reimpl of CompleteTask_helper_8760 — try-complete a task: walk to the matching node,
// CheckPtr the Work, invoke slot 6 with (entityCopy, targetCopy, param). Returns the low
// byte of the result.
bool TryCompleteTask(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx,
                     ComponentEntity* const* targetRef, uint32_t taskParam) {
    WorkIdNode* node = FindTaskNode(sched, taskIdx);
    if (node == nullptr) return false;

    ComponentEntity* targetCopy = *targetRef;
    Work* owner = node->work.Get();
    CitizenComponent* entityCopy = *entityRef;

    int r = owner->OnComplete(&entityCopy, &targetCopy, static_cast<int>(taskParam));   // slot 6
    return (r & 0xff) != 0;
}

// Reimpl of CompleteTask_helper_8800 — task reassign: same walk, slot 7 (void).
void TaskReassign(const WorkScheduler* sched, CitizenComponent* const* entityRef, int taskIdx,
                  ComponentEntity* const* targetRef, uint32_t taskParam) {
    WorkIdNode* node = FindTaskNode(sched, taskIdx);
    if (node == nullptr) return;

    ComponentEntity* targetCopy = *targetRef;
    Work* owner = node->work.Get();
    CitizenComponent* entityCopy = *entityRef;

    owner->OnCancel(&entityCopy, &targetCopy, static_cast<int>(taskParam));   // slot 7
}

// ============================================================
// CitizenAI_impl_8e30 — task cleanup after completion.
// Walks the id tree (trees[0]) to the matching task node, then:
//  1. For each of the 14 action-list trees (trees[1..14]), find and remove (ca60) the node
//     whose key == taskIdx.
//  2. Find/remove the matching node in the field_0x10 ORDER tree (key compared against the
//     Work's +0x24 field; this tree uses the 0x28-node layout: parent@+0x08,
//     less@+0x10, greater@+0x18 — remove via a6e0). A long-lived offset inversion here
//     (fixed 2026-07-26) meant the order-tree node was never found.
//  3. The Work's slot 10 (GetTarget) gives a key; look it up in the field_0x120 bucket
//     tree and remove the task's entry from that bucket (ca60).
//  4. Work slots 9 then 8, DeletePtr the Work, then remove the task node itself (ca60).
//
// Retail does not tolerate a missing bucket in step 3 (0x1400F8F5E reads address 0x8);
// the tree's `if (node != nullptr)` guard continues instead (kept; WAVE 9 LANE N).
// ============================================================
void TaskCleanup(WorkScheduler* sched, int taskIdx) {
    // 1. Locate the task node in the id tree.
    WorkIdNode* task = IdRoot(sched->trees[0]);
    while (task != nullptr) {
        int key = task->key;
        if (taskIdx < key) {
            task = task->left;
            continue;
        }
        if (key < taskIdx) {
            task = task->right;
            continue;
        }
        break;  // key == taskIdx
    }
    if (task == nullptr) return;

    // 2. Remove the matching node (key == taskIdx) from each of the 14 action-list trees.
    for (int li = 0; li < 14; li++) {
        WorkSchedulerTree* list = &sched->trees[1 + li];
        WorkIdNode* n = IdRoot(*list);
        while (n != nullptr) {
            int k = n->key;
            if (taskIdx < k) {
                n = n->left;
            } else if (k < taskIdx) {
                n = n->right;
            } else {
                WorkCellBST_Remove_ca60(list, n);   // 0x14001ca60
                break;
            }
        }
    }

    // 3. Remove from the order tree (field_0x10) keyed on the Work's +0x24 field.
    {
        // The Work's queue-order key (Work::field_24, +0x24).
        int wantKey = task->work->field_24;
        WorkSchedulerCountNode* n = sched->field_0x10.root;
        while (n != nullptr) {
            int k = n->key;
            // Retail 8e30 step: `want < nodeKey -> node+0x10`, `nodeKey < want ->
            // node+0x18` (0x28-node order tree: less@+0x10, greater@+0x18).
            if (wantKey < k) {
                n = n->left;    // less
            } else if (k < wantKey) {
                n = n->right;   // greater
            } else {
                WorkBST_BSTRemove_a6e0(&sched->field_0x10,
                                       n);   // 0x14001a6e0
                break;
            }
        }
    }

    // 4. Work slot 10 -> key; look it up in the field_0x120 bucket tree and remove the entry.
    {
        Work* owner = task->work.Get();
        Profession* keyTmp = nullptr;
        Profession** keyPtr = owner->GetTarget(&keyTmp);   // slot 10, retslot

        WorkBucketNode* node = sched->field_0x120;
        if (node != nullptr) {
            while (true) {
                Profession* v = node->bucket.key;
                if (*keyPtr < v) {
                    node = node->left;    // +0x20
                } else if (v < *keyPtr) {
                    node = node->right;   // +0x28
                } else {
                    break;
                }
                if (node == nullptr) break;
            }
        }

        if (node != nullptr) {
            // the bucket's entry tree (node+0x8); key compared against taskIdx.
            WorkIdNode* e = IdRoot(node->bucket.entries);
            while (e != nullptr) {
                int k = e->key;
                if (taskIdx < k) {
                    e = e->left;
                } else if (k < taskIdx) {
                    e = e->right;
                } else {
                    WorkCellBST_Remove_ca60(&node->bucket.entries, e);   // 0x14001ca60
                    break;
                }
            }
        }
    }

    // 5. Work slots 9 then 8; DeletePtr the Work; then remove the task node.
    task->work->Validate();   // slot 9, RCX only
    task->work->Cleanup();    // slot 8
    task->work.DeletePtr();
    WorkCellBST_Remove_ca60(&sched->trees[0], task);   // 0x14001ca60
}

// ============================================================
// ShowTool helpers (Entity_impl_73b0 / FUN_1401176e0)
// ============================================================
// These attach/detach a "tool" mesh to a citizen. The citizen's tool MeshInstance is
// CitizenComponent::meshInstance (+0xB8), built by the citizen's ctor (0x140114808).

// Reimpl of FUN_1401176e0 — hide/detach the current tool mesh: if toolSceneId is valid
// (>= 0), detach the tool geometry from the citizen's model geometry and remove the scene
// object, then mark toolId / toolSceneId invalid (one qword store of -1).
void HideCitizenTool(CitizenComponent* citizen) {
    if (citizen->toolSceneId < 0) return;

    ModelComponent* model = citizen->character->modelComponent.Get();
    model->meshInstance.Detach(&citizen->meshInstance);

    if (GameUpdateLoop::_singleton) {
        Graphics::Scene* scene = GameUpdateLoop::_singleton->m_graphicsScene;
        scene->RemoveObject(static_cast<uint32_t>(citizen->toolSceneId));
    }
    citizen->toolId = -1;          // one qword store of -1 in retail (H7)
    citizen->toolSceneId = -1;
}

// Reimpl of Entity_impl_73b0 — show a tool mesh on the citizen.
//   - If a tool is already shown (toolSceneId >= 0), hide it first (FUN_1401176e0).
//   - toolId >= 0: the mesh from the citizen description's _refs table, with g_fUVScale.
//   - toolId < 0: the storage's largest stack: its ModelDescription for the mesh and its
//     RawMaterialDescription for the UV scale. (The crash-prone path the ShowTool call
//     site deliberately guards off; ported faithfully.)
//   - Set the mesh on the tool MeshInstance, write the UV-scale vector (its _color),
//     attach it at the model's "tool" node, then add it to the scene and record the ids.
void ShowCitizenTool(CitizenComponent* citizen, int toolId) {
    ComponentEntity* parent = citizen->_entity.Get();
    if (((parent->_entityFlags >> 10) & 1) != 0) return;  // removed flag

    if (citizen->toolSceneId >= 0) HideCitizenTool(citizen);

    ModelComponent* model = citizen->character->modelComponent.Get();

    void* meshPtr = nullptr;
    float uvScale = 0.0f;

    if (toolId < 0) {
        // find best material from storage on the citizen's entity
        StorageComponent* storage = nullptr;
        fn_FindComponent_Storage(citizen->_entity.Get(), &storage);
        if (storage->storageSlots._size < 1) return;

        // pick the item with the highest count
        ComponentDescription* bestItem = nullptr;
        int bestCount = 0;
        int idx = 0;
        do {
            const StorageComponent::Item& slot = storage->storageSlots[idx];
            int c = slot._count;
            if (bestCount < c) {
                bestItem = slot._resource;
                bestCount = c;
            }
            idx++;
        } while (idx < storage->storageSlots._size);

        if (bestItem == nullptr) return;
        ComponentDescription* item = bestItem;

        // locate the ModelDescription for the mesh (cell 0x1401c7a88)
        const uint32_t tidModelDesc = ModelDescription::TypeId();
        const ModelDescription* fenceComp = nullptr;
        if (item->_subDescCount > 0) {
            int n = item->_subDescCount;
            Description** arr = item->_subDescs;
            for (int i = 0; i < n; i++) {
                Description* comp = arr[i];
                uint32_t t = comp->Type();   // slot 2
                if (t == tidModelDesc) {
                    Description* c = arr[i];
                    if (::Object::IsDerivedFromNullSafe(c, tidModelDesc)) fenceComp = static_cast<const ModelDescription*>(c);
                    break;
                }
            }
        }
        // LEFT: the tree reads the qword at ModelDescription +0x28 (_meshGroupCount /
        // _meshGroupCap) as a pointer and dereferences it; retail 0x1401173b0 reads
        // _meshGroups[0].meshes[0] (one more dereference). Recorded in known_port_issues
        // entry 14; kept as the tree's (no behaviour change here).
        meshPtr = **reinterpret_cast<void* const* const*>(&fenceComp->_meshGroupCount);   // LEFT: known_port_issues entry 14 (the tree's read differs from retail 0x1401173b0)

        // locate the RawMaterialDescription for the UV scale
        const uint32_t tidRawMat = RawMaterialDescription::TypeId();
        const RawMaterialDescription* rawComp = nullptr;
        if (item->_subDescCount > 0) {
            int n = item->_subDescCount;
            Description** arr = item->_subDescs;
            for (int i = 0; i < n; i++) {
                Description* comp = arr[i];
                uint32_t t = comp->Type();   // slot 2
                if (t == tidRawMat) {
                    Description* c = arr[i];
                    if (::Object::IsDerivedFromNullSafe(c, tidRawMat)) rawComp = static_cast<const RawMaterialDescription*>(c);
                    break;
                }
            }
        }
        uvScale = rawComp->_carryScale;
    } else {
        // the mesh table of the citizen description (_refs, +0x138)
        meshPtr = citizen->description->_refs[toolId];
        uvScale = kRdata_14017214c;   // g_fUVScale, 0x14017214c
    }

    // The citizen's ctor built +0xB8 (0x140114808); retail SetMesh's it straight away.
    Graphics::MeshInstance* tool = &citizen->meshInstance;

    tool->SetMesh(Graphics::ExternalPtrHandle{meshPtr});

    // write the UV-scale vector (Vector4 broadcast of uvScale) into the tool's _color
    // (+0x1c4..+0x1d0). Retail 0x1401173b0: Math::Vector4::Vector4(local_58, &scalar) then
    // four dword stores.
    Math::Vector4 vec4((Math::Scalar(uvScale)));
    tool->_color[0] = vec4.x;
    tool->_color[1] = vec4.y;
    tool->_color[2] = vec4.z;
    tool->_color[3] = vec4.w;

    // attach the tool geometry to the model's "tool" node
    // LEFT: MeshInstance::_mesh (Runtime) is an untyped ExternalPtr handle.
    const Graphics::GraphicsMesh* graphicsMesh = model->meshInstance._mesh.Get();
    int nodeIdx = graphicsMesh ? graphicsMesh->GetNodeIndexFromName("tool") : -1;
    model->meshInstance.Attach(tool, nodeIdx);

    // MeshInstance::Update(time=0) via slot 1, unconditionally as retail
    // (0x14011766c Time(0.0f), 0x140117688 CALL [RAX+0x8]).
    {
        System::Time timeVal;
        RtSystem_Time_Ctor(&timeVal, 0.0f);   // slot 0x140152b00
        static_cast<Graphics::Geometry*>(tool)->Update(timeVal);
    }

    // add to scene + record object id
    if (GameUpdateLoop::_singleton) {
        Graphics::Scene* scene = GameUpdateLoop::_singleton->m_graphicsScene;
        uint32_t objId = scene->AddObject(tool);
        citizen->toolId = toolId;
        citizen->toolSceneId = static_cast<int32_t>(objId);
        SceneWatch_Note(objId, tool, "ShowTool 0x1401173b0");
    }
}

// ===========================================================================
// B4 FORWARD: the C spellings other blocks' files still call, one line each.
// ===========================================================================

// (ActCt_TaskCleanup_8e30, the B4 FORWARD to TaskCleanup: its two callers call
// TaskCleanup now, deleted, B4 X4-WORK.)


