#pragma once
// HerbalistWork — Herbalist gathering work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: HerbalistWork_Factory (0x14002a5e0) — inline ctor after AutoPickupWork_Ctor
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HerbalistWork
// Same layout as ForesterWork (different vtable).

#include <cstdint>
#include <cstddef>

namespace HerbalistWorkAddr {
    constexpr uintptr_t Factory = 0x14002a5e0;
    constexpr uintptr_t Vtable  = 0x1401615a0;
}

struct HerbalistWork {
    // --- AutoPickupWork base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401615a0
    uint8_t  _workBase[0x40];    // +0x08: Work/AutoPickupWork fields
    // --- HerbalistWork fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    uint32_t _field58;           // +0x58: zeroed
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(HerbalistWork) == 0x60, "HerbalistWork size mismatch");
static_assert(offsetof(HerbalistWork, vtable) == 0x00, "");
