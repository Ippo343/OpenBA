#pragma once
// RemoveNaturalResource — Action to remove a natural resource, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: RemoveNaturalResource_Factory (0x140022f70, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> RemoveNaturalResource

#include <cstdint>
#include <cstddef>

namespace RemoveNaturalResourceAddr {
    constexpr uintptr_t Factory = 0x140022f70;
    constexpr uintptr_t Vtable  = 0x140160050;
}

struct RemoveNaturalResource {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160050
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- RemoveNaturalResource fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    uint8_t  _pad30[8];          // +0x30: padding
};

static_assert(sizeof(RemoveNaturalResource) == 0x38, "RemoveNaturalResource size mismatch");
static_assert(offsetof(RemoveNaturalResource, vtable) == 0x00, "");
