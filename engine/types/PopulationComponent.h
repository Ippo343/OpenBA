#pragma once
// PopulationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: PopulationComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a360
//   +0x28 = 0 (qword), +0x30 = 0 (dword), +0x38 = 0, +0x40 = 0
//   +0x48 = 0 (dword), +0x50 = 0, +0x58 = 0, +0x60 = 0, +0x68 = 0
//   flags |= 0xCB (Draw+Update+Serialize+Create+bit6+bit7)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PopulationComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a360;
}

struct PopulationComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // PopulationComponent own fields
    void*    field28;      // +0x28: zeroed
    uint32_t field30;      // +0x30: zeroed (dword)
    uint8_t  _pad34[4];    // +0x34: padding
    void*    field38;      // +0x38: zeroed
    void*    field40;      // +0x40: zeroed
    uint32_t field48;      // +0x48: zeroed (dword)
    uint8_t  _pad4C[4];    // +0x4C: padding
    void*    field50;      // +0x50: zeroed
    void*    field58;      // +0x58: zeroed
    void*    field60;      // +0x60: zeroed
    void*    field68;      // +0x68: zeroed
};

static_assert(sizeof(PopulationComponent) == 0x70, "PopulationComponent size mismatch");
static_assert(offsetof(PopulationComponent, vtable) == 0x00, "");
static_assert(offsetof(PopulationComponent, flags) == 0x20, "");
static_assert(offsetof(PopulationComponent, field28) == 0x28, "");
static_assert(offsetof(PopulationComponent, field30) == 0x30, "");
static_assert(offsetof(PopulationComponent, field48) == 0x48, "");
static_assert(offsetof(PopulationComponent, field68) == 0x68, "");
