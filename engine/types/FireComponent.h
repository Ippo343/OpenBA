#pragma once
// FireComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: FireComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140159380
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   flags |= 0x8

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace FireComponentAddr {
    constexpr uintptr_t Vtable = 0x140159380;
}

struct FireComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
    void*    field38;      // +0x38: zeroed
    void*    field40;      // +0x40: zeroed
};

static_assert(sizeof(FireComponent) == 0x48, "FireComponent size mismatch");
static_assert(offsetof(FireComponent, vtable) == 0x00, "");
static_assert(offsetof(FireComponent, flags) == 0x20, "");
static_assert(offsetof(FireComponent, field28) == 0x28, "");
static_assert(offsetof(FireComponent, field40) == 0x40, "");
