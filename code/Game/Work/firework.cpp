// code/Game/Work/firework.cpp
// Clean reimplementation of FireWork::Slot4 and FireWork::Slot9.
//
// FireWork (0x60 bytes) -- fire suppression work action.
// Extends Work (0x48 base) which extends Object::Internal.
// Vtable: 0x1401612a0
//
// Fields:
//   +0x48: int   _field48   -- number of currently assigned fire workers
//   +0x50: void* _field50   -- BST root (tracks entity -> citizen assignments)
//   +0x58: int   _field58   -- max fire workers capacity
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
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Work/firework_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>

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
int FireWork_Slot4_Impl(FireWork* self, void* citizenIBP, void* time) {
    uintptr_t s = (uintptr_t)self;

    // CheckPtr on the entity IBP at +0x10
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));

    // Find PositionComponent on building entity
    uintptr_t posCompIBP = 0;
    fn_Entity_FindComponent_Position(*(uintptr_t*)(s + 0x10), &posCompIBP);

    // Check capacity: _field48 (assigned count) vs _field58 (max capacity)
    int32_t assigned = *(int32_t*)(s + 0x48);
    int32_t maxCap   = *(int32_t*)(s + 0x58);
    if (assigned >= maxCap) {
        return 1;
    }

    // Get map data
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapData = *(uintptr_t*)(gameWorld + 0xf0);
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    // Get citizen entity -> StateMachineComponent -> PositionComponent -> world pos
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t smComp = *(uintptr_t*)(citizenEntity + 0x278);

    fn_InternalBasePtr_CheckPtr((void*)(smComp + 0x68));
    uintptr_t posData = *(uintptr_t*)(smComp + 0x68);

    float citizenY = *(float*)(posData + 0x160);
    int gridSize = *mapGrid;
    uint32_t centerCell = WorldToCell(gridSize,
        *(float*)(posData + 0x158), *(float*)(posData + 0x15c));

    // Check if citizen is a CitizenComponent (type check)
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    void* citizenObj = *(void**)(*(uintptr_t*)citizenIBP + 0x18);
    bool isCitizen = fn_IsDerivedFrom(citizenObj, *s_TypeId_CitizenComponent);
    void* citizenComp = isCitizen ? citizenObj : nullptr;

    // Initialize position buffers
    float srcPos[3] = {0.0f, 0.0f, 0.0f};
    // citizenY is used to initialize destPos[2] below
    float destPos[3] = {0.0f, 0.0f, citizenY};

    // Search for a burning entity (spiral search from citizen position)
    int searchRadius = *(int*)((uintptr_t)citizenComp + 0xf8);
    bool found = s_fn_FindFire(mapData, centerCell, searchRadius, srcPos, destPos);

    if (!found) {
        return 0;
    }

    // Check pathfinding accessibility
    fn_InternalBasePtr_CheckPtr(&posCompIBP);
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
            fn_InternalBasePtr_CheckPtr(childIBP);
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
    uintptr_t workEntityIBP = *(uintptr_t*)(s + 0x10);
    int priority = *(int32_t*)(s + 0x20);
    s_fn_SetupFireActions(
        (void*)mapGrid,  // param rewritten by Ghidra, actually citizenIBP
        priority,
        srcPos, destPos, bestPos, (void*)&workEntityIBP);

    // Update citizen entity state
    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t citizenVal = *(uintptr_t*)citizenIBP;

    // Get target entity via vtable slot 10 (+0x50)
    typedef void* (*fn_GetTarget_t)(void*, void*);
    void** vtbl = *(void***)self;
    uintptr_t targetIBP = 0;
    void* targetResult = ((fn_GetTarget_t)vtbl[10])(self, &targetIBP);
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
int FireWork_Slot9_Impl(FireWork* self, void* citizenIBP) {
    uintptr_t s = (uintptr_t)self;

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
                fn_InternalBasePtr_CheckPtr(bstNode);
                uintptr_t nodeKey = *(uintptr_t*)bstNode;
                if (removeArr[i] < nodeKey) {
                    bstNode = *(void**)((uintptr_t)bstNode + 0x10); // left
                } else {
                    fn_InternalBasePtr_CheckPtr(&removeArr[i]);
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
