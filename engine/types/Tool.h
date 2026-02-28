#pragma once
// Tool — Base tool class hierarchy, binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// Inheritance chain:
//   Object::Internal (0x10) -> Tool (0x28) -> DragTool (0x38) -> [concrete tools]
//   Object::Internal (0x10) -> Tool (0x28) -> EditTool (0x38)
//   Object::Internal (0x10) -> Tool (0x28) -> PathTool (0x60)
//   Object::Internal (0x10) -> Tool (0x28) -> FencedTool (0x70)
//
// Decompiled from: BridgeTool_Ctor (0x1400fb890) — shows full chain.
//   Object::Internal::Internal() -> set Tool vtable (0x140161f10) -> init fields ->
//   set DragTool vtable (0x140161c10) -> init DragTool fields ->
//   set BridgeTool vtable (0x140161b00) -> init BridgeTool fields

#include <cstdint>
#include <cstddef>

namespace ToolAddr {
    constexpr uintptr_t Vtable = 0x140161f10;
}

namespace DragToolAddr {
    constexpr uintptr_t Vtable = 0x140161c10;
}

// Tool base layout (0x28 bytes)
struct Tool {
    void*    vtable;             // +0x00: -> 0x140161f10 (base)
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    uint8_t  _pad10[8];          // +0x10: zeroed
    void*    _field18;           // +0x18: zeroed
    void*    _field20;           // +0x20: zeroed
};

static_assert(sizeof(Tool) == 0x28, "Tool size mismatch");
static_assert(offsetof(Tool, vtable) == 0x00, "");

// DragTool base layout (0x38 bytes) — for tools that support click-and-drag placement
struct DragTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161c10 (base DragTool)
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    uint8_t  _pad10[8];          // +0x10
    void*    _field18;           // +0x18
    void*    _field20;           // +0x20

    // --- DragTool fields ---
    uint16_t _field28;           // +0x28: zeroed (two flags?)
    uint8_t  _pad2A[2];          // +0x2A: padding
    int64_t  _field2C;           // +0x2C: -1 (all 0xFF bytes)
    int32_t  _field34;           // +0x34: -1 sentinel
};

static_assert(sizeof(DragTool) == 0x38, "DragTool size mismatch");
static_assert(offsetof(DragTool, vtable) == 0x00, "");
static_assert(offsetof(DragTool, _field28) == 0x28, "");
