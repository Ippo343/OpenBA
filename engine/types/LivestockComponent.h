#pragma once
// LivestockComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: LivestockComponent_Ctor (0x140123ee0)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Two 8-byte fields zeroed by ctor.
// Flags: base | 0x0B

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace LivestockComponentAddr {
    constexpr uintptr_t Ctor       = 0x140123ee0;
    constexpr uintptr_t Vtable     = 0x1401639b0;
    constexpr uintptr_t ScalarDtor = 0x140035680; // vtable[0]
    constexpr uintptr_t Startup    = 0x140035460; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140123e10; // vtable[3]
    constexpr uintptr_t Draw       = 0x140123c70; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140123c00; // vtable[7]
    constexpr uintptr_t Update     = 0x140123b50; // vtable[9]
    // Serialize = base no-op (0x140118a50)
}

struct LivestockComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // LivestockComponent fields
    uint64_t data28;       // +0x28: zeroed by ctor
    uint64_t data30;       // +0x30: zeroed by ctor
};

static_assert(sizeof(LivestockComponent) == 0x38, "LivestockComponent size mismatch");
static_assert(offsetof(LivestockComponent, vtable) == 0x00, "");
static_assert(offsetof(LivestockComponent, next)   == 0x08, "");
static_assert(offsetof(LivestockComponent, owner)  == 0x10, "");
static_assert(offsetof(LivestockComponent, flags)  == 0x20, "");
static_assert(offsetof(LivestockComponent, data28) == 0x28, "");
static_assert(offsetof(LivestockComponent, data30) == 0x30, "");
