#pragma once
// DropOffRawMaterial -- Action to drop off a raw material, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: DropOffRawMaterial_Factory (0x140022600, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> DropOffRawMaterial
//
// Factory shows:
//   +0x20: zeroed (bool flag)
//   +0x24: zeroed (int count)
//   +0x28: zeroed (InternalBasePtr - resource type ID)
//   +0x30: zeroed (InternalBasePtr - target storage)
//   +0x38: zeroed (InternalBasePtr - source entity)

#include <cstdint>
#include <cstddef>

namespace DropOffRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022600;
    constexpr uintptr_t Vtable  = 0x14015fd80;
}

struct DropOffRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fd80
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- DropOffRawMaterial fields ---
    uint8_t  _field20;           // +0x20: zeroed (bool flag)
    uint8_t  _pad21[3];          // +0x21: padding
    int32_t  _field24;           // +0x24: zeroed (count)
    void*    _field28;           // +0x28: zeroed (resource type)
    void*    _field30;           // +0x30: zeroed (target storage)
    void*    _field38;           // +0x38: zeroed (source entity)
};

static_assert(sizeof(DropOffRawMaterial) == 0x40, "DropOffRawMaterial size mismatch");
static_assert(offsetof(DropOffRawMaterial, vtable) == 0x00, "");
