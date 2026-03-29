#pragma once
// FencedTool — Fenced area placement tool (pasture/crop fields), binary-compatible struct.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: FencedTool_Ctor (0x1400dd070), FencedTool_Factory (0x1400268d0)
// Inherits: Object::Internal -> Tool -> DragTool (0x38) -> FencedTool
// NOTE: Extends DragTool, not base Tool.
//
// Startup searches for 5 description types:
//   DAT_1401c4100 (CreateFencedDescription)    -> +0x38
//   DAT_1401c7a18 (AreaDescription)            -> +0x40
//   DAT_1401c7a88 (FenceDescription)           -> +0x48 (from fencedDesc->entity components)
//   RotationSetting (from toolSetting at +0x18) -> +0x50 (IBP)
//   DAT_1401c7468 (SizeDescription)            -> +0x5C, +0x60 (reads specific fields)
// Also allocates a MeshInstance at +0x68 for fence preview rendering.

#include <cstdint>
#include <cstddef>

namespace FencedToolAddr {
    constexpr uintptr_t Ctor     = 0x1400dd070;
    constexpr uintptr_t Factory  = 0x1400268d0;
    constexpr uintptr_t Vtable   = 0x1401605d0;
    constexpr uintptr_t Dtor     = 0x140026930;
    constexpr uintptr_t Startup  = 0x1400dcd50;
    constexpr uintptr_t Shutdown2 = 0x1400dccc0;
    constexpr uintptr_t OnKey    = 0x1400dcbe0;
    constexpr uintptr_t Serialize = 0x1400dcad0;
    constexpr uintptr_t Draw     = 0x1400dc440;
    constexpr uintptr_t Shutdown = 0x1400dcd10;
    constexpr uintptr_t CalcPath = 0x1400dae00;
    constexpr uintptr_t CalcCost = 0x1400db490;
}

struct FencedTool {
    // --- DragTool base (0x38 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401605d0
    uint8_t  _internalBase[8];   // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: InternalBasePtr to placement UI dialog
    void*    toolSetting;        // +0x18: InternalBasePtr to tool setting
    void*    descriptionPtr;     // +0x20: pointer to entity description (ExternalPtr)
    uint8_t  dragState;          // +0x28: 0=idle, 1=dragging
    uint8_t  singleClick;        // +0x29: 1 if single-click mode
    uint8_t  _pad2A[2];          // +0x2A: padding
    int32_t  startCell;          // +0x2C: drag start cell index
    int32_t  endCell;            // +0x30: drag end cell index
    int32_t  lastEndCell;        // +0x34: previous endCell for change detection
    // --- FencedTool fields ---
    void*    fencedDesc;         // +0x38: CreateFencedDescription* (DAT_1401c4100)
    void*    areaDesc;           // +0x40: AreaDescription* (DAT_1401c7a18)
    void*    fenceDesc;          // +0x48: FenceDescription* (DAT_1401c7a88)
    void*    rotationSetting;    // +0x50: InternalBasePtr to RotationSetting
    int32_t  sceneObjectId;      // +0x58: Graphics::Scene object ID (-1 = not added)
    uint32_t _field5C;           // +0x5C: from SizeDescription +0x44
    uint32_t _field60;           // +0x60: from SizeDescription +0x48
    uint8_t  _pad64[4];          // +0x64: padding
    void*    meshInstance;        // +0x68: Graphics::MeshInstance* for fence preview
};

static_assert(sizeof(FencedTool) == 0x70, "FencedTool size mismatch");
static_assert(offsetof(FencedTool, vtable) == 0x00, "");
static_assert(offsetof(FencedTool, dialogPtr) == 0x10, "");
static_assert(offsetof(FencedTool, toolSetting) == 0x18, "");
static_assert(offsetof(FencedTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(FencedTool, dragState) == 0x28, "");
static_assert(offsetof(FencedTool, startCell) == 0x2C, "");
static_assert(offsetof(FencedTool, fencedDesc) == 0x38, "");
static_assert(offsetof(FencedTool, areaDesc) == 0x40, "");
static_assert(offsetof(FencedTool, fenceDesc) == 0x48, "");
static_assert(offsetof(FencedTool, rotationSetting) == 0x50, "");
static_assert(offsetof(FencedTool, sceneObjectId) == 0x58, "");
static_assert(offsetof(FencedTool, meshInstance) == 0x68, "");
