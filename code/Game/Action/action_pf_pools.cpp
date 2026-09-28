// code/Game/Action/action_pf_pools.cpp
// Pathfinder/Action node-pool grow + dynamic-array resize family.
//
// Faithful 1:1 ports of the A*/pathfinder node-pool free-list grow helpers and the
// associated dynamic-array (vector) resize/copy/free helpers. In standalone these are
#include "Action/exe_bodies.h"
// reached via ResolveAddr(Module::Exe, 0xADDR) from the pathfinder / trade-component /
// build-structure closures; unless registered they resolve to 0 and the call is silently
// skipped. None of this is on the autostart path, so a *wrong* port is undetectable
// latent corruption — every line below is transcribed literally from the Ghidra
// decompile (cross-checked against the disassembly for the intricate index math). Do
// NOT "improve" or simplify.
//
// Allocation matches engine_funcs.h: System::MemoryAllocator::MemAlloc in the exe is the
// static singleton allocator -> fn_MemAlloc(g_pMemAllocSingleton, size, align); MemFree ->
// fn_MemFreeStatic(ptr). The original passes (size, align) to a static MemAlloc; we route
// through fn_MemAlloc with the singleton as the implicit `this`.
//
// CLOSURE NOTES (see RegisterPfPoolsBatch in standalone/reg_pf_pools.cpp):
//   * Pf_VectorResize18Dup_d940 (0x14001d940) calls Pf_IntArrayClone_f9e0 (in-batch) and
//     Pf_FreeArray18_db40 (in-batch) — closure satisfied.
//   * Pf_VectorResize30_e210 (0x14001e210) calls Pf_IntArrayClone_f9e0 (in-batch) and
//     Pf_FreeArray30_e490 (in-batch) — closure satisfied.
//   * Pf_VectorResize60_fa80 (0x14001fa80) calls TradeWork_impl_sub_f5e0 (0x14001f5e0,
//     element copy-ctor; recurses into fdb0/fe60) and TradeWork_impl_sub_f250_sub_fb80
//     (0x14001fb80, 0x60-stride element free) — NEITHER is in this batch and neither is
//     registered. Its closure is therefore NOT satisfied; it is ported here for
//     completeness but is left UNREGISTERED in the registrar (registering a half-null
//     closure would be strictly worse than the skipped-call no-op).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>    // [POOLDIAG] diagnostic reporting
#include <cstdlib>

namespace {
// Exe sub-callees that live in OTHER functions in this closure. Resolved lazily; each is
// registered (the in-batch ones in our own registrar). For 1fa80's two callees we resolve
// them too, but they are not registered (see closure note above), so they stay null in
// standalone and the calls become no-ops — matching the "skip unregistered" policy.
typedef void  (*fn_FreeArray18_t)(void* unused, uintptr_t base, int count);   // 0x14001db40
typedef void  (*fn_FreeArray30_t)(void* unused, uintptr_t base, int count);   // 0x14001e490
typedef int64_t* (*fn_IntArrayClone_t)(int64_t* dst, int64_t* src);           // 0x14000f9e0
typedef void* (*fn_Copy60_t)(void* dst, void* src);                           // 0x14001f5e0
typedef void  (*fn_Free60_t)(void* unused, uintptr_t base, int count);        // 0x14001fb80

static fn_FreeArray18_t   s_fn_FreeArray18  = nullptr;
static fn_FreeArray30_t   s_fn_FreeArray30  = nullptr;
static fn_IntArrayClone_t s_fn_IntClone     = nullptr;
static fn_Copy60_t        s_fn_Copy60       = nullptr;
static fn_Free60_t        s_fn_Free60       = nullptr;
static bool s_fnInit = false;
static void EnsureFns() {
    if (s_fnInit) return;
    s_fn_FreeArray18 = reinterpret_cast<fn_FreeArray18_t>(ResolveAddr(Module::Exe, 0x14001db40));
    s_fn_FreeArray30 = reinterpret_cast<fn_FreeArray30_t>(ResolveAddr(Module::Exe, 0x14001e490));
    s_fn_IntClone    = reinterpret_cast<fn_IntArrayClone_t>(ResolveAddr(Module::Exe, 0x14000f9e0));
    s_fn_Copy60      = reinterpret_cast<fn_Copy60_t>(ResolveAddr(Module::Exe, 0x14001f5e0));
    s_fn_Free60      = reinterpret_cast<fn_Free60_t>(ResolveAddr(Module::Exe, 0x14001fb80));
    s_fnInit = true;
}

// MemAlloc helper mirroring System::MemoryAllocator::MemAlloc(size, align) (static).
static inline void* PfMemAlloc(uint64_t size, uint64_t align) {
    return fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, size, align) : nullptr;
}
} // namespace

