// code/Game/Component/citizencomponent.cpp
// Clean reimplementation of CitizenComponent helper functions.
//
// CitizenComponent_ShouldSwitchWorkplace (original: 0x1401109E0)
//   Called from CitizenComponent::Update. Determines if a citizen should
//   switch to a different workplace based on job availability and region
//   priority.
//
// Two branches:
//   1. Assigned path (workplaceEntity+0x4C != 0): citizen has specific workplace,
//      checks if another workplace in a better region has vacancy.
//   2. Unassigned path (workplaceEntity+0x4C == 0): citizen is unassigned,
//      checks if worker distribution is unbalanced (maxGap - minGap > 1).

#include "CitizenComponent.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// BST node layout for the profession registry tree
// (gameWorld+0xF0 -> +0x100 = tree root)
// ============================================================
// node[0] = key (uint64_t, matches citizen->data1F0)
// node[1] = array_ptr (pointer to array of entity IBPs)
// node[2] = count (int, number of jobs)
// node[3] = padding
// node[4] = left child
// node[5] = right child

// ============================================================
// BST node layout for the region map tree
// (gameWorld+0x118 -> +0x30 = tree root)
// ============================================================
// node[0] = key (int32_t, region ID)
// node[1] = value (pointer to region descriptor)
// node[2] = padding
// node[3] = left child (at int offset +6 = byte offset +0x18)
// node[4] = right child (at int offset +8 = byte offset +0x20)

// ============================================================
// BST lookup: profession registry (uint64 key)
// Returns count of jobs, or 0 if not found.
// ============================================================
static int LookupProfessionJobCount(uintptr_t registryBase, uint64_t key) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4]; // left
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5]; // right
        } else {
            // Found
            return (int)node[2];
        }
    }
    return 0; // not found
}

// Get job array entry from profession registry
static uintptr_t GetJobEntity(uintptr_t registryBase, uint64_t key, int64_t index) {
    uintptr_t* node = *(uintptr_t**)(registryBase + 0x100);
    while (node) {
        uint64_t nodeKey = node[0];
        if (key < nodeKey) {
            node = (uintptr_t*)node[4];
        } else if (key > nodeKey) {
            node = (uintptr_t*)node[5];
        } else {
            // Found — read array[index]
            uintptr_t* arr = (uintptr_t*)node[1];
            return arr[index];
        }
    }
    return 0;
}

// ============================================================
// BST lookup: region map (int32 key)
// Returns pointer to region descriptor, or 0 if not found.
// Tree nodes: int key at +0, value ptr at +8, left at +0x18, right at +0x20
// ============================================================
static uintptr_t LookupRegionByID(uintptr_t areaMap, int32_t regionId) {
    int* node = *(int**)(areaMap + 0x30);
    while (node) {
        int nodeKey = node[0];
        if (regionId < nodeKey) {
            node = *(int**)(node + 6); // left (+0x18)
        } else if (regionId > nodeKey) {
            node = *(int**)(node + 8); // right (+0x20)
        } else {
            // Found — value at node+8 (qword)
            return *(uintptr_t*)(node + 2);
        }
    }
    return 0; // not found
}

