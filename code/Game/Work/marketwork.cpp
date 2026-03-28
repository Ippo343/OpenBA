// code/Game/Work/marketwork.cpp
// Clean C++ reimplementation of MarketWork -- all methods.
// Source: Game/Work/MarketWork.cpp (from profile build debug strings)
//
// MarketWork (0x58 bytes) -- Market trading work action.
// Inherits: Object::Internal -> Work (0x48) -> MarketWork
// Vtable: 0x140161710
//
// CRITICAL DIFFERENCE from HerbalistWork/HuntWork/PastureWork:
// MarketWork uses a DIFFERENT BST node layout with 0x38-byte nodes:
//   +0x00: key (InternalBasePtr)
//   +0x08: data pointer (allocated separately, freed in dtor)
//   +0x10: data2
//   +0x18: parent
//   +0x20: left child
//   +0x28: right child
// vs the simpler BST used by other Work classes:
//   +0x00: key, +0x08: parent, +0x10: left, +0x18: right
//
// This means BST traversal, removal, cleanup, and serialization
// use different offsets and different helper functions.
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- it is portable for standalone exe.

#include "Work/marketwork_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers (resolved once by MarketWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with MW_ to avoid conflicts

// Work::Serialize
typedef void  (*MW_Work_Serialize_t)(void* self, void* stream);
// IO::Serializer ctor/dtor
typedef void* (*MW_Serializer_Ctor_t)(void* self, void* stream, int version);
typedef void  (*MW_Serializer_Dtor_t)(void* self);
// IO::Serializer::GetVersion
typedef int   (*MW_Serializer_GetVersion_t)(void* serializer);
// CitizenAI_impl_96b0 -- simple BST serialization (v0 format)
typedef void  (*MW_BST_Serialize_Simple_t)(void* bstRoot, void* serializer);
// MarketWork_helper_b290 -- market BST serialization (v1 format)
typedef void  (*MW_MarketBST_Serialize_t)(void* bstRoot, void* serializer);
// MarketWork_sub_a9f0 -- market BST insert with data (v0 migration)
typedef void  (*MW_MarketBST_InsertData_t)(void* bstRoot, void* keyIBP);
// MarketWork_sub_aaa0 -- market BST insert (Slot6 helper)
typedef void  (*MW_MarketBST_Insert_t)(void* bstRoot, void* keyIBP);
// MarketWork_sub_ac40 -- market BST remove node
typedef void  (*MW_MarketBST_Remove_t)(void* bstRoot, void* node);
// FUN_14002af40 -- market tree cleanup (recursive, frees node + data)
typedef void  (*MW_TreeCleanup_t)(void* bstRoot, void* node, void* param3, void* param4);
// FUN_1400f7100 -- validate work entity
typedef bool  (*MW_ValidateEntity_t)(uintptr_t work, void* citizenIBP, void* entityIBP, void* outTask);
// FUN_1400c5400 -- cancel all work items
typedef void  (*MW_CancelAllWork_t)(void* bstRoot);
// FUN_140002e10 -- find sub-entity
typedef void* (*MW_FindSubEntity_t)(uintptr_t entity, void* outIBP);
// Simple BST tree node cleanup (for v0 migration temp tree)
typedef void  (*MW_SimpleBSTCleanup_t)(void* bstRoot, void* node);
// System::MemoryAllocator::MemAlloc / MemFree
typedef void* (*MW_MemAlloc_t)(int64_t size, int align);
typedef void  (*MW_MemFree_t)(void* ptr);
// fn_MemFreeStatic
typedef void  (*MW_MemFreeStatic_t)(void* ptr);
// Object::Internal::~Internal
typedef void  (*MW_Internal_Dtor_t)(void* self);
// Slot4 original (call through trampoline for now)
typedef int   (*MW_Slot4_Original_t)(void* self, void* param2, void* param3);

