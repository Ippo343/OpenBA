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

#include "Work/plantarea_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Local function pointers (resolved once by PlantArea_ResolveFuncs)
// ============================================================

// All typedefs prefixed with PAW_ to avoid conflicts

// FindComponent helpers
typedef void* (*PAW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*PAW_FindComp_CropDesc_t)(uintptr_t entity, void* outIBP);  // 0x140005380
typedef void* (*PAW_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);

static PAW_FindComp_MatDesc_t     s_fn_FindComp_MatDesc = nullptr;      // 0x140003050
static PAW_FindComp_CropDesc_t    s_fn_FindComp_CropDesc = nullptr;     // 0x140005380
static PAW_FindComp_MeshInstance_t s_fn_FindComp_MeshInstance = nullptr; // 0x140002f90

// CalcMaxWorkers(matDesc) -> int
typedef int (*PAW_CalcMaxWorkers_t)(uintptr_t matDesc);
static PAW_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr;  // 0x14012e5c0

// Map functions
typedef int  (*PAW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool (*PAW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
static PAW_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr;  // 0x14009ccf0
static PAW_Map_CanPathTo_t          s_fn_Map_CanPathTo = nullptr;             // 0x1400a00f0

// GetTerrainPos
typedef float* (*PAW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                      char param4, uint32_t* outNormal);
static PAW_GetTerrainPos_t s_fn_GetTerrainPos = nullptr;  // 0x14009f3e0

// Math::Vector3::Vector3 (IAT dispatch)
typedef float* (*PAW_Vector3_Ctor_t)(float* self, float x, float y, float z);
static PAW_Vector3_Ctor_t s_fn_Vector3_Ctor = nullptr;

// ShowTool_Action_Ctor3 -- creates planting action chain for crop areas
// 12 parameters (RCX,RDX,R8,R9 + 8 stack params)
typedef void (*PAW_PlantAction_Ctor_t)(void* citizenIBP, int priority, void* externalPtr,
                                       uint32_t zero, float* position, int harvestMode,
                                       int* srcCellIdxPtr, uint32_t packedCellInfo,
                                       void* entityRefPtr, uint32_t* typeConstPtr,
                                       uint64_t unused, void* packedCellPairPtr);
static PAW_PlantAction_Ctor_t s_fn_PlantAction_Ctor = nullptr;  // 0x1400bdd20

// BST operations
typedef void* (*PAW_BST_Insert_t)(void* treeRoot, void* outResult, uint32_t* keyData);
typedef void  (*PAW_BST_Remove_t)(void* treeRoot, void* node);
typedef void  (*PAW_BST_Serialize_t)(void* treeRoot, void* serializer);
typedef void  (*PAW_BST_RecursiveFree_t)(void* treeRoot, void* node);
static PAW_BST_Insert_t       s_fn_BST_Insert = nullptr;        // 0x140029a40
static PAW_BST_Remove_t       s_fn_BST_Remove = nullptr;        // TradeDescription_impl_ca60
static PAW_BST_Serialize_t    s_fn_BST_Serialize = nullptr;      // TradeDescription_impl_9de0
static PAW_BST_RecursiveFree_t s_fn_BST_RecursiveFree = nullptr; // 0x14000e320

// Work validate task helper
typedef uint8_t (*PAW_ValidateTask_t)(uintptr_t self, uintptr_t param2, void* ibp, void** outTask);
static PAW_ValidateTask_t s_fn_ValidateTask = nullptr;  // 0x1400f7100

// Cancel work/task cleanup
typedef void (*PAW_TaskCancel_t)(void* taskList);
static PAW_TaskCancel_t s_fn_TaskCancel = nullptr;  // 0x1400c5400

// FindSubEntity (FindPositionComponent)
typedef void* (*PAW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
static PAW_FindSubEntity_t s_fn_FindSubEntity = nullptr;  // 0x140002e10

// Work::Serialize base
typedef void (*PAW_Work_Serialize_t)(void* self, void* stream);
static PAW_Work_Serialize_t s_fn_Work_Serialize = nullptr;  // 0x1400f7400

// Serializer ctor/dtor
typedef void* (*PAW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*PAW_Serializer_Dtor_t)(void* self);
static PAW_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
static PAW_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Serializer::Serialize for int fields
typedef void (*PAW_Serializer_SerializeInt_t)(void* ser, int* buf, const char* name);
static PAW_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// Serializer::Serialize for bool array
typedef void (*PAW_Serializer_SerializeBoolArray_t)(void* ser, int count, void* buf, const char* name);
static PAW_Serializer_SerializeBoolArray_t s_fn_Serializer_SerializeBoolArray = nullptr;

// ExternalRef serialize (FUN_140002340)
typedef void (*PAW_ExternalRef_Serialize_t)(void* refPtr, void* serializer);
static PAW_ExternalRef_Serialize_t s_fn_ExternalRef_Serialize = nullptr;  // 0x140002340

// Resize byte array (FUN_14000a590)
typedef void (*PAW_ResizeByteArray_t)(void* arrayField, int newCount);
static PAW_ResizeByteArray_t s_fn_ResizeByteArray = nullptr;  // 0x14000a590

// MemAlloc / MemFree
typedef void* (*PAW_MemAlloc_t)(int64_t size, int align);
static PAW_MemAlloc_t s_fn_MemAlloc = nullptr;

// CropDesc completion helper (called when count == 0 and all planted)
typedef void (*PAW_CropDescComplete_t)(uintptr_t cropDescComp);
static PAW_CropDescComplete_t s_fn_CropDescComplete = nullptr;  // 0x14011b390

// Float constants from .rdata
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Type constant (DAT_1401c7510)
static uint32_t* s_typeConst = nullptr;

// Vtable pointers for dtor chain
static void** s_vtable_PlantArea = nullptr;   // 0x140161890
static void** s_vtable_Work = nullptr;         // 0x140161a80 (Work vtable)

static bool s_pa_funcs_resolved = false;


// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool PlantArea_ResolveFuncs() {
    if (s_pa_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_FindComp_MatDesc = reinterpret_cast<PAW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_CropDesc = reinterpret_cast<PAW_FindComp_CropDesc_t>(
        ResolveAddr(Module::Exe, 0x140005380));
    s_fn_FindComp_MeshInstance = reinterpret_cast<PAW_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_CalcMaxWorkers = reinterpret_cast<PAW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Map_FindNearestPassable = reinterpret_cast<PAW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<PAW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<PAW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_PlantAction_Ctor = reinterpret_cast<PAW_PlantAction_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bdd20));
    s_fn_BST_Insert = reinterpret_cast<PAW_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140029a40));
    s_fn_BST_Remove = reinterpret_cast<PAW_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));  // was 0x140029a60 (mid-function; true remove entry is 0x14001ca60)
    s_fn_BST_Serialize = reinterpret_cast<PAW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140029de0));
    s_fn_BST_RecursiveFree = reinterpret_cast<PAW_BST_RecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x14000e320));
    s_fn_ValidateTask = reinterpret_cast<PAW_ValidateTask_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TaskCancel = reinterpret_cast<PAW_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<PAW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_Work_Serialize = reinterpret_cast<PAW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_ResizeByteArray = reinterpret_cast<PAW_ResizeByteArray_t>(
        ResolveAddr(Module::Exe, 0x14000a590));
    s_fn_ExternalRef_Serialize = reinterpret_cast<PAW_ExternalRef_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_fn_CropDescComplete = reinterpret_cast<PAW_CropDescComplete_t>(
        ResolveAddr(Module::Exe, 0x14011b390));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<PAW_Vector3_Ctor_t*>(iatVec3Ctor);

    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor)
        s_fn_Serializer_Ctor = *reinterpret_cast<PAW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor)
        s_fn_Serializer_Dtor = *reinterpret_cast<PAW_Serializer_Dtor_t*>(iatSerDtor);

    uintptr_t iatSerInt = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerInt)
        s_fn_Serializer_SerializeInt = *reinterpret_cast<PAW_Serializer_SerializeInt_t*>(iatSerInt);
    // Bool array serialize uses same fn as int serialize (different usage)
    s_fn_Serializer_SerializeBoolArray = reinterpret_cast<PAW_Serializer_SerializeBoolArray_t>(
        s_fn_Serializer_SerializeInt);

    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140152780);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<PAW_MemAlloc_t*>(iatMemAlloc);

    // Float constant
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Type constant (DAT_1401c7510 -- used as action type marker)
    s_typeConst = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7510));

    // Vtable pointers for dtor
    s_vtable_PlantArea = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161890));
    s_vtable_Work = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_FindComp_MatDesc && s_fn_FindComp_CropDesc &&
              s_fn_FindComp_MeshInstance && s_fn_CalcMaxWorkers &&
              s_fn_Map_FindNearestPassable && s_fn_Map_CanPathTo &&
              s_fn_GetTerrainPos && s_fn_Vector3_Ctor &&
              s_fn_PlantAction_Ctor && s_fn_BST_Insert &&
              s_fn_BST_Remove && s_fn_BST_Serialize &&
              s_fn_BST_RecursiveFree && s_fn_ValidateTask &&
              s_fn_TaskCancel && s_fn_FindSubEntity &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor &&
              s_fn_Serializer_Dtor && s_fn_Serializer_SerializeInt &&
              s_fn_ResizeByteArray && s_fn_ExternalRef_Serialize &&
              s_fn_CropDescComplete &&
              fn_InternalBasePtr_CheckPtr && fn_MemFreeStatic &&
              g_pGameWorldSingleton && s_fHalfCellOffset &&
              s_typeConst && s_vtable_PlantArea && s_vtable_Work;

    s_pa_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[PlantArea] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_CropDesc) printf("  MISSING: FindComp_CropDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
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
        if (!s_fn_CropDescComplete) printf("  MISSING: CropDescComplete\n");
    }
