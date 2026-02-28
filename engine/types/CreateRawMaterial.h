#pragma once
// CreateRawMaterial — Action to create a raw material, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: CreateRawMaterial_Factory (0x140022304, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> CreateRawMaterial

#include <cstdint>
#include <cstddef>

namespace CreateRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022240;
    constexpr uintptr_t Vtable  = 0x14015fd10;
}

struct CreateRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fd10
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- CreateRawMaterial fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint8_t  _pad40[4];          // +0x40: gap
    uint32_t _field44;           // +0x44: zeroed
    uint8_t  _pad48[8];          // +0x48: gap
    uint8_t  _field50;           // +0x50: zeroed
    uint8_t  _pad51[7];          // +0x51: padding
};

static_assert(sizeof(CreateRawMaterial) == 0x58, "CreateRawMaterial size mismatch");
static_assert(offsetof(CreateRawMaterial, vtable) == 0x00, "");
