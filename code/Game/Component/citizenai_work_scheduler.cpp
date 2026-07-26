// code/Game/Component/citizenai_work_scheduler.cpp
// Clean C++ port of the CitizenAI work-scheduler priority-AVL INSERT family. This is the
// chokepoint both construction gather (2b) and build labor (2c) post through:
//
//   CitizenAI_impl_ec60 (0x14012ec60)  — post wrapper: stop existing work slot, then
//   CitizenAI_impl_88a0 (0x1400f88a0)  — the scheduler insert: a primary AVL keyed by
//                                        queue order + a per-id map + 14 category-bitmask
//                                        sub-trees + the per-PROFESSION bucket tree.
//   TradeWork_impl_ccf0 (0x14002ccf0)  — BST insert into a per-entity work tree (0x38 node)
//   CitizenAI_helper_cb50 (0x14002cb50)— insert-with-temp-copy wrapper over ccf0
//   TradeWork_impl_ccf0_sub_ceb0 (0x14002ceb0) — 0x38-node copy-ctor
//   FUN_14002cfb0 (0x14002cfb0)        — recursive 0x30-node subtree deep-copy
//
// The remaining callees are already registered in standalone:
//   a8c0 (0x14001a8c0) primary-AVL rebalance, 3480 (0x140013480) category-AVL rebalance,
//   9a40 (0x140029a40) id-map insert, 4b40 (0x140014b40) entity-tree rebalance,
//   e320 (0x14000e320) node-free, ead0 (0x14012ead0)/eb50 (0x14012eb50) work-slot ops.
//
// Node layouts (verified against disassembly):
//   0x30 node (cfb0 tree): +0x00 u32 key, +0x08 u64 payload, +0x10 parent, +0x18 left,
//                          +0x20 right, +0x28 u16, +0x2a u16.
//   0x38 node (ccf0 tree): +0x00 u64 key, +0x08 subtree(0x30 nodes), +0x10 u32, +0x18
//                          parent, +0x20 left, +0x28 right, +0x30 u32 balance.
//   0x28 node (88a0 primary): +0x00 u32 key(prio), +0x04 u32 id, +0x08 parent, +0x10 left,
//                          +0x18 right, +0x20 u32 balance.
//   0x30 node (88a0 category/final): +0x00 u32 id, +0x08 u64 payload, +0x10 parent,
//                          +0x18 left, +0x20 right, +0x28 u32 balance.
//
// Faithful offset-by-offset port.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- resolved exe helpers --------------------------------------------------------
typedef void  (*WS_Rebalance_t)(void* treeField, void* pInsertedSlot);          // a8c0/3480/4b40
typedef void  (*WS_IdMapInsert_t)(void* treeField, void* pKeyNode, uint32_t* pId); // 9a40
typedef void  (*WS_NodeFree_t)(void* treeField, void* node);                     // e320
typedef char  (*WS_CheckSlot_t)(uintptr_t workComp);                             // ead0
typedef void  (*WS_StopSlot_t)(uintptr_t workComp);                              // eb50

typedef void  (*WS_NodeRemove_t)(void* treeField, void* node);                   // ca60

static WS_Rebalance_t   s_a8c0 = nullptr;
static WS_Rebalance_t   s_3480 = nullptr;
static WS_Rebalance_t   s_4b40 = nullptr;
static WS_IdMapInsert_t s_9a40 = nullptr;
static WS_NodeFree_t    s_e320 = nullptr;
static WS_CheckSlot_t   s_ead0 = nullptr;
static WS_StopSlot_t    s_eb50 = nullptr;
static WS_NodeRemove_t  s_ca60 = nullptr;
static bool s_ws_res = false;
static void WS_Resolve() {
    if (s_ws_res) return;
    s_a8c0 = reinterpret_cast<WS_Rebalance_t>(ResolveAddr(Module::Exe, 0x14001a8c0));
    s_3480 = reinterpret_cast<WS_Rebalance_t>(ResolveAddr(Module::Exe, 0x140013480));
    s_4b40 = reinterpret_cast<WS_Rebalance_t>(ResolveAddr(Module::Exe, 0x140014b40));
    s_9a40 = reinterpret_cast<WS_IdMapInsert_t>(ResolveAddr(Module::Exe, 0x140029a40));
    s_e320 = reinterpret_cast<WS_NodeFree_t>(ResolveAddr(Module::Exe, 0x14000e320));
    s_ead0 = reinterpret_cast<WS_CheckSlot_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_eb50 = reinterpret_cast<WS_StopSlot_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_ca60 = reinterpret_cast<WS_NodeRemove_t>(ResolveAddr(Module::Exe, 0x14001ca60));
    s_ws_res = true;
}

static void* WS_Alloc(size_t n) {
    return (fn_MemAlloc && g_pMemAllocSingleton) ? fn_MemAlloc(g_pMemAllocSingleton, (int)n, 8) : nullptr;
}

