#pragma once
// TendArea — Area tending work action, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: TendArea_Ctor (0x1400f5a00)
// Inherits: Object::Internal -> Work (0x48) -> TendArea
// Extends Work directly.

#include <cstdint>
#include <cstddef>

namespace TendAreaAddr {
    constexpr uintptr_t Ctor    = 0x1400f5a00;
    constexpr uintptr_t Vtable  = 0x140161990;
}

struct TendArea {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161990
    uint8_t  _workBase[0x40];    // +0x08: Work fields
    // --- TendArea fields ---
    void*    _field48;           // +0x48: zeroed (BST root for Serialize)
    uint32_t _field50;           // +0x50: zeroed
    uint8_t  _pad54[4];          // +0x54: padding
    uint32_t _field58;           // +0x58: zeroed (serialized int)
};

static_assert(sizeof(TendArea) == 0x60, "TendArea size mismatch");
static_assert(offsetof(TendArea, vtable) == 0x00, "");