// ============================================================
// FUN_14001a9b0 — A* node-pool alloc: grow free-list of 0x18-byte nodes.
// ============================================================
// Pool object: +0x18 chunk-list head, +0x20 free-list head, +0x28 grow-count (doubles each
// grow), +0x2c node stride (init 0x18), +0x30 total nodes, +0x34 free count, +0x38 used
// count. If the free-list (+0x20) is empty, allocate a chunk of (stride*2*growCount) nodes
// (8-aligned, rounded), link it onto the chunk list (+0x18), and push each new node onto the
// free-list. Then pop one node from the free-list, zero its [+8]/[+0x10] tail, write
// param_2's first dword into [+0], return it. Returns null if pop yielded null.
void* Pf_NodePoolAlloc18_a9b0(uintptr_t param_1, uint32_t* param_2) {
    if (*reinterpret_cast<int64_t*>(param_1 + 0x20) == 0) {
        if (*reinterpret_cast<int*>(param_1 + 0x2c) == 0) {
            *reinterpret_cast<int*>(param_1 + 0x2c) = 0x18;
        }
        int iVar2 = *reinterpret_cast<int*>(param_1 + 0x28) * 2;
        *reinterpret_cast<int*>(param_1 + 0x28) = iVar2;
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x2c) * iVar2 + 0xf;
        uint64_t* puVar3 = reinterpret_cast<uint64_t*>(
            PfMemAlloc(static_cast<uint64_t>(static_cast<int>((iVar2 + (iVar2 >> 0x1f & 7U)) & static_cast<int>(0xfffffff8))), 8));
        int iVar4 = 0;
        *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x18);
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x28);
        *reinterpret_cast<uint64_t**>(param_1 + 0x18) = puVar3;
        *reinterpret_cast<int*>(param_1 + 0x30) = *reinterpret_cast<int*>(param_1 + 0x30) + iVar2;
        *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + iVar2;
        puVar3 = puVar3 + 1;
        if (0 < iVar2) {
            do {
                *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + -1;
                *reinterpret_cast<int*>(param_1 + 0x38) = *reinterpret_cast<int*>(param_1 + 0x38) + 1;
                *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x20);
                *reinterpret_cast<uint64_t**>(param_1 + 0x20) = puVar3;
                iVar4 = iVar4 + 1;
                puVar3 = reinterpret_cast<uint64_t*>(
                    reinterpret_cast<uintptr_t>(puVar3) + static_cast<uintptr_t>(*reinterpret_cast<int*>(param_1 + 0x2c)));
            } while (iVar4 < *reinterpret_cast<int*>(param_1 + 0x28));
        }
    }
    uint64_t* puVar3 = *reinterpret_cast<uint64_t**>(param_1 + 0x20);
    *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + 1;
    *reinterpret_cast<int*>(param_1 + 0x38) = *reinterpret_cast<int*>(param_1 + 0x38) + -1;
    *reinterpret_cast<uint64_t*>(param_1 + 0x20) = *puVar3;
    if (puVar3 != nullptr) {
        uint32_t uVar1 = *param_2;
        puVar3[1] = 0;
        puVar3[2] = 0;
        *reinterpret_cast<uint32_t*>(puVar3) = uVar1;
        return puVar3;
    }
    return nullptr;
}

