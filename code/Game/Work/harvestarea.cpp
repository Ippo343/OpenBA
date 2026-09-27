// code/Game/Work/harvestarea.cpp
// Clean C++ reimplementation of HarvestArea Work class.
// Source: Game/Work/HarvestArea.cpp (from profile build debug strings)
//
// HarvestArea (0x68 bytes) -- Crop/field harvest work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestArea
// Vtable: 0x1401614a0
//
// Struct layout:
//   +0x00: vtable
//   +0x08: Work/AutoPickupWork base (0x40 bytes of fields)
//   +0x48: BST root (tree of assigned work items, int-keyed)
//   +0x50: BST count (int)
//   +0x58: validity array ptr (byte array, one per field cell)
//   +0x60: field cell count (int)
//
// Methods implemented:
//   Slot4 (0x1400ECCD0) -- main crop harvest assignment logic
//   Slot6 (0x1400ECC20) -- tree insert with paired slot removal
//   Slot7 (0x1400ECB60) -- mark cell valid, remove paired entry
//   Slot8 (0x1400ED270) -- CancelAll
//   Slot9 (0x1400EC930) -- RemoveInvalid (hook disabled, bug class 2)
//   Slot10 (0x1400ED2F0) -- GetWorkEntity
//   Serialize (0x1400ED350) -- serialization
//   ScalarDtor body (0x1400ED410) -- destructor
//

#include "Work/HarvestArea.h"
#include "Work/harvestarea_funcs.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/CropFieldDescription.h"   // ENDGAME B.3.9 D5: CropFieldDescription::TypeId()

// ============================================================
// Local function pointers (resolved once by HarvestArea_ResolveFuncs)
// ============================================================

// AutoPickupWork::CheckAssignments (base class work check)
typedef int (*HA_AutoPickup_CheckAssignments_t)(uintptr_t self, void* citizenRef);
static HA_AutoPickup_CheckAssignments_t s_fn_AutoPickup_CheckAssignments = nullptr;

// FindComponent helpers
typedef void* (*HA_FindComp_MatDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_MeshInstance_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_CropDesc_t)(uintptr_t entity, void* outIBP);
typedef void* (*HA_FindComp_NatRes_t)(uintptr_t entity, void* outIBP);

static HA_FindComp_MatDesc_t s_fn_FindComp_MatDesc = nullptr;         // 0x140003050
static HA_FindComp_MeshInstance_t s_fn_FindComp_MeshInstance = nullptr; // 0x140002f90
static HA_FindComp_CropDesc_t s_fn_FindComp_CropDesc = nullptr;       // 0x140005380
static HA_FindComp_NatRes_t s_fn_FindComp_NatRes = nullptr;            // 0x1400052c0

// MeshInstance::SetVisible(mesh, group, visible)
typedef void (*HA_MeshInstance_SetVisible_t)(void* meshInst, uint8_t group, char visible);
static HA_MeshInstance_SetVisible_t s_fn_MeshInstance_SetVisible = nullptr; // 0x14012c3f0

// CalcMaxWorkers(matDesc) -> int
typedef int (*HA_CalcMaxWorkers_t)(uintptr_t matDesc);
static HA_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr; // 0x14012e5c0

