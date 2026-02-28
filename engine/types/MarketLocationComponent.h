#pragma once
// MarketLocationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: MarketLocationComponent_Ctor (0x14004f060)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159dd0 (overwrites ControllerComponent base)
//   controllerVtable = 0x140159d80
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace MarketLocationComponentAddr {
    constexpr uintptr_t Ctor              = 0x14004f060;
    constexpr uintptr_t Vtable            = 0x140159dd0;
    constexpr uintptr_t ControllerVtable  = 0x140159d80;
}

struct MarketLocationComponent {
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

static_assert(sizeof(MarketLocationComponent) == 0x48, "MarketLocationComponent size mismatch");
static_assert(offsetof(MarketLocationComponent, vtable) == 0x00, "");
static_assert(offsetof(MarketLocationComponent, controllerVtable) == 0x28, "");
static_assert(offsetof(MarketLocationComponent, selfPtr) == 0x40, "");
