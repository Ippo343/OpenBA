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
#include "Work/BuildRoad.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)

// BuildRoad — the real class code/Game/Work/BuildRoad.h (ENDGAME B.3.8).

// ============================================================
// BuildStructure — structure construction work. Size = 0x70.
// Ctor: 0x1400e5a90, Vtable: 0x1401610a0, Factory: 0x140029630
// Inherits: Object::Internal -> Work (0x48) -> BuildStructure
// BaseDtor: 0x1400e5920
//
// Manages citizen work assignments for building structures.
// Maintains a slot array (per-build-point availability) and a
// binary tree of assigned citizens (InternalBasePtr keys).
// ============================================================
namespace BuildStructureAddr {
    constexpr uintptr_t Ctor         = 0x1400e5a90;
    constexpr uintptr_t Factory      = 0x140029630;
    constexpr uintptr_t Vtable       = 0x1401610a0;
    constexpr uintptr_t ScalarDtor   = 0x140029680;
    constexpr uintptr_t BaseDtor     = 0x1400e5920;
    constexpr uintptr_t Serialize    = 0x1400e5850;
    constexpr uintptr_t Execute      = 0x1400e4fc0;
    constexpr uintptr_t OnComplete   = 0x1400e4f80;
    constexpr uintptr_t OnCancel     = 0x1400e4f40;
    constexpr uintptr_t Cleanup      = 0x1400e57a0;
    constexpr uintptr_t Validate     = 0x1400e4be0;
    constexpr uintptr_t GetTarget    = 0x1400e5830;
    constexpr uintptr_t GetPriority  = 0x140029670;
}

struct BuildStructure {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x1401610a0
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: entity ptr (building being constructed)
    void*    field_18;           // +0x18: entity ptr (storage location)
    int32_t  field_20;           // +0x20: task ID (-1 sentinel)
    int32_t  field_24;           // +0x24: task sub-ID
    int32_t  field_28;           // +0x28: secondary ID (-1 sentinel)
    int32_t  field_2c;           // +0x2C: secondary sub-ID
    void*    field_30;           // +0x30: time start (zeroed)
    void*    field_38;           // +0x38: time end (zeroed)
    int32_t  field_40;           // +0x40: flags/state (0)
    uint8_t  _pad44[4];          // +0x44: padding

    // --- BuildStructure fields ---
    // Note: ctor doesn't init +0x48/+0x4C (set by Serialize or caller)
    uint8_t  field_48;           // +0x48: bool — serialized flag (has initial setup?)
    uint8_t  _pad49[3];          // +0x49: padding
    int32_t  field_4c;           // +0x4C: current slot index (round-robin for assignment)
    void*    field_50;           // +0x50: slot availability array (byte per build point, 0=free 1=busy)
    int32_t  field_58;           // +0x58: slot count (number of build points)
    uint8_t  _pad5c[4];          // +0x5C: padding
    void*    field_60;           // +0x60: binary tree root (citizen assignment tree, nodes 0x20 bytes)
    int32_t  field_68;           // +0x68: priority counter (GetPriority returns field_68 > 0)
    uint8_t  _pad6c[4];          // +0x6C: padding
};