static int s_ws_trace = -1;
static bool WS_Trace() {
    if (s_ws_trace < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        s_ws_trace = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s_ws_trace == 1;
}

// ==================================================================================
// FUN_14002cfb0 — recursive deep-copy of a 0x30-node subtree. param_1 = ctx (unused
// beyond being threaded through recursion); src = source node. Returns the copy.
// ==================================================================================
static void* WS_cfb0(void* ctx, uint8_t* src) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
    uint8_t* out = nullptr;
    if (dst) {
        *reinterpret_cast<uint32_t*>(dst + 0x00) = *reinterpret_cast<uint32_t*>(src + 0x00);
        *reinterpret_cast<uint64_t*>(dst + 0x08) = 0;
        *reinterpret_cast<uint64_t*>(dst + 0x08) = *reinterpret_cast<uint64_t*>(src + 0x08);
        *reinterpret_cast<uint64_t*>(dst + 0x10) = 0;
        *reinterpret_cast<uint64_t*>(dst + 0x18) = 0;
        *reinterpret_cast<uint64_t*>(dst + 0x20) = 0;
        *reinterpret_cast<uint32_t*>(dst + 0x28) = 0;
        out = dst;
    }
    if (!out) return nullptr;
    *reinterpret_cast<uint16_t*>(out + 0x2a) = *reinterpret_cast<uint16_t*>(src + 0x2a);
    *reinterpret_cast<uint16_t*>(out + 0x28) = *reinterpret_cast<uint16_t*>(src + 0x28);
    uint8_t* left = *reinterpret_cast<uint8_t**>(src + 0x18);
    if (left) {
        void* c = WS_cfb0(ctx, left);
        *reinterpret_cast<void**>(out + 0x18) = c;
        if (c) *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c) + 0x10) = out;
    }
    uint8_t* right = *reinterpret_cast<uint8_t**>(src + 0x20);
    if (right) {
        void* c = WS_cfb0(ctx, right);
        *reinterpret_cast<void**>(out + 0x20) = c;
        if (c) *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c) + 0x10) = out;
    }
    return out;
}

// ==================================================================================
// TradeWork_impl_ccf0_sub_ceb0 — 0x38-node copy-ctor. dst = fresh 0x38 buffer,
// src = source node, parent = parent to record at dst+0x18.
// ==================================================================================
static void* WS_ceb0(uint8_t* dst, uint8_t* src, void* parent) {
    *reinterpret_cast<uint64_t*>(dst + 0x00) = *reinterpret_cast<uint64_t*>(src + 0x00);   // key
    *reinterpret_cast<uint64_t*>(dst + 0x08) = 0;                                          // subtree
    *reinterpret_cast<uint32_t*>(dst + 0x10) = 0;
    // The original guards `src+1 != dst+1` (self-copy); for distinct nodes copy subtree+int.
    if (src + 0x08 != dst + 0x08) {
        uint8_t* sub = *reinterpret_cast<uint8_t**>(src + 0x08);
        if (sub) {
            void* c = WS_cfb0(dst + 0x08, sub);
            *reinterpret_cast<void**>(dst + 0x08) = c;
        }
        *reinterpret_cast<uint32_t*>(dst + 0x10) = *reinterpret_cast<uint32_t*>(src + 0x10);
    }
    *reinterpret_cast<void**>(dst + 0x18) = parent;                                        // parent
    *reinterpret_cast<uint64_t*>(dst + 0x20) = 0;                                          // left
    *reinterpret_cast<uint64_t*>(dst + 0x28) = 0;                                          // right
    *reinterpret_cast<uint32_t*>(dst + 0x30) = 0;                                          // balance
    return dst;
}

// ==================================================================================
// TradeWork_impl_ccf0 — BST insert (0x38 nodes) into the tree at treeField[0] (root),
// treeField[1] = count. key at *pKey (u64). On existing key: writes node to *pOut and
// returns. Otherwise allocates via ceb0, links, rebalances (4b40).
//   treeField: longlong* {root@0, count@8}. pKey: ulonglong*. pOut: undefined8*.
// ==================================================================================
static void* WS_ccf0(uint64_t* treeField, void** pOut, uint64_t* pKey) {
    uint8_t* root = *reinterpret_cast<uint8_t**>(treeField);
    if (root == nullptr) {
        uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x38));
        uint8_t* node = nullptr;
        if (n) node = reinterpret_cast<uint8_t*>(WS_ceb0(n, reinterpret_cast<uint8_t*>(pKey), nullptr));
        *reinterpret_cast<uint8_t**>(treeField) = node;
        *pOut = node;
    } else {
        uint8_t* cur = root;
        uint8_t* parent = cur;
        for (;;) {
            parent = cur;
            uint64_t nodeKey = *reinterpret_cast<uint64_t*>(cur + 0x00);
            if (nodeKey <= *pKey) {
                if (*pKey <= nodeKey) {           // equal -> found existing
                    *pOut = cur;
                    return pOut;
                }
                // *pKey > nodeKey -> go right (+0x28)
                cur = *reinterpret_cast<uint8_t**>(parent + 0x28);
                if (cur == nullptr) {
                    uint8_t* buf = reinterpret_cast<uint8_t*>(WS_Alloc(0x38));
                    uint8_t* node = buf ? reinterpret_cast<uint8_t*>(WS_ceb0(buf, reinterpret_cast<uint8_t*>(pKey), parent)) : nullptr;
                    *reinterpret_cast<uint8_t**>(parent + 0x28) = node;
                    cur = node;
                    break;
                }
            } else {
                // *pKey < nodeKey -> go left (+0x20)
                cur = *reinterpret_cast<uint8_t**>(parent + 0x20);
                if (cur == nullptr) {
                    uint8_t* buf = reinterpret_cast<uint8_t*>(WS_Alloc(0x38));
                    uint8_t* node = buf ? reinterpret_cast<uint8_t*>(WS_ceb0(buf, reinterpret_cast<uint8_t*>(pKey), parent)) : nullptr;
                    *reinterpret_cast<uint8_t**>(parent + 0x20) = node;
                    cur = node;
                    break;
                }
            }
        }
        void* localTmp = parent;   // &local_res8 holds `parent` for the rebalance
        if (s_4b40) s_4b40(treeField, &localTmp);
        *pOut = cur;
    }
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(treeField) + 0x08) += 1;   // count++
    return pOut;
}

