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
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/plantorchard_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

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

typedef float* (*POW_Vector3_Ctor_t)(float* self, float x, float y, float z);
static POW_Vector3_Ctor_t s_fn_Vector3_Ctor = nullptr;

// Orchard planting action ctor (FUN_1400be6e0) -- 11 parameters
typedef void (*POW_PlantAction_Ctor_t)(void* citizenIBP, int priority, void* externalPtr,
                                        uint32_t zero, float* position, int srcCellIdx,
                                        float cellIdx, void* entityRefPtr, uint32_t* typeConstPtr,
                                        uint64_t unused, float cellIdx2);
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
static POW_Work_Serialize_t s_fn_Work_Serialize = nullptr;

typedef void* (*POW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*POW_Serializer_Dtor_t)(void* self);
static POW_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
static POW_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*POW_Serializer_SerializeInt_t)(void* ser, int* buf, const char* name);
static POW_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

typedef void (*POW_Serializer_SerializeBoolArray_t)(void* ser, int count, void* buf, const char* name);
static POW_Serializer_SerializeBoolArray_t s_fn_Serializer_SerializeBoolArray = nullptr;

typedef void (*POW_ExternalRef_Serialize_t)(void* refPtr, void* serializer);
static POW_ExternalRef_Serialize_t s_fn_ExternalRef_Serialize = nullptr;

typedef void (*POW_ResizeByteArray_t)(void* arrayField, int newCount);
static POW_ResizeByteArray_t s_fn_ResizeByteArray = nullptr;

typedef void* (*POW_MemAlloc_t)(int64_t size, int align);
static POW_MemAlloc_t s_fn_MemAlloc = nullptr;

// Orchard slot refresh (FUN_140051830)
typedef void (*POW_OrchardRefresh_t)(uintptr_t orchardDescComp);
static POW_OrchardRefresh_t s_fn_OrchardRefresh = nullptr;  // 0x140051830

// Float constant
static float* s_fHalfCellOffset = nullptr;

// Type constant (DWORD_1401c46b0 -- orchard type marker)
static uint32_t* s_typeConst = nullptr;

