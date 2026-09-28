// code/Game/Work/plantarea.cpp
// Clean C++ reimplementation of PlantArea Work class -- all vtable methods.
// Source: Game/Work/PlantArea.cpp (from profile build debug strings)
//
// PlantArea (0x70 bytes) -- Crop field planting work action.
// Inherits: Object::Internal -> Work (0x48) -> PlantArea
// Vtable: 0x140161890
//
// Struct layout:
//   +0x00: vtable
//   +0x08: Object::Internal base
//   +0x10..+0x44: Work base fields
//   +0x48: BST root (tree of assigned work items, int-keyed)
//   +0x50: BST count (int)
//   +0x58: validity array ptr (byte array, one per field cell)
//   +0x60: field cell count (int)
//   +0x68: ExternalPtr (crop description resource reference)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/PlantArea.h"
#include "Work/plantarea_funcs.h"
#include "engine_funcs.h"
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
#include <cstdlib>
#include <new>
#include "Component/CropFieldComponent.h"   // ENDGAME B.3.9 D5: CropFieldComponent::TypeId()

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x140003050  WorkBase_FindComponent_3050
//   0x140005380  WorkAcc_FindComponent_5380
//   0x140002f90  WorkBase_FindComponent_2f90
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x1400bdd20  WorkAct_ShowToolCtor3_bdd20
//   0x140029a40  WorkCellBST_Insert_9a40
//   0x14001ca60  WorkCellBST_Remove_ca60
//   0x140029de0  WorkCellBST_Serialize_9de0
//   0x14000e320  WorkBase_TreeNodeFree_e320
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140002340  Reimpl_ExternalPtr_Serialize_StringTable
//   0x14011b390  WorkLeaf_PlantArea_b390
//   0x140172114  kRdata_140172114 (.rdata)

// B3R: resolves nothing. Kept only because cropfieldcomponent_states.cpp (another
// block's file) still calls it; delete it with that call.
bool PlantArea_ResolveFuncs() {
    return true;
}


// ============================================================
// Helper: BST search for int key
// BST node layout: [0]=int key, [+8]=IBP citizen, [+0x10]=parent, [+0x18]=left, [+0x20]=right
// ============================================================
static int* BST_FindInt(void* root, int key) {
    int* node = (int*)root;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);   // left child at +0x18
        } else if (key > *node) {
            node = *(int**)(node + 8);   // right child at +0x20
        } else {
            return node;  // found
        }
    }
    return nullptr;
}


