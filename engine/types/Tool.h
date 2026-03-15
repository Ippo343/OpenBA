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
//
// OnKey state machine:
//   dragState==0 && key pressed => pick startCell, open dialog, set dragState=1
//   dragState==1 (dragging)     => update endCell, call virtual BuildPath (+0x70),
//                                  update UI via operator_impl_fc60, handle release
//   singleClick flag at +0x29   => set when start==end on release (single click mode)
//
struct DragTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161c10 (base DragTool)
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    _field18;           // +0x18: unused? (zeroed in base)
    void*    descriptionPtr;     // +0x20: pointer to description/config (ExternalPtr)

    // --- DragTool fields ---
    uint8_t  dragState;          // +0x28: 0=idle, 1=dragging
    uint8_t  singleClick;        // +0x29: 1 if user single-clicked (start==end on release)
    uint8_t  _pad2A[2];          // +0x2A: padding
    int32_t  startCell;          // +0x2C: map cell index of drag start (-1 = invalid)
    int32_t  endCell;            // +0x30: map cell index of drag end (-1 = invalid)
    int32_t  lastEndCell;        // +0x34: previous endCell (used for change detection / sound)
};

static_assert(sizeof(DragTool) == 0x38, "DragTool size mismatch");
static_assert(offsetof(DragTool, vtable) == 0x00, "");
static_assert(offsetof(DragTool, dragState) == 0x28, "");
static_assert(offsetof(DragTool, singleClick) == 0x29, "");
static_assert(offsetof(DragTool, startCell) == 0x2C, "");
static_assert(offsetof(DragTool, endCell) == 0x30, "");
static_assert(offsetof(DragTool, lastEndCell) == 0x34, "");
