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
#include "Work/BuildStructure.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/ClearArea.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/ConsumeProduce.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/GatherRawMaterials.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/Pickup.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)
#include "Work/PlantArea.h"   // ENDGAME B.3.8 (WRK): the real class; its POD is gone (B8-4)

// BuildRoad — the real class code/Game/Work/BuildRoad.h (ENDGAME B.3.8).
// BuildStructure — the real class code/Game/Work/BuildStructure.h (ENDGAME B.3.8).

// ClearArea — the real class code/Game/Work/ClearArea.h (ENDGAME B.3.8).
// ConsumeProduce (the POD was ConsumeProduceAction) — the real class
// code/Game/Work/ConsumeProduce.h (ENDGAME B.3.8).

// GatherRawMaterials — the real class code/Game/Work/GatherRawMaterials.h (ENDGAME B.3.8).

// PlantArea — the real class code/Game/Work/PlantArea.h (ENDGAME B.3.8).

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

// Pickup — the real class code/Game/Work/Pickup.h (ENDGAME B.3.8).
