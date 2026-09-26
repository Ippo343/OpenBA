// code/Game/Work/firework.cpp
// Clean reimplementation of FireWork::Slot4 and FireWork::Slot9.
//
// FireWork (0x60 bytes) -- fire suppression work action.
// Extends Work (0x48 base) which extends Object::Internal.
// Vtable: 0x1401612a0
//
// Fields:
//   +0x48: int   _field48   -- max fire workers capacity
//   +0x50: void* _field50   -- BST root (tracks entity -> citizen assignments)
//   +0x58: int   _field58   -- number of currently assigned fire workers
//
// Slot4 (0x1400e9620): Fire suppression work assignment.
//   Finds a burning building, checks pathfinding, sets up citizen action queue.
//   Returns: 0 = no fire found, 1 = at capacity, 2 = work assigned.
//
// Slot9 (0x1400e93c0): BST validation pass.
//   Traverses BST at +0x50, calls validation on each entity. Removes
//   entries where the fire has been extinguished.
//   Returns: 1 if BST is empty after cleanup, 0 otherwise.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/FireWork.h"
#include "Work/firework_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <new>

// ============================================================
// Local sub-function pointers (resolved at runtime)
// ============================================================

// FireWork_helper_81d0 (0x1400881d0): Searches for a burning entity
// in a spiral pattern around the building. Returns true if found,
// fills outSrcPos and outDestPos with fire/citizen positions.
typedef bool (*fn_FireHelper_FindFire_t)(
    uintptr_t mapData, uint32_t centerCell, int searchRadius,
    float* outSrcPos, float* outDestPos);
static fn_FireHelper_FindFire_t s_fn_FindFire = nullptr;

// FUN_140122be0: Pathfinding accessibility check for entity positions.
// Checks if citizen can reach fire location. Writes best position to outPos
// and distance to outDist.
typedef bool (*fn_PathCheck_t)(
    uintptr_t entity, uintptr_t posArray, float* srcPos,
    float* outPos, int* outDist);
static fn_PathCheck_t s_fn_PathCheck = nullptr;

// FUN_140122b00: Recursive path check across entity children.
// Calls s_fn_PathCheck on each child entity.
typedef bool (*fn_PathCheckRecursive_t)(
    uintptr_t entity, int index, float* srcPos,
    float* outPos, int* outDist);
static fn_PathCheckRecursive_t s_fn_PathCheckRecursive = nullptr;

// BuildStructure_impl_6ef0 (0x1400f6ef0): Validates work assignment
// against citizen/entity, checks distance and timing.
typedef bool (*fn_ValidateAssignment_t)(
    uintptr_t work, void* citizenIBP, uint32_t destCell,
    uint32_t srcCell, void* time);
static fn_ValidateAssignment_t s_fn_ValidateAssignment = nullptr;

// PerformWork_Action_CopyCtor (0x1400ba9f0): Sets up the full action
// queue for fire suppression (MoveTo, ShowTool, PerformWork, CompleteTask).
typedef void (*fn_SetupFireActions_t)(
    void* citizenIBP, int priority, float* srcPos,
    float* destPos, float* firePos, void* workEntityIBP);
static fn_SetupFireActions_t s_fn_SetupFireActions = nullptr;

// CitizenAI_impl_1a30 (0x140021a30): Registers work assignment
// with the citizen AI's BST (inserts entity into work tracking).
typedef void (*fn_RegisterWork_t)(
    void* bstRoot, void* entityIBP, void* citizenIBP);
static fn_RegisterWork_t s_fn_RegisterWork = nullptr;