// Map functions
typedef int (*HA_Map_FindNearestPassable_t)(int* mapGrid, int srcCell, int destCell, uint32_t flags);
typedef bool (*HA_Map_CanPathTo_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
static HA_Map_FindNearestPassable_t s_fn_Map_FindNearestPassable = nullptr; // 0x14009ccf0
static HA_Map_CanPathTo_t s_fn_Map_CanPathTo = nullptr;                     // 0x1400a00f0

// GetTerrainPos(mapGrid, outPos, gridPos, param4, outNormal)
typedef float* (*HA_GetTerrainPos_t)(int* mapGrid, float* outPos, float* gridPos,
                                     char param4, uint32_t* outNormal);
static HA_GetTerrainPos_t s_fn_GetTerrainPos = nullptr; // 0x14009f3e0

// ShowTool_Action_Ctor4 -- creates harvest action chain for crop areas
// Signature: (citizenIBP, priority, harvestMode, targetIBP, position, packedCellInfo, ???)
typedef void (*HA_ShowTool_Action_Ctor4_t)(void* citizenIBP, int priority, int harvestMode,
                                           void* targetIBP, float* position,
                                           uint32_t packedCellInfo, uint32_t param7);
static HA_ShowTool_Action_Ctor4_t s_fn_ShowTool_Action_Ctor4 = nullptr; // ShowTool_Action_Ctor4

// BST insert: TradeDescription_impl_9a40(treeRoot, outResult, keyData)
typedef void* (*HA_BST_Insert_t)(void* treeRoot, void* outResult, uint32_t* keyData);
static HA_BST_Insert_t s_fn_BST_Insert = nullptr; // TradeDescription_impl_9a40

// BST remove: TradeDescription_impl_ca60(treeRoot, node)
typedef void (*HA_BST_Remove_t)(void* treeRoot, void* node);
static HA_BST_Remove_t s_fn_BST_Remove = nullptr; // TradeDescription_impl_ca60

// BST serialize: TradeDescription_impl_9de0(treeRoot, serializer)
typedef void (*HA_BST_Serialize_t)(void* treeRoot, void* serializer);
static HA_BST_Serialize_t s_fn_BST_Serialize = nullptr; // TradeDescription_impl_9de0

// FUN_1400f7100 -- Work validate task helper
typedef uint8_t (*HA_ValidateTask_t)(uintptr_t self, uintptr_t param2, void* ibp, void** outTask);
static HA_ValidateTask_t s_fn_ValidateTask = nullptr;

// FUN_1400c5400 -- cancel work/task cleanup
typedef void (*HA_TaskCancel_t)(void* taskList);
static HA_TaskCancel_t s_fn_TaskCancel = nullptr;

// FUN_140002e10 -- FindPositionComponent on entity
typedef void* (*HA_FindPosComp_t)(uintptr_t entity, void* outIBP);
static HA_FindPosComp_t s_fn_FindPosComp = nullptr;


// Resize byte array (FUN_14000a590)
// 0x14000a590 (Array<uint8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::U8> member call, so its resolved-pointer stand-in
// is gone.

// BST recursive free (FUN_14000e320)
typedef void (*HA_BST_RecursiveFree_t)(void* treeRoot, void* node);
static HA_BST_RecursiveFree_t s_fn_BST_RecursiveFree = nullptr;


// MemAlloc / MemFree
typedef void* (*HA_MemAlloc_t)(int64_t size, int align);
static HA_MemAlloc_t s_fn_MemAlloc = nullptr;
static void* HA_MemAllocViaSingleton(int64_t size, int align) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, align) : nullptr;
}

// Type IDs
// (s_typeId_NatResComp not needed -- NatRes lookup done via s_fn_FindComp_NatRes)

// Float constants
static float* s_fHalfCellOffset = nullptr;  // DAT_140172114 = 0.5f

// Vtable pointers for dtor chain

static bool s_ha_funcs_resolved = false;

