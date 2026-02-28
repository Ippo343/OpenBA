#pragma once
// TombstoneComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: TombstoneComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a980
//   +0x28 = 0 (qword)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TombstoneComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a980;
}

struct TombstoneComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
};

static_assert(sizeof(TombstoneComponent) == 0x30, "TombstoneComponent size mismatch");
static_assert(offsetof(TombstoneComponent, vtable) == 0x00, "");
static_assert(offsetof(TombstoneComponent, flags) == 0x20, "");
static_assert(offsetof(TombstoneComponent, field28) == 0x28, "");
