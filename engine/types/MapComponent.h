#pragma once
// MapComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: MapComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163aa0
//   +0x28 = 0xFFFF (uint16), +0x2A = 0xFFFF (uint16)
//   +0x2C = 0xFFFF (uint16), +0x2E = 0xFFFF (uint16)
//   These look like 4 sentinel tile coordinates (max uint16 = invalid).
//   flags |= 0x3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace MapComponentAddr {
    constexpr uintptr_t Vtable = 0x140163aa0;
}

struct MapComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    uint16_t tileX0;       // +0x28: initialized to 0xFFFF (invalid)
    uint16_t tileY0;       // +0x2A: initialized to 0xFFFF (invalid)
    uint16_t tileX1;       // +0x2C: initialized to 0xFFFF (invalid)
    uint16_t tileY1;       // +0x2E: initialized to 0xFFFF (invalid)
    uint8_t  _pad30[8];    // +0x30: padding to 0x38
};

static_assert(sizeof(MapComponent) == 0x38, "MapComponent size mismatch");
static_assert(offsetof(MapComponent, vtable) == 0x00, "");
static_assert(offsetof(MapComponent, flags) == 0x20, "");
static_assert(offsetof(MapComponent, tileX0) == 0x28, "");
static_assert(offsetof(MapComponent, tileY0) == 0x2A, "");
static_assert(offsetof(MapComponent, tileX1) == 0x2C, "");
static_assert(offsetof(MapComponent, tileY1) == 0x2E, "");
