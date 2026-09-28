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

#include "Work/MarketWork.h"
#include "Work/marketwork_funcs.h"
#include "engine_funcs.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/MarketComponent.h"   // ENDGAME B.3.8 CB3: MarketComponent::TypeId()
#include "Component/MarketDescription.h"   // ENDGAME B.3.8 CB3: MarketDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/StorageComponent.h"   // ENDGAME B.3.8 CB3: StorageComponent::TypeId()
#include "Component/ConsumeProduceComponent.h"   // ENDGAME B.3.8 CB3: ConsumeProduceComponent::TypeId()
#include "Component/InteractComponent.h"   // ENDGAME B.3.8 CB3: InteractComponent::TypeId()
#include "Component/InteractDescription.h"   // ENDGAME B.3.8 CB3: InteractDescription::TypeId()
#include "Component/MapComponent.h"   // ENDGAME B.3.8 CB3: MapComponent::TypeId()
#include "Component/StorageLocationComponent.h"   // ENDGAME B.3.8 CB3: StorageLocationComponent::TypeId()

// ============================================================
// Local function pointers (resolved once by MarketWork_ResolveFuncs)
// ============================================================

// All typedefs prefixed with MW_ to avoid conflicts

// Work::Serialize
typedef void  (*MW_Work_Serialize_t)(void* self, void* stream);
// CitizenAI_impl_96b0 -- simple BST serialization (v0 format)
typedef void  (*MW_BST_Serialize_Simple_t)(void* bstRoot, void* serializer);
// MarketWork_helper_b290 -- market BST serialization (v1 format)
typedef void  (*MW_MarketBST_Serialize_t)(void* bstRoot, void* serializer);
// MarketWork_sub_a9f0 -- market BST insert with data (v0 migration).
// ⚠ FOUR args, not two. Ghidra's recovered prototype is
//   undefined8* __fastcall(longlong *tree, undefined8 *out, undefined8 *key, longlong *vec)
// and the retail call site in MarketWork::Serialize (0x1400f1d70) passes all four:
//   MarketWork_sub_a9f0(param_1 + 0x48, local_res18, puVar4, &local_30)
// This typedef previously declared only (bstRoot, keyIBP), so the call below left R8/R9
// holding garbage and a9f0 would have dereferenced it as `key` and `vec`. See
// reference_gate_dependency_lists_undercount / the "(...)() means args UNKNOWN" trap.
typedef void  (*MW_MarketBST_InsertData_t)(void* tree, void* outIbp, void* keyNode, void* vecDesc);
// MarketWork_sub_aaa0 -- market BST ERASE BY KEY. ⚠ NOT an insert, despite the old
// name here and Ghidra's. It searches for the node whose key equals *keyIBP and
// erases it via sub_ac40; a miss is a no-op. Both vtable slot 6 (0x1400eff60) and
// slot 7 (0x1400eff50) are 1-line thunks that call THIS.
typedef void  (*MW_MarketBST_EraseByKey_t)(void* bstRoot, void* keyIBP);
// MarketWork_sub_ac40 -- market BST ERASE BY NODE (already-located node): unlink,
// destroy via sub_aee0, AVL-retrace. Reached from sub_aaa0 and from slot 9
// (0x1400efd20), which locates the node itself before calling.
typedef void  (*MW_MarketBST_EraseByNode_t)(void* bstRoot, void* node);
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
// Slot4 original exe function (called directly for now)
typedef int   (*MW_Slot4_Original_t)(void* self, void* param2, void* param3);

static MW_BST_Serialize_Simple_t    s_fn_BST_Serialize_Simple = nullptr;
static MW_MarketBST_Serialize_t     s_fn_MarketBST_Serialize = nullptr;
static MW_MarketBST_InsertData_t    s_fn_MarketBST_InsertData = nullptr;
static MW_MarketBST_EraseByKey_t        s_fn_MarketBST_EraseByKey = nullptr;
static MW_MarketBST_EraseByNode_t        s_fn_MarketBST_EraseByNode = nullptr;
static MW_TreeCleanup_t             s_fn_TreeCleanup = nullptr;
static MW_ValidateEntity_t          s_fn_ValidateEntity = nullptr;
static MW_CancelAllWork_t           s_fn_CancelAllWork = nullptr;
static MW_FindSubEntity_t           s_fn_FindSubEntity = nullptr;
static MW_SimpleBSTCleanup_t        s_fn_SimpleBSTCleanup = nullptr;
static MW_MemAlloc_t                s_fn_MemAlloc = nullptr;
static MW_MemFree_t                 s_fn_MemFree = nullptr;
static MW_MemFreeStatic_t           s_fn_MemFreeStatic = nullptr;


// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: MarketWork_Ctor
// disassembles to `0x1400f1f8d: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// This was previously 0x1401612e0, which is not a vtable at all: it is an INTERIOR
// address of FireWork's vtable (starts 0x1401612a0, 13 slots, ends 0x140161308 at the
// "FishingWork" name string) — i.e. slot 8. Ghidra reports ZERO xrefs to 0x1401612e0.

static bool s_market_funcs_resolved = false;

// ============================================================
// Resolution -- called once during hook installation
// ============================================================
bool MarketWork_ResolveFuncs() {
    if (s_market_funcs_resolved) return true;

    // Direct exe function addresses
    s_fn_BST_Serialize_Simple = reinterpret_cast<MW_BST_Serialize_Simple_t>(
        ResolveAddr(Module::Exe, 0x1400296b0));  // verified from original disasm
    s_fn_ValidateEntity = reinterpret_cast<MW_ValidateEntity_t>(
        ResolveAddr(Module::Exe, 0x1400f7100));
    s_fn_CancelAllWork = reinterpret_cast<MW_CancelAllWork_t>(
        ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_FindSubEntity = reinterpret_cast<MW_FindSubEntity_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    // MarketWork-specific BST/AVL helpers (DIFFERENT from HerbalistWork!)
    // Level-1 caller-bug fix: the four literals below were dropped-digit typos - the 5th
    // nibble 'f' should be '2' (0x1400f.... -> 0x14002....). The bad addresses landed
    // MID-FUNCTION inside BridgeTool (0x1400f9cd0 / 0x1400fadc0); the real MarketWork AVL
    // helpers are the named MarketWork_sub_* functions.
    //
    // PORTED + REGISTERED 2026-08-22 (reg_marketwork_bst.cpp). The old "DEFERRED, resolves
    // to 0" note here is obsolete: the whole family now has real bodies in
    // code/Game/Work/marketwork_bst_*.cpp. Two corrections landed with that wave:
    //   * sub_a9f0 takes FOUR args, not two (see its typedef above).
    //   * sub_aaa0 is ERASE-BY-KEY, not insert; slot 7 was calling sub_ac40 by mistake.
    // Note also that only part of this "AVL block" is tree code at all - 0x14002afb0 is a
    // dynamic-array grow, and 0x14002b680/0x14002ba70 are two qsort instantiations.
    s_fn_MarketBST_Serialize = reinterpret_cast<MW_MarketBST_Serialize_t>(
        ResolveAddr(Module::Exe, 0x14002b290));
    s_fn_MarketBST_InsertData = reinterpret_cast<MW_MarketBST_InsertData_t>(
        ResolveAddr(Module::Exe, 0x14002a9f0));
    s_fn_MarketBST_EraseByKey = reinterpret_cast<MW_MarketBST_EraseByKey_t>(
        ResolveAddr(Module::Exe, 0x14002aaa0));
    s_fn_MarketBST_EraseByNode = reinterpret_cast<MW_MarketBST_EraseByNode_t>(
        ResolveAddr(Module::Exe, 0x14002ac40));

    // Market tree cleanup (recursive, frees node+data, different from simple BST)
    s_fn_TreeCleanup = reinterpret_cast<MW_TreeCleanup_t>(
        ResolveAddr(Module::Exe, 0x14002af40));

    // Simple BST cleanup (for v0 migration temp tree)
    s_fn_SimpleBSTCleanup = reinterpret_cast<MW_SimpleBSTCleanup_t>(
        ResolveAddr(Module::Exe, 0x140014c30));

    // Static MemAlloc/MemFree. These previously read exe dispatch slots 0x140151460 /
    // 0x140151468; 0x140151460 is NOT MemAlloc, it is Graphics::Geometry::GetTransform
    // (see placementtool.cpp:78). Since that slot is registered, the pointer was
    // non-null and WRONG, so the first array grow called GetTransform(size, align) and
    // took an ACCESS_VIOLATION. Diagnosed in tradework.cpp, where a TradeWork actually
    // reached its grow path; the same wrong pair was copied into five work classes.
    s_fn_MemAlloc = reinterpret_cast<MW_MemAlloc_t>(fn_MemAllocStatic);
    s_fn_MemFree  = reinterpret_cast<MW_MemFree_t>(fn_MemFreeStatic);

    // fn_MemFreeStatic from engine_funcs
    s_fn_MemFreeStatic = reinterpret_cast<MW_MemFreeStatic_t>(fn_MemFreeStatic);

    // Vtable pointers for dtor

    // Verify critical pointers
    bool ok = s_fn_BST_Serialize_Simple &&
              s_fn_MarketBST_Serialize &&
              s_fn_MarketBST_InsertData &&
              s_fn_MarketBST_EraseByKey &&
              s_fn_MarketBST_EraseByNode &&
              s_fn_TreeCleanup &&
              s_fn_ValidateEntity &&
              s_fn_CancelAllWork &&
              s_fn_FindSubEntity &&
              s_fn_SimpleBSTCleanup;

    s_market_funcs_resolved = ok;

#ifdef _DEBUG
    printf("[MarketWork] ResolveFuncs: %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        if (!s_fn_BST_Serialize_Simple) printf("  MISSING: BST_Serialize_Simple\n");
        if (!s_fn_MarketBST_Serialize) printf("  MISSING: MarketBST_Serialize\n");
        if (!s_fn_MarketBST_InsertData) printf("  MISSING: MarketBST_InsertData\n");
        if (!s_fn_MarketBST_EraseByKey) printf("  MISSING: MarketBST_EraseByKey\n");
        if (!s_fn_MarketBST_EraseByNode) printf("  MISSING: MarketBST_EraseByNode\n");
        if (!s_fn_TreeCleanup) printf("  MISSING: TreeCleanup\n");
        if (!s_fn_ValidateEntity) printf("  MISSING: ValidateEntity\n");
        if (!s_fn_CancelAllWork) printf("  MISSING: CancelAllWork\n");
        if (!s_fn_FindSubEntity) printf("  MISSING: FindSubEntity\n");
        if (!s_fn_SimpleBSTCleanup) printf("  MISSING: SimpleBSTCleanup\n");
    }
#endif

    return ok;
}


// ============================================================
// MarketWork::Slot6 -- retail 0x1400eff60 (Ghidra: "MarketWork_Draw", a misnomer)
// ============================================================
// Retail body is a 1-line thunk:
//     MarketWork_sub_aaa0(param_1 + 0x48, param_2); return 0;
// i.e. ERASE the entry keyed by param_2, and return false. It is NOT an "add" —
// the old comment here said "Add entity to BST" purely because sub_aaa0 was
// mis-named "insert". Verified 2026-08-22 by decompiling 0x1400eff60.
//
int MarketWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_MarketBST_EraseByKey((void*)((uintptr_t)this + 0x48), param2);
    return false;
}


