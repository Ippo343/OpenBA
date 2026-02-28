#pragma once
// DecalComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: DecalComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140162fd0
//   +0x28 = 0 (dword), +0x2C = -1 (int32)
//   +0x30 = 0x3f800000 (float 1.0)
//   flags |= 0xC3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace DecalComponentAddr {
    constexpr uintptr_t Vtable = 0x140162fd0;
}

struct DecalComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    uint32_t decalState;   // +0x28: zeroed (dword)
    int32_t  decalId;      // +0x2C: initialized to -1
    float    decalScale;   // +0x30: initialized to 1.0f
    uint8_t  _pad34[4];    // +0x34: padding to 0x38
};

static_assert(sizeof(DecalComponent) == 0x38, "DecalComponent size mismatch");
static_assert(offsetof(DecalComponent, vtable) == 0x00, "");
static_assert(offsetof(DecalComponent, flags) == 0x20, "");
static_assert(offsetof(DecalComponent, decalState) == 0x28, "");
static_assert(offsetof(DecalComponent, decalId) == 0x2C, "");
static_assert(offsetof(DecalComponent, decalScale) == 0x30, "");
