#pragma once
// Build/Work action types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// These are all Work-derived (0x48 byte base), NOT Action-derived (0x20 byte base).
// Decompiled from constructors which all call Object::Internal::Internal() then
// init Work fields at +0x10..+0x40, then set derived vtable and init derived fields.
//
// Work vtable layout (12 slots, 96 bytes):
//   [0] ScalarDeletingDtor    [1] VectorDeletingDtor   [2] BaseDtor
//   [3] Serialize             [4] Execute/Update       [5] shared (0x1400f7540)
//   [6] OnComplete            [7] OnCancel             [8] Cleanup
//   [9] Validate              [10] GetTarget            [11] GetPriority

#include <cstdint>
#include <cstddef>

// ============================================================
// BuildRoad — road construction work. Size = 0x70.
// Ctor: 0x1400e4ad0, Vtable: 0x140161020, Factory: 0x140029560
// Inherits: Object::Internal -> Work (0x48) -> BuildRoad
// ============================================================
namespace BuildRoadAddr {
    constexpr uintptr_t Ctor         = 0x1400e4ad0;
    constexpr uintptr_t Factory      = 0x140029560;
    constexpr uintptr_t Vtable       = 0x140161020;
    constexpr uintptr_t ScalarDtor   = 0x1400295b0;
    constexpr uintptr_t Serialize    = 0x1400e4980;
}

struct BuildRoad {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161020
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr (zeroed)
    void*    field_18;           // +0x18: entity ptr (zeroed)
    int32_t  field_20;           // +0x20: -1 sentinel
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1 sentinel
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- BuildRoad fields ---
    void*    field_48;           // +0x48: zeroed (InternalBasePtr)
    void*    field_50;           // +0x50: zeroed
    int32_t  field_58;           // +0x58: -1 sentinel
    int32_t  field_5c;           // +0x5C: 2 (road type?)
    uint8_t  _fields60[0x10];    // +0x60: remaining fields
};

static_assert(sizeof(BuildRoad) == 0x70, "BuildRoad size mismatch");
static_assert(offsetof(BuildRoad, vtable) == 0x00, "");
static_assert(offsetof(BuildRoad, field_48) == 0x48, "");
static_assert(offsetof(BuildRoad, field_58) == 0x58, "");

// ============================================================
// BuildStructure — structure construction work. Size = 0x70.
// Ctor: 0x1400e5a90, Vtable: 0x1401610a0, Factory: 0x140029630
// Inherits: Object::Internal -> Work (0x48) -> BuildStructure
// BaseDtor: 0x1400e5920
// ============================================================
namespace BuildStructureAddr {
    constexpr uintptr_t Ctor         = 0x1400e5a90;
    constexpr uintptr_t Factory      = 0x140029630;
    constexpr uintptr_t Vtable       = 0x1401610a0;
    constexpr uintptr_t ScalarDtor   = 0x140029680;
    constexpr uintptr_t BaseDtor     = 0x1400e5920;
    constexpr uintptr_t Serialize    = 0x1400e5850;
}

struct BuildStructure {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401610a0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- BuildStructure fields ---
    // Note: ctor doesn't init +0x48, starts at +0x50
    uint8_t  field_48;           // +0x48: bool (serialized)
    uint8_t  _pad49[3];          // +0x49: padding
    int32_t  field_4c;           // +0x4C: int (serialized)
    void*    field_50;           // +0x50: array ptr (zeroed)
    int32_t  field_58;           // +0x58: count (zeroed)
    uint8_t  _pad5c[4];          // +0x5C: padding
    void*    field_60;           // +0x60: tree node ptr (zeroed)
    int32_t  field_68;           // +0x68: 0
    uint8_t  _pad6c[4];          // +0x6C: padding
};

static_assert(sizeof(BuildStructure) == 0x70, "BuildStructure size mismatch");
static_assert(offsetof(BuildStructure, vtable) == 0x00, "");
static_assert(offsetof(BuildStructure, field_50) == 0x50, "");