// ============================================================
// FUN_14001d730 — node-pool alloc: grow free-list of 0x30-byte nodes.
// ============================================================
// Same free-list-grow shape as a9b0 but for a pool whose head fields sit at +0x10
// (chunk-list head), +0x18 (free-list head), +0x20 (grow-count, doubles), +0x24 (node
// stride, init 0x30), +0x28 (total), +0x2c (free), +0x30 (used). After popping a node it
// writes a richer record from param_2 (a 3-dword src) + param_3 (a pointer), then zeros the
// remaining fields:
//   node[+0]   = param_2[0] (dword)
//   node[+4]   = param_2[1] (dword)
//   node[+8]   = param_3 (8 bytes, puVar3[2])
//   node[+0x10]= 0, node[+0x18]=0, node[+0x28]=0 (dword)
//   node[+0x20]= param_2[2] (dword, written as *(puVar3+1) low dword via +8 layout)
// Faithfully transcribed from the decompile's exact field writes.
void* Pf_NodePoolAlloc30_d730(uintptr_t param_1, uint32_t* param_2, uint64_t param_3) {
    int iVar4 = 0;
    if (*reinterpret_cast<int64_t*>(param_1 + 0x18) == 0) {
        if (*reinterpret_cast<int*>(param_1 + 0x24) == 0) {
            *reinterpret_cast<int*>(param_1 + 0x24) = 0x30;
        }
        int iVar2 = *reinterpret_cast<int*>(param_1 + 0x20) * 2;
        *reinterpret_cast<int*>(param_1 + 0x20) = iVar2;
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x24) * iVar2 + 0xf;
        uint64_t* puVar3 = reinterpret_cast<uint64_t*>(
            PfMemAlloc(static_cast<uint64_t>(static_cast<int>((iVar2 + (iVar2 >> 0x1f & 7U)) & static_cast<int>(0xfffffff8))), 8));
        *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x10);
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x20);
        *reinterpret_cast<uint64_t**>(param_1 + 0x10) = puVar3;
        *reinterpret_cast<int*>(param_1 + 0x28) = *reinterpret_cast<int*>(param_1 + 0x28) + iVar2;
        *reinterpret_cast<int*>(param_1 + 0x2c) = *reinterpret_cast<int*>(param_1 + 0x2c) + iVar2;
        puVar3 = puVar3 + 1;
        if (0 < iVar2) {
            do {
                *reinterpret_cast<int*>(param_1 + 0x2c) = *reinterpret_cast<int*>(param_1 + 0x2c) + -1;
                *reinterpret_cast<int*>(param_1 + 0x30) = *reinterpret_cast<int*>(param_1 + 0x30) + 1;
                *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x18);
                *reinterpret_cast<uint64_t**>(param_1 + 0x18) = puVar3;
                iVar4 = iVar4 + 1;
                puVar3 = reinterpret_cast<uint64_t*>(
                    reinterpret_cast<uintptr_t>(puVar3) + static_cast<uintptr_t>(*reinterpret_cast<int*>(param_1 + 0x24)));
            } while (iVar4 < *reinterpret_cast<int*>(param_1 + 0x20));
        }
    }
    uint64_t* puVar3 = *reinterpret_cast<uint64_t**>(param_1 + 0x18);
    *reinterpret_cast<int*>(param_1 + 0x2c) = *reinterpret_cast<int*>(param_1 + 0x2c) + 1;
    *reinterpret_cast<int*>(param_1 + 0x30) = *reinterpret_cast<int*>(param_1 + 0x30) + -1;
    *reinterpret_cast<uint64_t*>(param_1 + 0x18) = *puVar3;
    if (puVar3 == nullptr) {
        return nullptr;
    }
    *reinterpret_cast<uint32_t*>(puVar3) = param_2[0];
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(puVar3) + 4) = param_2[1];
    uint32_t uVar1 = param_2[2];
    puVar3[2] = param_3;
    puVar3[3] = 0;
    puVar3[4] = 0;
    *reinterpret_cast<uint32_t*>(puVar3 + 5) = 0;
    *reinterpret_cast<uint32_t*>(puVar3 + 1) = uVar1;
    return puVar3;
}

// ============================================================
// FUN_14001df40 — node-pool alloc: grow worklist free-list (0x18-byte nodes).
// ============================================================
// Identical free-list-grow + pop shape to a9b0 (same +0x18/+0x20/.../+0x38 layout, init
// stride 0x18), but the popped node is initialized from param_2 as a full 8-byte value:
// node[+0]=param_2[0] (8 bytes), node[+8]=0, node[+0x10]=0. Returns null on null pop.
void* Pf_WorklistAlloc_df40(uintptr_t param_1, uint64_t* param_2) {
    if (*reinterpret_cast<int64_t*>(param_1 + 0x20) == 0) {
        if (*reinterpret_cast<int*>(param_1 + 0x2c) == 0) {
            *reinterpret_cast<int*>(param_1 + 0x2c) = 0x18;
        }
        int iVar2 = *reinterpret_cast<int*>(param_1 + 0x28) * 2;
        *reinterpret_cast<int*>(param_1 + 0x28) = iVar2;
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x2c) * iVar2 + 0xf;
        uint64_t* puVar3 = reinterpret_cast<uint64_t*>(
            PfMemAlloc(static_cast<uint64_t>(static_cast<int>((iVar2 + (iVar2 >> 0x1f & 7U)) & static_cast<int>(0xfffffff8))), 8));
        int iVar4 = 0;
        *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x18);
        iVar2 = *reinterpret_cast<int*>(param_1 + 0x28);
        *reinterpret_cast<uint64_t**>(param_1 + 0x18) = puVar3;
        *reinterpret_cast<int*>(param_1 + 0x30) = *reinterpret_cast<int*>(param_1 + 0x30) + iVar2;
        *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + iVar2;
        puVar3 = puVar3 + 1;
        if (0 < iVar2) {
            do {
                *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + -1;
                *reinterpret_cast<int*>(param_1 + 0x38) = *reinterpret_cast<int*>(param_1 + 0x38) + 1;
                *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0x20);
                *reinterpret_cast<uint64_t**>(param_1 + 0x20) = puVar3;
                iVar4 = iVar4 + 1;
                puVar3 = reinterpret_cast<uint64_t*>(
                    reinterpret_cast<uintptr_t>(puVar3) + static_cast<uintptr_t>(*reinterpret_cast<int*>(param_1 + 0x2c)));
            } while (iVar4 < *reinterpret_cast<int*>(param_1 + 0x28));
        }
    }
    uint64_t* puVar3 = *reinterpret_cast<uint64_t**>(param_1 + 0x20);
    *reinterpret_cast<int*>(param_1 + 0x34) = *reinterpret_cast<int*>(param_1 + 0x34) + 1;
    *reinterpret_cast<int*>(param_1 + 0x38) = *reinterpret_cast<int*>(param_1 + 0x38) + -1;
    *reinterpret_cast<uint64_t*>(param_1 + 0x20) = *puVar3;
    if (puVar3 != nullptr) {
        uint64_t uVar1 = *param_2;
        puVar3[1] = 0;
        puVar3[2] = 0;
        *puVar3 = uVar1;
        return puVar3;
    }
    return nullptr;
}

