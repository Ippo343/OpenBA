#pragma once
// Component — ECS base component binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: Component_Ctor (0x1401188b0)
// Base class for all ECS components in the game.
//
// Layout:
//   +0x00: vtable (set by each derived class)
//   +0x08: next (linked list of components on same entity)
//   +0x10: owner (entity that owns this component)
//   +0x18: unknown (8 bytes, zeroed)
//   +0x20: flags (uint32, initially 0x80; derived ctors OR in additional bits)
//   +0x24: padding (4 bytes)
//
// Vtable layout (from CLAUDE.md):
//   +0x00=Dtor, +0x08=GetDescription, +0x10=Startup, +0x18=Shutdown, +0x20=Serialize
//   +0x28=Unknown(event?), +0x30=Draw, +0x38=OnCreate, +0x40=OnRemove
//   +0x48=Update, +0x50=Slot10, +0x58=OnUpdate2

#include <cstdint>
#include <cstddef>

namespace ComponentAddr {
    constexpr uintptr_t Ctor           = 0x1401188b0;
    constexpr uintptr_t ScalarDtor     = 0x140162920; // vtable[0] base
    constexpr uintptr_t Draw_NoOp      = 0x140118820; // base no-op Draw
    constexpr uintptr_t Update_NoOp    = 0x140118a80; // base no-op Update
    constexpr uintptr_t FlagNotify     = 0x1400c3470; // called when flags change & owner != null
}

struct Component {
    void*    vtable;       // +0x00: derived vtable pointer
    void*    next;         // +0x08: next component in entity's component list
    void*    owner;        // +0x10: owning entity (Object::Internal*)
    void*    _unk18;       // +0x18: unknown, zeroed by ctor
    uint32_t flags;        // +0x20: component flags (bit field, base = 0x80)
    uint8_t  _pad24[4];    // +0x24: padding to 8-byte alignment
};

static_assert(sizeof(Component) == 0x28, "Component size mismatch");
static_assert(offsetof(Component, vtable) == 0x00, "");
static_assert(offsetof(Component, next) == 0x08, "");
static_assert(offsetof(Component, owner) == 0x10, "");
static_assert(offsetof(Component, _unk18) == 0x18, "");
static_assert(offsetof(Component, flags) == 0x20, "");
