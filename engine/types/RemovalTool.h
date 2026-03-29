#pragma once
// RemovalTool — Building/item removal tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: RemovalTool_Ctor (0x140104cc0), RemovalTool_Factory (0x14002e7a0),
//                  RemovalTool_Startup (0x140104bd0), RemovalTool_Draw (0x140104050),
//                  RemovalTool_CalcCost (0x140103ef0), RemovalTool_CalcPath (0x1401035e0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> RemovalTool
//
// The removalDesc pointer (+0x38) is set during Startup by searching the entity's
// component list for the RemovalDescription type (DAT_1401c74a0).
// RemovalDescription fields used by Draw/CalcCost/CalcPath:
//   +0x38: void* entityTemplate — template for preview entities
//   +0x40: uint8_t removeRoads — flag: should removal clear road cells
//   +0x43: uint8_t removeBuildings — flag: should removal include occupied buildings
//   +0x80: char hasConstraints — if nonzero, constrain removal to specific cells
//   +0x98: int constrainStartCell — constrained start cell index
//   +0x9c: int constrainEndCell — constrained end cell index

#include <cstdint>
#include <cstddef>

namespace RemovalToolAddr {
    constexpr uintptr_t Ctor      = 0x140104cc0;
    constexpr uintptr_t Factory   = 0x14002e7a0;
    constexpr uintptr_t Vtable    = 0x140161de0;
    constexpr uintptr_t Dtor      = 0x14002e7f0;
    constexpr uintptr_t Startup   = 0x140104bd0;
    constexpr uintptr_t Draw      = 0x140104050;
    constexpr uintptr_t CalcPath  = 0x1401035e0;
    constexpr uintptr_t CalcCost  = 0x140103ef0;
}

struct RemovalTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161de0
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

    // --- RemovalTool fields ---
    void*    removalDesc;        // +0x38: RemovalDescription* — set by Startup (DAT_1401c74a0)
};

static_assert(sizeof(RemovalTool) == 0x40, "RemovalTool size mismatch");
static_assert(offsetof(RemovalTool, vtable) == 0x00, "");
static_assert(offsetof(RemovalTool, dialogPtr) == 0x10, "");
static_assert(offsetof(RemovalTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(RemovalTool, dragState) == 0x28, "");
static_assert(offsetof(RemovalTool, startCell) == 0x2C, "");
static_assert(offsetof(RemovalTool, endCell) == 0x30, "");
static_assert(offsetof(RemovalTool, removalDesc) == 0x38, "");