// ============================================================
// FUN_14001d400 — recursive free of an A* BST: return all nodes to the pool free-list.
// ============================================================
// param_1 = pool object; param_2 = subtree root. Post-order: recurse left child (node[+0x18],
// = param_2[3]) then right child (node[+0x20], = param_2[4]), then push this node back onto
// the pool's free-list (+0x18), bumping free/used counters at +0x2c/+0x30. Recursion
// preserved exactly (children freed before the node).
void Pf_BstFreeRecursive_d400(uintptr_t param_1, uint64_t* param_2) {
    if (param_2 != nullptr) {
        Pf_BstFreeRecursive_d400(param_1, reinterpret_cast<uint64_t*>(param_2[3]));
        Pf_BstFreeRecursive_d400(param_1, reinterpret_cast<uint64_t*>(param_2[4]));
        *reinterpret_cast<int*>(param_1 + 0x2c) = *reinterpret_cast<int*>(param_1 + 0x2c) + -1;
        *reinterpret_cast<int*>(param_1 + 0x30) = *reinterpret_cast<int*>(param_1 + 0x30) + 1;
        *param_2 = *reinterpret_cast<uint64_t*>(param_1 + 0x18);
        *reinterpret_cast<uint64_t**>(param_1 + 0x18) = param_2;
    }
}

// ============================================================
// FUN_14001d830 — resize a 0x18-stride dynamic array (3-qword element), copy + zero-fill.
// ============================================================
// arr = {ptr@+0, count@+8(int), cap@+0xc(int)}. Resize to newCount: grow buffer (3x on a +1
// growth from non-empty) via MemAlloc(cap*0x18, 8), copy existing 0x18-byte elements, and for
// new slots write the empty-element pattern: [+0]=0, [+8]=0xffffffff, [+0xc]=0 (8 bytes),
// [+0x14]=0. MemFree old buffer. The new-slot init writes [+8] as a dword 0xffffffff then an
// 8-byte zero at [+0xc] and a dword zero at [+0x14] — transcribed exactly.
void Pf_ArrayResize18_d830(int64_t* param_1, int param_2) {
    int iVar1 = static_cast<int>(param_1[1]);
    if (param_2 != iVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<int*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < param_2) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar4 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x18, 8);
            param_1[0] = reinterpret_cast<int64_t>(pvVar4);
        }
        int iVar3 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            iVar3 = iVar1;
        }
        int64_t lVar6 = iVar3;
        if (lVar6 < param_2) {
            int64_t lVar5 = lVar6 * 0x18;
            do {
                if (lVar6 < iVar1) {
                    uint64_t* puVar7 = reinterpret_cast<uint64_t*>(param_1[0] + lVar5);
                    if (puVar7 != nullptr) {
                        puVar7[0] = *reinterpret_cast<uint64_t*>(lVar5 + reinterpret_cast<int64_t>(pvVar2));
                        puVar7[1] = *reinterpret_cast<uint64_t*>(lVar5 + 8 + reinterpret_cast<int64_t>(pvVar2));
                        puVar7[2] = *reinterpret_cast<uint64_t*>(lVar5 + 0x10 + reinterpret_cast<int64_t>(pvVar2));
                    }
                } else {
                    uint64_t* puVar7 = reinterpret_cast<uint64_t*>(param_1[0] + lVar5);
                    if (puVar7 != nullptr) {
                        puVar7[0] = 0;
                        *reinterpret_cast<uint32_t*>(puVar7 + 1) = 0xffffffff;
                        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(puVar7) + 0xc) = 0;
                        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(puVar7) + 0x14) = 0;
                    }
                }
                lVar6 = lVar6 + 1;
                lVar5 = lVar5 + 0x18;
            } while (lVar6 < param_2);
        }
        if (pvVar2 != reinterpret_cast<void*>(param_1[0])) {
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14001db40 — free a 0x18-stride array's owned pointers (MemFree loop).
// ============================================================
// For each of param_3 elements at param_2 (stride 0x18), the element owns a heap pointer at
// [+8]: MemFree it, then zero [+8] and [+0x10]. param_1 is unused (a count-shaped arg the
// caller passes; preserved in the signature for ABI fidelity). Walks puVar1 = param_2+8,
// stride 3 qwords.
void Pf_FreeArray18_db40(uint64_t param_1, uintptr_t param_2, int param_3) {
    (void)param_1;
    if (0 < param_3) {
        int64_t lVar2 = param_3;
        uint64_t* puVar1 = reinterpret_cast<uint64_t*>(param_2 + 8);
        do {
            if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(*puVar1));
            puVar1[0] = 0;
            puVar1[1] = 0;
            puVar1 = puVar1 + 3;
            lVar2 = lVar2 + -1;
        } while (lVar2 != 0);
    }
}