// ============================================================
// MarketWork::Slot7 -- retail 0x1400eff50 (Ghidra: "MarketWork_OnCreate", a misnomer)
// ============================================================
// ⚠ FIXED 2026-08-22. Retail slot 7 is:
//     MarketWork_sub_aaa0(param_1 + 0x48, param_2); return;
// — the SAME erase-by-key helper slot 6 calls, differing only in return type
// (void vs bool). This port previously called sub_ac40 (erase-by-NODE) instead,
// which would have handed an InternalBasePtr* to a function that dereferences its
// second argument as a fully-formed 0x38-byte tree node: reading +0x18/+0x20/+0x28
// off a key would have unlinked garbage and corrupted the tree.
// Verified by decompiling 0x1400eff50 and 0x1400eff60 — both call 0x14002aaa0.
//
int MarketWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    s_fn_MarketBST_EraseByKey((void*)((uintptr_t)this + 0x48), param2);
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
int MarketWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
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
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
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
int MarketWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

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
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    // Go left (+0x20 for MarketWork BST)
                    searchNode = *(void**)((uintptr_t)searchNode + 0x20);
                } else {
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        s_fn_MarketBST_EraseByNode(bstRootPtr, searchNode);
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
void* MarketWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    // CheckPtr on self+0x10
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    // Find sub-entity via FUN_140002e10
    uintptr_t subIBP[2] = {0};
    void* result = s_fn_FindSubEntity(entityRef, subIBP);

    // CheckPtr on the result

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
void MarketWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Call Work::Serialize base
    Work::Persist(stream);   // 0x1400f7400, a direct call in retail — QUALIFIED here

    // Create serializer with version=1
    alignas(8) uint8_t serializer[24] = {0};
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge

    int version = reinterpret_cast<IO::Serializer*>(serializer)->GetVersion();   // B.3.4 bridge


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
                // Re-insert this key into MarketWork's BST.
                // Retail (0x1400f1d70) passes FOUR args:
                //   out  = a 2-qword stack InternalBasePtr receiving the inserted node
                //   key  = the v0 node itself (its +0x00 IS the key a9f0 reads)
                //   vec  = a zeroed 16-byte payload-array descriptor {data=0, count/cap=0},
                //          i.e. the migrated entry starts with an EMPTY payload; retail then
                //          "frees" it with MemFree(nullptr), a deliberate no-op on the
                //          always-null data pointer of that temporary.
                uintptr_t outIbp[2] = {0, 0};
                uintptr_t vecDesc[2] = {0, 0};
                s_fn_MarketBST_InsertData((void*)(selfAddr + 0x48), outIbp, node, vecDesc);
                vecDesc[0] = 0;
                vecDesc[1] = 0;

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

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// MarketWork::Execute (slot 4) -- STAGE 1 dependencies
// ============================================================
// Retail 0x1400eff80 is ~1408 decompiled lines and is being landed in stages.
// STAGE 1 is the entry block, 0x1400eff80 .. 0x1400f021f: the guards, the two
// cheap "return 1" rejections and the "citizen is already carrying, go deliver"
// arm that returns 2. Everything past 0x1400f021f (the demand array, the two
// qsorts, the candidate scoring and the FindPath/TransferToCarrier claim) is
// NOT ported yet and is represented by a single "keep walking" return 0.
//
// Ghidra names this MarketWork_Serialize. It is not serialization.
//
typedef int      (*MW_CalcMaxWorkers_t)(uintptr_t comp);                    // 0x14012e5c0
typedef void*    (*MW_FindStorage_t)(uintptr_t entity, void* outIBP);       // 0x140006270
typedef int      (*MW_StorageQueryCount_t)(void* storage);                  // 0x14012cf10
typedef void     (*MW_LongTermPlan_t)(uintptr_t citizen);                   // 0x140115e60
typedef int      (*MW_FreeCarryCapacity_t)(uintptr_t citizen);              // 0x140116540
// stage 2 (see the STAGE 2 dependencies block further down for what they are)
typedef void     (*MW_ArrayGrow_t)(int64_t* arrDesc, int32_t newCount);     // 0x14002afb0
typedef void     (*MW_AvlRetrace_t)(int64_t* treeDesc, int64_t* cursor);    // 0x140014b40
typedef void     (*MW_TreeFree_t)(int64_t ctx, void* node);                 // 0x140014b00

// stage 3
typedef int64_t* (*MW_PickDropTarget_t)(int64_t** town, int64_t* outEntity, uint32_t cellIdx,
                                        uint64_t resType, int count, char acceptTest,
                                        uint32_t* outPos, void* excludeIBP);   // 0x14008d590
typedef void*    (*MW_EntityResolvePos_t)(void* comp, float* outVec3, int p3, int p4, float* p5);
                                                                              // 0x140122e20
typedef int      (*MW_RecursiveChildSum_t)(int64_t child, int mode);           // 0x140123070
typedef void     (*MW_QsortDemand_t)(void* base, int count);                   // 0x14002b680
typedef void     (*MW_FindPath_t)(void* outResult, void* citizenIBP, int pathType,
                                  void* destEntityIBP, float* destPos, void* srcEntityIBP,
                                  float* srcPos, int64_t materialTypeEntity, int itemCount,
                                  int flags, int p11, char p12, int p13, int p14); // 0x1400bf790

static MW_ArrayGrow_t         s_fn_ArrayGrow         = nullptr;
static MW_AvlRetrace_t        s_fn_AvlRetrace        = nullptr;
static MW_TreeFree_t          s_fn_TreeFree          = nullptr;
// stage 4 / 5
typedef int   (*MW_EntityAnchorCell_t)(void* entityIBP, float* p2);         // 0x140087fb0
typedef void  (*MW_ZoneRefresh_t)(int* terrain, void*, void*, int);         // 0x14009c480
typedef int   (*MW_IsDstReachable_t)(int* terrain, uint32_t dst, uint32_t src, char f);
                                                                            // 0x14009f810
typedef float (*MW_Sqrtf_t)(float v);                                       // 0x140132010
typedef void  (*MW_QsortTake_t)(void* base, int count);                     // 0x14002ba70
typedef int   (*MW_FindBestAdjacentCell_t)(int64_t comp, uint32_t cell, float* outPos,
                                           int64_t roll);                   // 0x14012d5d0
typedef void  (*MW_TransferToCarrier_t)(int64_t path, int64_t resType, int count);
                                                                            // 0x1400ca230
typedef void  (*MW_IbpDefaultCtor_t)(void* self);            // dispatch slot 0x140152ac0
typedef void  (*MW_IbpCopy_t)(void* self, void* other);      // dispatch slot 0x140152ab8

static MW_EntityAnchorCell_t    s_fn_EntityAnchorCell    = nullptr;
static MW_ZoneRefresh_t         s_fn_ZoneRefresh         = nullptr;
static MW_IsDstReachable_t      s_fn_IsDstReachable      = nullptr;
static MW_Sqrtf_t               s_fn_Sqrtf               = nullptr;
static MW_QsortTake_t           s_fn_QsortTake           = nullptr;
static MW_FindBestAdjacentCell_t s_fn_FindBestAdjacentCell = nullptr;
static MW_TransferToCarrier_t   s_fn_TransferToCarrier   = nullptr;
static MW_IbpDefaultCtor_t      s_fn_IbpDefaultCtor      = nullptr;
static MW_IbpCopy_t             s_fn_IbpCopy             = nullptr;
static MW_PickDropTarget_t    s_fn_PickDropTarget    = nullptr;
static MW_EntityResolvePos_t  s_fn_EntityResolvePos  = nullptr;
static MW_RecursiveChildSum_t s_fn_RecursiveChildSum = nullptr;
static MW_QsortDemand_t       s_fn_QsortDemand       = nullptr;
static MW_FindPath_t          s_fn_FindPath          = nullptr;
static MW_CalcMaxWorkers_t    s_fn_CalcMaxWorkers    = nullptr;
static MW_FindStorage_t       s_fn_FindStorage       = nullptr;
static MW_StorageQueryCount_t s_fn_StorageQueryCount = nullptr;
static MW_LongTermPlan_t      s_fn_LongTermPlan      = nullptr;
static MW_FreeCarryCapacity_t s_fn_FreeCarryCapacity = nullptr;
static void**                 s_pGameWorldSingleton  = nullptr;  // DAT_1401c3dd8
static bool                   s_slot4_resolved       = false;

// ENDGAME B.3.9 D5: every type-id cell Slot4 reads (DAT_1401c44f0 = MarketComponent,
// DAT_1401c4528 = MarketDescription, and the rest below) is read BY CELL through the
// class's own TypeId() -- the old MW_TypeId() wrapper (read the exe .rdata cell,
// fall back to the class static if the cell was 0 or the pre-Startup sentinel
// 0xFFFFFFFF) is gone: the class static IS that cell's value once Factory::Startup
// has run, and nothing here runs before Startup.
static void MarketWork_Slot4_ResolveFuncs() {
    if (s_slot4_resolved) return;
    s_slot4_resolved = true;
    s_fn_CalcMaxWorkers    = reinterpret_cast<MW_CalcMaxWorkers_t   >(ResolveAddr(Module::Exe, 0x14012e5c0));
    s_fn_FindStorage       = reinterpret_cast<MW_FindStorage_t      >(ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount = reinterpret_cast<MW_StorageQueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_fn_LongTermPlan      = reinterpret_cast<MW_LongTermPlan_t     >(ResolveAddr(Module::Exe, 0x140115e60));
    s_fn_FreeCarryCapacity = reinterpret_cast<MW_FreeCarryCapacity_t>(ResolveAddr(Module::Exe, 0x140116540));
    s_pGameWorldSingleton  = reinterpret_cast<void**                >(ResolveAddr(Module::Exe, 0x1401c3dd8));
    s_fn_ArrayGrow         = reinterpret_cast<MW_ArrayGrow_t        >(ResolveAddr(Module::Exe, 0x14002afb0));
    s_fn_AvlRetrace        = reinterpret_cast<MW_AvlRetrace_t       >(ResolveAddr(Module::Exe, 0x140014b40));
    s_fn_TreeFree          = reinterpret_cast<MW_TreeFree_t         >(ResolveAddr(Module::Exe, 0x140014b00));
    s_fn_PickDropTarget    = reinterpret_cast<MW_PickDropTarget_t   >(ResolveAddr(Module::Exe, 0x14008d590));
    s_fn_EntityResolvePos  = reinterpret_cast<MW_EntityResolvePos_t >(ResolveAddr(Module::Exe, 0x140122e20));
    s_fn_RecursiveChildSum = reinterpret_cast<MW_RecursiveChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_fn_QsortDemand       = reinterpret_cast<MW_QsortDemand_t      >(ResolveAddr(Module::Exe, 0x14002b680));
    s_fn_FindPath          = reinterpret_cast<MW_FindPath_t         >(ResolveAddr(Module::Exe, 0x1400bf790));
    s_fn_EntityAnchorCell     = reinterpret_cast<MW_EntityAnchorCell_t   >(ResolveAddr(Module::Exe, 0x140087fb0));
    s_fn_ZoneRefresh          = reinterpret_cast<MW_ZoneRefresh_t        >(ResolveAddr(Module::Exe, 0x14009c480));
    s_fn_IsDstReachable       = reinterpret_cast<MW_IsDstReachable_t     >(ResolveAddr(Module::Exe, 0x14009f810));
    s_fn_Sqrtf                = reinterpret_cast<MW_Sqrtf_t              >(ResolveAddr(Module::Exe, 0x140132010));
    s_fn_QsortTake            = reinterpret_cast<MW_QsortTake_t          >(ResolveAddr(Module::Exe, 0x14002ba70));
    s_fn_FindBestAdjacentCell = reinterpret_cast<MW_FindBestAdjacentCell_t>(ResolveAddr(Module::Exe, 0x14012d5d0));
    s_fn_TransferToCarrier    = reinterpret_cast<MW_TransferToCarrier_t  >(ResolveAddr(Module::Exe, 0x1400ca230));
    // ⚠ These two .rdata dispatch slots are NOT in the standalone address registry
    // (nothing anywhere calls RegisterDispatchSlot for them), so they resolve to 0.
    // MW_MakeStrongRef below falls back to fn_InternalBasePtr_Ctor rather than
    // EXEC-of-0. See the note there.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ac0))
        s_fn_IbpDefaultCtor = *reinterpret_cast<MW_IbpDefaultCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ab8))
        s_fn_IbpCopy = *reinterpret_cast<MW_IbpCopy_t*>(s);
}

