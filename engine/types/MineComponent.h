#pragma once
// MineComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: MineComponent_Ctor
// Parent: Component (direct, minimal)
//
// Simplest possible component: just sets vtable, no additional fields.
// No flag modification. Size is exactly Component base (0x28).

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace MineComponentAddr {
    constexpr uintptr_t Vtable = 0x140159ed0;
}

struct MineComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24
};

static_assert(sizeof(MineComponent) == 0x28, "MineComponent size mismatch");
static_assert(offsetof(MineComponent, vtable) == 0x00, "");
static_assert(offsetof(MineComponent, flags) == 0x20, "");
