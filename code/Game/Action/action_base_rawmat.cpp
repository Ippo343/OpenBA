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
#include "Action/exe_bodies.h"
//
// TAIL LANDED (this file): 0x140129760 PositionEntity and 0x140088ef0 RegisterRawMaterial
// — the prior "deferred" note for both was STALE. PositionEntity's getters (0x140004680,
// 0x140003f60 = Entity_ClearPathQueue) were already registered (component-nature / CNR
// waves), so its closure was already satisfied; it just needed the port. RegisterRawMaterial's
// deps are now provided here: 0x140013320 (FindComp), 0x140058fd0 (ArrayUpsert), 0x14000d9d0
// (VectorClone), plus the already-registered 0x140088f90 (GetResourceCategory).
//
// DEFERRED (still NOT registered):
//   0x14009bd40 CreateRawMaterial FindPlacementCell — 260-instruction expanding-ring spiral
//     with heavy decompiler variable-aliasing, on an UNEXERCISED citizen-action path (a wrong
//     port would be undetectable latent corruption). Only dep 0x14009f3e0 is registered, so it
//     is unblockable-but-deferred to its own careful pass with in-game validation.
//
// Each function is a 1:1 port of the decompiled exe helper (Ghidra). Do NOT "improve".

// The runtime type headers come FIRST: they pull runtime/types/volume/Volume.h,
// whose global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()`
// once <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 batch I2.
#include "graphics/Geometry.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "math/Math.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (standalone type-id fallback)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/RawMaterialComponent.h"   // ENDGAME B.3.8 CB3: RawMaterialComponent::TypeId()
#include "Component/RawMaterialDescription.h"
#include "Component/TrackerComponent.h"

namespace {

// Exe sub-helpers / engine constants used by the PositionEntity / FindPlacementCell
// ports below. Resolved lazily (all already registered or Runtime exports).
//   0x140004680 / 0x140003f60 — member-ABI FindComponent (self, outIBP) -> outIBP.
//   0x14009f3e0 — GridToWorld (mapGrid, outVec3, inVec2, char flag, void* ptr).
typedef void* (*fn_FindCompMember_t)(uintptr_t self, void* outIBP);
typedef void  (*fn_GridToWorld_t)(void* mapGrid, float* out3, float* in2, char flag, void* ptr);
static fn_FindCompMember_t s_fn_FindComp_4680 = nullptr;  // 0x140004680
static fn_FindCompMember_t s_fn_FindComp_3f60 = nullptr;  // 0x140003f60
static fn_GridToWorld_t    s_fn_GridToWorld   = nullptr;  // 0x14009f3e0
static const float* s_pHalfCell_172114 = nullptr;  // 0.5f cell-centre offset
static const float* s_pDAT_17213c      = nullptr;  // slope threshold (bd40)
static const float* s_pDAT_172248      = nullptr;  // height-diff tolerance (bd40)
static bool s_fnInit = false;
static void EnsureFns() {
    if (s_fnInit) return;
    s_fn_FindComp_4680 = reinterpret_cast<fn_FindCompMember_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_FindComp_3f60 = reinterpret_cast<fn_FindCompMember_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_GridToWorld   = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_pHalfCell_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pDAT_17213c      = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017213c));
    s_pDAT_172248      = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172248));
    s_fnInit = true;
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
    // DIAGNOSTIC, not a guard — appending a count<=0 entry is legal here in retail too, but
    // it creates a stack that Pf_SpillAllStorage_7570 (action_pf_roots_drop.cpp:250) can
    // never drain: that loop's exit test is the stack COUNT at +0x30 while its work is gated
    // on the entry's own count, so a {resType, 0} entry spins the game forever (observed
    // 2026-07-31 after the first pasture cull, confirmed by gdb: +0x30 == 1, entry count 0).
    // Retail shipped assertions on exactly this invariant — the profile build carries
    // "_count >= 0 && _reserve >= 0 && _count - _reserve >= 0" and "_items[index]._count >= 0"
    // on StorageComponent::Draw — so naming the caller at the moment of the write is
    // retail-shaped. Do NOT turn this into `if (count <= 0) return;`: that would silently
    // drop a real deposit on the default path and hide whichever caller is passing 0.
    if (count <= 0) {
        std::fprintf(stderr, "[StorageAdd] ZERO/NEG COUNT: storage=%p resType=%p count=%d "
                     "stacks(+0x30)=%d capacity(+0x38)=%d — this entry will hang the spill loop\n",
                     reinterpret_cast<void*>(storage), reinterpret_cast<void*>(resType), count,
                     *reinterpret_cast<int*>(storage + 0x30),
                     *reinterpret_cast<int*>(storage + 0x38));
        std::fflush(stderr);
    }
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
    uint32_t typeId = RawMaterialDescription::TypeId();
    void* comp = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x30);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x28);
    for (int idx = 0; idx < n; idx++) {
        void* c = *reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
        uint32_t t = reinterpret_cast<Object::Base*>(c)->Type();   // B.3.x bridge (slot 2)
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
    uint32_t typeId = RawMaterialComponent::TypeId();
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int idx = 0; idx < n; idx++) {
        void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
        void* obj = *ibp;
        uint32_t t = reinterpret_cast<Object::Base*>(obj)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            void** ibp2 = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
            void* o = *ibp2;
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(o, typeId)) found = o;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

