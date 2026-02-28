#pragma once
// MarketWork — Market trading work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: MarketWork_Ctor (0x1400f1f70), MarketWork_Factory (0x14002a8d0)
// Inherits: Object::Internal -> Work (0x48) -> MarketWork
// Extends Work directly.

#include <cstdint>
#include <cstddef>

namespace MarketWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400f1f70;
    constexpr uintptr_t Factory = 0x14002a8d0;
    constexpr uintptr_t Vtable  = 0x140161710;
}

struct MarketWork {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161710
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- MarketWork fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
};

static_assert(sizeof(MarketWork) == 0x58, "MarketWork size mismatch");
static_assert(offsetof(MarketWork, vtable) == 0x00, "");
