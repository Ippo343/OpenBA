#pragma once
// DragPlacementTool — Drag-based placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: DragPlacementTool_Ctor (0x1400fc530), DragPlacementTool_Factory (0x14002d8b0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> DragPlacementTool
//
// Startup searches entity component list for two description types:
//   DAT_1401c7430 (CreatePlacementDescription) -> stored at +0x38
//   DAT_1401c7a18 (AreaDescription)            -> stored at +0x40
//
// Description at +0x38 provides:
//   +0x20: uint32_t terrainMask — bitmask for terrain validation
//   +0x24: char     allowOverlap
//   +0x28: int32_t  minWidth / +0x2C: int32_t minHeight (CalcCost min size)
//   +0x30: int32_t  maxWidth / +0x34: int32_t maxHeight (CalcCost max size not used - using +0x28/+0x2C)
//   +0x38: void*    entityTemplate — template for entity creation (CalcPath)

#include <cstdint>
#include <cstddef>

namespace DragPlacementToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fc530;
    constexpr uintptr_t Factory = 0x14002d8b0;
    constexpr uintptr_t Vtable  = 0x140161b90;
    constexpr uintptr_t Dtor    = 0x14002d900;
    constexpr uintptr_t Startup = 0x1400fc3e0;
    constexpr uintptr_t Draw    = 0x1400fbe40;
    constexpr uintptr_t CalcPath = 0x1400fb900;
    constexpr uintptr_t CalcCost = 0x1400fbae0;
}

struct DragPlacementTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161b90
    uint8_t  _internalBase[8];   // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    _field18;           // +0x18: zeroed (Tool field)
    void*    descriptionPtr;     // +0x20: pointer to description/config (ExternalPtr)
    uint8_t  dragState;          // +0x28: 0=idle, 1=dragging
    uint8_t  singleClick;        // +0x29: 1 if single-click mode
    uint8_t  _pad2A[2];          // +0x2A: padding
    int32_t  startCell;          // +0x2C: drag start cell index
    int32_t  endCell;            // +0x30: drag end cell index
    int32_t  lastEndCell;        // +0x34: previous endCell for change detection
    // --- DragPlacementTool fields ---
    void*    placementDesc;      // +0x38: CreatePlacementDescription* (DAT_1401c7430)
    void*    areaDesc;           // +0x40: AreaDescription* (DAT_1401c7a18)
};

static_assert(sizeof(DragPlacementTool) == 0x48, "DragPlacementTool size mismatch");
static_assert(offsetof(DragPlacementTool, vtable) == 0x00, "");
static_assert(offsetof(DragPlacementTool, dialogPtr) == 0x10, "");
static_assert(offsetof(DragPlacementTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(DragPlacementTool, dragState) == 0x28, "");
static_assert(offsetof(DragPlacementTool, startCell) == 0x2C, "");
static_assert(offsetof(DragPlacementTool, endCell) == 0x30, "");
static_assert(offsetof(DragPlacementTool, placementDesc) == 0x38, "");
static_assert(offsetof(DragPlacementTool, areaDesc) == 0x40, "");
