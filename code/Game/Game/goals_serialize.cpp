// code/Game/Game/goals_serialize.cpp
//
// Port of exe 0x1400f9240 (Ghidra: TradeComponent_helper_9240) — the manager object at
// GameUpdateLoop+0x118 serializer, called from the master GameUpdateLoop serialize
// (0x140095a90) during SAVE/LOAD. Despite the Ghidra "Trade*" auto-naming (an artifact of
// decompiler similarity heuristics, not a real namespace — see reference_misnamed_component_finders),
// the tree this function walks at self+0x30 is the SAME work-slot tree WorkComponent::CheckWorkSlot
// already documents at *(g_pGameWorldSingleton+0x118)+0x30 (code/Game/Component/workcomponent.cpp).
// self == *(g_pGameWorldSingleton+0x118) is the manager itself.
//
// Self layout (byte offsets, derived from the decompile — no separate type header; every
// field is walked via raw pointer arithmetic exactly as Ghidra shows, matching the codebase's
// established style for this kind of BST-heavy internal manager, e.g. workcomponent.cpp /
// action_base_complete.cpp):
//   +0x00        int      field0                        (serialized directly)
//   +0x04        int      field1                        (serialized directly)
//   +0x08        uint     field2                        (serialized directly)
//   +0x10        Layout-A tree root  "workCountTree"     (scratch bookkeeping, never itself
//                                                          serialized — built/consumed during LOAD)
//   +0x18        int      "insert counter" for workCountTree (also used as scratch key source)
//   +0x30        Layout-B tree root  "idTree" — slot[0] of the 15-slot description-tree array
//                passed to TradeDescription_impl_d060 as (self+0x30, count). This is the SAME
//                tree WorkComponent::CheckWorkSlot walks (*(g_pGameWorldSingleton+0x118)+0x30).
//   +0x40..+0x11F  14 more Layout-B tree headers (root@0,count@8, stride 0x10) — d060 slots[1..14].
//                Only touched by the id-removal pass (erase-by-key via ca60 across all 14).
//   +0x120       Layout-C (0x38-node) tree root passed to TradeWork_impl_d5f0.
//
// Node layouts (see code/Game/Action/action_pf_avl.cpp for the shared rotation family):
//   Layout A (0x28 node): value@0x00(8B, low32=sort key), parent@0x08, left@0x10, right@0x18,
//                          balance/height@0x20. Rebalance: 0x14001a8c0.
//   Layout B (0x30 node): key@0x00(4B), payload/IBP@0x08(8B), parent@0x10, left@0x18, right@0x20,
//                          balance@0x28, height@0x2a. Rebalance: 0x140013480.
//   Layout C (0x38 node): key@0x00(8B), subtree(Layout-B)@0x08, int@0x10, parent@0x18, left@0x20,
//                          right@0x28, balance/height@0x30. Rebalance: 0x140014b40 (already
//                          ported as TradeWork_impl_4b40 — code/Game/Component/keystone_a_restree.cpp).
//
// Sub-callees:
//   0x1400f8270  TradeComponent_helper_8270   — NEW (this file) — SAVE-only vtbl+0x48 walk over
//                the idTree (self+0x30).
//   0x14002d060  TradeDescription_impl_d060   — NEW (this file) — 14/15-slot Layout-B array
//                serializer. Calls 0x140013480 (rebalance, ALREADY ported/registered —
//                Pf_AvlRebalanceB_13480, code/Game/Action/action_pf_avl.cpp).
//   0x14002d5f0  TradeWork_impl_d5f0          — NEW (this file) — Layout-C tree serializer.
//   0x14002d340  TradeWork_impl_d5f0_sub_d340 — NEW (this file) — single Layout-B tree
//                serializer (same shape as one d060 slot body); called from d5f0.
//   0x14002ccf0  TradeWork_impl_ccf0          — NEW (this file) — Layout-C BST insert.
//   0x14002ceb0  TradeWork_impl_ccf0_sub_ceb0 — NEW (this file) — Layout-C node copy-ctor.
//   0x14002cfb0  FUN_14002cfb0                — NEW (this file) — Layout-B subtree deep-copy
//                (recursive), used by ceb0.
//   0x140018020  FUN_140018020 (ExternalRef serialize) — ALREADY ported/registered
//                (Reimpl_ExternalPtr_Serialize_StringTable, standalone/address_registry.cpp).
//   0x14001a8c0  Layout-A rebalance — ALREADY ported (Pf_AvlRebalanceA_a8c0).
//   0x14001ca60  generic BST erase  — ALREADY ported (WorkCellBST_Remove_ca60).
//   0x14001a6e0  Layout-A BST erase — ALREADY ported (WorkBST_BSTRemove_a6e0).
//   0x140014c30  Layout-A post-order free — ALREADY ported (WorkBase_TreeNodeFree_14c30).
//   0x14000e320  Layout-B post-order free — ALREADY ported (WorkBase_TreeNodeFree_e320).
//
// DWORD_1401c6cf8 ("TypeInfo_Work" per Ghidra) — a NEW type-id cell (distinct from the
// already-registered g_TypeId_WorkComponent @ 0x1401c8070). Registered here as a zero-filled
// storage cell (same pattern as standalone/reg_economy_typeids.cpp); it still needs a
// GetTypeIdByName("Work") fill wired into FillEconomyTypeIdGlobals() (code/Game/Game/game_main.cpp)
// for the load-time IsDerivedFrom filter in d060/d340 to pass real objects through instead of
// nulling everything — flagged in the port report, not done here (out of this file's scope;
// avoids touching game_main.cpp / address_registry.cpp per task instructions).
//
// Byte-faithful port — every branch, walk, and rebuild step mirrors the decompile. In-order BST
// walks (Ghidra's coalesced goto-successor idiom) are rewritten as clean iterator loops via the
// BstLeftmost*/BstNext* helpers below; this is a structural cleanup only (the two forms are
// provably equivalent — the classic parent-pointer in-order successor algorithm), not a logic change.

