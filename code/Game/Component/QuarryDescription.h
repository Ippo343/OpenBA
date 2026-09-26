#pragma once
// ============================================================================
// code/Game/Component/QuarryDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "QuarryDescription", parent "Description", ti 0x1401c4838
//   create 0x14000b430: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000b4a0.
//   table 0x14015a510, 10 slots:
//     [0] 0x14000b570 scalar deleting dtor   [1] 0x14000b420 TypeName   [2] 0x14000b410 Type
//     [3] 0x140055390 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x140055320 Serialize (`MOV AL,1`)
//     [6] 0x14000b500 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000b560 GetComponentType: QuarryComponent's TypeId
//     [9] 0x14000b4a0 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class QuarryDescription final : public Description {
    DECLARE_EXE_TYPE(QuarryDescription)

public:
    QuarryDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14000b570
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x140055390
    bool              Serialize(void* stream) override;                    // [5] 0x140055320
    Object::External* Reconstruct() override;                              // [6] 0x14000b500
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000b560
    virtual Object::External* Construct();                                 // [9] 0x14000b4a0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _product;                             // +0x20 ExternalPtr (product material)
    float _minDepth;                            // +0x28
    float _maxDepth;                            // +0x2c

protected:
    ~QuarryDescription() = default;
};

static_assert(sizeof(QuarryDescription) == 0x30, "QuarryDescription is 0x30 (create 0x14000b430 MemAlloc(0x30))");
static_assert(alignof(QuarryDescription) == 8, "QuarryDescription alignment");
static_assert(offsetof(QuarryDescription, _product) == 0x20, "QuarryDescription::_product @ +0x20");
static_assert(offsetof(QuarryDescription, _minDepth) == 0x28, "QuarryDescription::_minDepth @ +0x28");
static_assert(offsetof(QuarryDescription, _maxDepth) == 0x2c, "QuarryDescription::_maxDepth @ +0x2c");
