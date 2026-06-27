// code/Game/Action/region_paircache.cpp
// Pathfinder region pair-cache: ring-buffer initializer + object builder.
//
// Faithful 1:1 port of FUN_14001cca0 @ 0x14001cca0 (the region-pair ring-buffer
// resize/init) plus a builder that replicates the pair-cache creation snippet from
// CreateNaturalResource_impl_sub_e8c0 @ 0x14009e8c0 (the region-system builder).
//
// The pair-cache caches region-to-region path lengths. It is a 0x38-byte object:
//   q[0]  (qword) seq/link
//   q[1]  (qword) Time-value staleness seed   <-- see DEVIATION below
//   q[2]  (ints)  ring head/cap
//   q[3]  (ptr)   ring buffer base            <-- the Vector header lives here (+0x18)
//   q[4]            (count/cap ints of the ring Vector)
//   q[5]  (ptr)   pair-BST root
//   *(int32*)(q+6)  trailing int = 0
// The ring buffer Vector header is the sub-object at byte offset +0x18 (= q+3), whose
// fields are: +0x00 base ptr (qword), +0x08 count (int), +0x0c capacity (int).
// FUN_14001cca0 resizes that ring to 0x20000 entries of stride 0x18 (~6 MB), which is
// faithful/correct.
//
// DEVIATION (one, documented): the original seeds q[1] via
//   System::Time::Time(&tmp, DAT_140172210)
// DAT_140172210 is a raw .rdata constant; ResolveAddr(Module::Exe, dataAddr) returns
// NULL for raw .rdata reads in standalone (see reference_standalone_rdata_null_fallback).
// q[1] is only a staleness timestamp used by cache eviction; for a freshly-built cache
// a 0 seed is safe (nothing in the cache yet to evict). We therefore store 0 and avoid
// the System::Time::Time dependency entirely.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ---- FUN_14001cca0: resize the region-pair ring buffer to 0x20000 entries (stride 0x18).
// param_1 is the ring Vector header: [0]=base ptr, +0x08=count(int), +0x0c=capacity(int).
void Pf_RegionPairRingResize_cca0(int64_t* param_1) {
    int iVar1 = (int)param_1[1];
    if (iVar1 != 0x20000) {
        void* pvVar2 = (void*)*param_1;
        *(uint32_t*)(param_1 + 1) = 0x20000;
        if (*(int*)((int64_t)param_1 + 0xc) < 0x20000) {
            *(uint32_t*)((int64_t)param_1 + 0xc) = 0x20000;
            if (iVar1 == 0x1ffff) {
                *(uint32_t*)((int64_t)param_1 + 0xc) = 0x60000;
            }
            void* pvVar4 = fn_MemAlloc(g_pMemAllocSingleton,
                                       (int64_t)*(int*)((int64_t)param_1 + 0xc) * 0x18, 8);
            *param_1 = (int64_t)pvVar4;
        }
        int iVar3 = 0;
        if (pvVar2 == (void*)*param_1) {
            iVar3 = iVar1;
        }
        int64_t lVar7 = (int64_t)iVar3;
        if (lVar7 < 0x20000) {
            int64_t lVar5 = lVar7 * 0x18;
            do {
                if (lVar7 < iVar1) {
                    uint64_t* puVar6 = (uint64_t*)(*param_1 + lVar5);
                    if (puVar6 != nullptr) {
                        puVar6[0] = *(uint64_t*)(lVar5 + (int64_t)pvVar2);
                        puVar6[1] = *(uint64_t*)(lVar5 + 8 + (int64_t)pvVar2);
                        puVar6[2] = *(uint64_t*)(lVar5 + 0x10 + (int64_t)pvVar2);
                    }
                } else {
                    uint64_t* puVar6 = (uint64_t*)(*param_1 + lVar5);
                    if (puVar6 != nullptr) {
                        puVar6[0] = 0xffffffffffffffffULL;
                        *(uint32_t*)(puVar6 + 1) = 0xffffffff;
                        puVar6[2] = 0;
                    }
                }
                lVar5 = lVar5 + 0x18;
                lVar7 = lVar7 + 1;
            } while (lVar5 < 0x300000);
        }
        if (pvVar2 != (void*)*param_1) {
            fn_MemFreeStatic(pvVar2);
        }
    }
}

// ---- Builder: allocate + init the whole 0x38-byte pair-cache object.
// Replicates the snippet in CreateNaturalResource_impl_sub_e8c0 (0x14009e8c0).
// Returns null on allocation failure (caller then zeroes Map+0x78 fields).
extern "C" void* Pf_RegionPairCache_Build() {
    void* pc = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 4);
    if (pc == nullptr) {
        return nullptr;
    }
    int64_t* q = (int64_t*)pc;
    q[0] = 0;       // [0] seq/link
    q[1] = 0;       // [1] Time-value staleness seed (DEVIATION: 0 instead of Time::Time(DAT_140172210))
    q[2] = 0;       // [2] ring head/cap ints
    q[3] = 0;       // [3] ring buffer base ptr (Vector header at +0x18)
    q[4] = 0;       // [4] ring Vector count/cap
    q[5] = 0;       // [5] pair-BST root
    *(int32_t*)(q + 6) = 0;  // [6] trailing int

    // Init the ring buffer at byte +0x18 (= q+3): resizes to 0x20000 entries of stride 0x18.
    Pf_RegionPairRingResize_cca0((int64_t*)((char*)pc + 0x18));

    return pc;
}