#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <new>


namespace {

// ============================================================
// Resolved sub-callees (already ported elsewhere; resolved lazily here)
// ============================================================

typedef void (*fn_RebalanceA_t)(int64_t* root, int64_t* node);          // 0x14001a8c0
typedef void (*fn_RebalanceB_t)(int64_t* root, int64_t* node);          // 0x140013480
typedef void (*fn_RebalanceC_t)(int64_t* treeDesc, int64_t* cursor);    // 0x140014b40
typedef void (*fn_BstErase_t)(void* treeOwner, void* node);             // 0x14001ca60 / 0x14001a6e0
typedef void (*fn_TreeFree_t)(void* root, void* node);                  // 0x140014c30 / 0x14000e320
typedef void (*fn_ExternalRefSerialize_t)(void* refField, void* serializer); // 0x140018020

static fn_RebalanceA_t s_rebA        = nullptr; // 0x14001a8c0
static fn_RebalanceB_t s_rebB        = nullptr; // 0x140013480
static fn_RebalanceC_t s_rebC        = nullptr; // 0x140014b40
static fn_BstErase_t   s_eraseB      = nullptr; // 0x14001ca60 (Layout B / C-subtree erase)
static fn_BstErase_t   s_eraseA      = nullptr; // 0x14001a6e0 (Layout A erase)
static fn_TreeFree_t   s_freeA       = nullptr; // 0x140014c30 (Layout A post-order free)
static fn_TreeFree_t   s_freeB       = nullptr; // 0x14000e320 (Layout B post-order free)
static fn_ExternalRefSerialize_t s_externalRefSerialize = nullptr; // 0x140018020

static uint32_t g_tid_Work = 0; // 0x1401c6cf8 — NEEDS GetTypeIdByName("Work") fill (see header note)

static bool s_depsInit = false;
static void EnsureDeps() {
    if (s_depsInit) return;
    // Fill the 0x1401c6cf8 type-id cell from the factory (retail fills it at
    // Work_RegisterType static-init; a zero cell would make the load-time
    // IsDerivedFrom filter null every payload — "registered but filled with
    // ZERO" trap).
    if (g_tid_Work == 0) g_tid_Work = ::Object::GetTypeIdByName("Work");
    s_rebA   = reinterpret_cast<fn_RebalanceA_t>(ResolveAddr(Module::Exe, 0x14001a8c0));
    s_rebB   = reinterpret_cast<fn_RebalanceB_t>(ResolveAddr(Module::Exe, 0x140013480));
    s_rebC   = reinterpret_cast<fn_RebalanceC_t>(ResolveAddr(Module::Exe, 0x140014b40));
    s_eraseB = reinterpret_cast<fn_BstErase_t>(ResolveAddr(Module::Exe, 0x14001ca60));
    s_eraseA = reinterpret_cast<fn_BstErase_t>(ResolveAddr(Module::Exe, 0x14001a6e0));
    s_freeA  = reinterpret_cast<fn_TreeFree_t>(ResolveAddr(Module::Exe, 0x140014c30));
    s_freeB  = reinterpret_cast<fn_TreeFree_t>(ResolveAddr(Module::Exe, 0x14000e320));
    s_externalRefSerialize = reinterpret_cast<fn_ExternalRefSerialize_t>(ResolveAddr(Module::Exe, 0x140018020));
    s_depsInit = true;
}

// ============================================================
// Generic in-order BST walk helpers (parent-pointer successor algorithm).
// Layout B: parent@0x10, left@0x18, right@0x20.  Layout A: parent@0x08, left@0x10, right@0x18.
// ============================================================

static uint8_t* BstLeftmostB(uint8_t* node) {
    if (!node) return nullptr;
    while (*reinterpret_cast<uint8_t**>(node + 0x18)) node = *reinterpret_cast<uint8_t**>(node + 0x18);
    return node;
}
static uint8_t* BstNextB(uint8_t* node) {
    uint8_t* right = *reinterpret_cast<uint8_t**>(node + 0x20);
    if (right) return BstLeftmostB(right);
    uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x10);
    uint8_t* cur = node;
    while (parent && *reinterpret_cast<uint8_t**>(parent + 0x20) == cur) {
        cur = parent;
        parent = *reinterpret_cast<uint8_t**>(parent + 0x10);
    }
    return parent;
}

static uint8_t* BstLeftmostA(uint8_t* node) {
    if (!node) return nullptr;
    while (*reinterpret_cast<uint8_t**>(node + 0x10)) node = *reinterpret_cast<uint8_t**>(node + 0x10);
    return node;
}
static uint8_t* BstNextA(uint8_t* node) {
    uint8_t* right = *reinterpret_cast<uint8_t**>(node + 0x18);
    if (right) return BstLeftmostA(right);
    uint8_t* parent = *reinterpret_cast<uint8_t**>(node + 0x08);
    uint8_t* cur = node;
    while (parent && *reinterpret_cast<uint8_t**>(parent + 0x18) == cur) {
        cur = parent;
        parent = *reinterpret_cast<uint8_t**>(parent + 0x08);
    }
    return parent;
}

