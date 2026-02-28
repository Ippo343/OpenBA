#pragma once
// ShowTool — Action that shows a tool UI, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: ShowTool_Factory (0x1400013b0) — inline ctor
// Inherits: Object::Internal -> Action (0x20) -> ShowTool

#include <cstdint>
#include <cstddef>

namespace ShowToolAddr {
    constexpr uintptr_t Factory      = 0x1400013b0;
    constexpr uintptr_t Vtable       = 0x1401589b0;
    constexpr uintptr_t RegisterType = 0x14014a5c0;
}

struct ShowTool {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401589b0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- ShowTool fields ---
    void*    _field20;           // +0x20: zeroed (tool ref?)
    int32_t  _field28;           // +0x28: -1 sentinel (tool ID?)
    uint8_t  _pad2C[4];          // +0x2C: padding
};

static_assert(sizeof(ShowTool) == 0x30, "ShowTool size mismatch");
static_assert(offsetof(ShowTool, vtable) == 0x00, "");
