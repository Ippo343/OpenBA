#pragma once
// SpawnComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: SpawnComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a670
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace SpawnComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a670;
}

struct SpawnComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
};

static_assert(sizeof(SpawnComponent) == 0x38, "SpawnComponent size mismatch");
static_assert(offsetof(SpawnComponent, vtable) == 0x00, "");
static_assert(offsetof(SpawnComponent, flags) == 0x20, "");
static_assert(offsetof(SpawnComponent, field28) == 0x28, "");
static_assert(offsetof(SpawnComponent, field30) == 0x30, "");
