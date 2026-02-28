#pragma once
// NaturalResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Created via: NaturalResourceComponent_Factory (0x140036550)
// Parent: Component (direct)
//
// Factory does: Component_Ctor + set vtable + init fields
//   vtable = 0x140163e10
//   +0x28 = 0x3f800000 (float 1.0)
//   +0x2c = 0x100 (uint16, two packed bytes: 0x00, 0x01)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace NaturalResourceComponentAddr {
    constexpr uintptr_t Factory = 0x140036550;
    constexpr uintptr_t Vtable  = 0x140163e10;
}

struct NaturalResourceComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    float    resourceScale; // +0x28: initialized to 1.0f
    uint16_t resourceFlags; // +0x2C: initialized to 0x100
    uint8_t  _pad2E[2];    // +0x2E: padding to 0x30
};

static_assert(sizeof(NaturalResourceComponent) == 0x30, "NaturalResourceComponent size mismatch");
static_assert(offsetof(NaturalResourceComponent, vtable) == 0x00, "");
static_assert(offsetof(NaturalResourceComponent, flags) == 0x20, "");
static_assert(offsetof(NaturalResourceComponent, resourceScale) == 0x28, "");
static_assert(offsetof(NaturalResourceComponent, resourceFlags) == 0x2C, "");
