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
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Local function pointers (resolved once on first use)
// ============================================================

// Object::Internal::~Internal — dtor for base class
typedef void (*fn_ObjectInternal_Dtor_t)(void* self);
static fn_ObjectInternal_Dtor_t s_fn_ObjectInternal_Dtor = nullptr;

// BST find-and-remove by key (CitizenAI_impl_5520 at 0x140025520)
typedef void (*fn_BSTFindRemove_t)(void* bstRoot, void* key);
static fn_BSTFindRemove_t s_fn_BSTFindRemove = nullptr;

// FUN_1400c5400 — cancel all work on action list
typedef void (*fn_TaskCancel_t)(void* actionList);
static fn_TaskCancel_t s_fn_TaskCancel = nullptr;

// FUN_140014c30 — recursive BST node free
typedef void (*fn_TreeNodeCleanup_t)(void* tree, void* branch);
static fn_TreeNodeCleanup_t s_fn_TreeNodeCleanup = nullptr;

// Vtable pointers
static void* s_vtable_ClearStorage = nullptr;
static void* s_vtable_Work = nullptr;

static bool s_initialized = false;

void ClearStorage_InitFuncs() {
    if (s_initialized) return;
    s_initialized = true;

    // Object::Internal::~Internal — IAT slot at 0x140152770 (verified from dtor disasm JMP)
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152770);
    if (iatDtor) s_fn_ObjectInternal_Dtor = *reinterpret_cast<fn_ObjectInternal_Dtor_t*>(iatDtor);

    s_fn_BSTFindRemove = reinterpret_cast<fn_BSTFindRemove_t>(
        ResolveAddr(Module::Exe, 0x140025520));

    s_fn_TaskCancel = reinterpret_cast<fn_TaskCancel_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));

    s_fn_TreeNodeCleanup = reinterpret_cast<fn_TreeNodeCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    s_vtable_ClearStorage = (void*)ResolveAddr(Module::Exe, ClearStorageAddr::Vtable);
    s_vtable_Work = (void*)ResolveAddr(Module::Exe, 0x140161a80);

#ifdef _DEBUG
    printf("[ClearStorage_InitFuncs] ObjDtor=%p BSTFindRemove=%p TaskCancel=%p TreeCleanup=%p\n",
           (void*)s_fn_ObjectInternal_Dtor, (void*)s_fn_BSTFindRemove,
           (void*)s_fn_TaskCancel, (void*)s_fn_TreeNodeCleanup);
    printf("[ClearStorage_InitFuncs] vtable_CS=%p vtable_Work=%p\n",
           s_vtable_ClearStorage, s_vtable_Work);