static MW_Work_Serialize_t          s_fn_Work_Serialize = nullptr;
static MW_Serializer_Ctor_t         s_fn_Serializer_Ctor = nullptr;
static MW_Serializer_Dtor_t         s_fn_Serializer_Dtor = nullptr;
static MW_Serializer_GetVersion_t   s_fn_Serializer_GetVersion = nullptr;
static MW_BST_Serialize_Simple_t    s_fn_BST_Serialize_Simple = nullptr;
static MW_MarketBST_Serialize_t     s_fn_MarketBST_Serialize = nullptr;
static MW_MarketBST_InsertData_t    s_fn_MarketBST_InsertData = nullptr;
static MW_MarketBST_Insert_t        s_fn_MarketBST_Insert = nullptr;
static MW_MarketBST_Remove_t        s_fn_MarketBST_Remove = nullptr;
static MW_TreeCleanup_t             s_fn_TreeCleanup = nullptr;
static MW_ValidateEntity_t          s_fn_ValidateEntity = nullptr;
static MW_CancelAllWork_t           s_fn_CancelAllWork = nullptr;
static MW_FindSubEntity_t           s_fn_FindSubEntity = nullptr;
static MW_SimpleBSTCleanup_t        s_fn_SimpleBSTCleanup = nullptr;
static MW_MemAlloc_t                s_fn_MemAlloc = nullptr;
static MW_MemFree_t                 s_fn_MemFree = nullptr;
static MW_MemFreeStatic_t           s_fn_MemFreeStatic = nullptr;

// Vtable pointers for dtor chain
static void** s_vtable_MarketWork = nullptr;   // 0x140161710
static void** s_vtable_Work = nullptr;          // 0x1401612e0

static bool s_market_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool MarketWork_ResolveFuncs() {
    if (s_market_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_Work_Serialize = reinterpret_cast<MW_Work_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400f7400));
    s_fn_BST_Serialize_Simple = reinterpret_cast<MW_BST_Serialize_Simple_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));  // verified from original disasm
    s_fn_ValidateEntity = reinterpret_cast<MW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_CancelAllWork = reinterpret_cast<MW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<MW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // MarketWork-specific BST helpers (DIFFERENT from HerbalistWork!)
    s_fn_MarketBST_Serialize = reinterpret_cast<MW_MarketBST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1400fb290));
    s_fn_MarketBST_InsertData = reinterpret_cast<MW_MarketBST_InsertData_t>(
        ResolveAddr(Module::Exe, 0x1400fa9f0));
    s_fn_MarketBST_Insert = reinterpret_cast<MW_MarketBST_Insert_t>(
        ResolveAddr(Module::Exe, 0x1400faaa0));
    s_fn_MarketBST_Remove = reinterpret_cast<MW_MarketBST_Remove_t>(
        ResolveAddr(Module::Exe, 0x1400fac40));

    // Market tree cleanup (recursive, frees node+data, different from simple BST)
    s_fn_TreeCleanup = reinterpret_cast<MW_TreeCleanup_t>(
        ResolveAddr(Module::Exe, 0x14002af40));

    // Simple BST cleanup (for v0 migration temp tree)
    s_fn_SimpleBSTCleanup = reinterpret_cast<MW_SimpleBSTCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Dispatch slots (indirect through .rdata)
    uintptr_t iatSerCtorSlot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iatSerCtorSlot)
        s_fn_Serializer_Ctor = *reinterpret_cast<MW_Serializer_Ctor_t*>(iatSerCtorSlot);
    uintptr_t iatSerDtorSlot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatSerDtorSlot)
        s_fn_Serializer_Dtor = *reinterpret_cast<MW_Serializer_Dtor_t*>(iatSerDtorSlot);

    // IO::Serializer::GetVersion dispatch slot
    uintptr_t iatSerGetVer = ResolveAddr(Module::Exe, 0x140151510);
    if (iatSerGetVer)
        s_fn_Serializer_GetVersion = *reinterpret_cast<MW_Serializer_GetVersion_t*>(iatSerGetVer);

    // MemAlloc/MemFree from dispatch slots
    uintptr_t iatMemAlloc = ResolveAddr(Module::Exe, 0x140151460);
    if (iatMemAlloc)
        s_fn_MemAlloc = *reinterpret_cast<MW_MemAlloc_t*>(iatMemAlloc);
    uintptr_t iatMemFree = ResolveAddr(Module::Exe, 0x140151468);
    if (iatMemFree)
        s_fn_MemFree = *reinterpret_cast<MW_MemFree_t*>(iatMemFree);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<MW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Vtable pointers for dtor
    s_vtable_MarketWork = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x140161710));
    s_vtable_Work = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401612e0));

    // Verify critical pointers
    bool ok = s_fn_Work_Serialize &&
              s_fn_Serializer_Ctor && s_fn_Serializer_Dtor &&
              s_fn_Serializer_GetVersion &&
              s_fn_BST_Serialize_Simple &&
              s_fn_MarketBST_Serialize &&
              s_fn_MarketBST_InsertData &&
              s_fn_MarketBST_Insert &&
              s_fn_MarketBST_Remove &&
              s_fn_TreeCleanup &&
              s_fn_ValidateEntity &&
              s_fn_CancelAllWork &&
              s_fn_FindSubEntity &&
              s_fn_SimpleBSTCleanup &&
              fn_InternalBasePtr_CheckPtr &&
              s_vtable_MarketWork && s_vtable_Work;

    s_market_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[MarketWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_Work_Serialize) printf("  MISSING: Work_Serialize\n");
        if (!s_fn_Serializer_Ctor) printf("  MISSING: Serializer_Ctor\n");
        if (!s_fn_Serializer_Dtor) printf("  MISSING: Serializer_Dtor\n");
        if (!s_fn_Serializer_GetVersion) printf("  MISSING: Serializer_GetVersion\n");
        if (!s_fn_BST_Serialize_Simple) printf("  MISSING: BST_Serialize_Simple\n");
        if (!s_fn_MarketBST_Serialize) printf("  MISSING: MarketBST_Serialize\n");
        if (!s_fn_MarketBST_InsertData) printf("  MISSING: MarketBST_InsertData\n");
        if (!s_fn_MarketBST_Insert) printf("  MISSING: MarketBST_Insert\n");
        if (!s_fn_MarketBST_Remove) printf("  MISSING: MarketBST_Remove\n");
        if (!s_fn_TreeCleanup) printf("  MISSING: TreeCleanup\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_SimpleBSTCleanup) printf("  MISSING: SimpleBSTCleanup\n");
        if (!fn_InternalBasePtr_CheckPtr) printf("  MISSING: InternalBasePtr_CheckPtr\n");
        if (!s_vtable_MarketWork) printf("  MISSING: vtable_MarketWork\n");
        if (!s_vtable_Work) printf("  MISSING: vtable_Work\n");
    }
