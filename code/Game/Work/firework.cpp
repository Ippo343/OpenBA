// code/Game/Work/firework.cpp
// Clean reimplementation of FireWork::Slot4 and FireWork::Slot9.
//
// FireWork (0x60 bytes) -- fire suppression work action.
// Extends Work (0x48 base) which extends Object::Internal.
// Vtable: 0x1401612a0
//
// Fields:
//   +0x48: int   _field48   -- max fire workers capacity
//   +0x50: void* _field50   -- BST root (tracks entity -> citizen assignments)
//   +0x58: int   _field58   -- number of currently assigned fire workers
//
// Slot4 (0x1400e9620): Fire suppression work assignment.
//   Finds a burning building, checks pathfinding, sets up citizen action queue.
//   Returns: 0 = no fire found, 1 = at capacity, 2 = work assigned.
//
// Slot9 (0x1400e93c0): BST validation pass.
//   Traverses BST at +0x50, calls validation on each entity. Removes
//   entries where the fire has been extinguished.
//   Returns: 1 if BST is empty after cleanup, 0 otherwise.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/FireWork.h"
#include "Work/firework_funcs.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"   // B3R: the Work bodies called by name
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/CitizenDescription.h"   // ENDGAME B.3.9 D5: CitizenDescription::TypeId()
#include "Component/InteractDescription.h"  // ENDGAME B.3.9 D5: InteractDescription::TypeId()
#include "Component/InteractComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Entity/ComponentEntity.h"
#include "Entity/CitizenBehavior.h"
#include "Game/ProfessionList.h"
#include <cstdint>
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <new>

// ============================================================
// Sub-functions, called by name (B3R; Work/exe_bodies.h)
// ============================================================
//   0x1400881d0 FireWork_helper_81d0 spiral fire search  FireWork_SpiralSearch_81d0
//               (returns 0/1 in the low byte; the caller tests the byte)
//   0x140122be0 walkable-position check                   WorkImpl_WalkablePos_122be0
//   0x140122b00 recursive walkable-position check         WorkImpl_WalkablePosRecursive_122b00
//   0x1400f6ef0 BuildStructure_impl_6ef0 validate         WorkImpl_BuildStructureStep_6ef0
//               (a uint64 whose LOW BYTE is the bool; the upper bytes are garbage)
//   0x1400ba9f0 PerformWork_Action_CopyCtor (fire queue)  WorkAct_PerformWorkCopyCtor_ba9f0
//   0x140021a30 CitizenAI_impl_1a30 BST insert            WorkBST_RegisterWork_1a30
//   0x1400f7100 validate the fire entitys CompleteTask   WorkBase_ValidateTask_f7100
//   0x14001a6e0 CitizenAI_impl_a6e0 BST node remove       WorkBST_BSTRemove_a6e0
//   0x140025520 CitizenAI_impl_5520 BST remove-by-key     WorkBST_BSTFindRemove_25520
//   0x1400c5400 remove pending actions                    WorkBase_TaskListCleanup_c5400

// ============================================================
// Helper: Convert world position to clamped grid cell index.
// ============================================================
static uint32_t WorldToCell(int gridSize, float x, float z) {
    int half = gridSize >> 1;
    int cx = (int)(x + (float)half);
    int cz = (int)(z + (float)half);
    if (cx < 0) cx = 0;
    if (cx >= gridSize) cx = gridSize - 1;
    if (cz < 0) cz = 0;
    if (cz >= gridSize) cz = gridSize - 1;
    return (uint32_t)(gridSize * cz + cx);
}

