#pragma once
// PathTool — Path placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: PathTool_Ctor (0x1400de9e0), PathTool_Factory (0x140026b10)
// Inherits: Object::Internal -> Tool (0x28) -> PathTool
// NOTE: PathTool extends Tool directly, NOT DragTool. Different approach to path creation.
//
// Vtable at 0x140160680 (13 slots).
//
// PathTool uses a multi-point placement model (not drag-based like DragTool):
//   OnKey picks the map cell under the cursor and manages entity selection/highlighting.
//   Draw builds render geometry for the path preview overlay.
//   Serialize compares an IBP to the selected entity.

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
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    _field18;           // +0x18: unused (zeroed)
    void*    descriptionPtr;     // +0x20: pointer to description/config (ExternalPtr)

    // --- PathTool fields ---
    uint64_t selectedEntity;     // +0x28: IBP to currently selected/highlighted entity
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: int array ptr (cell indices for path points)
    int32_t  _field48;           // +0x48: count of path points in _field40 array
    uint8_t  _pad4C[4];          // +0x4C: padding
    void*    _field50;           // +0x50: int pair array ptr (line segments: cell pairs)
    int32_t  _field58;           // +0x58: count of line segments in _field50 array
    uint8_t  _pad5C[4];          // +0x5C: padding
};

static_assert(sizeof(PathTool) == 0x60, "PathTool size mismatch");
static_assert(offsetof(PathTool, vtable) == 0x00, "");
static_assert(offsetof(PathTool, dialogPtr) == 0x10, "");
static_assert(offsetof(PathTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(PathTool, selectedEntity) == 0x28, "");
static_assert(offsetof(PathTool, _field40) == 0x40, "");
static_assert(offsetof(PathTool, _field48) == 0x48, "");
static_assert(offsetof(PathTool, _field50) == 0x50, "");
static_assert(offsetof(PathTool, _field58) == 0x58, "");