// FUN_1400f7100: Validates a citizen entity still has active fire work.
// Checks if the entity's task list contains a CompleteTask matching
// the work's priority ID at +0x20.
typedef bool (*fn_ValidateFireEntity_t)(
    uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
static fn_ValidateFireEntity_t s_fn_ValidateEntity = nullptr;

// CitizenAI_impl_a6e0 (0x14001a6e0): Removes a node from the intrusive
// BST (AVL/red-black tree). Handles rebalancing.
typedef void (*fn_BSTRemove_t)(void* bstRoot, void* node);
static fn_BSTRemove_t s_fn_BSTRemove = nullptr;

// Type IDs for component lookups
static uint32_t* s_TypeId_PositionComponent = nullptr; // DAT_1401c7938
static uint32_t* s_TypeId_CitizenComponent  = nullptr; // DWORD_1401c7190

// ============================================================
// FireWork_ResolveFuncs
// ============================================================
bool FireWork_ResolveFuncs() {
    s_fn_FindFire = reinterpret_cast<fn_FireHelper_FindFire_t>(
        ResolveAddr(Module::Exe, 0x1400881d0));

    s_fn_PathCheck = reinterpret_cast<fn_PathCheck_t>(
        ResolveAddr(Module::Exe, 0x140122be0));

    s_fn_PathCheckRecursive = reinterpret_cast<fn_PathCheckRecursive_t>(
        ResolveAddr(Module::Exe, 0x140122b00));

    s_fn_ValidateAssignment = reinterpret_cast<fn_ValidateAssignment_t>(
        ResolveAddr(Module::Exe, 0x1400f6ef0));

    s_fn_SetupFireActions = reinterpret_cast<fn_SetupFireActions_t>(
        ResolveAddr(Module::Exe, 0x1400ba9f0));

    s_fn_RegisterWork = reinterpret_cast<fn_RegisterWork_t>(
        ResolveAddr(Module::Exe, 0x140021a30));

    s_fn_ValidateEntity = reinterpret_cast<fn_ValidateFireEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));

    s_fn_BSTRemove = reinterpret_cast<fn_BSTRemove_t>(
        ResolveAddr(Module::Exe, 0x14001a6e0));

    // Type ID pointers (addresses of static type IDs in .rdata)
    s_TypeId_PositionComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7938));
    s_TypeId_CitizenComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7190));

    bool ok = s_fn_FindFire && s_fn_PathCheck && s_fn_PathCheckRecursive &&
              s_fn_ValidateAssignment && s_fn_SetupFireActions &&
              s_fn_RegisterWork && s_fn_ValidateEntity && s_fn_BSTRemove &&
              s_TypeId_PositionComponent && s_TypeId_CitizenComponent;

#ifdef _DEBUG
    printf("[FireWork] ResolveFuncs: FindFire=%p PathCheck=%p ValidateAssign=%p "
           "SetupActions=%p RegisterWork=%p ValidateEntity=%p BSTRemove=%p ok=%d\n",
           (void*)s_fn_FindFire, (void*)s_fn_PathCheck,
           (void*)s_fn_ValidateAssignment, (void*)s_fn_SetupFireActions,
           (void*)s_fn_RegisterWork, (void*)s_fn_ValidateEntity,
           (void*)s_fn_BSTRemove, ok);
#endif
    return ok;
}

// ============================================================
// Helper: Convert world position to clamped grid cell index.
// ============================================================
static uint32_t WorldToCell(int gridSize, float x, float z) {
    int half = gridSize >> 1;
    int cx = (int)(x + (float)half);
    int cz = (int)(z + (float)half);
    if (cx < 0) cx = 0;
    if (cx >= gridSize) cx = gridSize - 1;
    if (cz < 0) cz = 0;
    if (cz >= gridSize) cz = gridSize - 1;
    return (uint32_t)(gridSize * cz + cx);
}

