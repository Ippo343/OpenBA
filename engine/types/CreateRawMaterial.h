#pragma once
// CreateRawMaterial -- Action to create a raw material, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: CreateRawMaterial_Factory (0x140022304, inline ctor)
// Inherits: Object::Internal -> Action (0x20) -> CreateRawMaterial
//
// Fields from Serialize (version 3):
//   +0x20: description entity (ExternalPtr/type ref, serialized via FUN_140002340)
//   +0x28: InternalBasePtr (source building)
//   +0x30: InternalBasePtr (secondary entity, version > 0)
//   +0x38: InternalBasePtr (tertiary entity, version > 2)
//   +0x40: int (total quantity)
//   +0x44: int (material type index / cell index)
//   +0x48: uint (placement flags, version >= 2, default 4)
//   +0x4c: float (yield multiplier)
//   +0x50: bool (natural resource flag)
//   +0x51: bool (place near flag)

#include <cstdint>
#include <cstddef>

namespace CreateRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022240;
    constexpr uintptr_t Vtable  = 0x14015fd10;
}

struct CreateRawMaterial {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015fd10
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding

    // --- CreateRawMaterial fields ---
    void*    descEntity;         // +0x20: description entity (ExternalPtr)
    void*    sourceBuilding;     // +0x28: InternalBasePtr (source building)
    void*    secondaryEntity;    // +0x30: InternalBasePtr (optional secondary entity)
    void*    tertiaryEntity;     // +0x38: InternalBasePtr (optional tertiary entity)
    int32_t  totalQuantity;      // +0x40: total quantity to produce
    int32_t  materialCellIdx;    // +0x44: material type index / cell index
    uint32_t placementFlags;     // +0x48: placement flags
    float    yieldMultiplier;    // +0x4c: yield multiplier
    uint8_t  naturalResFlag;     // +0x50: natural resource flag
    uint8_t  placeNearFlag;      // +0x51: place near flag
    uint8_t  _pad52[6];          // +0x52: padding
};

static_assert(sizeof(CreateRawMaterial) == 0x58, "CreateRawMaterial size mismatch");
static_assert(offsetof(CreateRawMaterial, vtable) == 0x00, "");
static_assert(offsetof(CreateRawMaterial, descEntity) == 0x20, "");
static_assert(offsetof(CreateRawMaterial, sourceBuilding) == 0x28, "");
static_assert(offsetof(CreateRawMaterial, secondaryEntity) == 0x30, "");
static_assert(offsetof(CreateRawMaterial, tertiaryEntity) == 0x38, "");
static_assert(offsetof(CreateRawMaterial, totalQuantity) == 0x40, "");
static_assert(offsetof(CreateRawMaterial, materialCellIdx) == 0x44, "");
static_assert(offsetof(CreateRawMaterial, placementFlags) == 0x48, "");
static_assert(offsetof(CreateRawMaterial, yieldMultiplier) == 0x4c, "");
static_assert(offsetof(CreateRawMaterial, naturalResFlag) == 0x50, "");
static_assert(offsetof(CreateRawMaterial, placeNearFlag) == 0x51, "");