// ============================================================
// PlantArea::Slot4 -- Main planting assignment logic (0x1400f4010)
// ============================================================
//
// Algorithm:
// 1. Find MaterialDescription and CropDescription on workplace entity
// 2. Check max workers vs current assigned count
// 3. Get citizen position, compute grid cell
// 4. Iterate field cells: find plantable spots (validity flag set, not in BST)
// 5. Check for paired cell (adjacent, also valid and unassigned)
// 6. Look up grid cell index from CropDescription, check passability
// 7. Create planting action, assign work, insert into BST
// 8. If no cells remaining and count==0, mark crop description complete
//
int PlantArea::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Step 1: Find components on workplace entity
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    uintptr_t cropDescIBP = 0;
    WorkAcc_FindComponent_5380(*(uintptr_t*)(selfAddr + 0x10), &cropDescIBP);

    // Step 2: Check max workers vs current tree count
    int treeCount = (int)selfQ[10];  // self->field_50
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    // ── Plant-search census (BANISHED_PLANTAREA_TRACE) ───────────────────────────
    // Execute runs but never reaches the action ctor, and "found nothing" cannot say
    // which of the five gates is responsible. Count each. Zero cost when unset.

    if (treeCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Step 3: Get map and citizen position
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

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

    // Step 4: Iterate field cells
    int fieldCellCount = (int)selfQ[0xc];  // self->field_60
    bool anyUnplanted = true;  // tracks if we found any cell with validity flag set

    if (fieldCellCount > 0) {
        for (int i = 0; i < fieldCellCount; i++) {
            char* validityArray = (char*)selfQ[0xb];  // self->field_58
            if (validityArray[i] == 0) {
                continue;
            }

            // Check if cell is already assigned (in BST)
            int* bstRoot = (int*)selfQ[9];  // self->field_48
            if (BST_FindInt(bstRoot, i) != nullptr) {
                continue;
            }

            // This cell needs planting -- mark that we had work to do
            anyUnplanted = false;

            // Check for paired cell (i+1)
            bool pairedAvailable = false;
            int pairedIdx = i + 1;
            if (pairedIdx < fieldCellCount && validityArray[pairedIdx] != 0) {
                int* bstRoot2 = (int*)selfQ[9];
                if (BST_FindInt(bstRoot2, pairedIdx) == nullptr) {
                    pairedAvailable = true;
                }
            }

            // Get grid cell index from CropDescription
            uintptr_t cropDescVal = cropDescIBP;
            int srcCellIdx = *(int*)((uintptr_t)i * 0x20 + *(uintptr_t*)(cropDescVal + 0x58));

            // ⚠ The action ctor's 7th argument is a pointer to TWO adjacent ints, not one.
            // Retail builds them as the stack pair local_70 / local_6c:
            //     local_70 = slot[i].cell
            //     local_6c = paired ? slot[pairedIdx].cell : -1
            // and passes &local_70, so the callee reads [ptr+0] AND [ptr+4].
            //
            // This used to pass the address of a LONE int, and computed the paired cell
            // only to discard it with a (void) cast. The callee therefore read whatever
            // stack local happened to follow, and CreateNaturalResource ended up with a
            // truncated heap pointer as its cell index (measured: cellIndex=87879760),
            // faulting on grid + cell*0x30. That is the out-param-buffer-too-small trap
            // already recorded for this codebase, in its read-side form.
            int cellPair[2];
            cellPair[0] = srcCellIdx;
            cellPair[1] = -1;
            if (pairedAvailable) {
                uintptr_t cropDescVal2 = cropDescIBP;
                cellPair[1] = *(int*)((uintptr_t)pairedIdx * 0x20 +
                                      *(uintptr_t*)(cropDescVal2 + 0x58));
            }

            // Check pathfinding
            int nearestCell = WorkGrid_FindCell_9ccf0(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);
            if (nearestCell < 0) {
                continue;
            }

            bool canPath = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, citizenGridCell, nearestCell));   // the door read AL
            if (!canPath) {
                continue;
            }

            uint32_t typeConstVal = CropFieldComponent::TypeId();  // DAT_1401c7510
            uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

            // Pack cell indices: primary in low 16 bits, paired in high 16 bits
            uint32_t packedCells[2];
            packedCells[0] = (uint32_t)i;
            packedCells[1] = (uint32_t)pairedIdx;

            float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
            Math::Vector3 targetPos(
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            float pathPos[4];
            Reimpl_MapGetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

            // Get citizen entity for action
            uintptr_t citizenBuf = *(uintptr_t*)entityIBP;

            int harvestMode = pairedAvailable ? 2 : 1;
            uint32_t packedCellInfo = ((uint32_t)pairedIdx << 16) | (uint32_t)i;

            // Call ShowTool_Action_Ctor3 (0x1400bdd20) with 12 params
            WorkAct_ShowToolCtor3_bdd20(
                &citizenBuf,                      // RCX: citizenIBP
                (int)selfQ[4],                    // EDX: priority
                reinterpret_cast<uint32_t*>((void*)selfQ[0xd]),                // R8: self->field_68 (ExternalPtr)
                0,                                // R9D: 0
                pathPos,                          // [RSP+0x20]: position
                harvestMode,                      // [RSP+0x28]: 1=single, 2=paired
                reinterpret_cast<uint32_t*>(cellPair),                         // [RSP+0x30]: ptr to {cell, pairedCell}
                packedCellInfo,                   // [RSP+0x38]: packed cell indices
                reinterpret_cast<int64_t*>(&entityRef),                       // [RSP+0x40]: entity ref from field_10
                &typeConstVal,                    // [RSP+0x48]: type constant
                0,                                // [RSP+0x50]: unused
                reinterpret_cast<int64_t>(packedCells)   // [RSP+0x58]: packed cell pair values
            );

            // Assign work and register with AI
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

            // Insert primary cell into BST
            uintptr_t insertResult[2] = {0};
            uintptr_t citizenIBPVal = *(uintptr_t*)entityIBP;
            float keyBuf[4];
            *(uint32_t*)&keyBuf[0] = 0;  // padding for alignment
            keyBuf[0] = (float)i;  // actually int key
            // Build key: first 4 bytes = cell index, next 8 bytes = citizen IBP
            uint32_t insertKey[4];
            insertKey[0] = (uint32_t)i;
            insertKey[1] = 0;
            insertKey[2] = (uint32_t)citizenIBPVal;
            insertKey[3] = (uint32_t)(citizenIBPVal >> 32);
            WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), insertResult, insertKey);

            // Mark primary cell as used (not available)
            validityArray[i] = 0;

            // If paired, also insert and mark
            if (pairedAvailable) {
                uintptr_t insertResult2[2] = {0};
                uintptr_t citizenIBPVal2 = *(uintptr_t*)entityIBP;
                uint32_t insertKey2[4];
                insertKey2[0] = (uint32_t)pairedIdx;
                insertKey2[1] = 0;
                insertKey2[2] = (uint32_t)citizenIBPVal2;
                insertKey2[3] = (uint32_t)(citizenIBPVal2 >> 32);
                WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), insertResult2, insertKey2);
                validityArray[pairedIdx] = 0;
            }

            return 2;
        }

        // If we had no cells with validity flag set (anyUnplanted still true)
        // we return 1 below
        if (!anyUnplanted) {
            return 1;
        }
    }

    // If tree count is 0 and we exhausted all cells, mark crop desc as complete
    if ((int)selfQ[10] == 0) {
        WorkLeaf_PlantArea_b390(cropDescIBP);
    }

    return 1;
}