// ============================================================
// FUN_140013320 — FindComponent (member-style) over an IBP array, type 0x1401c4b10.
// ============================================================
// Identical to FUN_1400223c0 (above) and the other registered FindComponent accessors,
// differing only by the type-ID global (DWORD_1401c4b10). void* fn(self, retIBP): scan
// the InternalBasePtr array at self+0x30 (count +0x38), CheckPtr each slot, GetTypeId
// (vtable+0x10) vs the target, on first match re-CheckPtr + IsDerivedFrom, construct
// retIBP from the found object (or null). Returns retIBP. Leaf (engine_funcs only).
void* ActRm_FindComp_013320(uintptr_t self, void* retIBP) {
    uint32_t typeId = TrackerComponent::TypeId();
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(self + 0x38);
    uintptr_t base = *reinterpret_cast<uintptr_t*>(self + 0x30);
    for (int idx = 0; idx < n; idx++) {
        void** ibp = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
        void* obj = *ibp;
        uint32_t t = reinterpret_cast<Object::Base*>(obj)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            void** ibp2 = reinterpret_cast<void**>(base + static_cast<uintptr_t>(idx) * 8);
            void* o = *ibp2;
            if (fn_IsDerivedFrom && fn_IsDerivedFrom(o, typeId)) found = o;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

// ============================================================
// FUN_14000d9d0 — deep-copy a 16-byte-element dynamic array (dst <- src).
// ============================================================
// arr = {ptr@+0, count@+8(int), cap@+0xc(int)}. Frees dst's buffer, resets count/cap to
// 0, sets count=cap=src.count, allocs a fresh src.count*0x10 buffer, copies each 16-byte
// element from src. No-op if dst==src. Leaf (alloc only).
void ActRm_VectorClone_d9d0(int64_t* dst, int64_t* src) {
    if (dst == src) return;
    if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(dst[0]));
    dst[1] = 0;          // clears count(+8) and cap(+0xc) together (8-byte write)
    dst[0] = 0;
    int srcN = static_cast<int>(src[1]);
    *reinterpret_cast<int*>(&dst[1]) = srcN;
    if (srcN > 0) {
        *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dst) + 0xc) = srcN;
        void* nb = fn_MemAlloc
            ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(srcN) << 4, 8)
            : nullptr;
        dst[0] = reinterpret_cast<int64_t>(nb);
    }
    int n = *reinterpret_cast<int*>(&dst[1]);
    for (int i = 0; i < n; i++) {
        uint64_t* d = reinterpret_cast<uint64_t*>(static_cast<uintptr_t>(dst[0]) + static_cast<uintptr_t>(i) * 0x10);
        if (!d) continue;
        uintptr_t s = static_cast<uintptr_t>(src[0]) + static_cast<uintptr_t>(i) * 0x10;
        d[0] = *reinterpret_cast<uint64_t*>(s);
        d[1] = *reinterpret_cast<uint64_t*>(s + 8);
    }
}

