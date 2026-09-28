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

#include "Work/HuntWork.h"
#include "Work/huntwork_funcs.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/HuntDescription.h"   // ENDGAME B.3.9 D5: HuntDescription::TypeId()

// ── Slot4 destination census (BANISHED_CROPFIELD_TRACE, the hunt trace flag). ──
// Retail's sub_bbc0 builds TWO different chains from this function's last argument:
//   param6=1 -> MoveTo(chosen cell, Slow, 0xe1f) + HuntAnimal chained behind it
//   param6=0 -> PerformWork(Time(5.0s)) IN PLACE  + HuntAnimal chained behind it

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
typedef float*(*HuW_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
typedef void  (*HuW_CreateHuntAction_t)(void* citizenEntityIBP, int priority, void* huntCompIBP,
                                        float* position, float param5, char param6);
typedef uint32_t (*HuW_Entity_GetGridCell_t)(void* entityIBP, float* outPos);
typedef void  (*HuW_CitizenAI_RegisterWork_t)(void* citizenAISub, void* entityIBP, void* citizenIBP);
typedef void  (*HuW_CitizenAI_LongTermPlan_t)(void* citizenEntity);
typedef void* (*HuW_FindComp_Hunt_t)(uintptr_t entity, void* outIBP);
typedef void* (*HuW_GetAreaComponent_t)(uintptr_t entityPtr, void* outIBP);
typedef int   (*HuW_Map_CalcDistance_t)(int* mapGrid, uint32_t cellA, uint32_t cellB);
typedef int   (*HuW_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool  (*HuW_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);

// Work::Serialize
typedef void  (*HuW_Work_Serialize_t)(void* self, void* stream);
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
static HuW_GetTerrainPos_t              s_fn_GetTerrainPos = nullptr;
static HuW_CreateHuntAction_t          s_fn_CreateHuntAction = nullptr;
static HuW_CitizenAI_RegisterWork_t     s_fn_CitizenAI_RegisterWork = nullptr;
static HuW_CitizenAI_LongTermPlan_t     s_fn_CitizenAI_LongTermPlan = nullptr;
static HuW_FindComp_Hunt_t           s_fn_FindComp_Hunt = nullptr;
static HuW_GetAreaComponent_t           s_fn_GetAreaComponent = nullptr;
static HuW_Map_CalcDistance_t            s_fn_Map_CalcDistance = nullptr;
static HuW_Map_FindNearestPassable_t    s_fn_Map_FindNearestPassable = nullptr;
static HuW_Map_CanPathTo_t              s_fn_Map_CanPathTo = nullptr;
static HuW_Entity_GetGridCell_t         s_fn_Entity_GetGridCell = nullptr;
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

// Float constants from .rdata
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: HuntWork_Ctor
// disassembles to `0x1400ef749: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.

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
    s_fn_CreateHuntAction = reinterpret_cast<HuW_CreateHuntAction_t>(
        ResolveAddr(Module::Exe, 0x1400bbbc0));
    s_fn_CitizenAI_RegisterWork = reinterpret_cast<HuW_CitizenAI_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));
    s_fn_CitizenAI_LongTermPlan = reinterpret_cast<HuW_CitizenAI_LongTermPlan_t>(
        ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FindComp_Hunt = reinterpret_cast<HuW_FindComp_Hunt_t>(
        ResolveAddr(Module::Exe, 0x14002a730));
    s_fn_GetAreaComponent = reinterpret_cast<HuW_GetAreaComponent_t>(
        ResolveAddr(Module::Exe, 0x1400063d0));
    s_fn_Map_CalcDistance = reinterpret_cast<HuW_Map_CalcDistance_t>(
        ResolveAddr(Module::Exe, 0x140008830));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HuW_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HuW_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_Entity_GetGridCell = reinterpret_cast<HuW_Entity_GetGridCell_t>(
        ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_BST_Serialize = reinterpret_cast<HuW_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));  // verified from original disasm
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

    // Static MemAlloc/MemFree. These previously read exe dispatch slots 0x140151460 /
    // 0x140151468; 0x140151460 is NOT MemAlloc, it is Graphics::Geometry::GetTransform
    // (see placementtool.cpp:78). Since that slot is registered, the pointer was
    // non-null and WRONG, so the first array grow called GetTransform(size, align) and
    // took an ACCESS_VIOLATION. Diagnosed in tradework.cpp, where a TradeWork actually
    // reached its grow path; the same wrong pair was copied into five work classes.
    s_fn_MemAlloc = reinterpret_cast<HuW_MemAlloc_t>(fn_MemAllocStatic);
    s_fn_MemFree  = reinterpret_cast<HuW_MemFree_t>(fn_MemFreeStatic);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<HuW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Float constants from .rdata
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable pointers for dtor

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_FindComp_Storage &&
              s_fn_StorageQueryCount &&
              s_fn_GetTerrainPos &&
              s_fn_CreateHuntAction && s_fn_CitizenAI_RegisterWork &&
              s_fn_CitizenAI_LongTermPlan && s_fn_FindComp_Hunt &&
              s_fn_GetAreaComponent && s_fn_Map_CalcDistance &&
              s_fn_Map_FindNearestPassable && s_fn_Map_CanPathTo &&
              s_fn_BST_Serialize &&
              s_fn_ValidateEntity && s_fn_BSTRemove &&
              s_fn_BSTFindRemove && s_fn_CancelAllWork &&
              s_fn_FindSubEntity && s_fn_TreeNodeCleanup &&
              fn_IsDerivedFrom &&
              g_pGameWorldSingleton &&
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
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_CreateHuntAction) printf("  MISSING: PerformWork_Ctor3\n");
        if (!s_fn_CitizenAI_RegisterWork) printf("  MISSING: CitizenAI_RegisterWork\n");
        if (!s_fn_CitizenAI_LongTermPlan) printf("  MISSING: CitizenAI_LongTermPlan\n");
        if (!s_fn_FindComp_Hunt) printf("  MISSING: FindComp_Hunt\n");
        if (!s_fn_GetAreaComponent) printf("  MISSING: GetAreaComponent\n");
        if (!s_fn_Map_CalcDistance) printf("  MISSING: Map_CalcDistance\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
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
int HuntWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

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
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;

    uintptr_t storageIBP[2] = {0};
    s_fn_FindComp_Storage(*(uintptr_t*)(citizenEntity + 0x10), storageIBP);

    int storageCount = s_fn_StorageQueryCount((void*)*(uintptr_t*)storageIBP);
    if (storageCount >= 1) {
        s_fn_CitizenAI_LongTermPlan(*(void**)entityIBP);
        return 2;
    }

    // -------------------------------------------------------
    // Step 3: Find material description and herd component
    // -------------------------------------------------------
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    uintptr_t herdIBP[2] = {0};
    s_fn_FindComp_Hunt(*(uintptr_t*)(selfAddr + 0x10), herdIBP);

    // Get the herd object, check if it's derived from HerdComponent
    void* herdObj = *(void**)(*(uintptr_t*)herdIBP + 0x18);
    // DAT_1401c78c8 = HuntDescription (ENDGAME B.3.9 D5: by cell, not the legacy
    // variable name "HerdComponent").
    bool isHerd = fn_IsDerivedFrom(herdObj, HuntDescription::TypeId());
    void* herdComp = isHerd ? herdObj : nullptr;

    // Get area component from the workplace entity
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
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*(uintptr_t*)(selfAddr + 0x10), meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 6: Check max workers vs current count
    // -------------------------------------------------------
    int huntCount = (int)selfQ[10]; // self->_field50

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

    // Generate random position within radius (rejection sampling)
    // Original only bounds-checks Y; we check both for safety but the
    // else branch below handles the case where the cell is invalid.
    int randX, randY;
    int testCellX, testCellY;

    do {
        do {
            randX = reinterpret_cast<System::Random*>(rng)->RandTol(areaRadius);   // B.3.4b bridge
            randY = reinterpret_cast<System::Random*>(rng)->RandTol(areaRadius);   // B.3.4b bridge
            testCellX = cellCol + randX;
            testCellY = cellRow + randY;
        } while (radiusSq < randY * randY + randX * randX);
    } while (testCellY < 0 || testCellY >= mapSize);
    // Note: X bounds not checked here matches original behavior.
    // Negative X will fail terrain/entity checks -> else branch handles it.

    uint32_t testCell = (uint32_t)(mapSize * testCellY + testCellX);

    // Get citizen's current position for pathfinding
    uintptr_t citEnt = *(uintptr_t*)citizenIBP;

    uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);
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

    // Get workplace entity's grid cell for distance check
    uintptr_t entityIBPBuf[2] = { *(uintptr_t*)(selfAddr + 0x10), 0 };
    uint32_t entityGridCell = s_fn_Entity_GetGridCell(entityIBPBuf, nullptr);

    // -------------------------------------------------------
    // Step 8: Validate cell — terrain, distance, entity, passability
    // Original checks all 4 conditions; if ANY fail, uses fallback action.
    // -------------------------------------------------------
    uintptr_t gridData = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    bool allChecksPass = false;

    // Only check if cell index is in bounds (negative X can produce bad index)
    if (testCellX < 0 || testCellX >= mapSize) {
    } else {
        // Check 1: terrain type — skip resource terrain (0x1e0 mask)
        uint8_t terrainByte = *(uint8_t*)(gridData + (uintptr_t)testCell * 0x30 + 0x29);
        uint32_t terrainBit = 1u << (terrainByte & 0x1f);
        bool terrainOk = (terrainBit & 0x1e0u) == 0;

        // Check 2: distance from workplace to random cell
        int dist = s_fn_Map_CalcDistance(mapGrid, entityGridCell, testCell);
        bool distOk = dist <= areaRadius * 2;

        // Check 3: no entity at the cell
        uintptr_t cellEntity = *(uintptr_t*)(gridData + (uintptr_t)testCell * 0x30 + 8);
        bool emptyCell = (cellEntity == 0);

        // Attribute the failure to the FIRST failing check only, so the columns sum to
        // the fallback count instead of double-counting a cell that fails several.

        // Check 4: passability from random cell to citizen
        if (terrainOk && distOk && emptyCell) {
            bool canPath = (char)s_fn_Map_CanPathTo(mapGrid, 0xe1f, testCell, citizenGridCell) != 0;
            allChecksPass = canPath;
        }
    }


    if (allChecksPass) {
        // -------------------------------------------------------
        // Valid hunt target — create hunt action at terrain position
        // -------------------------------------------------------
        float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
        Math::Vector3 huntPos(
            (float)((int)testCell % *mapGrid) - halfOffset,
            (float)((int)testCell / *mapGrid) - halfOffset,
            0.0f);

        // Get terrain height
        float pathPos[3];
        s_fn_GetTerrainPos(mapGrid, pathPos, &huntPos.x, 0, nullptr);

        uintptr_t citizenEntRef = *(uintptr_t*)citizenIBP;

        uintptr_t huntCompIBP2 = 0;
        s_fn_FindComp_Hunt(*(uintptr_t*)(selfAddr + 0x10), &huntCompIBP2);

        s_fn_CreateHuntAction(
            &citizenEntRef,        // citizen entity IBP
            (int)selfQ[4],         // priority
            &huntCompIBP2,         // hunt component IBP
            pathPos,               // terrain position
            0.0f,                  // float param
            1);                    // char param (specific location)
    } else {
        // -------------------------------------------------------
        // Cell checks failed — create fallback hunt action
        // Position (0,0,0), g_pAgeMultiplier float, char=0 (roaming)
        // -------------------------------------------------------
        uintptr_t citizenEntRef2 = *(uintptr_t*)citizenIBP;

        uintptr_t huntCompIBP3 = 0;
        uintptr_t workplaceEnt = *(uintptr_t*)(selfAddr + 0x10);
        s_fn_FindComp_Hunt(workplaceEnt, &huntCompIBP3);

        float fallbackPos[3] = { 0.0f, 0.0f, 0.0f };

        s_fn_CreateHuntAction(
            &citizenEntRef2,       // citizen entity IBP
            (int)selfQ[4],         // priority
            &huntCompIBP3,         // hunt component IBP
            fallbackPos,           // fallback position (0,0,0)
            *g_pAgeMultiplier,     // float param (roaming radius)
            0);                    // char param (roaming mode)
    }

    // -------------------------------------------------------
    // Both paths: assign work and return 2
    // -------------------------------------------------------
    int priority = (int)selfQ[4];
    uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

    uintptr_t workEntIBP[2] = {0};
    uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
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


