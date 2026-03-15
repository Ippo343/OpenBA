#pragma once
// BridgeTool — Bridge placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: BridgeTool_Ctor (0x1400fb890), BridgeTool_Factory (0x14002d7a0),
//                  BridgeTool_Startup (0x1400fb7a0), BridgeTool_Slot13 (0x1400f9cd0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> BridgeTool
//
// The bridgeDesc pointer (+0x38) is set during Startup by searching the entity's
// component list for the CreateBridgeDescription type (DAT_1401c73f8).
// BridgeDescription fields used by CalcPath/CalcCost:
//   +0x20: uint32_t terrainMaskA — bitmask of allowed terrain types (bridge body)
//   +0x24: uint32_t terrainMaskB — bitmask of approach terrain types (bridge ends)
//   +0x2C: uint32_t terrainMaskC — bitmask of water/crossing terrain
//   +0x30: uint32_t terrainMaskD — bitmask for neighbor validation
//   +0x34: int32_t  neighborRadius — radius for neighbor cell checking
//   +0x38: void*    segmentTemplates[5] — array of entity templates for segments
//                    (index 0..4 = segment sizes: 0-cell, 1-cell, 2-cell, 4-cell, 8-cell)

#include <cstdint>
#include <cstddef>

namespace BridgeToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fb890;
    constexpr uintptr_t Factory = 0x14002d7a0;
    constexpr uintptr_t Vtable  = 0x140161b00;
    constexpr uintptr_t Dtor    = 0x14002d7f0;
    constexpr uintptr_t Startup = 0x1400fb7a0;
    constexpr uintptr_t CalcPath = 0x1400f9cd0;
    constexpr uintptr_t CalcCost = 0x1400facf0;
    constexpr uintptr_t SnapEndCell = 0x1400fb710;  // sub_b710: snaps end cell to row/col
    constexpr uintptr_t ValidatePath = 0x1400fb2d0; // sub_b2d0: validates bridge path
}

struct BridgeTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161b00
    uint8_t  _internalBase[8];   // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    _field18;           // +0x18: zeroed (Tool field)
    void*    descriptionPtr;     // +0x20: pointer to description/config (ExternalPtr)
    uint8_t  dragState;          // +0x28: 0=idle, 1=dragging
    uint8_t  singleClick;        // +0x29: 1 if single-click mode
    uint8_t  _pad2A[2];          // +0x2A: padding
    int32_t  startCell;          // +0x2C: drag start cell index (-1 sentinel)
    int32_t  endCell;            // +0x30: drag end cell index (-1 sentinel)
    int32_t  lastEndCell;        // +0x34: previous endCell for change detection

    // --- BridgeTool fields ---
    void*    bridgeDesc;         // +0x38: CreateBridgeDescription* — set by Startup
};

static_assert(sizeof(BridgeTool) == 0x40, "BridgeTool size mismatch");
static_assert(offsetof(BridgeTool, vtable) == 0x00, "");
static_assert(offsetof(BridgeTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(BridgeTool, startCell) == 0x2C, "");
static_assert(offsetof(BridgeTool, bridgeDesc) == 0x38, "");
