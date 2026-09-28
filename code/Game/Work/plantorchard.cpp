// code/Game/Work/plantorchard.cpp
// Clean C++ reimplementation of PlantOrchard Work class -- all vtable methods.
// Source: Game/Work/PlantOrchard.cpp (from profile build debug strings)
//
// PlantOrchard (0x70 bytes) -- Orchard planting work action.
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// Vtable: 0x140161910
//
// Nearly identical to PlantArea except:
// - Uses FindComp_OrchardDesc (0x140005440) instead of FindComp_CropDesc
// - Uses different action ctor (0x1400be6e0) with different parameter layout
// - Orchard cells have slot data at OrchardDesc+0x50 with 0x28 stride
// - Calls FUN_140051830 to refresh orchard slot state
// - OnComplete/OnCancel deal with single cell (param4 is plain int, not packed)
// - Validate marks cells with 0 (not available) instead of 1
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/PlantOrchard.h"
#include "Work/plantorchard_funcs.h"
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
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/OrchardComponent.h"   // ENDGAME B.3.9 D5: OrchardComponent::TypeId()

// Diagnostics-only counters (standalone/orchard_diag.cpp). No-op unless
// BANISHED_ORCHARD_DIAG is set; never change behaviour.
extern "C" void OrchardDiag_NotePlantConstructed();
extern "C" void OrchardDiag_NotePlantExecuted();

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x140003050  WorkBase_FindComponent_3050
//   0x140005440  WorkAcc_FindComponent_5440
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x1400be6e0  WorkAct_PlantWorkCtor_be6e0
//   0x140029a40  WorkCellBST_Insert_9a40
//   0x14001ca60  WorkCellBST_Remove_ca60
//   0x140029de0  WorkCellBST_Serialize_9de0
//   0x14000e320  WorkBase_TreeNodeFree_e320
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140002340  Reimpl_ExternalPtr_Serialize_StringTable
//   0x140051830  OrchardComponent_ValidateRefs
//   0x140172114  kRdata_140172114 (.rdata)


// ============================================================
// Helper: BST search for int key
// ============================================================
static int* BST_FindInt(void* root, int key) {
    int* node = (int*)root;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);
        } else if (key > *node) {
            node = *(int**)(node + 8);
        } else {
            return node;
        }
    }
    return nullptr;
}


