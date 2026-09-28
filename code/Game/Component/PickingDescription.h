#pragma once
// ============================================================================
// code/Game/Component/PickingDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PickingDescription", parent "Description", ti 0x1401c7c48
//   create 0x140036e00: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140036e70.
//   table 0x1401640c0, 10 slots:
//     [0] 0x140036f40 scalar deleting dtor   [1] 0x140036df0 TypeName   [2] 0x140036de0 Type
//     [3] 0x140128e00 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x140128d00 Serialize (`MOV AL,1`)
//     [6] 0x140036ed0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140036f30 GetComponentType: PickingComponent's TypeId
//     [9] 0x140036e70 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class PickingDescription final : public Description {
    DECLARE_EXE_TYPE(PickingDescription)

public:
    PickingDescription();

    ~PickingDescription() override;                                        // [0] 0x140036f40
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x140128e00
    bool              Serialize(void* stream) override;                    // [5] 0x140128d00
    Object::External* Reconstruct() override;                              // [6] 0x140036ed0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140036f30
    virtual Object::External* Construct();                                 // [9] 0x140036e70

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _groups;                              // +0x20 group array data (0x10 stride: {External* array, count, cap})
    int32_t _groupCount;                        // +0x28
    int32_t _groupCap;                          // +0x2c
    void* _meshRef;                             // +0x30 ExternalPtr
    bool _enabled;                              // +0x38
    uint8_t _pad39[7];                          // +0x39 never written by the ctor
};

static_assert(sizeof(PickingDescription) == 0x40, "PickingDescription is 0x40 (create 0x140036e00 MemAlloc(0x40))");
static_assert(alignof(PickingDescription) == 8, "PickingDescription alignment");
static_assert(offsetof(PickingDescription, _groups) == 0x20, "PickingDescription::_groups @ +0x20");
static_assert(offsetof(PickingDescription, _groupCount) == 0x28, "PickingDescription::_groupCount @ +0x28");
static_assert(offsetof(PickingDescription, _groupCap) == 0x2c, "PickingDescription::_groupCap @ +0x2c");
static_assert(offsetof(PickingDescription, _meshRef) == 0x30, "PickingDescription::_meshRef @ +0x30");
static_assert(offsetof(PickingDescription, _enabled) == 0x38, "PickingDescription::_enabled @ +0x38");
