#pragma once
// RemovalTool — Building/item removal tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: RemovalTool_Ctor (0x140104cc0), RemovalTool_Factory (0x14002e7a0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> RemovalTool

#include <cstdint>
#include <cstddef>

namespace RemovalToolAddr {
    constexpr uintptr_t Ctor    = 0x140104cc0;
    constexpr uintptr_t Factory = 0x14002e7a0;
    constexpr uintptr_t Vtable  = 0x140161de0;
}

struct RemovalTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161de0
    uint8_t  _dragBase[0x30];    // +0x08: DragTool fields
    // --- RemovalTool fields ---
    void*    _field38;           // +0x38: zeroed
};

static_assert(sizeof(RemovalTool) == 0x40, "RemovalTool size mismatch");
static_assert(offsetof(RemovalTool, vtable) == 0x00, "");
