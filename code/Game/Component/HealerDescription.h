#pragma once
// ============================================================================
// code/Game/Component/HealerDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HealerDescription", parent "Description", ti 0x1401c4368
//   create 0x1400064f0: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140006550.
//   table 0x1401597c0, 10 slots:
//     [0] 0x140006600 scalar deleting dtor   [1] 0x1400064e0 TypeName   [2] 0x1400064d0 Type
//     [3] 0x14004b0d0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14004b090 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x1400065a0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400065f0 GetComponentType: HealerComponent's TypeId
//     [9] 0x140006550 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class HealerDescription final : public Description {
    DECLARE_EXE_TYPE(HealerDescription)

public:
    HealerDescription() = default;

    ~HealerDescription() override = default;                               // [0] 0x140006600
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14004b0d0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14004b090
    Object::External* Reconstruct() override;                              // [6] 0x1400065a0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400065f0
    virtual Object::External* Construct();                                 // [9] 0x140006550
};

static_assert(sizeof(HealerDescription) == 0x20, "HealerDescription is 0x20 (create 0x1400064f0 MemAlloc(0x20))");
static_assert(alignof(HealerDescription) == 8, "HealerDescription alignment");