// ============================================================
// PlantOrchard::Slot4 -- Main orchard planting assignment (0x1400f4af0)
// ============================================================
//
// Algorithm:
// 1. Find MaterialDescription and OrchardDescription on workplace entity
// 2. Refresh orchard slot state (FUN_140051830)
// 3. Check max workers vs current count
// 4. Iterate orchard cells, find plantable spots (validity flag==0, not in BST)
// 5. Check orchard slot data for available planting location
// 6. Check pathfinding
// 7. Create orchard planting action, assign work, insert into BST
//
int PlantOrchard::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Step 1: Find components
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    uintptr_t orchardDescIBP = 0;
    WorkAcc_FindComponent_5440(*(uintptr_t*)(selfAddr + 0x10), &orchardDescIBP);

    // Step 2: Refresh orchard slot state
    OrchardComponent_ValidateRefs(reinterpret_cast<void*>(orchardDescIBP));

    // Step 3: Check max workers
    int treeCount = (int)selfQ[10];  // self->field_50
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Step 4: Get map and citizen position
    int* mapGrid = *(int**)((uintptr_t)GameUpdateLoop::_singleton + 0xf8);

    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;
    uint32_t citizenGridCell = (uint32_t)(mapSz * citizenCellY + citizenCellX);

    // Step 5: Iterate orchard cells
    int fieldCellCount = (int)selfQ[0xc];  // self->field_60

    if (fieldCellCount > 0) {
        uintptr_t slotOffset = 0;  // stride 0x28 per slot in orchard desc
        for (int i = 0; i < fieldCellCount; i++, slotOffset += 0x28) {
            char* validityArray = (char*)selfQ[0xb];  // self->field_58

            // Orchard uses inverted validity: 0 = available, 1 = planted
            if (validityArray[i] != 0) {
                continue;
            }

            // Check if cell is already assigned (in BST)
            int* bstRoot = (int*)selfQ[9];
            if (BST_FindInt(bstRoot, i) != nullptr) {
                continue;
            }

            // Check orchard slot data: slot at OrchardDesc+0x50 with 0x28 stride
            // If slot's entity field (+8) is non-zero, it's already occupied
            uintptr_t orchardSlotData = *(uintptr_t*)(orchardDescIBP + 0x50);
            if (*(uintptr_t*)(orchardSlotData + slotOffset + 8) != 0) {
                continue;
            }

            // Get grid cell index from orchard description slot data
            uintptr_t orchardSlotData2 = *(uintptr_t*)(orchardDescIBP + 0x50);
            int srcCellIdx = *(int*)(orchardSlotData2 + slotOffset);

            // Check pathfinding
            int nearestCell = WorkGrid_FindCell_9ccf0(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);
            if (nearestCell < 0) {
                continue;
            }

            bool canPath = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, citizenGridCell, nearestCell));   // the door read AL
            if (!canPath) {
                continue;
            }

            // Valid spot found -- create orchard planting action
            uint32_t typeConstVal = OrchardComponent::TypeId();  // DAT_1401c46b0
            uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

            float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
            Math::Vector3 targetPos(
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            float pathPos[4];
            Reimpl_MapGetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

            // Get citizen entity
            uintptr_t citizenBuf = *(uintptr_t*)entityIBP;

            // Call FUN_1400be6e0 orchard planting action ctor
            WorkAct_PlantWorkCtor_be6e0(
                &citizenBuf,                      // RCX: citizenIBP
                (int)selfQ[4],                    // EDX: priority
                (void*)selfQ[0xd],                // R8: self->field_68 (ExternalPtr)
                0,                                // R9D: 0
                pathPos,                          // [RSP+0x20]: position
                srcCellIdx,                       // [RSP+0x28]: srcCellIdx
                (uint32_t)i,                      // [RSP+0x30]: cellIdx (integer, EBX)
                reinterpret_cast<int64_t*>(&entityRef),                       // [RSP+0x38]: entity ref from field_10
                &typeConstVal,                    // [RSP+0x40]: type constant
                0,                                // [RSP+0x48]: unused
                (uint32_t)i                       // [RSP+0x50]: cellIdx again (integer, EBX)
            );

            // Assign work
            int priority = (int)selfQ[4];
            uintptr_t citizenEnt = *(uintptr_t*)entityIBP;

            uint32_t resultBuf[4] = {0};
            uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(resultBuf));
            uintptr_t workEnt = *wResult;

            *(uint32_t*)(citizenEnt + 0x54) |= 4;
            *(int32_t*)(citizenEnt + 0x30) = priority;
            *(uint32_t*)(citizenEnt + 0xb0) = 0;

            if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
            }

            // Insert into BST
            uintptr_t insertResult[2] = {0};
            uintptr_t citizenIBPVal = *(uintptr_t*)entityIBP;
            uint32_t insertKey[4];
            // BST node key: first 4 bytes = cell index, next 8 bytes = citizen IBP.
            // Integer, not float — retail is `MOV dword ptr [RBP+0xf],EBX` (0x1400f4e3a),
            // and the matching lookups (BST_FindInt above, OnComplete's param4) both
            // compare it as an int.
            insertKey[0] = (uint32_t)i;
            insertKey[1] = 0;
            insertKey[2] = (uint32_t)citizenIBPVal;
            insertKey[3] = (uint32_t)(citizenIBPVal >> 32);
            WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), insertResult, insertKey);

            // Mark cell as planted (not available)
            validityArray[i] = 1;

            OrchardDiag_NotePlantExecuted();
            return 2;
        }
    }

    return 1;
}





// ============================================================
// PlantOrchard::OnComplete (Slot6) -- 0x1400f4a90
// Removes BST entry, marks cell available (sets validity to 0).
// param4 is plain int cell index (not packed like PlantArea).
// ============================================================
int PlantOrchard::OnComplete(void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Search BST for cell index and remove
    int* node = (int*)selfQ[9];
    while (node != nullptr) {
        if (param4 < *node) {
            node = *(int**)(node + 6);
        } else if (param4 > *node) {
            node = *(int**)(node + 8);
        } else {
            WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node);
            break;
        }
    }

    // Mark cell as available (validity = 0 for orchard)
    char* validityArray = (char*)selfQ[0xb];
    validityArray[param4] = 0;
    return 0;
}


