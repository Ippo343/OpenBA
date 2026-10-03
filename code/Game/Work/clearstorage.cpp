// code/Game/Work/clearstorage.cpp
// Clean reimplementation of ClearStorage work class methods.
//
// ClearStorage is a Work subclass (0x68 bytes) for clearing items from storage
// buildings. Citizens assigned to this work pick up items from one storage and
// carry them to another available storage location.
//
// Vtable at 0x1401611a0.
//
// Decompiled from Application-x64.exe.

#include "Work/ClearStorage.h"
#include "Work/clearstorage_funcs.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 D5
#include "Component/StorageComponent.h"         // ENDGAME B.3.9 D5
#include "Component/StorageLocationComponent.h"
#include "Component/BuildComponent.h"
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Entity/ComponentEntity.h"
#include "Game/ProfessionList.h"

// Sub-functions, called by name (B3R): 0x140025520 WorkBST_BSTFindRemove_25520,
// 0x1400c5400 WorkBase_TaskListCleanup_c5400, 0x140014c30 WorkBase_TreeNodeFree_14c30.

// ============================================================
// Helper: Clamp int to [0, max-1]
// ============================================================
static inline int ClampGrid(int val, int gridSize) {
    if (val < 0) return 0;
    if (val >= gridSize) return gridSize - 1;
    return val;
}

// ============================================================
// Helper: Compute grid cell index from world position + grid info
// ============================================================
static inline uint32_t WorldToCell(float x, float z, int gridSize) {
    int halfGrid = gridSize >> 1;
    int cellX = ClampGrid((int)(x + (float)halfGrid), gridSize);
    int cellZ = ClampGrid((int)(z + (float)halfGrid), gridSize);
    return (uint32_t)(cellZ * gridSize + cellX);
}

