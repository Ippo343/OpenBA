#pragma once
// FruitComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: FruitComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140159450
//   +0x28 = 0 (qword), +0x30 = 0 (dword), +0x34 = 0 (byte)
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace FruitComponentAddr {
    constexpr uintptr_t Vtable = 0x140159450;
}

struct FruitComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    uint32_t field30;      // +0x30: zeroed (dword)
    uint8_t  fruitFlag;    // +0x34: zeroed (byte)
    uint8_t  _pad35[3];    // +0x35: padding to 0x38
};

static_assert(sizeof(FruitComponent) == 0x38, "FruitComponent size mismatch");
static_assert(offsetof(FruitComponent, vtable) == 0x00, "");
static_assert(offsetof(FruitComponent, flags) == 0x20, "");
static_assert(offsetof(FruitComponent, field28) == 0x28, "");
static_assert(offsetof(FruitComponent, field30) == 0x30, "");
static_assert(offsetof(FruitComponent, fruitFlag) == 0x34, "");
