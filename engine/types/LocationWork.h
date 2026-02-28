#pragma once
// LocationWork — Location-based work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: LocationWork_Ctor (0x1400efc60), LocationWork_Factory (0x14002a810)
// Inherits: Object::Internal -> Work (0x48) -> LocationWork
// Extends Work directly (NOT AutoPickupWork).

#include <cstdint>
#include <cstddef>

namespace LocationWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400efc60;
    constexpr uintptr_t Factory = 0x14002a810;
    constexpr uintptr_t Vtable  = 0x1401616a0;
}

struct LocationWork {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401616a0
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- LocationWork fields ---
    void*    _field48;           // +0x48: zeroed
};

static_assert(sizeof(LocationWork) == 0x50, "LocationWork size mismatch");
static_assert(offsetof(LocationWork, vtable) == 0x00, "");
