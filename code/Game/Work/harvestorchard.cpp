// code/Game/Work/harvestorchard.cpp
// Clean C++ reimplementation of HarvestOrchard Work class.
// Source: Game/Work/HarvestOrchard.cpp (from profile build debug strings)
//
// HarvestOrchard (0x68 bytes) -- Orchard harvest work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestOrchard
// Vtable: 0x140161520
//
// Same struct layout as HarvestArea but different harvest logic:
// - Orchards use a different description component (OrchardDescription at DAT_1401c46b0)
// - Slot4 only harvests single trees (no paired cell logic like HarvestArea)
// - Slot4 uses FUN_1400bb7b0 to create harvest action (different from ShowTool_Action_Ctor4)
// - Slot4 also looks up RawMaterial description and passes extra params
// - Slot6/Slot7 use simple int key (not packed high/low 16-bit)
//
// Methods implemented:
//   Slot4 (0x1400ED8C0), Slot6 (0x1400ED880), Slot7 (0x1400ED810)
//   Slot8 (0x1400EDDC0), Slot9 (0x1400ED5E0), Slot10 (0x1400EDE40)
//   Serialize (0x1400EDEA0), ScalarDtor body (0x1400EDF60)
//

#include "Work/HarvestOrchard.h"
#include "Work/harvestorchard_funcs.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_140172114
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <new>
#include "Component/OrchardDescription.h"   // ENDGAME B.3.9 D5: OrchardDescription::TypeId()
#include "Component/FruitDescription.h"     // ENDGAME B.3.9 D5: FruitDescription::TypeId()

// Diagnostics-only counters (standalone/orchard_diag.cpp). No-op unless
// BANISHED_ORCHARD_DIAG is set; never change behaviour.
extern "C" void OrchardDiag_NoteHarvestConstructed();
extern "C" void OrchardDiag_NoteHarvestExecuted();

// ============================================================
// Sub-functions, called by name (B3R; Work/Action/Component/Tutorial/Game headers)
// ============================================================
//   AutoPickupWork::CheckAssignments (0x1400e3db0)  WorkGrid_AutoPickup_e3db0
//   0x140003050 FindComponent MaterialDesc          WorkBase_FindComponent_3050
//   0x140002f90 FindComponent MeshInstance          WorkBase_FindComponent_2f90
//   0x140005440 FindComponent OrchardDescription    WorkAcc_FindComponent_5440
//   0x1400052c0 FindComponent NaturalResource       WorkBase_FindComponent_52c0
//   0x14000a0b0 FindComponent (description comp)    WorkLeaf_FindComp_a0b0
//   0x14012c3f0 MeshInstance set visible            CitizenComp_FUN_14012c3f0
//   0x14012e5c0 CalcMaxWorkers                      WorkComponent_CalcMaxWorkers
//   0x14009ccf0 Map_FindNearestPassable             WorkGrid_FindCell_9ccf0
//   0x1400a00f0 Map_CanPathTo                       WorkBase_ValidateCell_a00f0
//               (a uint64 whose low byte is the bool; read through uint8_t)
//   0x14009f3e0 GetTerrainPos                       Reimpl_MapGetTerrainPos
//   0x140006330 FindComponent RawMaterial           WorkAcc_FindComponent_6330
//   0x1400bb7b0 orchard harvest action ctor         WorkAct_GatherWorkCtor_bb7b0
//   0x140029a40 / 0x14001ca60 / 0x140029de0 cell BST insert / remove / serialize
//                                                   WorkCellBST_Insert_9a40 / _Remove_ca60 / _Serialize_9de0
//   0x1400f7100 ValidateTask                        WorkBase_ValidateTask_f7100
//   0x1400c5400 cancel work                         WorkBase_TaskListCleanup_c5400
//   0x140002e10 FindPositionComponent               AttendanceComponent_GetJobHostComponent
//   0x14000e320 BST recursive free                  WorkBase_TreeNodeFree_e320
//   MemAlloc                                        System::MemoryAllocator::MemAlloc
//   DAT_140172114 = 0.5f                            kRdata_140172114