// Retail's S5 builds each FindPath argument as
//     Object::InternalBasePtr::InternalBasePtr(&tmp);        [slot 0x140152ac0]
//     Object::InternalBasePtr::Copy(&tmp, &src);             [slot 0x140152ab8]
// i.e. "give me a strong ref to the same Internal". NEITHER slot is registered in
// the standalone build today, so calling them would be an EXEC-of-0 on the S5 claim
// path -- the one path we are trying to exercise. This is a genuine
// standalone-vs-retail difference, not a faithfulness fix, so it is guarded and
// stated rather than silently papered over: when the slots exist we use them, and
// when they do not we construct the same end state with fn_InternalBasePtr_Ctor,
// the resolved constructor every other InternalBasePtr in this file already goes
// through. Register the two slots and this fallback stops being reached.
static void MW_MakeStrongRef(uintptr_t* dst, uintptr_t* src) {
    if (s_fn_IbpDefaultCtor && s_fn_IbpCopy) {
        s_fn_IbpDefaultCtor(dst);
        s_fn_IbpCopy(dst, src);
        return;
    }
    dst[0] = 0;
    dst[1] = 0;
    new (dst) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(src[0]));   // B.3.3 bridge
}

// Retail inlines this component scan at every site; slot 4 alone contains eight
// copies of it. Entity layout: component InternalBasePtr array at entity+0x30,
// int32 count at entity+0x38. Retail re-loads the array base inside the loop and
// re-CheckPtrs the slot after the match, which is reproduced here.
static void* MW_FindComponentByTypeId(uintptr_t entity, uint32_t typeId) {
    const int count = *reinterpret_cast<int32_t*>(entity + 0x38);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(
            *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
        void* comp = *slot;
        if (reinterpret_cast<Object::Base*>(comp)->Type() == typeId) {   // B.3.8 bridge
            slot = reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(entity + 0x30) + static_cast<int64_t>(i) * 8);
            comp = *slot;
            return fn_IsDerivedFrom(comp, typeId) ? comp : nullptr;
        }
    }
    return nullptr;
}


// ---- STAGE 2 dependencies -------------------------------------------------
// 0x14002afb0 grows the 0x20-byte demand-record array (ported as MarketBst_afb0).
// 0x140014b40 is the SHARED AVL retrace and 0x140014b00 the shared recursive
// free; both operate on the same +0x18/+0x20/+0x28/+0x30 node shape.
//
// ⚠ 0x14002a9f0 is deliberately NOT used by stage 2. It inserts into MarketWork's
// OWN tree at self+0x48, and retail calls it exactly twice, both times as the
// final act of a successful claim (0x1400f0e49 and 0x1400f1bd4). The tree stage 2
// builds is a function-local one whose inserts retail fully inlines -- three
// separate MemAlloc(0x38,8) sites at 0x1400f02d1 / 0x1400f0363 / 0x1400f03a6.
// Routing stage 2 through a9f0 would write the town's demand nodes into the live
// claim registry.
// (the three function pointers themselves are declared with the stage-1 set
// above, so MarketWork_Slot4_ResolveFuncs can fill them in one place)
//
// The element type of the afb0 array. afb0's default-init deliberately leaves
// +0x00..+0x03 untouched and zeroes +0x04..+0x1f, which is exactly why retail
// writes the resource mask into +0x00 itself immediately after the grow.
struct MW_DemandRecord {
    uint32_t resourceMask;   // +0x00 from MarketDescription+0x28 entry+0x00
    uint32_t inMarket;       // +0x04 loop A: unitSize * available, this market
    uint32_t inTransit;      // +0x08 loop B: unitSize * qty already claimed
    uint32_t inTown;         // +0x0c loop C: unitSize * available, town-wide
    float    stockedFrac;    // +0x10 stage 3
    float    satisfaction;   // +0x14 stage 3 -- the b680 qsort key
    uint32_t stage4Scratch;  // +0x18 stage 4 (zeroed per candidate)
    int32_t  shortfall;      // +0x1c stage 3
};
static_assert(sizeof(MW_DemandRecord) == 0x20, "demand record stride is 0x20");

// The 0x10-byte {data,count,capacity} descriptor afb0 takes in RCX.
struct MW_ArrayDesc {
    uint8_t* data;
    int32_t  count;
    int32_t  capacity;
};
static_assert(sizeof(MW_ArrayDesc) == 0x10, "array descriptor is 0x10 bytes");

// The function-local supply/demand tree. Same 0x38-byte node shape as
// MarketWork's own BST, but +0x08..+0x17 are three uint32 accumulators instead
// of a payload array. Retail zeroes that span with two 8-byte stores.
namespace {
constexpr uintptr_t kNodeKey     = 0x00;
constexpr uintptr_t kNodeAccumA  = 0x08;  // loop A, weighted (see below)
constexpr uintptr_t kNodeAccumC  = 0x0c;  // loop C, town-wide available
constexpr uintptr_t kNodeAccumB  = 0x10;  // loop B, in-transit quantity
constexpr uintptr_t kNodeParent  = 0x18;
constexpr uintptr_t kNodeLeft    = 0x20;
constexpr uintptr_t kNodeRight   = 0x28;
constexpr uintptr_t kNodeBalance = 0x30;
constexpr int64_t   kNodeSize    = 0x38;
}

// Retail inlines this descent at all three accumulation sites (0x1400f04c0,
// 0x1400f0607, 0x1400f08e8).
// ⚠ On a miss -- and on an empty tree -- retail leaves the cursor NULL and then
// unconditionally executes ADD [cursor+off],reg. That deref is load-bearing: it
// relies on every resource type reachable from a storage list also being present
// in the town resource registry the tree was built from. Callers below reproduce
// the unguarded write; do not add a null check here.
static uint8_t* MW_TreeFind(uint8_t* root, uintptr_t key) {
    uint8_t* cur = root;
    while (cur != nullptr) {
        const uintptr_t k = *reinterpret_cast<uintptr_t*>(cur + kNodeKey);
        if (key < k)      cur = *reinterpret_cast<uint8_t**>(cur + kNodeLeft);
        else if (k < key) cur = *reinterpret_cast<uint8_t**>(cur + kNodeRight);
        else              return cur;
    }
    return nullptr;
}

// DIAGNOSTIC ONLY, printed right before the faithful null deref described above fires:
// WHICH storage entity holds WHICH material type that the town resource registry
// (townMgr+0x110) never learned about. Retail's invariant is "every type in any storage
// list was registered"; when it breaks here the culprit is whatever deposited the
// material without RawMaterialComponent_RegisterRawMaterial, not this walk. First seen
// in the BANISHED_STRESS town (2026-09-03) at loop C, frame ~8400.
static void MW_ReportTreeMiss(const char* site, int entityIdx, uintptr_t entityIBP,
                              uintptr_t resType, void* rawMat, uintptr_t town) {
    char path[160] = "?";
    if (resType) {
        // the hidden return slot GetPath constructs into
        System::Filename fnPath =
            reinterpret_cast<const ::Object::External*>(resType)->GetPath();  // B.3.3 bridge
        const char* s = fnPath._data;
        if (s) {
            size_t i = 0;
            for (; i + 1 < sizeof(path) && s[i] >= 0x20 && s[i] < 0x7f; i++) path[i] = s[i];
            path[i] = '\0';
        }
    }
    const int regCount = *reinterpret_cast<int32_t*>(town + 0x118);
    fprintf(stderr, "[MarketWork::Execute] TREE MISS at %s: storage entity[%d]=%p holds resType=%p "
            "(%s) rawMatDesc=%p flags=0x%x -- not among the %d types in townMgr+0x110; "
            "about to fault faithfully\n",
            site, entityIdx, reinterpret_cast<void*>(entityIBP), reinterpret_cast<void*>(resType),
            path, rawMat,
            rawMat ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x5c) : 0,
            regCount);
    fflush(stderr);
}

// The other inlined scan shape in this function: a Description's CHILD array,
// which is a raw pointer array at obj+0x28 with an int32 count at obj+0x30 --
// no InternalBasePtr, hence no CheckPtr (verified at 0x1400f0450).
// Distinct from MW_FindComponentByTypeId, which walks an Entity's +0x30/+0x38
// array of InternalBasePtrs.
static void* MW_FindChildByTypeId(uintptr_t obj, uint32_t typeId) {
    const int count = *reinterpret_cast<int32_t*>(obj + 0x30);
    for (int i = 0; i < count; ++i) {
        void* child = *reinterpret_cast<void**>(
            *reinterpret_cast<uintptr_t*>(obj + 0x28) + static_cast<int64_t>(i) * 8);
        if (reinterpret_cast<Object::Base*>(child)->Type() == typeId) {   // B.3.8 bridge
            child = *reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(obj + 0x28) + static_cast<int64_t>(i) * 8);
            return fn_IsDerivedFrom(child, typeId) ? child : nullptr;
        }
    }
    return nullptr;
}

// Retail's shared epilogue at 0x1400f1c15: free the local tree (two recursive
// subtree frees plus the root) and the record array, then return. Stage 2 runs
// it on its own exit path because retail runs it on EVERY exit path from
// 0x1400f021f onward -- skipping it would leak a tree and an array per call.
static void MW_Stage2Teardown(int64_t* treeDesc, MW_ArrayDesc* records) {
    uint8_t* root = *reinterpret_cast<uint8_t**>(treeDesc);
    if (root != nullptr) {
        s_fn_TreeFree(reinterpret_cast<int64_t>(treeDesc),
                      *reinterpret_cast<void**>(root + kNodeLeft));
        s_fn_TreeFree(reinterpret_cast<int64_t>(treeDesc),
                      *reinterpret_cast<void**>(root + kNodeRight));
        s_fn_MemFree(root);
    }
    treeDesc[0] = 0;
    *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(treeDesc) + 8) = 0;
    s_fn_MemFree(records->data);
}


