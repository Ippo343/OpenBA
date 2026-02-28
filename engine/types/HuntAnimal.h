#pragma once
// HuntAnimal -- Action to hunt an animal, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: HuntAnimal_Factory (0x140022700, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> HuntAnimal
//
// Factory shows:
//   +0x20: zeroed (InternalBasePtr - animal entity)
//   +0x28: zeroed (InternalBasePtr - herd entity)
//   +0x30: zeroed (InternalBasePtr - ??)
//   +0x38: zeroed (bool - hunt started flag)

#include <cstdint>
#include <cstddef>

namespace HuntAnimalAddr {
    constexpr uintptr_t Factory = 0x140022700;
    constexpr uintptr_t Vtable  = 0x14015fde0;
}

struct HuntAnimal {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fde0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- HuntAnimal fields ---
    void*    _field20;           // +0x20: zeroed (animal entity)
    void*    _field28;           // +0x28: zeroed (herd entity)
    void*    _field30;           // +0x30: zeroed
    uint8_t  _field38;           // +0x38: zeroed (hunt started flag)
    uint8_t  _pad39[7];          // +0x39: padding
};

static_assert(sizeof(HuntAnimal) == 0x40, "HuntAnimal size mismatch");
static_assert(offsetof(HuntAnimal, vtable) == 0x00, "");
