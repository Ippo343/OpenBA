#pragma once
// ============================================================================
// code/Game/Component/StorageDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StorageDescription", parent "Description", ti 0x1401c7eb0
//   create 0x140038bb0: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140038c20.
//   table 0x1401647a0, 10 slots:
//     [0] 0x140038cf0 scalar deleting dtor   [1] 0x140038ba0 TypeName   [2] 0x140038b90 Type
//     [3] 0x14012caa0 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14012c9a0 Serialize (`MOV AL,1`)
//     [6] 0x140038c80 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140038ce0 GetComponentType: StorageComponent's TypeId
//     [9] 0x140038c20 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;   // Component/ComponentDescription.h

class StorageDescription final : public Description {
    DECLARE_EXE_TYPE(StorageDescription)

public:
    StorageDescription();

    ~StorageDescription() override;                                        // [0] 0x140038cf0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14012caa0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14012c9a0
    Object::External* Reconstruct() override;                              // [6] 0x140038c80
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140038ce0
    virtual Object::External* Construct();                                 // [9] 0x140038c20

    // One `_volumes` element, 0x10 bytes (Serialize 0x14012c9a0: Serialize(int&) +0x08,
    // then ExternalPtr<ComponentDescription>::Serialize 0x140002340 +0x00): a material
    // the storage starts with and how much of it (StorageComponent::OnAdded 0x14012c640).
    struct Volume {
        ComponentDescription* _resource;   // +0x00 ExternalPtr<ComponentDescription>
        int32_t               _count;      // +0x08
        int32_t               _pad0C;      // +0x0C zeroed with _count by the tree's load (one qword)
    };

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    bool _flagA;                                // +0x20
    bool _flagB;                                // +0x21
    uint8_t _pad22[2];                          // +0x22 never written by the ctor
    int32_t _pickupValue;                       // +0x24
    uint32_t _rawMaterialFlags;                 // +0x28 enum (serialized through the uint door)
    uint8_t _pad2c[4];                          // +0x2c never written by the ctor
    Volume* _volumes;                           // +0x30 Array<Volume> data (H5: the raw triple)
    int32_t _volumeCount;                       // +0x38
    int32_t _volumeCap;                         // +0x3c
};

static_assert(sizeof(StorageDescription) == 0x40, "StorageDescription is 0x40 (create 0x140038bb0 MemAlloc(0x40))");
static_assert(alignof(StorageDescription) == 8, "StorageDescription alignment");
static_assert(offsetof(StorageDescription, _flagA) == 0x20, "StorageDescription::_flagA @ +0x20");
static_assert(offsetof(StorageDescription, _flagB) == 0x21, "StorageDescription::_flagB @ +0x21");
static_assert(offsetof(StorageDescription, _pickupValue) == 0x24, "StorageDescription::_pickupValue @ +0x24");
static_assert(offsetof(StorageDescription, _rawMaterialFlags) == 0x28, "StorageDescription::_rawMaterialFlags @ +0x28");
static_assert(offsetof(StorageDescription, _volumes) == 0x30, "StorageDescription::_volumes @ +0x30");
static_assert(sizeof(StorageDescription::Volume) == 0x10, "volume stride 0x10 (Serialize 0x14012c9a0)");
static_assert(offsetof(StorageDescription::Volume, _count) == 0x08, "volume count +0x08 (Serialize 0x14012ca5d)");
static_assert(offsetof(StorageDescription, _volumeCount) == 0x38, "StorageDescription::_volumeCount @ +0x38");
static_assert(offsetof(StorageDescription, _volumeCap) == 0x3c, "StorageDescription::_volumeCap @ +0x3c");