// Vtable pointers
static void** s_vtable_PlantOrchard = nullptr;
static void** s_vtable_Work = nullptr;

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
        ResolveAddr(Module::Exe, 0x140029a60));
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
    s_fn_Work_Serialize = reinterpret_cast<POW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ResizeByteArray = reinterpret_cast<POW_ResizeByteArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_ExternalRef_Serialize = reinterpret_cast<POW_ExternalRef_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_fn_OrchardRefresh = reinterpret_cast<POW_OrchardRefresh_t>(
        ResolveAddr(Module::Exe, 0x140051830));

    // Dispatch slots
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<POW_Vector3_Ctor_t*>(iatVec3Ctor);

    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<POW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<POW_Serializer_Dtor_t*>(iatSerDtor);

    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt)
        s_fn_Serializer_SerializeInt = *reinterpret_cast<POW_Serializer_SerializeInt_t*>(iatSerInt);
    s_fn_Serializer_SerializeBoolArray = reinterpret_cast<POW_Serializer_SerializeBoolArray_t>(
        s_fn_Serializer_SerializeInt);

    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140152780);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<POW_MemAlloc_t*>(iatMemAlloc);

    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_typeConst = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));

    s_vtable_PlantOrchard = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161910));
    s_vtable_Work = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161a80));

    bool ok = s_fn_FindComp_MatDesc && s_fn_FindComp_OrchardDesc &&
              s_fn_CalcMaxWorkers && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_GetTerrainPos &&
              s_fn_Vector3_Ctor && s_fn_PlantAction_Ctor &&
              s_fn_BST_Insert && s_fn_BST_Remove &&
              s_fn_BST_Serialize && s_fn_BST_RecursiveFree &&
              s_fn_ValidateTask && s_fn_TaskCancel &&
              s_fn_FindSubEntity && s_fn_Work_Serialize &&
              s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_SerializeInt && s_fn_ResizeByteArray &&
              s_fn_ExternalRef_Serialize && s_fn_OrchardRefresh &&
              fn_InternalBasePtr_CheckPtr && fn_MemFreeStatic &&
              g_pGameWorldSingleton && s_fHalfCellOffset &&
              s_typeConst && s_vtable_PlantOrchard && s_vtable_Work;

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
        if (!s_fn_Vector3_Ctor) printf("  MISSING: Vector3_Ctor\n");
        if (!s_fn_PlantAction_Ctor) printf("  MISSING: PlantAction_Ctor\n");
        if (!s_fn_BST_Insert) printf("  MISSING: BST_Insert\n");
        if (!s_fn_BST_Remove) printf("  MISSING: BST_Remove\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_BST_RecursiveFree) printf("  MISSING: BST_RecursiveFree\n");
        if (!s_fn_ValidateTask) printf("  MISSING: ValidateTask\n");
        if (!s_fn_TaskCancel) printf("  MISSING: TaskCancel\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_SerializeInt) printf("  MISSING: Serializer_SerializeInt\n");
        if (!s_fn_ResizeByteArray) printf("  MISSING: ResizeByteArray\n");
        if (!s_fn_ExternalRef_Serialize) printf("  MISSING: ExternalRef_Serialize\n");
        if (!s_fn_OrchardRefresh) printf("  MISSING: OrchardRefresh\n");
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
int PlantOrchard_Slot4_Impl(PlantOrchard* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Step 1: Find components
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t orchardDescIBP = 0;
    s_fn_FindComp_OrchardDesc(*(uintptr_t*)(selfAddr + 0x10), &orchardDescIBP);

    // Step 2: Refresh orchard slot state
    fn_InternalBasePtr_CheckPtr(&orchardDescIBP);
    s_fn_OrchardRefresh(orchardDescIBP);

    // Step 3: Check max workers
    int treeCount = (int)selfQ[10];  // self->field_50
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // Step 4: Get map and citizen position
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;
    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(aiComp + 0x68));
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
            fn_InternalBasePtr_CheckPtr(&orchardDescIBP);
            uintptr_t orchardSlotData = *(uintptr_t*)(orchardDescIBP + 0x50);
            if (*(uintptr_t*)(orchardSlotData + slotOffset + 8) != 0) {
                continue;
            }

            // Get grid cell index from orchard description slot data
            fn_InternalBasePtr_CheckPtr(&orchardDescIBP);
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
            float targetPos[3];
            s_fn_Vector3_Ctor(targetPos,
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            float pathPos[4];
            s_fn_GetTerrainPos(mapGrid, pathPos, targetPos, 0, nullptr);

            // Get citizen entity
            fn_InternalBasePtr_CheckPtr(entityIBP);
            uintptr_t citizenBuf = *(uintptr_t*)entityIBP;

            // Call FUN_1400be6e0 orchard planting action ctor
            s_fn_PlantAction_Ctor(
                &citizenBuf,                      // RCX: citizenIBP
                (int)selfQ[4],                    // EDX: priority
                (void*)selfQ[0xd],                // R8: self->field_68 (ExternalPtr)
                0,                                // R9D: 0
                pathPos,                          // [RSP+0x20]: position
                srcCellIdx,                       // [RSP+0x28]: srcCellIdx
                (float)i,                         // [RSP+0x30]: cellIdx as float
                &entityRef,                       // [RSP+0x38]: entity ref from field_10
                &typeConstVal,                    // [RSP+0x40]: type constant
                0,                                // [RSP+0x48]: unused
                (float)i                          // [RSP+0x50]: cellIdx again
            );

            // Assign work
            int priority = (int)selfQ[4];
            fn_InternalBasePtr_CheckPtr(entityIBP);
            uintptr_t citizenEnt = *(uintptr_t*)entityIBP;

            uintptr_t vtable = *(uintptr_t*)self;
            typedef uintptr_t* (*GetTarget_t)(PlantOrchard*, void*);
            auto getTarget = *(GetTarget_t*)(vtable + 0x50);
            uint32_t resultBuf[4] = {0};
            uintptr_t* wResult = getTarget(self, resultBuf);
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
            // BST node key: first 4 bytes = cell index (float bits), next 8 bytes = citizen IBP
            *(float*)&insertKey[0] = (float)i;
            insertKey[1] = 0;
            insertKey[2] = (uint32_t)citizenIBPVal;
            insertKey[3] = (uint32_t)(citizenIBPVal >> 32);
            s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, insertKey);

            // Mark cell as planted (not available)
            validityArray[i] = 1;

            return 2;
        }
    }

    return 1;
}