// ============================================================
// Resolution
// ============================================================
bool HarvestArea_ResolveFuncs() {
    if (s_ha_funcs_resolved) return true;

    s_fn_AutoPickup_CheckAssignments = reinterpret_cast<HA_AutoPickup_CheckAssignments_t>(
        ResolveAddr(Module::Exe, 0x1400e3db0));
    s_fn_FindComp_MatDesc = reinterpret_cast<HA_FindComp_MatDesc_t>(
        ResolveAddr(Module::Exe, 0x140003050));
    s_fn_FindComp_MeshInstance = reinterpret_cast<HA_FindComp_MeshInstance_t>(
        ResolveAddr(Module::Exe, 0x140002f90));
    s_fn_FindComp_CropDesc = reinterpret_cast<HA_FindComp_CropDesc_t>(
        ResolveAddr(Module::Exe, 0x140005380));
    s_fn_FindComp_NatRes = reinterpret_cast<HA_FindComp_NatRes_t>(
        ResolveAddr(Module::Exe, 0x1400052c0));
    s_fn_MeshInstance_SetVisible = reinterpret_cast<HA_MeshInstance_SetVisible_t>(
        ResolveAddr(Module::Exe, 0x14012c3f0));
    s_fn_CalcMaxWorkers = reinterpret_cast<HA_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_Map_FindNearestPassable = reinterpret_cast<HA_Map_FindNearestPassable_t>(
        ResolveAddr(Module::Exe, 0x14009ccf0));
    s_fn_Map_CanPathTo = reinterpret_cast<HA_Map_CanPathTo_t>(
        ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_GetTerrainPos = reinterpret_cast<HA_GetTerrainPos_t>(
        ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_ShowTool_Action_Ctor4 = reinterpret_cast<HA_ShowTool_Action_Ctor4_t>(
        ResolveAddr(Module::Exe, 0x1400c1530));  // was 0x1400c1720 (mid-function!)
    s_fn_BST_Insert = reinterpret_cast<HA_BST_Insert_t>(
        ResolveAddr(Module::Exe, 0x140029a40));
    s_fn_BST_Remove = reinterpret_cast<HA_BST_Remove_t>(
        ResolveAddr(Module::Exe, 0x14001ca60));  // was 0x140029a60 (wrong function!)
    s_fn_BST_Serialize = reinterpret_cast<HA_BST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x140029de0));
    s_fn_ValidateTask = reinterpret_cast<HA_ValidateTask_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_TaskCancel = reinterpret_cast<HA_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindPosComp = reinterpret_cast<HA_FindPosComp_t>(
        ResolveAddr(Module::Exe, 0x140002e10));
    s_fn_BST_RecursiveFree = reinterpret_cast<HA_BST_RecursiveFree_t>(
        ResolveAddr(Module::Exe, 0x14000e320));

    // Dispatch slot 0x140152780 is MISLABELED "MemAlloc" (it is Vector2::Max, see
    // reg_player_screenrect.cpp) and is deliberately left unregistered, so the old
    // `*ResolveAddr(...)` read here yielded NULL and the collect-array grow in the
    // Validate slot called it: EXEC of 0 the first harvest season a field had stale
    // tasks (BANISHED_STRESS town at 10x, 2026-09-03). Allocate through the real
    // allocator singleton instead; fn_MemFreeStatic pairs with it.
    s_fn_MemAlloc = &HA_MemAllocViaSingleton;

    // Float constants
    s_fHalfCellOffset = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x140172114));

    // Vtable addresses
    // ⚠ Were 0x140161380 / 0x140161300 — RTTI type descriptors ("ForesterWork\0" /
    // "FishingWork\0"), not vtables; same adjacency error harvestorchard.cpp carried.
    // The real base vtables are AutoPickupWorkAddr::Vtable / WorkAddr::Vtable.

    // Verify critical pointers
    bool ok = s_fn_AutoPickup_CheckAssignments && s_fn_FindComp_MatDesc &&
              s_fn_FindComp_MeshInstance && s_fn_FindComp_CropDesc &&
              s_fn_FindComp_NatRes && s_fn_MeshInstance_SetVisible &&
              s_fn_CalcMaxWorkers && s_fn_Map_FindNearestPassable &&
              s_fn_Map_CanPathTo && s_fn_GetTerrainPos &&
              s_fn_ShowTool_Action_Ctor4 &&
              s_fn_BST_Insert && s_fn_BST_Remove && s_fn_BST_Serialize &&
              s_fn_ValidateTask && s_fn_TaskCancel && s_fn_FindPosComp &&
              s_fn_BST_RecursiveFree &&
              fn_InternalBasePtr_Ctor &&
              fn_IsDerivedFrom && g_pGameWorldSingleton && s_fHalfCellOffset;

    s_ha_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[HarvestArea] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_AutoPickup_CheckAssignments) printf("  MISSING: AutoPickup_CheckAssignments\n");
        if (!s_fn_FindComp_MatDesc) printf("  MISSING: FindComp_MatDesc\n");
        if (!s_fn_FindComp_MeshInstance) printf("  MISSING: FindComp_MeshInstance\n");
        if (!s_fn_FindComp_CropDesc) printf("  MISSING: FindComp_CropDesc\n");
        if (!s_fn_FindComp_NatRes) printf("  MISSING: FindComp_NatRes\n");
        if (!s_fn_MeshInstance_SetVisible) printf("  MISSING: MeshInstance_SetVisible\n");
        if (!s_fn_CalcMaxWorkers) printf("  MISSING: CalcMaxWorkers\n");
        if (!s_fn_Map_FindNearestPassable) printf("  MISSING: Map_FindNearestPassable\n");
        if (!s_fn_Map_CanPathTo) printf("  MISSING: Map_CanPathTo\n");
        if (!s_fn_GetTerrainPos) printf("  MISSING: GetTerrainPos\n");
        if (!s_fn_ShowTool_Action_Ctor4) printf("  MISSING: ShowTool_Action_Ctor4\n");
        if (!s_fn_BST_Insert) printf("  MISSING: BST_Insert\n");
        if (!s_fn_BST_Remove) printf("  MISSING: BST_Remove\n");
        if (!s_fn_BST_Serialize) printf("  MISSING: BST_Serialize\n");
        if (!s_fn_ValidateTask) printf("  MISSING: ValidateTask\n");
        if (!s_fn_TaskCancel) printf("  MISSING: TaskCancel\n");
        if (!s_fn_FindPosComp) printf("  MISSING: FindPosComp\n");
        if (!s_fn_BST_RecursiveFree) printf("  MISSING: BST_RecursiveFree\n");
    }
#endif

    return ok;
}


// ============================================================
// Helper: BST search for int key
// Returns node pointer if found, nullptr otherwise.
// BST node layout: [0]=int key, [+8]=IBP data, [+0x10]=parent, [+0x18]=left, [+0x20]=right
// ============================================================
static void* BST_Find(void* root, int key) {
    int* node = (int*)root;
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);  // left child at +0x18
        } else if (key > *node) {
            node = *(int**)(node + 8);  // right child at +0x20
        } else {
            return node;  // found
        }
    }
    return nullptr;
}