// ============================================================
// PlantOrchard::OnCancel (Slot7) -- 0x1400f4a20
// Marks cell available. Does NOT remove BST entry (unlike OnComplete).
// ============================================================
int PlantOrchard::OnCancel(void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Search BST for cell and remove
    int* node = (int*)selfQ[9];
    while (node != nullptr) {
        if (param4 < *node) {
            node = *(int**)(node + 6);
        } else if (param4 > *node) {
            node = *(int**)(node + 8);
        } else {
            WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node);
            break;
        }
    }

    // Mark cell as available
    char* validityArray = (char*)selfQ[0xb];
    validityArray[param4] = 0;
    return 0;
}


// ============================================================
// PlantOrchard::Cleanup (Slot8) -- 0x1400f4e70
// CancelAll: identical to PlantArea -- loops BST, cancels each work item.
// ============================================================
int PlantOrchard::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18);
        }

        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// PlantOrchard::Validate (Slot9) -- 0x1400f47f0
// RemoveInvalid: identical pattern to PlantArea, but marks cells with 0.
// ============================================================
int PlantOrchard::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    int* collectArray = nullptr;
    int* currentArray = nullptr;
    uint32_t collectCount = 0;
    uint32_t collectCapacity = 0;

    int** bstRootPtr = (int**)(selfAddr + 0x48);
    int* node = *bstRootPtr;

    if (node == nullptr) {
        goto cleanup;
    }

    // Find leftmost node
    {
        int* cur = node;
        while (cur != nullptr) {
            node = cur;
            cur = *(int**)(cur + 6);
        }
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2);

        uint8_t valid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), uintptr_t{0}, (void*)&ibpData, nullptr);

        if (valid == 0) {
            uint32_t newCount = collectCount + 1;
            if (newCount != collectCount) {
                int* newArray = collectArray;
                if ((int)collectCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (collectCount != 0) {
                        newCap = newCount * 3;
                    }
                    collectCapacity = newCap;
                    newArray = (int*)System::MemoryAllocator::MemAlloc((int64_t)newCap << 2, 4);
                    currentArray = newArray;
                }

                uint32_t copyStart = 0;
                if (collectArray == newArray) {
                    copyStart = collectCount;
                }
                for (uint32_t j = copyStart; j < newCount; j++) {
                    if (j < collectCount) {
                        newArray[j] = collectArray[j];
                    } else {
                        newArray[j] = *node;
                    }
                }
                if (collectArray != newArray) {
                    System::MemoryAllocator::MemFree(collectArray);
                }
                collectArray = newArray;
                collectCount = newCount;
            }

            // Mark cell available (orchard uses 0 for available)
            char* validityArray = (char*)selfQ[0xb];
            validityArray[*node] = 0;
        }

        // Move to next in-order node
        int* right = *(int**)(node + 8);
        if (right != nullptr) {
            int* cur = right;
            while (cur != nullptr) {
                node = cur;
                cur = *(int**)(cur + 6);
            }
        } else {
            int* parent = *(int**)(node + 4);
            if (parent != nullptr) {
                if (*(int**)(parent + 6) != node) {
                    while (parent != nullptr && *(int**)(parent + 8) == node) {
                        node = parent;
                        parent = *(int**)(parent + 4);
                    }
                }
                node = parent;
            } else {
                node = nullptr;
            }
        }
    }

cleanup:
    if ((int)collectCount > 0) {
        for (uint32_t k = 0; k < collectCount; k++) {
            int* bstNode = *bstRootPtr;
            while (bstNode != nullptr) {
                if (collectArray[k] < *bstNode) {
                    bstNode = *(int**)(bstNode + 6);
                } else if (collectArray[k] > *bstNode) {
                    bstNode = *(int**)(bstNode + 8);
                } else {
                    WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(bstRootPtr), bstNode);
                    break;
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(currentArray ? currentArray : collectArray);
    return collectCount == 0;
}


// ============================================================
// PlantOrchard::GetTarget (Slot10) -- 0x1400f4ef0
// Identical to PlantArea::GetTarget.
// ============================================================
void* PlantOrchard::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entity), subIBP);
    void* result = subIBP;

    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)result + 0x48);

    return outPtr;
}


