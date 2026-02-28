#pragma once
// CharacterComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x80.
//
// Decompiled from: CharacterComponent_Ctor (0x140109d20)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Handles character rendering/animation state (position, mesh, animation).
// Flags: base | 0x8F
//
// Ctor init pattern:
//   byte(+0x28)=0, *(u64)(+0x2C)=0, *(u64)(+0x34)=0,
//   *(u32)(+0x40)=0xFFFFFFFF, *(u64)(+0x48)=0, *(u64)(+0x50)=0,
//   *(u64)(+0x58)=0, *(u32)(+0x60)=0xFFFFFFFF, *(u64)(+0x68)=0,
//   *(u64)(+0x70)=0, *(u64)(+0x78)=0

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace CharacterComponentAddr {
    constexpr uintptr_t Ctor       = 0x140109d20;
    constexpr uintptr_t Vtable     = 0x140162370;
    constexpr uintptr_t ScalarDtor = 0x14002faa0; // vtable[0]
    constexpr uintptr_t Startup    = 0x14002f940; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140109ab0; // vtable[3]
    constexpr uintptr_t Serialize  = 0x140109a30; // vtable[4]
    constexpr uintptr_t Draw       = 0x1401099c0; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140109980; // vtable[7]
    constexpr uintptr_t OnRemove   = 0x1401094c0; // vtable[8]
    constexpr uintptr_t Update     = 0x140109480; // vtable[9]
}

struct CharacterComponent {
    // Component base (0x28 bytes)
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    // CharacterComponent fields
    uint8_t  data28;       // +0x28: zeroed (byte write)
    uint8_t  _pad29[3];    // +0x29: gap
    uint32_t data2C;       // +0x2C: zeroed (lower of u64 at 0x2C)
    uint32_t data30;       // +0x30: zeroed (upper of u64 at 0x2C)
    uint32_t data34;       // +0x34: zeroed (lower of u64 at 0x34)
    uint32_t data38;       // +0x38: zeroed (upper of u64 at 0x34)
    uint8_t  _pad3C[4];    // +0x3C: gap
    int32_t  data40;       // +0x40: initialized to -1 (0xFFFFFFFF)
    uint8_t  _pad44[4];    // +0x44: gap
    uint64_t data48;       // +0x48: zeroed
    uint64_t data50;       // +0x50: zeroed
    uint64_t data58;       // +0x58: zeroed
    int32_t  data60;       // +0x60: initialized to -1 (0xFFFFFFFF)
    uint8_t  _pad64[4];    // +0x64: gap
    uint64_t data68;       // +0x68: zeroed
    uint64_t data70;       // +0x70: zeroed
    uint64_t data78;       // +0x78: zeroed
};

static_assert(sizeof(CharacterComponent) == 0x80, "CharacterComponent size mismatch");
static_assert(offsetof(CharacterComponent, vtable) == 0x00, "");
static_assert(offsetof(CharacterComponent, next)   == 0x08, "");
static_assert(offsetof(CharacterComponent, owner)  == 0x10, "");
static_assert(offsetof(CharacterComponent, flags)  == 0x20, "");
static_assert(offsetof(CharacterComponent, data28) == 0x28, "");
static_assert(offsetof(CharacterComponent, data2C) == 0x2C, "");
static_assert(offsetof(CharacterComponent, data30) == 0x30, "");
static_assert(offsetof(CharacterComponent, data34) == 0x34, "");
static_assert(offsetof(CharacterComponent, data38) == 0x38, "");
static_assert(offsetof(CharacterComponent, data40) == 0x40, "");
static_assert(offsetof(CharacterComponent, data48) == 0x48, "");
static_assert(offsetof(CharacterComponent, data50) == 0x50, "");
static_assert(offsetof(CharacterComponent, data58) == 0x58, "");
static_assert(offsetof(CharacterComponent, data60) == 0x60, "");
static_assert(offsetof(CharacterComponent, data68) == 0x68, "");
static_assert(offsetof(CharacterComponent, data70) == 0x70, "");
static_assert(offsetof(CharacterComponent, data78) == 0x78, "");
