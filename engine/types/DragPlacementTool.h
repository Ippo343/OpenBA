#pragma once
// DragPlacementTool — Drag-based placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: DragPlacementTool_Ctor (0x1400fc530), DragPlacementTool_Factory (0x14002d8b0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> DragPlacementTool

#include <cstdint>
#include <cstddef>

namespace DragPlacementToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fc530;
    constexpr uintptr_t Factory = 0x14002d8b0;
    constexpr uintptr_t Vtable  = 0x140161b90;
}

struct DragPlacementTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161b90
    uint8_t  _dragBase[0x30];    // +0x08: DragTool fields
    // --- DragPlacementTool fields ---
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
};

static_assert(sizeof(DragPlacementTool) == 0x48, "DragPlacementTool size mismatch");
static_assert(offsetof(DragPlacementTool, vtable) == 0x00, "");