#endif
}

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
    void* entity = *(void**)entityRef;

    uint64_t positionIBP = 0;
    fn_FindComponent_Position(entity, &positionIBP);

    uint64_t unknownCompIBP = 0;
    entity = *(void**)entityRef;
    fn_FindComponent_Unknown7ee8(entity, &unknownCompIBP);

    uint64_t storageIBP = 0;
    entity = *(void**)entityRef;
    fn_FindComponent_Storage(entity, &storageIBP);

    // Get game world grid data
    uintptr_t gameWorld = *(uintptr_t*)g_pGameWorldSingleton;
    int* gridDataPtr = *(int**)(gameWorld + 0xf8);
    void* worldArray = *(void**)(gameWorld + 0xf0);

    // Get citizen position from citizenAI -> entity -> +0x278 -> +0x68
    uintptr_t citizenEntity = *(uintptr_t*)citizenAI;

    uintptr_t comp278 = *(uintptr_t*)(citizenEntity + 0x278);

    uintptr_t posData = *(uintptr_t*)(comp278 + 0x68);

    int gridSize = *gridDataPtr;
    // citizenY at +0x160 is stored but only used for searchPos[2] init
    (void)*(float*)(posData + 0x160);  // citizenY — unused in grid calc
    float citizenX = *(float*)(posData + 0x158);
    float citizenZ = *(float*)(posData + 0x15c);

    uint32_t srcCell = WorldToCell(citizenX, citizenZ, gridSize);

    // Get storage item count
    uintptr_t storageComp = *(uintptr_t*)&storageIBP;
    int itemCount = *(int*)(storageComp + 0x30);

    if (itemCount <= 0) {
        return 1;  // No items in storage
    }

    // Iterate over storage items
    for (int i = 0; i < itemCount; i++) {
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
        uintptr_t citizenEnt = *(uintptr_t*)citizenAI;
        uintptr_t citizenBuilding = *(uintptr_t*)(citizenEnt + 0x10);

        void* destStorage = nullptr;
        if (*(int*)(citizenBuilding + 0x38) > 0) {
            for (int64_t k = 0; k < *(int*)(citizenBuilding + 0x38); k++) {
                void* compIBP = (void*)(*(uintptr_t*)(citizenBuilding + 0x30) + k * 8);
                void* comp = *(void**)compIBP;
                typedef uint32_t (*fn_GetTypeId_t)(void*);
                fn_GetTypeId_t getTypeId = *(fn_GetTypeId_t*)(*(uintptr_t*)comp + 0x10);
                uint32_t typeId = getTypeId(comp);
                if (typeId == *g_TypeId_StorageComponent_FC) {
                    compIBP = (void*)(*(uintptr_t*)(citizenBuilding + 0x30) + k * 8);
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
        new (&destStorageIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(destStorage));   // B.3.3 bridge

        // Check capacity: destStorage capacity - count vs rawMat resourceSize * availCount
        uintptr_t dsc = *(uintptr_t*)&destStorageIBP;
        int maxCapacity = *(int*)(dsc + 0x38);

        dsc = *(uintptr_t*)&destStorageIBP;
        int currentCount = fn_StorageComponent_QueryCount((void*)dsc);

        int freeSpace = maxCapacity - currentCount;
        int resourceSize = rawMatDesc ? *(int*)((uintptr_t)rawMatDesc + 0x48) : 1;

        if (freeSpace < resourceSize * availCount) {
            // Not enough room in dest, check citizen's building storage
            uintptr_t cEnt = *(uintptr_t*)citizenAI;
            uintptr_t cBuilding = *(uintptr_t*)(cEnt + 0x10);

            uint64_t altStorageIBP = 0;
            fn_FindComponent_Storage((void*)cBuilding, &altStorageIBP);

            uintptr_t altSC = *(uintptr_t*)&altStorageIBP;
            int altMax = *(int*)(altSC + 0x38);

            altSC = *(uintptr_t*)&altStorageIBP;
            int altCurrent = fn_StorageComponent_QueryCount((void*)altSC);

            availCount = (altMax - altCurrent) / resourceSize;
        }

        if (availCount == 0) {
            // No capacity available, transition to long-term plan
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

        int dbgBranch = -1, dbgSlot = 0;   // trace: 0=position, 1=findslot
        if (unknownCompIBP == 0) {
            // No position component variant — use positionIBP
            if (positionIBP == 0) {
                return 1;
            }
            dbgBranch = 0;
            fn_Entity_GetPosition((void*)*(uintptr_t*)&positionIBP,
                                  &destPos, 0, 0, nullptr);
            destCell = WorldToCell(destPos[0], destPos[1], gridSize);
        } else {
            // Use search position
            dbgBranch = 1;
            destCell = WorldToCell(searchPos[0], searchPos[1], gridSize);

            void* unkComp = *(void**)&unknownCompIBP;
            int slotIdx = fn_Storage_FindSlot(unkComp, destCell, (float*)&destPos, workTypeId);
            dbgSlot = slotIdx;
            if (slotIdx < 0) {
                return 1;
            }
        }

        // Validate the work assignment
        void* citizenVal = *(void**)citizenAI;

        if (!fn_Work_ValidateAssignment(self, citizenAI, destCell, srcCell, time)) {
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
                                (int64_t)itemEntity, availCount, pathFlags, 0, 1, 0, 0);

        // Get the Slot10 result (vtable +0x50)
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
                uint64_t nodeKey = *(uint64_t*)searchNode;
                if (key < nodeKey) {
                    searchNode = *(uintptr_t*)(searchNode + 0x10);
                } else {
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

// ============================================================
// ClearStorage::ScalarDtor (vtable +0x00, 0x14002a100)
// ============================================================
// Calls the ClearStorage destructor (frees BST + aux data, resets vtable
// chain to Work, calls Internal::~Internal), then conditionally frees
// the object's memory.
void* ClearStorage_ScalarDtor_Impl(ClearStorage* self, uint32_t flags) {
    ClearStorage_InitFuncs();

    // --- ClearStorage destructor logic (0x1400e73b0) ---

    // Set vtable to ClearStorage (in case called from derived)
    self->vtable = s_vtable_ClearStorage;

    // Free BST at +0x58 (citizen assignment tree)
    void* bstRoot = self->_field58;
    if (bstRoot != nullptr) {
        s_fn_TreeNodeCleanup(&self->_field58, *(void**)((uintptr_t)bstRoot + 0x10));
        s_fn_TreeNodeCleanup(&self->_field58, *(void**)((uintptr_t)bstRoot + 0x18));
        fn_MemFreeStatic(bstRoot);
    }
    self->_field58 = nullptr;
    self->_field60 = 0;

    // Free auxiliary data at +0x48
    fn_MemFreeStatic(self->_field48);
    self->_field48 = nullptr;
    self->_field50 = nullptr;

    // Reset to Work vtable and call Internal::~Internal
    self->vtable = s_vtable_Work;
    s_fn_ObjectInternal_Dtor(self);

    // Conditionally free memory
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// ClearStorage::Slot6 (vtable +0x30, 0x1400e6c30)
// ============================================================
// BST find-and-remove by key on the citizen BST at self+0x58,
// then returns false (0). This removes a specific citizen's
// entry from the work tracking tree.
int ClearStorage_Slot6_Impl(ClearStorage* self, void* param2) {
    ClearStorage_InitFuncs();

    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x58), param2);
    return 0;
}

// ============================================================
// ClearStorage::Slot7 (vtable +0x38, 0x1400e6c20) -- task CANCELLED
// ============================================================
// Retail is the 6-byte thunk `ADD RCX,0x58; JMP CitizenAI_impl_5520`: the same
// remove-by-citizen as slot 6, without the return-0 wrapper.
//
// ⚠ This slot was pinned to Work_SlotRet0 ("NOT IMPLEMENTED"), so a cancelled
// clear-storage task left its citizen node in the +0x58 tree forever. That is the
// exact stale-entry family HarvestArea::Slot7 had (harvestarea.cpp): CancelAll below
// spins on the leftmost node until the count drops, and Validate hands the stale
// citizen pointer to Object::IsA, which CALLs through it. Ported 2026-09-05.
int ClearStorage_Slot7_Impl(ClearStorage* self, void* param2) {
    ClearStorage_InitFuncs();

    s_fn_BSTFindRemove((void*)((uintptr_t)self + 0x58), param2);
    return 0;
}

// ============================================================
// ClearStorage::Slot11 (vtable +0x58, 0x14002a0f0) -- HasActiveTasks
// ============================================================
// Retail: `return 0 < *(int*)(self + 0x60)`. ClearStorage keeps its citizen tree at
// +0x58 with the count at +0x60 (one qword later than the +0x48/+0x50 layout the
// shared Work_Slot11_HasActiveTasks reads), so it needs its own body. Was pinned to
// Work_SlotRet0, i.e. permanently "nobody is working this".
int ClearStorage_Slot11_Impl(ClearStorage* self) {
    return *(int*)((uintptr_t)self + 0x60) > 0 ? 1 : 0;
}

// ============================================================
// ClearStorage::CancelAll (Slot8, vtable +0x40, 0x1400e72a0)
// ============================================================
// Iterates the BST at self+0x58 by finding the leftmost node
// each iteration. For each node, resolves the citizen entity,
// navigates to its state machine component (+0x278), and cancels
// the pending work action via TaskCancel on its action list (+0x48).
// Repeats until the BST is empty (count at self+0x60 drops to 0).
void ClearStorage_CancelAll_Impl(ClearStorage* self) {
    ClearStorage_InitFuncs();

    while (true) {
        void* node = self->_field58;
        if (node == nullptr) {
            return;
        }

        // Find leftmost node in BST
        void* leftmost = node;
        void* left = *(void**)((uintptr_t)leftmost + 0x10);
        while (left != nullptr) {
            leftmost = left;
            left = *(void**)((uintptr_t)leftmost + 0x10);
        }

        if (leftmost == nullptr) {
            return;
        }

        // Resolve the citizen entity from the BST node
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to state machine component (+0x278) and cancel its action list (+0x48)
        s_fn_TaskCancel((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));
    }
}

// ============================================================
// ClearStorage::GetTarget (Slot10, vtable +0x50, 0x1400e7330)
// ============================================================
// Returns the map entity IBP from the game world singleton.
// Uses hidden return pointer ABI (member function returning non-trivial).
// Path: g_pGameWorldSingleton -> +0xb0 -> +0x30
void* ClearStorage_GetTarget_Impl(ClearStorage* self, void* retbuf) {
    (void)self;  // unused
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t mapHolder = *(uintptr_t*)(gameWorld + 0xb0);
    *(uintptr_t*)retbuf = *(uintptr_t*)(mapHolder + 0x30);
    return retbuf;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x1401611a0 (13 slots), size 0x68, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// *** FLAGS — this file is INCOMPLETE for a full factory ***
//   - No ClearStorage_Ctor / _Ctor_Impl exists -> object CANNOT be constructed.
//     The factory below registers the vtable but returns nullptr (no ctor).
//   - slot3  Serialize     : PORTED (ClearStorage_Serialize_Impl, 0x1400e7350)
//   Slot mapping ground-truthed from the real exe vtable @ 0x1401611a0 (2026-09-05):
//     slot6  OnComplete     -> ClearStorage_Slot6_Impl   (0x1400e6c30)
//     slot7  OnCancel       -> ClearStorage_Slot7_Impl   (0x1400e6c20, same remove)
//     slot8  Cleanup        -> ClearStorage_CancelAll_Impl (0x1400e72a0)
//     slot11 HasActiveTasks -> ClearStorage_Slot11_Impl  (0x14002a0f0, count at +0x60)
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ClearStorage_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ClearStorage");
    return c;
}

// ============================================================
// Slot3 Serialize (vtable +0x18, retail 0x1400e7350) — was Work_SerializeNop.
// ============================================================
// A no-op here consumes ZERO bytes on load while retail's body consumes the
// ClearStorage task BST, so every save that contains a live ClearStorage work
// object desynced the entity stream on the record AFTER it. Retail, read off the
// 0x1400e7350 disassembly (22 instructions, no branches):
//
//   Work::Serialize(this, stream)                       CALL 0x1400f7400
//   IO::Serializer ser(stream, 0)                       CALL [0x140152ad8]
//   CitizenAI_impl_96b0(this + 0x58, &ser)              CALL 0x1400296b0   <- the BST
//   ~IO::Serializer(ser)                                CALL [0x140152ad0]
//
// Identical in shape to FireWork::Serialize (0x1400e9a50, firework.cpp) minus the
// trailing capacity int — FireWork's BST is at +0x50, ClearStorage's at +0x58
// (LEA RCX,[RDI+0x58] at 0x1400e7381). Both sub-calls are already ported and
// registered (reg_work_base.cpp / reg_work_bst.cpp), so this is a transcription.
int ClearStorage_Serialize_Impl(ClearStorage* self, void* stream) {
    if (!self || !stream) return 0;
    const uintptr_t s = reinterpret_cast<uintptr_t>(self);

    typedef void (*CS_WorkSerialize_t)(void* self, void* stream);
    typedef void (*CS_BSTSerialize_t)(void* bstRoot, void* serializer);
    static CS_WorkSerialize_t s_workSer = nullptr;
    static CS_BSTSerialize_t  s_bstSer  = nullptr;
    if (!s_workSer)
        s_workSer = reinterpret_cast<CS_WorkSerialize_t>(ResolveAddr(Module::Exe, 0x1400f7400));
    if (!s_bstSer)
        s_bstSer  = reinterpret_cast<CS_BSTSerialize_t>(ResolveAddr(Module::Exe, 0x1400296b0));

    if (s_workSer) s_workSer(self, stream);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return 0;

    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);
    if (s_bstSer) s_bstSer(reinterpret_cast<void*>(s + 0x58), ser);
    g_fn_Serializer_Dtor(ser);
    return 0;
}

static void* g_ClearStorage_Vtable[13] = {
    (void*)&ClearStorage_ScalarDtor_Impl,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                    // +0x08 slot1 (null)
    (void*)&ClearStorage_GetTypeId,        // +0x10 GetTypeId
    (void*)&ClearStorage_Serialize_Impl,   // +0x18 Serialize  (0x1400e7350, PORTED)
    (void*)&ClearStorage_Execute,          // +0x20 Execute
    (void*)&Work_Slot5,                    // +0x28 slot5 (returns 0)
    (void*)&ClearStorage_Slot6_Impl,       // +0x30 OnComplete
    (void*)&ClearStorage_Slot7_Impl,       // +0x38 OnCancel   (0x1400e6c20)
    (void*)&ClearStorage_CancelAll_Impl,   // +0x40 Cleanup
    (void*)&ClearStorage_Validate,         // +0x48 Validate
    (void*)&ClearStorage_GetTarget_Impl,   // +0x50 GetTarget
    (void*)&ClearStorage_Slot11_Impl,      // +0x58 HasActiveTasks (0x14002a0f0: +0x60 > 0)
    (void*)&Work_Slot12,                   // +0x60 slot12 (void nop)
};

extern "C" void RegisterClearStorageVtable() {
    RegisterExeAddr(0x1401611a0, (uintptr_t)g_ClearStorage_Vtable);
}
extern "C" void* ClearStorage_CreateInPlace(void* mem) {
    ClearStorage_InitFuncs();
    return Work_GenericCtor(mem, 0x68, g_ClearStorage_Vtable);
}
extern "C" void* ClearStorage_Factory(void) {
    ClearStorage_InitFuncs();
    void* p = Work_Alloc(0x68);
    return p ? Work_GenericCtor(p, 0x68, g_ClearStorage_Vtable) : nullptr;
}
REGISTER_TYPE(ClearStorage, "Work", &ClearStorage_Factory, &ClearStorage_CreateInPlace);
