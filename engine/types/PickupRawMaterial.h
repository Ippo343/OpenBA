#pragma once
// PickupRawMaterial — Action to pick up a raw material, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: PickupRawMaterial_Factory (0x140022c60, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> PickupRawMaterial

#include <cstdint>
#include <cstddef>

namespace PickupRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022c60;
    constexpr uintptr_t Vtable  = 0x14015ff10;
}

struct PickupRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ff10
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- PickupRawMaterial fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
};

static_assert(sizeof(PickupRawMaterial) == 0x30, "PickupRawMaterial size mismatch");
static_assert(offsetof(PickupRawMaterial, vtable) == 0x00, "");