// ============================================================
// CitizenComponent_ShouldSwitchWorkplace
// Original: 0x1401109E0 (CitizenComponent_helper_09e0)
// ============================================================
bool CitizenComponent_ShouldSwitchWorkplace(CitizenComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gwAddr = (uintptr_t)gameWorld;

    // Read citizen's workplace reference (profession group key)
    uint64_t profKey = *(uint64_t*)(selfAddr + 0x1F0);

    // Lookup job count from profession registry
    uintptr_t registry = *(uintptr_t*)(gwAddr + 0xF0);
    int jobCount = LookupProfessionJobCount(registry, profKey);

    // Read workplace entity flag at +0x4C
    uint8_t workplaceFlag = *(uint8_t*)(profKey + 0x4C);

    if (workplaceFlag != 0) {
        // ====== ASSIGNED PATH ======
        // Citizen has a specific workplace assignment.
        // Check if another workplace in a lower-priority region has vacancy.
        uintptr_t areaMap = *(uintptr_t*)(gwAddr + 0x118);

        // Get current workplace IBP and find StorageLocationComponent
        fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x248));
        uintptr_t currentWorkplace = *(uintptr_t*)(selfAddr + 0x248);

        uintptr_t storageLocIBP = 0;
        void* storageLocResult = fn_FindStorageLocationComp(currentWorkplace, &storageLocIBP);
        fn_InternalBasePtr_CheckPtr(storageLocResult);

        uintptr_t storageLocComp = *(uintptr_t*)storageLocResult;
        int32_t regionId = *(int32_t*)(storageLocComp + 0x28);

        // Look up current region in region map BST
        uintptr_t regionDesc = LookupRegionByID(areaMap, regionId);

        // *** FIX: null check for region lookup failure ***
        int currentPriority;
        if (regionDesc == 0) {
            // Region not in tree — don't switch, stay at current workplace.
            // Original game crashes here (null deref). Returning false keeps
            // citizens working instead of endlessly reshuffling.
            return false;
        } else {
            currentPriority = *(int32_t*)(regionDesc + 0x24);
        }

        // Check if current workplace is valid (reachable)
        uintptr_t workplaceRef = *(uintptr_t*)(selfAddr + 0x248);
        char valid = fn_CitizenComponent_CheckWorkplaceValid(self, &workplaceRef);
        if (!valid) {
            return true; // workplace not reachable -> should switch
        }

        // Iterate all available jobs looking for a better workplace
        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);

            // CheckPtr + FindWorkplaceComponent
            fn_InternalBasePtr_CheckPtr(&entityIBP);
            uintptr_t workplaceCompIBP = 0;
            fn_FindWorkplaceComp(entityIBP, &workplaceCompIBP);

            // CheckPtr + FindStorageLocationComponent
            fn_InternalBasePtr_CheckPtr(&entityIBP);
            uintptr_t otherStorageLocIBP = 0;
            fn_FindStorageLocationComp(entityIBP, &otherStorageLocIBP);

            // Skip if this is the current workplace
            fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x248));
            if (entityIBP == *(uintptr_t*)(selfAddr + 0x248))
                continue;

            // Check workplace is active (+0x50 flag)
            fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
            uintptr_t wpComp = workplaceCompIBP;
            if (*(uint8_t*)(wpComp + 0x50) == 0)
                continue;

            // Check has vacancy
            fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
            if (!fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                continue;

            // Look up this workplace's region (re-read areaMap fresh, matches original asm)
            fn_InternalBasePtr_CheckPtr(&otherStorageLocIBP);
            int32_t otherRegionId = *(int32_t*)(otherStorageLocIBP + 0x28);
            uintptr_t freshAreaMap = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uintptr_t otherRegionDesc = LookupRegionByID(freshAreaMap, otherRegionId);

            // Null check on region lookup (original would crash)
            if (otherRegionDesc == 0)
                continue;

            // If other region has lower priority value, switch
            if (*(int32_t*)(otherRegionDesc + 0x24) < currentPriority) {
                // Double-check vacancy
                fn_InternalBasePtr_CheckPtr(&workplaceCompIBP);
                if (fn_CitizenComponent_HasVacancy(workplaceCompIBP))
                    return true;
            }
        }

        return false;
    } else {
        // ====== UNASSIGNED PATH ======
        // Citizen has no specific workplace. Check if worker distribution is unbalanced.
        int maxGap = 0;
        uint32_t minGap = 0xFFFF;

        for (int64_t i = 0; i < (int64_t)jobCount; i++) {
            uintptr_t entityIBP = GetJobEntity(registry, profKey, i);
            fn_InternalBasePtr_CheckPtr(&entityIBP);

            // Find WorkplaceComponent on this entity
            uintptr_t entity = entityIBP;
            int compCount = *(int32_t*)(entity + 0x38);
            uintptr_t compArray = *(uintptr_t*)(entity + 0x30);

            // Scan for WorkplaceComponent by type ID
            uintptr_t wpComp = 0;
            uint32_t wpTypeId = *g_TypeId_WorkplaceComponent;
            for (int ci = 0; ci < compCount; ci++) {
                uintptr_t* compSlot = (uintptr_t*)(compArray + ci * 8);
                fn_InternalBasePtr_CheckPtr(compSlot);
                uintptr_t comp = *compSlot;
                // Call vtable[2] = GetTypeId
                uintptr_t vtable = *(uintptr_t*)comp;
                typedef uint32_t (*GetTypeId_t)(void*);
                uint32_t typeId = ((GetTypeId_t)(*(uintptr_t*)(vtable + 0x10)))((void*)comp);
                if (typeId == wpTypeId) {
                    // Verify with IsDerivedFrom
                    compSlot = (uintptr_t*)(compArray + ci * 8);
                    fn_InternalBasePtr_CheckPtr(compSlot);
                    uintptr_t finalComp = *compSlot;
                    if (fn_IsDerivedFrom((void*)finalComp, wpTypeId)) {
                        wpComp = finalComp;
                    }
                    break;
                }
            }

            // Construct IBP for found component
            uintptr_t wpIBP = 0;
            fn_InternalBasePtr_Ctor(&wpIBP, (void*)wpComp);
            fn_InternalBasePtr_CheckPtr(&wpIBP);

            uintptr_t wpCompVal = wpIBP;
            if (*(uint8_t*)(wpCompVal + 0x50) == 0)
                continue; // not active

            // Check workplace validity
            uintptr_t entityRef = entityIBP;
            char isValid = fn_CitizenComponent_CheckWorkplaceValid(self, &entityRef);
            if (!isValid)
                continue;

            // Get worker slot count via StorageLocationComponent
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            uintptr_t wpBase = wpIBP;
            fn_InternalBasePtr_CheckPtr((void*)(wpBase + 0x10));
            uintptr_t parentEntity = *(uintptr_t*)(wpBase + 0x10);

            uintptr_t slcIBP = 0;
            void* slcResult = fn_FindStorageLocationComp(parentEntity, &slcIBP);
            fn_InternalBasePtr_CheckPtr(slcResult);
            uintptr_t slc = *(uintptr_t*)slcResult;

            // Calculate worker slots (lazy-compute cached at +0x30)
            if (*(int32_t*)(slc + 0x30) == 0) {
                *(int32_t*)(slc + 0x30) = fn_CalcWorkerSlots((void*)slc);
            }
            int workerSlots = *(int32_t*)(slc + 0x30);

            // Get current worker count from WorkplaceComponent
            fn_InternalBasePtr_CheckPtr(&wpIBP);
            int currentWorkers = *(int32_t*)(wpIBP + 0x60);

            int gap = workerSlots - currentWorkers;
            if (gap > maxGap) maxGap = gap;
            if (gap < (int)minGap) minGap = (uint32_t)gap;
        }

        return (maxGap - (int)minGap) > 1;
    }
}