#endif

    return ok;
}


// ============================================================
// MarketWork::Slot4 -- Main market trading logic
// ============================================================
//
// This function is extremely complex (~400+ instructions) and involves:
// - Max worker checks, citizen storage
// - MarketComponent/MarketDescription/StorageComponent lookups
// - Demand BST construction from all resource types
// - Supply/demand calculations across trade routes
// - Entity iteration to find available goods
// - Distance scoring and best entity selection
// - FindPath action creation and resource transfer
//
// Since the hook is DISABLED (MOV RAX,RSP prologue), this only runs
// in standalone exe mode. For now we call through to the original.
//
// TODO: Full reimplementation when all sub-functions are understood.
// The original function address is 0x1400eff80.
//
int MarketWork_Slot4_Impl(MarketWork* self, void* entityIBP, uint64_t /*param3*/, uint64_t /*param4*/) {
    // Hook is DISABLED -- this code path is only for standalone exe.
    // Call the original function directly via resolved address.
    typedef int (*MW_Slot4_Orig_t)(void* self, void* param2, void* param3);
    static MW_Slot4_Orig_t s_fn_Slot4_Original = nullptr;
    if (!s_fn_Slot4_Original) {
        s_fn_Slot4_Original = reinterpret_cast<MW_Slot4_Orig_t>(
            ResolveAddr(Module::Exe, 0x1400eff80));
    }
    if (s_fn_Slot4_Original) {
        return s_fn_Slot4_Original(self, entityIBP, nullptr);
    }
    return 0;
}


// ============================================================
// MarketWork::Slot6 -- Add entity to BST
// ============================================================
// Thunk: sub rsp,0x28; add rcx,0x48; call MarketWork_sub_aaa0; xor al,al; add rsp,0x28; ret
// Calls MarketBST_Insert on the BST at self+0x48, returns false.
// DISABLED hook (relative CALL).
//
bool MarketWork_Slot6_Impl(MarketWork* self, void* param2) {
    s_fn_MarketBST_Insert((void*)((uintptr_t)self + 0x48), param2);
    return false;
}


