#pragma once
// RoadTool — Road placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: RoadTool_Ctor (0x1401057d0), RoadTool_Factory (0x14002e870)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> RoadTool

#include <cstdint>
#include <cstddef>

namespace RoadToolAddr {
    constexpr uintptr_t Ctor    = 0x1401057d0;
    constexpr uintptr_t Factory = 0x14002e870;
    constexpr uintptr_t Vtable  = 0x140161e70;
}

struct RoadTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161e70
    uint8_t  _dragBase[0x30];    // +0x08: DragTool fields
    // --- RoadTool fields ---
    void*    _field38;           // +0x38: zeroed
    uint8_t  _field40;           // +0x40: zeroed
    uint8_t  _pad41[7];          // +0x41: padding
};

static_assert(sizeof(RoadTool) == 0x48, "RoadTool size mismatch");
static_assert(offsetof(RoadTool, vtable) == 0x00, "");