// ============================================================
// ClearStorage::Execute (Slot4, vtable +0x20)
// Original: 0x1400e6c50
// ============================================================
// Iterates storage items on the assigned building. For each item type with
// available quantity, finds a destination storage, validates the path, and
// sets up citizen pathfinding to carry items.
//
// Returns: 0 = failed validation, 1 = no work available (retry), 2 = work assigned
int ClearStorage::Execute(CitizenComponent** citizenAI, System::Time* time) {
    // Find components on the entity (+0x10) through the guarded finders (typed locals)
    BuildComponent* positionComp = nullptr;          // fn_FindComponent_Position: BuildComponent's cell
    fn_FindComponent_Position(field_10, &positionComp);

    StorageLocationComponent* unknownComp = nullptr; // cell 0x1401c7ee8
    fn_FindComponent_Unknown7ee8(field_10, &unknownComp);

    StorageComponent* storage = nullptr;
    fn_FindComponent_Storage(field_10, &storage);

    // Get game world grid data (m_game / m_citizenList are still void*: LEFT)
    GameUpdateLoop* gameWorld = GameUpdateLoop::_singleton;
    int* gridDataPtr = (int*)gameWorld->m_game;
    void* worldArray = gameWorld->m_citizenList;

    // Citizen position: CharacterComponent (+0x278) -> ModelComponent (+0x68); the
    // transform is raw storage (H5): the float reads are LEFT.
    uintptr_t posData = reinterpret_cast<uintptr_t>((*citizenAI)->character->modelComponent);

    int gridSize = *gridDataPtr;
    // citizenY at +0x160 is stored but only used for searchPos[2] init
    (void)*(float*)(posData + 0x160);  // citizenY — unused in grid calc
    float citizenX = *(float*)(posData + 0x158);
    float citizenZ = *(float*)(posData + 0x15c);

    uint32_t srcCell = WorldToCell(citizenX, citizenZ, gridSize);

    // Get storage item count
    int itemCount = storage->slotCount;

    if (itemCount <= 0) {
        return 1;  // No items in storage
    }

    // Iterate over storage items
    for (int i = 0; i < itemCount; i++) {
        // (StorageComponent's items are raw storage, H5: LEFT)
        uintptr_t itemArray = (uintptr_t)storage->storageSlots;
        uintptr_t itemEntry = itemArray + (uintptr_t)i * 0x10;

        // Check available count: stored - reserved > 0
        int stored = *(int*)(itemEntry + 0x8);
        int reserved = *(int*)(itemEntry + 0xc);
        int availCount = stored - reserved;
        if (availCount <= 0) continue;

        // Get the entity pointer from item entry
        uintptr_t itemEntity = *(uintptr_t*)itemEntry;

        // Find RawMaterialDescription component on item entity
        void* rawMatDesc = nullptr;
        if (*(int*)(itemEntity + 0x30) > 0) {
            for (int j = 0; j < *(int*)(itemEntity + 0x30); j++) {
                uintptr_t compArray = *(uintptr_t*)(itemEntity + 0x28);
                void* comp = *(void**)(compArray + (uintptr_t)j * 8);
                // slot 2 Type() of the description's component description
                uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
                if (typeId == RawMaterialDescription::TypeId()) {
                    // Verify via IsDerivedFrom
                    comp = *(void**)(*(uintptr_t*)(itemEntity + 0x28) + (uintptr_t)j * 8);
                    if (Object::IsDerivedFromNullSafe(comp, RawMaterialDescription::TypeId())) {
                        rawMatDesc = comp;
                    }
                    break;
                }
            }
        }

        // Find StorageComponent on the citizen's entity (+0x10): the FindComponent walk
        StorageComponent* destStorage = (*citizenAI)->_entity->FindComponent<StorageComponent>();

        // Check capacity: destStorage capacity - count vs rawMat resourceSize * availCount
        int maxCapacity = destStorage->capacity;

        int currentCount = fn_StorageComponent_QueryCount(destStorage);

        int freeSpace = maxCapacity - currentCount;
        int resourceSize = rawMatDesc ? *(int*)((uintptr_t)rawMatDesc + 0x48) : 1;

        if (freeSpace < resourceSize * availCount) {
            // Not enough room in dest, check citizen's building storage
            StorageComponent* altStorage = nullptr;
            fn_FindComponent_Storage((*citizenAI)->_entity, &altStorage);

            int altMax = altStorage->capacity;

            int altCurrent = fn_StorageComponent_QueryCount(altStorage);

            availCount = (altMax - altCurrent) / resourceSize;
        }

        if (availCount == 0) {
            // No capacity available, transition to long-term plan
            CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*citizenAI));
            return 2;
        }

        // Set up search for destination storage
        ComponentEntity* entityIBP = field_10;

        float searchPos[3] = {0.0f, 0.0f, 0.0f};
        uint32_t resourceTypeId = (uint32_t)field_28;
        void* destEntity = nullptr;

        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(worldArray), reinterpret_cast<int64_t*>(&destEntity), resourceTypeId,
                                  reinterpret_cast<uint64_t>((void*)itemEntity), availCount, 1,
                                  reinterpret_cast<uint32_t*>(searchPos), &entityIBP);

        if (destEntity == nullptr) {
            continue;
        }

        // Check if path is possible
        uint32_t workTypeId = (uint32_t)field_28;
        if (!WorkBase_ValidateCell_a00f0(gridDataPtr, 0xe1f, srcCell, workTypeId)) {
            return 1;
        }

        // Determine destination position and cell
        float destPos[3] = {0.0f, 0.0f, 0.0f};
        uint32_t destCell = 0;

        int dbgBranch = -1, dbgSlot = 0;   // trace: 0=position, 1=findslot
        if (unknownComp == nullptr) {
            // No storage-location component — use the position component
            if (positionComp == nullptr) {
                return 1;
            }
            dbgBranch = 0;
            Pf_EntityResolvePos_2e20(positionComp, destPos, 0, 0, nullptr);
            destCell = WorldToCell(destPos[0], destPos[1], gridSize);
        } else {
            // Use search position
            dbgBranch = 1;
            destCell = WorldToCell(searchPos[0], searchPos[1], gridSize);

            int slotIdx = Pf_FindBestAdjacentCell_d5d0(reinterpret_cast<int64_t>(unknownComp), destCell, destPos, workTypeId);
            dbgSlot = slotIdx;
            if (slotIdx < 0) {
                return 1;
            }
        }

        // Validate the work assignment
        CitizenComponent* citizenVal = *citizenAI;

        if (!WorkImpl_BuildStructureStep_6ef0(reinterpret_cast<uintptr_t>(this), citizenAI, destCell, srcCell, time)) {
            return 0;
        }

        // Set up pathfinding
        ComponentEntity* workEntity = field_10;
        int pathType = field_20;
        void* destEntityCopy = destEntity;
        uint32_t pathFlags = (availCount << 8) | 0;  // flags from local_res20[0]

        uint32_t pathResult = 0;
        WorkAct_GatherRawMatCtor_bf790(&pathResult, &citizenVal, pathType, reinterpret_cast<uint64_t*>(&destEntityCopy),
                                searchPos, reinterpret_cast<void**>(&workEntity), destPos,
                                (int64_t)itemEntity, availCount, pathFlags, 0, 1, 0, 0);

        // Get the Slot10 result (vtable +0x50)
        CitizenComponent* citizenPtr = *citizenAI;

        Profession* entityRefOut = nullptr;
        Profession* resultVal = *GetTarget(&entityRefOut);

        // Update citizen state
        citizenPtr->statusFlags |= 4;
        citizenPtr->field_0x30 = pathType;
        citizenPtr->field_0xB0 = 0;

        if (resultVal == citizenPtr->profession && pathType >= 0) {
            citizenPtr->happinessFlags |= 0x20000;
        }

        // Register work with citizen AI (the tree helpers take the root as uintptr_t*: LEFT)
        WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>(&_field58), &entityRefOut, citizenAI);
        return 2;
    }

    return 1;  // No valid items found
}

