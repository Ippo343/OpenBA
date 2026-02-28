#pragma once
// InteractComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: InteractComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x1401638b0
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x1

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace InteractComponentAddr {
    constexpr uintptr_t Vtable = 0x1401638b0;
}

struct InteractComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
};

static_assert(sizeof(InteractComponent) == 0x38, "InteractComponent size mismatch");
static_assert(offsetof(InteractComponent, vtable) == 0x00, "");
static_assert(offsetof(InteractComponent, flags) == 0x20, "");
static_assert(offsetof(InteractComponent, field28) == 0x28, "");