// ============================================================
// MarketWork::Slot7 -- Too small / not a real function
// ============================================================
// In Ghidra this shows as not a function. For other Work classes this
// is a 9-byte thunk: add rcx,0x48; jmp BSTFindRemove.
// MarketWork likely uses the same pattern with its own BST remove.
// Implemented for standalone exe completeness.
//
int MarketWork_Slot7_Impl(MarketWork* self, void* param2) {
    // MarketWork's BST remove uses different node layout
    // This is the "remove by key" equivalent for market BST
    s_fn_MarketBST_Remove((void*)((uintptr_t)self + 0x48), param2);
    return 0;
}


// ============================================================
// MarketWork::Slot8 / CancelAll
// ============================================================
// Loops through BST at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
//
// CRITICAL: Uses DIFFERENT BST offsets from HerbalistWork!
// - Left child at node+0x20 (not +0x10)
// - node[4] in longlong* terms = byte offset 0x20
//
int MarketWork_Slot8_Impl(MarketWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find leftmost node in BST (follow left children at +0x20)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            // Follow left child at +0x20 (MarketWork BST layout)
            node = *(void**)((uintptr_t)node + 0x20);
        }

        // Cancel the work item: get citizen entity from node key
        fn_InternalBasePtr_CheckPtr(leftmost);
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
        s_fn_CancelAllWork((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// MarketWork::Slot9 / RemoveInvalid
// ============================================================
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if all entries are valid (none removed).
//
// CRITICAL: Uses DIFFERENT BST node offsets from HerbalistWork!
// - Left child:  +0x20 (not +0x10)
// - Right child: +0x28 (not +0x18)
// - Parent:      +0x18 (not +0x08)
// Uses MarketWork_sub_ac40 for BST removal (not CitizenAI_impl_a6e0).
//
bool MarketWork_Slot9_Impl(MarketWork* self, void* param2) {
    uintptr_t selfAddr = (uintptr_t)self;
    void** bstRootPtr = (void**)((uintptr_t)self + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;  // Empty tree -- nothing to validate
    }

    // Find leftmost node (minimum) -- left child at +0x20
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x20);
    }

    // In-order traversal using MarketWork BST offsets
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

        // In-order successor using MarketWork BST offsets:
        // Right child at +0x28, left child at +0x20, parent at +0x18
        void* rightChild = *(void**)((uintptr_t)node + 0x28);
        if (rightChild != nullptr) {
            // Go right, then all the way left
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x20) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x20);
            }
        } else {
            // Go up until we come from a left child
            void* parent = *(void**)((uintptr_t)node + 0x18);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x20) != node) {
                // We came from the right -- keep going up
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x28) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x18);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST using MarketWork's BST remove
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            // Find and remove the node with this key from BST
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                fn_InternalBasePtr_CheckPtr(searchNode);
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    // Go left (+0x20 for MarketWork BST)
                    searchNode = *(void**)((uintptr_t)searchNode + 0x20);
                } else {
                    fn_InternalBasePtr_CheckPtr(&removeArr[i]);
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_MarketBST_Remove(bstRootPtr, searchNode);
                        break;
                    }
                    // Go right (+0x28 for MarketWork BST)
                    searchNode = *(void**)((uintptr_t)searchNode + 0x28);
                }
            }
        }
    }

    fn_MemFreeStatic(removeArr);
    return removeCount == 0;
}


// ============================================================
// MarketWork::Slot10 / GetWorkEntity
// ============================================================
// Returns entity from this->_field10's sub-object at +0x48.
// IDENTICAL pattern to other Work classes.
//
void* MarketWork_Slot10_Impl(MarketWork* self, void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)self;

    // CheckPtr on self+0x10
    fn_InternalBasePtr_CheckPtr((void*)(selfAddr + 0x10));
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    // Find sub-entity via FUN_140002e10
    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    // CheckPtr on the result
    fn_InternalBasePtr_CheckPtr(result);

    // Read +0x48 from the resolved entity and store in output
    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// MarketWork::Serialize