// ============================================================
// PlantArea::OnComplete (Slot6) -- 0x1400f3f60
// Removes BST entries, marks cells valid.
// param4 packed: low 16 = primary cell, high 16 = paired cell.
// ============================================================

// Slot-traffic census (BANISHED_PLANTAREA_TRACE). A dropped plant action leaves its
// BST entry and its cleared mark in place forever unless the cancel/validate path
// reclaims it. Counting the three slots separates "never dispatched" from "runs and
// does not restore".
namespace { void PA_SlotHit(const char* which) {
    static long long s_complete = 0, s_cancel = 0, s_validate = 0, s_n = 0;
    if (which[0] == 'C') ++s_complete; else if (which[0] == 'X') ++s_cancel; else ++s_validate;
    if ((++s_n % 10) == 0) {
        std::fprintf(stderr, "[PlantSlots] OnComplete=%lld OnCancel=%lld Validate=%lld\n",
                     s_complete, s_cancel, s_validate);
        std::fflush(stderr);
    }
} }
int PlantArea::OnComplete(void* /*param2*/, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    PA_SlotHit("C");
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    uint32_t primaryIdx = param4 & 0xFFFF;
    uint32_t pairedIdx = (param4 >> 16) & 0xFFFF;

    // Search BST for primary cell and remove
    int* node = (int*)selfQ[9];  // BST root at +0x48
    while (node != nullptr) {
        if ((int)primaryIdx < *node) {
            node = *(int**)(node + 6);
        } else if ((int)primaryIdx > *node) {
            node = *(int**)(node + 8);
        } else {
            WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node);
            break;
        }
    }

    // ⚠ DO NOT restore the validity byte here. Retail's slot 6 (0x1400f3f60) removes the
    // primary and paired indices from the BST and touches +0x58 NOT AT ALL; it is slot 7,
    // OnCancel (0x1400f3ea0), that sets validity[idx] = 1 to put the cells back up for
    // replanting. This port had slot 7's body in slot 6, so every COMPLETED plant
    // immediately re-offered its own cell: the citizen replanted the same tile forever,
    // and CreateNaturalResource bailed "tile already holds an entity" on every retry
    // (measured: 129 plant attempts, 2 binds, the same cell 41318 every time).
    //
    // This is the identical Slot6/Slot7 swap the orchard wave already paid for — see
    // reference note "Slot6 (COMPLETED) must NOT restore the validity byte". Same bug,
    // different class, found the same way: by a counter showing work happening over and
    // over with no progress.
    //
    // Retail's return here really is 0 in the low byte (`in_RAX & ...ff00`), so
    // TryCompleteTask reporting false is faithful and was never the defect.

    // Handle paired cell (if within bounds)
    if ((int)pairedIdx < *(int*)(selfAddr + 0x60)) {
        // Remove paired cell from BST
        int* node2 = (int*)selfQ[9];
        while (node2 != nullptr) {
            if ((int)pairedIdx < *node2) {
                node2 = *(int**)(node2 + 6);
            } else if ((int)pairedIdx > *node2) {
                node2 = *(int**)(node2 + 8);
            } else {
                WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node2);
                break;
            }
        }
    }
    return 0;
}