// ============================================================
// Layout-A insert (0x28 node): value@0x00 (8B, low32 = sort key), parent@0x08, left@0x10,
// right@0x18, balance/height@0x20. *treeRoot is the tree's root-pointer field.
// Returns the (found-or-inserted) node; *outInserted reports whether a NEW node was made
// (retail's per-slot counter increments only on genuine insert, not on found-existing).
// ============================================================
static int32_t* LayoutA_Insert(int32_t** treeRoot, uint64_t value, bool* outInserted) {
    EnsureDeps();
    *outInserted = false;
    int32_t key = static_cast<int32_t>(value & 0xFFFFFFFFu);
    int32_t* root = *treeRoot;
    if (root == nullptr) {
        int32_t* node = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8));
        if (node) {
            *reinterpret_cast<uint64_t*>(node) = value;
            node[2] = 0; node[3] = 0; node[4] = 0; node[5] = 0; node[6] = 0; node[7] = 0; node[8] = 0;
        }
        *treeRoot = node;
        *outInserted = true;
        return node;
    }
    int32_t* parent = root;
    int32_t* node = nullptr;
    for (;;) {
        if (*parent <= key) {
            if (key <= *parent) {
                return parent; // found existing — no insert
            }
            int32_t* right = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18);
            if (right == nullptr) {
                node = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8));
                if (node) {
                    *reinterpret_cast<uint64_t*>(node) = value;
                    *reinterpret_cast<int32_t**>(node + 2) = parent;
                    node[4] = 0; node[5] = 0; node[6] = 0; node[7] = 0; node[8] = 0;
                }
                *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18) = node;
                break;
            }
            parent = right;
        } else {
            int32_t* left = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x10);
            if (left == nullptr) {
                node = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8));
                if (node) {
                    *reinterpret_cast<uint64_t*>(node) = value;
                    *reinterpret_cast<int32_t**>(node + 2) = parent;
                    node[4] = 0; node[5] = 0; node[6] = 0; node[7] = 0; node[8] = 0;
                }
                *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x10) = node;
                break;
            }
            parent = left;
        }
    }
    int64_t cursor = reinterpret_cast<int64_t>(parent);
    if (s_rebA) s_rebA(reinterpret_cast<int64_t*>(treeRoot), &cursor);
    *outInserted = true;
    return node;
}

// Layout-A BST search by int32 key. left@0x10, right@0x18.
static int32_t* LayoutA_Find(int32_t* root, int32_t key) {
    int32_t* node = root;
    while (node != nullptr) {
        if (key < *node) {
            node = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(node) + 0x10);
        } else if (*node < key) {
            node = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(node) + 0x18);
        } else {
            return node;
        }
    }
    return nullptr;
}

// Layout-B BST search by int32 key. left@0x18, right@0x20.
static int32_t* LayoutB_Find(int32_t* root, int32_t key) {
    int32_t* node = root;
    while (node != nullptr) {
        if (key < *node) {
            node = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(node) + 0x18);
        } else if (*node < key) {
            node = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(node) + 0x20);
        } else {
            return node;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================
// 0x14002cfb0 — recursive deep-copy of a Layout-B (0x30 node) subtree.
// param_1 is unused by retail beyond being forwarded (allocator context placeholder).
// ============================================================
extern "C" int32_t* Goals_LayoutB_DeepCopy_cfb0(uint64_t param_1, int32_t* src) {
    EnsureDeps();
    int32_t* dst = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
    if (dst != nullptr) {
        dst[0] = src[0];
        *reinterpret_cast<uint64_t*>(dst + 2) = *reinterpret_cast<uint64_t*>(src + 2); // payload/IBP @0x08
        *reinterpret_cast<uint64_t*>(dst + 4) = 0; // parent
        *reinterpret_cast<uint64_t*>(dst + 6) = 0; // left
        *reinterpret_cast<uint64_t*>(dst + 8) = 0; // right
        dst[10] = 0; // balance+height (4B @ 0x28)
    }
    // Faithful: retail derefs `dst` unconditionally here even on OOM (dst==null) — no guard added.
    *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(dst) + 0x2a) =
        *reinterpret_cast<int16_t*>(reinterpret_cast<uint8_t*>(src) + 0x2a);
    *reinterpret_cast<int16_t*>(dst + 10) = *reinterpret_cast<int16_t*>(src + 10);

    if (*reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(src) + 0x18) != nullptr) {
        int32_t* leftSrc = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(src) + 0x18);
        int32_t* leftCopy = Goals_LayoutB_DeepCopy_cfb0(param_1, leftSrc);
        *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(dst) + 0x18) = leftCopy;
        *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(leftCopy) + 0x10) = dst;
    }
    if (*reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(src) + 0x20) != nullptr) {
        int32_t* rightSrc = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(src) + 0x20);
        int32_t* rightCopy = Goals_LayoutB_DeepCopy_cfb0(param_1, rightSrc);
        *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(dst) + 0x20) = rightCopy;
        *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(rightCopy) + 0x10) = dst;
    }
    return dst;
}

// ============================================================
// 0x14002ceb0 — Layout-C (0x38 node) copy-ctor. dst = fresh 0x38 buffer, src = {key@0,
// subtreeRoot@8, int@0x10}, parent = the new node's parent pointer.
// ============================================================
extern "C" uint8_t* Goals_LayoutC_CopyCtor_ceb0(uint8_t* dst, const uint8_t* src, void* parent) {
    EnsureDeps();
    *reinterpret_cast<uint64_t*>(dst) = *reinterpret_cast<const uint64_t*>(src); // key
    *reinterpret_cast<uint64_t*>(dst + 0x08) = 0;
    *reinterpret_cast<uint32_t*>(dst + 0x10) = 0;
    if (src + 0x08 != dst + 0x08) { // always true at our call sites; kept for fidelity
        const int32_t* srcSubtree = *reinterpret_cast<int32_t* const*>(src + 0x08);
        if (srcSubtree != nullptr) {
            int32_t* copy = Goals_LayoutB_DeepCopy_cfb0(0, const_cast<int32_t*>(srcSubtree));
            *reinterpret_cast<int32_t**>(dst + 0x08) = copy;
        }
        *reinterpret_cast<uint32_t*>(dst + 0x10) = *reinterpret_cast<const uint32_t*>(src + 0x10);
    }
    *reinterpret_cast<void**>(dst + 0x18) = parent;
    *reinterpret_cast<uint64_t*>(dst + 0x20) = 0; // left
    *reinterpret_cast<uint64_t*>(dst + 0x28) = 0; // right
    *reinterpret_cast<uint32_t*>(dst + 0x30) = 0; // balance/height
    return dst;
}