// ============================================================
// Slot4 (0x1400e9620): Fire suppression work assignment.
//
// Algorithm:
// 1. Check if at capacity (_field48 >= _field58)
// 2. Get citizen position, compute grid cell
// 3. Call FireWork_helper_81d0 to find a burning entity in spiral
// 4. Check pathfinding accessibility (FUN_140122be0/FUN_140122b00)
// 5. Validate assignment (BuildStructure_impl_6ef0)
// 6. Set up action queue (PerformWork_Action_CopyCtor)
// 7. Register work with citizen AI (CitizenAI_impl_1a30)
// Returns: 0 = no fire, 1 = at capacity, 2 = work assigned.
// ============================================================
int FireWork::Execute(void* citizenIBP, void* time, uint64_t /*arg3*/) {
    uintptr_t s = (uintptr_t)this;

    // CheckPtr on the entity IBP at +0x10

    // Find PositionComponent on building entity
    uintptr_t posCompIBP = 0;
    fn_Entity_FindComponent_Position(*(uintptr_t*)(s + 0x10), &posCompIBP);

    // Check capacity: _field48 (max capacity) vs _field58 (assigned count)
    // Original: if ((int)param_1[9] <= (int)param_1[0xb]) return 1;
    // i.e. if max_capacity <= assigned_count, we're at capacity.
    int32_t maxCap   = *(int32_t*)(s + 0x48);
    int32_t assigned = *(int32_t*)(s + 0x58);
    if (maxCap <= assigned) {
        return 1;
    }

    // Get map data
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapData = *(uintptr_t*)(gameWorld + 0xf0);
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    // Get citizen entity -> StateMachineComponent -> PositionComponent -> world pos
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    uintptr_t smComp = *(uintptr_t*)(citizenEntity + 0x278);

    uintptr_t posData = *(uintptr_t*)(smComp + 0x68);

    // Original reads posData+0x160 (citizenY) but only uses it for dead-store
    // to local_60[2] which is zeroed before FindFire — skip it.
    int gridSize = *mapGrid;
    uint32_t centerCell = WorldToCell(gridSize,
        *(float*)(posData + 0x158), *(float*)(posData + 0x15c));

    // Check if citizen is a CitizenComponent (type check)
    void* citizenObj = *(void**)(*(uintptr_t*)citizenIBP + 0x18);
    bool isCitizen = fn_IsDerivedFrom(citizenObj, *s_TypeId_CitizenComponent);
    void* citizenComp = isCitizen ? citizenObj : nullptr;

    // Initialize position buffers (all zeroed — original zeros local_60 before FindFire)
    float srcPos[3] = {0.0f, 0.0f, 0.0f};
    float destPos[3] = {0.0f, 0.0f, 0.0f};

    // Search for a burning entity (spiral search from citizen position)
    int searchRadius = *(int*)((uintptr_t)citizenComp + 0xf8);
    bool found = s_fn_FindFire(mapData, centerCell, searchRadius, srcPos, destPos);

    // Outcome of the TownManager water search (0x1400881d0). This is the ONLY place
    // that walks townMgr+0x140, so it is also the only place that proves the coarse
    // cell index was sized at world-init (gameupdateloop_load.cpp Step 31). Before that
    // resize landed, this call faulted at coarseCell*0x10 instead of returning.
    // found=0 is NOT a failure by itself: with no water source registered the buckets
    // are legitimately empty and retail returns 0 too.
    // A first-N budget cannot measure a soak: it burns out inside the first fire and then
    // reports nothing for the next hour. Print the first few calls in full, then a running
    // CENSUS of both outcomes -- a run that starts finding water and later stops is
    // invisible to a budget and obvious in a census.
    {
        static int s_fwCalls = 0, s_fwFound = 0, s_fwMissed = 0;
        ++s_fwCalls;
        if (found) ++s_fwFound; else ++s_fwMissed;
        if (s_fwCalls <= 4 || (s_fwCalls % 250) == 0) {
            fprintf(stderr, "[FireWork] water search #%d: townMgr=%p buckets=%p "
                    "centerCell=%u radius=%d -> found=%d  (census: found=%d missed=%d)\n",
                    s_fwCalls, (void*)mapData, *(void**)(mapData + 0x140), centerCell,
                    searchRadius, (int)found, s_fwFound, s_fwMissed);
            fflush(stderr);
        }
    }

    if (!found) {
        return 0;
    }

    // Check pathfinding accessibility
    void* posComp = *(void**)(posCompIBP + 0x18);
    bool isPositionComp = fn_IsDerivedFrom(posComp, *s_TypeId_PositionComponent);
    void* posCompTyped = isPositionComp ? posComp : nullptr;

    int bestDist = 0x7fffffff;
    float bestPos[3] = {0.0f, 0.0f, 0.0f};

    // Check main entity path
    bool pathFound = s_fn_PathCheck(
        posCompIBP, (uintptr_t)posCompTyped + 0x28,
        srcPos, bestPos, &bestDist);

    // Check child entities
    uintptr_t posCompVal = *(uintptr_t*)&posCompIBP;
    if (*(int32_t*)(posCompVal + 0x30) > 0) {
        uintptr_t childOffset = 0;
        for (int i = 0; i < *(int32_t*)(posCompVal + 0x30); i++) {
            uintptr_t* childIBP = (uintptr_t*)(*(uintptr_t*)(posCompVal + 0x28) + childOffset);
            bool childResult = s_fn_PathCheckRecursive(
                *childIBP, 0, srcPos, bestPos, &bestDist);
            pathFound = pathFound || childResult;
            citizenIBP = citizenIBP; // preserve param register (Ghidra artifact)
            childOffset += 8;
        }
    }

    if (!pathFound) {
        return 0;
    }

    // Convert best position to grid cell
    gridSize = *mapGrid;
    int cx = (int)(bestPos[0] + (float)(gridSize >> 1));
    int cz = (int)(bestPos[1] + (float)(gridSize >> 1));
    if (cx < 0) cx = 0;
    if (cx >= gridSize) cx = gridSize - 1;
    if (cz < 0) cz = 0;
    if (cz >= gridSize) cz = gridSize - 1;
    uint32_t destCell = (uint32_t)(gridSize * cz + cx);

    // Validate work assignment (checks timing, distance, etc.)
    uintptr_t citizenIBPVal = *(uintptr_t*)citizenIBP;
    bool valid = s_fn_ValidateAssignment(
        s, (void*)&citizenIBPVal, destCell, centerCell, time);

    if (!valid) {
        return 0;
    }

    // Set up fire suppression action queue
    // Original: local_res8 = param_1[2]; local_88 = *(int**)param_2;
    // local_88 is REASSIGNED from mapGrid to citizen entity ptr here.
    uintptr_t workEntityIBP = *(uintptr_t*)(s + 0x10);
    int priority = *(int32_t*)(s + 0x20);
    uintptr_t citizenPtr = *(uintptr_t*)citizenIBP;
    s_fn_SetupFireActions(
        (void*)&citizenPtr,
        priority,
        srcPos, destPos, bestPos, (void*)&workEntityIBP);

    // Update citizen entity state
    uintptr_t citizenVal = *(uintptr_t*)citizenIBP;

    // Get target entity via vtable slot 10 (+0x50)
    typedef void* (*fn_GetTarget_t)(void*, void*);
    void** vtbl = *(void***)this;
    uintptr_t targetIBP = 0;
    void* targetResult = ((fn_GetTarget_t)vtbl[10])(this, &targetIBP);
    uintptr_t targetVal = *(uintptr_t*)targetResult;

    // Set citizen flags
    *(uint32_t*)(citizenVal + 0x54) |= 4;
    *(int32_t*)(citizenVal + 0x30) = priority;
    *(int32_t*)(citizenVal + 0xb0) = 0;

    if (targetVal == *(uintptr_t*)(citizenVal + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizenVal + 0x88) |= 0x20000;
    }

    // Register with citizen AI BST at +0x50
    s_fn_RegisterWork((void*)(s + 0x50), &targetIBP, citizenIBP);

    return 2;
}

