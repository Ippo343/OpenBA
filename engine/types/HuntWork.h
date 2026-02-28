#pragma once
// HuntWork — Hunting work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: HuntWork_Factory (0x14002a690) — inline ctor after AutoPickupWork_Ctor
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HuntWork

#include <cstdint>
#include <cstddef>

namespace HuntWorkAddr {
    constexpr uintptr_t Factory = 0x14002a690;
    constexpr uintptr_t Vtable  = 0x140161620;
}

struct HuntWork {
    // --- AutoPickupWork base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161620
    uint8_t  _workBase[0x40];    // +0x08: Work/AutoPickupWork fields
    // --- HuntWork fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
};

static_assert(sizeof(HuntWork) == 0x58, "HuntWork size mismatch");
static_assert(offsetof(HuntWork, vtable) == 0x00, "");