// ============================================================
// PlantOrchard::GetPriority (Slot11) -- shared 0x140029860
// ============================================================
int PlantOrchard::GetPriority() {
    return field_50 > 0 ? 1 : 0;
}


// ============================================================
// PlantOrchard::Serialize -- 0x1400f4f50
// Identical to PlantArea::Serialize.
// ============================================================
void PlantOrchard::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    Work::Persist(stream);   // 0x1400f7400, a direct call in retail — QUALIFIED here

    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializerBuf);

    int cellCountBuf[2] = { *(int*)(selfAddr + 0x60), 0 };
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(cellCountBuf), nullptr);   // B.3.4 bridge

    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::U8>((void*)(selfAddr + 0x58)).SetSize(cellCountBuf[0]);
    }

    // Serialize validity array through the BOOL-ARRAY door.
    //
    // ENDGAME B.3.4a closing — same alias bug, same retail proof as
    // plantarea.cpp and harvestarea.cpp. PlantOrchard::Serialize
    // (Application-x64.exe 0x1400f4f50) is instruction-for-instruction the same
    // function as PlantArea's with one different field offset earlier:
    //     1400f4fc1  XOR R9D,R9D                    ; name = nullptr
    //     1400f4fc4  MOV R8,qword ptr [RDI + 0x58]  ; the buffer
    //     1400f4fc8  MOV EDX,dword ptr [RSP + 0x50] ; the count
    //     1400f4fcc  LEA RCX,[RSP + 0x28]           ; the Serializer
    //     1400f4fd1  CALL qword ptr [0x140151458]   ; the bool-array door
    // The local pointer was aliased onto 0x140152ac8, the three-parameter
    // `Serialize(int&)` scalar door, and called with four arguments.
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(
        cellCountBuf[0], reinterpret_cast<bool*>(*(void**)(selfAddr + 0x58)), nullptr);   // B.3.4 bridge  (slot 0x140151458 = bool-array door)

    Reimpl_ExternalPtr_Serialize_StringTable((void*)(selfAddr + 0x68), serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// PlantOrchard::DtorBody -- 0x1400f5010
// Identical structure to PlantArea dtor.
// ============================================================
// PlantOrchard dtor 0x1400f5010. The stamps and the `~Internal` call are the compiler's chain.
PlantOrchard::~PlantOrchard() {
    uintptr_t selfAddr = (uintptr_t)this;

    System::MemoryAllocator::MemFree(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        WorkBase_TreeNodeFree_e320((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        WorkBase_TreeNodeFree_e320((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        System::MemoryAllocator::MemFree(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor 0x1400f50b0 (deserialize/factory variant): `??0Internal`, the Work base fields,
// the leaf fields zeroed (+0x60/+0x64 as the one qword the old port wrote; the two
// stamps are the compiler's). The orchard diagnostic counts the construction (the old
// factory ctor did the same).
PlantOrchard::PlantOrchard()
    : Work(), field_48(nullptr), field_50(0), field_58(nullptr), field_60(0), field_64(0),
      field_68(nullptr) {
    OrchardDiag_NotePlantConstructed();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_PlantOrchard_Vtable` (slot 1 null, slot 2 a lazily
// cached id), PlantOrchard_Ctor_Impl (the ctor with both stamps written by hand) and
// REGISTER_TYPE over PlantOrchard_Factory / _CreateInPlace, plus two vtable-pointer
// statics for the dtor's stamps. Nothing outside this block reads 0x140161910 back
// (the from-entity ctor that stamped it is the real ctor now), so its registration
// goes with the table.

// create 0x14002c500: MemAlloc(sizeof, 8), then the ctor.
void* PlantOrchard::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PlantOrchard), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PlantOrchard());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* PlantOrchard::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PlantOrchard());
}


#include "Object/factory_registration.h"

DEFINE_EXE_TYPE(PlantOrchard, "Work")
