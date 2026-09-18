// code/Game/Action/action_base_hunt.cpp
// Shared / high-reuse exe sub-helpers referenced from the HuntAnimal / RemoveAnimal /
// CompleteRoad action families (and, for 0x1400c3470 / 0x140003110, from many
// Component & Work files as well).
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from
// code/Game/Action/*.cpp (and others). They resolve to 0 (silently skipped) unless
// registered.
// This file provides faithful 1:1 reimplementations and a single registration point
// (RegisterActionHuntBatch in standalone/reg_action_hunt.cpp) so the shared call
// sites fire real logic in the standalone exe.
//
// Wave 3 (Action). Each function is a 1:1 port of the decompiled exe helper; do not
// "improve" behavior.
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// FindComponent family (same shape as work_base_reimpl.cpp FindComponentByTypeId)
// ============================================================
// FUN_140005500 / HuntAnimal_helper_27e0 are byte-for-byte identical except for the
// component type-ID global they compare against. Each scans the entity's component
// list (ptr array at entity+0x30, count at entity+0x38), calls each component's
// GetTypeId (vtable +0x10), and on the first type match runs an IsDerivedFrom check
// before returning the component through an out InternalBasePtr.
//
// Caller signature: void* fn(uintptr_t entity, void* retIBP)
static void* HuntFindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTypeId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

// Lazily-resolved type-ID storage globals (exe .rdata, filled by RegisterType).
static const uint32_t* s_pTid_6f98 = nullptr;  // 0x1401c6f98  (FUN_140005500 — model/character)
static const uint32_t* s_pTid_8000 = nullptr;  // 0x1401c8000  (HuntAnimal_helper_27e0 — herd)
static bool s_huntTidInit = false;
static void EnsureHuntTypeIds() {
    if (s_huntTidInit) return;
    s_pTid_6f98 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c6f98));
    s_pTid_8000 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8000));
    s_huntTidInit = true;
}

// Reimpl of FUN_140005500 (FindComponent, type 0x1401c6f98)
void* ActHunt_FindComponent_5500(uintptr_t entity, void* retIBP) {
    EnsureHuntTypeIds();
    return HuntFindComponentByTypeId(entity, retIBP, s_pTid_6f98);
}

// Reimpl of HuntAnimal_helper_27e0 (FindComponent, type 0x1401c8000)
void* ActHunt_FindComponent_27e0(uintptr_t entity, void* retIBP) {
    EnsureHuntTypeIds();
    return HuntFindComponentByTypeId(entity, retIBP, s_pTid_8000);
}

// ============================================================
// HuntAnimal_helper_2a80 (0x140122a80) — animal-hold-time check.
// ============================================================
// obj = self+0x18; if IsDerivedFrom(obj, DAT_1401c78c8) keep it, else null. Build a
// Time from (desc->holdSeconds @+0x20) * DAT_140172234, and return whether that Time
// (in raw ticks) is <= self->heldTime (raw longlong at self+0x50).
//
// Caller signature: bool fn(void* self)
bool ActHunt_IsAnimalReady_2a80(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* obj = *reinterpret_cast<void**>(s + 0x18);
    const uint32_t* pTid = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c78c8));
    void* desc = nullptr;
    if (pTid && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, *pTid)) desc = obj;

    const float* pScale = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172234));
    float scale = pScale ? *pScale : 0.0f;
    float holdSeconds = desc ? *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(desc) + 0x20) : 0.0f;

    int64_t timeTicks = 0;
    if (fn_Time_Ctor_Float) {
        int64_t tmp = 0;
        fn_Time_Ctor_Float(&tmp, holdSeconds * scale);
        timeTicks = tmp;
    }
    return timeTicks <= *reinterpret_cast<int64_t*>(s + 0x50);
}

// ============================================================
// FUN_140003110 (0x140003110) — dynamic 8-byte-element array set-count + value insert.
// ============================================================
// arr layout: [0]=base ptr, +8=count(int), +0xc=capacity(int). Grows the array to
// param_2 elements (capacity grows: if first growth beyond a non-zero count by exactly
// 1, cap *= 3), then fills [oldCount..param_2) with param_3's value (the rows below the
// old count are copied from the OLD buffer; new rows take *param_3). Frees the old
// buffer if reallocated.
//
// Caller signature: void fn(void* arr, int newCount, void* valuePtr)
void ActHunt_DynArraySetCount_3110(void* arrV, int newCount, void* valuePtr) {
    int64_t* arr = reinterpret_cast<int64_t*>(arrV);
    int oldCount = static_cast<int>(arr[1]);          // (int)param_1[1]
    if (newCount == oldCount) return;

    void* oldBase = reinterpret_cast<void*>(arr[0]);
    *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(arr) + 8) = newCount;   // count = newCount
    int* pCap = reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(arr) + 0xc);
    if (*pCap < newCount) {
        *pCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0)
            *pCap = newCount * 3;
        void* nb = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                                             static_cast<uint64_t>(static_cast<int64_t>(*pCap)) << 3, 8)
                               : nullptr;
        arr[0] = reinterpret_cast<int64_t>(nb);
    }

    int startIdx = (oldBase == reinterpret_cast<void*>(arr[0])) ? oldCount : 0;
    for (int64_t i = startIdx; i < newCount; i++) {
        void** dst = reinterpret_cast<void**>(arr[0] + i * 8);
        if (dst) {
            *dst = nullptr;
            void* srcVal;
            if (i < oldCount)
                srcVal = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(oldBase) + i * 8);
            else
                srcVal = *reinterpret_cast<void**>(valuePtr);
            *dst = srcVal;
        }
    }

    if (oldBase != reinterpret_cast<void*>(arr[0]) && fn_MemFreeStatic)
        fn_MemFreeStatic(oldBase);
}