// ============================================================
// Helper: BST search for int key
// ============================================================
static void* BST_Find(void* root, int key) {
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
// HarvestOrchard::Slot4 -- Main orchard harvest assignment logic
// ============================================================
//
// Key differences from HarvestArea:
// - Uses OrchardDescription component (DAT_1401c46b0) instead of CropDescription
// - No paired cell harvesting (single tree per assignment)
// - Checks NaturalResource growth flags + description type
// - Uses FUN_1400bb7b0 to create harvest action (orchard-specific)
// - Passes raw material ref and material count to action constructor
// - Orchard cell data at +0x50 with stride 0x28 (not +0x58 with stride 0x20)
//
int HarvestOrchard::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // Step 1: Check auto-pickup
    uintptr_t checkRef = citizenRef;
    int checkResult = WorkGrid_AutoPickup_e3db0(selfAddr, &checkRef);
    if (checkResult == 2) {
        return 2;
    }

    // Step 2: Find components
    uintptr_t* workEntityPtr = (uintptr_t*)(selfAddr + 0x10);
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*workEntityPtr, &matDescIBP);

    uintptr_t orchardDescIBP[2] = {0};
    WorkAcc_FindComponent_5440(*workEntityPtr, orchardDescIBP);

    // Get OrchardDescription object
    void* descObj = *(void**)(orchardDescIBP[0] + 0x18);
    bool isDerived = Object::IsDerivedFromNullSafe(descObj, OrchardDescription::TypeId());
    void* orchardDesc = isDerived ? descObj : nullptr;

    // Check tree count vs max workers
    int treeCount = (int)selfQ[10]; // _field50
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Check capacity
    int materialIdx = *(int*)((uintptr_t)orchardDesc + 0x20);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    uintptr_t meshIBP[2] = {0};
    WorkBase_FindComponent_2f90(*workEntityPtr, meshIBP);
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>((void*)(*(uintptr_t*)meshIBP)), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // Step 3: Get map and citizen position
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;
    uint32_t citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Step 4: Iterate orchard cells
    int fieldCellCount = (int)selfQ[0xc]; // _field60 as cell count
    [[maybe_unused]] float cellFloat = 0.0f;

    if (fieldCellCount > 0) {
        for (int i = 0; i < fieldCellCount; i++) {
            char* validityArray = (char*)selfQ[0xb]; // _field58
            if (validityArray[i] == 0) {
                goto next_cell;
            }

            // Check if cell is already in BST
            {
                int* bstRoot = (int*)selfQ[9]; // _field48
                void* found = BST_Find(bstRoot, i);
                if (found != nullptr) {
                    goto next_cell;
                }
            }

            // Look up entity at this cell
            {
                uintptr_t orchDescVal = orchardDescIBP[0];
                uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20);
                // Orchard cell data stride is 0x28 (different from HarvestArea's 0x20)
                int cellGridIdx = *(int*)(*(uintptr_t*)(orchDescVal + 0x50) + (uintptr_t)i * 0x28);
                uintptr_t entityAtCell = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx * 0x30 + 8);

                if (entityAtCell == 0) {
                    validityArray[i] = 0;
                } else {
                    // Check pathfinding
                    int srcCellIdx = *(int*)(*(uintptr_t*)(orchardDescIBP[0] + 0x50) + (uintptr_t)i * 0x28);
                    int nearestCell = WorkGrid_FindCell_9ccf0(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);

                    if (nearestCell >= 0) {
                        uint64_t canPath = (uint64_t)static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, citizenGridCell, nearestCell));   // the bool door read AL
                        if ((char)canPath != 0) {
                            // Check NaturalResource component
                            uintptr_t natResIBP = 0;
                            WorkBase_FindComponent_52c0(entityAtCell, &natResIBP);

                            if (natResIBP == 0) {
                                validityArray[i] = 0;
                            } else {
                                // Check flag bit 20 (flagged for removal)
                                if ((*(uint32_t*)(natResIBP + 0x20) >> 20) & 1) {
                                    goto next_cell;
                                }

                                // Get description component from entity
                                uintptr_t descCompIBP = 0;
                                WorkLeaf_FindComp_a0b0(entityAtCell, &descCompIBP);

                                void* descCompObj = *(void**)(descCompIBP + 0x18);
                                // DAT_1401c4288 = FruitDescription (ENDGAME B.3.9 D5: by
                                // cell, not the legacy variable name "DescComp").
                                bool isDescDerived = Object::IsDerivedFromNullSafe(descCompObj, FruitDescription::TypeId());
                                void* descComp = isDescDerived ? descCompObj : nullptr;

                                // Get raw material reference
                                uintptr_t rawMatRef = *(uintptr_t*)((uintptr_t)descComp + 0x30);
                                uintptr_t rawMatIBP = 0;
                                WorkAcc_FindComponent_6330(rawMatRef, &rawMatIBP);

                                // Arg 7 of the action ctor. Read from the DESCRIPTION-COMPONENT
                                // holder (RBP-0x39, the 0x14000a0b0 finder's out-param), NOT the
                                // orchard component, and as a FLOAT:
                                //   1400edc74  MOV RAX,[RBP-0x39] ; 1400edc7f MOVSS XMM6,[RAX+0x30]
                                //   1400edd04  MOVSS [RSP+0x30],XMM6      <- 7th argument slot
                                // It used to be read as a uint32 off orchardDescIBP — wrong object
                                // and wrong type, so the callee (which takes a float here, and
                                // forwards it to CreateRawMaterial in XMM2) got an integer bit
                                // pattern from an unrelated field.
                                float rawMatAmount = *(float*)(descCompIBP + 0x30);

                                // Calculate world position
                                float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
                                Math::Vector3 targetPos(
                                    (float)(nearestCell % *mapGrid) - halfOffset,
                                    (float)(nearestCell / *mapGrid) - halfOffset,
                                    0.0f);

                                float pathPos[4];
                                Reimpl_MapGetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

                                // Get entity ref from orchard description
                                uintptr_t entityRef = *(uintptr_t*)(orchardDescIBP[0] + 0x10);

                                // Create orchard harvest action
                                uintptr_t citizenVal = *(uintptr_t*)citizenIBP;
                                uintptr_t actionBuf[2] = { citizenVal, 0 };
                                // Arg 6 is the PARITY of the slot index, not "is this the first
                                // cell". Retail: 1400edd00 MOVZX EAX,SIL ; 1400edd0a NOT AL ;
                                // 1400edd14 AND AL,1 — with ESI = i, that is `!(i & 1)`. The old
                                // `(i == 0) ? 0 : 1` was both the wrong test and inverted.
                                char altCellFlag = (char)(~i & 1);

                                {   // arg 6 gates exactly ONE node of the action chain the
                                    // ctor builds: CreateRawMaterial (verified in retail
                                    // 0x1400bb7b0 — `if (param_6 != 0)` wraps the
                                    // MemAlloc(0x58) + CreateRawMaterial_Action_CopyCtor).
                                    // So on ODD slot indices the citizen picks the tree and
                                    // reports it, but spawns no produce. That is retail's
                                    // shape (MOVZX EAX,SIL / NOT AL / AND AL,1 at
                                    // 0x1400edd00, ESI = the loop index) — this trace exists
                                    // to measure what it COSTS, since "half the trees yield
                                    // nothing" and "yield is batched two trees per pile"
                                    // look identical from the outside.
                                    static int s_n = 0;
                                    if (s_n++ < 24) {
                                        fprintf(stderr, "[OrchardHarvest] slot=%d cell=%u "
                                                "amount=%.3f createsProduce=%d\n",
                                                i, (unsigned)nearestCell, (double)rawMatAmount,
                                                (int)altCellFlag);
                                        fflush(stderr);
                                    }
                                }

                                WorkAct_GatherWorkCtor_bb7b0(
                                    actionBuf,                     // citizenIBP
                                    (uint32_t)selfQ[4],            // work id / priority (self+0x20)
                                    reinterpret_cast<int64_t*>(&entityRef),                    // entity ref ptr
                                    pathPos,                       // position
                                    rawMatRef,                     // raw material ref
                                    altCellFlag,                   // alternating-cell flag
                                    rawMatAmount,                  // float, -> CreateRawMaterial XMM2
                                    (uint32_t)nearestCell,         // map cell index
                                    (uint32_t)i);                  // SLOT index — the value that
                                                                   // comes back as the harvested-slot
                                                                   // id (CompleteTask p5 /
                                                                   // SendComponentMessage p6). This
                                                                   // used to be `cellFloat`, a float
                                                                   // in an integer slot, so every
                                                                   // completion reported the same
                                                                   // wrong slot. See
                                                                   // reference_int_arg_typed_as_float.

                                // Assign work and register with AI
                                int priority = (int)selfQ[4];
                                uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

                                uintptr_t workEntIBP[2] = {0};
                                uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
                                uintptr_t workEnt = *wResult;

                                *(uint32_t*)(citizenEnt + 0x54) |= 4;
                                *(int32_t*)(citizenEnt + 0x30) = priority;
                                *(uint32_t*)(citizenEnt + 0xb0) = 0;

                                if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                                    *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                                }

                                // Insert into BST
                                uintptr_t insertResult[2] = {0};
                                uint32_t keyBuf[4] = { (uint32_t)i, 0, 0, 0 };
                                uintptr_t citizenIBPVal = *(uintptr_t*)citizenIBP;
                                keyBuf[2] = (uint32_t)citizenIBPVal;
                                keyBuf[3] = (uint32_t)(citizenIBPVal >> 32);
                                WorkCellBST_Insert_9a40(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), insertResult, keyBuf);

                                // Mark cell as used
                                validityArray[i] = 0;

                                OrchardDiag_NoteHarvestExecuted();
                                return 2;
                            }
                        }
                    }
                }
            }

        next_cell:
            cellFloat = (float)(i + 1);
        }
    }

    return 1;
}