// ============================================================
// ClearArea — area clearing work. Size = 0x78.
// Ctor: 0x1400e68d0, Vtable: 0x140161110, Factory: 0x140029720
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// BaseDtor: 0x1400e6810
// ============================================================
namespace ClearAreaAddr {
    constexpr uintptr_t Ctor         = 0x1400e68d0;
    constexpr uintptr_t Vtable       = 0x140161110;
    constexpr uintptr_t ScalarDtor   = 0x140029870;
    constexpr uintptr_t BaseDtor     = 0x1400e6810;
    constexpr uintptr_t Serialize    = 0x1400e6740;
}

struct ClearArea {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161110
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- ClearArea fields ---
    void*    field_48;           // +0x48: tree node ptr (zeroed)
    int32_t  field_50;           // +0x50: count (zeroed)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: tree node ptr (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: InternalBasePtr (zeroed)
    uint8_t  field_70;           // +0x70: bool (serialized)
    uint8_t  field_71;           // +0x71: bool (serialized, version >= 1)
    uint8_t  field_72;           // +0x72: bool (serialized, version >= 1)
    uint8_t  _pad73[5];          // +0x73: padding
};

static_assert(sizeof(ClearArea) == 0x78, "ClearArea size mismatch");
static_assert(offsetof(ClearArea, vtable) == 0x00, "");
static_assert(offsetof(ClearArea, field_48) == 0x48, "");
static_assert(offsetof(ClearArea, field_68) == 0x68, "");

// ============================================================
// ConsumeProduce — consume/produce work cycle. Size = 0x78.
// Ctor: 0x1400e9340, Vtable: 0x140161220, Factory: 0x14002a150
// Inherits: Object::Internal -> Work (0x48) -> ConsumeProduce
// BaseDtor: 0x1400e9190
// ============================================================
namespace ConsumeProduceActionAddr {
    constexpr uintptr_t Ctor         = 0x1400e9340;
    constexpr uintptr_t Factory      = 0x14002a150;
    constexpr uintptr_t Vtable       = 0x140161220;
    constexpr uintptr_t ScalarDtor   = 0x14002a190;
    constexpr uintptr_t BaseDtor     = 0x1400e9190;
    constexpr uintptr_t Serialize    = 0x1400e9020;
}

struct ConsumeProduceAction {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161220
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- ConsumeProduce fields ---
    uint8_t  field_48;           // +0x48: bool (serialized)
    uint8_t  _pad49[3];          // +0x49: padding
    int32_t  field_4c;           // +0x4C: int (serialized)
    int32_t  field_50;           // +0x50: Type (zeroed, serialized)
    int32_t  field_54;           // +0x54: int (serialized)
    void*    field_58;           // +0x58: InternalBasePtr array (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: int array (zeroed)
    int32_t  field_70;           // +0x70: count (zeroed)
    uint8_t  _pad74[4];          // +0x74: padding
};

static_assert(sizeof(ConsumeProduceAction) == 0x78, "ConsumeProduceAction size mismatch");
static_assert(offsetof(ConsumeProduceAction, vtable) == 0x00, "");
static_assert(offsetof(ConsumeProduceAction, field_58) == 0x58, "");

// ============================================================
// GatherRawMaterials — raw material gathering work. Size = 0x78.
// Ctor: 0x1400ec8a0, Vtable: 0x140161420, Factory: 0x14002a3c0
// Inherits: Object::Internal -> Work (0x48) -> GatherRawMaterials
// BaseDtor: 0x1400ec650
// ============================================================
namespace GatherRawMaterialsAddr {
    constexpr uintptr_t Ctor         = 0x1400ec8a0;
    constexpr uintptr_t Factory      = 0x14002a3c0;
    constexpr uintptr_t Vtable       = 0x140161420;
    constexpr uintptr_t ScalarDtor   = 0x14002a410;
    constexpr uintptr_t BaseDtor     = 0x1400ec650;
    constexpr uintptr_t Serialize    = 0x1400ec560;
}

