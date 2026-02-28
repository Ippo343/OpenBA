#pragma once
// PastureWork — Pasture animal work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: PastureWork_Factory (0x14002bf00) — inline ctor after AutoPickupWork_Ctor
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> PastureWork
// Same layout as HuntWork (different vtable).

#include <cstdint>
#include <cstddef>

namespace PastureWorkAddr {
    constexpr uintptr_t Factory = 0x14002bf00;
    constexpr uintptr_t Vtable  = 0x1401617a0;
}

struct PastureWork {
    // --- AutoPickupWork base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401617a0
    uint8_t  _workBase[0x40];    // +0x08: Work/AutoPickupWork fields
    // --- PastureWork fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
};

static_assert(sizeof(PastureWork) == 0x58, "PastureWork size mismatch");
static_assert(offsetof(PastureWork, vtable) == 0x00, "");
