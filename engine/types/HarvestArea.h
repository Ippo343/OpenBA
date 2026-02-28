#pragma once
// HarvestArea — Crop/field harvest work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: HarvestArea_Factory (0x14002a460) — inline ctor after AutoPickupWork_Ctor
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestArea

#include <cstdint>
#include <cstddef>

namespace HarvestAreaAddr {
    constexpr uintptr_t Factory = 0x14002a460;
    constexpr uintptr_t Vtable  = 0x1401614a0;
}

struct HarvestArea {
    // --- AutoPickupWork base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401614a0
    uint8_t  _workBase[0x40];    // +0x08: Work/AutoPickupWork fields
    // --- HarvestArea fields ---
    void*    _field48;           // +0x48: zeroed
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
};

static_assert(sizeof(HarvestArea) == 0x68, "HarvestArea size mismatch");
static_assert(offsetof(HarvestArea, vtable) == 0x00, "");