// ============================================================
// HarvestOrchard::Slot6 -- task COMPLETED (retail 0x1400ed880)
// ============================================================
// Simpler than HarvestArea -- single int key, no paired cell.
//
// ⚠ This slot must NOT restore the validity byte. Retail 0x1400ed880 is exactly
// "walk the BST, remove the node if found, XOR AL,AL, RET" — there is no store to
// [self+0x58] anywhere in the function. Only the CANCEL slot (0x1400ed810, idx 7)
// writes validityArray[param4] = 1, and it does so on BOTH exits. The distinction is
// the whole point: a cancelled pick goes back in the pool, a COMPLETED one does not.
//
// This port had Slot7's body copied into Slot6, so every completed pick immediately
// re-marked its tree as available. Slot4 scans from i=0 and takes the lowest valid
// index, so one worker re-harvested slot 0 forever: 85 assignments against a single
// distinct picked slot, `picked` stuck at 1/12, and state 11 could never reach its
// `picked >= live` latch. The [OrchardGrow] harness corroborated it — exactly one of
// twelve trees was repeatedly zeroed and re-ripened.
int HarvestOrchard::OnComplete(void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Search BST for key and remove if found
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

    // No validity-byte write here — see the header note. Retail: XOR AL,AL ; RET.
    return 0;
}