// ============================================================
// FUN_14001e490 — free a 0x30-stride array's two owned pointers per element.
// ============================================================
// For each of param_3 elements at param_2 (stride 0x30), two owned heap pointers live at
// [+0x20] and [+0x10] (the loop walks lVar1 = param_2+0x2c and uses lVar1-0xc => +0x20 and
// lVar1-0x1c => +0x10). MemFree each, then zero the {ptr, ptr+8} pairs. param_1 unused.
void Pf_FreeArray30_e490(uint64_t param_1, uintptr_t param_2, int param_3) {
    (void)param_1;
    if (0 < param_3) {
        int64_t lVar2 = param_3;
        uintptr_t lVar1 = param_2 + 0x2c;
        do {
            if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(lVar1 + -0xc));
            *reinterpret_cast<uint64_t*>(lVar1 + -0xc) = 0;
            *reinterpret_cast<uint64_t*>(lVar1 + -4) = 0;
            if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(lVar1 + -0x1c));
            *reinterpret_cast<uint64_t*>(lVar1 + -0x1c) = 0;
            *reinterpret_cast<uint64_t*>(lVar1 + -0x14) = 0;
            lVar1 = lVar1 + 0x30;
            lVar2 = lVar2 + -1;
        } while (lVar2 != 0);
    }
}