// ============================================================
// MarketWork::Execute -- returns false (3 bytes, unhookable)
// ============================================================
// ⚠ THIS IS THE REAL SLOT 4 — vtable index 4 (+0x20), retail 0x1400eff80.
// A second body, MarketWork_Slot4_Impl, used to sit above claiming the same retail
// address while being dispatched by nothing. It was deleted 2026-08-22: two ports of
// one retail function drift apart and the dead copy accumulates stale notes (its
// header still described the BST block as "BLOCKED", long after that stopped being
// true). See reference_one_retail_fn_ported_twice_diverges.
//
// PORTING SPEC for whoever lands the 1408-line body, recovered from the ONLY call
// site — the citizen idle claim-walk, retail 0x1400f7b60 (ported as Helper_sub_7b60,
// citizenai_housing_idle.cpp:506, vcall at :527):
//
//     uVar2 = (**(code **)(*(longlong *)*plVar4 + 0x20))(work, local_res18, param_1 + 0x130);
//     if (uVar2 == 2) return 1;                       // CLAIMED -> stop the walk
//     if (uVar2 == 1) work[+0x2c] = sched[+8];        // visited-this-tick stamp
//
// so the true signature is THREE args, not the four the deleted copy declared:
//     int Execute(MarketWork* self, InternalBasePtr* citizenHolder, void* ctx /*sched+0x130*/)
// and the return is a TRI-STATE: 2 = claimed (walk stops), 1 = examined-but-declined
// (stamp it, keep walking), 0 = keep walking without stamping. Slot 9 (Validate) is
// called immediately BEFORE this on the same object, unconditionally.
//
// Retail has NO unconditional per-tick call of slot 4 anywhere; it is purely
// claim-driven, so it fires only when a citizen whose profession key (+0x1f0) matches
// this workplace's WPC+0x48 enters AI state Idle.
// ---- STAGE 1 (retail 0x1400eff80 .. 0x1400f021f) --------------------------
// Returns 1 at two sites and 2 at one site, all faithful. Falls through to 0
// ("keep walking") where stage 2 will begin.
int MarketWork::Execute(void* citizenHolder, void* ctx) {
    (void)ctx;   // sched+0x130; retail slot 4 never reads param_3 -- it is only a
                 // decompiler artefact of a caller-saved stack slot reused as a temp.

    MarketWork_Slot4_ResolveFuncs();

    const uintptr_t a = reinterpret_cast<uintptr_t>(this);

    // --- 0x1400effd0..0x1400efffc: hauler-slot cap ------------------------
    // EDI = MarketWork+0x50, the BST node count = citizens already claimed by
    // this market. self+0x18 is the market's work Component; its int32 at +0x30
    // is a lazily-filled cache of "how many haulers may work here at once",
    // computed by CalcMaxWorkers (0x14012e5c0) from the WorkDescription at
    // component+0x18. Full -> 1 = examined but declined.
    const int claimed = *reinterpret_cast<int32_t*>(a + 0x50);
    const uintptr_t workComp = *reinterpret_cast<uintptr_t*>(a + 0x18);
    if (*reinterpret_cast<int32_t*>(workComp + 0x30) == 0) {
        *reinterpret_cast<int32_t*>(workComp + 0x30) = s_fn_CalcMaxWorkers(workComp);
    }
    const int maxHaulers = *reinterpret_cast<int32_t*>(workComp + 0x30);
    if (maxHaulers <= claimed) {
        return 1;
    }

    // --- 0x1400f0001..0x1400f005c: citizen is already carrying ------------
    // *citizenHolder is the CitizenComponent; its +0x10 is the citizen Entity.
    // If that entity's own StorageComponent holds anything, the citizen must go
    // dump it first: push AI state LongTermPlan and report 2 = CLAIMED, which
    // stops the caller's claim-walk.
    const uintptr_t citizen = *reinterpret_cast<uintptr_t*>(citizenHolder);
    uintptr_t carriedIBP[2] = {0, 0};
    s_fn_FindStorage(*reinterpret_cast<uintptr_t*>(citizen + 0x10), carriedIBP);
    const int carried = s_fn_StorageQueryCount(reinterpret_cast<void*>(carriedIBP[0]));
    if (carried > 0) {
        s_fn_LongTermPlan(*reinterpret_cast<uintptr_t*>(citizenHolder));
        return 2;
    }

    // --- 0x1400f0061..0x1400f0118: the market's MarketDescription ---------
    // self+0x10 is the market Entity. Scan it for the MarketComponent, wrap the
    // hit in an InternalBasePtr (retail keeps a strong ref for the rest of the
    // body), then take component+0x18 = its Description, narrowed to
    // MarketDescription. MarketDescription layout (marketdescription.cpp):
    //   +0x20 float  global fill ratio (defaults to 1.0)
    //   +0x28 array  per-resource {enum @+0, float ratio @+4}, stride 8
    //   +0x30 int32  element count
    const uint32_t tidMarketComponent = MarketComponent::TypeId();
    const uint32_t tidMarketDescription = MarketDescription::TypeId();
    const uint32_t tidRawMaterialDescription = RawMaterialDescription::TypeId();
    const uint32_t tidStorageComponent = StorageComponent::TypeId();
    const uint32_t tidConsumeProduceComponent = ConsumeProduceComponent::TypeId();
    const uint32_t tidInteractComponent = InteractComponent::TypeId();
    const uint32_t tidInteractDescription = InteractDescription::TypeId();
    const uint32_t tidMapComponent = MapComponent::TypeId();
    const uint32_t tidStorageLocationComponent = StorageLocationComponent::TypeId();

    const uintptr_t marketEntity = *reinterpret_cast<uintptr_t*>(a + 0x10);
    void* marketComp = MW_FindComponentByTypeId(marketEntity, tidMarketComponent);

    uintptr_t marketCompIBP[2] = {0, 0};
    new (marketCompIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(marketComp));   // B.3.3 bridge
    void* descRaw = *reinterpret_cast<void**>(marketCompIBP[0] + 0x18);
    void* marketDesc = fn_IsDerivedFrom(descRaw, tidMarketDescription) ? descRaw : nullptr;

    // --- 0x1400f011c..0x1400f0132: the market's own StorageComponent ------
    uintptr_t marketStorageIBP[2] = {0, 0};
    s_fn_FindStorage(*reinterpret_cast<uintptr_t*>(a + 0x10), marketStorageIBP);

    // --- 0x1400f0132..0x1400f01c5: the citizen's terrain cell index -------
    // gameWorld+0xf8 is the terrain; its int32 at +0 is the map edge length.
    // citizen+0x278 is the CitizenAIComponent, whose +0x68 object carries the
    // world position at +0x158/+0x15c. Both axes are biased by dim/2, truncated
    // and clamped, then folded into a single cell index. Stage 1 computes it
    // because retail does, and because the value is load-bearing for stage 3/4
    // (it is the origin handed to the resource query and to FindPath).
    const uintptr_t terrain =
        *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*s_pGameWorldSingleton) + 0xf8);
    const uintptr_t citizen2 = *reinterpret_cast<uintptr_t*>(citizenHolder);
    const uintptr_t citizenAI = *reinterpret_cast<uintptr_t*>(citizen2 + 0x278);
    const uintptr_t pos = *reinterpret_cast<uintptr_t*>(citizenAI + 0x68);

    const int dim = *reinterpret_cast<int32_t*>(terrain);
    const float half = static_cast<float>(dim >> 1);
    int cellX = static_cast<int>(half + *reinterpret_cast<float*>(pos + 0x158));
    int cellY = static_cast<int>(half + *reinterpret_cast<float*>(pos + 0x15c));
    if (cellX < 0)    cellX = 0;
    if (cellX >= dim) cellX = dim - 1;
    if (cellY < 0)    cellY = 0;
    if (cellY >= dim) cellY = dim - 1;
    const int cellIdx = cellY * dim + cellX;
    (void)cellIdx;   // consumed by stage 3 (FUN_14008d590) and stage 4 (FUN_14012d5d0)

    // --- 0x1400f01c8..0x1400f01da: how much the citizen can still carry ---
    const int freeCarry = s_fn_FreeCarryCapacity(*reinterpret_cast<uintptr_t*>(citizenHolder));
    (void)freeCarry; // consumed by stage 3/4

    // --- 0x1400f01dd..0x1400f0219: market already full enough -------------
    // fill = QueryCount(marketStorage) / (float)marketStorage+0x38 (capacity).
    // COMISS/JNC: if fill >= MarketDescription+0x20 the market does not want any
    // more stock at all -> 1 = examined but declined.
    const float stocked = static_cast<float>(
        s_fn_StorageQueryCount(reinterpret_cast<void*>(marketStorageIBP[0])));
    const float capacity =
        static_cast<float>(*reinterpret_cast<int32_t*>(marketStorageIBP[0] + 0x38));
    const float fill = stocked / capacity;
    if (*reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(marketDesc) + 0x20) <= fill) {
        return 1;
    }

    // ======================================================================
    // STAGE 2 -- demand records + town supply/demand tree
    // retail 0x1400f021f .. 0x1400f096b
    // ======================================================================

    // --- 0x1400f021f..0x1400f0267: the demand record array ----------------
    // One 0x20-byte record per entry in MarketDescription's per-resource array
    // (+0x28 data, +0x30 count, element stride 8 = {mask @+0, float ratio @+4}).
    // afb0 grows the array; retail then seeds only record+0x00 from each
    // description entry's +0x00, because afb0's default-init leaves +0x00 alone.
    const uintptr_t descAddr = reinterpret_cast<uintptr_t>(marketDesc);
    MW_ArrayDesc records = {nullptr, 0, 0};
    s_fn_ArrayGrow(reinterpret_cast<int64_t*>(&records),
                   *reinterpret_cast<int32_t*>(descAddr + 0x30));
    const uintptr_t descEntries = *reinterpret_cast<uintptr_t*>(descAddr + 0x28);
    for (int i = 0; i < records.count; ++i) {
        reinterpret_cast<MW_DemandRecord*>(records.data)[i].resourceMask =
            *reinterpret_cast<uint32_t*>(descEntries + static_cast<int64_t>(i) * 8);
    }
    MW_DemandRecord* const rec = reinterpret_cast<MW_DemandRecord*>(records.data);

    // --- 0x1400f0267..0x1400f0406: one tree node per town resource type ---
    // gameWorld+0xf0 is the town object: entity array at +0x00 / int32 count at
    // +0x08, and the resource-type registry at +0x110 / int32 count at +0x118.
    // ⚠ Ghidra prints these as pplVar40[0x22] and [0x23] on a longlong** -- those
    // are ELEMENT subscripts, i.e. bytes 0x110 and 0x118. Byte offsets confirmed
    // in the disassembly (MOV RBX,[R13+0x110] / CMP dword [R13+0x118],EDI).
    const int marketEntryCount = *reinterpret_cast<int32_t*>(marketStorageIBP[0] + 0x30);
    const uintptr_t town =
        *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(*s_pGameWorldSingleton) + 0xf0);

    int64_t treeDesc[2] = {0, 0};   // {root @+0x00, int32 nodeCount @+0x08}
    {
        const int typeCount = *reinterpret_cast<int32_t*>(town + 0x118);
        for (int i = 0; i < typeCount; ++i) {
            const uintptr_t key = *reinterpret_cast<uintptr_t*>(
                *reinterpret_cast<uintptr_t*>(town + 0x110) + static_cast<int64_t>(i) * 8);

            uint8_t* root = *reinterpret_cast<uint8_t**>(&treeDesc[0]);
            if (root == nullptr) {
                // 0x1400f02c9: first node.
                uint8_t* n = static_cast<uint8_t*>(s_fn_MemAlloc(kNodeSize, 8));
                uint8_t* stored = nullptr;
                if (n != nullptr) {
                    *reinterpret_cast<uintptr_t*>(n + kNodeKey)     = key;
                    *reinterpret_cast<uint64_t*>(n + kNodeAccumA)   = 0;  // +0x08/+0x0c
                    *reinterpret_cast<uint64_t*>(n + kNodeAccumB)   = 0;  // +0x10/+0x14
                    *reinterpret_cast<uint64_t*>(n + kNodeParent)   = 0;
                    *reinterpret_cast<uint64_t*>(n + kNodeLeft)     = 0;
                    *reinterpret_cast<uint64_t*>(n + kNodeRight)    = 0;
                    *reinterpret_cast<uint32_t*>(n + kNodeBalance)  = 0;
                    stored = n;
                }
                *reinterpret_cast<uint8_t**>(&treeDesc[0]) = stored;
            } else {
                // 0x1400f030f: descend to the insertion point. A key that is
                // already present is a no-op AND skips the count increment
                // (retail jumps straight to 0x1400f03f2).
                uint8_t* cur = root;
                uint8_t* parent = cur;
                bool duplicate = false;
                bool goLeft = false;
                for (;;) {
                    parent = cur;
                    const uintptr_t k = *reinterpret_cast<uintptr_t*>(cur + kNodeKey);
                    if (key < k) {
                        uint8_t* l = *reinterpret_cast<uint8_t**>(cur + kNodeLeft);
                        if (l == nullptr) { goLeft = true; break; }
                        cur = l;
                    } else if (k < key) {
                        uint8_t* r = *reinterpret_cast<uint8_t**>(cur + kNodeRight);
                        if (r == nullptr) { goLeft = false; break; }
                        cur = r;
                    } else {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;

                uint8_t* n = static_cast<uint8_t*>(s_fn_MemAlloc(kNodeSize, 8));
                uint8_t* stored = nullptr;
                if (n != nullptr) {
                    *reinterpret_cast<uintptr_t*>(n + kNodeKey)    = key;
                    *reinterpret_cast<uint64_t*>(n + kNodeAccumA)  = 0;
                    *reinterpret_cast<uint64_t*>(n + kNodeAccumB)  = 0;
                    *reinterpret_cast<uint8_t**>(n + kNodeParent)  = parent;
                    *reinterpret_cast<uint64_t*>(n + kNodeLeft)    = 0;
                    *reinterpret_cast<uint64_t*>(n + kNodeRight)   = 0;
                    *reinterpret_cast<uint32_t*>(n + kNodeBalance) = 0;
                    stored = n;
                }
                *reinterpret_cast<uint8_t**>(parent + (goLeft ? kNodeLeft : kNodeRight)) = stored;

                // 0x1400f03ea: shared AVL retrace, cursor starts at the PARENT
                // (retail keeps the descent cursor in a stack slot and hands its
                // address over, so the retrace can walk and rewrite it).
                int64_t cursor = reinterpret_cast<int64_t>(parent);
                s_fn_AvlRetrace(treeDesc, &cursor);
            }
            // 0x1400f03ef
            *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(treeDesc) + 8) += 1;
        }
    }
    uint8_t* const treeRoot = *reinterpret_cast<uint8_t**>(&treeDesc[0]);

    // --- 0x1400f0406..0x1400f0525: LOOP A, what this market already holds --
    // StorageComponent entry layout: array @+0x28, int32 count @+0x30, stride
    // 0x10 = {resource type ptr @+0x00, total @+0x08, reserved @+0x0c}.
    // available = total - reserved; unitSize = RawMaterialDescription+0x48.
    // ⚠ The tree accumulator is available * (unitSize * available) -- QUADRATIC.
    // That is not a decompiler artefact: 0x1400f04df is IMUL R10D,R9D where R10D
    // is the available count and R9D is already unitSize*available. The record
    // accumulator on the very next lines gets the plain unitSize*available. The
    // asymmetry is retail's, and it is what makes a market that already stocks a
    // resource outrank the town's supply of it super-linearly.
    for (int i = 0; i < marketEntryCount; ++i) {
        const uintptr_t entries = *reinterpret_cast<uintptr_t*>(marketStorageIBP[0] + 0x28);
        const uintptr_t entry   = entries + static_cast<int64_t>(i) * 0x10;
        const uintptr_t resType = *reinterpret_cast<uintptr_t*>(entry + 0x00);

        void* rawMat = MW_FindChildByTypeId(resType, tidRawMaterialDescription);
        const int available = *reinterpret_cast<int32_t*>(entry + 0x08) -
                              *reinterpret_cast<int32_t*>(entry + 0x0c);
        const int32_t mass =
            *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x48) * available;

        uint8_t* node = MW_TreeFind(treeRoot, resType);
        if (!node) MW_ReportTreeMiss("loopA(market)", -1, marketStorageIBP[0], resType, rawMat, town);
        *reinterpret_cast<int32_t*>(node + kNodeAccumA) += available * mass;

        const uint32_t mask = *reinterpret_cast<uint32_t*>(
            reinterpret_cast<uintptr_t>(rawMat) + 0x5c);
        for (int r = 0; r < records.count; ++r) {
            if ((rec[r].resourceMask & mask) != 0) rec[r].inMarket += mass;
        }
    }

    // --- 0x1400f0525..0x1400f06c3: LOOP B, what haulers are already bringing
    // In-order walk of MarketWork's OWN BST at self+0x48. Each node's payload is
    // an array at node+0x08 with an int32 count at node+0x10, stride 0x10 =
    // {resource type ptr @+0x00, qty @+0x08}. Note the tree accumulator here
    // takes the RAW qty (+0x10) while the records take unitSize*qty -- checked at
    // 0x1400f0626 (ADD [RAX+0x10],R8D) versus 0x1400f0648 (ADD [RDX],R9D).
    {
        uint8_t* node = *reinterpret_cast<uint8_t**>(a + 0x48);
        if (node != nullptr) {
            while (*reinterpret_cast<uint8_t**>(node + kNodeLeft) != nullptr)
                node = *reinterpret_cast<uint8_t**>(node + kNodeLeft);
        }
        while (node != nullptr) {
            const int n = *reinterpret_cast<int32_t*>(node + 0x10);
            for (int i = 0; i < n; ++i) {
                const uintptr_t elem =
                    *reinterpret_cast<uintptr_t*>(node + 0x08) + static_cast<int64_t>(i) * 0x10;
                const uintptr_t resType = *reinterpret_cast<uintptr_t*>(elem + 0x00);
                const int32_t   qty     = *reinterpret_cast<int32_t*>(elem + 0x08);

                void* rawMat = MW_FindChildByTypeId(resType, tidRawMaterialDescription);
                const int32_t mass =
                    *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(rawMat) + 0x48) * qty;

                uint8_t* tn = MW_TreeFind(treeRoot, resType);
                if (!tn) MW_ReportTreeMiss("loopB(haulers)", -1, 0, resType, rawMat, town);
                *reinterpret_cast<int32_t*>(tn + kNodeAccumB) += qty;

                const uint32_t mask = *reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uintptr_t>(rawMat) + 0x5c);
                for (int r = 0; r < records.count; ++r) {
                    if ((rec[r].resourceMask & mask) != 0) rec[r].inTransit += mass;
                }
            }
            // in-order successor, MarketWork BST offsets (left +0x20, right
            // +0x28, parent +0x18) -- same shape as Slot9's walk.
            uint8_t* right = *reinterpret_cast<uint8_t**>(node + kNodeRight);
            if (right != nullptr) {
                node = right;
                while (*reinterpret_cast<uint8_t**>(node + kNodeLeft) != nullptr)
                    node = *reinterpret_cast<uint8_t**>(node + kNodeLeft);
            } else {
                uint8_t* parent = *reinterpret_cast<uint8_t**>(node + kNodeParent);
                if (parent != nullptr &&
                    *reinterpret_cast<uint8_t**>(parent + kNodeLeft) != node) {
                    while (parent != nullptr &&
                           *reinterpret_cast<uint8_t**>(parent + kNodeRight) == node) {
                        node = parent;
                        parent = *reinterpret_cast<uint8_t**>(parent + kNodeParent);
                    }
                }
                node = parent;
            }
        }
    }

    // --- 0x1400f06c3..0x1400f096b: LOOP C, what the town is holding -------
    // Every town entity that has a StorageComponent but NO ConsumeProduceComponent
    // (a producer that eats its own output is not a source) contributes its
    // available stock. Tree and records both take the plain unitSize*available
    // here (0x1400f0907 / 0x1400f0928).
    {
        const int entityCount = *reinterpret_cast<int32_t*>(town + 0x08);
        for (int e = 0; e < entityCount; ++e) {
            uintptr_t entityIBP = *reinterpret_cast<uintptr_t*>(
                *reinterpret_cast<uintptr_t*>(town + 0x00) + static_cast<int64_t>(e) * 8);
            void* storage = MW_FindComponentByTypeId(entityIBP, tidStorageComponent);
            uintptr_t storageIBP[2] = {0, 0};
            new (storageIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(storage));   // B.3.3 bridge

            entityIBP = *reinterpret_cast<uintptr_t*>(
                *reinterpret_cast<uintptr_t*>(town + 0x00) + static_cast<int64_t>(e) * 8);
            void* consumeProduce =
                MW_FindComponentByTypeId(entityIBP, tidConsumeProduceComponent);
            uintptr_t cpIBP[2] = {0, 0};
            new (cpIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(consumeProduce));   // B.3.3 bridge

            if (cpIBP[0] != 0) continue;   // 0x1400f0812: has a consumer -> skip

            const int n = *reinterpret_cast<int32_t*>(storageIBP[0] + 0x30);
            for (int i = 0; i < n; ++i) {
                const uintptr_t entries = *reinterpret_cast<uintptr_t*>(storageIBP[0] + 0x28);
                const uintptr_t entry   = entries + static_cast<int64_t>(i) * 0x10;
                const uintptr_t resType = *reinterpret_cast<uintptr_t*>(entry + 0x00);

                void* rawMat = MW_FindChildByTypeId(resType, tidRawMaterialDescription);
                const int available = *reinterpret_cast<int32_t*>(entry + 0x08) -
                                      *reinterpret_cast<int32_t*>(entry + 0x0c);
                const int32_t mass = available * *reinterpret_cast<int32_t*>(
                    reinterpret_cast<uintptr_t>(rawMat) + 0x48);

                uint8_t* tn = MW_TreeFind(treeRoot, resType);
                if (!tn) MW_ReportTreeMiss("loopC(town)", e, entityIBP, resType, rawMat, town);
                *reinterpret_cast<int32_t*>(tn + kNodeAccumC) += mass;

                const uint32_t mask = *reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uintptr_t>(rawMat) + 0x5c);
                for (int r = 0; r < records.count; ++r) {
                    if ((rec[r].resourceMask & mask) != 0) rec[r].inTown += mass;
                }
            }
        }
    }

    // ======================================================================
    // STAGE 3 -- ratio pass, surplus shed (the arm that returns 2), qsort
    // retail 0x1400f096b .. 0x1400f0e8a
    // ======================================================================

    // --- 0x1400f096b..0x1400f09eb: the ratio pass -------------------------
    // The market capacity float from stage 1 is still live in XMM10 across all
    // of stage 2 in retail; here it becomes the denominator.
    // 0x1400f096b MOVSS XMM8,[0x14017214c] -- that .rdata single is byte-verified
    // 1.0f (00 00 80 3F), so the project's usual `g_fUVScale ? *g_fUVScale : 1.0f`
    // fallback is exact here rather than a guess.
    const float kOne = g_fUVScale ? *g_fUVScale : 1.0f;
    const float invCapacity = kOne / capacity;            // 0x1400f0978 DIVSS XMM6,XMM10
    for (int r = 0; r < records.count; ++r) {
        const float want = *reinterpret_cast<float*>(descEntries + static_cast<int64_t>(r) * 8 + 4);
        // (inTransit + inMarket) as a SIGNED int32 before the float convert
        // (CVTDQ2PS at 0x1400f09a5), then scaled by 1/capacity.
        const float stocked =
            static_cast<float>(static_cast<int32_t>(rec[r].inTransit + rec[r].inMarket)) * invCapacity;
        rec[r].stockedFrac  = stocked;             // +0x10
        rec[r].satisfaction = stocked / want;      // +0x14 -- the b680 sort key
        rec[r].shortfall    = static_cast<int32_t>(capacity * want)
                            - static_cast<int32_t>(rec[r].inTransit)
                            - static_cast<int32_t>(rec[r].inMarket);   // +0x1c
    }

    // --- 0x1400f09eb..0x1400f0be0: shed a per-resource SURPLUS ------------
    // ⚠ CORRECTION to my own earlier stage map: this arm is NOT "pick up a loose
    // resource off the ground". It fires when the market holds MORE THAN TWICE
    // its target share of a resource, and it hands the excess to the citizen to
    // carry AWAY. 0x14008d590 with its char flag = 1 calls
    // StorageComponent_impl_cd80(storage, resType, count) at 0x14008da0e -- an
    // "will you ACCEPT n of this?" test -- so it returns the nearest OTHER entity
    // that will take the goods, excluding this market. Verified by decompiling
    // 0x14008d590 end to end.
    //
    // The two magic floats are exe .rdata singles, read at 0x140172174 and
    // 0x140172158: 2.0 (the trigger) and 1.25 (how much to keep).
    constexpr float kSurplusTrigger = 2.0f;    // DAT_140172174
    constexpr float kSurplusKeep    = 1.25f;   // DAT_140172158

    for (int r = 0; r < records.count; ++r) {
        const float want = *reinterpret_cast<float*>(descEntries + static_cast<int64_t>(r) * 8 + 4);
        if (!(want * kSurplusTrigger <
              static_cast<float>(static_cast<int32_t>(rec[r].inMarket)) * invCapacity))
            continue;

        // 0x1400f0a3x: how much of this resource the market keeps.
        const int keep = static_cast<int32_t>(want * kSurplusKeep * capacity);

        for (int i = 0; i < marketEntryCount; ++i) {
            const uintptr_t entries = *reinterpret_cast<uintptr_t*>(marketStorageIBP[0] + 0x28);
            const uintptr_t entry   = entries + static_cast<int64_t>(i) * 0x10;
            const uintptr_t resType = *reinterpret_cast<uintptr_t*>(entry + 0x00);

            void* rawMat = MW_FindChildByTypeId(resType, tidRawMaterialDescription);
            const uintptr_t rawMatAddr = reinterpret_cast<uintptr_t>(rawMat);
            if ((*reinterpret_cast<uint32_t*>(rawMatAddr + 0x5c) & rec[r].resourceMask) == 0)
                continue;

            // 0x1400f0b02 / 0x1400f0b0c: two signed IDIVs by the unit size.
            const int unitSize   = *reinterpret_cast<int32_t*>(rawMatAddr + 0x48);
            const int surplus    = (static_cast<int32_t>(rec[r].inMarket) - keep) / unitSize;
            const int entryAvail = *reinterpret_cast<int32_t*>(entry + 0x08) -
                                   *reinterpret_cast<int32_t*>(entry + 0x0c);
            int take = freeCarry / unitSize;
            if (entryAvail < take) take = entryAvail;   // CMOVL  0x1400f0b1e
            if (take > surplus)    take = surplus;      // CMOVG  0x1400f0b25
            if (take <= 0) continue;

            // 0x1400f0b2e: destPos is a 12-byte Vector3 out-param -- three dwords,
            // written by 0x14008d590 via param_7[0..2]. Deliberately a SEPARATE
            // local from the a9f0 payload descriptor below: retail reuses one
            // stack slot for both and re-zeroes 16 bytes before the a9f0 call, so
            // aliasing them here would leave 4 bytes of position data in the
            // descriptor's capacity field.
            float destPos[3] = {0.0f, 0.0f, 0.0f};
            uintptr_t excludeIBP = *reinterpret_cast<uintptr_t*>(a + 0x10);  // this market
            int64_t found = 0;
            s_fn_PickDropTarget(reinterpret_cast<int64_t**>(town), &found,
                                static_cast<uint32_t>(cellIdx), resType, take, '\x01',
                                reinterpret_cast<uint32_t*>(destPos), &excludeIBP);
            if (found == 0) continue;   // 0x1400f0b86: nobody will take it, keep scanning

            // ============ THE CLAIM (retail returns 2 from here) ============
            // 0x1400f0be3: a random interact point on the MARKET, weighted by the
            // InteractComponent's own +0x30 plus each child's recursive sum.
            const uintptr_t marketEntity2 = *reinterpret_cast<uintptr_t*>(a + 0x10);
            void* interact = MW_FindComponentByTypeId(marketEntity2, tidInteractComponent);
            uintptr_t interactIBP[2] = {0, 0};
            new (interactIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(interact));   // B.3.3 bridge
            const uintptr_t ic = interactIBP[0];
            void* icDescRaw = *reinterpret_cast<void**>(ic + 0x18);
            void* icDesc = fn_IsDerivedFrom(icDescRaw, tidInteractDescription) ? icDescRaw : nullptr;
            uint32_t weight = *reinterpret_cast<uint32_t*>(
                reinterpret_cast<uintptr_t>(icDesc) + 0x30);
            const int icChildren = *reinterpret_cast<int32_t*>(ic + 0x30);
            for (int k = 0; k < icChildren; ++k) {
                void** slot = reinterpret_cast<void**>(
                    *reinterpret_cast<uintptr_t*>(ic + 0x28) + static_cast<int64_t>(k) * 8);
                weight += s_fn_RecursiveChildSum(reinterpret_cast<int64_t>(*slot), 0);
            }
            void* rng = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(*s_pGameWorldSingleton) + 0xd8);
            const uint32_t roll = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(weight));   // B.3.4b bridge
            float marketPos[3] = {0.0f, 0.0f, 0.0f};
            s_fn_EntityResolvePos(reinterpret_cast<void*>(interactIBP[0]), marketPos, 0,
                                  static_cast<int>(roll), nullptr);

            // 0x1400f0d57..0x1400f0ddc: build the citizen's action chain.
            // Arg 8 is the resource TYPE ENTITY POINTER, full 64 bits -- see the
            // engine_funcs.h note; it was truncated to 32 once before and crashed.
            uintptr_t pathDest   = static_cast<uintptr_t>(found);          // where to deliver
            uintptr_t pathSrc    = *reinterpret_cast<uintptr_t*>(a + 0x10);// this market
            uintptr_t pathWalker = *reinterpret_cast<uintptr_t*>(citizenHolder);
            uintptr_t pathOut[2] = {0, 0};   // retail discards this handle on this arm
            const int pathType = *reinterpret_cast<int32_t*>(a + 0x20);
            s_fn_FindPath(pathOut, &pathWalker, pathType, &pathDest, destPos,
                          &pathSrc, marketPos, static_cast<int64_t>(resType), take,
                          0, 1, '\0', 0, 1);

            // 0x1400f0de1..0x1400f0e2a: stamp the citizen.
            const int pathTypeAfter = *reinterpret_cast<int32_t*>(a + 0x20);
            const uintptr_t citizen3 = *reinterpret_cast<uintptr_t*>(citizenHolder);
            int64_t* tgt = reinterpret_cast<int64_t*>(GetTarget(&pathSrc));
            const int64_t tgtVal = *tgt;               // 0x1400f0e03 MOV RAX,[RAX]
            *reinterpret_cast<int32_t*>(citizen3 + 0x30)   = pathTypeAfter;
            *reinterpret_cast<uint32_t*>(citizen3 + 0x54) |= 4;
            *reinterpret_cast<int32_t*>(citizen3 + 0xb0)   = 0;
            if (tgtVal == *reinterpret_cast<int64_t*>(citizen3 + 0x1f0) && pathTypeAfter >= 0) {
                *reinterpret_cast<uint32_t*>(citizen3 + 0x88) |= 0x20000;
            }

            // 0x1400f0e2a..0x1400f0e51: record the claim in MarketWork's OWN BST.
            // ⚠ The payload descriptor is ZEROED on this arm -- 0x1400f0e2a writes
            // two qwords of 0 into the slot before LEA R9. It is the S5 arm at
            // 0x1400f1bd4 that passes a real take list (&local_100). Passing a real
            // payload here would be wrong. The MemFree(nullptr) that follows at
            // 0x1400f0e51 is retail's deliberate no-op on that always-null pointer,
            // exactly as in the v0 save-migration path in Serialize above.
            uintptr_t vecDesc[2] = {0, 0};
            s_fn_MarketBST_InsertData(reinterpret_cast<void*>(a + 0x48), &pathSrc,
                                      citizenHolder, vecDesc);
            s_fn_MemFree(nullptr);
            vecDesc[0] = 0;
            vecDesc[1] = 0;


            MW_Stage2Teardown(treeDesc, &records);
            return 2;
        }
    }

    // --- 0x1400f0e6e..0x1400f0e82: sort the demand records ----------------
    // Ascending by the float at +0x14 (stocked fraction / want), so the LEAST
    // satisfied resource ends up first. Stage 4 consumes this order.
    if (records.count > 1) {
        s_fn_QsortDemand(records.data, records.count);
    }

    // ======================================================================
    // STAGE 4 -- pick the building to fetch from.  retail 0x1400f0e87..0x1400f13a3
    // ======================================================================
    // 0x1400f0e87/0x1400f0e97 also seed the epilogue's default return value of 1,
    // which is what this function yields when no candidate wins.
    int  result     = 1;      // uVar29 / R10D
    int  bestIndex  = -1;     // local_160, 0x1400f0e87 MOV dword [RBP-0x68],0xffffffff
    float bestScore = 0.0f;   // 0x1400f0e8e XORPS XMM6,XMM6
    const int townEntityCount = *reinterpret_cast<int32_t*>(town + 0x08);

    for (int e = 0; e < townEntityCount; ++e) {
        const uintptr_t townEntities = *reinterpret_cast<uintptr_t*>(town + 0x00);

        uintptr_t entIBP = *reinterpret_cast<uintptr_t*>(
            townEntities + static_cast<int64_t>(e) * 8);
        uintptr_t candStorage[2] = {0, 0};
        new (candStorage) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(entIBP, tidStorageComponent)));   // B.3.3 bridge

        entIBP = *reinterpret_cast<uintptr_t*>(townEntities + static_cast<int64_t>(e) * 8);
        uintptr_t candMap[2] = {0, 0};
        new (candMap) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(entIBP, tidMapComponent)));   // B.3.3 bridge

        // 0x1400f1013: the candidate's anchor cell index.
        uintptr_t anchorIBP = *reinterpret_cast<uintptr_t*>(
            townEntities + static_cast<int64_t>(e) * 8);
        const int candCell = s_fn_EntityAnchorCell(&anchorIBP, nullptr);

        entIBP = *reinterpret_cast<uintptr_t*>(townEntities + static_cast<int64_t>(e) * 8);
        uintptr_t candCP[2] = {0, 0};
        new (candCP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(entIBP, tidConsumeProduceComponent)));   // B.3.3 bridge

        // 0x1400f10bc: a building that consumes its own stock is not a source.
        // (Ghidra renders the other side of this branch as an empty
        // `else { LAB_1400f135b: }` -- there is no else body, it is the loop
        // increment falling through.)
        if (candCP[0] != 0) continue;

        // 0x1400f10ce: recompute the region ids at cell+0x26. ⚠ ONE argument --
        // RCX only. Ghidra prints a 4-arg prototype; the extra three are stale
        // registers, and the ported body reads only param_1. Passing zeros is
        // therefore equivalent to retail's garbage.
        s_fn_ZoneRefresh(reinterpret_cast<int*>(terrain), nullptr, nullptr, 0);

        // --- 0x1400f10d3..0x1400f1156: the passability / region test ---------
        // Cell records live at terrain+0x80, stride 0x30 (LEA r,[r+r*2]; SHL 4).
        const uintptr_t cells = *reinterpret_cast<uintptr_t*>(terrain + 0x80);
        const uintptr_t cellSrc = cells + static_cast<int64_t>(cellIdx)  * 0x30;
        const uintptr_t cellDst = cells + static_cast<int64_t>(candCell) * 0x30;
        const uint32_t bitDst = 1u << (*reinterpret_cast<uint8_t*>(cellDst + 0x29) & 0x1f);
        const uint32_t bitSrc = 1u << (*reinterpret_cast<uint8_t*>(cellSrc + 0x29) & 0x1f);
        const uint32_t mask   = (bitSrc | bitDst) | 0xe1fu;
        // ⚠ These two tests are PROVABLY DEAD in retail: `mask` is OR'd with both
        // bits before being TESTed against each of them, so TEST is always
        // non-zero and neither JZ is ever taken (0x1400f1119 / 0x1400f1121).
        // Kept because they are what the listing does and they cost nothing --
        // do not "clean them up", they are the evidence.
        if ((mask & bitSrc) == 0) continue;
        if ((mask & bitDst) == 0) continue;
        // 0x1400f112a: 0xe1f contributes no bits to 0x1e0, so this is non-zero
        // only when one of the two cells' own layer bit lands in 5..8. When it
        // does, the pair is accepted outright; otherwise both cells must carry a
        // non-zero region id at +0x26 and the two ids must match.
        if ((mask & 0x1e0u) == 0) {
            const uint16_t regionSrc = *reinterpret_cast<uint16_t*>(cellSrc + 0x26);
            if (regionSrc == 0) continue;
            const uint16_t regionDst = *reinterpret_cast<uint16_t*>(cellDst + 0x26);
            if (regionDst == 0) continue;
            if (regionSrc != regionDst) continue;
        }

        // --- 0x1400f1156..0x1400f127f: how much this candidate could supply ---
        // record+0x18 is scratch, zeroed per candidate then filled with the
        // candidate's own stock of each demand resource.
        const int candEntries = *reinterpret_cast<int32_t*>(candStorage[0] + 0x30);
        for (int r = 0; r < records.count; ++r) rec[r].stage4Scratch = 0;

        for (int i = 0; i < candEntries; ++i) {
            const uintptr_t centries = *reinterpret_cast<uintptr_t*>(candStorage[0] + 0x28);
            const uintptr_t centry   = centries + static_cast<int64_t>(i) * 0x10;
            void* crm = MW_FindChildByTypeId(*reinterpret_cast<uintptr_t*>(centry + 0x00),
                                             tidRawMaterialDescription);
            const uintptr_t crmAddr = reinterpret_cast<uintptr_t>(crm);
            const uint32_t cmass = static_cast<uint32_t>(
                (*reinterpret_cast<int32_t*>(centry + 0x08) -
                 *reinterpret_cast<int32_t*>(centry + 0x0c)) *
                *reinterpret_cast<int32_t*>(crmAddr + 0x48));
            const uint32_t cmask = *reinterpret_cast<uint32_t*>(crmAddr + 0x5c);
            for (int r = 0; r < records.count; ++r) {
                if ((rec[r].resourceMask & cmask) != 0) rec[r].stage4Scratch += cmass;
            }
        }

        // --- 0x1400f127f..0x1400f1355: distance-weighted score ----------------
        // f810 is the real path cost; a negative result means "no path known", and
        // retail falls back to map-edge + euclidean cell distance.
        int dist = s_fn_IsDstReachable(reinterpret_cast<int*>(terrain),
                                       static_cast<uint32_t>(candCell),
                                       static_cast<uint32_t>(cellIdx), '\x01');
        if (dist < 0) {
            const int dim2 = *reinterpret_cast<int32_t*>(terrain);
            const int dx = (candCell % dim2) - (cellIdx % dim2);
            const int dy = (candCell / dim2) - (cellIdx / dim2);
            dist = static_cast<int>(s_fn_Sqrtf(static_cast<float>(dy * dy + dx * dx))) + dim2;
        }
        float distWeight = kOne;
        if (dist > 0) distWeight = kOne / static_cast<float>(dist);

        // Rank weight 1/(r+1) over the b680-SORTED record order: the least
        // satisfied resource is index 0 and counts fullest. This is the only
        // consumer of that sort, so a wrong b680 shows up here as a wrong winner.
        float value = 0.0f;
        for (int r = 0; r < records.count; ++r) {
            uint32_t amount = static_cast<uint32_t>(freeCarry);
            if (static_cast<int32_t>(rec[r].stage4Scratch) < freeCarry)
                amount = rec[r].stage4Scratch;
            value += (kOne - rec[r].satisfaction) *
                     static_cast<float>(static_cast<int32_t>(amount)) *
                     (kOne / (static_cast<float>(r) + kOne));
        }
        if (bestScore < value * distWeight) {
            bestIndex = e;
            bestScore = value * distWeight;
        }
    }

    // ======================================================================
    // STAGE 5 -- take list, path, transfer, claim.  retail 0x1400f13a3..0x1400f1c15
    // ======================================================================
    if (bestIndex >= 0) {   // 0x1400f0e8a-era check: local_160 < 0 -> straight to epilogue
        const uintptr_t winner = *reinterpret_cast<uintptr_t*>(
            *reinterpret_cast<uintptr_t*>(town + 0x00) + static_cast<int64_t>(bestIndex) * 8);
        uintptr_t winnerIBP = winner;
        uintptr_t winStorage[2] = {0, 0};
        new (winStorage) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(winnerIBP, tidStorageComponent)));   // B.3.3 bridge

        // --- 0x1400f1425..0x1400f16a0: build the take list ------------------
        // 0x18-byte records: +0x00 resource type, +0x08 rank (the ba70 sort key,
        // SIGNED int32), +0x0c available, +0x10 demand-record index.
        MW_ArrayDesc takeList = {nullptr, 0, 0};
        const int winEntries = *reinterpret_cast<int32_t*>(winStorage[0] + 0x30);

        for (int i = 0; i < winEntries; ++i) {
            const uintptr_t wentries = *reinterpret_cast<uintptr_t*>(winStorage[0] + 0x28);
            const uintptr_t wentry   = wentries + static_cast<int64_t>(i) * 0x10;
            const uintptr_t wresType = *reinterpret_cast<uintptr_t*>(wentry + 0x00);
            void* wrm = MW_FindChildByTypeId(wresType, tidRawMaterialDescription);
            const uint32_t wmask =
                *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(wrm) + 0x5c);

            // First demand record this resource satisfies AND that the market is
            // not already double-stocked on. ⚠ RETAIL QUIRK, ported as-is: the
            // `want` here is read from MarketDescription entry [r] using the
            // record's CURRENT index, but the records were reordered by b680 at
            // 0x1400f0e82. So after the sort this pairs record[r] with the
            // description entry that was at position r before sorting. That is
            // what the listing does (the desc offset advances by 8 in lockstep
            // with the record pointer advancing by 0x20); it is not a port bug.
            int matched = -1;
            for (int r = 0; r < records.count; ++r) {
                if ((rec[r].resourceMask & wmask) == 0) continue;
                const float lim =
                    *reinterpret_cast<float*>(descEntries + static_cast<int64_t>(r) * 8 + 4)
                    * kSurplusTrigger;
                if (rec[r].stockedFrac < lim) matched = r;
                break;   // 0x1400f1660: first mask hit ends the scan either way
            }
            if (matched < 0) continue;

            uint8_t* tn = MW_TreeFind(treeRoot, wresType);
            // 0x1400f159f/0x1400f15a2: node+0x10 (in-transit qty) + node+0x08
            // (the quadratic market-weighted term from loop A).
            const int32_t rank = static_cast<int32_t>(
                *reinterpret_cast<uint32_t*>(tn + kNodeAccumB) +
                *reinterpret_cast<uint32_t*>(tn + kNodeAccumA));
            const int32_t avail = *reinterpret_cast<int32_t*>(wentry + 0x08) -
                                  *reinterpret_cast<int32_t*>(wentry + 0x0c);

            const int32_t oldCount = takeList.count;
            const int32_t newCount = oldCount + 1;
            uint8_t* oldData = takeList.data;
            takeList.count = newCount;
            if (takeList.capacity < newCount) {
                int32_t cap = newCount;
                if (oldCount != 0) cap = newCount * 3;
                takeList.capacity = cap;
                takeList.data = static_cast<uint8_t*>(
                    s_fn_MemAlloc(static_cast<int64_t>(cap) * 0x18, 8));
            }
            int32_t start = (oldData == takeList.data) ? oldCount : 0;
            for (int32_t k = start; k < newCount; ++k) {
                uint8_t* dst = takeList.data + static_cast<int64_t>(k) * 0x18;
                if (k < oldCount) {
                    const uint8_t* src = oldData + static_cast<int64_t>(k) * 0x18;
                    reinterpret_cast<uint64_t*>(dst)[0] = reinterpret_cast<const uint64_t*>(src)[0];
                    reinterpret_cast<uint64_t*>(dst)[1] = reinterpret_cast<const uint64_t*>(src)[1];
                    reinterpret_cast<uint64_t*>(dst)[2] = reinterpret_cast<const uint64_t*>(src)[2];
                } else {
                    *reinterpret_cast<uintptr_t*>(dst + 0x00) = wresType;
                    *reinterpret_cast<int32_t*>  (dst + 0x08) = rank;
                    *reinterpret_cast<int32_t*>  (dst + 0x0c) = avail;
                    *reinterpret_cast<int32_t*>  (dst + 0x10) = matched;
                    *reinterpret_cast<int32_t*>  (dst + 0x14) = 0;
                }
            }
            if (oldData != takeList.data) s_fn_MemFree(oldData);
        }

        // 0x1400f16a0: ascending by the signed int32 at +0x08.
        if (takeList.count > 1) s_fn_QsortTake(takeList.data, takeList.count);

        // --- 0x1400f16a5..0x1400f18c0: two interact points -------------------
        const uintptr_t mktEntity = *reinterpret_cast<uintptr_t*>(a + 0x10);
        uintptr_t mktInteract[2] = {0, 0};
        new (mktInteract) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(mktEntity, tidInteractComponent)));   // B.3.3 bridge
        const uintptr_t mic = mktInteract[0];
        void* micDescRaw = *reinterpret_cast<void**>(mic + 0x18);
        void* micDesc = fn_IsDerivedFrom(micDescRaw, tidInteractDescription) ? micDescRaw : nullptr;
        uint32_t weight =
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(micDesc) + 0x30);
        const int micChildren = *reinterpret_cast<int32_t*>(mic + 0x30);
        for (int k = 0; k < micChildren; ++k) {
            void** slot = reinterpret_cast<void**>(
                *reinterpret_cast<uintptr_t*>(mic + 0x28) + static_cast<int64_t>(k) * 8);
            weight += s_fn_RecursiveChildSum(reinterpret_cast<int64_t>(*slot), 0);
        }
        void* rng2 = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(*s_pGameWorldSingleton) + 0xd8);
        const uint32_t roll2 = reinterpret_cast<System::Random*>(rng2)->Rand(static_cast<uint32_t>(weight));   // B.3.4b bridge
        float marketPos2[3] = {0.0f, 0.0f, 0.0f};
        s_fn_EntityResolvePos(reinterpret_cast<void*>(mktInteract[0]), marketPos2, 0,
                              static_cast<int>(roll2), nullptr);

        float winnerPos[3] = {0.0f, 0.0f, 0.0f};
        uintptr_t winnerIBP2 = winner;
        uintptr_t winLoc[2] = {0, 0};
        new (winLoc) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(MW_FindComponentByTypeId(winnerIBP2, tidStorageLocationComponent)));   // B.3.3 bridge
        s_fn_FindBestAdjacentCell(static_cast<int64_t>(winLoc[0]),
                                  static_cast<uint32_t>(cellIdx), winnerPos,
                                  static_cast<int64_t>(roll2));

        // --- 0x1400f18c0..0x1400f1a80: how much of each to actually carry ----
        // 0x10-byte records {resource type, count}. This IS the payload a9f0
        // stores on the claim node, unlike the S3 arm's zeroed descriptor.
        MW_ArrayDesc carry = {nullptr, 0, 0};
        int carryRemaining = freeCarry;
        for (int t = 0; t < takeList.count; ++t) {
            const uint8_t* te = takeList.data + static_cast<int64_t>(t) * 0x18;
            const uintptr_t tType = *reinterpret_cast<const uintptr_t*>(te + 0x00);
            const int32_t   tAvail = *reinterpret_cast<const int32_t*>(te + 0x0c);
            const int32_t   tRec   = *reinterpret_cast<const int32_t*>(te + 0x10);

            void* trm = MW_FindChildByTypeId(tType, tidRawMaterialDescription);
            const int unit = *reinterpret_cast<int32_t*>(
                reinterpret_cast<uintptr_t>(trm) + 0x48);
            int take = carryRemaining / unit;
            if (tAvail < take) take = tAvail;
            const int byDemand = rec[tRec].shortfall / unit;
            if (byDemand < take) take = byDemand;
            if (take <= 0) continue;

            const int32_t oldCount = carry.count;
            const int32_t newCount = oldCount + 1;
            uint8_t* oldData = carry.data;
            carry.count = newCount;
            if (carry.capacity < newCount) {
                int32_t cap = newCount;
                if (oldCount != 0) cap = newCount * 3;
                carry.capacity = cap;
                carry.data = static_cast<uint8_t*>(
                    s_fn_MemAlloc(static_cast<int64_t>(cap) * 0x10, 8));
            }
            int32_t start = (oldData == carry.data) ? oldCount : 0;
            for (int32_t k = start; k < newCount; ++k) {
                uint8_t* dst = carry.data + static_cast<int64_t>(k) * 0x10;
                if (dst == nullptr) continue;   // retail's own per-element guard
                if (k < oldCount) {
                    const uint8_t* src = oldData + static_cast<int64_t>(k) * 0x10;
                    reinterpret_cast<uint64_t*>(dst)[0] = reinterpret_cast<const uint64_t*>(src)[0];
                    reinterpret_cast<uint64_t*>(dst)[1] = reinterpret_cast<const uint64_t*>(src)[1];
                } else {
                    *reinterpret_cast<uintptr_t*>(dst + 0x00) = tType;
                    *reinterpret_cast<int32_t*>  (dst + 0x08) = take;
                    *reinterpret_cast<int32_t*>  (dst + 0x0c) = 0;
                }
            }
            if (oldData != carry.data) s_fn_MemFree(oldData);

            // 0x1400f1a5x: consume both budgets in MASS, not in units.
            const int mass = take * unit;
            carryRemaining -= mass;
            rec[tRec].shortfall -= mass;
        }

        // --- 0x1400f1a80..0x1400f1bd9: the claim ------------------------------
        if (carry.count != 0) {
            uintptr_t pathWinner[2], pathMarket[2], pathCitizen[2];
            uintptr_t winnerRef = winner;
            uintptr_t marketRef = *reinterpret_cast<uintptr_t*>(a + 0x10);
            MW_MakeStrongRef(pathWinner,  &winnerRef);
            MW_MakeStrongRef(pathMarket,  &marketRef);
            MW_MakeStrongRef(pathCitizen, reinterpret_cast<uintptr_t*>(citizenHolder));

            // Note the direction: SRC is the winner (pick up there), DEST is the
            // market (drop off here) -- the mirror of the S3 shed arm. Args 8/9 are
            // 0 and -1 here, not a material template and a count.
            uintptr_t pathOut2[2] = {0, 0};
            const int pathType2 = *reinterpret_cast<int32_t*>(a + 0x20);
            s_fn_FindPath(pathOut2, pathCitizen, pathType2, pathMarket, marketPos2,
                          pathWinner, winnerPos, 0, -1, 0, 1, '\0', 0, 1);

            const uintptr_t citizen5 = *reinterpret_cast<uintptr_t*>(citizenHolder);
            int64_t* tgt2 = reinterpret_cast<int64_t*>(GetTarget(pathWinner));
            const int64_t tgtVal2 = *tgt2;
            *reinterpret_cast<int32_t*>(citizen5 + 0x30)   = pathType2;
            *reinterpret_cast<uint32_t*>(citizen5 + 0x54) |= 4;
            *reinterpret_cast<int32_t*>(citizen5 + 0xb0)   = 0;
            if (tgtVal2 == *reinterpret_cast<int64_t*>(citizen5 + 0x1f0) && pathType2 >= 0) {
                *reinterpret_cast<uint32_t*>(citizen5 + 0x88) |= 0x20000;
            }

            for (int k = 0; k < carry.count; ++k) {
                const uint8_t* ce = carry.data + static_cast<int64_t>(k) * 0x10;
                s_fn_TransferToCarrier(static_cast<int64_t>(pathOut2[0]),
                                       *reinterpret_cast<const int64_t*>(ce + 0x00),
                                       *reinterpret_cast<const int32_t*>(ce + 0x08));
            }

            s_fn_MarketBST_InsertData(reinterpret_cast<void*>(a + 0x48), pathWinner,
                                      citizenHolder, &carry);
            result = 2;

        }

        // 0x1400f1be8/0x1400f1bfe: both S5 arrays are freed here, before the
        // shared epilogue. The carry array is freed even on the claiming path --
        // a9f0 copies the payload into the node, it does not take ownership.
        s_fn_MemFree(carry.data);
        s_fn_MemFree(takeList.data);
    }


    MW_Stage2Teardown(treeDesc, &records);
    return result;
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
// MarketWork dtor (inlined in retail slot 0 0x14002a910). The stamps and the `~Internal` call are the compiler's chain.
MarketWork::~MarketWork() {
    uintptr_t selfAddr = (uintptr_t)this;

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

}