// ==================================================================================
// CitizenAI_helper_cb50 — build a temp key node (copying a sub-object via cfb0 from
// pSrcKey), insert it into treeField via ccf0, then free the temp copy.
//   param_1 = treeField, param_2 = pOut, pKey = ulonglong* key source,
//   pSrcKey = optional {ptr@0, int@8} whose ptr is deep-copied.
// ==================================================================================
static void* WS_cb50(uint64_t* treeField, void** pOut, uint64_t* pKey, void** pSrc) {
    uint64_t local_key = *pKey;
    void*    local_sub = nullptr;
    uint32_t local_int = 0;
    // Build a temp {key@0, sub@8, int@0x10} on the stack mirroring the node prefix.
    struct { uint64_t key; void* sub; uint32_t i; uint32_t pad; } tmp;
    tmp.key = local_key; tmp.sub = nullptr; tmp.i = 0; tmp.pad = 0;
    if (pSrc != reinterpret_cast<void**>(&local_sub)) {
        if (pSrc && *pSrc != nullptr) {
            local_sub = WS_cfb0(&tmp.sub, reinterpret_cast<uint8_t*>(*pSrc));
            tmp.sub = local_sub;
        }
        local_int = pSrc ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(pSrc) + 8) : 0;
        tmp.i = local_int;
    }
    WS_ccf0(treeField, pOut, reinterpret_cast<uint64_t*>(&tmp));
    // free the temp deep-copied subtree
    if (tmp.sub) {
        uint8_t* sub = reinterpret_cast<uint8_t*>(tmp.sub);
        if (s_e320) {
            s_e320(&tmp.sub, *reinterpret_cast<void**>(sub + 0x18));
            s_e320(&tmp.sub, *reinterpret_cast<void**>(sub + 0x20));
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(sub);
    }
    return pOut;
}

