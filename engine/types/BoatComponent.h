#pragma once
// BoatComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x98.
//
// Decompiled from: BoatComponent_Ctor (0x140046800)
//
// Extends Component (0x28). NO real Update (uses base no-op).
// Has a secondary vtable sub-object at +0x78 (ControllerComponent pattern).
// Two vtables are set: first ControllerComponent base (0x140162b80, then 0x140158f40),
// and secondary at +0x78 (purecall -> 0x140158ef0).
// No flags OR -- uses base flags only (0x80).
//
// Ctor init pattern:
//   *(u64)(+0x28..+0x40) = 0 (zeroed)
//   *(u32)(+0x48) = 0
//   *(u64)(+0x50) = 0, *(u32)(+0x58) = 0
//   *(u64)(+0x60) = 0, *(u64)(+0x68) = 0
//   *(u32)(+0x70) = 0xFFFFFFFF
//   secondary vtable at +0x78, zeroed data at +0x80, +0x88
//   back-pointer to self at +0x90

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace BoatComponentAddr {
    constexpr uintptr_t Ctor             = 0x140046800;
    constexpr uintptr_t Vtable           = 0x140158f40;
    constexpr uintptr_t SecondaryVtable  = 0x140158ef0; // controller sub-vtable at +0x78
    constexpr uintptr_t ScalarDtor       = 0x140003970; // vtable[0]
    constexpr uintptr_t Startup          = 0x1400036c0; // vtable[2]
    constexpr uintptr_t Shutdown         = 0x1400465d0; // vtable[3]
    // Update = base no-op (0x140118a80)
    // Serialize = base no-op (0x140118a50)
    // Draw = base no-op (0x140118820)
}

struct BoatComponent {
    // Component base (0x28 bytes)
    void*    vtable;              // +0x00
    void*    next;                // +0x08
    void*    owner;               // +0x10
    void*    _unk18;              // +0x18
    uint32_t flags;               // +0x20
    uint8_t  _pad24[4];           // +0x24

    // BoatComponent fields
    uint64_t data28;              // +0x28: zeroed
    uint64_t data30;              // +0x30: zeroed
    uint64_t data38;              // +0x38: zeroed
    uint64_t data40;              // +0x40: zeroed
    uint32_t data48;              // +0x48: zeroed
    uint8_t  _pad4C[4];           // +0x4C: gap
    uint64_t data50;              // +0x50: zeroed
    uint32_t data58;              // +0x58: zeroed
    uint8_t  _pad5C[4];           // +0x5C: gap
    uint64_t data60;              // +0x60: zeroed
    uint64_t data68;              // +0x68: zeroed
    int32_t  data70;              // +0x70: initialized to -1 (0xFFFFFFFF)
    uint8_t  _pad74[4];           // +0x74: gap

    // Secondary controller sub-object at +0x78
    void*    controllerVtable;    // +0x78: secondary vtable (purecall -> 0x140158ef0)
    uint64_t controllerData0;     // +0x80: zeroed
    uint64_t controllerData1;     // +0x88: zeroed
    void*    selfPtr;             // +0x90: back-pointer to this component
};

static_assert(sizeof(BoatComponent) == 0x98, "BoatComponent size mismatch");
static_assert(offsetof(BoatComponent, vtable)            == 0x00, "");
static_assert(offsetof(BoatComponent, next)              == 0x08, "");
static_assert(offsetof(BoatComponent, owner)             == 0x10, "");
static_assert(offsetof(BoatComponent, flags)             == 0x20, "");
static_assert(offsetof(BoatComponent, data28)            == 0x28, "");
static_assert(offsetof(BoatComponent, data30)            == 0x30, "");
static_assert(offsetof(BoatComponent, data38)            == 0x38, "");
static_assert(offsetof(BoatComponent, data40)            == 0x40, "");
static_assert(offsetof(BoatComponent, data48)            == 0x48, "");
static_assert(offsetof(BoatComponent, data50)            == 0x50, "");
static_assert(offsetof(BoatComponent, data58)            == 0x58, "");
static_assert(offsetof(BoatComponent, data60)            == 0x60, "");
static_assert(offsetof(BoatComponent, data68)            == 0x68, "");
static_assert(offsetof(BoatComponent, data70)            == 0x70, "");
static_assert(offsetof(BoatComponent, controllerVtable)  == 0x78, "");
static_assert(offsetof(BoatComponent, controllerData0)   == 0x80, "");
static_assert(offsetof(BoatComponent, controllerData1)   == 0x88, "");
static_assert(offsetof(BoatComponent, selfPtr)           == 0x90, "");