// ============================================================
// Slot9 (0x1400e93c0): BST validation pass.
//
// Algorithm:
// 1. In-order traverse BST at +0x50
// 2. For each node, call FUN_1400f7100 to check if entity still
//    needs fire suppression
// 3. Collect invalid entities in a dynamic array
// 4. Remove all invalid entries from BST using CitizenAI_impl_a6e0
// 5. Free the dynamic array
// Returns: 1 if BST is now empty, 0 otherwise.
// ============================================================
int FireWork::Validate(void* citizenIBP) {
    uintptr_t s = (uintptr_t)this;

    // BST root is at +0x50
    void** bstRoot = (void**)(s + 0x50);
    void* root = *bstRoot;

    // Dynamic array of entities to remove
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        // Empty tree -- nothing to validate
        return 1;
    }

    // In-order traversal of the BST
    // BST node layout:
    //   +0x00: key (uintptr_t, entity ID)
    //   +0x08: parent
    //   +0x10: left child
    //   +0x18: right child
    //   +0x20: balance factor (short)
    //   +0x22: height (short)

    // Find leftmost node (minimum)
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // Traverse in-order
    while (node != nullptr) {
        // Read entity IBP from node key
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Check if this entity still needs fire work
        bool stillValid = s_fn_ValidateEntity(s, citizenIBP, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Need to add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    uintptr_t* newArr = (uintptr_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);

                    // Copy existing entries
                    uint32_t copyStart = 0;
                    if (removeArr == newArr) {
                        copyStart = removeCount;
                    }
                    for (uint32_t i = copyStart; i < newCount; i++) {
                        if (newArr != nullptr) {
                            uintptr_t src = 0;
                            if (i < removeCount && removeArr != nullptr) {
                                src = removeArr[i];
                            } else {
                                src = entityIBP;
                            }
                            newArr[i] = src;
                        }
                    }

                    // Free old array if different
                    if (removeArr != newArr) {
                        fn_MemFreeStatic(removeArr);
                    }
                    removeArr = newArr;
                } else {
                    // Capacity sufficient, just append
                    removeArr[removeCount] = entityIBP;
                }
                removeCount = newCount;
            }
        }

        // Move to in-order successor
        void* right = *(void**)((uintptr_t)node + 0x18);
        if (right != nullptr) {
            // Go right, then all the way left
            node = right;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            // Go up until we come from a left child
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
                // We came from the right -- keep going up
                while (parent != nullptr &&
                       *(void**)((uintptr_t)parent + 0x18) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)node + 0x08);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove collected invalid entries from BST
    if ((int)removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            // Find the BST node matching this entity key
            void* bstNode = *bstRoot;
            while (bstNode != nullptr) {
                uintptr_t nodeKey = *(uintptr_t*)bstNode;
                if (removeArr[i] < nodeKey) {
                    bstNode = *(void**)((uintptr_t)bstNode + 0x10); // left
                } else {
                    if (removeArr[i] <= nodeKey) {
                        // Found -- remove it
                        s_fn_BSTRemove(bstRoot, bstNode);
                        break;
                    }
                    bstNode = *(void**)((uintptr_t)bstNode + 0x18); // right
                }
            }
        }
    }

    // Free the removal array
    fn_MemFreeStatic(removeArr);

    // Return 1 if BST is now empty, 0 otherwise
    return (removeCount == 0) ? 1 : 0;
}

