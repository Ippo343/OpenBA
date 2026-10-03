#pragma once
// ============================================================================
// code/Game/Component/MineDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MineDescription", parent "Description", ti 0x1401c4608
//   create 0x140008e30: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140008e90.
//   table 0x140159f50, 10 slots:
//     [0] 0x140008f40 scalar deleting dtor   [1] 0x140008e20 TypeName   [2] 0x140008e10 Type
//     [3] 0x14004f390 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14004f350 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x140008ee0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140008f30 GetComponentType: MineComponent's TypeId
//     [9] 0x140008e90 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class MineDescription final : public Description {
    DECLARE_EXE_TYPE(MineDescription)

public:
    MineDescription() = default;

    ~MineDescription() override = default;                                 // [0] 0x140008f40
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14004f390
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14004f350
    Object::External* Reconstruct() override;                              // [6] 0x140008ee0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140008f30
    virtual Object::External* Construct();                                 // [9] 0x140008e90
};

static_assert(sizeof(MineDescription) == 0x20, "MineDescription is 0x20 (create 0x140008e30 MemAlloc(0x20))");
static_assert(alignof(MineDescription) == 8, "MineDescription alignment");