// ============================================================
// 0x14002ccf0 — Layout-C BST insert. treeField = {root@0, count@8}. pKey = {u64 key@0,
// subtreeRoot@8, int@0x10} (the record to insert-or-find). On existing key: writes the
// existing node to *pOut. Otherwise allocates via ceb0, links, rebalances (0x140014b40).
// ============================================================
extern "C" void** Goals_LayoutC_Insert_ccf0(uint64_t* treeField, void** pOut, const uint8_t* pKey) {
    EnsureDeps();
    uint64_t key = *reinterpret_cast<const uint64_t*>(pKey);
    uint8_t* root = *reinterpret_cast<uint8_t**>(treeField);
    uint8_t* result;
    if (root == nullptr) {
        uint8_t* buf = reinterpret_cast<uint8_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
        result = buf ? Goals_LayoutC_CopyCtor_ceb0(buf, pKey, nullptr) : nullptr;
        *reinterpret_cast<uint8_t**>(treeField) = result;
    } else {
        uint8_t* cur = root;
        uint8_t* parent = cur;
        for (;;) {
            parent = cur;
            uint64_t nodeKey = *reinterpret_cast<uint64_t*>(parent);
            if (nodeKey <= key) {
                if (key <= nodeKey) {
                    *pOut = parent;
                    return pOut;
                }
                cur = *reinterpret_cast<uint8_t**>(parent + 0x28); // right
                if (cur == nullptr) {
                    uint8_t* buf = reinterpret_cast<uint8_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
                    uint8_t* node = buf ? Goals_LayoutC_CopyCtor_ceb0(buf, pKey, parent) : nullptr;
                    *reinterpret_cast<uint8_t**>(parent + 0x28) = node;
                    result = node;
                    goto rebalance;
                }
            } else {
                cur = *reinterpret_cast<uint8_t**>(parent + 0x20); // left
                if (cur == nullptr) {
                    uint8_t* buf = reinterpret_cast<uint8_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8));
                    uint8_t* node = buf ? Goals_LayoutC_CopyCtor_ceb0(buf, pKey, parent) : nullptr;
                    *reinterpret_cast<uint8_t**>(parent + 0x20) = node;
                    result = node;
                    goto rebalance;
                }
            }
        }
    rebalance:
        int64_t cursor = reinterpret_cast<int64_t>(parent);
        if (s_rebC) s_rebC(reinterpret_cast<int64_t*>(treeField), &cursor);
    }
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(treeField) + 0x08) += 1; // count++
    *pOut = result;
    return pOut;
}

// ============================================================
// 0x1400f8270 — TradeComponent_helper_8270 (SAVE-side pre-pass). In-order walk of the
// idTree (self+0x30, Layout B); CheckPtr the payload IBP then call vtbl+0x48 on the
// pointed entity (self, no extra args).
// ============================================================
extern "C" void Goals_SavePrepass_8270(void* selfV) {
    EnsureDeps();
    uint8_t* selfB = reinterpret_cast<uint8_t*>(selfV);
    uint8_t* root = *reinterpret_cast<uint8_t**>(selfB + 0x30);
    for (uint8_t* node = BstLeftmostB(root); node; node = BstNextB(node)) {
        uint8_t* ibpField = node + 0x08;
        void* entity = *reinterpret_cast<void**>(ibpField);
        void* vtbl = *reinterpret_cast<void**>(entity);
        typedef void (*fn_Vtbl48_t)(void*);
        reinterpret_cast<fn_Vtbl48_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtbl) + 0x48))(entity);
    }
}

// ============================================================
// 0x14002d340 — single Layout-B tree serializer (same shape as one d060 slot body).
// treeField = {root@0, count@8}. On LOAD, rebuilds the tree from the stream; on SAVE,
// walks it in-order. IsDerivedFrom-filters payload against g_tid_Work (nulls non-Work).
// ============================================================
extern "C" void Goals_SingleTree_Serialize_d340(int64_t* treeField, void* serializer) {
    EnsureDeps();
    int32_t count = static_cast<int32_t>(treeField[1]);
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge

    bool loading = (*reinterpret_cast<int32_t*>(*reinterpret_cast<uint8_t**>(serializer) + 8) == 0);
    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            // Retail re-reads *param_1 on every iteration — the previous insertion
            // may have installed (or rebalanced) the root. Hoisting this read out of
            // the loop (as an earlier copy of the d060 body did) leaves `root` pinned
            // at the entry value, so every key takes the "empty tree" path and
            // overwrites the single root node instead of building the tree.
            int32_t* root = reinterpret_cast<int32_t*>(treeField[0]);
            int32_t key = 0;
            if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(key, nullptr);   // B.3.4 bridge
            void* payload = nullptr;
            if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(&payload)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            if (payload != nullptr && fn_IsDerivedFrom && !fn_IsDerivedFrom(payload, g_tid_Work)) {
                payload = nullptr;
            }
            int32_t* node = LayoutB_Find(root, key);
            if (node == nullptr) {
                if (root == nullptr) {
                    node = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                    if (node) {
                        node[0] = key;
                        *reinterpret_cast<int32_t**>(node + 2) = reinterpret_cast<int32_t*>(payload);
                        node[4] = 0; node[5] = 0; node[6] = 0; node[7] = 0; node[8] = 0; node[9] = 0; node[10] = 0;
                    }
                    treeField[0] = reinterpret_cast<int64_t>(node);
                } else {
                    int32_t* parent = root;
                    int32_t* fresh = nullptr;
                    for (;;) {
                        if (*parent <= key) {
                            int32_t* right = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x20);
                            if (right == nullptr) {
                                fresh = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                                if (fresh) {
                                    fresh[0] = key;
                                    *reinterpret_cast<int32_t**>(fresh + 2) = reinterpret_cast<int32_t*>(payload);
                                    *reinterpret_cast<int32_t**>(fresh + 4) = parent;
                                    fresh[6] = 0; fresh[7] = 0; fresh[8] = 0; fresh[9] = 0; fresh[10] = 0;
                                }
                                *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x20) = fresh;
                                break;
                            }
                            parent = right;
                        } else {
                            int32_t* left = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18);
                            if (left == nullptr) {
                                fresh = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                                if (fresh) {
                                    fresh[0] = key;
                                    *reinterpret_cast<int32_t**>(fresh + 2) = reinterpret_cast<int32_t*>(payload);
                                    *reinterpret_cast<int32_t**>(fresh + 4) = parent;
                                    fresh[6] = 0; fresh[7] = 0; fresh[8] = 0; fresh[9] = 0; fresh[10] = 0;
                                }
                                *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18) = fresh;
                                break;
                            }
                            parent = left;
                        }
                    }
                    int64_t cursor = reinterpret_cast<int64_t>(parent);
                    if (s_rebB) s_rebB(treeField, &cursor);
                    node = fresh;
                }
                treeField[1] += 1;
            }
        }
    } else {
        uint8_t* root = *reinterpret_cast<uint8_t**>(treeField);
        for (uint8_t* node = BstLeftmostB(root); node; node = BstNextB(node)) {
            if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int32_t*>(node), nullptr);   // B.3.4 bridge
            if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(node + 0x08)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
            void* payload = *reinterpret_cast<void**>(node + 0x08);
            if (payload != nullptr && fn_IsDerivedFrom && !fn_IsDerivedFrom(payload, g_tid_Work)) {
                *reinterpret_cast<void**>(node + 0x08) = nullptr;
            }
        }
    }
}


