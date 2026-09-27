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
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <new>

// Diagnostics-only counters (standalone/orchard_diag.cpp). No-op unless
// BANISHED_ORCHARD_DIAG is set; never change behaviour.
extern "C" void OrchardDiag_NotePlantConstructed();
extern "C" void OrchardDiag_NotePlantExecuted();

// ============================================================
// Local function pointers (resolved once by PlantOrchard_ResolveFuncs)
// ============================================================

// All typedefs prefixed with POW_ to avoid conflicts

typedef void* (*POW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*POW_FindComp_OrchardDesc_t)(uintptr_t entity, void* outIBP);  // 0x140005440

static POW_FindComp_MatDesc_t       s_fn_FindComp_MatDesc = nullptr;
static POW_FindComp_OrchardDesc_t   s_fn_FindComp_OrchardDesc = nullptr;

typedef int (*POW_CalcMaxWorkers_t)(uintptr_t matDesc);
static POW_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr;

typedef int  (*POW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool (*POW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
static POW_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr;
static POW_Map_CanPathTo_t          s_fn_Map_CanPathTo = nullptr;

typedef float* (*POW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                      char param4, uint32_t* outNormal);
static POW_GetTerrainPos_t s_fn_GetTerrainPos = nullptr;

// Orchard planting action ctor (FUN_1400be6e0) -- 11 parameters.
// ⚠ cellIdx / cellIdx2 are INTEGERS, not floats. The call site stores them with
// `MOV dword ptr [RSP+0x30],EBX` (0x1400f4dd1) and `MOV dword ptr [RSP+0x50],EBX`
// (0x1400f4daf), where EBX is the loop counter — there is no CVTSI2SS anywhere near them.
// Typing them float here reinterpreted the index as its float BIT PATTERN: slot 0 still
// came out 0, but slot 1 became 0x3F800000, so the completion message that binds a planted
// tree into its orchard slot (OrchardComponent::OnMessage msg 0, param p1 = slot index)
// addressed an absurd index instead of slot 1.
typedef void (*POW_PlantAction_Ctor_t)(void* citizenIBP, int priority, void* externalPtr,
                                        uint32_t zero, float* position, int srcCellIdx,
                                        uint32_t cellIdx, void* entityRefPtr, uint32_t* typeConstPtr,
                                        uint64_t unused, uint32_t cellIdx2);
static POW_PlantAction_Ctor_t s_fn_PlantAction_Ctor = nullptr;  // 0x1400be6e0

// BST operations
typedef void* (*POW_BST_Insert_t)(void* treeRoot, void* outResult, uint32_t* keyData);
typedef void  (*POW_BST_Remove_t)(void* treeRoot, void* node);
typedef void  (*POW_BST_Serialize_t)(void* treeRoot, void* serializer);
typedef void  (*POW_BST_RecursiveFree_t)(void* treeRoot, void* node);
static POW_BST_Insert_t       s_fn_BST_Insert = nullptr;
static POW_BST_Remove_t       s_fn_BST_Remove = nullptr;
static POW_BST_Serialize_t    s_fn_BST_Serialize = nullptr;
static POW_BST_RecursiveFree_t s_fn_BST_RecursiveFree = nullptr;

typedef uint8_t (*POW_ValidateTask_t)(uintptr_t self, uintptr_t param2, void* ibp, void** outTask);
static POW_ValidateTask_t s_fn_ValidateTask = nullptr;

typedef void (*POW_TaskCancel_t)(void* taskList);
static POW_TaskCancel_t s_fn_TaskCancel = nullptr;

typedef void* (*POW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
static POW_FindSubEntity_t s_fn_FindSubEntity = nullptr;

typedef void (*POW_Work_Serialize_t)(void* self, void* stream);

typedef void (*POW_ExternalRef_Serialize_t)(void* refPtr, void* serializer);
static POW_ExternalRef_Serialize_t s_fn_ExternalRef_Serialize = nullptr;

// 0x14000a590 (Array<uint8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::U8> member call, so its resolved-pointer stand-in
// is gone.

typedef void* (*POW_MemAlloc_t)(int64_t size, int align);
static POW_MemAlloc_t s_fn_MemAlloc = nullptr;
static void* POW_MemAllocViaSingleton(int64_t size, int align) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, align) : nullptr;
}

// Orchard slot refresh (FUN_140051830)
typedef void (*POW_OrchardRefresh_t)(uintptr_t orchardDescComp);
static POW_OrchardRefresh_t s_fn_OrchardRefresh = nullptr;  // 0x140051830

// Float constant
static float* s_fHalfCellOffset = nullptr;

// Type constant (DWORD_1401c46b0 -- orchard type marker)
static uint32_t* s_typeConst = nullptr;

// Vtable pointers

static bool s_po_funcs_resolved = false;


// ============================================================
// Resolution
// ============================================================
bool PlantOrchard_ResolveFuncs() {
    if (s_po_funcs_resolved) return true;

    s_fn_FindComp_MatDesc = reinterpret_cast<POW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_OrchardDesc = reinterpret_cast<POW_FindComp_OrchardDesc_t>(
        ResolveAddr(Module::Exe, 0x140005440));
    s_fn_CalcMaxWorkers = reinterpret_cast<POW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Map_FindNearestPassable = reinterpret_cast<POW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<POW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<POW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_PlantAction_Ctor = reinterpret_cast<POW_PlantAction_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400be6e0));
    s_fn_BST_Insert = reinterpret_cast<POW_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140029a40));
    s_fn_BST_Remove = reinterpret_cast<POW_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));  // was 0x140029a60 (mid-function; true remove entry is 0x14001ca60)
    s_fn_BST_Serialize = reinterpret_cast<POW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140029de0));
    s_fn_BST_RecursiveFree = reinterpret_cast<POW_BST_RecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x14000e320));
    s_fn_ValidateTask = reinterpret_cast<POW_ValidateTask_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TaskCancel = reinterpret_cast<POW_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<POW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_ExternalRef_Serialize = reinterpret_cast<POW_ExternalRef_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_fn_OrchardRefresh = reinterpret_cast<POW_OrchardRefresh_t>(
        ResolveAddr(Module::Exe, 0x140051830));

    // Dispatch slot 0x140152780 is MISLABELED "MemAlloc" (it is Vector2::Max, see
    // reg_player_screenrect.cpp) and is deliberately left unregistered, so the old
    // `*ResolveAddr(...)` read here yielded NULL and the collect-array grow in the
    // Validate slot called it: EXEC of 0 the first harvest season a field had stale
    // tasks (BANISHED_STRESS town at 10x, 2026-09-03). Allocate through the real
    // allocator singleton instead; fn_MemFreeStatic pairs with it.
    s_fn_MemAlloc = &POW_MemAllocViaSingleton;

    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_typeConst = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));


    bool ok = s_fn_FindComp_MatDesc && s_fn_FindComp_OrchardDesc &&
              s_fn_CalcMaxWorkers && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_GetTerrainPos &&
              s_fn_PlantAction_Ctor &&
              s_fn_BST_Insert && s_fn_BST_Remove &&
              s_fn_BST_Serialize && s_fn_BST_RecursiveFree &&
              s_fn_ValidateTask && s_fn_TaskCancel &&
              s_fn_FindSubEntity && s_fn_ExternalRef_Serialize && s_fn_OrchardRefresh &&
              fn_MemFreeStatic &&
              g_pGameWorldSingleton && s_fHalfCellOffset &&
              s_typeConst;

    s_po_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[PlantOrchard] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_OrchardDesc) printf("  MISSING: FindComp_OrchardDesc\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_PlantAction_Ctor) printf("  MISSING: PlantAction_Ctor\n");
        if (!s_fn_BST_Insert) printf("  MISSING: BST_Insert\n");
        if (!s_fn_BST_Remove) printf("  MISSING: BST_Remove\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_BST_RecursiveFree) printf("  MISSING: BST_RecursiveFree\n");
        if (!s_fn_ValidateTask) printf("  MISSING: ValidateTask\n");
        if (!s_fn_TaskCancel) printf("  MISSING: TaskCancel\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_ExternalRef_Serialize) printf("  MISSING: ExternalRef_Serialize\n");
        if (!s_fn_OrchardRefresh) printf("  MISSING: OrchardRefresh\n");
        // The seven conditions below were in the `ok` expression but not in this list, so a
        // failure caused by any of them printed "FAILED" with no reason at all — which is
        // exactly what a 2026-08-10 orchard run hit. Report every term you test.
        if (!fn_MemFreeStatic) printf("  MISSING: MemFreeStatic\n");
        if (!g_pGameWorldSingleton) printf("  MISSING: g_pGameWorldSingleton\n");
        if (!s_fHalfCellOffset) printf("  MISSING: fHalfCellOffset (0x140172114)\n");
        if (!s_typeConst) printf("  MISSING: typeConst (0x1401c46b0 OrchardComponent type id)\n");
    }
#endif

    return ok;
}


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
int PlantOrchard::Execute(void* entityIBP, void* /*context*/, uint64_t /*arg3*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Step 1: Find components
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    uintptr_t orchardDescIBP = 0;
    s_fn_FindComp_OrchardDesc(*(uintptr_t*)(selfAddr + 0x10), &orchardDescIBP);

    // Step 2: Refresh orchard slot state
    s_fn_OrchardRefresh(orchardDescIBP);

    // Step 3: Check max workers
    int treeCount = (int)selfQ[10];  // self->field_50
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Step 4: Get map and citizen position
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
            int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);
            if (nearestCell < 0) {
                continue;
            }

            bool canPath = s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
            if (!canPath) {
                continue;
            }

            // Valid spot found -- create orchard planting action
            uint32_t typeConstVal = *s_typeConst;
            uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

            float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
            Math::Vector3 targetPos(
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            float pathPos[4];
            s_fn_GetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

            // Get citizen entity
            uintptr_t citizenBuf = *(uintptr_t*)entityIBP;

            // Call FUN_1400be6e0 orchard planting action ctor
            s_fn_PlantAction_Ctor(
                &citizenBuf,                      // RCX: citizenIBP
                (int)selfQ[4],                    // EDX: priority
                (void*)selfQ[0xd],                // R8: self->field_68 (ExternalPtr)
                0,                                // R9D: 0
                pathPos,                          // [RSP+0x20]: position
                srcCellIdx,                       // [RSP+0x28]: srcCellIdx
                (uint32_t)i,                      // [RSP+0x30]: cellIdx (integer, EBX)
                &entityRef,                       // [RSP+0x38]: entity ref from field_10
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
            s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, insertKey);

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
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
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
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
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
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// PlantOrchard::Validate (Slot9) -- 0x1400f47f0
// RemoveInvalid: identical pattern to PlantArea, but marks cells with 0.
// ============================================================
int PlantOrchard::Validate(void* param2) {
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

        uint8_t valid = s_fn_ValidateTask(selfAddr, (uintptr_t)param2, (void*)&ibpData, nullptr);

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
                    newArray = (int*)s_fn_MemAlloc((int64_t)newCap << 2, 4);
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
                    fn_MemFreeStatic(collectArray);
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
                    s_fn_BST_Remove(bstRootPtr, bstNode);
                    break;
                }
            }
        }
    }

    fn_MemFreeStatic(currentArray ? currentArray : collectArray);
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
    void* result = s_fn_FindSubEntity(entity, subIBP);

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

    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

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

    s_fn_ExternalRef_Serialize((void*)(selfAddr + 0x68), serializerBuf);

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// PlantOrchard::DtorBody -- 0x1400f5010
// Identical structure to PlantArea dtor.
// ============================================================
// PlantOrchard dtor 0x1400f5010. The stamps and the `~Internal` call are the compiler's chain.
PlantOrchard::~PlantOrchard() {
    uintptr_t selfAddr = (uintptr_t)this;

    fn_MemFreeStatic(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        fn_MemFreeStatic(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}


// ============================================================
// PlantOrchard::ScalarDtor -- 0x14002c540
// ============================================================
// [0] 0x14002c540: ~PlantOrchard, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* PlantOrchard::DeletingDtor(unsigned flags) {
    this->~PlantOrchard();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}



// Ctor 0x1400f50b0 (deserialize/factory variant): `??0Internal`, the Work base fields,
// the leaf fields zeroed (+0x60/+0x64 as the one qword the old port wrote; the two
// stamps are the compiler's). PlantOrchard_ResolveFuncs wires the file's helper
// pointers and the orchard diagnostic counts the construction (the old factory ctor
// did both).
PlantOrchard::PlantOrchard()
    : Work(), field_48(nullptr), field_50(0), field_58(nullptr), field_60(0), field_64(0),
      field_68(nullptr) {
    PlantOrchard_ResolveFuncs();
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

DEFINE_EXE_VTABLE(PlantOrchard, "_ZTV12PlantOrchard")

#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(PlantOrchard, "Work")