// ============================================================
// FUN_1400c2ea0 (0x1400c2ea0) — mark entity (and removable components) for removal.
// ============================================================
// If self->flags(+0x40) bit 1 (mask 2) is set: walk component list (base +0x30,
// count +0x38); for each component whose flags(+0x20)>>1 bit is set, invoke its
// vtable +0x38 (OnRemove). Always finally OR flag bit 0x400 into self+0x40.
//
// Caller signature: void fn(void* self)
void ActHunt_MarkEntityRemoval_c2ea0(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uint32_t* pFlags = reinterpret_cast<uint32_t*>(s + 0x40);
    // Diagnostic: the +0x38 removed-from-world broadcast is the engine's dangling-
    // drawable protection (each drawable component RemoveObject's its scene geometry
    // here). Trace which components it visits/skips so a live run pins any leak.
    if ((*reinterpret_cast<uint8_t*>(s + 0x40) & 2) != 0) {
        int count = *reinterpret_cast<int*>(s + 0x38);
        if (count >= 1) {
            uintptr_t base = *reinterpret_cast<uintptr_t*>(s + 0x30);
            for (int64_t i = 0; i < count; i++) {
                void* node = *reinterpret_cast<void**>(base + i * 8);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&node);
                // component flags at node+0x20 (uint), bit 1 (>>1 & 1)
                uint32_t cflags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(node) + 0x20);
                bool visits = ((cflags >> 1) & 1) != 0;
                if (visits) {
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&node);
                    (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<uintptr_t*>(node) + 0x38))(node);
                }
            }
        }
    }
    *pFlags |= 0x400u;
}

// ============================================================
// FUN_1400c3470 (0x1400c3470) — recompute entity component-flags + (de)register in
// the two world flag-buckets. HIGH-REUSE (called from many Component/Work files).
// ============================================================
// oldFlags = self+0x40; reset self+0x40=0; OR in each component's flag-word (uint16 at
// component+0x20) by walking the component list (base +0x30, count +0x38). If self has
// an owning entity (self+0x10 != 0): when bit 4 (mask 4) of the flag word changed, push
// self into the world list at owner+0x88 (count owner+0x90); when bit 0x10 changed,
// push into owner+0x98 (count owner+0xa0). Pushes use FUN_140003110.
//
// Caller signature: void fn(void* self)
void ActHunt_NotifyEntityChanged_c3470(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uint32_t oldFlags = *reinterpret_cast<uint32_t*>(s + 0x40);
    int count = *reinterpret_cast<int*>(s + 0x38);
    *reinterpret_cast<uint32_t*>(s + 0x40) = 0;

    if (count > 0) {
        uintptr_t base = *reinterpret_cast<uintptr_t*>(s + 0x30);
        for (int64_t i = 0; i < count; i++) {
            void** slot = reinterpret_cast<void**>(base + i * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            uint16_t cflags = *reinterpret_cast<uint16_t*>(reinterpret_cast<uintptr_t>(comp) + 0x20);
            *reinterpret_cast<uint32_t*>(s + 0x40) |= static_cast<uint32_t>(cflags);
        }
    }

    uintptr_t owner = *reinterpret_cast<uintptr_t*>(s + 0x10);
    if (owner != 0) {
        uint32_t newFlags = *reinterpret_cast<uint32_t*>(s + 0x40);
        if (((oldFlags ^ newFlags) & 4) != 0) {
            void* ibp = nullptr;
            if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
            ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(owner + 0x88),
                                          *reinterpret_cast<int*>(owner + 0x90) + 1, &ibp);
        }
        if (((oldFlags ^ newFlags) & 0x10) != 0) {
            void* ibp = nullptr;
            if (fn_InternalBasePtr_Ctor) new (&ibp) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(self));   // B.3.3 bridge
            ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(owner + 0x98),
                                          *reinterpret_cast<int*>(owner + 0xa0) + 1, &ibp);
        }
    }
}

// ============================================================
// CompleteRoad_helper_c640 (0x14009c640) — RETIRED duplicate. The registered port is
// Pf_FinalizeRoadTile_9c640 in action_pf_roots_road.cpp (wired by reg_pf_roots_road.cpp),
// which calls its sub-helpers 9f2f0/9e0e0/1a6e0 directly as registered ports.
// ============================================================
