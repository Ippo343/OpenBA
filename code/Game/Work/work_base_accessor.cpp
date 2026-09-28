// code/Game/Work/work_base_accessor.cpp
// Shared Work accessor / small-loop helpers referenced from several Work classes.
//
// Like work_base_reimpl.cpp, these are exe sub-helpers resolved via
// ResolveAddr(Module::Exe, 0xADDR) from code/Game/Work/*.cpp. They resolve to 0
#include "Work/exe_bodies.h"
// (silently skipped) unless registered. This file provides faithful 1:1 reimplementations and
// a single registration point (RegisterWorkAccessorBatch in
// standalone/reg_work_accessor.cpp) so the shared call sites fire real logic.
//
// Wave 2 (Work) — shared accessor sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "engine_funcs.h"
#include "Action/exe_bodies.h"   // B3R: Pf_FloodFillRegion_9c170
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CropFieldComponent.h"       // ENDGAME B.3.9 D5
#include "Component/OrchardComponent.h"         // ENDGAME B.3.9 D5
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 D5
#include "Component/AutoPickupComponent.h"      // ENDGAME B.3.9 CB: 0x1401c3fb0 cell class

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
// ENDGAME B.3.9 D5: same scan, but the type-id is a class's own TypeId() value
// (registered cell, always available -- no resolve/null-gate needed).
static void* FindComponentByTypeIdVal(uintptr_t entity, void* retIBP, uint32_t typeId) {
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
        void* comp = *slot;
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            void* c = *slot;
            found = (Object::IsDerivedFromNullSafe(c, typeId)) ? c : nullptr;
            break;
        }
    }
    new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

// Reimpl of FUN_140005380 — FindComponent (type-ID DAT_1401c7510, CropFieldComponent)
void* WorkAcc_FindComponent_5380(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeIdVal(entity, retIBP, CropFieldComponent::TypeId());
}

// Reimpl of FUN_140005440 — FindComponent (type-ID DWORD_1401c46b0, OrchardComponent)
void* WorkAcc_FindComponent_5440(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeIdVal(entity, retIBP, OrchardComponent::TypeId());
}

// Reimpl of FUN_140022520 — FindComponent<AutoPickupComponent>: retail compares against
// [0x1401c3fb0] at 0x140022563 and IsDerivedFrom-checks with it at 0x1400225bc.
// ENDGAME B.3.9 CB: that cell is unregistered, so the old pointer-based scan was
// skipped outright and this finder answered null for every entity. It now finds the
// component (AutoPickupWork::Cleanup, WorkGrid_AutoPickup_e3db0, CreateNaturalResource
// and CreateRawMaterial reach it through the registered 0x140022520).
void* WorkAcc_FindComponent_22520(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeIdVal(entity, retIBP, AutoPickupComponent::TypeId());
}

// ============================================================
// FindComponent (RawMaterial) — variant with a different container layout
// ============================================================
// Reimpl of FUN_140006330 — same FindComponent shape as the family above, but the
// container offsets differ (count at +0x30, ptr-array base at +0x28) and there are
// NO InternalBasePtr_CheckPtr calls; the match is written straight into *retOut
// (a plain undefined8* slot, not an IBP ctor). Type-ID global DWORD_1401c7cb8.
// Caller signature: void* fn(uintptr_t compList, void* retOut)
void* WorkAcc_FindComponent_6330(uintptr_t compList, void* retOut) {
    const uint32_t typeId = RawMaterialDescription::TypeId();  // 0x1401c7cb8
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(compList + 0x30);
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(compList + 0x28);
    for (int i = 0; i < n; i++) {
        void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
            found = (Object::IsDerivedFromNullSafe(c, typeId)) ? c : nullptr;
            break;
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
// FUN_14009c170(mapGrid, cellIndex, regionCounter) is
// Pf_FloodFillRegion_9c170 (Action/exe_bodies.h), called directly (B3R).
// This body has no caller (the registered 0x14009c480 is Pf_ZoneRefresh_9c480).
// One parameter, as retail (docs/known_port_issues.md 7).
void WorkAcc_ZoneRefresh_9c480(int* mapGrid) {
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
            Pf_FloodFillRegion_9c170(mapGrid, static_cast<uint32_t>(i), static_cast<uint16_t>(regionCounter));
            regionCounter = static_cast<short>(regionCounter + 1);
        }
        off += 0x30;
    }
}
