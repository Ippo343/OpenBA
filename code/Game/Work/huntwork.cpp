// code/Game/Work/huntwork.cpp
// Clean C++ reimplementation of HuntWork -- all methods.
// Source: Game/Work/HuntWork.cpp (from profile build debug strings)
//
// HuntWork (0x58 bytes) -- Hunting work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HuntWork
// Vtable: 0x140161620
//
// Key difference from HerbalistWork: no herb search or growth check.
// HuntWork finds a valid hunting area cell, checks animal population,
// then creates a PerformWork action to hunt at that location.
// Does NOT serialize an extra int field (unlike HerbalistWork/ForesterWork).
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/huntwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers (resolved once by HuntWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with HuW_ to avoid conflicts

typedef int   (*HuW_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
typedef void* (*HuW_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HuW_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void  (*HuW_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
typedef int   (*HuW_CalcMaxWorkers_t)(uintptr_t matDesc);
typedef void* (*HuW_FindComp_Storage_t)(uintptr_t entity, void* outIBP);
typedef int   (*HuW_StorageQueryCount_t)(void* storageComp);
typedef int   (*HuW_Random_RandTol_t)(void* rng, int range);
typedef float*(*HuW_Vector3_Ctor_t)(float* self, float x, float y, float z);
typedef float*(*HuW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
typedef void  (*HuW_PerformWork_Ctor3_t)(void* entityIBP, int priority, void* targetIBP,
                                         float* position, int flag, uint8_t param6);
typedef void  (*HuW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
typedef void  (*HuW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
typedef void* (*HuW_FindComp_Pasture_t)(uintptr_t entity, void* outIBP);
typedef void* (*HuW_GetAreaComponent_t)(uintptr_t entityPtr, void* outIBP);
typedef int   (*HuW_Map_CalcDistance_t)(int* mapGrid, uint32_t cellA, uint32_t cellB);
typedef int   (*HuW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*HuW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);

// Work::Serialize
typedef void  (*HuW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor
typedef void* (*HuW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*HuW_Serializer_Dtor_t)(void* self);
// BST serialization
typedef void  (*HuW_BST_Serialize_t)(void* bstRoot, void* serializer);

// FUN_1400f7100 -- validate work entity
typedef bool  (*HuW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// MemAlloc/MemFree
typedef void* (*HuW_MemAlloc_t)(int64_t size, int align);
typedef void  (*HuW_MemFree_t)(void* ptr);
// BST node remove
typedef void  (*HuW_BSTRemove_t)(void* bstRoot, void* node);
// BST find-and-remove by key
typedef void  (*HuW_BSTFindRemove_t)(void* bstRoot, void* keyIBP);
// Cancel all work items in BST
typedef void  (*HuW_CancelAllWork_t)(void* bstRoot);
// Find sub-entity
typedef void* (*HuW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// Object::Internal::~Internal (dtor tail call)
typedef void  (*HuW_Internal_Dtor_t)(void* self);
// Tree node cleanup
typedef void  (*HuW_TreeNodeCleanup_t)(void* bstRoot, void* node);
// fn_MemFreeStatic
typedef void  (*HuW_MemFreeStatic_t)(void* ptr);

static HuW_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;
static HuW_FindComp_MatDesc_t            s_fn_FindComp_MatDesc = nullptr;
static HuW_FindComp_MeshInstance_t       s_fn_FindComp_MeshInstance = nullptr;
static HuW_MeshInstance_SetVisible_t     s_fn_MeshInstance_SetVisible = nullptr;
static HuW_CalcMaxWorkers_t              s_fn_CalcMaxWorkers = nullptr;
static HuW_FindComp_Storage_t            s_fn_FindComp_Storage = nullptr;
static HuW_StorageQueryCount_t           s_fn_StorageQueryCount = nullptr;
static HuW_Random_RandTol_t              s_fn_Random_RandTol = nullptr;
static HuW_Vector3_Ctor_t               s_fn_Vector3_Ctor = nullptr;
static HuW_GetTerrainPos_t              s_fn_GetTerrainPos = nullptr;
static HuW_PerformWork_Ctor3_t          s_fn_PerformWork_Ctor3 = nullptr;
static HuW_CitizenAI_RegisterWork_t     s_fn_CitizenAI_RegisterWork = nullptr;
static HuW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static HuW_FindComp_Pasture_t           s_fn_FindComp_Pasture = nullptr;
static HuW_GetAreaComponent_t           s_fn_GetAreaComponent = nullptr;
static HuW_Map_CalcDistance_t            s_fn_Map_CalcDistance = nullptr;
static HuW_Map_FindNearestPassable_t    s_fn_Map_FindNearestPassable = nullptr;
static HuW_Map_CanPathTo_t              s_fn_Map_CanPathTo = nullptr;
static HuW_Work_Serialize_t             s_fn_Work_Serialize = nullptr;
static HuW_Serializer_Ctor_t            s_fn_Serializer_Ctor = nullptr;
static HuW_Serializer_Dtor_t            s_fn_Serializer_Dtor = nullptr;
static HuW_BST_Serialize_t              s_fn_BST_Serialize = nullptr;
static HuW_ValidateEntity_t             s_fn_ValidateEntity = nullptr;
static HuW_MemAlloc_t                   s_fn_MemAlloc = nullptr;
static HuW_MemFree_t                    s_fn_MemFree = nullptr;
static HuW_BSTRemove_t                  s_fn_BSTRemove = nullptr;
static HuW_BSTFindRemove_t              s_fn_BSTFindRemove = nullptr;
static HuW_CancelAllWork_t              s_fn_CancelAllWork = nullptr;
static HuW_FindSubEntity_t              s_fn_FindSubEntity = nullptr;
static HuW_TreeNodeCleanup_t            s_fn_TreeNodeCleanup = nullptr;
static HuW_MemFreeStatic_t              s_fn_MemFreeStatic = nullptr;

// Type ID constants
static uint32_t* s_typeId_HerdComponent = nullptr;  // DAT_1401c78c8

// Float constants from .rdata
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain
static void** s_vtable_HuntWork = nullptr;       // 0x140161620
static void** s_vtable_AutoPickupWork = nullptr;  // 0x140161340
static void** s_vtable_Work = nullptr;             // 0x1401612e0

static bool s_hunt_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool HuntWork_ResolveFuncs() {
    if (s_hunt_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<HuW_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<HuW_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<HuW_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<HuW_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<HuW_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_FindComp_Storage = reinterpret_cast<HuW_FindComp_Storage_t>(
        ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<HuW_StorageQueryCount_t>(
        ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_GetTerrainPos = reinterpret_cast<HuW_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_PerformWork_Ctor3 = reinterpret_cast<HuW_PerformWork_Ctor3_t>(
        ResolveAddr(Module::Exe, 0x1400c1eb0));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<HuW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<HuW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Pasture = reinterpret_cast<HuW_FindComp_Pasture_t>(
        ResolveAddr(Module::Exe, 0x140004fe0));
    s_fn_GetAreaComponent = reinterpret_cast<HuW_GetAreaComponent_t>(
        ResolveAddr(Module::Exe, 0x140004680));
    s_fn_Map_CalcDistance = reinterpret_cast<HuW_Map_CalcDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HuW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HuW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_Work_Serialize = reinterpret_cast<HuW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize = reinterpret_cast<HuW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14001a6b0));
    s_fn_ValidateEntity = reinterpret_cast<HuW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_BSTRemove = reinterpret_cast<HuW_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_BSTFindRemove = reinterpret_cast<HuW_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));
    s_fn_CancelAllWork = reinterpret_cast<HuW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<HuW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_TreeNodeCleanup = reinterpret_cast<HuW_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatRandTol = ResolveAddr(Module::Exe, 0x1401512f0);
    if (iatRandTol)
        s_fn_Random_RandTol = *reinterpret_cast<HuW_Random_RandTol_t*>(iatRandTol);
    uintptr_t iatVec3Ctor = ResolveAddr(Module::Exe, 0x140152a30);
    if (iatVec3Ctor)
        s_fn_Vector3_Ctor = *reinterpret_cast<HuW_Vector3_Ctor_t*>(iatVec3Ctor);

    // Serializer ctor/dtor from dispatch slots
    uintptr_t iatSerCtorSlot = ResolveAddr(Module::Exe, 0x140151500);
    if (iatSerCtorSlot)
        s_fn_Serializer_Ctor = *reinterpret_cast<HuW_Serializer_Ctor_t*>(iatSerCtorSlot);
    uintptr_t iatSerDtorSlot = ResolveAddr(Module::Exe, 0x140151508);
    if (iatSerDtorSlot)
        s_fn_Serializer_Dtor = *reinterpret_cast<HuW_Serializer_Dtor_t*>(iatSerDtorSlot);

    // MemAlloc/MemFree from dispatch slots
    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140151460);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<HuW_MemAlloc_t*>(iatMemAlloc);
    uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x140151468);
    if (iatMemFree)
        s_fn_MemFree = *reinterpret_cast<HuW_MemFree_t*>(iatMemFree);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<HuW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Type ID pointers
    s_typeId_HerdComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c78c8));

    // Float constants from .rdata
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable pointers for dtor
    s_vtable_HuntWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161620));
    s_vtable_AutoPickupWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161340));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401612e0));

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_FindComp_Storage &&
              s_fn_StorageQueryCount && s_fn_Random_RandTol &&
              s_fn_Vector3_Ctor && s_fn_GetTerrainPos &&
              s_fn_PerformWork_Ctor3 && s_fn_CitizenAI_RegisterWork &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Pasture &&
              s_fn_GetAreaComponent && s_fn_Map_CalcDistance &&
              s_fn_Map_FindNearestPassable && s_fn_Map_CanPathTo &&
              s_fn_Work_Serialize && s_fn_Serializer_Ctor &&
              s_fn_Serializer_Dtor && s_fn_BST_Serialize &&
              s_fn_ValidateEntity && s_fn_BSTRemove &&
              s_fn_BSTFindRemove && s_fn_CancelAllWork &&
              s_fn_FindSubEntity && s_fn_TreeNodeCleanup &&
              fn_InternalBasePtr_CheckPtr && fn_IsDerivedFrom &&
              g_pGameWorldSingleton && s_typeId_HerdComponent &&
              s_fHalfCellOffset;

    s_hunt_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[HuntWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_FindComp_Storage) printf("  MISSING: FindComp_Storage\n");
        if (!s_fn_StorageQueryCount) printf("  MISSING: StorageQueryCount\n");
        if (!s_fn_Random_RandTol) printf("  MISSING: Random_RandTol\n");
        if (!s_fn_Vector3_Ctor) printf("  MISSING: Vector3_Ctor\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_PerformWork_Ctor3) printf("  MISSING: PerformWork_Ctor3\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Pasture) printf("  MISSING: FindComp_Pasture\n");
        if (!s_fn_GetAreaComponent) printf("  MISSING: GetAreaComponent\n");
        if (!s_fn_Map_CalcDistance) printf("  MISSING: Map_CalcDistance\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_BSTRemove) printf("  MISSING: BSTRemove\n");
        if (!s_fn_BSTFindRemove) printf("  MISSING: BSTFindRemove\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_TreeNodeCleanup) printf("  MISSING: TreeNodeCleanup\n");
    }
#endif

    return ok;
}


