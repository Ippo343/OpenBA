#pragma once
// QuarryComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: QuarryComponent_Ctor
// Parent: Component (direct, minimal)
//
// Just sets vtable and flags |= 0x3. No additional fields.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace QuarryComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a490;
}

struct QuarryComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24
};

static_assert(sizeof(QuarryComponent) == 0x28, "QuarryComponent size mismatch");
static_assert(offsetof(QuarryComponent, vtable) == 0x00, "");
static_assert(offsetof(QuarryComponent, flags) == 0x20, "");