static_assert(sizeof(BuildStructure) == 0x70, "BuildStructure size mismatch");
static_assert(offsetof(BuildStructure, vtable) == 0x00, "");
static_assert(offsetof(BuildStructure, field_48) == 0x48, "");
static_assert(offsetof(BuildStructure, field_4c) == 0x4C, "");
static_assert(offsetof(BuildStructure, field_50) == 0x50, "");
static_assert(offsetof(BuildStructure, field_58) == 0x58, "");
static_assert(offsetof(BuildStructure, field_60) == 0x60, "");
static_assert(offsetof(BuildStructure, field_68) == 0x68, "");

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
    constexpr uintptr_t Execute      = 0x1400e5ee0;
    constexpr uintptr_t OnComplete   = 0x1400e5e30;
    constexpr uintptr_t OnCancel     = 0x1400e5e00;
    constexpr uintptr_t Cleanup      = 0x1400e6650;
    constexpr uintptr_t Validate     = 0x1400e5b10;  // bug class 3 - unhookable
    constexpr uintptr_t GetTarget    = 0x1400e6720;
    constexpr uintptr_t GetPriority  = 0x140029860;  // 9 bytes - too small to hook
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
    void*    field_48;           // +0x48: BST root — entities to clear (InternalBasePtr tree nodes)
    int32_t  field_50;           // +0x50: entity tree count
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: BST root — cell indices to clear (int tree nodes)
    int32_t  field_60;           // +0x60: cell tree count
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: InternalBasePtr — map component entity
    uint8_t  field_70;           // +0x70: bool — clear trees/resources
    uint8_t  field_71;           // +0x71: bool — clear ground (version >= 1)
    uint8_t  field_72;           // +0x72: bool — clear buildings (version >= 1)
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
    void*    field_58;           // +0x58: InternalBasePtr DynArray data ptr (zeroed)
    int32_t  field_60;           // +0x60: DynArray count (zeroed)
    int32_t  field_64;           // +0x64: DynArray capacity (zeroed)
    void*    field_68;           // +0x68: int DynArray data ptr (zeroed)
    int32_t  field_70;           // +0x70: DynArray count (zeroed)
    int32_t  field_74;           // +0x74: DynArray capacity (zeroed)
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
    constexpr uintptr_t Slot4        = 0x1400f4010;
    constexpr uintptr_t OnComplete   = 0x1400f3f60;
    constexpr uintptr_t OnCancel     = 0x1400f3ea0;
    constexpr uintptr_t Cleanup      = 0x1400f4460;
    constexpr uintptr_t Validate     = 0x1400f3c70;
    constexpr uintptr_t GetTarget    = 0x1400f44e0;
}

struct PlantArea {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161890
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: workplace entity IBP
    void*    field_18;           // +0x18: entity ptr (zeroed)
    int32_t  field_20;           // +0x20: work ID (-1 sentinel)
    int32_t  field_24;           // +0x24: priority (0)
    int32_t  field_28;           // +0x28: -1 sentinel
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- PlantArea fields ---
    void*    field_48;           // +0x48: BST root (assigned work items, int-keyed)
    int32_t  field_50;           // +0x50: BST count (assigned workers)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    field_58;           // +0x58: validity byte array (one per field cell)
    int32_t  field_60;           // +0x60: field cell count
    uint8_t  _pad64[4];          // +0x64: padding
    void*    field_68;           // +0x68: ExternalPtr (crop description resource)
};

static_assert(sizeof(PlantArea) == 0x70, "PlantArea size mismatch");
static_assert(offsetof(PlantArea, vtable) == 0x00, "");
static_assert(offsetof(PlantArea, field_10) == 0x10, "");
static_assert(offsetof(PlantArea, field_20) == 0x20, "");
static_assert(offsetof(PlantArea, field_48) == 0x48, "");
static_assert(offsetof(PlantArea, field_50) == 0x50, "");
static_assert(offsetof(PlantArea, field_58) == 0x58, "");
static_assert(offsetof(PlantArea, field_60) == 0x60, "");
static_assert(offsetof(PlantArea, field_68) == 0x68, "");