// ============================================================
// HuntWork::Slot4 -- Hunt target assignment (0x1400EEFF0)
// ============================================================
//
// Algorithm:
// 1. Check auto-pickup assignments
// 2. If storage has items, replan citizen (LongTermPlan)
// 3. Find material description and herd/pasture component
// 4. Check if animal population allows hunting (resource tracker)
// 5. Set mesh visibility based on capacity
// 6. Check max workers vs current count
// 7. Random search for valid hunting cell within area radius
// 8. Validate cell: distance, pathability, terrain
// 9. Create PerformWork action at valid position
// 10. On failure: idle animation or replan
//
int HuntWork_Slot4_Impl(HuntWork* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t selfAddr = (uintptr_t)self;
    uintptr_t* selfQ = (uintptr_t*)self;

    // Save citizen ref
    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments
    // -------------------------------------------------------
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &citizenRef);
    if (checkResult == 2) {
        return 2;
    }

    // -------------------------------------------------------
    // Step 2: Check if citizen's storage has items -> replan
    // -------------------------------------------------------
    fn_InternalBasePtr_CheckPtr(entityIBP);
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;

    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x10));
    uintptr_t storageIBP[2] = {0};
    s_fn_FindComp_Storage(*(uintptr_t*)(citizenEntity + 0x10), storageIBP);

    fn_InternalBasePtr_CheckPtr(storageIBP);
    int storageCount = s_fn_StorageQueryCount((void*)*(uintptr_t*)storageIBP);
    if (storageCount >= 1) {
        fn_InternalBasePtr_CheckPtr(entityIBP);
        s_fn_CitizenAI_LongTermPlan(*(void**)entityIBP);
        return 2;
    }

    // -------------------------------------------------------
    // Step 3: Find material description and herd component
    // -------------------------------------------------------
    fn_InternalBasePtr_CheckPtr((uintptr_t*)(selfAddr + 0x10));
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    fn_InternalBasePtr_CheckPtr((uintptr_t*)(selfAddr + 0x10));
    uintptr_t herdIBP[2] = {0};
    s_fn_FindComp_Pasture(*(uintptr_t*)(selfAddr + 0x10), herdIBP);

    // Get the herd object, check if it's derived from HerdComponent
    fn_InternalBasePtr_CheckPtr(herdIBP);
    void* herdObj = *(void**)(*(uintptr_t*)herdIBP + 0x18);
    bool isHerd = fn_IsDerivedFrom(herdObj, *s_typeId_HerdComponent);
    void* herdComp = isHerd ? herdObj : nullptr;

    // Get area component from the workplace entity
    fn_InternalBasePtr_CheckPtr((uintptr_t*)(selfAddr + 0x10));
    uintptr_t areaIBP[2] = {0};
    s_fn_GetAreaComponent(*(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x10) + 0x28), areaIBP);

    // -------------------------------------------------------
    // Step 4: Check animal population via resource tracker
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)herdComp + 0x24);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // -------------------------------------------------------
    // Step 5: Set mesh visibility based on capacity (visGroup 2)
    // -------------------------------------------------------
    fn_InternalBasePtr_CheckPtr((uintptr_t*)(selfAddr + 0x10));
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*(uintptr_t*)(selfAddr + 0x10), meshIBP);
    fn_InternalBasePtr_CheckPtr(meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 6: Check max workers vs current count
    // -------------------------------------------------------
    int huntCount = (int)selfQ[10]; // self->_field50

    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    if (huntCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 7: Random search for valid hunting cell
    // -------------------------------------------------------
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void* rng = *(void**)(gameWorld + 0xd8);

    int areaRadius = *(int*)((uintptr_t)areaIBP[0] + 0x28);
    int radiusSq = areaRadius * areaRadius;
    int mapSize = *mapGrid;

    // Workplace grid cell position
    uintptr_t workCell = selfQ[5]; // self->_field28
    int cellCol = (int)workCell % mapSize;
    int cellRow = (int)workCell / mapSize;

    // Search loop
    // The original code uses a do-while for rejection sampling within radius
    // and map bounds, then checks terrain/path viability
    int randX, randY;
    int testCellX, testCellY;

    // Generate random position within radius (rejection sampling)
    do {
        do {
            randX = s_fn_Random_RandTol(rng, areaRadius);
            randY = s_fn_Random_RandTol(rng, areaRadius);
            testCellX = cellCol + randX;
            testCellY = cellRow + randY;
        } while (radiusSq < randY * randY + randX * randX);
    } while (testCellX < 0 || testCellX >= mapSize ||
             testCellY < 0 || testCellY >= mapSize);

    uint32_t testCell = (uint32_t)mapSize * testCellY + testCellX;

    // Find nearest passable cell
    // Get citizen's current position for pathfinding
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citEnt = *(uintptr_t*)citizenIBP;

    fn_InternalBasePtr_CheckPtr((void*)(citEnt + 0x278));
    uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(aiComp + 0x68));
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));

    // Clamp citizen cell to map bounds
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;

    int citizenGridCell = mapSz * citizenCellY + citizenCellX;

    int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, testCell, citizenGridCell, 0xe07);
    if (nearestCell >= 0) {
        bool canPath = s_fn_Map_CanPathTo(mapGrid, 0xe1f, nearestCell, citizenGridCell);
        if (canPath) {
            // -------------------------------------------------------
            // Valid hunt target found -- create PerformWork action
            // -------------------------------------------------------
            float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
            float huntPos[3];
            s_fn_Vector3_Ctor(huntPos,
                (float)(nearestCell % *mapGrid) - halfOffset,
                (float)(nearestCell / *mapGrid) - halfOffset,
                0.0f);

            // Get terrain height
            float pathPos[3];
            s_fn_GetTerrainPos(mapGrid, pathPos, huntPos, 0, nullptr);

            // Create PerformWork action (hunt at position)
            uintptr_t huntEntityRef = *(uintptr_t*)citizenIBP;
            uintptr_t huntBuf[2] = { huntEntityRef, 0 };
            // Target is the herd entity -- use entityIBP for the work target
            fn_InternalBasePtr_CheckPtr(entityIBP);
            s_fn_PerformWork_Ctor3(
                huntBuf,               // entityIBP
                (int)selfQ[4],         // priority (workplaceId)
                citizenIBP,            // targetIBP
                pathPos,               // position
                0,                     // flag
                1);                    // param6

            // Assign work and register with AI
            int priority = (int)selfQ[4];
            fn_InternalBasePtr_CheckPtr(citizenIBP);
            uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

            uintptr_t vtable = *(uintptr_t*)self;
            typedef uintptr_t* (*GetWorkEntity_t)(HuntWork*, void*);
            auto getWorkEntity = *(GetWorkEntity_t*)(vtable + 0x50);
            uintptr_t workEntIBP[2] = {0};
            uintptr_t* wResult = getWorkEntity(self, workEntIBP);
            uintptr_t workEnt = *wResult;

            *(uint32_t*)(citizenEnt + 0x54) |= 4;
            *(uint32_t*)(citizenEnt + 0xb0) = 0;
            *(int32_t*)(citizenEnt + 0x30) = priority;

            if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
            }

            s_fn_CitizenAI_RegisterWork((void*)(selfAddr + 0x48), workEntIBP, citizenIBP);
            return 2;
        }
    }

    // -------------------------------------------------------
    // No valid target found -- return 1 (done/no work)
    // -------------------------------------------------------
    return 1;
}