// ============================================================
// Slot4 (0x1400e9620): Fire suppression work assignment.
//
// Algorithm:
// 1. Check if at capacity (_field48 >= _field58)
// 2. Get citizen position, compute grid cell
// 3. Call FireWork_helper_81d0 to find a burning entity in spiral
// 4. Check pathfinding accessibility (FUN_140122be0/FUN_140122b00)
// 5. Validate assignment (BuildStructure_impl_6ef0)
// 6. Set up action queue (PerformWork_Action_CopyCtor)
// 7. Register work with citizen AI (CitizenAI_impl_1a30)
// Returns: 0 = no fire, 1 = at capacity, 2 = work assigned.
// ============================================================
int FireWork::Execute(CitizenComponent** citizenIBP, System::Time* time) {
    // CheckPtr on the entity IBP at +0x10; its InteractComponent
    InteractComponent* posCompIBP = field_10->FindComponent<InteractComponent>();

    // Check capacity: _field48 (max capacity) vs _field58 (assigned count)
    // Original: if ((int)param_1[9] <= (int)param_1[0xb]) return 1;
    // i.e. if max_capacity <= assigned_count, we're at capacity.
    int32_t maxCap   = _field48;
    int32_t assigned = _field58;
    if (maxCap <= assigned) {
        return 1;
    }

    // Get map data (GameUpdateLoop m_citizenList / m_game are still void*: LEFT)
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    CitizenBehavior* townMgr = (CitizenBehavior*)gameWorld->m_citizenList;
    int* mapGrid = (int*)gameWorld->m_game;

    // Citizen -> CharacterComponent (+0x278) -> ModelComponent (+0x68) -> world pos
    // (the transform is raw storage, H5: the float reads are LEFT)
    uintptr_t posData = reinterpret_cast<uintptr_t>((*citizenIBP)->character->modelComponent);

    // Original reads posData+0x160 (citizenY) but only uses it for dead-store
    // to local_60[2] which is zeroed before FindFire — skip it.
    int gridSize = *mapGrid;
    uint32_t centerCell = WorldToCell(gridSize,
        *(float*)(posData + 0x158), *(float*)(posData + 0x15c));

    // The citizen's description (+0x18), type-checked
    void* citizenObj = (*citizenIBP)->_description;
    // DWORD_1401c7190 = CitizenDescription (ENDGAME B.3.9 D5: by cell, not by the
    // legacy variable name "CitizenComponent").
    bool isCitizen = Object::IsDerivedFromNullSafe(citizenObj, CitizenDescription::TypeId());
    // Component::_description is void*: LEFT for Component.h's owner
    CitizenDescription* citizenComp = isCitizen ? reinterpret_cast<CitizenDescription*>(citizenObj) : nullptr;

    // Initialize position buffers (all zeroed — original zeros local_60 before FindFire)
    float srcPos[3] = {0.0f, 0.0f, 0.0f};
    float destPos[3] = {0.0f, 0.0f, 0.0f};

    // Search for a burning entity (spiral search from citizen position)
    int searchRadius = citizenComp->_if8;
    bool found = static_cast<uint8_t>(townMgr->FindWater(centerCell, searchRadius, srcPos, destPos)) != 0;

    // Outcome of the TownManager water search (0x1400881d0). This is the ONLY place
    // that walks townMgr+0x140, so it is also the only place that proves the coarse
    // cell index was sized at world-init (gameupdateloop_load.cpp Step 31). Before that
    // resize landed, this call faulted at coarseCell*0x10 instead of returning.
    // found=0 is NOT a failure by itself: with no water source registered the buckets
    // are legitimately empty and retail returns 0 too.
    // A first-N budget cannot measure a soak: it burns out inside the first fire and then
    // reports nothing for the next hour. Print the first few calls in full, then a running
    // CENSUS of both outcomes -- a run that starts finding water and later stops is
    // invisible to a budget and obvious in a census.
    {
        static int s_fwCalls = 0, s_fwFound = 0, s_fwMissed = 0;
        ++s_fwCalls;
        if (found) ++s_fwFound; else ++s_fwMissed;
        if (s_fwCalls <= 4 || (s_fwCalls % 250) == 0) {
            fprintf(stderr, "[FireWork] water search #%d: townMgr=%p buckets=%p "
                    "centerCell=%u radius=%d -> found=%d  (census: found=%d missed=%d)\n",
                    s_fwCalls, (void*)townMgr, (void*)townMgr->field_0x140, centerCell,
                    searchRadius, (int)found, s_fwFound, s_fwMissed);
            fflush(stderr);
        }
    }

    if (!found) {
        return 0;
    }

    // Check pathfinding accessibility
    void* posComp = posCompIBP->_description;
    // DAT_1401c7938 = InteractDescription (ENDGAME B.3.9 D5: by cell, not by the
    // legacy variable name "PositionComponent").
    bool isPositionComp = Object::IsDerivedFromNullSafe(posComp, InteractDescription::TypeId());
    void* posCompTyped = isPositionComp ? posComp : nullptr;

    int bestDist = 0x7fffffff;
    float bestPos[3] = {0.0f, 0.0f, 0.0f};

    // Check main entity path
    bool pathFound = WorkImpl_WalkablePos_122be0(
        reinterpret_cast<uintptr_t>(posCompIBP), (uintptr_t)posCompTyped + 0x28,
        srcPos, bestPos, &bestDist);

    // Check child entities (InteractComponent keeps its +0x28 array raw: LEFT)
    uintptr_t posCompVal = reinterpret_cast<uintptr_t>(posCompIBP);
    if (*(int32_t*)(posCompVal + 0x30) > 0) {
        uintptr_t childOffset = 0;
        for (int i = 0; i < *(int32_t*)(posCompVal + 0x30); i++) {
            uintptr_t* childIBP = (uintptr_t*)(*(uintptr_t*)(posCompVal + 0x28) + childOffset);
            bool childResult = WorkImpl_WalkablePosRecursive_122b00(
                *childIBP, 0, srcPos, bestPos, &bestDist);
            pathFound = pathFound || childResult;
            childOffset += 8;
        }
    }

    if (!pathFound) {
        return 0;
    }

    // Convert best position to grid cell
    gridSize = *mapGrid;
    int cx = (int)(bestPos[0] + (float)(gridSize >> 1));
    int cz = (int)(bestPos[1] + (float)(gridSize >> 1));
    if (cx < 0) cx = 0;
    if (cx >= gridSize) cx = gridSize - 1;
    if (cz < 0) cz = 0;
    if (cz >= gridSize) cz = gridSize - 1;
    uint32_t destCell = (uint32_t)(gridSize * cz + cx);

    // Validate work assignment (checks timing, distance, etc.)
    CitizenComponent* citizenIBPVal = *citizenIBP;
    bool valid = static_cast<uint8_t>(WorkImpl_BuildStructureStep_6ef0(
        reinterpret_cast<uintptr_t>(this), &citizenIBPVal, destCell, centerCell, time)) != 0;

    if (!valid) {
        return 0;
    }

    // Set up fire suppression action queue
    // Original: local_res8 = param_1[2]; local_88 = *(int**)param_2;
    // local_88 is REASSIGNED from mapGrid to citizen entity ptr here.
    ComponentEntity* workEntityIBP = field_10;
    int priority = field_20;
    CitizenComponent* citizenPtr = *citizenIBP;
    WorkAct_PerformWorkCopyCtor_ba9f0(
        &citizenPtr,
        priority,
        srcPos, destPos, bestPos, reinterpret_cast<int64_t*>(&workEntityIBP));

    // Update citizen entity state
    CitizenComponent* citizenVal = *citizenIBP;

    // Get target via vtable slot 10 (+0x50): a profession
    Profession* targetIBP = nullptr;
    Profession* targetVal = *GetTarget(&targetIBP);

    // Set citizen flags
    citizenVal->statusFlags |= 4;
    citizenVal->field_0x30 = priority;
    citizenVal->field_0xB0 = 0;

    if (targetVal == citizenVal->profession && priority >= 0) {
        citizenVal->happinessFlags |= 0x20000;
    }

    // Register with citizen AI BST at +0x50 (the tree helpers take the root as uintptr_t*: LEFT)
    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(&_field50), &targetIBP, citizenIBP);

    return 2;
}

