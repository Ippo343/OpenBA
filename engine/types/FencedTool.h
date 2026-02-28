#pragma once
// FencedTool — Fenced area placement tool (pasture/crop fields), binary-compatible struct.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: FencedTool_Ctor (0x1400dd070), FencedTool_Factory (0x1400268d0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> FencedTool
// NOTE: Extends DragTool, not base Tool.

#include <cstdint>
#include <cstddef>

namespace FencedToolAddr {
    constexpr uintptr_t Ctor    = 0x1400dd070;
    constexpr uintptr_t Factory = 0x1400268d0;
    constexpr uintptr_t Vtable  = 0x1401605d0;
}

struct FencedTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401605d0
    uint8_t  _dragBase[0x30];    // +0x08: DragTool fields
    // --- FencedTool fields ---
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    int32_t  _field58;           // +0x58: -1 sentinel
    uint8_t  _pad5C[4];          // +0x5C: padding
    uint8_t  _pad60[8];          // +0x60: gap
    void*    _field68;           // +0x68: zeroed
};

static_assert(sizeof(FencedTool) == 0x70, "FencedTool size mismatch");
static_assert(offsetof(FencedTool, vtable) == 0x00, "");