// ============================================================
// Work::Serialize + IO::Serializer(version=1) with version migration.
//
// Version 0 (old format):
//   Deserialize into temp simple BST, then re-insert each entry
//   into self+0x48 using MarketWork_sub_a9f0 (which allocates
//   the 0x38-byte nodes with data fields).
//   Then clean up the temp simple BST.
//
// Version 1+ (new format):
//   Call MarketWork_helper_b290 directly on self+0x48.
//
void MarketWork_Serialize_Impl(MarketWork* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Call Work::Serialize base
    s_fn_Work_Serialize(self, stream);

    // Create serializer with version=1
    uint8_t serializer[24] = {0};
    s_fn_Serializer_Ctor(serializer, stream, 1);

    int version = s_fn_Serializer_GetVersion(serializer);

    if (version == 0) {
        // OLD format: deserialize into temp simple BST, then migrate
        uintptr_t tempBST[2] = {0, 0};  // root=0, count=0

        // Deserialize using simple BST format
        s_fn_BST_Serialize_Simple(tempBST, serializer);

        // In-order traverse temp BST (simple layout: +0x08=parent, +0x10=left, +0x18=right)
        // For each entry, re-insert into MarketWork's BST using market insert
        void* tempRoot = (void*)tempBST[0];
        if (tempRoot != nullptr) {
            // Find leftmost in temp (simple BST: left at +0x10)
            void* node = tempRoot;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }

            // In-order traversal of simple BST
            while (node != nullptr) {
                // Re-insert this key into MarketWork's BST
                s_fn_MarketBST_InsertData((void*)(selfAddr + 0x48), node);

                // In-order successor (simple BST offsets)
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

            // Clean up temp simple BST
            // Free left subtree, then right subtree, then root
            s_fn_SimpleBSTCleanup(tempBST, *(void**)((uintptr_t)tempRoot + 0x10));
            s_fn_SimpleBSTCleanup(tempBST, *(void**)((uintptr_t)tempRoot + 0x18));
            if (s_fn_MemFreeStatic) {
                s_fn_MemFreeStatic(tempRoot);
            }
        }
    } else {
        // NEW format: call MarketWork_helper_b290 directly
        s_fn_MarketBST_Serialize((void*)(selfAddr + 0x48), serializer);
    }

    s_fn_Serializer_Dtor(serializer);
}


// ============================================================
// MarketWork::Execute -- returns false (3 bytes, unhookable)
// ============================================================
bool MarketWork_Execute_Impl(MarketWork* /*self*/) {
    return false;
}


// ============================================================
// MarketWork::DtorBody (called from ScalarDtor)
// ============================================================
// Sets vtables down chain, frees tree nodes at +0x48 using
// FUN_14002af40 (which also frees node data at +0x08), then
// calls Work -> Internal dtor chain.
//
// CRITICAL: Uses FUN_14002af40 for tree cleanup, NOT the simple
// TreeNodeCleanup used by HerbalistWork. FUN_14002af40 recurses
// via left(+0x20) and right(+0x28), frees data at +0x08.
//
void MarketWork_DtorBody(MarketWork* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    // Set vtable to MarketWork
    *(void**)selfAddr = s_vtable_MarketWork;

    // Free BST nodes at +0x48 using market-specific cleanup
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // FUN_14002af40 recursively frees left(+0x20), right(+0x28),
        // then frees data at +0x08, zeros +0x08 and +0x10, frees node
        s_fn_TreeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x20), nullptr, nullptr);
        s_fn_TreeCleanup((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x28), nullptr, nullptr);
        // Free root node's data and the root itself
        if (s_fn_MemFreeStatic) {
            void* rootData = *(void**)((uintptr_t)bstRoot + 0x08);
            if (rootData) {
                s_fn_MemFreeStatic(rootData);
            }
            s_fn_MemFreeStatic(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

    // Set vtable to Work, then call Internal dtor
    *(void**)selfAddr = s_vtable_Work;
    fn_ObjectInternal_Dtor((void*)selfAddr);
}


// ============================================================
// MarketWork::ScalarDtor (0x14002a910)
// ============================================================
void* MarketWork_ScalarDtor_Impl(MarketWork* self, uint32_t flags) {
    MarketWork_DtorBody(self);
    if (flags & 1) {
        if (s_fn_MemFreeStatic) {
            s_fn_MemFreeStatic(self);
        }
    }
    return self;
}


// ============================================================
// MarketWork::Slot11 -- returns (this->_field50 > 0)
// ============================================================
// 9 bytes, too small to hook.
bool MarketWork_Slot11_Impl(MarketWork* self) {
    return *(int*)((uintptr_t)self + 0x50) > 0;
}