// ============================================================
// Slot6 (0x1400e9600): OnComplete — remove citizen from BST,
// return false (0).
//
// Calls CitizenAI_impl_5520 on the BST at +0x50 to remove
// the citizen entry identified by param2, then returns 0.
// ============================================================

// CitizenAI_impl_5520 (0x140025520): BST lookup + remove by key.
// Searches BST for a node matching param2's IBP key, removes it.
typedef void (*fn_BST_RemoveByKey_t)(void* bstRoot, void* ibpKey);
static fn_BST_RemoveByKey_t s_fn_BST_RemoveByKey = nullptr;

static bool EnsureSlot6Funcs() {
    if (!s_fn_BST_RemoveByKey) {
        s_fn_BST_RemoveByKey = reinterpret_cast<fn_BST_RemoveByKey_t>(
            ResolveAddr(Module::Exe, 0x140025520));
    }
    return s_fn_BST_RemoveByKey != nullptr;
}

int FireWork::OnComplete(void* citizenIBP, void* /*arg2*/, int /*arg3*/) {
    if (!EnsureSlot6Funcs()) return 0;
    uintptr_t s = (uintptr_t)this;
    // Remove citizen entry from BST at +0x50
    s_fn_BST_RemoveByKey((void*)(s + 0x50), citizenIBP);
    return 0;
}