// ============================================================
// ClearStorage::Validate (Slot9, vtable +0x48)
// Original: 0x1400e69f0
// ============================================================
// Traverses the BST at self+0x58, checking each stored task via
// Work::CanStartWork. Invalid tasks are collected into a temporary
// array and then removed from the BST.
//
// Returns: true (1) if no entries remain (all cleared), false (0) otherwise.
//
// NOTE: Hook is DISABLED due to bug class 3 (MOV RAX,RSP in prologue).
// This clean implementation exists for documentation and future standalone exe.
int ClearStorage::Validate() {

    // Phase 1: In-order traverse BST, collect entries that fail CanStartWork
    AssignmentNode** removeList = nullptr;
    int removeCount = 0;
    int removeCapacity = 0;

    // Find leftmost node (smallest key)
    AssignmentNode* node = _field58;
    if (node == nullptr) goto phase2;

    // Go to leftmost
    while (node->left != nullptr) {
        node = node->left;
    }

    // In-order traversal
    while (node != nullptr) {
        // Get task key from node
        CitizenComponent* taskIBP = node->element;

        // Check if task is still valid
        bool canStart = WorkBase_ValidateTask_f7100(this, nullptr, &taskIBP, nullptr);

        if (!canStart) {
            // Need to add this node to remove list
            int newCount = removeCount + 1;
            if (newCount > removeCapacity) {
                // Grow capacity
                int newCap = newCount;
                if (removeCount != 0 && newCount == removeCount + 1) {
                    newCap = newCount * 3;
                }
                removeCapacity = newCap;
                AssignmentNode** newList = (AssignmentNode**)System::MemoryAllocator::MemAlloc((uint64_t)newCap * 8, 8);

                // Copy existing entries
                int copyStart = 0;
                if (removeList == newList) {
                    copyStart = removeCount;
                }
                for (int j = copyStart; j < newCount; j++) {
                    if (newList != nullptr) {
                        AssignmentNode* src = (j < removeCount && removeList != nullptr)
                            ? removeList[j]
                            : node;  // Current node for the new entry
                        newList[j] = src;
                    }
                }

                if (removeList != newList && removeList != nullptr) {
                    System::MemoryAllocator::MemFree(removeList);
                }
                removeList = newList;
            } else {
                // Enough capacity, just store
                if (removeList != nullptr) {
                    removeList[removeCount] = node;
                }
            }
            removeCount = newCount;
        }

        // Move to next in-order node
        AssignmentNode* right = node->right;
        if (right != nullptr) {
            // Go to leftmost of right subtree
            node = right;
            while (node->left != nullptr) {
                node = node->left;
            }
        } else {
            // Go up until we come from a left child
            AssignmentNode* parent = node->parent;
            if (parent != nullptr && parent->left == node) {
                // We are left child of parent, parent is next
                node = parent;
            } else {
                // We are right child, go up while we are right child
                while (parent != nullptr && parent->right == node) {
                    node = parent;
                    parent = node->parent;
                }
                node = parent;
            }
        }
    }

phase2:
    // Phase 2: Remove collected entries from BST
    if (removeCount > 0) {
        for (int i = 0; i < removeCount; i++) {
            // Find node in BST by key
            AssignmentNode* searchNode = _field58;
            CitizenComponent* key = removeList[i]->element;

            while (searchNode != nullptr) {
                CitizenComponent* nodeKey = searchNode->element;
                if (key < nodeKey) {
                    searchNode = searchNode->left;
                } else {
                    if (key <= nodeKey) {
                        // Found it, remove (the tree helpers take the root as uintptr_t*: LEFT)
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(&_field58), searchNode);
                        break;
                    }
                    searchNode = searchNode->right;
                }
            }
        }
    }

    // Free remove list
    System::MemoryAllocator::MemFree(removeList);

    return removeCount == 0 ? 1 : 0;
}

