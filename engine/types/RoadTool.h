#pragma once
// RoadTool — Road placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: RoadTool_Ctor (0x1401057d0), RoadTool_Factory (0x14002e870)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> RoadTool
//
// Vtable at 0x140161e70 (15 slots).
//
// RoadTool extends DragTool with road-specific path calculation and validation.
// Uses CreateRoadDescription type ID (DAT_1401c75b8) for component lookup.
// The _field38 field stores the found road description component pointer.
// The _field40 byte is a modifier flag (e.g., upgrade vs new road) read from
// Input::KeyMap::GetValue(0, 0x16).

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
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    _field18;           // +0x18: unused (zeroed)
    void*    descriptionPtr;     // +0x20: pointer to description/config (ExternalPtr)
    uint8_t  dragState;          // +0x28: 0=idle, 1=dragging
    uint8_t  singleClick;        // +0x29: 1 if user single-clicked
    uint8_t  _pad2A[2];          // +0x2A: padding
    int32_t  startCell;          // +0x2C: map cell index of drag start
    int32_t  endCell;            // +0x30: map cell index of drag end
    int32_t  lastEndCell;        // +0x34: previous endCell (change detection)

    // --- RoadTool fields ---
    void*    roadDesc;           // +0x38: CreateRoadDescription component pointer
    uint8_t  modifierFlag;       // +0x40: road modifier (0=new, 1=upgrade), from KeyMap GetValue(0,0x16)
    uint8_t  _pad41[7];          // +0x41: padding
};

static_assert(sizeof(RoadTool) == 0x48, "RoadTool size mismatch");
static_assert(offsetof(RoadTool, vtable) == 0x00, "");
static_assert(offsetof(RoadTool, dialogPtr) == 0x10, "");
static_assert(offsetof(RoadTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(RoadTool, dragState) == 0x28, "");
static_assert(offsetof(RoadTool, startCell) == 0x2C, "");
static_assert(offsetof(RoadTool, endCell) == 0x30, "");
static_assert(offsetof(RoadTool, lastEndCell) == 0x34, "");
static_assert(offsetof(RoadTool, roadDesc) == 0x38, "");
static_assert(offsetof(RoadTool, modifierFlag) == 0x40, "");
