#pragma once
// ============================================================================
// code/Game/Component/BoatDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "BoatDescription", parent "Description", ti 0x1401c4058
//   create 0x140003760: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x1400037c0.
//   table 0x140158fd0, 10 slots:
//     [0] 0x140003910 scalar deleting dtor   [1] 0x140003750 TypeName   [2] 0x140003740 Type
//     [3] 0x140046a00 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x1400469c0 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x140003810 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140003860 GetComponentType: BoatComponent's TypeId
//     [9] 0x1400037c0 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class BoatDescription final : public Description {
    DECLARE_EXE_TYPE(BoatDescription)

public:
    BoatDescription() = default;

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140003910
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140046a00
    bool              Serialize(void* stream) override;                    // [5] 0x1400469c0
    Object::External* Reconstruct() override;                              // [6] 0x140003810
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140003860
    virtual Object::External* Construct();                                 // [9] 0x1400037c0

protected:
    ~BoatDescription() = default;
};

static_assert(sizeof(BoatDescription) == 0x20, "BoatDescription is 0x20 (create 0x140003760 MemAlloc(0x20))");
static_assert(alignof(BoatDescription) == 8, "BoatDescription alignment");
