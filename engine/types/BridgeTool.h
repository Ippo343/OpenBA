#pragma once
// BridgeTool — Bridge placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: BridgeTool_Ctor (0x1400fb890), BridgeTool_Factory (0x14002d7a0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> BridgeTool

#include <cstdint>
#include <cstddef>

namespace BridgeToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fb890;
    constexpr uintptr_t Factory = 0x14002d7a0;
    constexpr uintptr_t Vtable  = 0x140161b00;
}

struct BridgeTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161b00
    uint8_t  _dragBase[0x30];    // +0x08: DragTool fields
    // --- BridgeTool fields ---
    void*    _field38;           // +0x38: zeroed
};

static_assert(sizeof(BridgeTool) == 0x40, "BridgeTool size mismatch");
static_assert(offsetof(BridgeTool, vtable) == 0x00, "");
