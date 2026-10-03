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

#include <new>
#include "object/TypeInfo.h"
#include "Action/exe_bodies.h"   // B3R: Pf_FloodFillRegion_9c170
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CropFieldComponent.h"       // ENDGAME B.3.9 D5
#include "Component/OrchardComponent.h"         // ENDGAME B.3.9 D5
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.9 D5
#include "Component/AutoPickupComponent.h"      // ENDGAME B.3.9 CB: 0x1401c3fb0 cell class
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>
#include "Component/ComponentDescription.h"
#include "Component/Description.h"

// (FindComponent instances 0x140005380 / 0x140005440 / 0x140022520, the B4 FORWARD
// forwarders to ComponentEntity::FindComponent<CropFieldComponent / OrchardComponent /
// AutoPickupComponent>, had no caller left: deleted, B4 P2 WORK-3.)

// ============================================================
// FindComponent (RawMaterial) — variant with a different container layout
// ============================================================
// Reimpl of FUN_140006330 — same FindComponent shape as the family above, but the
// container offsets differ (count at +0x30, ptr-array base at +0x28) and there are
// NO InternalBasePtr_CheckPtr calls; the match is written straight into *retOut
// (a plain undefined8* slot, not an IBP ctor). Type-ID global DWORD_1401c7cb8.
// Caller signature: void* fn(uintptr_t compList, void* retOut)
// RCX is a ComponentDescription: the list is its sub-descriptions (+0x28 / +0x30).
// LEFT: the six outside callers hand it as an integer and the out slot as void*.
void* WorkAcc_FindComponent_6330(uintptr_t compList, void* retOut) {
    ComponentDescription* desc = reinterpret_cast<ComponentDescription*>(compList);
    const uint32_t typeId = RawMaterialDescription::TypeId();  // 0x1401c7cb8
    RawMaterialDescription* found = nullptr;
    int n = desc->_subDescCount;
    Description** arrBase = desc->_subDescs;
    for (int i = 0; i < n; i++) {
        Description* comp = arrBase[i];
        uint32_t t = comp->Type();
        if (t == typeId) {
            Description* c = arrBase[i];
            found = (Object::IsDerivedFromNullSafe(c, typeId)) ? static_cast<RawMaterialDescription*>(c) : nullptr;
            break;
        }
    }
    *reinterpret_cast<RawMaterialDescription**>(retOut) = found;
    return retOut;
}
