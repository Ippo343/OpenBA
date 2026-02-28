#pragma once
// Pickup — Item pickup work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: Pickup_Ctor (0x1400f3c00), Pickup_Factory (see vtable)
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// Extends Work directly.

#include <cstdint>
#include <cstddef>

namespace PickupAddr {
    constexpr uintptr_t Ctor    = 0x1400f3c00;
    constexpr uintptr_t Vtable  = 0x140161810;
}

struct Pickup {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161810
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- Pickup fields ---
    uint32_t _field48;           // +0x48: zeroed
    uint8_t  _pad4C[4];          // +0x4C: padding
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
};

static_assert(sizeof(Pickup) == 0x60, "Pickup size mismatch");
static_assert(offsetof(Pickup, vtable) == 0x00, "");
