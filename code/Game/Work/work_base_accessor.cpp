// code/Game/Work/work_base_accessor.cpp
// Shared Work accessor / small-loop helpers referenced from several Work classes.
//
// Like work_base_reimpl.cpp, these are exe sub-helpers resolved via
// ResolveAddr(Module::Exe, 0xADDR) from code/Game/Work/*.cpp. They resolve to 0
// (silently skipped) unless registered. This file provides faithful 1:1 reimplementations and
// a single registration point (RegisterWorkAccessorBatch in
// standalone/reg_work_accessor.cpp) so the shared call sites fire real logic.
//
// Wave 2 (Work) — shared accessor sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// FindComponent family (clones of the WorkBase_FindComponent_* set in
// work_base_reimpl.cpp — byte-for-byte identical except for the component
// type-ID global they compare against)
// ============================================================
// Each scans the entity's component list (ptr array at entity+0x30, count at
// entity+0x38), calls each component's GetTypeId (vtable +0x10), and on the first
// type match runs an IsDerivedFrom recheck before returning the component through
// an out InternalBasePtr. Caller signature (all variants):
//   void* fn(uintptr_t entity, void* retIBP)
//
// pTypeId points at the exe's type-ID storage global (a uint32_t filled in by the
// component's RegisterType at startup).
static void* FindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            void* comp = *slot;
            // GetTypeId is component vtable slot +0x10.
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTypeId) {
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
static const uint32_t* s_pTid_5380 = nullptr;  // 0x1401c7510  (CropFieldComponent)
static const uint32_t* s_pTid_5440 = nullptr;  // 0x1401c46b0  (OrchardComponent)
static const uint32_t* s_pTid_22520 = nullptr; // 0x1401c3fb0  (AutoPickupComponent)
static bool s_tidInit = false;

static void EnsureTypeIds() {
    if (s_tidInit) return;
    s_pTid_5380  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7510));
    s_pTid_5440  = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));
    s_pTid_22520 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c3fb0));
    s_tidInit = true;
}

// Reimpl of FUN_140005380 — FindComponent (type-ID DAT_1401c7510, AreaCrop/CropDescription)
void* WorkAcc_FindComponent_5380(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_5380);
}

// Reimpl of FUN_140005440 — FindComponent (type-ID DWORD_1401c46b0, AreaOrchard/OrchardDescription)
void* WorkAcc_FindComponent_5440(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_5440);
}

// Reimpl of FUN_140022520 — FindComponent (type-ID DAT_1401c3fb0, AutoPickupComponent)
void* WorkAcc_FindComponent_22520(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_22520);
}

// ============================================================
// FindComponent (RawMaterial) — variant with a different container layout
// ============================================================
// Reimpl of FUN_140006330 — same FindComponent shape as the family above, but the
// container offsets differ (count at +0x30, ptr-array base at +0x28) and there are
// NO InternalBasePtr_CheckPtr calls; the match is written straight into *retOut
// (a plain undefined8* slot, not an IBP ctor). Type-ID global DWORD_1401c7cb8.
// Caller signature: void* fn(uintptr_t compList, void* retOut)
static const uint32_t* s_pTid_6330 = nullptr;  // 0x1401c7cb8  (RawMaterial)
static bool s_tid6330Init = false;

void* WorkAcc_FindComponent_6330(uintptr_t compList, void* retOut) {
    if (!s_tid6330Init) {
        s_pTid_6330 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
        s_tid6330Init = true;
    }
    void* found = nullptr;
    if (s_pTid_6330) {
        int n = *reinterpret_cast<int*>(compList + 0x30);
        uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(compList + 0x28);
        for (int i = 0; i < n; i++) {
            void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *s_pTid_6330) {
                void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTid_6330)) ? c : nullptr;
                break;
            }
        }
    }
    *reinterpret_cast<void**>(retOut) = found;
    return retOut;
}

// ============================================================
// Connectivity / zone refresh
// ============================================================
// Reimpl of FUN_14009c480 — region/zone refresh pass. When the dirty flag at
// mapGrid+0xa2 is set, clears the per-cell region word (+0x26) across all cells
// (count at mapGrid[0x22], cell stride 0x30, cell array at mapGrid[0x20]), then
// flood-assigns region ids by calling FUN_14009c170 for every cell whose pass
// byte (+0x29) has a bit in mask 0xe1f AND whose region word is still 0.
//
// FUN_14009c170(mapGrid, cellIndex, regionCounter, param4) is an unresolved exe
// sub-dep — invoked guarded (mirrors the work_base_reimpl.cpp exe-subcall pattern).
// Reported as unresolved; NOT registered here.
//
// Caller signature (consumeproduce.cpp / foresterwork.cpp): the function takes 4
// args; only param1 (mapGrid) and param4 (an int) are used by the body, but we
// declare all four so trailing register args are not dropped.
typedef void (*fn_FloodRegion_9c170_t)(int* mapGrid, int cellIndex, short regionCounter, int param4);
static fn_FloodRegion_9c170_t s_fn_FloodRegion_9c170 = nullptr;
static bool s_floodInit = false;

void WorkAcc_ZoneRefresh_9c480(int* mapGrid, uint64_t /*param2*/, uint64_t /*param3*/, int param4) {
    if (!s_floodInit) {
        s_fn_FloodRegion_9c170 = reinterpret_cast<fn_FloodRegion_9c170_t>(
            ResolveAddr(Module::Exe, 0x14009c170));
        s_floodInit = true;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(mapGrid);
    // dirty flag at +0xa2
    if (*reinterpret_cast<char*>(base + 0xa2) == '\0') return;
    *reinterpret_cast<uint8_t*>(base + 0xa2) = 0;

    // int* index: param_1[0x22] == byte +0x88 (cell count), param_1[0x20] == byte +0x80 (cell array)
    int cellCount = *reinterpret_cast<int*>(base + 0x88);
    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(base + 0x80);

    // Pass 1: clear every cell's region word (+0x26), stride 0x30.
    {
        uintptr_t off = 0;
        for (int i = 0; i < cellCount; i++) {
            *reinterpret_cast<uint16_t*>(cellArr + off + 0x26) = 0;
            off += 0x30;
        }
    }

    // Pass 2: flood-assign region ids.
    short regionCounter = 1;
    uintptr_t off = 0;
    for (int i = 0; i < cellCount; i++) {
        uint8_t passByte = *reinterpret_cast<uint8_t*>(cellArr + off + 0x29);
        if (((1u << (passByte & 0x1f)) & 0xe1fu) != 0 &&
            *reinterpret_cast<int16_t*>(cellArr + off + 0x26) == 0) {
            if (s_fn_FloodRegion_9c170) s_fn_FloodRegion_9c170(mapGrid, i, regionCounter, param4);
            regionCounter = static_cast<short>(regionCounter + 1);
        }
        off += 0x30;
    }
}
