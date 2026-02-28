#pragma once
// AmbientEmitterComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AmbientEmitterComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140158a60
//   +0x28 = 0, +0x30 = 0 (dword), +0x38 = 0, +0x40 = 0
//   flags |= 0x43

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AmbientEmitterComponentAddr {
    constexpr uintptr_t Vtable = 0x140158a60;
}

struct AmbientEmitterComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    uint32_t field30;      // +0x30: zeroed (dword)
    uint8_t  _pad34[4];    // +0x34: padding
    void*    field38;      // +0x38: zeroed
    void*    field40;      // +0x40: zeroed
};

static_assert(sizeof(AmbientEmitterComponent) == 0x48, "AmbientEmitterComponent size mismatch");
static_assert(offsetof(AmbientEmitterComponent, vtable) == 0x00, "");
static_assert(offsetof(AmbientEmitterComponent, flags) == 0x20, "");
static_assert(offsetof(AmbientEmitterComponent, field28) == 0x28, "");
static_assert(offsetof(AmbientEmitterComponent, field30) == 0x30, "");
static_assert(offsetof(AmbientEmitterComponent, field40) == 0x40, "");
