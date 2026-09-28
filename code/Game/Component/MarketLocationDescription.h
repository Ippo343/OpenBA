#pragma once
// ============================================================================
// code/Game/Component/MarketLocationDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MarketLocationDescription", parent "Description", ti 0x1401c4598
//   create 0x140008720: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140008780.
//   table 0x140159e60, 10 slots:
//     [0] 0x1400088a0 scalar deleting dtor   [1] 0x140008710 TypeName   [2] 0x140008700 Type
//     [3] 0x14004f1b0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14004f170 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x1400087d0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140008820 GetComponentType: MarketLocationComponent's TypeId
//     [9] 0x140008780 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class MarketLocationDescription final : public Description {
    DECLARE_EXE_TYPE(MarketLocationDescription)

public:
    MarketLocationDescription() = default;

    ~MarketLocationDescription() override = default;                       // [0] 0x1400088a0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14004f1b0
    bool              Serialize(void* stream) override;                    // [5] 0x14004f170
    Object::External* Reconstruct() override;                              // [6] 0x1400087d0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140008820
    virtual Object::External* Construct();                                 // [9] 0x140008780
};

static_assert(sizeof(MarketLocationDescription) == 0x20, "MarketLocationDescription is 0x20 (create 0x140008720 MemAlloc(0x20))");
static_assert(alignof(MarketLocationDescription) == 8, "MarketLocationDescription alignment");
