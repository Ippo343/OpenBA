#pragma once
// TradeWork — Trade post work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: TradeWork_Ctor (0x1400f6da0), TradeWork_Factory (0x14002c620)
// Inherits: Object::Internal -> Work (0x48) -> TradeWork
// Extends Work directly.

#include <cstdint>
#include <cstddef>

namespace TradeWorkAddr {
    constexpr uintptr_t Ctor    = 0x1400f6da0;
    constexpr uintptr_t Factory = 0x14002c620;
    constexpr uintptr_t Vtable  = 0x140161a10;
}

struct TradeWork {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161a10
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- TradeWork fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
};

static_assert(sizeof(TradeWork) == 0x58, "TradeWork size mismatch");
static_assert(offsetof(TradeWork, vtable) == 0x00, "");
