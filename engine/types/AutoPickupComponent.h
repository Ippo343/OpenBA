#pragma once
// AutoPickupComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: AutoPickupComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140158df0
//   +0x28 = 1 (byte, likely "enabled" flag)
//   +0x30 = 0, +0x38 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AutoPickupComponentAddr {
    constexpr uintptr_t Vtable = 0x140158df0;
}

struct AutoPickupComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    uint8_t  isEnabled;    // +0x28: initialized to 1
    uint8_t  _pad29[7];    // +0x29: padding
    void*    field30;      // +0x30: zeroed
    void*    field38;      // +0x38: zeroed
};

static_assert(sizeof(AutoPickupComponent) == 0x40, "AutoPickupComponent size mismatch");
static_assert(offsetof(AutoPickupComponent, vtable) == 0x00, "");
static_assert(offsetof(AutoPickupComponent, flags) == 0x20, "");
static_assert(offsetof(AutoPickupComponent, isEnabled) == 0x28, "");
static_assert(offsetof(AutoPickupComponent, field30) == 0x30, "");
