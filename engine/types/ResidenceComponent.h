#pragma once
// ResidenceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: ResidenceComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401642c0
//   controllerVtable = 0x140164270
//   +0x48 = 0, +0x50 = 0, +0x58 = 0
//   flags |= 0x82

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ResidenceComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401642c0;
    constexpr uintptr_t ControllerVtable  = 0x140164270;
}

struct ResidenceComponent {
    // ControllerComponent base (0x48 bytes)
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24
    void*    controllerVtable;  // +0x28
    void*    controllerData0;   // +0x30
    void*    controllerData1;   // +0x38
    void*    selfPtr;           // +0x40

    // ResidenceComponent own fields
    int64_t  heatRemaining;     // +0x48: System::Time countdown; heat tick 0x140129970
                                //        does -= dt each frame, refills to
                                //        Time(desc->fuelBurnDuration +0x2C) on a burn.
                                //        Warm-house check 0x14012ac30: warm <=> > 0
                                //        (or worldTemp >= desc->heatThreshold +0x28).
    void*    residentSlots;     // +0x50: resident slot array (0x10 stride)
    int32_t  residentSlotCount; // +0x58: slot count (desc capacity +0x20)
    uint8_t  _pad5c[4];         // +0x5c
};

static_assert(sizeof(ResidenceComponent) == 0x60, "ResidenceComponent size mismatch");
static_assert(offsetof(ResidenceComponent, vtable) == 0x00, "");
static_assert(offsetof(ResidenceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(ResidenceComponent, selfPtr) == 0x40, "");
static_assert(offsetof(ResidenceComponent, heatRemaining) == 0x48, "");
static_assert(offsetof(ResidenceComponent, residentSlots) == 0x50, "");
static_assert(offsetof(ResidenceComponent, residentSlotCount) == 0x58, "");
