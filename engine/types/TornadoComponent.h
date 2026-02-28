#pragma once
// TornadoComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: TornadoComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015aa80
//   +0x28 = 0, +0x30 = 0, +0x38 = 0 (dword), +0x40 = 0, +0x48 = 0
//   +0x50 = -1 (int32), +0x58 = 0, +0x60 = 0 (dword)
//   flags |= 0x84

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace TornadoComponentAddr {
    constexpr uintptr_t Vtable = 0x14015aa80;
}

struct TornadoComponent {
    void*    vtable;       // +0x00
    void*    next;         // +0x08
    void*    owner;        // +0x10
    void*    _unk18;       // +0x18
    uint32_t flags;        // +0x20
    uint8_t  _pad24[4];    // +0x24

    void*    field28;      // +0x28: zeroed
    void*    field30;      // +0x30: zeroed
    uint32_t field38;      // +0x38: zeroed (dword)
    uint8_t  _pad3C[4];    // +0x3C: padding
    void*    field40;      // +0x40: zeroed
    void*    field48;      // +0x48: zeroed
    int32_t  tornadoId;    // +0x50: initialized to -1
    uint8_t  _pad54[4];    // +0x54: padding
    void*    field58;      // +0x58: zeroed
    uint32_t field60;      // +0x60: zeroed (dword)
    uint8_t  _pad64[4];    // +0x64: padding to 0x68
};

static_assert(sizeof(TornadoComponent) == 0x68, "TornadoComponent size mismatch");
static_assert(offsetof(TornadoComponent, vtable) == 0x00, "");
static_assert(offsetof(TornadoComponent, flags) == 0x20, "");
static_assert(offsetof(TornadoComponent, field28) == 0x28, "");
static_assert(offsetof(TornadoComponent, field38) == 0x38, "");
static_assert(offsetof(TornadoComponent, tornadoId) == 0x50, "");
static_assert(offsetof(TornadoComponent, field60) == 0x60, "");