// ============================================================
// HarvestOrchard::Slot7 -- task CANCELLED (retail 0x1400ed810)
// ============================================================
// The mirror of Slot6: same BST walk, but it DOES restore the validity byte, on both
// the found and not-found exits (0x1400ed84d and 0x1400ed865). Cancelling a pick
// returns that tree to the pool.
int HarvestOrchard::OnCancel(void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Search BST for key and remove if found
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

    // Mark cell as valid
    char* validityArray = (char*)selfQ[0xb];
    validityArray[param4] = 1;
    return 0;
}


// ============================================================
// HarvestOrchard::Slot8 -- CancelAll
// ============================================================
int HarvestOrchard::Cleanup() {
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
// HarvestOrchard::Slot9 -- RemoveInvalid
// ============================================================
// Identical structure to HarvestArea::Slot9.
// Hook DISABLED (bug class 2: MOV RAX,RSP in prologue).
int HarvestOrchard::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    int* collectArray = nullptr;
    int* currentArray = nullptr;
    uint32_t collectCount = 0;
    uint32_t collectCapacity = 0;

    int* root = (int*)(selfAddr + 0x48);
    int* node = *(int**)root;

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

    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2);

        uint8_t valid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), uintptr_t{0}, (void*)&ibpData, nullptr);

        if (valid == 0) {
            uint32_t newCount = collectCount + 1;
            if (newCount != collectCount) {
                if ((int)collectCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (collectCount != 0) {
                        newCap = newCount * 3;
                    }
                    collectCapacity = newCap;
                    int* newArray = (int*)System::MemoryAllocator::MemAlloc((int64_t)newCap << 2, 4);
                    currentArray = newArray;

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
                } else {
                    currentArray = collectArray;
                    collectArray[collectCount] = *node;
                }
                collectCount = newCount;
            }

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
    if ((int)collectCount > 0) {
        for (uint32_t k = 0; k < collectCount; k++) {
            int* bstNode = *(int**)root;
            while (bstNode != nullptr) {
                if (collectArray[k] < *bstNode) {
                    bstNode = *(int**)(bstNode + 6);
                } else if (collectArray[k] > *bstNode) {
                    bstNode = *(int**)(bstNode + 8);
                } else {
                    WorkCellBST_Remove_ca60(reinterpret_cast<uintptr_t*>(root), bstNode);
                    break;
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(currentArray ? currentArray : collectArray);
    return collectCount == 0 ? 1 : 0;
}


// ============================================================
// HarvestOrchard::Slot10 -- GetWorkEntity
// ============================================================
void* HarvestOrchard::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t posCompIBP[2] = {0};
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entity), posCompIBP);

    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCompIBP + 0x48);

    return outPtr;
}