#endif

    return ok;
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
int PlantArea_Slot4_Impl(PlantArea* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Step 1: Find components on workplace entity
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t cropDescIBP = 0;
    s_fn_FindComp_CropDesc(*(uintptr_t*)(selfAddr + 0x10), &cropDescIBP);

    // Step 2: Check max workers vs current tree count
    int treeCount = (int)selfQ[10];  // self->field_50
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    // ── Plant-search census (BANISHED_PLANTAREA_TRACE) ───────────────────────────
    // Execute runs but never reaches the action ctor, and "found nothing" cannot say
    // which of the five gates is responsible. Count each. Zero cost when unset.
    static int s_ptrace = -1;
    if (s_ptrace < 0) {
        const char* pe = std::getenv("BANISHED_PLANTAREA_TRACE");
        s_ptrace = (pe && pe[0] && pe[0] != '0') ? 1 : 0;
    }
    static long long s_calls = 0, s_maxWorkers = 0, s_noMark = 0,
                     s_inBst = 0, s_noPass = 0, s_noPath = 0, s_ok = 0;
    if (s_ptrace) {
        ++s_calls;
        if ((s_calls % 40) == 0) {
            std::fprintf(stderr, "[PlantArea] after %lld Execute calls: maxWorkers=%lld "
                         "noMark=%lld alreadyInBst=%lld noPassableCell=%lld noPath=%lld "
                         "ACCEPTED=%lld  (slotCount=%d treeCount=%d cap=%d)\n",
                         s_calls, s_maxWorkers, s_noMark, s_inBst, s_noPass, s_noPath, s_ok,
                         (int)((uintptr_t*)self)[0xc], treeCount,
                         matDescIBP ? *(int*)(matDescIBP + 0x30) : -1);
            std::fflush(stderr);
        }
    }

    if (treeCount >= *(int*)(matDescIBP + 0x30)) {
        if (s_ptrace) ++s_maxWorkers;
        return 1;
    }

    // Step 3: Get map and citizen position
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

    // Step 4: Iterate field cells
    int fieldCellCount = (int)selfQ[0xc];  // self->field_60
    bool anyUnplanted = true;  // tracks if we found any cell with validity flag set

    if (fieldCellCount > 0) {
        for (int i = 0; i < fieldCellCount; i++) {
            char* validityArray = (char*)selfQ[0xb];  // self->field_58
            if (validityArray[i] == 0) {
                // Cell not available for planting
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
            fn_InternalBasePtr_CheckPtr(&cropDescIBP);
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
                fn_InternalBasePtr_CheckPtr(&cropDescIBP);
                uintptr_t cropDescVal2 = cropDescIBP;
                cellPair[1] = *(int*)((uintptr_t)pairedIdx * 0x20 +
                                      *(uintptr_t*)(cropDescVal2 + 0x58));
            }

            // Check pathfinding
            int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);
            if (nearestCell < 0) {
                continue;
            }

            bool canPath = s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
            if (!canPath) {
                continue;
            }

            // Valid cell found -- create planting action
            uint32_t typeConstVal = *s_typeConst;
            uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

            // Pack cell indices: primary in low 16 bits, paired in high 16 bits
            uint32_t packedCells[2];
            packedCells[0] = (uint32_t)i;
            packedCells[1] = (uint32_t)pairedIdx;

            float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
            float targetPos[3];
            s_fn_Vector3_Ctor(targetPos,
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            float pathPos[4];
            s_fn_GetTerrainPos(mapGrid, pathPos, targetPos, 0, nullptr);

            // Get citizen entity for action
            fn_InternalBasePtr_CheckPtr(entityIBP);
            uintptr_t citizenBuf = *(uintptr_t*)entityIBP;

            int harvestMode = pairedAvailable ? 2 : 1;
            uint32_t packedCellInfo = ((uint32_t)pairedIdx << 16) | (uint32_t)i;

            // Call ShowTool_Action_Ctor3 (0x1400bdd20) with 12 params
            s_fn_PlantAction_Ctor(
                &citizenBuf,                      // RCX: citizenIBP
                (int)selfQ[4],                    // EDX: priority
                (void*)selfQ[0xd],                // R8: self->field_68 (ExternalPtr)
                0,                                // R9D: 0
                pathPos,                          // [RSP+0x20]: position
                harvestMode,                      // [RSP+0x28]: 1=single, 2=paired
                cellPair,                         // [RSP+0x30]: ptr to {cell, pairedCell}
                packedCellInfo,                   // [RSP+0x38]: packed cell indices
                &entityRef,                       // [RSP+0x40]: entity ref from field_10
                &typeConstVal,                    // [RSP+0x48]: type constant
                0,                                // [RSP+0x50]: unused
                packedCells                       // [RSP+0x58]: packed cell pair values
            );

            // Assign work and register with AI
            int priority = (int)selfQ[4];
            fn_InternalBasePtr_CheckPtr(entityIBP);
            uintptr_t citizenEnt = *(uintptr_t*)entityIBP;

            uintptr_t vtable = *(uintptr_t*)self;
            typedef uintptr_t* (*GetTarget_t)(PlantArea*, void*);
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
            s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, insertKey);

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
                s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult2, insertKey2);
                validityArray[pairedIdx] = 0;
            }

            return 2;
        }

        // If we had no cells with validity flag set (anyUnplanted still true)
        // we return 1 below
        if (!anyUnplanted) {
            // All cells assigned but none pathable -- return 1
            return 1;
        }
    }

    // If tree count is 0 and we exhausted all cells, mark crop desc as complete
    if ((int)selfQ[10] == 0) {
        fn_InternalBasePtr_CheckPtr(&cropDescIBP);
        s_fn_CropDescComplete(cropDescIBP);
    }

    return 1;
}