// ============================================================
// FUN_14001ee40 — resize a vector<0x14-byte element>, copy (no element ownership).
// ============================================================
// arr = {ptr@+0, count@+8(int), cap@+0xc(int)}. Resize to newCount: grow (3x on +1 from
// non-empty) via MemAlloc(cap*0x14, 2), copy existing 0x14-byte elements (16+4 bytes:
// two qwords + a dword) from old buffer or leave new slots untouched (this variant copies
// only the existing range; new slots are NOT written because lVar5<iVar1 guards the copy and
// there is no else branch). MemFree old.
void Pf_VectorResize14_ee40(int64_t* param_1, int param_2) {
    int iVar1 = static_cast<int>(param_1[1]);
    if (param_2 != iVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<int*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < param_2) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar4 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x14, 2);
            param_1[0] = reinterpret_cast<int64_t>(pvVar4);
        }
        int iVar3 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            iVar3 = iVar1;
        }
        int64_t lVar5 = iVar3;
        if (iVar3 < param_2) {
            int64_t lVar6 = lVar5 * 0x14;
            do {
                if ((lVar5 < iVar1)) {
                    uint64_t* puVar7 = reinterpret_cast<uint64_t*>(param_1[0] + lVar6);
                    if (puVar7 != nullptr) {
                        puVar7[0] = *reinterpret_cast<uint64_t*>(lVar6 + reinterpret_cast<int64_t>(pvVar2));
                        puVar7[1] = *reinterpret_cast<uint64_t*>(lVar6 + 8 + reinterpret_cast<int64_t>(pvVar2));
                        *reinterpret_cast<uint32_t*>(puVar7 + 2) = *reinterpret_cast<uint32_t*>(lVar6 + 0x10 + reinterpret_cast<int64_t>(pvVar2));
                    }
                }
                lVar5 = lVar5 + 1;
                lVar6 = lVar6 + 0x14;
            } while (lVar5 < param_2);
        }
        if (pvVar2 != reinterpret_cast<void*>(param_1[0])) {
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14001f8d0 — resize a vector<0x14-byte element>, copy w/ append-fill from param_3.
// ============================================================
// Variant of ee40 that fills new slots (lVar7>=iVar1) from the template element *param_3
// (two qwords + a dword). Copy existing 0x14-byte elements from the old buffer; for slots at
// or past the old count, copy from param_3. MemFree old. The loop counts (param_2-iVar4)
// iterations and walks both the dst (param_1[0]+lVar6) and the old-buffer cursor
// (puVar9 = pvVar2+lVar6) in lockstep.
void Pf_VectorResize14Fill_f8d0(int64_t* param_1, int param_2, uint64_t* param_3) {
    int iVar1 = static_cast<int>(param_1[1]);
    if (param_2 != iVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<int*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < param_2) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar3 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x14, 2);
            param_1[0] = reinterpret_cast<int64_t>(pvVar3);
        }
        int iVar4 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            iVar4 = iVar1;
        }
        int64_t lVar7 = iVar4;
        if (iVar4 < param_2) {
            int64_t lVar6 = lVar7 * 0x14;
            uint64_t uVar10 = static_cast<uint64_t>(static_cast<uint32_t>(param_2 - iVar4));
            uint64_t* puVar9 = reinterpret_cast<uint64_t*>(lVar6 + reinterpret_cast<int64_t>(pvVar2));
            do {
                uint64_t* puVar8 = reinterpret_cast<uint64_t*>(param_1[0] + lVar6);
                if (puVar8 != nullptr) {
                    uint64_t* puVar5 = param_3;
                    if (lVar7 < iVar1) {
                        puVar5 = puVar9;
                    }
                    puVar8[0] = puVar5[0];
                    puVar8[1] = puVar5[1];
                    *reinterpret_cast<uint32_t*>(puVar8 + 2) = *reinterpret_cast<uint32_t*>(puVar5 + 2);
                }
                puVar9 = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(puVar9) + 0x14);
                lVar7 = lVar7 + 1;
                lVar6 = lVar6 + 0x14;
                uVar10 = uVar10 - 1;
            } while (uVar10 != 0);
        }
        if (pvVar2 != reinterpret_cast<void*>(param_1[0])) {
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14001d940 — resize a vector<0x18-byte element> with owned int-array + dup/free.
// ============================================================
// Each 0x18 element = {dword@+0, dword@+4, int-vector@+8 (ptr/count/cap, 0x10 bytes)}. Resize:
// grow (3x on +1 from non-empty) via MemAlloc(cap*0x18, 8). Copy existing or fill from
// template *param_3: copy the two leading dwords, zero the element's int-vector (+8/+0x10),
// then deep-copy the source int-vector via Pf_IntArrayClone_f9e0 (element[+8] <- src[+8]).
// On shrink-in-place (no realloc) free the tail elements' int-vectors via Pf_FreeArray18_db40;
// on realloc, free the whole OLD buffer's int-vectors (inline loop, walks old+8 stride 3
// qwords) then MemFree the old buffer. Closure: f9e0 + db40 (both in-batch).
void Pf_VectorResize18Dup_d940(int64_t* param_1, int param_2, uint8_t* param_3) {
    EnsureFns();
    int iVar1 = static_cast<int>(param_1[1]);
    int64_t lVar8 = iVar1;
    if (param_2 != iVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<int*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < param_2) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar3 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x18, 8);
            param_1[0] = reinterpret_cast<int64_t>(pvVar3);
        }
        int iVar4 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            iVar4 = iVar1;
        }
        int64_t lVar12 = iVar4;
        if (iVar4 < param_2) {
            int64_t lVar11 = lVar12 * 0x18;
            uint8_t* puVar9 = reinterpret_cast<uint8_t*>(reinterpret_cast<int64_t>(pvVar2) + lVar11);
            uint64_t uVar10 = static_cast<uint64_t>(static_cast<uint32_t>(param_2 - iVar4));
            do {
                uint8_t* puVar5 = reinterpret_cast<uint8_t*>(param_1[0] + lVar11);
                if (puVar5 != nullptr) {
                    uint8_t* puVar6 = param_3;
                    if (lVar12 < lVar8) {
                        puVar6 = puVar9;
                    }
                    *reinterpret_cast<uint8_t*>(puVar5) = *reinterpret_cast<uint8_t*>(puVar6);
                    *reinterpret_cast<uint32_t*>(puVar5 + 4) = *reinterpret_cast<uint32_t*>(puVar6 + 4);
                    *reinterpret_cast<int64_t*>(puVar5 + 8) = 0;
                    *reinterpret_cast<uint64_t*>(puVar5 + 0x10) = 0;
                    if (s_fn_IntClone)
                        s_fn_IntClone(reinterpret_cast<int64_t*>(puVar5 + 8),
                                      reinterpret_cast<int64_t*>(puVar6 + 8));
                }
                puVar9 = puVar9 + 0x18;
                lVar12 = lVar12 + 1;
                lVar11 = lVar11 + 0x18;
                uVar10 = uVar10 - 1;
            } while (uVar10 != 0);
        }
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            if (param_2 < iVar1) {
                if (s_fn_FreeArray18)
                    s_fn_FreeArray18(reinterpret_cast<void*>(static_cast<int64_t>(param_2) * 3),
                                     reinterpret_cast<int64_t>(pvVar2) + static_cast<int64_t>(param_2) * 0x18,
                                     iVar1 - param_2);
            }
        } else {
            if (0 < lVar8) {
                uint64_t* puVar7 = reinterpret_cast<uint64_t*>(reinterpret_cast<int64_t>(pvVar2) + 8);
                do {
                    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(*puVar7));
                    puVar7[0] = 0;
                    puVar7[1] = 0;
                    puVar7 = puVar7 + 3;
                    lVar8 = lVar8 + -1;
                } while (lVar8 != 0);
            }
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14001fa80 — resize a vector<0x60-byte element> with element copy-ctor + dtor.
// ============================================================
// 0x60-byte elements requiring a real copy-ctor (TradeWork_impl_sub_f5e0 @0x14001f5e0,
// recurses into fdb0/fe60) and dtor (TradeWork_impl_sub_f250_sub_fb80 @0x14001fb80). Resize:
// grow (3x on +1 from non-empty) via MemAlloc(cap*0x60, 8). For each slot, copy-construct from
// the old buffer (or template *param_3 for appended slots). On shrink-in-place, destruct the
// tail; on realloc, destruct the whole old buffer then MemFree it.
//
// *** CLOSURE NOT SATISFIED: f5e0 and fb80 are NOT in this batch and are unregistered; this
// function is therefore NOT registered in RegisterPfPoolsBatch. Ported for completeness. ***
void Pf_VectorResize60_fa80(int64_t* param_1, int param_2, uint64_t* param_3) {
    EnsureFns();
    int iVar1 = static_cast<int>(param_1[1]);
    if (param_2 != iVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<int*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < param_2) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar3 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x60, 8);
            param_1[0] = reinterpret_cast<int64_t>(pvVar3);
        }
        int iVar6 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            iVar6 = iVar1;
        }
        if (iVar6 < param_2) {
            int64_t lVar7 = static_cast<int64_t>(iVar6) * 0x60;
            do {
                int64_t* plVar4 = reinterpret_cast<int64_t*>(param_1[0] + lVar7);
                if (plVar4 != nullptr) {
                    uint64_t* puVar5 = reinterpret_cast<uint64_t*>(lVar7 + reinterpret_cast<int64_t>(pvVar2));
                    if (iVar1 <= iVar6) {
                        puVar5 = param_3;
                    }
                    if (s_fn_Copy60) s_fn_Copy60(plVar4, puVar5);
                }
                iVar6 = iVar6 + 1;
                lVar7 = lVar7 + 0x60;
            } while (iVar6 < param_2);
        }
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            if (param_2 < iVar1) {
                if (s_fn_Free60)
                    s_fn_Free60(nullptr,
                                static_cast<int64_t>(param_2) * 0x60 + reinterpret_cast<int64_t>(pvVar2),
                                iVar1 - param_2);
            }
        } else {
            if (s_fn_Free60) s_fn_Free60(nullptr, reinterpret_cast<int64_t>(pvVar2), iVar1);
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14001e210 — resize a vector<0x30-byte element> with TWO owned int-arrays.
// ============================================================
// Each 0x30 element = {3 dwords@+0/+4/+8, int-vector@+0x10 (0x10 bytes), int-vector@+0x20
// (0x10 bytes)}. Resize: grow (3x on +1 from non-empty) via MemAlloc(cap*0x30, 8). Copy the
// three leading dwords, zero+deep-copy BOTH int-vectors (element[+0x10] and element[+0x20])
// via Pf_IntArrayClone_f9e0. On shrink-in-place free the tail's two int-arrays via
// Pf_FreeArray30_e490; on realloc free the whole old buffer's int-arrays (e490) then MemFree.
// Closure: f9e0 + e490 (both in-batch).
void Pf_VectorResize30_e210(int64_t* param_1, uint32_t param_2, uint32_t* param_3) {
    EnsureFns();
    uint32_t uVar1 = *reinterpret_cast<uint32_t*>(param_1 + 1);
    if (param_2 != uVar1) {
        void* pvVar2 = reinterpret_cast<void*>(param_1[0]);
        *reinterpret_cast<uint32_t*>(param_1 + 1) = param_2;
        if (*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) < static_cast<int>(param_2)) {
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2;
            if ((param_2 == uVar1 + 1) && (uVar1 != 0)) {
                *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = param_2 * 3;
            }
            void* pvVar3 = PfMemAlloc(
                static_cast<uint64_t>(*reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc)) * 0x30, 8);
            param_1[0] = reinterpret_cast<int64_t>(pvVar3);
        }
        uint64_t uVar5 = 0;
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            uVar5 = static_cast<uint64_t>(uVar1);
        }
        int iVar4 = static_cast<int>(uVar5);
        int64_t lVar8 = iVar4;
        if (iVar4 < static_cast<int>(param_2)) {
            int64_t lVar10 = lVar8 * 0x30;
            uint32_t* puVar9 = reinterpret_cast<uint32_t*>(lVar10 + reinterpret_cast<int64_t>(pvVar2));
            uVar5 = static_cast<uint64_t>(param_2 - static_cast<uint32_t>(iVar4));
            do {
                uint32_t* puVar7 = reinterpret_cast<uint32_t*>(param_1[0] + lVar10);
                if (puVar7 != nullptr) {
                    uint32_t* puVar6 = param_3;
                    if (lVar8 < static_cast<int>(uVar1)) {
                        puVar6 = puVar9;
                    }
                    puVar7[0] = puVar6[0];
                    puVar7[1] = puVar6[1];
                    puVar7[2] = puVar6[2];
                    *reinterpret_cast<int64_t*>(puVar7 + 4) = 0;
                    *reinterpret_cast<uint64_t*>(puVar7 + 6) = 0;
                    if (s_fn_IntClone)
                        s_fn_IntClone(reinterpret_cast<int64_t*>(puVar7 + 4),
                                      reinterpret_cast<int64_t*>(puVar6 + 4));
                    *reinterpret_cast<int64_t*>(puVar7 + 8) = 0;
                    *reinterpret_cast<uint64_t*>(puVar7 + 10) = 0;
                    if (s_fn_IntClone)
                        s_fn_IntClone(reinterpret_cast<int64_t*>(puVar7 + 8),
                                      reinterpret_cast<int64_t*>(puVar6 + 8));
                }
                puVar9 = puVar9 + 0xc;
                lVar8 = lVar8 + 1;
                lVar10 = lVar10 + 0x30;
                uVar5 = uVar5 - 1;
            } while (uVar5 != 0);
            uVar5 = 0;
        }
        if (pvVar2 == reinterpret_cast<void*>(param_1[0])) {
            if (static_cast<int>(param_2) < static_cast<int>(uVar1)) {
                if (s_fn_FreeArray30)
                    s_fn_FreeArray30(reinterpret_cast<void*>(uVar5),
                                     static_cast<int64_t>(static_cast<int>(param_2)) * 0x30 + reinterpret_cast<int64_t>(pvVar2),
                                     uVar1 - param_2);
            }
        } else {
            if (s_fn_FreeArray30)
                s_fn_FreeArray30(reinterpret_cast<void*>(uVar5), reinterpret_cast<int64_t>(pvVar2), uVar1);
            if (fn_MemFreeStatic) fn_MemFreeStatic(pvVar2);
        }
    }
}

