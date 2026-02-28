#pragma once
// Build action types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// BuildRoad and BuildStructure are Action-derived types for construction.
// Both are 0x70 bytes with dedicated constructors.

#include <cstdint>
#include <cstddef>

// BuildRoad — action to build a road segment. Size = 0x70.
// Decompiled from: BuildRoad_Factory (0x140029560) -> BuildRoad_Ctor
namespace BuildRoadAddr {
    constexpr uintptr_t Factory = 0x140029560;
}

struct BuildRoad {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding
    // --- BuildRoad fields (0x50 bytes) ---
    uint8_t  _fields[0x50];      // +0x20: build parameters, coordinates, etc.
};

static_assert(sizeof(BuildRoad) == 0x70, "BuildRoad size mismatch");

// BuildStructure — action to build a structure. Size = 0x70.
// Decompiled from: BuildStructure_Factory (0x140029630) -> BuildStructure_Ctor
namespace BuildStructureAddr {
    constexpr uintptr_t Factory = 0x140029630;
}

struct BuildStructure {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding
    // --- BuildStructure fields (0x50 bytes) ---
    uint8_t  _fields[0x50];      // +0x20: build parameters
};

static_assert(sizeof(BuildStructure) == 0x70, "BuildStructure size mismatch");

// GatherRawMaterials — action to gather raw materials. Size = 0x78.
// Decompiled from: GatherRawMaterials_Factory (0x14002a3c0) -> GatherRawMaterials_Ctor
namespace GatherRawMaterialsAddr {
    constexpr uintptr_t Factory = 0x14002a3c0;
}

struct GatherRawMaterials {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding
    // --- GatherRawMaterials fields (0x58 bytes) ---
    uint8_t  _fields[0x58];      // +0x20: gather parameters
};

static_assert(sizeof(GatherRawMaterials) == 0x78, "GatherRawMaterials size mismatch");

// Pickup — action to pick up items. Size = 0x60.
// Decompiled from: Pickup_Factory (0x14002bfc0) -> Pickup_Ctor
namespace PickupAddr {
    constexpr uintptr_t Factory = 0x14002bfc0;
}

struct Pickup {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding
    // --- Pickup fields (0x40 bytes) ---
    uint8_t  _fields[0x40];      // +0x20: pickup parameters
};

static_assert(sizeof(Pickup) == 0x60, "Pickup size mismatch");

// ConsumeProduce — action related to consume/produce cycle. Size = 0x78.
// Decompiled from: ConsumeProduce_Factory (0x14002a150) -> ConsumeProduce_Ctor
namespace ConsumeProduceActionAddr {
    constexpr uintptr_t Factory = 0x14002a150;
}

struct ConsumeProduceAction {
    // --- Action base (0x20 bytes) ---
    void*    vtable;             // +0x00
    uint8_t  _base08[8];         // +0x08: Object::Internal base
    void*    entity;             // +0x10: entity pointer
    uint8_t  status;             // +0x18: status byte
    uint8_t  _pad19[7];          // +0x19: padding
    // --- ConsumeProduce fields (0x58 bytes) ---
    uint8_t  _fields[0x58];      // +0x20
};

static_assert(sizeof(ConsumeProduceAction) == 0x78, "ConsumeProduceAction size mismatch");