// ==================================================================================
// CitizenAI_impl_88a0 — the scheduler insert.
//   sched (int*) = gw+0x118. pWorkIBP = InternalBasePtr* to the work. code = category
//   bitmask (bits 1..14). fromExisting = param_4. prio = param_5.
// `prio` is a QUEUE-ORDER key, not an importance level: when fromExisting == 0 the caller's
// value is thrown away and replaced by sched[1]++ (a monotonic counter), so a caller passing
// 0 and a caller passing -1 behave identically. It is only honoured when fromExisting != 0,
// which is exactly the "re-post at the position I already had" case — the wrapper (ec60)
// sets that flag only when workComp+0x2c holds a previously assigned order (!= INT_MIN).
// Retail's only direct caller, FUN_14009c960, passes ('\0', -1): the value is dead there.
// Returns the assigned id.
// ==================================================================================
extern "C" int CitizenAI_WorkScheduler_Insert(int* sched, void* pWorkIBP, uint32_t code,
                                              char fromExisting, int prio) {
    WS_Resolve();
    uint8_t* schedB = reinterpret_cast<uint8_t*>(sched);

    // --- assign id (wraps at 0x7ffffffe) ---
    sched[0] += 1;
    int id = sched[0];
    if (id == 0x7ffffffe) sched[0] = 0;

    // --- assign priority ---
    if (fromExisting == '\0') {
        prio = sched[1];
        sched[1] = prio + 1;
    } else {
        // dedup: while a primary node already holds `prio`, decrement and restart search
        uint8_t* p = *reinterpret_cast<uint8_t**>(schedB + 0x10);
        while (p != nullptr) {
            int nodeKey = *reinterpret_cast<int*>(p + 0x00);
            if (prio < nodeKey)      p = *reinterpret_cast<uint8_t**>(p + 0x10);   // left
            else if (nodeKey < prio) p = *reinterpret_cast<uint8_t**>(p + 0x18);   // right
            else { prio -= 1; p = *reinterpret_cast<uint8_t**>(schedB + 0x10); }
        }
    }

    // --- stamp the work IBP: +0x20 = id, +0x24 = prio ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pWorkIBP);
    uint8_t* work = *reinterpret_cast<uint8_t**>(pWorkIBP);
    *reinterpret_cast<int*>(work + 0x20) = id;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pWorkIBP);
    work = *reinterpret_cast<uint8_t**>(pWorkIBP);
    *reinterpret_cast<int*>(work + 0x24) = prio;

    // --- insert into the primary AVL (0x28 nodes) at sched+0x10, keyed by prio ---
    // key node scratch = {prio, id}
    struct { int prio; int id; } keyNode = { prio, id };
    uint8_t* proot = *reinterpret_cast<uint8_t**>(schedB + 0x10);
    if (proot == nullptr) {
        uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x28));
        if (n) {
            *reinterpret_cast<int*>(n + 0x00) = keyNode.prio;
            *reinterpret_cast<int*>(n + 0x04) = keyNode.id;
            *reinterpret_cast<uint64_t*>(n + 0x08) = 0;
            *reinterpret_cast<uint64_t*>(n + 0x10) = 0;
            *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
            *reinterpret_cast<uint32_t*>(n + 0x20) = 0;
        }
        *reinterpret_cast<uint8_t**>(schedB + 0x10) = n;
    } else {
        uint8_t* cur = proot;
        uint8_t* parent = cur;
        bool linked = false;
        for (;;) {
            parent = cur;
            int nodeKey = *reinterpret_cast<int*>(cur + 0x00);
            if (nodeKey <= prio) {
                if (prio <= nodeKey) break;   // equal (shouldn't happen post-dedup)
                cur = *reinterpret_cast<uint8_t**>(parent + 0x18);   // right
                if (cur == nullptr) {
                    uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x28));
                    if (n) {
                        *reinterpret_cast<int*>(n + 0x00) = keyNode.prio;
                        *reinterpret_cast<int*>(n + 0x04) = keyNode.id;
                        *reinterpret_cast<void**>(n + 0x08) = parent;
                        *reinterpret_cast<uint64_t*>(n + 0x10) = 0;
                        *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                        *reinterpret_cast<uint32_t*>(n + 0x20) = 0;
                    }
                    *reinterpret_cast<uint8_t**>(parent + 0x18) = n;
                    linked = true; break;
                }
            } else {
                cur = *reinterpret_cast<uint8_t**>(parent + 0x10);   // left
                if (cur == nullptr) {
                    uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x28));
                    if (n) {
                        *reinterpret_cast<int*>(n + 0x00) = keyNode.prio;
                        *reinterpret_cast<int*>(n + 0x04) = keyNode.id;
                        *reinterpret_cast<void**>(n + 0x08) = parent;
                        *reinterpret_cast<uint64_t*>(n + 0x10) = 0;
                        *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                        *reinterpret_cast<uint32_t*>(n + 0x20) = 0;
                    }
                    *reinterpret_cast<uint8_t**>(parent + 0x10) = n;
                    linked = true; break;
                }
            }
        }
        if (linked) {
            void* tmp = parent;
            if (s_a8c0) s_a8c0(schedB + 0x10, &tmp);
            sched[6] += 1;   // +0x18 primary count
        }
    }

    // --- id-map insert (9a40) at sched+0x30. arg2 = &{prio,id} key node; arg3 =
    //     &{id, pad, workPtr} (the mapped value; ec60 later reads node+0x08 = workPtr). ---
    struct { int id; int pad; uint64_t workPtr; } mapVal;
    mapVal.id = id; mapVal.pad = 0; mapVal.workPtr = *reinterpret_cast<uint64_t*>(pWorkIBP);
    if (s_9a40) s_9a40(schedB + 0x30, &keyNode, reinterpret_cast<uint32_t*>(&mapVal));

    // --- 14 category sub-trees at sched+0x40 (stride 0x10), driven by `code` bits ---
    uint64_t workPtr = *reinterpret_cast<uint64_t*>(pWorkIBP);
    uint32_t mask = 2;
    for (int t = 0; t < 14; ++t) {
        if (code & mask) {
            uint8_t* treeField = schedB + 0x40 + (uintptr_t)t * 0x10;
            uint8_t* croot = *reinterpret_cast<uint8_t**>(treeField);
            if (croot == nullptr) {
                uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
                if (n) {
                    *reinterpret_cast<int*>(n + 0x00) = id;
                    *reinterpret_cast<uint64_t*>(n + 0x08) = workPtr;
                    *reinterpret_cast<uint64_t*>(n + 0x10) = 0;
                    *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                    *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                    *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
                }
                *reinterpret_cast<uint8_t**>(treeField) = n;
            } else {
                uint8_t* cur = croot;
                uint8_t* parent = cur;
                bool linked = false;
                for (;;) {
                    parent = cur;
                    int nodeKey = *reinterpret_cast<int*>(cur + 0x00);
                    if (nodeKey <= id) {
                        if (id <= nodeKey) break;
                        cur = *reinterpret_cast<uint8_t**>(parent + 0x20);   // right
                        if (cur == nullptr) {
                            uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
                            if (n) {
                                *reinterpret_cast<int*>(n + 0x00) = id;
                                *reinterpret_cast<uint64_t*>(n + 0x08) = workPtr;
                                *reinterpret_cast<void**>(n + 0x10) = parent;
                                *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                                *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                                *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
                            }
                            *reinterpret_cast<uint8_t**>(parent + 0x20) = n;
                            linked = true; break;
                        }
                    } else {
                        cur = *reinterpret_cast<uint8_t**>(parent + 0x18);   // left
                        if (cur == nullptr) {
                            uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
                            if (n) {
                                *reinterpret_cast<int*>(n + 0x00) = id;
                                *reinterpret_cast<uint64_t*>(n + 0x08) = workPtr;
                                *reinterpret_cast<void**>(n + 0x10) = parent;
                                *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                                *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                                *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
                            }
                            *reinterpret_cast<uint8_t**>(parent + 0x18) = n;
                            linked = true; break;
                        }
                    }
                }
                if (linked) {
                    void* tmp = parent;
                    if (s_3480) s_3480(treeField, &tmp);
                }
            }
            *reinterpret_cast<uint32_t*>(treeField + 0x08) += 1;   // sub-tree count++
        }
        // signed-shift replicate: uVar17 = uVar17<<1 | (uVar17<0)
        mask = (mask << 1) | ((int)mask < 0 ? 1u : 0u);
    }

    // --- per-PROFESSION bucket tree at sched+0x120 (0x38 nodes), keyed by the u64 the
    //     work's vtable slot +0x50 returns. That slot is the work's "who can do this"
    //     getter, NOT an entity id: Pickup_Slot10 (0x1400f3a50) and
    //     GatherRawMaterials_Slot10 (0x1400ec510) both return *(*(gw+0xb0)+0x30), the
    //     Laborer key. It is the same key space the re-key (f8400) moves items between —
    //     its callers pass wpc+0x48 / *(*(gw+0xb0)+0x38) (Builder) / desc+0x20. Each bucket
    //     node holds its own id-keyed inner tree at +0x08 with a count at +0x10.
    //     If the bucket is absent, cb50 inserts a fresh one. ---
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pWorkIBP);
    uint8_t* wobj = *reinterpret_cast<uint8_t**>(pWorkIBP);
    typedef uint64_t* (*Slot50_t)(void*, void*);
    void* vt = *reinterpret_cast<void**>(wobj);
    uint64_t slot50Buf[2] = {0, 0};
    uint64_t* targetKey = ((Slot50_t)(*reinterpret_cast<uintptr_t*>((uint8_t*)vt + 0x50)))(wobj, slot50Buf);

    uint8_t* froot = *reinterpret_cast<uint8_t**>(schedB + 0x120);
    uint8_t* found = nullptr;
    if (froot != nullptr) {
        uint8_t* p = froot;
        while (p != nullptr) {
            uint64_t nk = *reinterpret_cast<uint64_t*>(p + 0x00);
            if (*targetKey < nk)       p = *reinterpret_cast<uint8_t**>(p + 0x20);   // left([4])
            else if (*targetKey == nk) { found = p; break; }
            else                       p = *reinterpret_cast<uint8_t**>(p + 0x28);   // right([5])
        }
    }
    if (found == nullptr) {
        // cb50 builds+inserts a fresh bucket node whose key = the work's profession.
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pWorkIBP);
        void* wobj2 = *reinterpret_cast<void**>(pWorkIBP);
        void* vt2 = *reinterpret_cast<void**>(wobj2);
        uint64_t keyBuf[2] = {0, 0};
        uint64_t* freshKey = ((Slot50_t)(*reinterpret_cast<uintptr_t*>((uint8_t*)vt2 + 0x50)))(wobj2, keyBuf);
        void* outNode = nullptr;
        void* nullSrc[2] = { nullptr, nullptr };   // {ptr=0, int=0}: no subtree to copy
        WS_cb50(reinterpret_cast<uint64_t*>(schedB + 0x120), &outNode, freshKey, nullSrc);
        found = reinterpret_cast<uint8_t*>(outNode);
    }

    // --- inner tree hanging off the found profession bucket (+0x08), 0x30 nodes keyed by id ---
    if (found) {
        uint8_t* innerField = found + 0x08;
        uint64_t innerKeyPayload = *reinterpret_cast<uint64_t*>(pWorkIBP);
        uint8_t* iroot = *reinterpret_cast<uint8_t**>(innerField);
        if (iroot == nullptr) {
            uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
            if (n) {
                *reinterpret_cast<int*>(n + 0x00) = id;
                *reinterpret_cast<uint64_t*>(n + 0x08) = innerKeyPayload;
                *reinterpret_cast<uint64_t*>(n + 0x10) = 0;
                *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
            }
            *reinterpret_cast<uint8_t**>(innerField) = n;
        } else {
            uint8_t* cur = iroot;
            uint8_t* parent = cur;
            bool linked = false;
            for (;;) {
                parent = cur;
                int nodeKey = *reinterpret_cast<int*>(cur + 0x00);
                if (nodeKey <= id) {
                    if (id <= nodeKey) return id;   // already present
                    cur = *reinterpret_cast<uint8_t**>(parent + 0x20);
                    if (cur == nullptr) {
                        uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
                        if (n) {
                            *reinterpret_cast<int*>(n + 0x00) = id;
                            *reinterpret_cast<uint64_t*>(n + 0x08) = innerKeyPayload;
                            *reinterpret_cast<void**>(n + 0x10) = parent;
                            *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                            *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                            *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
                        }
                        *reinterpret_cast<uint8_t**>(parent + 0x20) = n;
                        linked = true; break;
                    }
                } else {
                    cur = *reinterpret_cast<uint8_t**>(parent + 0x18);
                    if (cur == nullptr) {
                        uint8_t* n = reinterpret_cast<uint8_t*>(WS_Alloc(0x30));
                        if (n) {
                            *reinterpret_cast<int*>(n + 0x00) = id;
                            *reinterpret_cast<uint64_t*>(n + 0x08) = innerKeyPayload;
                            *reinterpret_cast<void**>(n + 0x10) = parent;
                            *reinterpret_cast<uint64_t*>(n + 0x18) = 0;
                            *reinterpret_cast<uint64_t*>(n + 0x20) = 0;
                            *reinterpret_cast<uint32_t*>(n + 0x28) = 0;
                        }
                        *reinterpret_cast<uint8_t**>(parent + 0x18) = n;
                        linked = true; break;
                    }
                }
            }
            if (linked) {
                void* tmp = parent;
                if (s_3480) s_3480(innerField, &tmp);
            }
        }
        *reinterpret_cast<uint32_t*>(found + 0x10) += 1;   // profession bucket count++
    }

    // The flat id-map at sched+0x30 is filed here (the 9a40 call above) and is the thing
    // f8400 needs to find a work item by id. It was recorded as "never populates"; report its
    // root and count right where it is written so that claim is measured at the source rather
    // than inferred from the re-key's downstream "NOT in the sched+0x30 id-map" miss. NB most
    // posts reach this function DIRECTLY (exe 0x1400f88a0), not through the ec60 wrapper — a
    // trace placed only in the wrapper sees nothing.
    if (WS_Trace())
        std::fprintf(stderr, "[BuildTrace] WorkScheduler_Insert id=%d prio=%d code=0x%x "
                     "-> primary+cats+entity trees; idMap(sched+0x30) root=%p count=%d\n",
                     id, prio, code,
                     (void*)*reinterpret_cast<uint8_t**>(schedB + 0x30),
                     *reinterpret_cast<int*>(schedB + 0x38));
    return id;
}