struct GatherRawMaterials {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161420
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- GatherRawMaterials fields ---
    void*    field_48;           // +0x48: int array (zeroed)
    int32_t  field_50;           // +0x50: count (zeroed)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: int array (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: tree node ptr (zeroed)
    int32_t  field_70;           // +0x70: count (zeroed)
    uint8_t  _pad74[4];          // +0x74: padding
};

static_assert(sizeof(GatherRawMaterials) == 0x78, "GatherRawMaterials size mismatch");
static_assert(offsetof(GatherRawMaterials, vtable) == 0x00, "");
static_assert(offsetof(GatherRawMaterials, field_48) == 0x48, "");

// ============================================================
// PlantArea — crop field planting work. Size = 0x70.
// Ctor: 0x1400f4770, Vtable: 0x140161890
// Inherits: Object::Internal -> Work (0x48) -> PlantArea
// BaseDtor: 0x1400f4600
// ============================================================
namespace PlantAreaAddr {
    constexpr uintptr_t Ctor         = 0x1400f4770;
    constexpr uintptr_t Vtable       = 0x140161890;
    constexpr uintptr_t ScalarDtor   = 0x14002c4b0;
    constexpr uintptr_t BaseDtor     = 0x1400f4600;
    constexpr uintptr_t Serialize    = 0x1400f4540;
}

struct PlantArea {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161890
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- PlantArea fields ---
    void*    field_48;           // +0x48: tree node ptr (zeroed)
    int32_t  field_50;           // +0x50: count (zeroed)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: bool array (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: InternalBasePtr (zeroed)
};

static_assert(sizeof(PlantArea) == 0x70, "PlantArea size mismatch");
static_assert(offsetof(PlantArea, vtable) == 0x00, "");
static_assert(offsetof(PlantArea, field_48) == 0x48, "");

// ============================================================
// PlantOrchard — orchard planting work. Size = 0x70.
// Ctor: 0x1400f50b0, Vtable: 0x140161910
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// BaseDtor: 0x1400f5010
// ============================================================
namespace PlantOrchardAddr {
    constexpr uintptr_t Ctor         = 0x1400f50b0;
    constexpr uintptr_t Vtable       = 0x140161910;
    constexpr uintptr_t ScalarDtor   = 0x14002c540;
    constexpr uintptr_t BaseDtor     = 0x1400f5010;
    constexpr uintptr_t Serialize    = 0x1400f4f50;
}

struct PlantOrchard {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161910
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- PlantOrchard fields ---
    void*    field_48;           // +0x48: tree node ptr (zeroed)
    int32_t  field_50;           // +0x50: count (zeroed)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: bool array (zeroed)
    int32_t  field_60;           // +0x60: count (zeroed)
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: InternalBasePtr (zeroed)
};

static_assert(sizeof(PlantOrchard) == 0x70, "PlantOrchard size mismatch");
static_assert(offsetof(PlantOrchard, vtable) == 0x00, "");
static_assert(offsetof(PlantOrchard, field_48) == 0x48, "");

// ============================================================
// Pickup — item pickup work. Size = 0x60.
// Decompiled from: Pickup_Factory (0x14002bfc0) -> Pickup_Ctor
// Inherits: Object::Internal -> Work (0x48) -> Pickup
// ============================================================
namespace PickupAddr {
    constexpr uintptr_t Factory = 0x14002bfc0;
}

struct Pickup {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr
    void*    field_18;           // +0x18: entity ptr
    int32_t  field_20;           // +0x20: -1
    int32_t  field_24;           // +0x24: 0
    int32_t  field_28;           // +0x28: -1
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- Pickup fields ---
    void*    field_48;           // +0x48: zeroed
    void*    field_50;           // +0x50: zeroed
    void*    field_58;           // +0x58: zeroed
};

static_assert(sizeof(Pickup) == 0x60, "Pickup size mismatch");
static_assert(offsetof(Pickup, vtable) == 0x00, "");