// ============================================================
// Slot7 (0x1400e95f0): OnCancel — thunk to BST remove by key.
//
// This is a 6-byte thunk: ADD RCX,0x50; JMP CitizenAI_impl_5520
// Adjusts this pointer to point to BST root at +0x50, then
// jumps to the BST remove-by-key function. The effect is
// identical to Slot6 without the SUB RSP / return false wrapper.
// ============================================================
int FireWork::OnCancel(void* citizenIBP, void* /*arg2*/, int /*arg3*/) {
    if (!EnsureSlot6Funcs()) return 0;
    uintptr_t s = (uintptr_t)this;
    // Remove citizen entry from BST at +0x50
    s_fn_BST_RemoveByKey((void*)(s + 0x50), citizenIBP);
    return 0;
}

// ============================================================
// Slot8 (0x1400e99a0): CancelAll / Cleanup.
//
// Walks the BST at +0x50 in a loop. For each iteration:
//   1. Get leftmost node in the BST
//   2. CheckPtr on the node, read entity from node
//   3. Get StateMachineComponent at entity+0x278
//   4. Call FUN_1400c5400 (remove pending action) on SMC+0x48
// Continues until the BST root is null.
// ============================================================

// FUN_1400c5400: remove pending action from state machine
typedef void (*fn_RemovePendingAction_t)(void* smActionList);
static fn_RemovePendingAction_t s_fn_RemovePendingAction_FW = nullptr;

static bool EnsureSlot8Funcs() {
    if (!s_fn_RemovePendingAction_FW) {
        s_fn_RemovePendingAction_FW = reinterpret_cast<fn_RemovePendingAction_t>(
            ResolveAddr(Module::Exe, 0x1400c5400));
    }
    return s_fn_RemovePendingAction_FW != nullptr;
}

int FireWork::Cleanup() {
    if (!EnsureSlot8Funcs()) return 0;
    uintptr_t s = (uintptr_t)this;
    void** bstRoot = (void**)(s + 0x50);

    while (true) {
        void* root = *bstRoot;
        if (root == nullptr) return 0;

        // Find leftmost node in BST
        void* node = root;
        void* leftChild = *(void**)((uintptr_t)node + 0x10);
        while (leftChild != nullptr) {
            node = leftChild;
            leftChild = *(void**)((uintptr_t)node + 0x10);
        }

        if (node == nullptr) return 0;

        // Double-traverse to find leftmost again (matches decompilation pattern)
        void* node2 = root;
        while (node2 != nullptr) {
            void* left2 = *(void**)((uintptr_t)node2 + 0x10);
            if (left2 == nullptr) break;
            node2 = left2;
        }

        // CheckPtr on the node, read entity
        uintptr_t entityPtr = *(uintptr_t*)node2;

        // Get StateMachineComponent at entity+0x278
        uintptr_t smComp = *(uintptr_t*)(entityPtr + 0x278);

        // Remove pending action from SMC+0x48
        s_fn_RemovePendingAction_FW((void*)(smComp + 0x48));
    }
}

