#pragma once
// TerraformComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: TerraformComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a880
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   flags |= 0x3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TerraformComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a880;
}

struct TerraformComponent {
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

static_assert(sizeof(TerraformComponent) == 0x48, "TerraformComponent size mismatch");
static_assert(offsetof(TerraformComponent, vtable) == 0x00, "");
static_assert(offsetof(TerraformComponent, flags) == 0x20, "");
static_assert(offsetof(TerraformComponent, field28) == 0x28, "");
static_assert(offsetof(TerraformComponent, field40) == 0x40, "");
