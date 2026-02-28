#pragma once
// GrowthComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: GrowthComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163590
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   +0x48 = 0x3f800000 (float 1.0), +0x4C = 0x3f800000 (float 1.0)
//   flags |= 0xCB

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace GrowthComponentAddr {
    constexpr uintptr_t Vtable = 0x140163590;
}

struct GrowthComponent {
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
    float    growthRate;   // +0x48: initialized to 1.0f
    float    growthScale;  // +0x4C: initialized to 1.0f
};

static_assert(sizeof(GrowthComponent) == 0x50, "GrowthComponent size mismatch");
static_assert(offsetof(GrowthComponent, vtable) == 0x00, "");
static_assert(offsetof(GrowthComponent, flags) == 0x20, "");
static_assert(offsetof(GrowthComponent, field28) == 0x28, "");
static_assert(offsetof(GrowthComponent, growthRate) == 0x48, "");
static_assert(offsetof(GrowthComponent, growthScale) == 0x4C, "");