// ============================================================
// Slot9 (0x1400e93c0): BST validation pass.
//
// Algorithm:
// 1. In-order traverse BST at +0x50
// 2. For each node, call FUN_1400f7100 to check if entity still
//    needs fire suppression
// 3. Collect invalid entities in a dynamic array
// 4. Remove all invalid entries from BST using CitizenAI_impl_a6e0
// 5. Free the dynamic array
// Returns: 1 if BST is now empty, 0 otherwise.
// ============================================================
int FireWork::Validate() {
    // BST root is at +0x50
    AssignmentNode* root = _field50;

    // Dynamic array of entities to remove
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        // Empty tree -- nothing to validate
        return 1;
    }

    // In-order traversal of the BST
    // BST node layout:
    //   +0x00: key (uintptr_t, entity ID)
    //   +0x08: parent
    //   +0x10: left child
    //   +0x18: right child
    //   +0x20: balance factor (short)
    //   +0x22: height (short)

    // Find leftmost node (minimum)
    AssignmentNode* node = root;
    while (node->left != nullptr) {
        node = node->left;
    }

    // Traverse in-order
    while (node != nullptr) {
        // Read the citizen IBP from the node key
        uintptr_t entityIBP = (uintptr_t)node->element;

        // Check if this citizen still has the fire work's task
        bool stillValid = WorkBase_ValidateTask_f7100(this, nullptr, &entityIBP, nullptr);

        if (!stillValid) {
            // Need to add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    uintptr_t* newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((uint64_t)newCap * 8, 8);

                    // Copy existing entries
                    uint32_t copyStart = 0;
                    if (removeArr == newArr) {
                        copyStart = removeCount;
                    }
                    for (uint32_t i = copyStart; i < newCount; i++) {
                        if (newArr != nullptr) {
                            uintptr_t src = 0;
                            if (i < removeCount && removeArr != nullptr) {
                                src = removeArr[i];
                            } else {
                                src = entityIBP;
                            }
                            newArr[i] = src;
                        }
                    }

                    // Free old array if different
                    if (removeArr != newArr) {
                        System::MemoryAllocator::MemFree(removeArr);
                    }
                    removeArr = newArr;
                } else {
                    // Capacity sufficient, just append
                    removeArr[removeCount] = entityIBP;
                }
                removeCount = newCount;
            }
        }

        // Move to in-order successor
        AssignmentNode* right = node->right;
        if (right != nullptr) {
            // Go right, then all the way left
            node = right;
            while (node->left != nullptr) {
                node = node->left;
            }
        } else {
            // Go up until we come from a left child
            AssignmentNode* parent = node->parent;
            if (parent != nullptr && parent->left != node) {
                // We came from the right -- keep going up
                while (parent != nullptr && parent->right == node) {
                    node = parent;
                    parent = node->parent;
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove collected invalid entries from BST
    if ((int)removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            // Find the BST node matching this entity key
            AssignmentNode* bstNode = _field50;
            while (bstNode != nullptr) {
                uintptr_t nodeKey = (uintptr_t)bstNode->element;
                if (removeArr[i] < nodeKey) {
                    bstNode = bstNode->left; // left
                } else {
                    if (removeArr[i] <= nodeKey) {
                        // Found -- remove it (the tree helpers take the root as uintptr_t*: LEFT)
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&_field50), bstNode);
                        break;
                    }
                    bstNode = bstNode->right; // right
                }
            }
        }
    }

    // Free the removal array
    System::MemoryAllocator::MemFree(removeArr);

    // Return 1 if BST is now empty, 0 otherwise
    return (removeCount == 0) ? 1 : 0;
}