// ============================================================
// HuntWork::Slot6 -- NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// Calls BSTFindRemove on the BST at self+0x48, returns false.
// ============================================================
bool HuntWork_Slot6_Impl(HuntWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// HuntWork::Slot7 -- 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// ============================================================
void HuntWork_Slot7_Impl(HuntWork* self, void* param2) {
    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x48), param2);
}


// ============================================================
// HuntWork::Slot8 / CancelAll (0x1400EF530)
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
// ============================================================
void HuntWork_Slot8_Impl(HuntWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find leftmost node in BST (following +0x10 = left child)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        fn_InternalBasePtr_CheckPtr(leftmost);
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
}


// ============================================================
// HuntWork::Slot9 / RemoveInvalid (0x1400EED90)
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if all valid (removeCount == 0), false otherwise.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP).
// ============================================================
bool HuntWork_Slot9_Impl(HuntWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    void** bstRootPtr = (void**)((uintptr_t)self + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;  // Empty tree -- nothing to validate
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
                    s_fn_MemFree(removeArr);
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
                fn_InternalBasePtr_CheckPtr(searchNode);
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                } else {
                    fn_InternalBasePtr_CheckPtr(&removeArr[i]);
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_BSTRemove(bstRootPtr, searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                }
            }
        }
    }

    s_fn_MemFree(removeArr);
    return removeCount == 0;
}