// ============================================================
// HuntWork::Slot6 -- NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// Calls BSTFindRemove on the BST at self+0x48, returns false.
// ============================================================
int HuntWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);
    return false;
}


// ============================================================
// HuntWork::Slot7 -- 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// ============================================================
int HuntWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_BSTFindRemove((void*)((uintptr_t)this + 0x48), param2);
    return 0;
}


// ============================================================
// HuntWork::Slot8 / CancelAll (0x1400EF530)
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
// ============================================================
int HuntWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
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
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HuntWork::Slot9 / RemoveInvalid (0x1400EED90)
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if all valid (removeCount == 0), false otherwise.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP).
// ============================================================
int HuntWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

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
        bool stillValid = s_fn_ValidateEntity(selfAddr, nullptr, (void*)&entityIBP, nullptr);

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
// HuntWork::Slot10 / GetWorkEntity (0x1400EF5A0)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* HuntWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// HuntWork::GetPriority [11] 0x140029860 (shared by 13 tables) / HasWorkers.
// Returns (this->_field50 > 0). Slot 5 is Work's (0x1400f7540): the old dead
// HuntWork_Execute_Impl copy of it is gone.
// ============================================================
int HuntWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}


// ============================================================
// HuntWork::Serialize (0x1400EF600)
// Work::Serialize + Serializer(version=0) + BST serialize.
// Note: No extra int field (unlike HerbalistWork/ForesterWork).
// ============================================================
void HuntWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create first serializer (version block, version=0)
    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Create second serializer (version=0) for BST
    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at self+0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializer2);

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// HuntWork::DtorBody (0x1400EF680)
// Sets vtables down chain, frees tree nodes at +0x48, then calls
// AutoPickupWork -> Work -> Internal dtor chain.
// ============================================================
// HuntWork dtor (retail 0x1400ef680, called by slot 0 0x14002a700). The stamps and the `~Internal` call are the compiler's chain.
HuntWork::~HuntWork() {
    uintptr_t selfAddr = (uintptr_t)this;

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

}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's. HuntWork_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
HuntWork::HuntWork() : AutoPickupWork(), _field48(nullptr), _field50(0) {
    HuntWork_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_HuntWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over HuntWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a690: MemAlloc(sizeof, 8), then the ctor.
void* HuntWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HuntWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HuntWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* HuntWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HuntWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(HuntWork, "AutoPickupWork")
