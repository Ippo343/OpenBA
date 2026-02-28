#pragma once
// PathTool — Path placement tool (paths/roads?), binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: PathTool_Ctor (0x1400de9e0), PathTool_Factory (0x140026b10)
// Inherits: Object::Internal -> Tool (0x28) -> PathTool
// NOTE: PathTool extends Tool directly, NOT DragTool. Different approach to path creation.

#include <cstdint>
#include <cstddef>

namespace PathToolAddr {
    constexpr uintptr_t Ctor    = 0x1400de9e0;
    constexpr uintptr_t Factory = 0x140026b10;
    constexpr uintptr_t Vtable  = 0x140160680;
}

struct PathTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160680
    uint8_t  _toolBase[0x20];    // +0x08: Tool fields
    // --- PathTool fields ---
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
};

static_assert(sizeof(PathTool) == 0x60, "PathTool size mismatch");
static_assert(offsetof(PathTool, vtable) == 0x00, "");
