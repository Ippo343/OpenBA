// code/Game/Work/clearstorage.cpp
// Clean reimplementation of ClearStorage work class methods.
//
// ClearStorage is a Work subclass (0x68 bytes) for clearing items from storage
// buildings. Citizens assigned to this work pick up items from one storage and
// carry them to another available storage location.
//
// Vtable at 0x1401611a0.
//
// Decompiled from Application-x64.exe.

#include "Work/clearstorage_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Helper: Clamp int to [0, max-1]
// ============================================================
static inline int ClampGrid(int val, int gridSize) {
    if (val < 0) return 0;
    if (val >= gridSize) return gridSize - 1;
    return val;
}

// ============================================================
// Helper: Compute grid cell index from world position + grid info
// ============================================================
static inline uint32_t WorldToCell(float x, float z, int gridSize) {
    int halfGrid = gridSize >> 1;
    int cellX = ClampGrid((int)(x + (float)halfGrid), gridSize);
    int cellZ = ClampGrid((int)(z + (float)halfGrid), gridSize);
    return (uint32_t)(cellZ * gridSize + cellX);
}

// ============================================================
// ClearStorage::Execute (Slot4, vtable +0x20)
// Original: 0x1400e6c50
// ============================================================
// Iterates storage items on the assigned building. For each item type with
// available quantity, finds a destination storage, validates the path, and
// sets up citizen pathfinding to carry items.
//
// Returns: 0 = failed validation, 1 = no work available (retry), 2 = work assigned
int ClearStorage_Execute(ClearStorage* self, void* citizenAI, void* time) {
    // Work base has entity ref at +0x10 (offset 2 in qword terms)
    uintptr_t workBase = (uintptr_t)self;
    void* entityRef = (void*)(workBase + 0x10);  // InternalBasePtr to entity

    // Find components on the entity
    fn_InternalBasePtr_CheckPtr(entityRef);
    void* entity = *(void**)entityRef;

    uint64_t positionIBP = 0;
    fn_FindComponent_Position(entity, &positionIBP);

    uint64_t unknownCompIBP = 0;
    fn_InternalBasePtr_CheckPtr(entityRef);
    entity = *(void**)entityRef;
    fn_FindComponent_Unknown7ee8(entity, &unknownCompIBP);

    uint64_t storageIBP = 0;
    fn_InternalBasePtr_CheckPtr(entityRef);
    entity = *(void**)entityRef;
    fn_FindComponent_Storage(entity, &storageIBP);

    // Get game world grid data
    uintptr_t gameWorld = *(uintptr_t*)g_pGameWorldSingleton;
    int* gridDataPtr = *(int**)(gameWorld + 0xf8);
    void* worldArray = *(void**)(gameWorld + 0xf0);

    // Get citizen position from citizenAI -> entity -> +0x278 -> +0x68
    fn_InternalBasePtr_CheckPtr(citizenAI);
    uintptr_t citizenEntity = *(uintptr_t*)citizenAI;

    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t comp278 = *(uintptr_t*)(citizenEntity + 0x278);

    fn_InternalBasePtr_CheckPtr((void*)(comp278 + 0x68));
    uintptr_t posData = *(uintptr_t*)(comp278 + 0x68);

    int gridSize = *gridDataPtr;
    // citizenY at +0x160 is stored but only used for searchPos[2] init
    (void)*(float*)(posData + 0x160);  // citizenY — unused in grid calc
    float citizenX = *(float*)(posData + 0x158);
    float citizenZ = *(float*)(posData + 0x15c);

    uint32_t srcCell = WorldToCell(citizenX, citizenZ, gridSize);

    // Get storage item count
    fn_InternalBasePtr_CheckPtr(&storageIBP);
    uintptr_t storageComp = *(uintptr_t*)&storageIBP;
    int itemCount = *(int*)(storageComp + 0x30);

    if (itemCount <= 0) {
        return 1;  // No items in storage
    }

    // Iterate over storage items
    for (int i = 0; i < itemCount; i++) {
        fn_InternalBasePtr_CheckPtr(&storageIBP);
        storageComp = *(uintptr_t*)&storageIBP;
        uintptr_t itemArray = *(uintptr_t*)(storageComp + 0x28);
        uintptr_t itemEntry = itemArray + (uintptr_t)i * 0x10;

        // Check available count: stored - reserved > 0
        int stored = *(int*)(itemEntry + 0x8);
        int reserved = *(int*)(itemEntry + 0xc);
        int availCount = stored - reserved;
        if (availCount <= 0) continue;

        // Get the entity pointer from item entry
        uintptr_t itemEntity = *(uintptr_t*)itemEntry;

        // Find RawMaterialDescription component on item entity
        void* rawMatDesc = nullptr;
        if (*(int*)(itemEntity + 0x30) > 0) {
            for (int j = 0; j < *(int*)(itemEntity + 0x30); j++) {
                uintptr_t compArray = *(uintptr_t*)(itemEntity + 0x28);
                void* comp = *(void**)(compArray + (uintptr_t)j * 8);
                // Check type via vtable +0x10
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
                uint32_t typeId = getTypeId(comp);
                if (typeId == *g_TypeId_RawMaterialDescription) {
                    // Verify via IsDerivedFrom
                    comp = *(void**)(*(uintptr_t*)(itemEntity + 0x28) + (uintptr_t)j * 8);
                    if (fn_IsDerivedFrom(comp, *g_TypeId_RawMaterialDescription)) {
                        rawMatDesc = comp;
                    }
                    break;
                }
            }
        }

        // Find StorageComponent on citizen's assigned entity (citizenAI -> +0x10)
        fn_InternalBasePtr_CheckPtr(citizenAI);
        uintptr_t citizenEnt = *(uintptr_t*)citizenAI;
        fn_InternalBasePtr_CheckPtr((void*)(citizenEnt + 0x10));
        uintptr_t citizenBuilding = *(uintptr_t*)(citizenEnt + 0x10);

        void* destStorage = nullptr;
        if (*(int*)(citizenBuilding + 0x38) > 0) {
            for (int64_t k = 0; k < *(int*)(citizenBuilding + 0x38); k++) {
                void* compIBP = (void*)(*(uintptr_t*)(citizenBuilding + 0x30) + k * 8);
                fn_InternalBasePtr_CheckPtr(compIBP);
                void* comp = *(void**)compIBP;
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
                uint32_t typeId = getTypeId(comp);
                if (typeId == *g_TypeId_StorageComponent_FC) {
                    compIBP = (void*)(*(uintptr_t*)(citizenBuilding + 0x30) + k * 8);
                    fn_InternalBasePtr_CheckPtr(compIBP);
                    void* compVal = *(void**)compIBP;
                    if (fn_IsDerivedFrom(compVal, *g_TypeId_StorageComponent_FC)) {
                        destStorage = compVal;
                    }
                    goto found_dest_storage;
                }
            }
        }
        destStorage = nullptr;
found_dest_storage:

        // Create InternalBasePtr for destStorage
        uint64_t destStorageIBP = 0;
        fn_InternalBasePtr_Ctor(&destStorageIBP, destStorage);

        // Check capacity: destStorage capacity - count vs rawMat resourceSize * availCount
        fn_InternalBasePtr_CheckPtr(&destStorageIBP);
        uintptr_t dsc = *(uintptr_t*)&destStorageIBP;
        int maxCapacity = *(int*)(dsc + 0x38);

        fn_InternalBasePtr_CheckPtr(&destStorageIBP);
        dsc = *(uintptr_t*)&destStorageIBP;
        int currentCount = fn_StorageComponent_QueryCount((void*)dsc);

        int freeSpace = maxCapacity - currentCount;
        int resourceSize = rawMatDesc ? *(int*)((uintptr_t)rawMatDesc + 0x48) : 1;

        if (freeSpace < resourceSize * availCount) {
            // Not enough room in dest, check citizen's building storage
            fn_InternalBasePtr_CheckPtr(citizenAI);
            uintptr_t cEnt = *(uintptr_t*)citizenAI;
            fn_InternalBasePtr_CheckPtr((void*)(cEnt + 0x10));
            uintptr_t cBuilding = *(uintptr_t*)(cEnt + 0x10);

            uint64_t altStorageIBP = 0;
            fn_FindComponent_Storage((void*)cBuilding, &altStorageIBP);

            fn_InternalBasePtr_CheckPtr(&altStorageIBP);
            uintptr_t altSC = *(uintptr_t*)&altStorageIBP;
            int altMax = *(int*)(altSC + 0x38);

            fn_InternalBasePtr_CheckPtr(&altStorageIBP);
            altSC = *(uintptr_t*)&altStorageIBP;
            int altCurrent = fn_StorageComponent_QueryCount((void*)altSC);

            availCount = (altMax - altCurrent) / resourceSize;
        }

        if (availCount == 0) {
            // No capacity available, transition to long-term plan
            fn_InternalBasePtr_CheckPtr(citizenAI);
            fn_CitizenAI_LongTermPlan(*(void**)citizenAI);
            return 2;
        }

        // Set up search for destination storage
        uint64_t entityIBP = 0;
        uintptr_t wk = (uintptr_t)self;
        void* entityRefVal = *(void**)(wk + 0x10);
        // Copy entity IBP value
        *(void**)&entityIBP = entityRefVal;

        float searchPos[3] = {0.0f, 0.0f, 0.0f};
        uint32_t resourceTypeId = *(uint32_t*)(wk + 0x28);
        void* destEntity = nullptr;

        fn_FindStorageDestination(worldArray, &destEntity, resourceTypeId,
                                  (void*)itemEntity, availCount, 1,
                                  searchPos, &entityIBP);

        if (destEntity == nullptr) {
            // No destination found, continue to next item
            continue;
        }

        // Check if path is possible
        uintptr_t wkPtr = (uintptr_t)self;
        uint32_t workTypeId = *(uint32_t*)(wkPtr + 0x28);
        if (!fn_Map_CanPathTo(gridDataPtr, 0xe1f, srcCell, workTypeId)) {
            return 1;
        }

        // Determine destination position and cell
        float destPos[3] = {0.0f, 0.0f, 0.0f};
        uint32_t destCell = 0;

        if (unknownCompIBP == 0) {
            // No position component variant — use positionIBP
            if (positionIBP == 0) {
                return 1;
            }
            fn_InternalBasePtr_CheckPtr(&positionIBP);
            fn_Entity_GetPosition((void*)*(uintptr_t*)&positionIBP,
                                  &destPos, 0, 0, nullptr);
            destCell = WorldToCell(destPos[0], destPos[1], gridSize);
        } else {
            // Use search position
            destCell = WorldToCell(searchPos[0], searchPos[1], gridSize);

            fn_InternalBasePtr_CheckPtr(&unknownCompIBP);
            void* unkComp = *(void**)&unknownCompIBP;
            int slotIdx = fn_Storage_FindSlot(unkComp, destCell, (float*)&destPos, workTypeId);
            if (slotIdx < 0) {
                return 1;
            }
        }

        // Validate the work assignment
        fn_InternalBasePtr_CheckPtr(citizenAI);
        void* citizenVal = *(void**)citizenAI;
        uint64_t citizenRefCopy = (uint64_t)citizenVal;

        if (!fn_Work_ValidateAssignment(self, &citizenRefCopy, destCell, srcCell, time)) {
            return 0;
        }

        // Set up pathfinding
        uintptr_t wkBase = (uintptr_t)self;
        void* workEntity = *(void**)(wkBase + 0x10);
        int pathType = *(int*)(wkBase + 0x20);
        void* destEntityCopy = destEntity;
        uint32_t pathFlags = (availCount << 8) | 0;  // flags from local_res20[0]

        uint32_t pathResult = 0;
        fn_Pathfinding_FindPath(&pathResult, &citizenVal, pathType, &destEntityCopy,
                                searchPos, &workEntity, (float*)&destPos,
                                (long)itemEntity, availCount, pathFlags, 0, 1, 0, 0);

        // Get the Slot10 result (vtable +0x50)
        fn_InternalBasePtr_CheckPtr(citizenAI);
        uintptr_t citizenPtr = *(uintptr_t*)citizenAI;

        typedef void* (*fn_Slot10_t)(void* self, void* entityIBPOut);
        uint64_t entityRefOut = 0;
        fn_Slot10_t slot10 = *(fn_Slot10_t*)(*(uintptr_t*)self + 0x50);
        void* resultPtr = slot10(self, &entityRefOut);
        uintptr_t resultVal = *(uintptr_t*)resultPtr;

        // Update citizen state
        *(uint32_t*)(citizenPtr + 0x54) |= 4;
        *(int*)(citizenPtr + 0x30) = pathType;
        *(uint32_t*)(citizenPtr + 0xb0) = 0;

        if (resultVal == *(uintptr_t*)(citizenPtr + 0x1f0) && pathType >= 0) {
            *(uint32_t*)(citizenPtr + 0x88) |= 0x20000;
        }

        // Register work with citizen AI
        fn_CitizenAI_RegisterWork((void*)(wkBase + 0x58), &entityRefOut, citizenAI);
        return 2;
    }

    return 1;  // No valid items found
}

