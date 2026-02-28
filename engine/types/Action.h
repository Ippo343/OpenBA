#pragma once
// Action — State Machine Action base class, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x20 (base only).
//
// Decompiled from: MoveTo_Ctor (0x1400c90e0) — shows Action base init before derived vtable.
// Base: Object::Internal (0x10 bytes: vtable + refcount/flags)
//
// The Action base is the first layer of the state machine action hierarchy:
//   Object::Internal -> Action -> [derived actions]
//
// Vtable at 0x14015faa0 (10 slots, 80 bytes).
// Constructor sequence: Object::Internal::Internal() -> set vtable to Action -> init fields
//
// Field layout from constructor:
//   +0x00: vtable (set to 0x14015faa0 for base, overwritten by derived)
//   +0x08: Object::Internal base fields (8 bytes)
//   +0x10: entity ptr (zeroed)
//   +0x18: status byte (zeroed)
//   +0x19-0x1F: padding

#include <cstdint>
#include <cstddef>

namespace ActionAddr {
    constexpr uintptr_t Vtable = 0x14015faa0;
}

struct Action {
    void*    vtable;       // +0x00
    uint8_t  _base08[8];   // +0x08: Object::Internal base
    void*    entity;       // +0x10: entity pointer (zeroed)
    uint8_t  status;       // +0x18: status byte
    uint8_t  _pad19[7];    // +0x19: padding
};

static_assert(sizeof(Action) == 0x20, "Action size mismatch");
static_assert(offsetof(Action, vtable) == 0x00, "");
static_assert(offsetof(Action, entity) == 0x10, "");
static_assert(offsetof(Action, status) == 0x18, "");
