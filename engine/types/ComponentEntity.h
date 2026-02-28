#pragma once
// ComponentEntity — The ECS entity container, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: ComponentEntity_Ctor (0x1400c3620)
// Base: Object::Internal (0x10 bytes)
// This is the container that holds Component* linked lists.
// Each entity has an ID, flags, and component chain.

#include <cstdint>
#include <cstddef>

namespace ComponentEntityAddr {
    constexpr uintptr_t Ctor    = 0x1400c3620;
    constexpr uintptr_t Vtable  = 0x14015fa78;
}

struct ComponentEntity {
    // --- Object::Internal base (0x10 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fa78
    uint8_t  _base08[8];         // +0x08: Internal base

    // --- ComponentEntity fields ---
    void*    _field10;           // +0x10: zeroed
    int32_t  entityId;           // +0x18: -1 sentinel (unique entity ID)
    uint32_t flags;              // +0x1C: |= 0xFFFFFF (24-bit flag mask)
    uint8_t  _field1F;           // +0x1F: byte, zeroed (part of flags word)
    // NOTE: The ctor does *(uint*)(+0x1c) |= 0xffffff, masking existing top byte
    void*    _field20;           // +0x20: zeroed (component list head?)
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    uint8_t  _pad44[4];          // +0x44: padding
};

static_assert(sizeof(ComponentEntity) == 0x48, "ComponentEntity size mismatch");
static_assert(offsetof(ComponentEntity, vtable) == 0x00, "");
static_assert(offsetof(ComponentEntity, entityId) == 0x18, "");