// ============================================================
// PlantArea::OnCancel (Slot7) -- 0x1400f3ea0
// Marks cells valid, removes paired BST entry.
// param4 packed: low 16 = primary cell, high 16 = paired cell.
// ============================================================
int PlantArea::OnCancel(void* /*param2*/, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    PA_SlotHit("X");
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    uint32_t primaryIdx = param4 & 0xFFFF;
    uint32_t pairedIdx = (param4 >> 16) & 0xFFFF;

    // Search BST for primary cell and remove
    int* node = (int*)selfQ[9];
    while (node != nullptr) {
        if ((int)primaryIdx < *node) {
            node = *(int**)(node + 6);
        } else if ((int)primaryIdx > *node) {
            node = *(int**)(node + 8);
        } else {
            WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node);
            break;
        }
    }

    // Mark primary cell as valid
    char* validityArray = (char*)selfQ[0xb];
    validityArray[primaryIdx] = 1;

    // Handle paired cell
    if ((int)pairedIdx < *(int*)(selfAddr + 0x60)) {
        validityArray[pairedIdx] = 1;

        // Remove paired cell from BST
        int* node2 = (int*)selfQ[9];
        while (node2 != nullptr) {
            if ((int)pairedIdx < *node2) {
                node2 = *(int**)(node2 + 6);
            } else if ((int)pairedIdx > *node2) {
                node2 = *(int**)(node2 + 8);
            } else {
                WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), node2);
                return 0;
            }
        }
    }
    return 0;
}