// ============================================================
// Slot6 (0x1400e9600): OnComplete — remove citizen from BST,
// return false (0).
//
// Calls CitizenAI_impl_5520 on the BST at +0x50 to remove
// the citizen entry identified by param2, then returns 0.
// ============================================================

// CitizenAI_impl_5520 (0x140025520) = WorkBST_BSTFindRemove_25520: BST lookup + remove by key.

int FireWork::OnComplete(CitizenComponent** citizenIBP, ComponentEntity** /*arg2*/, int /*arg3*/) {
    // Remove citizen entry from BST at +0x50
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&_field50), citizenIBP);
    return 0;
}

// ============================================================
// Slot7 (0x1400e95f0): OnCancel — thunk to BST remove by key.
//
// This is a 6-byte thunk: ADD RCX,0x50; JMP CitizenAI_impl_5520
// Adjusts this pointer to point to BST root at +0x50, then
// jumps to the BST remove-by-key function. The effect is
// identical to Slot6 without the SUB RSP / return false wrapper.
// ============================================================
int FireWork::OnCancel(CitizenComponent** citizenIBP, ComponentEntity** /*arg2*/, int /*arg3*/) {
    // Remove citizen entry from BST at +0x50
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&_field50), citizenIBP);
    return 0;
}

// ============================================================
// Slot8 (0x1400e99a0): CancelAll / Cleanup.
//
// Walks the BST at +0x50 in a loop. For each iteration:
//   1. Get leftmost node in the BST
//   2. CheckPtr on the node, read entity from node
//   3. Get StateMachineComponent at entity+0x278
//   4. Call FUN_1400c5400 (remove pending action) on SMC+0x48
// Continues until the BST root is null.
// ============================================================

// FUN_1400c5400 = WorkBase_TaskListCleanup_c5400: remove pending action from state machine.