// ============================================================
// Slot10 (0x1400e9a30): GetTarget.
//
// Returns the map entity from gameWorld singleton:
//   gameWorld->field_b0->field_30
// Uses hidden return pointer ABI (member function returning
// non-trivial InternalBasePtr): RCX=this, RDX=retbuf.
// ============================================================
void* FireWork::GetTarget(void* retbuf) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapHolder = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)retbuf = *(uintptr_t*)(mapHolder + 0x30);
    return retbuf;
}

// ============================================================
// Slot map (the old hand-built table's notes, 2026-08-22): slot 3 Serialize
// (0x1400e9a50), slot 11 (0x14002a220, `+0x58 > 0`) both ported; slot 5 is Work's
// `return 0`; the real producer is the 3-arg ctor 0x1400e9bd0 (firecomponent_ignite.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

// ============================================================
// Slot3 Serialize (vtable +0x18, 0x1400e9a50) — was Work_SerializeNop.
// ============================================================
//   Work::Serialize(this, stream)                       0x1400f7400
//   Serializer ser(stream, 0)
//   CitizenAI_impl_96b0(this + 0x50, ser)               0x1400296b0  (the BST)
//   Serializer::Serialize(ser, (int*)(this + 0x48))                  (the capacity)
//   ~Serializer(ser)
// Both sub-calls are already ported and registered (reg_work_base.cpp:49,
// reg_work_bst.cpp:19), so this is a plain transcription. ENDGAME B.3.8: the two
// function-local lazy pointer caches are gone — the base call is the qualified
// Work::Persist, the BST body is called by name (B38 D7).
extern void WorkBST_BST_Serialize_296b0(uintptr_t* root, void* serializer);
void FireWork::Persist(void* stream) {
    if (!stream) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(s + 0x50), ser);   // 0x1400296b0 (work_base_bst.cpp)
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x48), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Slot11 (vtable +0x58, 0x14002a220) — was Work_SlotRet0 (pinned to 0).
// ============================================================
// Retail body, in full: `XOR EAX,EAX ; CMP [RCX+0x58],EAX ; SETG AL ; RET`
// i.e. `return *(int*)(this + 0x58) > 0` — "does this fire currently have any
// firefighters assigned". +0x58 is the assigned count Slot4 compares against the
// +0x48 capacity, NOT the +0x50 field the five FishingWork-family siblings read,
// which is why work_base_standalone.h's Work_Slot11_HasActiveTasks helper is
// explicitly documented as unusable here.
int FireWork::GetPriority() {
    return *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(this) + 0x58) > 0 ? 1 : 0;
}

// ============================================================
// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The stamps are the compiler's. FireWork_ResolveFuncs wires
// the file's helper pointers (the old factory did the same).
// ============================================================
FireWork::FireWork() : Work(), _field48(nullptr), _field50(nullptr), _field58(0) {
    FireWork_ResolveFuncs();
}

// [0] 0x14002a230: ~FireWork, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* FireWork::DeletingDtor(unsigned flags) {
    this->~FireWork();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_FireWork_Vtable` (slot 0 Work_GenericScalarDtor,
// slot 1 null, slot 2 a lazily cached id) and REGISTER_TYPE over FireWork_Factory /
// _CreateInPlace, both Work_GenericCtor.

// create 0x14002a1e0: MemAlloc(sizeof, 8), then the ctor.
void* FireWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(FireWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) FireWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* FireWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) FireWork());
}

DEFINE_EXE_VTABLE(FireWork, "_ZTV8FireWork")

#include "Object/factory_registration.h"
#include "address_registry.h"

// B38 D5: 0x1401612a0 is still read back BY ADDRESS — firecomponent_ignite.cpp (block
// CIT) builds the 3-argument FireWork (0x1400e9bd0) by hand and stamps it — so the
// exe address serves the COMPILER's table.
extern "C" void RegisterFireWorkVtable() {
    RegisterExeAddr(FireWorkAddr::Vtable, reinterpret_cast<uintptr_t>(FireWork::Vtable()));   // 0x1401612a0
}
DEFINE_EXE_TYPE(FireWork, "Work")
