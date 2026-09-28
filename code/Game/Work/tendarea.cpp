// code/Game/Work/tendarea.cpp
// Clean C++ reimplementation of TendArea -- all methods.
// Source: Game/Work/TendArea.cpp (from profile build debug strings)
//
// TendArea (0x60 bytes) -- Area tending work action (crops, orchards, etc).
// Inherits: Object::Internal -> Work (0x48) -> TendArea
// Vtable: 0x140161990
//
// BST node layout (simple, 0x20 bytes):
//   +0x00: key (InternalBasePtr, citizen entity)
//   +0x08: parent pointer
//   +0x10: left child
//   +0x18: right child
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/TendArea.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/CropFieldComponent.h"   // ENDGAME B.3.9 D5: CropFieldComponent::TypeId()
#include "Component/OrchardComponent.h"     // ENDGAME B.3.9 D5: OrchardComponent::TypeId()

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x1400296b0  WorkBST_BST_Serialize_296b0
//   0x140025520  WorkBST_BSTFindRemove_25520
//   0x14001a6e0  WorkBST_BSTRemove_a6e0
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x140014c30  WorkBase_TreeNodeFree_14c30
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x140003050  WorkBase_FindComponent_3050
//   0x140005380  WorkAcc_FindComponent_5380
//   0x140005440  WorkAcc_FindComponent_5440
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x1400bb0a0  WorkAct_GrazingWorkCtor_bb0a0
//   0x140021a30  WorkBST_RegisterWork_1a30
//   0x140115e60  CitizenAI_State_LongTermPlan
//   0x140006270  WorkBase_FindComponent_6270
//   0x14012cf10  CitizenComp_StorageComponent_QueryCount
//   0x140172114  kRdata_140172114 (.rdata)
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: TendArea_Ctor
// disassembles to `0x1400f5a1d: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.


// ============================================================
// TendArea::Slot4 -- Tending target assignment (0x1400f5490)
// ============================================================
//
// Algorithm:
// 1. Check max workers vs current count
// 2. Get citizen position, random access RNG
// 3. Depending on tendMode: find CropAreaComponent or OrchardAreaComponent
// 4. Pick a random cell from the area, check pathability
// 5. Get terrain position for target and citizen
// 6. Create PerformWork action
// 7. Register with CitizenAI
//
int TendArea::Execute(void* citizenIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Step 1: Check max workers
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    int64_t currentWorkers = (int64_t)selfQ[10]; // self->workerCount at +0x50

    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    if ((int)currentWorkers >= *(int*)(matDescIBP + 0x30)) {
        return 1;  // At capacity, no work
    }

    // Step 2: Get game world singletons
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void* rng = *(void**)(gameWorld + 0xd8);

    // Step 3: Get citizen position
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSize = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSize >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSize >> 1));

    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSize) citizenCellX = mapSize - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSize) citizenCellY = mapSize - 1;

    uint32_t citizenGridCell = (uint32_t)(mapSize * citizenCellY + citizenCellX);

    // Step 4: Choose area component based on tendMode and pick random cell
    uintptr_t areaCompIBP = 0;
    int targetCell;
    uint32_t typeId;
    int workFlag;
    uint32_t randIdx;

    if (tendMode == 0) {
        // Crop mode: find CropAreaComponent
        WorkAcc_FindComponent_5380(*(uintptr_t*)(selfAddr + 0x10), &areaCompIBP);

        uint32_t areaSize = *(uint32_t*)(areaCompIBP + 0x60);
        randIdx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(areaSize));   // B.3.4b bridge

        workFlag = 1;
        targetCell = *(int*)((int64_t)(int)randIdx * 0x20 + *(int64_t*)(areaCompIBP + 0x58));
        typeId = CropFieldComponent::TypeId();  // DAT_1401c7510
    } else {
        // Orchard mode: find OrchardAreaComponent
        WorkAcc_FindComponent_5440(*(uintptr_t*)(selfAddr + 0x10), &areaCompIBP);

        uint32_t areaSize = *(uint32_t*)(areaCompIBP + 0x58);
        randIdx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(areaSize));   // B.3.4b bridge

        workFlag = 2;
        targetCell = *(int*)(*(int64_t*)(areaCompIBP + 0x50) + (int64_t)(int)randIdx * 0x28);
        typeId = OrchardComponent::TypeId();  // DWORD_1401c46b0
    }

    // Step 5: Find nearest passable cell and check pathability
    int nearestCell = WorkGrid_FindCell_9ccf0(mapGrid, targetCell, citizenGridCell, 0xe1f);
    if (nearestCell < 0 || (int)randIdx < 0) {
        return 0;  // No passable cell found
    }

    bool canPath = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, citizenGridCell, nearestCell));   // the door read AL
    if (!canPath) {
        return 0;  // Can't path to cell
    }

    // Step 6: Calculate terrain positions
    float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
    Math::Vector3 targetPos(
        (float)(targetCell % *mapGrid) - halfOffset,
        (float)(targetCell / *mapGrid) - halfOffset,
        0.0f);
    float targetTerrain[3];
    Reimpl_MapGetTerrainPos(mapGrid, targetTerrain, &targetPos.x, '\0', nullptr);

    Math::Vector3 citizenPos(
        (float)(nearestCell % *mapGrid) - halfOffset,
        (float)(nearestCell / *mapGrid) - halfOffset,
        0.0f);
    float citizenTerrain[3];
    Reimpl_MapGetTerrainPos(mapGrid, citizenTerrain, &citizenPos.x, '\0', nullptr);

    // Step 7: Create PerformWork action
    uintptr_t workEntityRef = *(uintptr_t*)(selfAddr + 0x10);
    uintptr_t entityBuf[2] = { *(uintptr_t*)citizenIBP, 0 };

    WorkAct_GrazingWorkCtor_bb0a0(
        entityBuf,                  // citizenIBP
        (int)selfQ[4],             // priority
        (void*)&workEntityRef,      // work entity
        0,                          // unused
        citizenTerrain,             // target position
        targetTerrain,              // source position
        typeId,                     // type ID
        workFlag,                   // work flag (1=crop, 2=orchard)
        randIdx);                   // cell index

    // Step 8: Assign work and register with AI
    int priority = (int)selfQ[4];
    uintptr_t citEnt = *(uintptr_t*)citizenIBP;

    // Call vtable Slot10 (GetWorkEntity) to get work entity
    uintptr_t workEntIBP[2] = {0};
    uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
    uintptr_t workEnt = *wResult;

    *(uint32_t*)(citEnt + 0x54) |= 4;
    *(int32_t*)(citEnt + 0x30) = priority;
    *(uint32_t*)(citEnt + 0xb0) = 0;

    if (workEnt == *(uintptr_t*)(citEnt + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citEnt + 0x88) |= 0x20000;
    }

    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), workEntIBP, citizenIBP);
    return 2;
}