// ============================================================
// HarvestOrchard::Serialize
// ============================================================
void HarvestOrchard::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    Work::Persist(stream);   // 0x1400f7400, a direct call in retail — QUALIFIED here

    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge

    alignas(8) uint8_t serializerBuf2[16];
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    WorkCellBST_Serialize_9de0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializerBuf2);

    int cellCount = *(int*)(selfAddr + 0x60);
    int cellCountBuf[2] = { cellCount, 0 };
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->Serialize(*reinterpret_cast<int*>(cellCountBuf), nullptr);   // B.3.4 bridge

    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf2)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::U8>((void*)(selfAddr + 0x58)).SetSize(cellCountBuf[0]);
    }

    // Serialize validity array through the BOOL-ARRAY door.
    //
    // ENDGAME B.3.4 callers batch H — SETTLED FROM RETAIL. HarvestOrchard's
    // Serialize (Application-x64.exe 0x1400edea0) is instruction-for-instruction
    // the same as HarvestArea's at this site:
    //     1400edf2d  XOR R9D,R9D                    ; name = nullptr
    //     1400edf30  MOV R8,qword ptr [RDI + 0x58]  ; the buffer
    //     1400edf34  MOV EDX,dword ptr [RSP + 0x60] ; the count
    //     1400edf38  LEA RCX,[RSP + 0x28]           ; the Serializer
    //     1400edf3d  CALL qword ptr [0x140151458]
    // 0x140151458 is the four-parameter (int n, bool*, const char*) array door,
    // not the three-parameter `Serialize(int&)` scalar door 0x140152ac8 that
    // the old `s_fn_Serializer_SerializeBoolArray` alias pointed at (removed with
    // its door in the B.3.4a closing sweep). See
    // harvestarea.cpp for the full argument.
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->Serialize(
        cellCountBuf[0], reinterpret_cast<bool*>(*(void**)(selfAddr + 0x58)), nullptr);   // B.3.4 bridge  (slot 0x140151458 = bool-array door)

    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();   // B.3.4 bridge
}


// The HarvestOrchard / AutoPickupWork / Work stamps and the `~Internal` call are the
// compiler's chain.
HarvestOrchard::~HarvestOrchard() {
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



// [11] 0x140029860 (shared by 13 tables): `+0x50 > 0` — "is anyone assigned". The
// old table pointed at Work_Slot11_HasActiveTasks, the same body.
int HarvestOrchard::GetPriority() {
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's.
HarvestOrchard::HarvestOrchard()
    : AutoPickupWork(), _field48(nullptr), _field50(0), _field58(nullptr), _field60(nullptr) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_HarvestOrchard_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over HarvestOrchard_Factory / _CreateInPlace (both
// Work_GenericCtor + the orchard diagnostic's construction count). Nothing outside this
// block reads 0x140161520 back (the from-entity ctor that stamped it is the real ctor
// now), so its registration goes with the table.

// create 0x14002a520: MemAlloc(sizeof, 8), then the ctor.
void* HarvestOrchard::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HarvestOrchard), 8);
    if (!p) return nullptr;
    Object::Base* r = new (p) HarvestOrchard();
    OrchardDiag_NoteHarvestConstructed();
    return r;
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* HarvestOrchard::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    Object::Base* r = new (storage) HarvestOrchard();
    OrchardDiag_NoteHarvestConstructed();
    return r;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(HarvestOrchard, "AutoPickupWork")