// ============================================================
// 0x14002d060 — 14/15-slot Layout-B tree array serializer. count = number of slots
// (0xe or 0xf, version-gated by the caller). arrayBase = base of the {root,count} header
// array (stride 0x10). outParam forwarded through (unused directly here, matches retail:
// it's read by param_4 but never dereferenced inside this function body).
// ============================================================

extern "C" void Goals_TreeArray_Serialize_d060(void* serializer, int32_t count, int64_t* arrayBase, int32_t* /*outParam*/) {
    EnsureDeps();
    bool loading = (*reinterpret_cast<int32_t*>(*reinterpret_cast<uint8_t**>(serializer) + 8) == 0);
    int64_t* slot = arrayBase;
    for (int32_t s = 0; s < count; ++s, slot += 2) {
        int32_t slotCount = static_cast<int32_t>(slot[1]);
        if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(slotCount, nullptr);   // B.3.4 bridge

        if (loading) {
            for (int32_t i = 0; i < slotCount; ++i) {
                int32_t key = 0;
                if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(key, nullptr);   // B.3.4 bridge
                void* payload = nullptr;
                if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(&payload)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
                if (payload != nullptr && fn_IsDerivedFrom && !fn_IsDerivedFrom(payload, g_tid_Work)) {
                    payload = nullptr;
                }
                int32_t* root = reinterpret_cast<int32_t*>(slot[0]);
                int32_t* existing = LayoutB_Find(root, key);
                if (existing == nullptr) {
                    if (root == nullptr) {
                        int32_t* node = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                        if (node) {
                            node[0] = key;
                            *reinterpret_cast<int32_t**>(node + 2) = reinterpret_cast<int32_t*>(payload);
                            node[4] = 0; node[5] = 0; node[6] = 0; node[7] = 0; node[8] = 0; node[9] = 0; node[10] = 0;
                        }
                        slot[0] = reinterpret_cast<int64_t>(node);
                    } else {
                        int32_t* parent = root;
                        int32_t* fresh = nullptr;
                        for (;;) {
                            if (*parent <= key) {
                                int32_t* right = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x20);
                                if (right == nullptr) {
                                    fresh = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                                    if (fresh) {
                                        fresh[0] = key;
                                        *reinterpret_cast<int32_t**>(fresh + 2) = reinterpret_cast<int32_t*>(payload);
                                        *reinterpret_cast<int32_t**>(fresh + 4) = parent;
                                        fresh[6] = 0; fresh[7] = 0; fresh[8] = 0; fresh[9] = 0; fresh[10] = 0;
                                    }
                                    *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x20) = fresh;
                                    break;
                                }
                                parent = right;
                            } else {
                                int32_t* left = *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18);
                                if (left == nullptr) {
                                    fresh = reinterpret_cast<int32_t*>(fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8));
                                    if (fresh) {
                                        fresh[0] = key;
                                        *reinterpret_cast<int32_t**>(fresh + 2) = reinterpret_cast<int32_t*>(payload);
                                        *reinterpret_cast<int32_t**>(fresh + 4) = parent;
                                        fresh[6] = 0; fresh[7] = 0; fresh[8] = 0; fresh[9] = 0; fresh[10] = 0;
                                    }
                                    *reinterpret_cast<int32_t**>(reinterpret_cast<uint8_t*>(parent) + 0x18) = fresh;
                                    break;
                                }
                                parent = left;
                            }
                        }
                        int64_t cursor = reinterpret_cast<int64_t>(parent);
                        if (s_rebB) s_rebB(slot, &cursor);
                    }
                    slot[1] += 1;
                }
            }
        } else {
            uint8_t* root = reinterpret_cast<uint8_t*>(slot[0]);
            for (uint8_t* node = BstLeftmostB(root); node; node = BstNextB(node)) {
                if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int32_t*>(node), nullptr);   // B.3.4 bridge
                if (fn_InternalBasePtr_Serialize) reinterpret_cast<Object::InternalBasePtr*>(node + 0x08)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge
                void* payload = *reinterpret_cast<void**>(node + 0x08);
                if (payload != nullptr && fn_IsDerivedFrom && !fn_IsDerivedFrom(payload, g_tid_Work)) {
                    *reinterpret_cast<void**>(node + 0x08) = nullptr;
                }
            }
        }
    }
}