// ============================================================
// PlantOrchard::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool PlantOrchard_Execute_Impl(PlantOrchard* /*self*/) {
    return false;
}


// ============================================================
// PlantOrchard::OnComplete (Slot6) -- 0x1400f4a90
// Removes BST entry, marks cell available (sets validity to 0).
// param4 is plain int cell index (not packed like PlantArea).
// ============================================================
int PlantOrchard_OnComplete_Impl(PlantOrchard* self, void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

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
int PlantOrchard_OnCancel_Impl(PlantOrchard* self, void* /*param2*/, void* /*param3*/, int param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

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
void PlantOrchard_Cleanup_Impl(PlantOrchard* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18);
        }

        fn_InternalBasePtr_CheckPtr((void*)(rightmost + 8));
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
}


// ============================================================
// PlantOrchard::Validate (Slot9) -- 0x1400f47f0
// RemoveInvalid: identical pattern to PlantArea, but marks cells with 0.
// ============================================================
bool PlantOrchard_Validate_Impl(PlantOrchard* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

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
void* PlantOrchard_GetTarget_Impl(PlantOrchard* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entity, subIBP);

    fn_InternalBasePtr_CheckPtr(result);
    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)result + 0x48);

    return outPtr;
}


// ============================================================
// PlantOrchard::GetPriority (Slot11) -- shared 0x140029860
// ============================================================
bool PlantOrchard_GetPriority_Impl(PlantOrchard* self) {
    return self->field_50 > 0;
}


// ============================================================
// PlantOrchard::Serialize -- 0x1400f4f50
// Identical to PlantArea::Serialize.
// ============================================================
void PlantOrchard_Serialize_Impl(PlantOrchard* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    s_fn_Work_Serialize(self, stream);

    uint8_t serializerBuf[24] = {0};
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

    int cellCountBuf[2] = { *(int*)(selfAddr + 0x60), 0 };
    s_fn_Serializer_SerializeInt(serializerBuf, cellCountBuf, nullptr);

    if (*(int*)(*(uintptr_t*)serializerBuf + 8) == 0) {
        s_fn_ResizeByteArray((void*)(selfAddr + 0x58), cellCountBuf[0]);
    }

    s_fn_Serializer_SerializeBoolArray(serializerBuf, cellCountBuf[0],
                                       *(void**)(selfAddr + 0x58), nullptr);

    s_fn_ExternalRef_Serialize((void*)(selfAddr + 0x68), serializerBuf);

    s_fn_Serializer_Dtor(serializerBuf);
}


// ============================================================
// PlantOrchard::DtorBody -- 0x1400f5010
// Identical structure to PlantArea dtor.
// ============================================================
void PlantOrchard_DtorBody(PlantOrchard* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    *(void**)selfAddr = s_vtable_PlantOrchard;

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

    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// PlantOrchard::ScalarDtor -- 0x14002c540
// ============================================================
void* PlantOrchard_ScalarDtor_Impl(PlantOrchard* self, uint32_t flags) {
    PlantOrchard_DtorBody(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}
