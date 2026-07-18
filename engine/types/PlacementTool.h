#pragma once
// PlacementTool — Building placement tool, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: PlacementTool_Ctor (0x1400fe850), PlacementTool_Factory (0x14002dc40)
// Factory allocates 0x70 bytes. Vtable at 0x140161d20.
// Inherits: Object::Internal (0x10) -> Tool (0x28) -> PlacementTool (0x70)
// NOTE: Does NOT inherit DragTool. PlacementTool extends Tool directly.
//
// Vtable layout (13 slots):
//   +0x00: Dtor (0x14002dc90)
//   +0x08: GetTypeInfo (0x14002dc30)
//   +0x10: GetTypeId (0x14002dc20)
//   +0x18: JMP thunk (0x14003a95a)
//   +0x20: Startup (0x1400fe580)
//   +0x28: Shutdown2 (0x1400fe4e0)
//   +0x30: OnKey (0x1400fe2a0)
//   +0x38: Serialize (0x1400fdee0)
//   +0x40: Draw (0x1400fdcc0)
//   +0x48: Slot9 (returns false)
//   +0x50: Slot10 (returns false)
//   +0x58: Slot11
//   +0x60: Slot12 (returns 5)

#include <cstdint>
#include <cstddef>

namespace PlacementToolAddr {
    constexpr uintptr_t Ctor    = 0x1400fe850;
    constexpr uintptr_t Factory = 0x14002dc40;
    constexpr uintptr_t Vtable  = 0x140161d20;
}

struct PlacementTool {
    // --- Tool base (0x28 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161d20
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    dialogPtr;          // +0x10: pointer to InternalBasePtr<DialogInstance>
    void*    _field18;           // +0x18: zeroed, some IBP (RotationSetting related)
    void*    descriptionPtr;     // +0x20: pointer to ComponentEntity (description/config)

    // --- PlacementTool fields ---
    // 🔑 Component types Ghidra-verified via RegisterType DATA writers (2026-07-02);
    // the DAT globals' engine_funcs labels are WRONG — true types below.
    void*    descComponent;      // +0x28: found via DAT_1401c7468 = CreatePlacedDescription
    void*    mapComponent;       // +0x30: found via DAT_1401c7a18 = MapDescription
    void*    createComponent;    // +0x38: found via DAT_1401c7a88 = ModelDescription (mesh; Startup derefs +0x28 UNGUARDED)
    void*    radiusComponent;    // +0x40: found via DAT_1401c4870 = RadiusDescription
    uint64_t rotationIBP;        // +0x48: InternalBasePtr to RotationSetting
    int32_t  lastStartCol;       // +0x50: last cell column (for tick sound check)
    int32_t  lastStartRow;       // +0x54: last cell row (for tick sound check)
    int32_t  lastCenterCell;     // +0x58: last center cell (for radius display)
    int32_t  radiusDisplayIdx;   // +0x5c: radius display index (-1 = none)
    int32_t  sceneObjectIdx;     // +0x60: Graphics::Scene object index (-1 = none)
    int32_t  meshVariant;        // +0x64: mesh variant/rotation index
    void*    meshInstance;       // +0x68: Graphics::MeshInstance* for placement preview
};

static_assert(sizeof(PlacementTool) == 0x70, "PlacementTool size mismatch");
static_assert(offsetof(PlacementTool, vtable) == 0x00, "");
static_assert(offsetof(PlacementTool, dialogPtr) == 0x10, "");
static_assert(offsetof(PlacementTool, descriptionPtr) == 0x20, "");
static_assert(offsetof(PlacementTool, descComponent) == 0x28, "");
static_assert(offsetof(PlacementTool, mapComponent) == 0x30, "");
static_assert(offsetof(PlacementTool, createComponent) == 0x38, "");
static_assert(offsetof(PlacementTool, radiusComponent) == 0x40, "");
static_assert(offsetof(PlacementTool, rotationIBP) == 0x48, "");
static_assert(offsetof(PlacementTool, lastStartCol) == 0x50, "");
static_assert(offsetof(PlacementTool, lastStartRow) == 0x54, "");
static_assert(offsetof(PlacementTool, lastCenterCell) == 0x58, "");
static_assert(offsetof(PlacementTool, radiusDisplayIdx) == 0x5c, "");
static_assert(offsetof(PlacementTool, sceneObjectIdx) == 0x60, "");
static_assert(offsetof(PlacementTool, meshVariant) == 0x64, "");
static_assert(offsetof(PlacementTool, meshInstance) == 0x68, "");