// ============================================================
// PlantArea::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool PlantArea_Execute_Impl(PlantArea* /*self*/) {
    return false;
}


// ============================================================
// PlantArea::OnComplete (Slot6) -- 0x1400f3f60
// Removes BST entries, marks cells valid.
// param4 packed: low 16 = primary cell, high 16 = paired cell.
// ============================================================
int PlantArea_OnComplete_Impl(PlantArea* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

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
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
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
                s_fn_BST_Remove((void*)(selfAddr + 0x48), node2);
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
int PlantArea_OnCancel_Impl(PlantArea* self, void* /*param2*/, void* /*param3*/, uint32_t param4) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

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
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
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
                s_fn_BST_Remove((void*)(selfAddr + 0x48), node2);
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
void PlantArea_Cleanup_Impl(PlantArea* self) {
    uintptr_t selfAddr = (uintptr_t)self;
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
        fn_InternalBasePtr_CheckPtr((void*)(rightmost + 8));
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
}


// ============================================================
// PlantArea::Validate (Slot9) -- 0x1400f3c70
// RemoveInvalid: iterates BST, validates entries, removes dead ones.
// Marks invalid cells as valid (available for re-planting).
// ============================================================
bool PlantArea_Validate_Impl(PlantArea* self, void* param2) {
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
            cur = *(int**)(cur + 6);  // left child
        }
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2);  // IBP at +0x8

        uint8_t valid = s_fn_ValidateTask(selfAddr, (uintptr_t)param2, (void*)&ibpData, nullptr);

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
                        newArray[j] = *node;  // cell index key
                    }
                }
                if (collectArray != newArray) {
                    fn_MemFreeStatic(collectArray);
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
// PlantArea::GetTarget (Slot10) -- 0x1400f44e0
// Returns entity ptr from work's entity ref sub-object at +0x48.
// ============================================================
void* PlantArea_GetTarget_Impl(PlantArea* self, void* outPtr) {
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
// PlantArea::GetPriority (Slot11) -- 0x140029860
// 9 bytes, shared across ClearArea/PlantArea/PlantOrchard.
// Returns (this->field_50 > 0).
// ============================================================
bool PlantArea_GetPriority_Impl(PlantArea* self) {
    return self->field_50 > 0;
}


// ============================================================
// PlantArea::Serialize -- 0x1400f4540
// Work::Serialize + BST + count + bool array + ExternalRef.
// ============================================================
void PlantArea_Serialize_Impl(PlantArea* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Serialize Work base class
    s_fn_Work_Serialize(self, stream);

    // Create Serializer with version=0
    uint8_t serializerBuf[24] = {0};
    s_fn_Serializer_Ctor(serializerBuf, stream, 0);

    // Serialize BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

    // Serialize field cell count at +0x60
    int cellCountBuf[2] = { *(int*)(selfAddr + 0x60), 0 };
    s_fn_Serializer_SerializeInt(serializerBuf, cellCountBuf, nullptr);

    // If reading (serializer mode == 0), resize validity array
    if (*(int*)(*(uintptr_t*)serializerBuf + 8) == 0) {
        s_fn_ResizeByteArray((void*)(selfAddr + 0x58), cellCountBuf[0]);
    }

    // Serialize validity array (byte array)
    s_fn_Serializer_SerializeBoolArray(serializerBuf, cellCountBuf[0],
                                       *(void**)(selfAddr + 0x58), nullptr);

    // Serialize ExternalRef at +0x68
    s_fn_ExternalRef_Serialize((void*)(selfAddr + 0x68), serializerBuf);

    // Destroy serializer
    s_fn_Serializer_Dtor(serializerBuf);
}


// ============================================================
// PlantArea::DtorBody -- 0x1400f4600
// Sets vtable, frees validity array + BST, calls Work dtor chain.
// ============================================================
void PlantArea_DtorBody(PlantArea* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set PlantArea vtable
    *(void**)selfAddr = s_vtable_PlantArea;

    // Free validity array at +0x58
    fn_MemFreeStatic(*(void**)(selfAddr + 0x58));
    *(uintptr_t*)(selfAddr + 0x58) = 0;
    *(uintptr_t*)(selfAddr + 0x60) = 0;

    // Free BST at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        s_fn_BST_RecursiveFree((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20));
        fn_MemFreeStatic(bstRoot);
    }
    *(uintptr_t*)(selfAddr + 0x48) = 0;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set Work vtable, then call Object::Internal::~Internal
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// PlantArea::ScalarDtor -- 0x14002c4b0
// Calls dtor body, then conditional free.
// ============================================================
void* PlantArea_ScalarDtor_Impl(PlantArea* self, uint32_t flags) {
    PlantArea_DtorBody(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161890 (13 slots), size 0x70, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// NOTE: slot4 (+0x20) Execute = PlantArea_Slot4_Impl (the main planting
// logic). PlantArea_Execute_Impl is the 3-byte returns-false stub documented
// as "Slot5 (+0x28)", which is the shared Work_Slot5 — it is NOT used here.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

// Constructor — port of PlantArea_Ctor (0x1400f4770, factory variant):
// Object::Internal::Internal -> Work base fields -> PlantArea vtable + zero fields.
static void* PlantArea_Ctor_Impl(PlantArea* self) {
    fn_ObjectInternal_Ctor(self);

    uintptr_t a = (uintptr_t)self;
    // Work base vtable (intermediate, immediately overwritten)
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    *(void**)(a + 0x10) = nullptr;
    *(void**)(a + 0x18) = nullptr;
    *(int32_t*)(a + 0x20) = -1;
    *(int32_t*)(a + 0x24) = 0;
    *(int32_t*)(a + 0x28) = -1;
    *(int32_t*)(a + 0x2c) = 0;
    *(void**)(a + 0x30) = nullptr;
    *(void**)(a + 0x38) = nullptr;
    *(int32_t*)(a + 0x40) = 0;

    // PlantArea vtable (our registered standalone table)
    self->vtable = (void*)ResolveAddr(Module::Exe, 0x140161890);
    *(void**)(a + 0x48) = nullptr;     // BST root
    *(int32_t*)(a + 0x50) = 0;         // BST count
    *(void**)(a + 0x58) = nullptr;     // validity array
    *(void**)(a + 0x60) = nullptr;     // field_60 (cell count + pad)
    *(void**)(a + 0x68) = nullptr;     // ExternalPtr (crop desc)

    return self;
}

static uint32_t PlantArea_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("PlantArea");
    return c;
}

static void* g_PlantArea_Vtable[13] = {
    (void*)&PlantArea_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&PlantArea_GetTypeId,        // +0x10 GetTypeId
    (void*)&PlantArea_Serialize_Impl,   // +0x18 Serialize
    (void*)&PlantArea_Slot4_Impl,       // +0x20 Execute (main planting logic)
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&PlantArea_OnComplete_Impl,  // +0x30 OnComplete
    (void*)&PlantArea_OnCancel_Impl,    // +0x38 OnCancel
    (void*)&PlantArea_Cleanup_Impl,     // +0x40 Cleanup
    (void*)&PlantArea_Validate_Impl,    // +0x48 Validate
    (void*)&PlantArea_GetTarget_Impl,   // +0x50 GetTarget
    (void*)&PlantArea_GetPriority_Impl, // +0x58 GetPriority
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterPlantAreaVtable() {
    RegisterExeAddr(0x140161890, (uintptr_t)g_PlantArea_Vtable);
}
extern "C" void* PlantArea_CreateInPlace(void* mem) {
    PlantArea_ResolveFuncs();
    return PlantArea_Ctor_Impl((PlantArea*)mem);
}
extern "C" void* PlantArea_Factory(void) {
    PlantArea_ResolveFuncs();
    void* p = Work_Alloc(0x70);
    return p ? PlantArea_Ctor_Impl((PlantArea*)p) : nullptr;
}
REGISTER_TYPE(PlantArea, "Work", &PlantArea_Factory, &PlantArea_CreateInPlace);