int FireWork::Cleanup() {
    while (true) {
        AssignmentNode* root = _field50;
        if (root == nullptr) return 0;

        // Find leftmost node in BST
        AssignmentNode* node = root;
        AssignmentNode* leftChild = node->left;
        while (leftChild != nullptr) {
            node = leftChild;
            leftChild = node->left;
        }

        if (node == nullptr) return 0;

        // Double-traverse to find leftmost again (matches decompilation pattern)
        AssignmentNode* node2 = root;
        while (node2 != nullptr) {
            AssignmentNode* left2 = node2->left;
            if (left2 == nullptr) break;
            node2 = left2;
        }

        // CheckPtr on the node, read the citizen; its CharacterComponent's action list (+0x48)
        CitizenComponent* citizen = node2->element;
        WorkBase_TaskListCleanup_c5400(&citizen->character->actions);
    }
}

// ============================================================
// Slot10 (0x1400e9a30): GetTarget.
//
// Returns the profession list's +0x30 profession (GameUpdateLoop::m_professionList).
// Uses hidden return pointer ABI (member function returning
// non-trivial InternalBasePtr): RCX=this, RDX=retbuf.
// ============================================================
Profession** FireWork::GetTarget(Profession** retbuf) {
    *retbuf = GameUpdateLoop::_singleton->m_professionList->field_0x30;
    return retbuf;
}

// ============================================================
// Slot map (the old hand-built table's notes, 2026-08-22): slot 3 Serialize
// (0x1400e9a50), slot 11 (0x14002a220, `+0x58 > 0`) both ported; slot 5 is Work's
// `return 0`; the real producer is the 3-arg ctor 0x1400e9bd0 (firecomponent_ignite.cpp).
// ============================================================
#include "Object/factory_registration.h"

// ============================================================
// Slot3 Serialize (vtable +0x18, 0x1400e9a50) — was Work_SerializeNop.
// ============================================================
//   Work::Serialize(this, stream)                       0x1400f7400
//   Serializer ser(stream, 0)
//   CitizenAI_impl_96b0(this + 0x50, ser)               0x1400296b0  (the BST)
//   Serializer::Serialize(ser, (int*)(this + 0x48))                  (the capacity)
//   ~Serializer(ser)
// Both sub-calls are already ported and registered (reg_work_base.cpp:49,
// reg_work_bst.cpp:19), so this is a plain transcription. ENDGAME B.3.8: the two
// function-local lazy pointer caches are gone — the base call is the qualified
// Work::Persist, the BST body is called by name (B38 D7).
extern void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer);
void FireWork::Persist(IO::Stream& stream) {

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    IO::Serializer ser(stream, 0);
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(&_field50), &ser);   // 0x1400296b0 (work_base_bst.cpp)
    ser.Serialize(_field48, nullptr);
}

// ============================================================
// Slot11 (vtable +0x58, 0x14002a220) — was Work_SlotRet0 (pinned to 0).
// ============================================================
// Retail body, in full: `XOR EAX,EAX ; CMP [RCX+0x58],EAX ; SETG AL ; RET`
// i.e. `return *(int*)(this + 0x58) > 0` — "does this fire currently have any
// firefighters assigned". +0x58 is the assigned count Slot4 compares against the
// +0x48 capacity, NOT the +0x50 field the five FishingWork-family siblings read,
// which is why work_base_standalone.h's Work_Slot11_HasActiveTasks helper is
// explicitly documented as unusable here.
int FireWork::GetPriority() {
    return _field58 > 0 ? 1 : 0;
}

// ============================================================
// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
// ============================================================
FireWork::FireWork() : Work(), _field48(0), _field4C(0), _field50(nullptr), _field58(0) {
}

// 0x1400e9bd0: `MOV dword [RDI+0x48],EBX` -- a DWORD store; +0x4C keeps whatever
// the allocation held (retail's).
FireWork::FireWork(void* pEntityIBP, uint32_t capacity)
    : Work(pEntityIBP), _field48(static_cast<int32_t>(capacity)), _field50(nullptr), _field58(0) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_FireWork_Vtable` (slot 0 Work_GenericScalarDtor,
// slot 1 null, slot 2 a lazily cached id) and REGISTER_TYPE over FireWork_Factory /
// _CreateInPlace, both Work_GenericCtor.

// create 0x14002a1e0: MemAlloc(sizeof, 8), then the ctor.
void* FireWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FireWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FireWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* FireWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FireWork());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(FireWork, "Work")