// ==================================================================================
// CitizenAI_impl_ec60 — post wrapper. workComp = the WorkComponent, code = 0x2001,
// flag = param_3, pWorkIBP = the work IBP.
// ==================================================================================
extern "C" void CitizenAI_WorkScheduler_Post(uintptr_t workComp, uint32_t code, char flag, void* pWorkIBP) {
    WS_Resolve();
    if (s_ead0 && s_ead0(workComp)) {
        if (s_eb50) s_eb50(workComp);
    }
    char prioFlag;
    if (flag == '\0' || *reinterpret_cast<int*>(workComp + 0x2c) == (int)0x80000000)
        prioFlag = '\0';
    else
        prioFlag = '\x01';

    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uint8_t* gw = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* sched = *reinterpret_cast<int**>(gw + 0x118);
    if (!sched) return;

    // Faithful to ec60: pass a pointer to a local copy of the work-ptr (88a0 only reads it).
    void* localWork = *reinterpret_cast<void**>(pWorkIBP);
    int assignedId = CitizenAI_WorkScheduler_Insert(sched, &localWork, code, prioFlag,
                                                    *reinterpret_cast<int*>(workComp + 0x2c));
    *reinterpret_cast<int*>(workComp + 0x28) = assignedId;

    // Walk the sched+0x30 id-map (0x30 nodes: +0x00 id, +0x08 payload=workPtr, +0x18 left,
    // +0x20 right) for the assigned id; on match write back workComp+0x2c = *(payload+0x24)
    // (the work's stored priority). On miss, 0.
    uint8_t* schedB = reinterpret_cast<uint8_t*>(sched);
    uint8_t* p = *reinterpret_cast<uint8_t**>(schedB + 0x30);
    uint8_t* payload = nullptr;
    while (p != nullptr) {
        int nk = *reinterpret_cast<int*>(p + 0x00);
        if (nk <= assignedId) {
            if (assignedId <= nk) { payload = *reinterpret_cast<uint8_t**>(p + 0x08); break; }
            p = *reinterpret_cast<uint8_t**>(p + 0x20);   // right
        } else {
            p = *reinterpret_cast<uint8_t**>(p + 0x18);   // left
        }
    }
    if (payload) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&payload);
        *reinterpret_cast<int*>(workComp + 0x2c) = *reinterpret_cast<int*>(payload + 0x24);
    } else {
        *reinterpret_cast<int*>(workComp + 0x2c) = 0;
    }
    // The id-map half of the post is the one nothing downstream can see going wrong: a miss
    // here silently writes priority 0 into workComp+0x2c and makes the f8400 re-key a no-op
    // ("NOT in the sched+0x30 id-map"). Report the map's own state next to the lookup result
    // so "the id-map never populates" is a measured fact and not an inference from a
    // downstream symptom.
    if (WS_Trace())
        std::fprintf(stderr, "[BuildTrace] WorkScheduler_Post workComp=%p code=0x%x -> id=%d "
                     "idMap(root=%p count=%d) payload=%p prio(+0x2c)=%d\n",
                     (void*)workComp, code, assignedId,
                     (void*)*reinterpret_cast<uint8_t**>(schedB + 0x30),
                     *reinterpret_cast<int*>(schedB + 0x38), (void*)payload,
                     *reinterpret_cast<int*>(workComp + 0x2c));
}