// ============================================================
// 0x14002d5f0 — Layout-C (0x38 node) tree serializer. param_1 = {root@0, count@8}.
// ============================================================
extern "C" void Goals_WorkTree_Serialize_d5f0(int64_t* treeField, void* serializer) {
    EnsureDeps();
    int32_t count = static_cast<int32_t>(treeField[1]);
    if (g_fn_Serialize_Int) reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);   // B.3.4 bridge

    bool loading = (*reinterpret_cast<int32_t*>(*reinterpret_cast<uint8_t**>(serializer) + 8) == 0);
    if (loading) {
        for (int32_t i = 0; i < count; ++i) {
            // Record layout (retail's local_40/local_38/local_30 triple):
            //   +0x00 u64 external key, +0x08 subtree root, +0x10 u32 subtree count.
            // The {root,count} header d340 fills IS record+0x08 — retail passes
            // &local_38, not a separate pair. Building the subtree somewhere else and
            // then inserting the record loses every Work subtree on load.
            uint8_t recordBuf[0x18];
            std::memset(recordBuf, 0, sizeof(recordBuf));
            if (s_externalRefSerialize) s_externalRefSerialize(recordBuf, serializer);

            int64_t* subtreeField = reinterpret_cast<int64_t*>(recordBuf + 8);
            Goals_SingleTree_Serialize_d340(subtreeField, serializer);

            void* outNode = nullptr;
            Goals_LayoutC_Insert_ccf0(reinterpret_cast<uint64_t*>(treeField), &outNode, recordBuf);
            // The insert deep-copies the record, so the subtree we just built is ours to
            // release — retail's FUN_14000e320 free-then-MemFree pair on the same header.
            if (subtreeField[0] != 0) {
                void* subRoot = reinterpret_cast<void*>(subtreeField[0]);
                if (s_freeB) {
                    s_freeB(subtreeField, *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(subRoot) + 0x18)); // left
                    s_freeB(subtreeField, *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(subRoot) + 0x20)); // right
                }
                if (fn_MemFreeStatic) fn_MemFreeStatic(subRoot);
            }
        }
    } else {
        uint8_t* root = *reinterpret_cast<uint8_t**>(treeField);
        // Layout-C in-order walk: parent@0x18, left@0x20, right@0x28.
        auto leftmostC = [](uint8_t* n) -> uint8_t* {
            if (!n) return nullptr;
            while (*reinterpret_cast<uint8_t**>(n + 0x20)) n = *reinterpret_cast<uint8_t**>(n + 0x20);
            return n;
        };
        auto nextC = [&leftmostC](uint8_t* n) -> uint8_t* {
            uint8_t* right = *reinterpret_cast<uint8_t**>(n + 0x28);
            if (right) return leftmostC(right);
            uint8_t* parent = *reinterpret_cast<uint8_t**>(n + 0x18);
            uint8_t* cur = n;
            while (parent && *reinterpret_cast<uint8_t**>(parent + 0x28) == cur) {
                cur = parent;
                parent = *reinterpret_cast<uint8_t**>(parent + 0x18);
            }
            return parent;
        };
        for (uint8_t* cNode = leftmostC(root); cNode; cNode = nextC(cNode)) {
            if (s_externalRefSerialize) s_externalRefSerialize(cNode, serializer);
            // Retail passes plVar5+1 — the node's OWN {root@+0x08, count@+0x10}
            // header. Copying the root into a stack pair with a zeroed count made
            // d340 write "0 entries" and then walk the real subtree anyway, so the
            // save stream carried N records the load never read back: the reader
            // stopped at count 0 and hit those records as the next record's key.
            Goals_SingleTree_Serialize_d340(
                reinterpret_cast<int64_t*>(cNode + 0x08), serializer);
        }
    }
}

