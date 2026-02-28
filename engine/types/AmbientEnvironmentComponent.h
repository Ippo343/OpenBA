#pragma once
// AmbientEnvironmentComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AmbientEnvironmentComponent_Ctor
// Parent: Component (direct)
//
// NOTE: Fields +0x28..+0x34 are NOT initialized by ctor (left as Component base state).
// Constructor only initializes:
//   vtable = 0x140158b70
//   +0x38 = 0 (qword), +0x40 = 0 (dword)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AmbientEnvironmentComponentAddr {
    constexpr uintptr_t Vtable = 0x140158b70;
}

struct AmbientEnvironmentComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: not initialized by ctor
    void*    field30;      // +0x30: not initialized by ctor
    void*    field38;      // +0x38: zeroed
    uint32_t field40;      // +0x40: zeroed (dword)
    uint8_t  _pad44[4];    // +0x44: padding to 0x48
};

static_assert(sizeof(AmbientEnvironmentComponent) == 0x48, "AmbientEnvironmentComponent size mismatch");
static_assert(offsetof(AmbientEnvironmentComponent, vtable) == 0x00, "");
static_assert(offsetof(AmbientEnvironmentComponent, flags) == 0x20, "");
static_assert(offsetof(AmbientEnvironmentComponent, field38) == 0x38, "");
static_assert(offsetof(AmbientEnvironmentComponent, field40) == 0x40, "");