// ============================================================
// PlantArea::Cleanup (Slot8) -- 0x1400f4460
// CancelAll: loops BST while count > 0, cancels each work item.
// ============================================================
int PlantArea::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find rightmost node in BST (follows +0x18 = right child)
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18);
        }

        // Cancel work via citizen AI
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// PlantArea::Validate (Slot9) -- 0x1400f3c70
// RemoveInvalid: iterates BST, validates entries, removes dead ones.
// Marks invalid cells as valid (available for re-planting).
// ============================================================
int PlantArea::Validate() {
    PA_SlotHit("V");
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
            cur = *(int**)(cur + 6);  // left child
        }
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2);  // IBP at +0x8

        uint8_t valid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), uintptr_t{0}, (void*)&ibpData, nullptr);


        if (valid == 0) {
            // Invalid -- add to collect list (grow array if needed)
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
                        newArray[j] = *node;  // cell index key
                    }
                }
                if (collectArray != newArray) {
                    System::MemoryAllocator::MemFree(collectArray);
                }
                collectArray = newArray;
                collectCount = newCount;
            }

            // Mark cell as valid (available for re-planting)
            char* validityArray = (char*)selfQ[0xb];
            validityArray[*node] = 1;
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
    // Remove collected invalid nodes from BST
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
// PlantArea::GetTarget (Slot10) -- 0x1400f44e0
// Returns entity ptr from work's entity ref sub-object at +0x48.
// ============================================================
void* PlantArea::GetTarget(void* outPtr) {
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
// PlantArea::GetPriority (Slot11) -- 0x140029860
// 9 bytes, shared across ClearArea/PlantArea/PlantOrchard.
// Returns (this->field_50 > 0).
// ============================================================
int PlantArea::GetPriority() {
    return field_50 > 0 ? 1 : 0;
}


// ============================================================
// PlantArea::Serialize -- 0x1400f4540
// Work::Serialize + BST + count + bool array + ExternalRef.
// ============================================================
void PlantArea::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create Serializer with version=0
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at +0x48
    WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializerBuf);

    // Serialize field cell count at +0x60
    int cellCountBuf[2] = { *(int*)(selfAddr + 0x60), 0 };
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(cellCountBuf), nullptr);   // B.3.4 bridge

    // If reading (serializer mode == 0), resize validity array.
    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::U8>((void*)(selfAddr + 0x58)).SetSize(cellCountBuf[0]);
    }

    // Serialize validity array through the BOOL-ARRAY door.
    //
    // ENDGAME B.3.4a closing — SETTLED FROM RETAIL, not from the alias, exactly
    // as batch H settled the identical bug in harvestarea.cpp. Retail
    // PlantArea::Serialize (Application-x64.exe 0x1400f4540) ends with
    //     1400f45b1  XOR R9D,R9D                    ; name = nullptr
    //     1400f45b4  MOV R8,qword ptr [RDI + 0x58]  ; the buffer
    //     1400f45b8  MOV EDX,dword ptr [RSP + 0x50] ; the count
    //     1400f45bc  LEA RCX,[RSP + 0x28]           ; the Serializer
    //     1400f45c1  CALL qword ptr [0x140151458]
    // 0x140151458 is `?Serialize@Serializer@IO@@QEAAXHPEA_NPEBD@Z`
    // (address_registry.cpp:1163), the FOUR-parameter (int n, bool*, const char*)
    // array door at rw_vtbl::kArrayBool.
    //
    // `s_fn_Serializer_SerializeBoolArray` was ALIASED onto 0x140152ac8 (that alias
    // and its door are gone with the B.3.4a closing sweep), which is the
    // THREE-parameter `Serialize(int&)` SCALAR door. Called with four arguments,
    // the element COUNT landed in RDX — the register the scalar body
    // dereferences as `int*` — so this was a wild read off a small integer, not
    // a differently-shaped write. It could never have been retail behaviour.
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(
        cellCountBuf[0], reinterpret_cast<bool*>(*(void**)(selfAddr + 0x58)), nullptr);   // B.3.4 bridge  (slot 0x140151458 = bool-array door)

    // Serialize ExternalRef at +0x68
    Reimpl_ExternalPtr_Serialize_StringTable((void*)(selfAddr + 0x68), serializerBuf);

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// PlantArea::DtorBody -- 0x1400f4600
// Sets vtable, frees validity array + BST, calls Work dtor chain.
// ============================================================
// PlantArea dtor 0x1400f4600. The stamps and the `~Internal` call are the compiler's chain.
PlantArea::~PlantArea() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free validity array at +0x58
    System::MemoryAllocator::MemFree(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    // Free BST at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        WorkBase_TreeNodeFree_e320((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        WorkBase_TreeNodeFree_e320((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        System::MemoryAllocator::MemFree(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor 0x1400f4770 (factory variant): `??0Internal`, the Work base fields, the leaf
// fields zeroed (the two stamps are the compiler's).
PlantArea::PlantArea()
    : Work(), field_48(nullptr), field_50(0), field_58(nullptr), field_60(0), _pad64{},
      field_68(nullptr) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_PlantArea_Vtable` (slot 1 null, slot 2 a lazily
// cached id), PlantArea_Ctor_Impl (the ctor with both stamps written by hand) and
// REGISTER_TYPE over PlantArea_Factory / _CreateInPlace, plus two vtable-pointer
// statics for the dtor's stamps.

// create 0x14002c470: MemAlloc(sizeof, 8), then the ctor.
void* PlantArea::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PlantArea), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PlantArea());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* PlantArea::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PlantArea());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(PlantArea, "Work")