// ============================================================
// 0x1400f9240 — main manager serializer.
// ============================================================
extern "C" void Goals_Serialize_f9240(void* selfV, void* stream, void* /*p3*/, int32_t* outParam) {
    EnsureDeps();
    uint8_t* self = reinterpret_cast<uint8_t*>(selfV);

    alignas(8) uint8_t serializerBuf[24];
    if (g_fn_Serializer_Ctor) ::new (static_cast<void*>(serializerBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge

    bool saving = (*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(stream) + 8) == 1);
    if (saving) {
        Goals_SavePrepass_8270(selfV);
    }

    int32_t version = g_fn_Serializer_GetVersion ? reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion() : 0;   // B.3.4 bridge
    int32_t slotCount = (version > 2) ? 0xf : 0xe;

    Goals_TreeArray_Serialize_d060(serializerBuf, slotCount, reinterpret_cast<int64_t*>(self + 0x30), outParam);
    Goals_WorkTree_Serialize_d5f0(reinterpret_cast<int64_t*>(self + 0x120), serializerBuf);

    if (g_fn_Serialize_Int) {
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int32_t*>(self + 0x00), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<int32_t*>(self + 0x04), nullptr);   // B.3.4 bridge
    }
    if (fn_Serializer_Serialize_Uint) {
        reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<uint32_t*>(self + 0x08), nullptr);   // B.3.4 bridge
    }

    bool loading = (*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(stream) + 8) == 0);
    if (loading) {
        int32_t* workCountRoot = *reinterpret_cast<int32_t**>(self + 0x10);

        // --- Stage 1: walk the idTree (self+0x30), notify each entity (vtbl+0x60), and for
        // version==2 entries build the workCountTree (self+0x10) keyed by the running counter
        // at self+0x18. ---
        {
            uint8_t* idRoot = *reinterpret_cast<uint8_t**>(self + 0x30);
            for (uint8_t* node = BstLeftmostB(idRoot); node; node = BstNextB(node)) {
                uint8_t* ibpField = node + 0x08;
                void* entity = *reinterpret_cast<void**>(ibpField);
                void* vtbl = *reinterpret_cast<void**>(entity);

                if (g_fn_Serializer_GetVersion) reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion(); // discarded, matches retail   // B.3.4 bridge
                typedef void (*fn_Vtbl60_t)(void*);
                reinterpret_cast<fn_Vtbl60_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtbl) + 0x60))(entity);

                int32_t ver = g_fn_Serializer_GetVersion ? reinterpret_cast<IO::Serializer*>(serializerBuf)->GetVersion() : 0;   // B.3.4 bridge
                if (ver == 2) {
                    int32_t counter = *reinterpret_cast<int32_t*>(self + 0x18);
                    uint32_t hi = *reinterpret_cast<uint32_t*>(*reinterpret_cast<uint8_t**>(ibpField) + 0x20);
                    uint64_t value = static_cast<uint64_t>(static_cast<uint32_t>(counter)) |
                                      (static_cast<uint64_t>(hi) << 32);
                    bool inserted = false;
                    LayoutA_Insert(&workCountRoot, value, &inserted);
                    if (inserted) {
                        *reinterpret_cast<int32_t*>(self + 0x18) = counter + 1;
                    }
                }
            }
            *reinterpret_cast<int32_t**>(self + 0x10) = workCountRoot;
        }

        // --- Stage 2: re-walk the idTree; for entries whose entity+0x10 field is nonzero,
        // collect the bare id into local_60 (a fresh Layout-A set). ---
        int32_t* localFoundRoot = nullptr;
        int32_t localFoundCount = 0;
        {
            uint8_t* idRoot = *reinterpret_cast<uint8_t**>(self + 0x30);
            for (uint8_t* node = BstLeftmostB(idRoot); node; node = BstNextB(node)) {
                uint8_t* ibpField = node + 0x08;
                void* entity = *reinterpret_cast<void**>(ibpField);
                if (*reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(entity) + 0x10) != 0) {
                    uint64_t value = static_cast<uint32_t>(*reinterpret_cast<int32_t*>(node)); // key only; upper 32 unused by retail
                    bool inserted = false;
                    LayoutA_Insert(&localFoundRoot, value, &inserted);
                    localFoundCount++;
                }
            }
        }

        // --- Stage 3: scan the live GameWorld entity list; for each entity, find its
        // WorkComponent (g_TypeId_WorkComponent), look its workId up in the world work-slot
        // tree (*g_pGameWorldSingleton+0x118)+0x30 == self+0x30's OWN tree), and if present,
        // erase the matching id from localFoundRoot (it's still alive). ---
        if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
            uint8_t* gameWorld = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
            void* entityListObj = *reinterpret_cast<void**>(gameWorld + 0xe8);
            int32_t entityCount = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(entityListObj) + 0x10);
            void* entityArrayBase = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(entityListObj) + 0x08);

            for (int32_t i = 0; i < entityCount; ++i) {
                void* entity = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(entityArrayBase) + i * 8);

                void* workComponent = nullptr;
                int32_t compCount = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(entity) + 0x38);
                if (compCount > 0) {
                    void* compArrayBase = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(entity) + 0x30);
                    for (int32_t c = 0; c < compCount; ++c) {
                        void** slot = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(compArrayBase) + c * 8);
                        void* comp = *slot;
                        void* compVtbl = *reinterpret_cast<void**>(comp);
                        typedef uint32_t (*fn_GetTypeId_t)(void*);
                        uint32_t tid = reinterpret_cast<fn_GetTypeId_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(compVtbl) + 0x10))(comp);
                        if (g_TypeId_WorkComponent && tid == *g_TypeId_WorkComponent) {
                            void* obj = *slot;
                            bool derived = fn_IsDerivedFrom && fn_IsDerivedFrom(obj, *g_TypeId_WorkComponent);
                            workComponent = derived ? obj : nullptr;
                            break;
                        }
                    }
                }

                void* ibp = nullptr;
                if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(workComponent));   // B.3.3 bridge
                if (ibp != nullptr) {
                    int32_t workId = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(ibp) + 0x28);

                    uint8_t* slotRoot = *reinterpret_cast<uint8_t**>(gameWorld + 0x118);
                    slotRoot = *reinterpret_cast<uint8_t**>(slotRoot + 0x30);
                    int32_t* slotNode = LayoutB_Find(reinterpret_cast<int32_t*>(slotRoot), workId);

                    if (slotNode != nullptr) {
                        int32_t* found = LayoutA_Find(localFoundRoot, workId);
                        if (found != nullptr && s_eraseA) {
                            s_eraseA(&localFoundRoot, found);
                        }
                    }
                }
            }
        }

        // --- Stage 4: for every id remaining in localFoundRoot (genuinely dead), remove it
        // from the 14/15-tree array, workCountTree, and the Layout-C work tree, then delete
        // the entity and drop it from the idTree itself. ---
        if (localFoundRoot != nullptr) {
            for (uint8_t* idNode = BstLeftmostA(reinterpret_cast<uint8_t*>(localFoundRoot)); idNode; ) {
                int32_t deadId = *reinterpret_cast<int32_t*>(idNode);
                uint8_t* nextIdNode = BstNextA(idNode); // capture successor before any mutation below

                int32_t* idTreeNode = LayoutB_Find(reinterpret_cast<int32_t*>(*reinterpret_cast<uint8_t**>(self + 0x30)), deadId);
                if (idTreeNode != nullptr) {
                    // Erase the id from all 14 remaining slot-array trees (self+0x40..+0x11F).
                    int64_t* slot = reinterpret_cast<int64_t*>(self + 0x40);
                    for (int32_t s = 0; s < 14; ++s, slot += 2) {
                        int32_t* subRoot = reinterpret_cast<int32_t*>(slot[0]);
                        int32_t* match = LayoutB_Find(subRoot, deadId);
                        if (match != nullptr && s_eraseB) {
                            s_eraseB(slot, match);
                        }
                    }

                    uint8_t* ibpField = reinterpret_cast<uint8_t*>(idTreeNode) + 0x08; // node+0x08
                    void* entity = *reinterpret_cast<void**>(ibpField);
                    int32_t orderKey = *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(entity) + 0x24);

                    int32_t* workCountRoot2 = *reinterpret_cast<int32_t**>(self + 0x10);
                    int32_t* wcMatch = LayoutA_Find(workCountRoot2, orderKey);
                    if (wcMatch != nullptr && s_eraseA) {
                        s_eraseA(&workCountRoot2, wcMatch);
                        *reinterpret_cast<int32_t**>(self + 0x10) = workCountRoot2;
                    }

                    uint8_t keyOutBuf[0x20];
                    std::memset(keyOutBuf, 0, sizeof(keyOutBuf));
                    void* entityVtbl = *reinterpret_cast<void**>(entity);
                    typedef uint64_t* (*fn_Vtbl50_t)(void*, void*);
                    uint64_t* keyPtr = reinterpret_cast<fn_Vtbl50_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(entityVtbl) + 0x50))(entity, keyOutBuf);

                    uint8_t* workTreeRoot = *reinterpret_cast<uint8_t**>(self + 0x120);
                    uint8_t* cNode = workTreeRoot;
                    while (cNode != nullptr) {
                        uint64_t nodeKey = *reinterpret_cast<uint64_t*>(cNode);
                        if (*keyPtr < nodeKey) {
                            cNode = *reinterpret_cast<uint8_t**>(cNode + 0x20); // left
                        } else if (nodeKey < *keyPtr) {
                            cNode = *reinterpret_cast<uint8_t**>(cNode + 0x28); // right
                        } else {
                            break;
                        }
                    }
                    if (cNode != nullptr) {
                        int32_t* subtreeRoot = *reinterpret_cast<int32_t**>(cNode + 0x08);
                        int32_t* subMatch = LayoutB_Find(subtreeRoot, deadId);
                        if (subMatch != nullptr && s_eraseB) {
                            s_eraseB(cNode + 0x08, subMatch);
                        }
                    }

                    if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(ibpField)->DeletePtr(
                        );   // B.3.3 bridge
                    if (s_eraseB) s_eraseB(self + 0x30, idTreeNode);
                }

                idNode = nextIdNode;
            }
        }

        // --- Stage 5: final walk over the idTree (self+0x30) calling vtbl+0x48 (same shape
        // as the SAVE-side pre-pass). ---
        {
            uint8_t* idRoot = *reinterpret_cast<uint8_t**>(self + 0x30);
            for (uint8_t* node = BstLeftmostB(idRoot); node; node = BstNextB(node)) {
                uint8_t* ibpField = node + 0x08;
                void* entity = *reinterpret_cast<void**>(ibpField);
                void* vtbl = *reinterpret_cast<void**>(entity);
                typedef void (*fn_Vtbl48_t)(void*);
                reinterpret_cast<fn_Vtbl48_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vtbl) + 0x48))(entity);
            }
        }

        // --- Stage 6: free localFoundRoot (Layout-A post-order free of children, then the
        // root node itself). ---
        if (localFoundRoot != nullptr) {
            uint8_t* rootB = reinterpret_cast<uint8_t*>(localFoundRoot);
            if (s_freeA) {
                s_freeA(&localFoundRoot, *reinterpret_cast<void**>(rootB + 0x10)); // left
                s_freeA(&localFoundRoot, *reinterpret_cast<void**>(rootB + 0x18)); // right
            }
            if (fn_MemFreeStatic) fn_MemFreeStatic(localFoundRoot);
        }
        (void)localFoundCount;
    }

    if (g_fn_Serializer_Dtor) reinterpret_cast<IO::Serializer*>(serializerBuf)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// Registration