// ============================================================
// HarvestArea::Slot4 -- Main crop harvest assignment logic
// ============================================================
//
// Algorithm:
// 1. Check auto-pickup assignments
// 2. Find MaterialDescription, CropDescription on workplace entity
// 3. Check capacity
// 4. Iterate field cells (0..cellCount), find unharvested crops
// 5. For each valid cell: check pathing, check NaturalResource status
// 6. If pairable cell available, harvest two at once; else harvest one
// 7. Create harvest action, assign work, register with citizen AI
//
// Harvest-assignment census counters. State 11 posts this work and its Execute runs
// every frame; "runs but produces nothing" has EIGHT possible causes here and they need
// different fixes, so each is counted separately. Two of them (atCapacity, workerCap)
// return before the cell loop is even reached.
static long long g_ha_calls = 0, g_ha_auto = 0, g_ha_cap = 0, g_ha_wcap = 0,
                 g_ha_nomark = 0, g_ha_inbst = 0, g_ha_noent = 0;
[[maybe_unused]] static long long g_ha_nopass = 0;
[[maybe_unused]] static long long g_ha_nopath = 0, g_ha_ok = 0;

int HarvestArea::Execute(void* entityIBP, void* /*context*/, uint64_t /*arg3*/) {
    // Harvest-assignment census. State 11 posts this work; whether a citizen ever
    // ACCEPTS a harvest cell is the last unmeasured link before a deposit, and an
    // Execute that finds nothing looks exactly like an Execute that is never called.
    {
        ++g_ha_calls;
    }
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // Step 1: Check auto-pickup
    int checkResult = s_fn_AutoPickup_CheckAssignments(selfAddr, &citizenRef);
    if (checkResult == 2) {
        ++g_ha_auto;
        return 2;
    }

    // Step 2: Find components
    uintptr_t* workEntityPtr = (uintptr_t*)(selfAddr + 0x10);
    uintptr_t matDescIBP = 0;
    s_fn_FindComp_MatDesc(*workEntityPtr, &matDescIBP);

    uintptr_t cropDescIBP = 0;
    s_fn_FindComp_CropDesc(*workEntityPtr, &cropDescIBP);

    // Check CropDescription -> get description object
    void* descObj = *(void**)(cropDescIBP + 0x18);
    bool isDerived = fn_IsDerivedFrom(descObj, CropFieldDescription::TypeId());
    void* cropDesc = isDerived ? descObj : nullptr;

    // Material index for resource tracking
    int materialIdx = *(int*)((uintptr_t)cropDesc + 0x20);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // THE DEPOSIT METER. resEntry+0 is the town's current stock of this crop's material
    // and resEntry+8 its limit (that is exactly what the atCapacity test above compares).
    // Harvest is confirmed running -- 61 crops through the harvestFlag=1 arm -- so the
    // only question left is whether the raw material reaches STORAGE. Printing the stock
    // here answers it directly instead of by inference: a rising number IS the deposit.
    {
        [[maybe_unused]] const int cur = *(int*)(resEntry);
    }

    // Set mesh visibility (group 2) based on capacity
    uintptr_t meshIBP[2] = {0};
    s_fn_FindComp_MeshInstance(*workEntityPtr, meshIBP);
    s_fn_MeshInstance_SetVisible((void*)(*(uintptr_t*)meshIBP), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        ++g_ha_cap;
        return 1;
    }

    // Check tree count vs max workers
    int treeCount = (int)selfQ[10]; // _field50
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = s_fn_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }
    if (treeCount > *(int*)(matDescIBP + 0x30)) {
        ++g_ha_wcap;
        return 1;
    }

    // Step 3: Setup iteration
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    // Get citizen position
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

    int fieldCellCount = (int)selfQ[0xc]; // self->_field60 as int

    if (fieldCellCount > 0) {
        int cellIdx2 = 1; // paired cell index starts at 1
        for (int i = 0; i < fieldCellCount; i++) {
            // Check if this cell's validity flag is set
            char* validityArray = (char*)selfQ[0xb]; // self->_field58
            if (validityArray[i] == 0) {
                ++g_ha_nomark;
                goto next_cell;
            }

            // Check if cell index is already in the BST (already assigned)
            {
                int* bstRoot = (int*)selfQ[9]; // self->_field48
                void* found = BST_Find(bstRoot, i);
                if (found != nullptr) {
                    ++g_ha_inbst;
                    goto next_cell;
                }
            }

            // Check if paired cell is also available
            {
                bool pairedAvailable = false;
                if (cellIdx2 < fieldCellCount && validityArray[cellIdx2] != 0) {
                    int* bstRoot = (int*)selfQ[9];
                    void* found2 = BST_Find(bstRoot, cellIdx2);
                    if (found2 == nullptr) {
                        pairedAvailable = true;
                    }
                }

                // Look up entity at this cell in the map grid
                uintptr_t entityAtCell1 = 0;
                uintptr_t entityAtCell2 = 0;

                uintptr_t cropDescVal = cropDescIBP;
                uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20); // +0x80 byte offset
                int cellGridIdx = *(int*)((uintptr_t)i * 0x20 + *(uintptr_t*)(cropDescVal + 0x58));
                entityAtCell1 = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx * 0x30 + 8);

                if (entityAtCell1 == 0) {
                    // No entity at cell -- mark as unavailable
                    ++g_ha_noent;
                    validityArray[i] = 0;
                } else {
                    if (pairedAvailable) {
                        uintptr_t cropDescVal2 = cropDescIBP;
                        int cellGridIdx2 = *(int*)((uintptr_t)cellIdx2 * 0x20 + *(uintptr_t*)(cropDescVal2 + 0x58));
                        entityAtCell2 = *(uintptr_t*)(gridData + (uintptr_t)cellGridIdx2 * 0x30 + 8);
                        if (entityAtCell2 == 0) {
                            pairedAvailable = false;
                        }
                    }

                    // Check pathfinding from cell to citizen
                    int srcCellIdx = *(int*)(*(uintptr_t*)(cropDescIBP + 0x58) + (uintptr_t)i * 0x20);
                    int nearestCell = s_fn_Map_FindNearestPassable(mapGrid, srcCellIdx, citizenGridCell, 0xe0f);

                    if (nearestCell >= 0) {
                        uint64_t canPath = (uint64_t)s_fn_Map_CanPathTo(mapGrid, 0xe1f, citizenGridCell, nearestCell);
                        if ((char)canPath != 0) {
                            // Check NaturalResourceComponent on entity
                            // natResIBPArray must be contiguous: ShowTool_Action_Ctor4
                            // iterates param_4 as an array of harvestMode entries
                            uintptr_t natResIBPArray[2] = {0, 0};
                            uintptr_t& natResIBP1 = natResIBPArray[0];
                            uintptr_t& natResIBP2 = natResIBPArray[1];

                            uintptr_t natResCompIBP[2] = {0};
                            s_fn_FindComp_NatRes(entityAtCell1, natResCompIBP);
                            natResIBP1 = *(uintptr_t*)natResCompIBP;

                            if (pairedAvailable) {
                                uintptr_t natResCompIBP2[2] = {0};
                                s_fn_FindComp_NatRes(entityAtCell2, natResCompIBP2);
                                natResIBP2 = *(uintptr_t*)natResCompIBP2;
                            }

                            // Check flag bit 20 on NatRes (flagged for removal)
                            if (natResIBP1 == 0) {
                                // NatRes component gone — skip this cell
                                validityArray[i] = 0;
                                goto next_cell;
                            }
                            if ((*(uint32_t*)(natResIBP1 + 0x20) >> 20) & 1) {
                                goto handle_removal;
                            }

                            if (natResIBP2 != 0) {
                                if (natResIBP2 != 0 && (*(uint32_t*)(natResIBP2 + 0x20) >> 20) & 1) {
                                    pairedAvailable = false;
                                }
                            }

                            if (!pairedAvailable) {
                                // Single cell harvest -- need cell count for packed info
                                cellIdx2 = fieldCellCount;
                            }

                        handle_removal:
                            if (!((*(uint32_t*)(natResIBP1 + 0x20) >> 20) & 1)) {
                                // Valid cell -- create harvest action
                                float halfOffset = (float)(*mapGrid >> 1) - *s_fHalfCellOffset;
                                Math::Vector3 targetPos(
                                    (float)(nearestCell % *mapGrid) - halfOffset,
                                    (float)(nearestCell / *mapGrid) - halfOffset,
                                    0.0f);

                                float pathPos[4]; // extra padding for GetTerrainPos
                                s_fn_GetTerrainPos(mapGrid, pathPos, &targetPos.x, 0, nullptr);

                                // Create the harvest action
                                void* pCitizenIBP = citizenIBP;
                                uintptr_t citizenVal = *(uintptr_t*)pCitizenIBP;
                                uintptr_t actionBuf[2] = { citizenVal, 0 };

                                int harvestMode = pairedAvailable ? 2 : 1;
                                uint32_t packedCellInfo = ((uint32_t)cellIdx2 << 16) | (uint32_t)i;


                                s_fn_ShowTool_Action_Ctor4(
                                    actionBuf,                    // citizenIBP
                                    (int)selfQ[4],                // priority
                                    harvestMode,                  // 1=single, 2=paired
                                    natResIBPArray,               // target NatRes IBP array (harvestMode entries)
                                    pathPos,                      // position
                                    packedCellInfo,               // packed cell indices
                                    0);                           // param7

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

                                // Insert cell index into BST
                                uintptr_t insertResult[2] = {0};
                                uint32_t keyBuf[4] = { (uint32_t)i, 0, 0, 0 };
                                uintptr_t citizenIBPBuf = *(uintptr_t*)citizenIBP;
                                keyBuf[2] = (uint32_t)citizenIBPBuf;
                                keyBuf[3] = (uint32_t)(citizenIBPBuf >> 32);
                                s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult, keyBuf);

                                // Mark cell as used
                                validityArray[i] = 0;

                                // If paired, also insert and mark
                                if (pairedAvailable) {
                                    uintptr_t insertResult2[2] = {0};
                                    uint32_t keyBuf2[4] = { (uint32_t)cellIdx2, 0, 0, 0 };
                                    uintptr_t citizenIBPBuf2 = *(uintptr_t*)citizenIBP;
                                    keyBuf2[2] = (uint32_t)citizenIBPBuf2;
                                    keyBuf2[3] = (uint32_t)(citizenIBPBuf2 >> 32);
                                    s_fn_BST_Insert((void*)(selfAddr + 0x48), insertResult2, keyBuf2);
                                    validityArray[cellIdx2] = 0;
                                }

                                return 2;
                            }
                        }
                    }

                    // Restore citizenGridCell for next iteration
                    citizenGridCell = (uint32_t)(mapSz * citizenCellY + citizenCellX);
                }
            }

        next_cell:
            cellIdx2 = i + 2; // paired cell is always i+1 in iteration
            // Note: cellIdx2 tracks as fVar13 in decompilation (i+1 each iteration)
        }
    }

    return 1;
}