// ============================================================
// MarketWork::ScalarDtor (0x14002a910)
// ============================================================
// [0] 0x14002a910: ~MarketWork, then MemFree on &1 (the compiler chains the base dtors).
Object::Base* MarketWork::DeletingDtor(unsigned flags) {
    this->~MarketWork();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}


// ============================================================
// MarketWork::Slot11 -- returns (this->_field50 > 0)
// ============================================================
// 9 bytes, too small to hook.
int MarketWork::GetPriority() {
    return *(int*)((uintptr_t)this + 0x50) > 0 ? 1 : 0;
}



// Ctor 0x1400f1f70: `??0Internal`, the Work base fields, the leaf fields zeroed (the
// two stamps are the compiler's). MarketWork_ResolveFuncs wires the file's helper
// pointers (the old factory did the same).
MarketWork::MarketWork() : Work(), _field48(nullptr), _field50(0) {
    MarketWork_ResolveFuncs();
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_MarketWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id), MarketWork_Ctor_Impl (the ctor with both stamps written by hand) and
// REGISTER_TYPE over MarketWork_Factory / _CreateInPlace, plus two vtable-pointer
// statics for the dtor's stamps.

// create 0x14002a8d0: MemAlloc(sizeof, 8), then the ctor.
void* MarketWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MarketWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MarketWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* MarketWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MarketWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(MarketWork, "Work")
