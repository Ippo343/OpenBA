#pragma once
// AbsorbResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AbsorbResourceComponent_Ctor (0x140105cc0)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162000 (overwrites ControllerComponent base 0x140162b80)
//   controllerVtable = 0x140161fb0 (overwrites purecall placeholder)
//   selfPtr at +0x40 = this
//   NO flag modification (no flags |= N)
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace AbsorbResourceComponentAddr {
    constexpr uintptr_t Ctor              = 0x140105cc0;
    constexpr uintptr_t Vtable            = 0x140162000;
    constexpr uintptr_t ControllerVtable  = 0x140161fb0;
}

struct AbsorbResourceComponent {
    // ControllerComponent base (0x48 bytes) — no additional fields
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

static_assert(sizeof(AbsorbResourceComponent) == 0x48, "AbsorbResourceComponent size mismatch");
static_assert(offsetof(AbsorbResourceComponent, vtable) == 0x00, "");
static_assert(offsetof(AbsorbResourceComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(AbsorbResourceComponent, selfPtr) == 0x40, "");
