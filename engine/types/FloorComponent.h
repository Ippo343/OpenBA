#pragma once
// FloorComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: FloorComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163360
//   +0x28 = 0 (byte), +0x2c = -1 (int32)
//   flags |= 0x43

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace FloorComponentAddr {
    constexpr uintptr_t Vtable = 0x140163360;
}

struct FloorComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    uint8_t  floorFlag;    // +0x28: zeroed (byte)
    uint8_t  _pad29[3];    // +0x29: padding
    int32_t  floorId;      // +0x2C: initialized to -1
};

static_assert(sizeof(FloorComponent) == 0x30, "FloorComponent size mismatch");
static_assert(offsetof(FloorComponent, vtable) == 0x00, "");
static_assert(offsetof(FloorComponent, flags) == 0x20, "");
static_assert(offsetof(FloorComponent, floorFlag) == 0x28, "");
static_assert(offsetof(FloorComponent, floorId) == 0x2C, "");
