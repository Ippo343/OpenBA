#pragma once
// ============================================================================
// code/Game/Component/ClearResourceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ClearResourceDescription", parent "Description", ti 0x1401c7270
//   create 0x140031600: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp. createInPlace 0x140031660.
//   table 0x1401628d0, 10 slots:
//     [0] 0x140031710 scalar deleting dtor   [1] 0x1400315f0 TypeName   [2] 0x1400315e0 Type
//     [3] 0x140118770 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140118730 Serialize: an EMPTY Serializer(stream, 0) scope, `MOV AL,1`
//     [6] 0x1400316b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140031700 GetComponentType: ClearResourceComponent's TypeId
//     [9] 0x140031660 Construct (folded with createInPlace)
//   sizeof 0x20 — no fields beyond Object::External.
// ============================================================================

#include <cstdint>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ClearResourceDescription final : public Description {
    DECLARE_EXE_TYPE(ClearResourceDescription)

public:
    ClearResourceDescription() = default;

    ~ClearResourceDescription() override = default;                        // [0] 0x140031710
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140118770
    bool              Serialize(void* stream) override;                    // [5] 0x140118730
    Object::External* Reconstruct() override;                              // [6] 0x1400316b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140031700
    virtual Object::External* Construct();                                 // [9] 0x140031660
};

static_assert(sizeof(ClearResourceDescription) == 0x20, "ClearResourceDescription is 0x20 (create 0x140031600 MemAlloc(0x20))");
static_assert(alignof(ClearResourceDescription) == 8, "ClearResourceDescription alignment");
