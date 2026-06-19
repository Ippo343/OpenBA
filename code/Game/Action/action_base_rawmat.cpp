// code/Game/Action/action_base_rawmat.cpp
// Wave 3 (Action) — raw-material cluster (batch E) closure-safe leaf sub-helpers.
//
// Shared exe sub-helpers resolved via ResolveAddr(Module::Exe, 0xADDR) from
// code/Game/Action/{consumerawmaterial,pickuprawmaterial,pickupstoredrawmaterial,
// createrawmaterial}.cpp. In standalone they resolve to 0 (silently skipped) unless
// registered. This file provides faithful 1:1 ports of the six that are *leaf*
// (pure array ops, MemAlloc/MemFree, or engine_funcs export-resolved pointers) — i.e.
// whose entire sub-callee closure is already satisfiable. The registration point is
// RegisterActionRawMatBatch() in standalone/reg_action_rawmat.cpp.
//
// DEFERRED (NOT in this file — deeper/unregistered closure):
//   0x14009bd40 CreateRawMaterial FindPlacementCell — intricate spiral cell search
//     (closure-safe: its only exe callee 0x14009f3e0 is registered, but the signed
//     index math + nested cell-validity warrants its own careful pass).
//   0x140129760 CreateRawMaterial PositionEntity — Geometry::SetTransform render-state
//     mutation; depends on Entity_ClearPathQueue (unregistered getter).
//   0x140088ef0 RegisterRawMaterial — depends on unregistered FUN_140013320 +
//     CreateRawMaterial_helper_8fd0.
//
// Each function is a 1:1 port of the decompiled exe helper (Ghidra). Do NOT "improve".
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
// Type-ID globals (resolved lazily from the exe .rdata).
static const uint32_t* s_pTid_RawMatDesc = nullptr;  // DAT_1401c7cb8 (RawMaterialDescription)
static const uint32_t* s_pTid_1c7c80     = nullptr;  // DAT_1401c7c80 (FindComponent_7c80 target)
static bool s_tidInit = false;
static void EnsureTids() {
    if (s_tidInit) return;
    s_pTid_RawMatDesc = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_pTid_1c7c80     = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7c80));
    s_tidInit = true;
}
} // namespace

// ============================================================
// FUN_14012d150 — StorageComponent::SubtractItem(storage, resType, count)
// ============================================================
// Inventory array at storage+0x28 (stride 0x10: [+0]=resType(8), [+8]=count(int)),
// element count at storage+0x30. Walk to the matching resType, subtract count; if the
// slot's count reaches 0, compact the array down one and decrement the count. Pure
// pointer/array ops — leaf.
void ActRm_StorageSubtract_d150(uintptr_t storage, int64_t resType, int count) {
    int n = *reinterpret_cast<int*>(storage + 0x30);
    if (n <= 0) return;
    int64_t* p = *reinterpret_cast<int64_t**>(storage + 0x28);
    int idx = 0;
    while (*p != resType) {
        idx++;
        p += 2;
        if (*reinterpret_cast<int*>(storage + 0x30) <= idx) return;
    }
    uintptr_t base = *reinterpret_cast<uintptr_t*>(storage + 0x28);
    int* cntField = reinterpret_cast<int*>(base + static_cast<uintptr_t>(idx) * 0x10 + 8);
    *cntField -= count;
    if (*reinterpret_cast<int*>(base + static_cast<uintptr_t>(idx) * 0x10 + 8) == 0) {
        int i = idx;
        uintptr_t off = static_cast<uintptr_t>(idx) * 0x10;
        if (i < *reinterpret_cast<int*>(storage + 0x30)) {
            do {
                i++;
                if (i < *reinterpret_cast<int*>(storage + 0x30)) {
                    uint64_t* dst = reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(storage + 0x28) + off);
                    dst[0] = dst[2];   // [idx] = [idx+1]
                    dst[1] = dst[3];
                }
                off += 0x10;
            } while (i < *reinterpret_cast<int*>(storage + 0x30));
        }
        *reinterpret_cast<int*>(storage + 0x30) -= 1;
    }
}

