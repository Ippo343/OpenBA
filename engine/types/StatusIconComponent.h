#pragma once
// StatusIconComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x108.
//
// Decompiled from: StatusIconComponent_Ctor
// Parent: Component (direct)
//
// Contains Graphics::Geometry sub-object at +0x28 (initialized by Geometry ctor),
// then overwritten vtable at +0x28 to 0x14015f0c0.
//
// Constructor sets:
//   vtable = 0x140164620
//   +0x28: Geometry sub-object + vtable override to 0x14015f0c0
//   +0xE8 = 0 (qword), +0xF0 = 0 (dword), +0xF4 = 0 (byte)
//   +0xF8 = 0 (qword), +0x100 = -1 (int32)
//   flags |= 0xC2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StatusIconComponentAddr {
    constexpr uintptr_t Vtable = 0x140164620;
}

struct StatusIconComponent {
    void*    vtable;            // +0x00
    void*    next;              // +0x08
    void*    owner;             // +0x10
    void*    _unk18;            // +0x18
    uint32_t flags;             // +0x20
    uint8_t  _pad24[4];         // +0x24

    // Graphics::Geometry sub-object (0xC0 bytes, vtable at +0x28 = 0x14015f0c0)
    uint8_t  geometry[0xC0];    // +0x28..+0xE7

    void*    fieldE8;           // +0xE8: zeroed
    uint32_t fieldF0;           // +0xF0: zeroed (dword)
    uint8_t  iconFlag;          // +0xF4: zeroed (byte)
    uint8_t  _padF5[3];         // +0xF5: padding
    void*    fieldF8;           // +0xF8: zeroed
    int32_t  iconId;            // +0x100: initialized to -1
    uint8_t  _pad104[4];        // +0x104: padding to 0x108
};

static_assert(sizeof(StatusIconComponent) == 0x108, "StatusIconComponent size mismatch");
static_assert(offsetof(StatusIconComponent, vtable) == 0x00, "");
static_assert(offsetof(StatusIconComponent, flags) == 0x20, "");
static_assert(offsetof(StatusIconComponent, geometry) == 0x28, "");
static_assert(offsetof(StatusIconComponent, fieldE8) == 0xE8, "");
static_assert(offsetof(StatusIconComponent, iconFlag) == 0xF4, "");
static_assert(offsetof(StatusIconComponent, iconId) == 0x100, "");