// ============================================================
// TendArea::Slot6 -- BSTFindRemove + return false (0x1400f5470)
// ============================================================
int TendArea::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return false;
}


// ============================================================
// TendArea::Slot7 -- BSTFindRemove thunk (0x1400f5460)
// ============================================================
int TendArea::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// TendArea::Slot8 / CancelAll (0x1400f5830)
// Loops through BST at +0x48 while count > 0.
// For each leftmost node, cancels the work item.
// ============================================================
int TendArea::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;

    while (*(int*)(selfAddr + 0x50) > 0) {
        // Find leftmost node in BST (following +0x10 = left child)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// TendArea::Slot9 / RemoveInvalid (0x1400f5230)
// Iterates BST, validates each entry, removes invalid ones.
// Returns true if all valid, false otherwise.
// Simple BST: node +0x08=parent, +0x10=left, +0x18=right
// ============================================================
int TendArea::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)(selfAddr + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find leftmost node (minimum)
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Validate this entity
        bool stillValid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                uintptr_t* newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 8);
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    uintptr_t val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = entityIBP;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    System::MemoryAllocator::MemFree(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        void* rightChild = *(void**)((uintptr_t)node + 0x18);
        if (rightChild != nullptr) {
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x08);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                } else {
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(bstRootPtr), searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(removeArr);
    return removeCount == 0;
}


// ============================================================
// TendArea::Slot10 / GetWorkEntity (0x1400f58a0)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* TendArea::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityRef), subIBP);
    void* result = subIBP;


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// TendArea::GetPriority [11] / HasWorkers (0x140029860)
// 9-byte thunk, returns (workerCount > 0).
// ============================================================
int TendArea::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(workerCount) > 0 ? 1 : 0;
}





// ============================================================
// TendArea::Serialize (0x1400f5900)
// Work::Serialize + Serializer(v0) + BST serialize + int field.
// ============================================================
void TendArea::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create serializer (version block, version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at +0x48
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializerBuf);

    // Serialize int at +0x58 (tendMode)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&tendMode), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// TendArea::DtorBody
// Sets vtable, frees BST nodes, calls base dtor.
// ============================================================
// TendArea dtor (called by slot 0 0x14002c5d0). The stamps and the `~Internal` call are the compiler's chain.
TendArea::~TendArea() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree (simple BST: +0x10=left, +0x18=right)
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        System::MemoryAllocator::MemFree(bstRoot);
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's.
TendArea::TendArea() : Work(), bstRoot(nullptr), workerCount(0), _pad54{}, tendMode(0) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_TendArea_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over TendArea_Factory / _CreateInPlace (both
// Work_GenericCtor), plus two vtable-pointer statics for the dtor's stamps.

// create 0x14002c590: MemAlloc(sizeof, 8), then the ctor.
void* TendArea::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TendArea), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TendArea());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* TendArea::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TendArea());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(TendArea, "Work")
