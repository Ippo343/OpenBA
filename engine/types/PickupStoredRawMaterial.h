#pragma once
// PickupStoredRawMaterial — Action to pick up a stored raw material, binary-compatible struct.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: PickupStoredRawMaterial_Factory (0x140022d60, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> PickupStoredRawMaterial

#include <cstdint>
#include <cstddef>

namespace PickupStoredRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022d60;
    constexpr uintptr_t Vtable  = 0x14015ff80;
}

struct PickupStoredRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ff80
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- PickupStoredRawMaterial fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint8_t  _pad40[8];          // +0x40: padding
};

static_assert(sizeof(PickupStoredRawMaterial) == 0x48, "PickupStoredRawMaterial size mismatch");
static_assert(offsetof(PickupStoredRawMaterial, vtable) == 0x00, "");
