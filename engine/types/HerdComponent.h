#pragma once
// HerdComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: HerdComponent_Ctor (0x140122150)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Manages a herd of animals. Fields include an entity ref and a counter.
// Flags: base | 0x0B
//
// Ctor init pattern:
//   *(u64)(+0x28)=0, *(u64)(+0x38)=0, *(u64)(+0x40)=0,
//   byte(+0x48)=0, *(u32)(+0x4C)=0xFFFFFFFF, *(u64)(+0x50)=0, *(u64)(+0x58)=0
//
// Note: +0x30 is NOT initialized by ctor (gap between 0x28 and 0x38)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace HerdComponentAddr {
    constexpr uintptr_t Ctor       = 0x140122150;
    constexpr uintptr_t Vtable     = 0x140163680;
    constexpr uintptr_t ScalarDtor = 0x140034ea0; // vtable[0]
    constexpr uintptr_t Startup    = 0x140034cb0; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140121f90; // vtable[3]
    constexpr uintptr_t Draw       = 0x140121f40; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140121e20; // vtable[7]
    constexpr uintptr_t Update     = 0x140121bf0; // vtable[9]
    // Serialize = base no-op (0x140118a50)
}

struct HerdComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // HerdComponent fields
    uint64_t data28;       // +0x28: zeroed
    uint64_t data30;       // +0x30: NOT initialized by ctor (inherited or set later)
    uint64_t data38;       // +0x38: zeroed
    uint64_t data40;       // +0x40: zeroed
    uint8_t  data48;       // +0x48: zeroed (byte write)
    uint8_t  _pad49[3];    // +0x49: gap
    int32_t  data4C;       // +0x4C: initialized to -1 (0xFFFFFFFF)
    uint64_t data50;       // +0x50: zeroed
    uint64_t data58;       // +0x58: zeroed
};

static_assert(sizeof(HerdComponent) == 0x60, "HerdComponent size mismatch");
static_assert(offsetof(HerdComponent, vtable) == 0x00, "");
static_assert(offsetof(HerdComponent, next)   == 0x08, "");
static_assert(offsetof(HerdComponent, owner)  == 0x10, "");
static_assert(offsetof(HerdComponent, flags)  == 0x20, "");
static_assert(offsetof(HerdComponent, data28) == 0x28, "");
static_assert(offsetof(HerdComponent, data30) == 0x30, "");
static_assert(offsetof(HerdComponent, data38) == 0x38, "");
static_assert(offsetof(HerdComponent, data40) == 0x40, "");
static_assert(offsetof(HerdComponent, data48) == 0x48, "");
static_assert(offsetof(HerdComponent, data4C) == 0x4C, "");
static_assert(offsetof(HerdComponent, data50) == 0x50, "");
static_assert(offsetof(HerdComponent, data58) == 0x58, "");