// ============================================================
// ClearStorage::Validate (Slot9, vtable +0x48)
// Original: 0x1400e69f0
// ============================================================
// Traverses the BST at self+0x58, checking each stored task via
// Work::CanStartWork. Invalid tasks are collected into a temporary
// array and then removed from the BST.
//
// Returns: true (1) if no entries remain (all cleared), false (0) otherwise.
//
// NOTE: Hook is DISABLED due to bug class 3 (MOV RAX,RSP in prologue).
// This clean implementation exists for documentation and future standalone exe.
int ClearStorage_Validate(ClearStorage* self, void* citizenAI) {
    uintptr_t base = (uintptr_t)self;
    void* bstRoot = (void*)(base + 0x58);  // BST root pointer

    // Phase 1: In-order traverse BST, collect entries that fail CanStartWork
    void** removeList = nullptr;
    int removeCount = 0;
    int removeCapacity = 0;

    // Find leftmost node (smallest key)
    uintptr_t node = *(uintptr_t*)bstRoot;
    if (node == 0) goto phase2;

    // Go to leftmost
    while (*(uintptr_t*)(node + 0x10) != 0) {
        node = *(uintptr_t*)(node + 0x10);
    }

    // In-order traversal
    while (node != 0) {
        // Get task key from node
        uint64_t taskKey = *(uint64_t*)node;
        uint64_t taskIBP = taskKey;

        // Check if task is still valid
        bool canStart = fn_Work_CanStartWork(self, citizenAI, &taskIBP, nullptr);

        if (!canStart) {
            // Need to add this node to remove list
            int newCount = removeCount + 1;
            if (newCount > removeCapacity) {
                // Grow capacity
                int newCap = newCount;
                if (removeCount != 0 && newCount == removeCount + 1) {
                    newCap = newCount * 3;
                }
                removeCapacity = newCap;
                void** newList = (void**)fn_MemAlloc(g_pMemAllocSingleton,
                    (uint64_t)newCap * 8, 8);

                // Copy existing entries
                int copyStart = 0;
                if (removeList == newList) {
                    copyStart = removeCount;
                }
                for (int j = copyStart; j < newCount; j++) {
                    if (newList != nullptr) {
                        void* src = (j < removeCount && removeList != nullptr)
                            ? removeList[j]
                            : (void*)node;  // Current node for the new entry
                        newList[j] = src;
                    }
                }

                if (removeList != newList && removeList != nullptr) {
                    fn_MemFreeStatic(removeList);
                }
                removeList = newList;
            } else {
                // Enough capacity, just store
                if (removeList != nullptr) {
                    removeList[removeCount] = (void*)node;
                }
            }
            removeCount = newCount;
        }

        // Move to next in-order node
        uintptr_t right = *(uintptr_t*)(node + 0x18);
        if (right != 0) {
            // Go to leftmost of right subtree
            node = right;
            while (*(uintptr_t*)(node + 0x10) != 0) {
                node = *(uintptr_t*)(node + 0x10);
            }
        } else {
            // Go up until we come from a left child
            uintptr_t parent = *(uintptr_t*)(node + 0x8);
            if (parent != 0 && *(uintptr_t*)(parent + 0x10) == node) {
                // We are left child of parent, parent is next
                node = parent;
            } else {
                // We are right child, go up while we are right child
                while (parent != 0 && *(uintptr_t*)(parent + 0x18) == node) {
                    node = parent;
                    parent = *(uintptr_t*)(node + 0x8);
                }
                node = parent;
            }
        }
    }

phase2:
    // Phase 2: Remove collected entries from BST
    if (removeCount > 0) {
        for (int i = 0; i < removeCount; i++) {
            // Find node in BST by key
            uintptr_t searchNode = *(uintptr_t*)bstRoot;
            uint64_t key = *(uint64_t*)removeList[i];

            while (searchNode != 0) {
                fn_InternalBasePtr_CheckPtr((void*)searchNode);
                uint64_t nodeKey = *(uint64_t*)searchNode;
                if (key < nodeKey) {
                    searchNode = *(uintptr_t*)(searchNode + 0x10);
                } else {
                    fn_InternalBasePtr_CheckPtr(removeList + i);
                    if (key <= nodeKey) {
                        // Found it, remove
                        fn_BST_Remove(bstRoot, (void*)searchNode);
                        break;
                    }
                    searchNode = *(uintptr_t*)(searchNode + 0x18);
                }
            }
        }
    }

    // Free remove list
    fn_MemFreeStatic(removeList);

    return removeCount == 0 ? 1 : 0;
}
