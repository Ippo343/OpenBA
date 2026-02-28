#pragma once
// HappinessComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: HappinessComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x30
//
// UNUSUAL: Has own data byte at +0x28, then controller vtable at +0x30 (not +0x28).
// Constructor sets:
//   vtable = 0x1401595d0 (overwrites ControllerComponent base 0x140162b80)
//   +0x28 = 0 (byte)
//   controllerVtable at +0x30 = 0x140159580
//   selfPtr at +0x48 = this
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace HappinessComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401595d0;
    constexpr uintptr_t ControllerVtable  = 0x140159580;
}

struct HappinessComponent {
    // Component base (0x28 bytes)
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // HappinessComponent own data
    uint8_t  happinessFlag;     // +0x28: zeroed (byte)
    uint8_t  _pad29[7];         // +0x29: padding to +0x30

    // Controller sub-object at +0x30 (shifted from normal +0x28)
    void*    controllerVtable;  // +0x30
    void*    controllerData0;   // +0x38
    void*    controllerData1;   // +0x40
    void*    selfPtr;           // +0x48
};

static_assert(sizeof(HappinessComponent) == 0x50, "HappinessComponent size mismatch");
static_assert(offsetof(HappinessComponent, vtable) == 0x00, "");
static_assert(offsetof(HappinessComponent, flags) == 0x20, "");
static_assert(offsetof(HappinessComponent, happinessFlag) == 0x28, "");
static_assert(offsetof(HappinessComponent, controllerVtable) == 0x30, "");
static_assert(offsetof(HappinessComponent, selfPtr) == 0x48, "");
