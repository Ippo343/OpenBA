#pragma once
// WildAnimalComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: WildAnimalComponent_Ctor (0x14012e3c0)
//
// Minimal component with no real Update (uses base no-op at 0x140118a80).
// Extends Component (0x28). Only adds a single 8-byte field.
// Flags: base | 0x02

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace WildAnimalComponentAddr {
    constexpr uintptr_t Ctor       = 0x14012e3c0;
    constexpr uintptr_t Vtable     = 0x140164af0;
    constexpr uintptr_t ScalarDtor = 0x140039df0; // vtable[0]
    constexpr uintptr_t Startup    = 0x140039c00; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x14012e300; // vtable[3]
    constexpr uintptr_t OnCreate   = 0x14012e2b0; // vtable[7]
    // Update = base no-op (0x140118a80)
    // Serialize = base no-op (0x140118a50)
    // Draw = base no-op (0x140118820)
}

struct WildAnimalComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // WildAnimalComponent fields
    uint64_t data28;       // +0x28: zeroed by ctor
};

static_assert(sizeof(WildAnimalComponent) == 0x30, "WildAnimalComponent size mismatch");
static_assert(offsetof(WildAnimalComponent, vtable) == 0x00, "");
static_assert(offsetof(WildAnimalComponent, next)   == 0x08, "");
static_assert(offsetof(WildAnimalComponent, owner)  == 0x10, "");
static_assert(offsetof(WildAnimalComponent, flags)  == 0x20, "");
static_assert(offsetof(WildAnimalComponent, data28) == 0x28, "");