// ==================================================================================
// FUN_1400f8400 — re-key a pending work item between profession buckets.
// ==================================================================================
// The scheduler files every posted work item twice: once in the flat id-map at sched+0x30
// (0x30 nodes keyed by the int work id, payload = the work object), and once inside a
// per-PROFESSION bucket tree at sched+0x120 (0x38 nodes keyed by the u64 profession key,
// each holding its own id-keyed inner tree at +0x08 with a count at +0x10). Citizens of a
// profession only ever see the items in their own bucket.
//
// When a building changes profession — a demolition or upgrade re-keys its WorkPlaceComponent
// to Builder (0x14012f520), a completed building hands it back to its natural profession
// (0x14012f670) — any work item still in flight has to move buckets with it. Without this,
// the item keeps its pre-demolition key and gets offered to the wrong profession's citizens.
//
//   sched  = *(gameWorld+0x118)
//   workId = WorkComponent+0x28 (>= 0 means an item is in flight; -1 means nothing to move)
//   oldKey / newKey = profession keys
//
// Faithful 1:1 port; the erase (ca60) and insert (9a40) helpers maintain the bucket counts
// themselves, which is why retail never touches +0x10 here.
extern "C" void CitizenAI_WorkScheduler_Rekey(uintptr_t sched, uint32_t workId,
                                              uint64_t oldKey, uint64_t newKey) {
    WS_Resolve();
    // [STANDALONE GUARD] Retail is only ever called from a live world.
    if (!sched) return;
    uint8_t* schedB = reinterpret_cast<uint8_t*>(sched);
    const int key = static_cast<int>(workId);

    // 1. The id-map entry (sched+0x30) — its +0x08 payload is what gets re-filed. Retail
    //    bails when the id is unknown, before touching either bucket.
    uint8_t* idNode = *reinterpret_cast<uint8_t**>(schedB + 0x30);
    while (idNode) {
        int nk = *reinterpret_cast<int*>(idNode + 0x00);
        if (nk > key)      idNode = *reinterpret_cast<uint8_t**>(idNode + 0x18);   // left
        else if (key > nk) idNode = *reinterpret_cast<uint8_t**>(idNode + 0x20);   // right
        else break;
    }
    if (!idNode) {
        if (WS_Trace()) {
            uint8_t* root = *reinterpret_cast<uint8_t**>(schedB + 0x30);
            // FAITHFUL NO-OP, not a failure (open D closed 2026-07-26): retail never
            // resets workComp+0x28 when work completes (ctor 0x14012e820 seeds -1 and
            // ec60 is the only writer), so the edf0/f670 completion site calls this
            // re-key with a stale id and retail ALSO finds nothing — the sched+0x30
            // lookup IS the engine's liveness test (same early-out in ead0). Keep the
            // wording neutral so a normal path stops reading like a defect.
            std::fprintf(stderr, "[BuildTrace] Rekey id=%d: no live work item in the "
                         "sched+0x30 id-map (stale workComp+0x28 — faithful no-op; "
                         "root=%p rootId=%d count=%d buckets=%d)\n", key,
                         (void*)root, root ? *reinterpret_cast<int*>(root) : -1,
                         *reinterpret_cast<int*>(schedB + 0x38),
                         *reinterpret_cast<int*>(schedB + 0x128));
            std::fflush(stderr);
        }
        return;
    }

    uint64_t* outer = reinterpret_cast<uint64_t*>(schedB + 0x120);   // {root@0, count@8}

    // 2. The OLD profession's bucket.
    uint8_t* oldBucket = *reinterpret_cast<uint8_t**>(outer);
    while (oldBucket) {
        uint64_t bk = *reinterpret_cast<uint64_t*>(oldBucket + 0x00);
        if (oldKey < bk)      oldBucket = *reinterpret_cast<uint8_t**>(oldBucket + 0x20);
        else if (bk < oldKey) oldBucket = *reinterpret_cast<uint8_t**>(oldBucket + 0x28);
        else break;
    }
    if (!oldBucket) {
        // Retail still materialises the (empty) bucket for oldKey and then returns: with no
        // bucket there is nothing filed to move. cb50's temp node is freed inside cb50.
        void* out = nullptr;
        void* noSubtree[2] = { nullptr, nullptr };
        uint64_t k = oldKey;
        WS_cb50(outer, &out, &k, noSubtree);
        if (WS_Trace()) std::fprintf(stderr, "[BuildTrace] Rekey id=%d: no bucket for oldKey "
                                     "%llx — created it empty, nothing filed to move\n", key,
                                     (unsigned long long)oldKey), std::fflush(stderr);
        return;
    }

    // 3. The item itself, in that bucket's inner id-keyed tree (bucket+0x08).
    uint8_t* item = *reinterpret_cast<uint8_t**>(oldBucket + 0x08);
    while (item) {
        int nk = *reinterpret_cast<int*>(item + 0x00);
        if (nk > key)      item = *reinterpret_cast<uint8_t**>(item + 0x18);
        else if (key > nk) item = *reinterpret_cast<uint8_t**>(item + 0x20);
        else break;
    }
    if (!item) {
        if (WS_Trace()) std::fprintf(stderr, "[BuildTrace] Rekey id=%d: not filed under oldKey "
                                     "%llx (bucket=%p) — nothing to move\n", key,
                                     (unsigned long long)oldKey, (void*)oldBucket), std::fflush(stderr);
        return;
    }

    // 4. Unfile it (ca60 frees the node and decrements bucket+0x10).
    if (!s_ca60) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            std::fprintf(stderr, "[WorkScheduler] Rekey: 0x14001ca60 unresolved — work id %d "
                         "stays filed under its old profession\n", key);
            std::fflush(stderr);
        }
        return;
    }
    s_ca60(oldBucket + 0x08, item);

    // 5. The NEW profession's bucket, created if this is its first item.
    uint8_t* newBucket = nullptr;
    for (uint8_t* p = *reinterpret_cast<uint8_t**>(outer); p; ) {
        uint64_t bk = *reinterpret_cast<uint64_t*>(p + 0x00);
        if (newKey < bk)      p = *reinterpret_cast<uint8_t**>(p + 0x20);
        else if (bk < newKey) p = *reinterpret_cast<uint8_t**>(p + 0x28);
        else { newBucket = p; break; }
    }
    if (!newBucket) {
        void* out = nullptr;
        void* noSubtree[2] = { nullptr, nullptr };
        uint64_t k = newKey;
        WS_cb50(outer, &out, &k, noSubtree);
        newBucket = reinterpret_cast<uint8_t*>(out);
    }
    // [STANDALONE GUARD] Retail derefs the insert result unconditionally; ours can be null
    // if the allocator failed. Dropping the item is bad, but far better than a write to 0x08.
    if (!newBucket) {
        std::fprintf(stderr, "[WorkScheduler] Rekey: could not create bucket for key %llx — "
                     "work id %d dropped\n", static_cast<unsigned long long>(newKey), key);
        std::fflush(stderr);
        return;
    }

    // 6. Re-file under the new key. The pair is {id, pad, payload}; 9a40 reads the u32 at +0
    //    and the u64 at +8, and bumps newBucket+0x10.
    struct { uint32_t id; uint32_t pad; uint64_t payload; } pair;
    pair.id      = workId;
    pair.pad     = 0;
    pair.payload = *reinterpret_cast<uint64_t*>(idNode + 0x08);
    void* outSlot = nullptr;
    if (s_9a40) s_9a40(newBucket + 0x08, &outSlot, reinterpret_cast<uint32_t*>(&pair));

    if (WS_Trace())
        std::fprintf(stderr, "[BuildTrace] WorkScheduler_Rekey id=%d %llx -> %llx\n", key,
                     static_cast<unsigned long long>(oldKey),
                     static_cast<unsigned long long>(newKey));
}

#include "address_registry.h"
extern "C" void RegisterCitizenAIWorkScheduler() {
    RegisterExeAddr(0x1400f88a0, reinterpret_cast<uintptr_t>(&CitizenAI_WorkScheduler_Insert));
    RegisterExeAddr(0x14012ec60, reinterpret_cast<uintptr_t>(&CitizenAI_WorkScheduler_Post));
    RegisterExeAddr(0x1400f8400, reinterpret_cast<uintptr_t>(&CitizenAI_WorkScheduler_Rekey));
}
