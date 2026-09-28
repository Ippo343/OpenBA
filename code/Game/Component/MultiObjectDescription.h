#pragma once
// ============================================================================
// code/Game/Component/MultiObjectDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MultiObjectDescription", parent "Description", ti 0x1401c7af8
//   create 0x140035f50: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140035fb0.
//   table 0x140163d80, 10 slots:
//     [0] 0x140036060 scalar deleting dtor   [1] 0x140035f40 TypeName   [2] 0x140035f30 Type
//     [3] 0x140125b90 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140125b50 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x140036000 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140036050 GetComponentType: MultiObjectComponent's TypeId
//     [9] 0x140035fb0 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class MultiObjectDescription final : public Description {
    DECLARE_EXE_TYPE(MultiObjectDescription)

public:
    MultiObjectDescription() = default;

    ~MultiObjectDescription() override = default;                          // [0] 0x140036060
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140125b90
    bool              Serialize(void* stream) override;                    // [5] 0x140125b50
    Object::External* Reconstruct() override;                              // [6] 0x140036000
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140036050
    virtual Object::External* Construct();                                 // [9] 0x140035fb0
};

static_assert(sizeof(MultiObjectDescription) == 0x20, "MultiObjectDescription is 0x20 (create 0x140035f50 MemAlloc(0x20))");
static_assert(alignof(MultiObjectDescription) == 8, "MultiObjectDescription alignment");