// ============================================================
// FUN_140058fd0 (CreateRawMaterial_helper_8fd0) — generation-aware {key,value} upsert.
// ============================================================
// self holds a per-generation resource-count cache:
//   +0x28 array ptr (stride 0x10: [+0]=key(8), [+8]=value(int)), +0x30 count, +0x34 cap,
//   +0x38 archive array (snapshot target), +0x48 generation epoch.
// If the world generation (**(int**)(g_pGameWorldSingleton+0x128)) changed, archive the
// current array into +0x38 (d9d0) and reset the live array. Then upsert {key, value}:
// if key present add value, else append (grow 3x on a non-first append). Leaf.
void ActRm_ArrayUpsert_58fd0(uintptr_t self, int64_t key, int value) {
    int gen = **reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x128);
    if (gen != *reinterpret_cast<int*>(self + 0x48)) {
        ActRm_VectorClone_d9d0(reinterpret_cast<int64_t*>(self + 0x38),
                               reinterpret_cast<int64_t*>(self + 0x28));
        if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(self + 0x28));
        *reinterpret_cast<void**>(self + 0x28) = nullptr;
        // Retail stores EIGHT bytes here -- `*(undefined8 *)(param_1 + 0x30) = 0;` --
        // which clears the count (+0x30) AND the capacity (+0x34) in one write. This was
        // ported as a 4-byte int store, so the stale capacity survived the generation
        // reset. That desync is fatal on the next upsert: with cap still, say, 3 the
        // `cap < newN` grow test below is false, so the array pointer (+0x28) stays NULL
        // while the count is committed to 1; the copy loop's `if (!d) continue` then
        // silently drops the entry, and the *following* upsert walks `*p` off a NULL
        // array -- an ACCESS_VIOLATION reading 0 inside the kill->raw-material chain.
        *reinterpret_cast<uint64_t*>(self + 0x30) = 0;
        *reinterpret_cast<int*>(self + 0x48) = gen;
    }

    int n = *reinterpret_cast<int*>(self + 0x30);
    if (n > 0) {
        int64_t* p = *reinterpret_cast<int64_t**>(self + 0x28);
        int idx = 0;
        do {
            if (*p == key) {
                int64_t* slot = *reinterpret_cast<int64_t**>(self + 0x28) + static_cast<int64_t>(idx) * 2 + 1;
                *reinterpret_cast<int*>(slot) = *reinterpret_cast<int*>(slot) + value;
                return;
            }
            idx++;
            p += 2;
        } while (idx < *reinterpret_cast<int*>(self + 0x30));
    }

    int oldN = *reinterpret_cast<int*>(self + 0x30);
    int newN = oldN + 1;
    void* oldBuf = *reinterpret_cast<void**>(self + 0x28);
    *reinterpret_cast<int*>(self + 0x30) = newN;
    if (*reinterpret_cast<int*>(self + 0x34) < newN) {
        *reinterpret_cast<int*>(self + 0x34) = newN;
        if (oldN != 0) *reinterpret_cast<int*>(self + 0x34) = newN * 3;
        void* nb = fn_MemAlloc
            ? fn_MemAlloc(g_pMemAllocSingleton,
                          static_cast<uint64_t>(*reinterpret_cast<int*>(self + 0x34)) << 4, 8)
            : nullptr;
        *reinterpret_cast<void**>(self + 0x28) = nb;
    }
    int srcIdx = (oldBuf == *reinterpret_cast<void**>(self + 0x28)) ? oldN : 0;
    for (int i = srcIdx; i < newN; i++) {
        uint64_t* d = reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(self + 0x28) + static_cast<uintptr_t>(i) * 0x10);
        if (!d) continue;
        if (i < oldN) {
            uint64_t* s = reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(oldBuf) + static_cast<uintptr_t>(i) * 0x10);
            d[0] = s[0];
            d[1] = s[1];
        } else {
            d[0] = static_cast<uint64_t>(key);
            d[1] = static_cast<uint64_t>(static_cast<uint32_t>(value));
        }
    }
    if (oldBuf != *reinterpret_cast<void**>(self + 0x28) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldBuf);
}


void ActRm_RegisterRawMaterial_88ef0(int64_t entityList, void* sourceIBP,
                                     char placeNear, int64_t descEntity, int quantity) {
    int64_t target = entityList;  // lVar3
    if (sourceIBP && *reinterpret_cast<int64_t*>(sourceIBP) != 0 && placeNear != '\0') {
        int64_t srcObj = *reinterpret_cast<int64_t*>(sourceIBP);
        uintptr_t foundIBP[2] = {0, 0};
        ActRm_FindComp_013320(static_cast<uintptr_t>(srcObj), &foundIBP);
        if (foundIBP[0] != 0) {
            ActRm_ArrayUpsert_58fd0(static_cast<uintptr_t>(foundIBP[0]), descEntity, quantity);
            target = static_cast<int64_t>(foundIBP[0]);
        }
    }
    int cat = ActRm_GetResourceCategory_88f90(reinterpret_cast<void*>(target),
                                              static_cast<uintptr_t>(descEntity));
    if (cat >= 0) {
        int* bucket = reinterpret_cast<int*>(static_cast<int64_t>(cat) * 0x20 + 0x10 +
                                             *reinterpret_cast<int64_t*>(entityList + 0x170));
        *bucket += quantity;
    }
}