// Shared by slots 6 and 7: find `key` in the cell BST at +0x48 and remove it if present.
static void HarvestArea_RemoveCellNode(uintptr_t selfAddr, int key) {
    int* node = *(int**)(selfAddr + 0x48);
    while (node != nullptr) {
        if (key < *node) {
            node = *(int**)(node + 6);          // left  +0x18
        } else if (key > *node) {
            node = *(int**)(node + 8);          // right +0x20
        } else {
            s_fn_BST_Remove((void*)(selfAddr + 0x48), node);
            return;
        }
    }
}


// ============================================================
// HarvestArea::Slot6 -- task COMPLETED (retail 0x1400ecc20, vtable +0x30)
// ============================================================
// Removes the primary cell's node, then (if the paired index is in range) the paired
// cell's node. param4 is packed: low 16 = primary cell, high 16 = paired cell.
//
// ⚠ Retail touches the validity array (+0x58) NOT AT ALL here: a harvested cell is
// gone and must not be re-offered. Only the CANCEL slot below restores validity.
// This port had the two slots' bodies mixed: slot 6 carried slot 7's validity writes
// (harvested cells re-offered, Slot4 then found no entity and re-cleared them -- a
// wasted scan per completion) and slot 7 LACKED the primary-node removal (see below).
// Same Slot6/Slot7 mix-up HarvestOrchard and PlantArea each paid for once already.
int HarvestArea::OnComplete(void* /*param2*/, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    uintptr_t selfAddr = (uintptr_t)this;
    int primaryIdx = (int)(param4 & 0xFFFF);
    int pairedIdx  = (int)((param4 >> 16) & 0xFFFF);

    HarvestArea_RemoveCellNode(selfAddr, primaryIdx);
    if (pairedIdx < *(int*)(selfAddr + 0x60)) {
        HarvestArea_RemoveCellNode(selfAddr, pairedIdx);
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot7 -- task CANCELLED (retail 0x1400ecb60, vtable +0x38)
// ============================================================
// Removes the primary cell's node, marks the primary cell valid again, and (if the
// paired index is in range) marks the paired cell valid and removes its node too.
// param4 is packed: low 16 = primary cell, high 16 = paired cell.
//
// ⚠ THE PRIMARY-NODE REMOVAL IS LOAD-BEARING. Retail 0x1400ecb60 walks the tree for
// the primary key and calls ca60 on the hit BEFORE the validity writes. This port only
// removed the PAIRED node, so every cancelled harvest task left its primary
// {cell, citizen} node in the tree with the count at +0x50 still counting it:
//   * Slot9 (Validate) later handed that citizen pointer to Object::IsA. Once the
//     citizen had died and its block been recycled, IsA CALLed through recycled bytes
//     (the "READ of vtable+0x10" crash with RAX = 0000079700000552 -- a 40-byte record
//     array that reused the freed entity block; CheckPtr is a bare RET in the shipped
//     Runtime, so nothing nulls a dangling entry).
//   * Slot8 (CancelAll, teardown) picks the leftmost node, cancels that citizen's
//     action list and expects the cancel to reach this slot and drop the count. With
//     the node never removed the count never drops and it re-picks the same node
//     forever: the 100%-CPU quit hang after the [Residence] ERASE burst.
// Both symptoms are this one missing removal (2026-09-05).
int HarvestArea::OnCancel(void* /*param2*/, void* /*param3*/, int arg3) {
    const uint32_t param4 = static_cast<uint32_t>(arg3);
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;
    int primaryIdx = (int)(param4 & 0xFFFF);
    int pairedIdx  = (int)((param4 >> 16) & 0xFFFF);

    HarvestArea_RemoveCellNode(selfAddr, primaryIdx);

    // Cancelled: the cell goes back in the pool.
    char* validityArray = (char*)selfQ[0xb];
    validityArray[primaryIdx] = 1;

    if (pairedIdx < *(int*)(selfAddr + 0x60)) {
        validityArray[pairedIdx] = 1;
        HarvestArea_RemoveCellNode(selfAddr, pairedIdx);
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot8 -- CancelAll
// ============================================================
// Loops through BST (rightmost traversal), cancels each work item
// via citizen AI task cleanup.
int HarvestArea::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find rightmost node in BST
        uintptr_t node = *(uintptr_t*)(selfAddr + 0x48);
        uintptr_t rightmost = node;
        while (node != 0) {
            rightmost = node;
            node = *(uintptr_t*)(node + 0x18); // left child (finds leftmost actually)
        }

        // Cancel work via citizen AI
        uintptr_t citizenEntity = *(uintptr_t*)(rightmost + 8);
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        const int before = count;
        count = *(int*)(selfAddr + 0x50);

        // Cancelling made no progress: retail would spin here forever. The guard is a
        // deliberate deviation (it can only fire in a state that would otherwise hang).
        if (count == before) {
            break;
        }
    }
    return 0;
}


// ============================================================
// HarvestArea::Slot9 -- RemoveInvalid
// ============================================================
// Hook DISABLED (bug class 2: MOV RAX,RSP in prologue).
// Implemented for standalone exe.
//
// Iterates BST in-order, validates each entry via FUN_1400f7100,
// collects invalid ones, then removes them from BST.
int HarvestArea::Validate(void* param2) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    int* collectArray = nullptr;
    int* currentArray = nullptr;
    uint32_t collectCount = 0;
    uint32_t collectCapacity = 0;

    // In-order traversal of BST
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
            cur = *(int**)(cur + 6); // left child
        }
    }

    // Traverse in-order
    while (node != nullptr) {
        uintptr_t ibpData = *(uintptr_t*)(node + 2); // IBP at +0x8

        uint8_t valid = s_fn_ValidateTask(selfAddr, (uintptr_t)param2, (void*)&ibpData, nullptr);

        if (valid == 0) {
            // Invalid -- add to collect list
            uint32_t newCount = collectCount + 1;
            if (newCount != collectCount) {
                if ((int)collectCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (collectCount != 0) {
                        newCap = newCount * 3;
                    }
                    collectCapacity = newCap;
                    int* newArray = (int*)s_fn_MemAlloc((int64_t)newCap << 2, 4);
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
                        fn_MemFreeStatic(collectArray);
                    }
                    collectArray = newArray;
                } else {
                    currentArray = collectArray;
                    collectArray[collectCount] = *node;
                }
                collectCount = newCount;
            }

            // Mark validity flag
            char* validityArray = (char*)selfQ[0xb];
            validityArray[*node] = 1;
        }

        // Move to next in-order node
        int* right = *(int**)(node + 8); // right child
        if (right != nullptr) {
            // Go to leftmost of right subtree
            int* cur = right;
            while (cur != nullptr) {
                node = cur;
                cur = *(int**)(cur + 6);
            }
        } else {
            // Go up until we find a node we're the left child of
            int* parent = *(int**)(node + 4); // parent
            if (parent != nullptr) {
                if (*(int**)(parent + 6) != node) {
                    // Not left child -- go up
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
            int* bstNode = *(int**)root;
            while (bstNode != nullptr) {
                if (collectArray[k] < *bstNode) {
                    bstNode = *(int**)(bstNode + 6);
                } else if (collectArray[k] > *bstNode) {
                    bstNode = *(int**)(bstNode + 8);
                } else {
                    s_fn_BST_Remove(root, bstNode);
                    break;
                }
            }
        }
    }

    fn_MemFreeStatic(currentArray ? currentArray : collectArray);
    return collectCount == 0 ? 1 : 0;
}