// 0x1400e73b0. The stamps (ClearStorage, then Work) and `~Internal` are the compiler's.
ClearStorage::~ClearStorage() {

    // Free BST at +0x58 (citizen assignment tree)
    AssignmentNode* bstRoot = _field58;
    if (bstRoot != nullptr) {
        WorkBase_TreeNodeFree_14c30(&_field58, bstRoot->left);
        WorkBase_TreeNodeFree_14c30(&_field58, bstRoot->right);
        System::MemoryAllocator::MemFree(bstRoot);
    }
    _field58 = nullptr;
    _field60 = 0;

    // Free auxiliary data at +0x48
    System::MemoryAllocator::MemFree(_field48);
    _field48 = nullptr;
    _field50 = nullptr;
}

// ============================================================
// ClearStorage::Slot6 (vtable +0x30, 0x1400e6c30)
// ============================================================
// BST find-and-remove by key on the citizen BST at self+0x58,
// then returns false (0). This removes a specific citizen's
// entry from the work tracking tree.
int ClearStorage::OnComplete(CitizenComponent** param2, ComponentEntity** /*arg2*/, int /*arg3*/) {

    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&_field58), param2);
    return 0;
}

// ============================================================
// ClearStorage::Slot7 (vtable +0x38, 0x1400e6c20) -- task CANCELLED
// ============================================================
// Retail is the 6-byte thunk `ADD RCX,0x58; JMP CitizenAI_impl_5520`: the same
// remove-by-citizen as slot 6, without the return-0 wrapper.
//
// ⚠ This slot was pinned to Work_SlotRet0 ("NOT IMPLEMENTED"), so a cancelled
// clear-storage task left its citizen node in the +0x58 tree forever. That is the
// exact stale-entry family HarvestArea::Slot7 had (harvestarea.cpp): CancelAll below
// spins on the leftmost node until the count drops, and Validate hands the stale
// citizen pointer to Object::IsA, which CALLs through it. Ported 2026-09-05.
int ClearStorage::OnCancel(CitizenComponent** param2, ComponentEntity** /*arg2*/, int /*arg3*/) {

    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>(&_field58), param2);
    return 0;
}

// ============================================================
// ClearStorage::Slot11 (vtable +0x58, 0x14002a0f0) -- HasActiveTasks
// ============================================================
// Retail: `return 0 < *(int*)(self + 0x60)`. ClearStorage keeps its citizen tree at
// +0x58 with the count at +0x60 (one qword later than the +0x48/+0x50 layout the
// shared Work_Slot11_HasActiveTasks reads), so it needs its own body. Was pinned to
// Work_SlotRet0, i.e. permanently "nobody is working this".
int ClearStorage::GetPriority() {
    return (int)_field60 > 0 ? 1 : 0;   // retail's signed compare of the dword
}

