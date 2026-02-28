#pragma once
// AutoPickupWork — Abstract work base for auto-pickup actions, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48 (base only).
//
// Decompiled from: AutoPickupWork_Ctor (0x1400e4050)
// Inherits: Object::Internal -> Work (vtable 0x140161a80) -> AutoPickupWork (vtable 0x140160fa0)
//
// Work base fields (+0x10 through +0x40) are shared by all Work-derived types.
// AutoPickupWork extends Work, overwriting vtable to 0x140160fa0.
// Many concrete work types extend AutoPickupWork: HarvestArea, HarvestOrchard,
// ForesterWork, HerbalistWork, HuntWork, PastureWork, FishingWork, etc.

#include <cstdint>
#include <cstddef>

namespace WorkAddr {
    constexpr uintptr_t Vtable       = 0x140161a80;
}

namespace AutoPickupWorkAddr {
    constexpr uintptr_t Ctor         = 0x1400e4050;
    constexpr uintptr_t Vtable       = 0x140160fa0;
    constexpr uintptr_t RegisterType = 0x14014dc10;
    constexpr uintptr_t Factory_Abstract = 0x140003730; // shared factory (45 xrefs)
}

// Work base layout (0x48 bytes) — shared by ALL Work-derived types
struct Work {
    void*    vtable;             // +0x00: -> Work vtable (0x140161a80)
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    _field10;           // +0x10: zeroed
    void*    _field18;           // +0x18: zeroed
    int32_t  workplaceId;        // +0x20: -1 sentinel (workplace entity ID)
    uint32_t _field24;           // +0x24: zeroed
    int32_t  _field28;           // +0x28: -1 sentinel
    uint32_t _field2C;           // +0x2C: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    uint32_t _field40;           // +0x40: zeroed
    uint8_t  _pad44[4];          // +0x44: padding
};

static_assert(sizeof(Work) == 0x48, "Work size mismatch");
static_assert(offsetof(Work, vtable) == 0x00, "");
static_assert(offsetof(Work, workplaceId) == 0x20, "");

// AutoPickupWork — extends Work with same size (vtable override only at base level)
// Concrete derived types add fields after +0x48
struct AutoPickupWork : Work {
    // No additional fields at base level — derived types add after +0x48
};

static_assert(sizeof(AutoPickupWork) == 0x48, "AutoPickupWork size mismatch");