// ============================================================
// HarvestArea::Slot10 -- GetWorkEntity
// ============================================================
// Returns entity pointer from workplace entity ref.
// Uses hidden return ptr ABI (RCX=self, RDX=outPtr).
void* HarvestArea::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    // CheckPtr on self->_field10 (work entity IBP)
    uintptr_t entity = *(uintptr_t*)(selfAddr + 0x10);

    // Find position component on entity
    uintptr_t posCompIBP[2] = {0};
    s_fn_FindPosComp(entity, posCompIBP);

    // CheckPtr and read +0x48 (workplace sub-entity)
    *(uintptr_t*)outPtr = *(uintptr_t*)(*(uintptr_t*)posCompIBP + 0x48);

    return outPtr;
}


// ============================================================
// HarvestArea::Serialize
// ============================================================
// Work::Serialize + version block + BST serialize + array + int
void HarvestArea::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create Serializer with version=0
    alignas(8) uint8_t serializerBuf[24];
    ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Immediately destroy (version block with no extra data)
    reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge

    // Create new Serializer (version=0) for our data
    alignas(8) uint8_t serializerBuf2[16];
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at +0x48
    s_fn_BST_Serialize((void*)(selfAddr + 0x48), serializerBuf2);

    // Serialize field cell count at +0x60
    int cellCount = *(int*)(selfAddr + 0x60);
    int cellCountBuf[2] = { cellCount, 0 };
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->Serialize(*reinterpret_cast<int*>(cellCountBuf), nullptr);   // B.3.4 bridge

    // If reading (serializer mode == 0), resize validity array.
    // B.3.2 bridge: FUN_14000a590 == Collection::Array<uint8>::SetSize.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializerBuf2)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        GameColl::AsArray<GameColl::U8>((void*)(selfAddr + 0x58)).SetSize(cellCountBuf[0]);
    }

    // Serialize validity array through the BOOL-ARRAY door.
    //
    // ENDGAME B.3.4 callers batch H — SETTLED FROM RETAIL, not from the alias.
    // Retail HarvestArea::Serialize (Application-x64.exe 0x1400ed350) ends with
    //     1400ed3dd  XOR R9D,R9D                    ; name = nullptr
    //     1400ed3e0  MOV R8,qword ptr [RDI + 0x58]  ; the buffer
    //     1400ed3e4  MOV EDX,dword ptr [RSP + 0x60] ; the count
    //     1400ed3e8  LEA RCX,[RSP + 0x28]           ; the Serializer
    //     1400ed3ed  CALL qword ptr [0x140151458]
    // 0x140151458 is `?Serialize@Serializer@IO@@QEAAXHPEA_NPEBD@Z`
    // (address_registry.cpp:1163), the four-parameter (int n, bool*, const char*)
    // array door at rw_vtbl::kArrayBool. It is NOT 0x140152ac8, which is what the
    // old `s_fn_Serializer_SerializeBoolArray` alias (removed with its door in the
    // B.3.4a closing sweep) pointed at — that is the THREE-parameter
    // `Serialize(int&)` scalar door, so the old call put the COUNT in the register
    // the scalar body dereferences as `int*`.
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->Serialize(
        cellCountBuf[0], reinterpret_cast<bool*>(*(void**)(selfAddr + 0x58)), nullptr);   // B.3.4 bridge  (slot 0x140151458 = bool-array door)

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// HarvestArea::ScalarDtor body
// ============================================================
// ~HarvestArea frees the validity array and the BST; then MemFree on &1.
// [0] 0x14002a4d0: ~HarvestArea, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* HarvestArea::DeletingDtor(unsigned flags) {
    this->~HarvestArea();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// The HarvestArea / AutoPickupWork / Work stamps and the `~Internal` call are the
// compiler's chain.
HarvestArea::~HarvestArea() {
    uintptr_t selfAddr = (uintptr_t)this;

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
}



// [11] 0x140029860 (shared by 13 tables): `+0x50 > 0` — "is anyone assigned". The
// old table pointed at Work_Slot11_HasActiveTasks, the same body.
int HarvestArea::GetPriority() {
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's. HarvestArea_ResolveFuncs
// wires the file's helper pointers (the old factory did the same).
HarvestArea::HarvestArea() : AutoPickupWork(), _field48(nullptr), _field50(0), _field58(nullptr), _field60(nullptr) {
    HarvestArea_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_HarvestArea_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over HarvestArea_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a460: MemAlloc(sizeof, 8), then the ctor.
void* HarvestArea::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HarvestArea), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HarvestArea());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* HarvestArea::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HarvestArea());
}

DEFINE_EXE_VTABLE(HarvestArea, "_ZTV11HarvestArea")

#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(HarvestArea, "AutoPickupWork")
