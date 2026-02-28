#pragma once
// EditTool — Building editing/moving tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: EditTool_Ctor (0x1400fd340), EditTool_Factory (0x14002daa0)
// Inherits: Object::Internal -> Tool (0x28) -> EditTool
// NOTE: EditTool extends Tool directly, NOT DragTool.

#include <cstdint>
#include <cstddef>

namespace EditToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fd340;
    constexpr uintptr_t Factory = 0x14002daa0;
    constexpr uintptr_t Vtable  = 0x140161ca0;
}

struct EditTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161ca0
    uint8_t  _toolBase[0x20];    // +0x08: Tool fields
    // --- EditTool fields ---
    void*    entityRef;          // +0x28: Object::InternalBasePtr (zeroed, init via ctor)
    void*    _field30;           // +0x30: zeroed
};

static_assert(sizeof(EditTool) == 0x38, "EditTool size mismatch");
static_assert(offsetof(EditTool, vtable) == 0x00, "");
