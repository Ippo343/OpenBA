#pragma once
// RemoveAnimal — Action to remove an animal, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: RemoveAnimal_Factory (0x140022e70, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> RemoveAnimal
// Smallest action type (only adds entity ref field).

#include <cstdint>
#include <cstddef>

namespace RemoveAnimalAddr {
    constexpr uintptr_t Factory = 0x140022e70;
    constexpr uintptr_t Vtable  = 0x14015ffe0;
}

struct RemoveAnimal {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ffe0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- RemoveAnimal fields ---
    void*    _field20;           // +0x20: zeroed (animal entity ref?)
};

static_assert(sizeof(RemoveAnimal) == 0x28, "RemoveAnimal size mismatch");
static_assert(offsetof(RemoveAnimal, vtable) == 0x00, "");
