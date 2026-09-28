#pragma once
// ============================================================================
// code/Game/Component/ModelDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ModelDescription", parent "Description", ti 0x1401c7a88
//   create 0x140035930: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400359a0.
//   table 0x140163c20, 10 slots:
//     [0] 0x140035a70 scalar deleting dtor   [1] 0x140035920 TypeName   [2] 0x140035910 Type
//     [3] 0x140124ef0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140124dc0 Serialize (`MOV AL,1`)
//     [6] 0x140035a00 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140035a60 GetComponentType: ModelComponent's TypeId
//     [9] 0x1400359a0 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ModelDescription final : public Description {
    DECLARE_EXE_TYPE(ModelDescription)

public:
    ModelDescription();

    ~ModelDescription() override;                                          // [0] 0x140035a70
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140124ef0
    bool              Serialize(void* stream) override;                    // [5] 0x140124dc0
    Object::External* Reconstruct() override;                              // [6] 0x140035a00
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140035a60
    virtual Object::External* Construct();                                 // [9] 0x1400359a0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _meshGroups;                          // +0x20 MeshGroup array data (0x18 stride: mesh, material array, count, cap)
    int32_t _meshGroupCount;                    // +0x28
    int32_t _meshGroupCap;                      // +0x2c
    int32_t _lod0;                              // +0x30
    int32_t _lod1;                              // +0x34
    bool _castShadow;                           // +0x38
    uint8_t _pad39[7];                          // +0x39 never written by the ctor
};

static_assert(sizeof(ModelDescription) == 0x40, "ModelDescription is 0x40 (create 0x140035930 MemAlloc(0x40))");
static_assert(alignof(ModelDescription) == 8, "ModelDescription alignment");
static_assert(offsetof(ModelDescription, _meshGroups) == 0x20, "ModelDescription::_meshGroups @ +0x20");
static_assert(offsetof(ModelDescription, _meshGroupCount) == 0x28, "ModelDescription::_meshGroupCount @ +0x28");
static_assert(offsetof(ModelDescription, _meshGroupCap) == 0x2c, "ModelDescription::_meshGroupCap @ +0x2c");
static_assert(offsetof(ModelDescription, _lod0) == 0x30, "ModelDescription::_lod0 @ +0x30");
static_assert(offsetof(ModelDescription, _lod1) == 0x34, "ModelDescription::_lod1 @ +0x34");
static_assert(offsetof(ModelDescription, _castShadow) == 0x38, "ModelDescription::_castShadow @ +0x38");