// ============================================================
// HuntWork::Slot10 / GetWorkEntity (0x1400EF5A0)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* HuntWork_Slot10_Impl(HuntWork* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    fn_InternalBasePtr_CheckPtr(result);

    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// HuntWork::Slot11 / HasWorkers -- 9-byte thunk, too small to hook.
// Returns (this->_field50 > 0).
// ============================================================
bool HuntWork_Slot11_Impl(HuntWork* self) {
    return self->_field50 > 0;
}


// ============================================================
// HuntWork::Execute (Slot5) -- 3 bytes, returns false.
// ============================================================
bool HuntWork_Execute_Impl(HuntWork* /*self*/) {
    return false;
}


// ============================================================
// HuntWork::Serialize (0x1400EF600)
// Work::Serialize + Serializer(version=0) + BST serialize.
// Note: No extra int field (unlike HerbalistWork/ForesterWork).
// ============================================================
void HuntWork_Serialize_Impl(HuntWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create first serializer (version block, version=0)
    uint8_t serializer1[24] = {0};
    s_fn_Serializer_Ctor(serializer1, stream, 0);
    s_fn_Serializer_Dtor(serializer1);

    // Create second serializer (version=0) for BST
    uint8_t serializer2[24] = {0};
    s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Serialize BST at self+0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializer2);

    s_fn_Serializer_Dtor(serializer2);
}


// ============================================================
// HuntWork::DtorBody (0x1400EF680)
// Sets vtables down chain, frees tree nodes at +0x48, then calls
// AutoPickupWork -> Work -> Internal dtor chain.
// ============================================================
void HuntWork_DtorBody(HuntWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to HuntWork
    *(void**)selfAddr = s_vtable_HuntWork;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        s_fn_TreeNodeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to AutoPickupWork, then Work
    *(void**)selfAddr = s_vtable_AutoPickupWork;
    *(void**)selfAddr = s_vtable_Work;

    // The original does a tail-call JMP to Object::Internal::~Internal.
    // We rely on the ScalarDtor wrapper to handle the free after this returns.
}


// ============================================================
// HuntWork::ScalarDtor (0x14002A700)
// Calls dtor body, then conditional free based on flags.
// ============================================================
void* HuntWork_ScalarDtor_Impl(HuntWork* self, uint32_t flags) {
    HuntWork_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}
