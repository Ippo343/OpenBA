#pragma once
// ============================================================================
// code/Game/Component/DecalDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "DecalDescription", parent "Description", ti 0x1401c75b8
//   create 0x140033820: MemAlloc(0x50, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140033860.
//   table 0x140163050, 10 slots:
//     [0] 0x1400338a0 scalar deleting dtor   [1] 0x140033810 TypeName   [2] 0x140033800 Type
//     [3] 0x14011bb00 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x14011b980 Serialize (`MOV AL,1`)
//     [6] 0x140033870 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140033890 GetComponentType: DecalComponent's TypeId
//     [9] 0x140033860 Construct (folded with createInPlace)
//   sizeof 0x50.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class DecalDescription final : public Description {
    DECLARE_EXE_TYPE(DecalDescription)

public:
    DecalDescription();

    ~DecalDescription() override;                                          // [0] 0x1400338a0
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x14011bb00
    bool              Serialize(void* stream) override;                    // [5] 0x14011b980
    Object::External* Reconstruct() override;                              // [6] 0x140033870
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140033890
    virtual Object::External* Construct();                                 // [9] 0x140033860

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _sizeA;                               // +0x20
    bool _flagA;                                // +0x24
    bool _flagB;                                // +0x25
    uint8_t _pad26[2];                          // +0x26 never written by the ctor
    void* _refs;                                // +0x28 External* array data
    int32_t _refCount;                          // +0x30
    int32_t _refCap;                            // +0x34
    void* _quads;                               // +0x38 quad array data (4 floats, 0x10 stride)
    int32_t _quadCount;                         // +0x40
    int32_t _quadCap;                           // +0x44
    float _sizeB;                               // +0x48
    uint8_t _pad4c[4];                          // +0x4c never written by the ctor
};

static_assert(sizeof(DecalDescription) == 0x50, "DecalDescription is 0x50 (create 0x140033820 MemAlloc(0x50))");
static_assert(alignof(DecalDescription) == 8, "DecalDescription alignment");
static_assert(offsetof(DecalDescription, _sizeA) == 0x20, "DecalDescription::_sizeA @ +0x20");
static_assert(offsetof(DecalDescription, _flagA) == 0x24, "DecalDescription::_flagA @ +0x24");
static_assert(offsetof(DecalDescription, _flagB) == 0x25, "DecalDescription::_flagB @ +0x25");
static_assert(offsetof(DecalDescription, _refs) == 0x28, "DecalDescription::_refs @ +0x28");
static_assert(offsetof(DecalDescription, _refCount) == 0x30, "DecalDescription::_refCount @ +0x30");
static_assert(offsetof(DecalDescription, _refCap) == 0x34, "DecalDescription::_refCap @ +0x34");
static_assert(offsetof(DecalDescription, _quads) == 0x38, "DecalDescription::_quads @ +0x38");
static_assert(offsetof(DecalDescription, _quadCount) == 0x40, "DecalDescription::_quadCount @ +0x40");
static_assert(offsetof(DecalDescription, _quadCap) == 0x44, "DecalDescription::_quadCap @ +0x44");
static_assert(offsetof(DecalDescription, _sizeB) == 0x48, "DecalDescription::_sizeB @ +0x48");