// ============================================================
// FUN_14000f9e0 — deep-copy an int-vector (dst <- src), MemAlloc.
// ============================================================
// vec = {ptr@+0, count@+8(int), cap@+0xc(int)}. Frees dst's old buffer, resets ptr/count/cap
// to 0, sets count=src.count, and (if count>0) cap=count + allocates count*4 bytes (align 4),
// then copies each 4-byte element from src. No-op if dst==src. Returns dst. Leaf (alloc/free
// only — used by d940 and e210 above).
int64_t* Pf_IntArrayClone_f9e0(int64_t* param_1, int64_t* param_2) {
    if (param_1 != param_2) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(param_1[0]));
        uint64_t uVar3 = 0;
        param_1[1] = 0;
        param_1[0] = 0;
        int iVar1 = static_cast<int>(param_2[1]);
        *reinterpret_cast<int*>(param_1 + 1) = iVar1;
        if (0 < iVar1) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_1) + 0xc) = iVar1;
            void* pvVar2 = PfMemAlloc(static_cast<uint64_t>(iVar1) << 2, 4);
            param_1[0] = reinterpret_cast<int64_t>(pvVar2);
        }
        uint64_t uVar5 = uVar3;
        if (0 < static_cast<int>(param_1[1])) {
            uint32_t uVar4;
            do {
                if (reinterpret_cast<uint32_t*>(param_1[0] + uVar3) != nullptr) {
                    *reinterpret_cast<uint32_t*>(param_1[0] + uVar3) = *reinterpret_cast<uint32_t*>(uVar3 + param_2[0]);
                }
                uVar4 = static_cast<uint32_t>(uVar5) + 1;
                uVar3 = uVar3 + 4;
                uVar5 = static_cast<uint64_t>(uVar4);
            } while (static_cast<int>(uVar4) < static_cast<int>(param_1[1]));
        }
        return param_1;
    }
    return param_1;
}
