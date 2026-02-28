#pragma once
// WellComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: WellComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x14015ae90
//   controllerVtable = 0x14015ae40
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace WellComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015ae90;
    constexpr uintptr_t ControllerVtable  = 0x14015ae40;
}

struct WellComponent {
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

static_assert(sizeof(WellComponent) == 0x48, "WellComponent size mismatch");
static_assert(offsetof(WellComponent, vtable) == 0x00, "");
static_assert(offsetof(WellComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(WellComponent, selfPtr) == 0x40, "");