// ============================================================
// FUN_140129760 (CreateRawMaterial_helper_9760) — PositionEntity.
// ============================================================
// Place the raw-material entity (param_1) at cell `cellIndex` with `rotation`. self =
// param_1+0x10 (an IBP field). Two components are found on *self:
//   A (004680): receives cell col/row (+0x28/+0x2a), 0x10001 (+0x2c), normalized rotation
//       in [0,360) (+0x30).
//   B (003f60): receives a Transform built from (yaw=rotation, pitch=0, offset=worldPos)
//       at +0x148, the raw rotation at +0x168, then Geometry::SetTransform(B+0x28,B+0x148).
// worldPos comes from GridToWorld (f3e0). Instruction-verified against the disassembly
// (RBX reloads to B after the 003f60 IBP resolve). All callees registered/exported.
void ActRm_PositionEntity_9760(int64_t param_1, int cellIndex, float rotation) {
    EnsureFns();
    void** self = reinterpret_cast<void**>(param_1 + 0x10);
    int* mapGrid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    int mapWidth = *mapGrid;
    int col = cellIndex % mapWidth;   // EDI (remainder)
    int row = cellIndex / mapWidth;   // ESI (quotient)

    uintptr_t ibpA[2] = {0, 0};
    if (s_fn_FindComp_4680) s_fn_FindComp_4680(reinterpret_cast<uintptr_t>(*self), &ibpA);

    uintptr_t ibpB[2] = {0, 0};
    if (s_fn_FindComp_3f60) s_fn_FindComp_3f60(reinterpret_cast<uintptr_t>(*self), &ibpB);

    uint8_t* objA = reinterpret_cast<uint8_t*>(ibpA[0]);
    int rot = static_cast<int>(rotation);   // CVTTSS2SI = truncate toward zero
    if (rot < 0) { do { rot += 0x168; } while (rot < 0); }
    // [standalone] objA = the spawned item entity's MapComponent (tile placement). On a freshly
    // cloned raw-material entity in standalone this can be inert/absent; guard the tile writes.
    if (objA) {
        *reinterpret_cast<int16_t*>(objA + 0x28) = static_cast<int16_t>(col);
        *reinterpret_cast<int16_t*>(objA + 0x2a) = static_cast<int16_t>(row);
        *reinterpret_cast<int32_t*>(objA + 0x2c) = 0x10001;
        *reinterpret_cast<int32_t*>(objA + 0x30) = rot;
    }

    // GridToWorld: localPos = ((cellIndex%w) - half, (cellIndex/w) - half, 0).
    float half = static_cast<float>(mapWidth >> 1) - (s_pHalfCell_172114 ? *s_pHalfCell_172114 : 0.5f);
    float localPos[3];
    localPos[0] = static_cast<float>(cellIndex % mapWidth) - half;
    localPos[1] = static_cast<float>(cellIndex / mapWidth) - half;
    localPos[2] = 0.0f;
    float worldPos[3] = {0, 0, 0};
    if (s_fn_GridToWorld) s_fn_GridToWorld(mapGrid, worldPos, localPos, '\0', nullptr);

    uint8_t* objB = reinterpret_cast<uint8_t*>(ibpB[0]);
    // [standalone] objB = the spawned item entity's Geometry/model component (visual transform).
    // Inert/absent on the freshly cloned raw-material entity in standalone → guard the transform
    // writes (this is the inert rendering layer; the economy accounting in the caller is unaffected).
    if (objB) {
        float yaw = rotation, pitch = 0.0f;
        // FromYawPitchOffset out (7 dwords copied to +0x148..+0x160)
        Math::Transform xform = Math::Transform::FromYawPitchOffset(
            yaw, pitch, *reinterpret_cast<const Math::Vector3*>(worldPos));           // B.3.1 bridge
        *reinterpret_cast<Math::Transform*>(objB + 0x148) = xform;                    // B.3.1 bridge
        *reinterpret_cast<float*>(objB + 0x168) = rotation;
        reinterpret_cast<Graphics::Geometry*>(objB + 0x28)
            ->SetTransform(*reinterpret_cast<const Math::Transform*>(objB + 0x148));   // B.3.5 bridge
    }
}
