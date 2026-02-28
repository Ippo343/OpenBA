#pragma once
// ConsumeRawMaterial — Action to consume a raw material, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: ConsumeRawMaterial_Factory (0x1400220f4, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> ConsumeRawMaterial

#include <cstdint>
#include <cstddef>

namespace ConsumeRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022090;
    constexpr uintptr_t Vtable  = 0x14015fc20;
}

struct ConsumeRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fc20
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- ConsumeRawMaterial fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    uint8_t  _pad38[8];          // +0x38: padding
};

static_assert(sizeof(ConsumeRawMaterial) == 0x40, "ConsumeRawMaterial size mismatch");
static_assert(offsetof(ConsumeRawMaterial, vtable) == 0x00, "");