// ============================================================
// PlantOrchard — orchard planting work. Size = 0x70.
// Ctor: 0x1400f50b0, Vtable: 0x140161910
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// BaseDtor: 0x1400f5010
// ============================================================
namespace PlantOrchardAddr {
    // ⚠ TWO constructors exist and they are not interchangeable:
    //   Ctor           0x1400f50b0 — the DEFAULT/zero-arg ctor (Object::Internal::Internal,
    //                                vtable_Work, manual field init, +0x68 = 0). This is the
    //                                deserialize/factory path.
    //   CtorFromEntity 0x1400f5140 — (mem, InternalBasePtr* entityIBP, ExternalPtr treeType).
    //                                This is the one the OrchardComponent state machine calls
    //                                from state 9 and state 12 enter; it sizes the per-slot
    //                                byte array from the orchard's slot count and stores the
    //                                tree type at +0x68. Anything wiring `Ctor` for the
    //                                orchard planting path is wiring the wrong constructor.
    constexpr uintptr_t Ctor           = 0x1400f50b0;
    constexpr uintptr_t CtorFromEntity = 0x1400f5140;
    constexpr uintptr_t Vtable       = 0x140161910;
    constexpr uintptr_t ScalarDtor   = 0x14002c540;
    constexpr uintptr_t BaseDtor     = 0x1400f5010;
    constexpr uintptr_t Serialize    = 0x1400f4f50;
    constexpr uintptr_t Slot4        = 0x1400f4af0;
    constexpr uintptr_t OnComplete   = 0x1400f4a90;
    constexpr uintptr_t OnCancel     = 0x1400f4a20;
    constexpr uintptr_t Cleanup      = 0x1400f4e70;
    constexpr uintptr_t Validate     = 0x1400f47f0;
    constexpr uintptr_t GetTarget    = 0x1400f4ef0;
}

struct PlantOrchard {
    // --- Work base (0x48 bytes) ---
    void*    vtable;             // +0x00: -> 0x140161910
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    field_10;           // +0x10: workplace entity IBP
    void*    field_18;           // +0x18: entity ptr (zeroed)
    int32_t  field_20;           // +0x20: work ID (-1 sentinel)
    int32_t  field_24;           // +0x24: priority (0)
    int32_t  field_28;           // +0x28: -1 sentinel
    int32_t  field_2c;           // +0x2C: 0
    void*    field_30;           // +0x30: zeroed
    void*    field_38;           // +0x38: zeroed
    int32_t  field_40;           // +0x40: 0
    uint8_t  _pad44[4];          // +0x44: padding

    // --- PlantOrchard fields ---
    void*    field_48;           // +0x48: BST root (assigned work items, int-keyed)
    int32_t  field_50;           // +0x50: BST count (assigned workers)
    uint8_t  _pad54[4];          // +0x54: padding
    // +0x58/+0x60/+0x64 are ONE dynamic-array triple {base, count, capacity} — the same
    // shape as OrchardComponent's +0x50/+0x58/+0x5c. 0x14000a590 (the resizer the ctor
    // calls) reads capacity at +0x0c of the triple, so +0x64 is NOT padding.
    void*    field_58;           // +0x58: validity byte array base (one byte per orchard slot)
    int32_t  field_60;           // +0x60: element count (= OrchardComponent+0x58 slot count)
    int32_t  field_64;           // +0x64: capacity (was mislabelled padding)
    void*    field_68;           // +0x68: ExternalPtr to the TREE TYPE template chosen in
                                 //        OrchardUI's combo (copied from OrchardComponent+0x60).
                                 //        NOT the OrchardDescription — the old comment misled.
};

static_assert(sizeof(PlantOrchard) == 0x70, "PlantOrchard size mismatch");
static_assert(offsetof(PlantOrchard, vtable) == 0x00, "");
static_assert(offsetof(PlantOrchard, field_10) == 0x10, "");
static_assert(offsetof(PlantOrchard, field_20) == 0x20, "");
static_assert(offsetof(PlantOrchard, field_48) == 0x48, "");
static_assert(offsetof(PlantOrchard, field_50) == 0x50, "");
static_assert(offsetof(PlantOrchard, field_58) == 0x58, "");
static_assert(offsetof(PlantOrchard, field_60) == 0x60, "");
static_assert(offsetof(PlantOrchard, field_68) == 0x68, "");

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