// ============================================================
// FUN_14012d000 — StorageComponent::AddItem(storage, resType, count)
// ============================================================
// If resType already present, add to its count. Else append a {resType, count, 0}
// entry, growing the 16-byte-element array (capacity at storage+0x34; grows to 3x on a
// non-first append) via MemAlloc + copy + MemFree. Leaf (alloc only).
void ActRm_StorageAdd_d000(uintptr_t storage, int64_t resType, int count) {
    int idx = 0;
    int n = *reinterpret_cast<int*>(storage + 0x30);
    if (n > 0) {
        int64_t* p = *reinterpret_cast<int64_t**>(storage + 0x28);
        do {
            if (*p == resType) {
                int64_t* slot = *reinterpret_cast<int64_t**>(storage + 0x28) + static_cast<int64_t>(idx) * 2 + 1;
                *reinterpret_cast<int*>(slot) = *reinterpret_cast<int*>(slot) + count;
                return;
            }
            idx++;
            p += 2;
        } while (idx < *reinterpret_cast<int*>(storage + 0x30));
    }

    int oldN = *reinterpret_cast<int*>(storage + 0x30);
    int newN = oldN + 1;
    if (newN == oldN) return;  // never, but mirror the guard

    void* oldBuf = *reinterpret_cast<void**>(storage + 0x28);
    *reinterpret_cast<int*>(storage + 0x30) = newN;
    if (*reinterpret_cast<int*>(storage + 0x34) < newN) {
        *reinterpret_cast<int*>(storage + 0x34) = newN;
        if (oldN != 0) *reinterpret_cast<int*>(storage + 0x34) = newN * 3;
        void* nb = fn_MemAlloc
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          static_cast<uint64_t>(*reinterpret_cast<int*>(storage + 0x34)) << 4, 8)
            : nullptr;
        *reinterpret_cast<void**>(storage + 0x28) = nb;
    }
    int srcIdx = (oldBuf == *reinterpret_cast<void**>(storage + 0x28)) ? oldN : 0;
    for (int i = srcIdx; i < newN; i++) {
        uint64_t* dst = reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(storage + 0x28) + static_cast<uintptr_t>(i) * 0x10);
        if (!dst) continue;
        if (i < oldN) {
            uint64_t* src = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(oldBuf) + static_cast<uintptr_t>(i) * 0x10);
            dst[0] = src[0];
            dst[1] = src[1];
        } else {
            dst[0] = static_cast<uint64_t>(resType);
            dst[1] = static_cast<uint64_t>(static_cast<uint32_t>(count));  // {count, 0}
        }
    }
    if (oldBuf != *reinterpret_cast<void**>(storage + 0x28) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldBuf);
}

// ============================================================
// FUN_14000da70 — resize a 16-byte-element dynamic array.
// ============================================================
// arr = {ptr@+0, count@+8(int), capacity@+0xc(int)}. Resize to newCount: grow buffer
// (3x on a +1 growth from non-empty) via MemAlloc, copy existing entries, zero new
// entries (the original zeroes only bytes [+0..+12) of each new 16-byte slot), MemFree
// old. Leaf (alloc only).
void ActRm_ArrayResize16_da70(int64_t* arr, int newCount) {
    int oldCount = static_cast<int>(arr[1]);
    if (newCount == oldCount) return;

    void* oldBuf = reinterpret_cast<void*>(arr[0]);
    *reinterpret_cast<int*>(&arr[1]) = newCount;
    int* cap = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(arr) + 0xc);
    if (*cap < newCount) {
        *cap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) *cap = newCount * 3;
        void* nb = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(*cap) << 4, 8) : nullptr;
        arr[0] = reinterpret_cast<int64_t>(nb);
    }
    int srcIdx = (oldBuf == reinterpret_cast<void*>(arr[0])) ? oldCount : 0;
    for (int i = srcIdx; i < newCount; i++) {
        uint64_t* dst = reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(arr[0]) + static_cast<uintptr_t>(i) * 0x10);
        if (!dst) continue;
        if (i < oldCount) {
            uint64_t* src = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(oldBuf) + static_cast<uintptr_t>(i) * 0x10);
            dst[0] = src[0];
            dst[1] = src[1];
        } else {
            dst[0] = 0;
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(dst) + 8) = 0;
        }
    }
    if (oldBuf != reinterpret_cast<void*>(arr[0]) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldBuf);
}

