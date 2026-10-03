#pragma once
// ============================================================================
// code/Game/Component/NomadDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "NomadDescription", parent "Description", ti 0x1401c4678
//   create 0x140009090: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140009100.
//   table 0x14015a0b0, 10 slots:
//     [0] 0x1400091d0 scalar deleting dtor   [1] 0x140009080 TypeName   [2] 0x140009070 Type
//     [3] 0x14004ff90 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14004ff20 Serialize (`MOV AL,1`)
//     [6] 0x140009160 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400091c0 GetComponentType: NomadComponent's TypeId
//     [9] 0x140009100 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;
namespace Graphics { class MaterialInstance; }

class NomadDescription final : public Description {
    DECLARE_EXE_TYPE(NomadDescription)

public:
    NomadDescription();

    ~NomadDescription() override = default;                                // [0] 0x1400091d0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14004ff90
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14004ff20
    Object::External* Reconstruct() override;                              // [6] 0x140009160
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400091c0
    virtual Object::External* Construct();                                 // [9] 0x140009100

    // ---- fields (B.4 COMP-d: from what NomadComponent does with them, Init
    // 0x140050030 / ConvertToCitizen 0x1400501a0) ----
    ComponentDescription*       _citizen;          // +0x20 ExternalPtr: the citizen entity a nomad becomes
    Graphics::MaterialInstance* _materialGender0;  // +0x28 ExternalPtr: MeshInstance material 0 when gender == 0
    Graphics::MaterialInstance* _materialGender1;  // +0x30 ExternalPtr: material 0 otherwise
};

static_assert(sizeof(NomadDescription) == 0x38, "NomadDescription is 0x38 (create 0x140009090 MemAlloc(0x38))");
static_assert(alignof(NomadDescription) == 8, "NomadDescription alignment");
static_assert(offsetof(NomadDescription, _citizen) == 0x20, "NomadDescription::_citizen @ +0x20");
static_assert(offsetof(NomadDescription, _materialGender0) == 0x28, "NomadDescription::_materialGender0 @ +0x28");
static_assert(offsetof(NomadDescription, _materialGender1) == 0x30, "NomadDescription::_materialGender1 @ +0x30");
