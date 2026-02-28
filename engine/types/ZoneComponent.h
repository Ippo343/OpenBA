#pragma once
// ZoneComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: ZoneComponent_Ctor
// Parent: Component (direct, minimal)
//
// Just sets vtable and flags |= 0x3. No additional fields.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ZoneComponentAddr {
    constexpr uintptr_t Vtable = 0x140164e30;
}

struct ZoneComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24
};

static_assert(sizeof(ZoneComponent) == 0x28, "ZoneComponent size mismatch");
static_assert(offsetof(ZoneComponent, vtable) == 0x00, "");
static_assert(offsetof(ZoneComponent, flags) == 0x20, "");