// ============================================================
// FUN_140044810 — append a 4-byte (int) element to a dynamic array.
// ============================================================
// Gated on *(char*)(self+0x28) != 0. arr = {ptr@+0x30, count@+0x38, capacity@+0x3c}.
// Append value: grow buffer (3x on a +1 growth from non-empty) via MemAlloc, copy
// existing ints, write value at the new slot, MemFree old. Leaf (alloc only).
void ActRm_DynArrayPush4_44810(uintptr_t self, uint32_t value) {
    if (*reinterpret_cast<char*>(self + 0x28) == '\0') return;

    int oldN = *reinterpret_cast<int*>(self + 0x38);
    int newN = oldN + 1;
    if (newN == oldN) return;

    void* oldBuf = *reinterpret_cast<void**>(self + 0x30);
    *reinterpret_cast<int*>(self + 0x38) = newN;
    if (*reinterpret_cast<int*>(self + 0x3c) < newN) {
        *reinterpret_cast<int*>(self + 0x3c) = newN;
        if (oldN != 0) *reinterpret_cast<int*>(self + 0x3c) = newN * 3;
        void* nb = fn_MemAlloc
            ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(*reinterpret_cast<int*>(self + 0x3c)) << 2, 4)
            : nullptr;
        *reinterpret_cast<void**>(self + 0x30) = nb;
    }
    int srcIdx = (oldBuf == *reinterpret_cast<void**>(self + 0x30)) ? oldN : 0;
    for (int i = srcIdx; i < newN; i++) {
        uint32_t* dst = reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(self + 0x30) + static_cast<uintptr_t>(i) * 4);
        if (!dst) continue;
        *dst = (i < oldN) ? reinterpret_cast<uint32_t*>(oldBuf)[i] : value;
    }
    if (oldBuf != *reinterpret_cast<void**>(self + 0x30) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldBuf);
}

// ============================================================
// FUN_140088f90 — resource category index from a RawMaterialDescription's flags.
// ============================================================
// int fn(void* /*unused*/, entity) : find the RawMaterialDescription component
// (DAT_1401c7cb8) in entity's component array (ptr@+0x28, count@+0x30) via GetTypeId
// (vtable+0x10) + IsDerivedFrom, then map the first set bit of comp+0x5c to a category
// index (faithful bit->index table). Returns -1 if none. Leaf (engine_funcs only).
int ActRm_GetResourceCategory_88f90(void* /*unused*/, uintptr_t entity) {
    EnsureTids();
    uint32_t typeId = s_pTid_RawMatDesc ? *s_pTid_RawMatDesc : 0;
    void* comp = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x30);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x28);
    for (int idx = 0; idx < n; idx++) {
        void* c = *reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(c) + 0x10))(c);
        if (t == typeId) {
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, typeId)) comp = c;
            break;
        }
    }
    if (!comp) return -1;  // defensive: the original derefs the (always-present) comp directly

    uint32_t f = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(comp) + 0x5c);
    if ((f >> 10) & 1) return 0;
    if ((f >> 11) & 1) return 1;
    if ((f >> 12) & 1) return 2;
    if ((f >> 7)  & 1) return 3;
    if ((f >> 8)  & 1) return 9;
    if ((f >> 9)  & 1) return 4;
    if ((f >> 5)  & 1) return 5;
    if ((f >> 0xd) & 1) return 6;
    if ((f >> 0xe) & 1) return 7;
    if ((f >> 0x11) & 1) return 8;
    if ((f >> 0x10) & 1) return 10;
    if ((f >> 0x12) & 1) return 0xb;
    if ((f >> 0x13) & 1) return 0xc;
    if ((f >> 0x14) & 1) return 0xd;
    if ((f >> 0x15) & 1) return 0xe;
    if ((f >> 0x16) & 1) return 0xf;
    if ((f >> 0x17) & 1) return 0x10;
    if ((f >> 0x18) & 1) return 0x11;
    if ((f >> 0x19) & 1) return 0x12;
    if ((f >> 0x1a) & 1) return 0x13;
    if ((f >> 0x1b) & 1) return 0x14;
    return -1;
}

// ============================================================
// FUN_1400223c0 — FindComponent (member-style) over an IBP array.
// ============================================================
// void* fn(entity, retIBP) : scan the InternalBasePtr array at entity+0x30 (count
// +0x38), CheckPtr each slot, GetTypeId (vtable+0x10) vs DAT_1401c7c80, on the first
// match re-CheckPtr + IsDerivedFrom, then construct retIBP from the found object (or
// null). Returns retIBP. Leaf (engine_funcs only).
void* ActRm_FindComponent_223c0(uintptr_t entity, void* retIBP) {
    EnsureTids();
    uint32_t typeId = s_pTid_1c7c80 ? *s_pTid_1c7c80 : 0;
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int idx = 0; idx < n; idx++) {
        void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
        void* obj = *ibp;
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(obj) + 0x10))(obj);
        if (t == typeId) {
            void** ibp2 = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp2);
            void* o = *ibp2;
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(o, typeId)) found = o;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}
