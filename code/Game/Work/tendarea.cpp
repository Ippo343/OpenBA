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

#include "Work/tendarea_funcs.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Local function pointers (resolved once by TendArea_ResolveFuncs)
// ============================================================

// All typedefs prefixed with TAW_ to avoid conflicts

// Work::Serialize
typedef void  (*TAW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor/Serialize
typedef void* (*TAW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*TAW_Serializer_Dtor_t)(void* self);
typedef void  (*TAW_Serializer_Serialize_t)(void* self, int* value, const char* name);
// BST serialization (simple BST at 0x1400296b0)
typedef void  (*TAW_BST_Serialize_t)(void* bstRoot, void* serializer);
// BST find-and-remove by key (CitizenAI_impl_5520 at 0x140025520)
typedef void  (*TAW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// BST node remove (CitizenAI_impl_a6e0 at 0x14001a6e0)
typedef void  (*TAW_BSTRemove_t)(void* bstRoot, void* node);
// Cancel all work items
typedef void  (*TAW_CancelAllWork_t)(void* citizenAISub);
// FUN_1400f7100 -- validate work entity
typedef bool  (*TAW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*TAW_MemAlloc_t)(int64_t size, int align);
typedef void  (*TAW_MemFree_t)(void* ptr);
typedef void  (*TAW_MemFreeStatic_t)(void* ptr);
// Tree node cleanup (recursive free, simple BST at 0x140014c30)
typedef void  (*TAW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// FindSubEntity
typedef void* (*TAW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// CalcMaxWorkers
typedef int   (*TAW_CalcMaxWorkers_t)(uintptr_t matDesc);
// FindComponent helpers
typedef void* (*TAW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*TAW_FindComp_AreaCrop_t)(uintptr_t entity, void* outIBP);
typedef void* (*TAW_FindComp_AreaOrchard_t)(uintptr_t entity, void* outIBP);
// Random
typedef uint32_t (*TAW_Random_Rand_t)(void* rng, uint32_t max);
// Map functions
typedef int   (*TAW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*TAW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
// GetTerrainPos
typedef float*(*TAW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
// PerformWork (FUN_1400bb0a0 -- the complex work ctor with target/position)
typedef void  (*TAW_PerformWork_Ctor_t)(void* citizenIBP, int priority, void* workEntityIBP,
                                        int unused, float* targetPos, float* srcPos,
                                        uint32_t typeId, int flag, uint32_t cellIdx);
// CitizenAI RegisterWork
typedef void  (*TAW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
// CitizenAI LongTermPlan
typedef void  (*TAW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
// FindComponent Storage
typedef void* (*TAW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
// StorageComponent_QueryCount
typedef int   (*TAW_StorageQueryCount_t)(void* storageComp);

static TAW_Work_Serialize_t          s_fn_Work_Serialize = nullptr;
static TAW_Serializer_Ctor_t         s_fn_Serializer_Ctor = nullptr;
static TAW_Serializer_Dtor_t         s_fn_Serializer_Dtor = nullptr;
static TAW_Serializer_Serialize_t    s_fn_Serializer_Serialize = nullptr;
static TAW_BST_Serialize_t           s_fn_BST_Serialize = nullptr;
static TAW_BSTFindRemove_t           s_fn_BSTFindRemove = nullptr;
static TAW_BSTRemove_t               s_fn_BSTRemove = nullptr;
static TAW_CancelAllWork_t           s_fn_CancelAllWork = nullptr;
static TAW_ValidateEntity_t          s_fn_ValidateEntity = nullptr;
static TAW_MemAlloc_t                s_fn_MemAlloc = nullptr;
static TAW_MemFree_t                 s_fn_MemFree = nullptr;
static TAW_MemFreeStatic_t           s_fn_MemFreeStatic = nullptr;
static TAW_TreeNodeCleanup_t         s_fn_TreeNodeCleanup = nullptr;
static TAW_FindSubEntity_t           s_fn_FindSubEntity = nullptr;
static TAW_CalcMaxWorkers_t          s_fn_CalcMaxWorkers = nullptr;
static TAW_FindComp_MatDesc_t        s_fn_FindComp_MatDesc = nullptr;
static TAW_FindComp_AreaCrop_t       s_fn_FindComp_AreaCrop = nullptr;
static TAW_FindComp_AreaOrchard_t    s_fn_FindComp_AreaOrchard = nullptr;
static TAW_Random_Rand_t             s_fn_Random_Rand = nullptr;
static TAW_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr;
static TAW_Map_CanPathTo_t           s_fn_Map_CanPathTo = nullptr;
static TAW_GetTerrainPos_t           s_fn_GetTerrainPos = nullptr;
static TAW_PerformWork_Ctor_t        s_fn_PerformWork_Ctor = nullptr;
static TAW_CitizenAI_RegisterWork_t  s_fn_CitizenAI_RegisterWork = nullptr;
static TAW_CitizenAI_LongTermPlan_t  s_fn_CitizenAI_LongTermPlan = nullptr;
static TAW_FindComp_Storage_t        s_fn_FindComp_Storage = nullptr;
static TAW_StorageQueryCount_t       s_fn_StorageQueryCount = nullptr;

// Type ID constants
static uint32_t* s_typeId_CropAreaComponent = nullptr;    // DAT_1401c7510
static uint32_t* s_typeId_OrchardAreaComponent = nullptr;  // DWORD_1401c46b0

// Float constants
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: TendArea_Ctor
// disassembles to `0x1400f5a1d: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.
static void** s_vtable_TendArea = nullptr;  // 0x140161990
static void** s_vtable_Work = nullptr;       // 0x140161a80

static bool s_tend_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool TendArea_ResolveFuncs() {
    if (s_tend_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<TAW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize = reinterpret_cast<TAW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));
    s_fn_BSTFindRemove = reinterpret_cast<TAW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_BSTRemove = reinterpret_cast<TAW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_CancelAllWork = reinterpret_cast<TAW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_ValidateEntity = reinterpret_cast<TAW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TreeNodeCleanup = reinterpret_cast<TAW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));
    s_fn_FindSubEntity = reinterpret_cast<TAW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_CalcMaxWorkers = reinterpret_cast<TAW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_FindComp_MatDesc = reinterpret_cast<TAW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_AreaCrop = reinterpret_cast<TAW_FindComp_AreaCrop_t>(
        ResolveAddr(Module::Exe, 0x140005380));
    s_fn_FindComp_AreaOrchard = reinterpret_cast<TAW_FindComp_AreaOrchard_t>(
        ResolveAddr(Module::Exe, 0x140005440));
    s_fn_Map_FindNearestPassable = reinterpret_cast<TAW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<TAW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<TAW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_PerformWork_Ctor = reinterpret_cast<TAW_PerformWork_Ctor_t>(
        ResolveAddr(Module::Exe, 0x1400bb0a0));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<TAW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<TAW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Storage = reinterpret_cast<TAW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<TAW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatSerCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtor) s_fn_Serializer_Ctor = *reinterpret_cast<TAW_Serializer_Ctor_t*>(iatSerCtor);
    uintptr_t iatSerDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtor) s_fn_Serializer_Dtor = *reinterpret_cast<TAW_Serializer_Dtor_t*>(iatSerDtor);
    uintptr_t iatSerSer = ResolveAddr(Module::Exe, 0x140152ac8);
    if (iatSerSer) s_fn_Serializer_Serialize = *reinterpret_cast<TAW_Serializer_Serialize_t*>(iatSerSer);
    // Static MemAlloc/MemFree. These previously read exe dispatch slots 0x140151460 /
    // 0x140151468; 0x140151460 is NOT MemAlloc, it is Graphics::Geometry::GetTransform
    // (see placementtool.cpp:78). Since that slot is registered, the pointer was
    // non-null and WRONG, so the first array grow called GetTransform(size, align) and
    // took an ACCESS_VIOLATION. Diagnosed in tradework.cpp, where a TradeWork actually
    // reached its grow path; the same wrong pair was copied into five work classes.
    s_fn_MemAlloc = reinterpret_cast<TAW_MemAlloc_t>(fn_MemAllocStatic);
    s_fn_MemFree  = reinterpret_cast<TAW_MemFree_t>(fn_MemFreeStatic);

    // Random::Rand from dispatch slot
    uintptr_t iatRand = ResolveAddr(Module::Exe, 0x1401512e8);
    if (iatRand) s_fn_Random_Rand = *reinterpret_cast<TAW_Random_Rand_t*>(iatRand);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<TAW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID pointers
    s_typeId_CropAreaComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7510));
    s_typeId_OrchardAreaComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c46b0));

    // Float constants
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable pointers for dtor
    s_vtable_TendArea = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161990));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161a80));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize && s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_Serialize && s_fn_BST_Serialize &&
              s_fn_BSTFindRemove && s_fn_BSTRemove && s_fn_CancelAllWork &&
              s_fn_ValidateEntity && s_fn_MemAlloc &&
              // s_fn_MemFree is deliberately NOT required. It comes from the IAT dispatch
              // slot 0x140151468, which is registered nowhere in our tree, so it is always
              // null — and this file never calls it (the real frees go through
              // s_fn_MemFreeStatic, which comes from engine_funcs and is non-null). Its
              // four sibling Work files resolve the same dead pointer and correctly leave it
              // out of their conjunction; only this one gated on it, which made
              // TendArea_ResolveFuncs() report FAILED on every call and never latch
              // s_tend_funcs_resolved — so it re-ran the whole resolve on every TendArea
              // construction, and a genuinely useful readiness signal read FAILED
              // permanently, ready to mask a real missing pointer later.
              // Still resolved and still reported below, so a future call site is visible.
              s_fn_TreeNodeCleanup && s_fn_FindSubEntity &&
              s_fn_CalcMaxWorkers && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_AreaCrop && s_fn_FindComp_AreaOrchard &&
              s_fn_Random_Rand && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_GetTerrainPos &&
              s_fn_PerformWork_Ctor && s_fn_CitizenAI_RegisterWork &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Storage &&
              s_fn_StorageQueryCount &&
              fn_IsDerivedFrom &&
              g_pGameWorldSingleton && s_fHalfCellOffset;

    s_tend_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[TendArea] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_Serialize) printf("  MISSING: Serializer_Serialize\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_MemAlloc) printf("  MISSING: MemAlloc\n");
        if (!s_fn_MemFree) printf("  MISSING: MemFree\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_AreaCrop) printf("  MISSING: FindComp_AreaCrop\n");
        if (!s_fn_FindComp_AreaOrchard) printf("  MISSING: FindComp_AreaOrchard\n");
        if (!s_fn_Random_Rand) printf("  MISSING: Random_Rand\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_PerformWork_Ctor) printf("  MISSING: PerformWork_Ctor\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: StorageQueryCount\n");
    }
#endif

    return ok;
}


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
int TendArea_Slot4_Impl(TendArea* self, void* citizenIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Step 1: Check max workers
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    int64_t currentWorkers = (int64_t)selfQ[10]; // self->workerCount at +0x50

    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    if ((int)currentWorkers >= *(int*)(matDescIBP + 0x30)) {
        return 1;  // At capacity, no work
    }

    // Step 2: Get game world singletons
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
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

    if (self->tendMode == 0) {
        // Crop mode: find CropAreaComponent
        s_fn_FindComp_AreaCrop(*(uintptr_t*)(selfAddr + 0x10), &areaCompIBP);

        uint32_t areaSize = *(uint32_t*)(areaCompIBP + 0x60);
        randIdx = s_fn_Random_Rand(rng, areaSize);

        workFlag = 1;
        targetCell = *(int*)((int64_t)(int)randIdx * 0x20 + *(int64_t*)(areaCompIBP + 0x58));
        typeId = *s_typeId_CropAreaComponent;
    } else {
        // Orchard mode: find OrchardAreaComponent
        s_fn_FindComp_AreaOrchard(*(uintptr_t*)(selfAddr + 0x10), &areaCompIBP);

        uint32_t areaSize = *(uint32_t*)(areaCompIBP + 0x58);
        randIdx = s_fn_Random_Rand(rng, areaSize);

        workFlag = 2;
        targetCell = *(int*)(*(int64_t*)(areaCompIBP + 0x50) + (int64_t)(int)randIdx * 0x28);
        typeId = *s_typeId_OrchardAreaComponent;
    }

    // Step 5: Find nearest passable cell and check pathability
    int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, targetCell, citizenGridCell, 0xe1f);
    if (nearestCell < 0 || (int)randIdx < 0) {
        return 0;  // No passable cell found
    }

    bool canPath = s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
    if (!canPath) {
        return 0;  // Can't path to cell
    }

    // Step 6: Calculate terrain positions
    float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
    Math::Vector3 targetPos(
        (float)(targetCell % *mapGrid) - halfOffset,
        (float)(targetCell / *mapGrid) - halfOffset,
        0.0f);
    float targetTerrain[3];
    s_fn_GetTerrainPos(mapGrid, targetTerrain, &targetPos.x, '\0', nullptr);

    Math::Vector3 citizenPos(
        (float)(nearestCell % *mapGrid) - halfOffset,
        (float)(nearestCell / *mapGrid) - halfOffset,
        0.0f);
    float citizenTerrain[3];
    s_fn_GetTerrainPos(mapGrid, citizenTerrain, &citizenPos.x, '\0', nullptr);

    // Step 7: Create PerformWork action
    uintptr_t workEntityRef = *(uintptr_t*)(selfAddr + 0x10);
    uintptr_t entityBuf[2] = { *(uintptr_t*)citizenIBP, 0 };

    s_fn_PerformWork_Ctor(
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
    uintptr_t vtable = *(uintptr_t*)self;
    typedef uintptr_t* (*GetWorkEntity_t)(TendArea*, void*);
    auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
    uintptr_t workEntIBP[2] = {0};
    uintptr_t* wResult = getWorkEntity(self, workEntIBP);
    uintptr_t workEnt = *wResult;

    *(uint32_t*)(citEnt + 0x54) |= 4;
    *(int32_t*)(citEnt + 0x30) = priority;
    *(uint32_t*)(citEnt + 0xb0) = 0;

    if (workEnt == *(uintptr_t*)(citEnt + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citEnt + 0x88) |= 0x20000;
    }

    s_fn_CitizenAI_RegisterWork((void*)(selfAddr + 0x48), workEntIBP, citizenIBP);
    return 2;
}


// ============================================================
// TendArea::Slot6 -- BSTFindRemove + return false (0x1400f5470)
// ============================================================
bool TendArea_Slot6_Impl(TendArea* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// TendArea::Slot7 -- BSTFindRemove thunk (0x1400f5460)
// ============================================================
int TendArea_Slot7_Impl(TendArea* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return 0;
}


// ============================================================
// TendArea::Slot8 / CancelAll (0x1400f5830)
// Loops through BST at +0x48 while count > 0.
// For each leftmost node, cancels the work item.
// ============================================================
int TendArea_Slot8_Impl(TendArea* self) {
    uintptr_t selfAddr = (uintptr_t)self;

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

        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
    return 0;
}


// ============================================================
// TendArea::Slot9 / RemoveInvalid (0x1400f5230)
// Iterates BST, validates each entry, removes invalid ones.
// Returns true if all valid, false otherwise.
// Simple BST: node +0x08=parent, +0x10=left, +0x18=right
// ============================================================
bool TendArea_Slot9_Impl(TendArea* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
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
        bool stillValid = s_fn_ValidateEntity(selfAddr, param2, (void*)&entityIBP, nullptr);

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
                    newArr = (uintptr_t*)s_fn_MemAlloc((int64_t)newCap * 8, 8);
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
                    fn_MemFreeStatic(removeArr);
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
                        s_fn_BSTRemove(bstRootPtr, searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return removeCount == 0;
}


// ============================================================
// TendArea::Slot10 / GetWorkEntity (0x1400f58a0)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* TendArea_Slot10_Impl(TendArea* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// TendArea::Slot11 / HasWorkers (0x140029860)
// 9-byte thunk, returns (workerCount > 0).
// ============================================================
bool TendArea_Slot11_Impl(TendArea* self) {
    return self->workerCount > 0;
}


// ============================================================
// TendArea::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool TendArea_Execute_Impl(TendArea* /*self*/) {
    return false;
}


// ============================================================
// TendArea::Serialize (0x1400f5900)
// Work::Serialize + Serializer(v0) + BST serialize + int field.
// ============================================================
void TendArea_Serialize_Impl(TendArea* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer (version block, version=0)
    alignas(8) uint8_t serializerBuf[24] = {0};
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf);

    // Serialize int at +0x58 (tendMode)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int*>(&self->tendMode), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// TendArea::DtorBody
// Sets vtable, frees BST nodes, calls base dtor.
// ============================================================
void TendArea_DtorBody(TendArea* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to TendArea
    *(void**)selfAddr = s_vtable_TendArea;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree (simple BST: +0x10=left, +0x18=right)
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to Work base
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// TendArea::ScalarDtor (0x14002c5d0)
// ============================================================
void* TendArea_ScalarDtor_Impl(TendArea* self, uint32_t flags) {
    TendArea_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}


// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x140161990 (13 slots), size 0x60, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
// Slot map ground-truthed from the real exe vtable @ 0x140161990:
//   slot4 (Execute)=TendArea_Slot4_Impl (0x1400f5490). The orphaned
//   TendArea_Execute_Impl ("returns false") is NOT in the vtable — slot5
//   is the shared Work_Slot5 (0x1400f7540). Do NOT use TendArea_Execute_Impl.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TendArea_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TendArea");
    return c;
}

static void* g_TendArea_Vtable[13] = {
    (void*)&TendArea_ScalarDtor_Impl,  // +0x00 ScalarDtor (0x14002c5d0)
    (void*)&Work_Slot1,                // +0x08 slot1 (null)
    (void*)&TendArea_GetTypeId,        // +0x10 GetTypeId
    (void*)&TendArea_Serialize_Impl,   // +0x18 Serialize (0x1400f5900)
    (void*)&TendArea_Slot4_Impl,       // +0x20 Execute (0x1400f5490)
    (void*)&Work_Slot5,                // +0x28 slot5 (returns 0)
    (void*)&TendArea_Slot6_Impl,       // +0x30 OnComplete (0x1400f5470)
    (void*)&TendArea_Slot7_Impl,       // +0x38 OnCancel (0x1400f5460)
    (void*)&TendArea_Slot8_Impl,       // +0x40 Cleanup (0x1400f5830)
    (void*)&TendArea_Slot9_Impl,       // +0x48 Validate (0x1400f5230)
    (void*)&TendArea_Slot10_Impl,      // +0x50 GetTarget (0x1400f58a0)
    (void*)&TendArea_Slot11_Impl,      // +0x58 GetPriority (0x140029860)
    (void*)&Work_Slot12,               // +0x60 slot12 (void nop)
};

extern "C" void RegisterTendAreaVtable() {
    RegisterExeAddr(0x140161990, (uintptr_t)g_TendArea_Vtable);
}
extern "C" void* TendArea_CreateInPlace(void* mem) {
    TendArea_ResolveFuncs();
    return Work_GenericCtor(mem, 0x60, g_TendArea_Vtable);
}
extern "C" void* TendArea_Factory(void) {
    TendArea_ResolveFuncs();
    void* p = Work_Alloc(0x60);
    return p ? Work_GenericCtor(p, 0x60, g_TendArea_Vtable) : nullptr;
}
REGISTER_TYPE(TendArea, "Work", &TendArea_Factory, &TendArea_CreateInPlace);
