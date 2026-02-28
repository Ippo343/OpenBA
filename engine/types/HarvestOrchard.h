#pragma once
// HarvestOrchard — Orchard harvest work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: HarvestOrchard_Factory (0x14002a520) — inline ctor after AutoPickupWork_Ctor
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestOrchard
// Same field layout as HarvestArea (different vtable).

#include <cstdint>
#include <cstddef>

namespace HarvestOrchardAddr {
    constexpr uintptr_t Factory = 0x14002a520;
    constexpr uintptr_t Vtable  = 0x140161520;
}

struct HarvestOrchard {
    // --- AutoPickupWork base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161520
    uint8_t  _workBase[0x40];    // +0x08: Work/AutoPickupWork fields
    // --- HarvestOrchard fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
};

static_assert(sizeof(HarvestOrchard) == 0x68, "HarvestOrchard size mismatch");
static_assert(offsetof(HarvestOrchard, vtable) == 0x00, "");