// ============================================================
void RegisterGoalsSerialize() {
    RegisterExeAddr(0x1400f9240, reinterpret_cast<uintptr_t>(&Goals_Serialize_f9240));
    RegisterExeAddr(0x1400f8270, reinterpret_cast<uintptr_t>(&Goals_SavePrepass_8270));
    RegisterExeAddr(0x14002d060, reinterpret_cast<uintptr_t>(&Goals_TreeArray_Serialize_d060));
    RegisterExeAddr(0x14002d5f0, reinterpret_cast<uintptr_t>(&Goals_WorkTree_Serialize_d5f0));
    RegisterExeAddr(0x14002d340, reinterpret_cast<uintptr_t>(&Goals_SingleTree_Serialize_d340));
    RegisterExeAddr(0x14002ccf0, reinterpret_cast<uintptr_t>(&Goals_LayoutC_Insert_ccf0));
    RegisterExeAddr(0x14002ceb0, reinterpret_cast<uintptr_t>(&Goals_LayoutC_CopyCtor_ceb0));
    RegisterExeAddr(0x14002cfb0, reinterpret_cast<uintptr_t>(&Goals_LayoutB_DeepCopy_cfb0));
    // Type-id storage cell — needs a GetTypeIdByName("Work") fill wired into
    // FillEconomyTypeIdGlobals() (code/Game/Game/game_main.cpp) for full correctness; see
    // the file header note. Registering the storage cell alone keeps ResolveAddr non-null.
    RegisterExeAddr(0x1401c6cf8, reinterpret_cast<uintptr_t>(&g_tid_Work));
}
