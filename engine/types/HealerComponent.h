#pragma once
// HealerComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: HealerComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159730
//   controllerVtable = 0x1401596e0
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace HealerComponentAddr {
    constexpr uintptr_t Vtable            = 0x140159730;
    constexpr uintptr_t ControllerVtable  = 0x1401596e0;
}

struct HealerComponent {
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
};

static_assert(sizeof(HealerComponent) == 0x48, "HealerComponent size mismatch");
static_assert(offsetof(HealerComponent, vtable) == 0x00, "");
static_assert(offsetof(HealerComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(HealerComponent, selfPtr) == 0x40, "");