// ============================================================
// ClearStorage::CancelAll (Slot8, vtable +0x40, 0x1400e72a0)
// ============================================================
// Iterates the BST at self+0x58 by finding the leftmost node
// each iteration. For each node, resolves the citizen entity,
// navigates to its state machine component (+0x278), and cancels
// the pending work action via TaskCancel on its action list (+0x48).
// Repeats until the BST is empty (count at self+0x60 drops to 0).
int ClearStorage::Cleanup() {

    while (true) {
        AssignmentNode* node = _field58;
        if (node == nullptr) {
            return 0;
        }

        // Find leftmost node in BST
        AssignmentNode* leftmost = node;
        AssignmentNode* left = leftmost->left;
        while (left != nullptr) {
            leftmost = left;
            left = leftmost->left;
        }

        if (leftmost == nullptr) {
            return 0;
        }

        // The node's citizen; cancel its CharacterComponent's action list (+0x48)
        CitizenComponent* citizen = leftmost->element;
        WorkBase_TaskListCleanup_c5400(&citizen->character->actions);
    }
    return 0;
}

// ============================================================
// ClearStorage::GetTarget (Slot10, vtable +0x50, 0x1400e7330)
// ============================================================
// Returns the profession list's +0x30 profession (GameUpdateLoop::m_professionList).
// Uses hidden return pointer ABI (member function returning non-trivial).
Profession** ClearStorage::GetTarget(Profession** retbuf) {
    *retbuf = GameUpdateLoop::_singleton->m_professionList->field_0x30;
    return retbuf;
}

// ============================================================
// Slot mapping ground-truthed from the real exe vtable @ 0x1401611a0 (2026-09-05):
//   slot6 OnComplete (0x1400e6c30), slot7 OnCancel (0x1400e6c20, same remove),
//   slot8 Cleanup (0x1400e72a0), slot11 GetPriority / HasActiveTasks (0x14002a0f0,
//   count at +0x60). ENDGAME B.3.8: all members of the class now.
// ============================================================

// ============================================================
// Slot3 Serialize (vtable +0x18, retail 0x1400e7350) — was Work_SerializeNop.
// ============================================================
// A no-op here consumes ZERO bytes on load while retail's body consumes the
// ClearStorage task BST, so every save that contains a live ClearStorage work
// object desynced the entity stream on the record AFTER it. Retail, read off the
// 0x1400e7350 disassembly (22 instructions, no branches):
//
//   Work::Serialize(this, stream)                       CALL 0x1400f7400
//   IO::Serializer ser(stream, 0)                       CALL [0x140152ad8]
//   CitizenAI_impl_96b0(this + 0x58, &ser)              CALL 0x1400296b0   <- the BST
//   ~IO::Serializer(ser)                                CALL [0x140152ad0]
//
// Identical in shape to FireWork::Serialize (0x1400e9a50, firework.cpp) minus the
// trailing capacity int — FireWork's BST is at +0x50, ClearStorage's at +0x58
// (LEA RCX,[RDI+0x58] at 0x1400e7381). Both sub-calls are already ported and
// registered (reg_work_base.cpp / reg_work_bst.cpp), so this is a transcription.
// ENDGAME B.3.8: the two function-local lazy pointer caches are gone — the base call
// is the qualified Work::Persist, the BST body is called by name (B38 D7).
extern void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer);
void ClearStorage::Persist(void* stream) {
    if (!stream) return;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge (Persist's stream is void*)
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(&_field58), &ser);   // 0x1400296b0 (work_base_bst.cpp)
}

// ============================================================
// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
// ============================================================
ClearStorage::ClearStorage()
    : Work(), _field48(nullptr), _field50(nullptr), _field58(nullptr), _field60(0) {
}

ClearStorage::ClearStorage(void* pEntityIBP)
    : Work(pEntityIBP), _field48(nullptr), _field50(nullptr), _field58(nullptr),
      _field60(0), _pad64{} {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_ClearStorage_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over ClearStorage_Factory / _CreateInPlace, both
// Work_GenericCtor (the object zero-filled from +0x10, +0x20/+0x28 = -1).

// create 0x14002a0b0: MemAlloc(sizeof, 8), then the ctor.
void* ClearStorage::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClearStorage), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ClearStorage());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* ClearStorage::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ClearStorage());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(ClearStorage, "Work")
