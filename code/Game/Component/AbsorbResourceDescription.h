#pragma once
// ============================================================================
// code/Game/Component/AbsorbResourceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AbsorbResourceDescription", parent "Description", ti 0x1401c6f60
//   create 0x14002ea80: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14002eae0.
//   table 0x140162090, 10 slots:
//     [0] 0x14002eb90 scalar deleting dtor   [1] 0x14002ea70 TypeName   [2] 0x14002ea60 Type
//     [3] 0x140105d90 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140105d50 Serialize (`MOV AL,1`)
//     [6] 0x14002eb30 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14002eb80 GetComponentType: AbsorbResourceComponent's TypeId
//     [9] 0x14002eae0 Construct (folded with createInPlace)
//   slot 0: the tree's hand-built slot 0 never ran ~External; the compiler chain now does (B38 D2, behaviour)
//   sizeof 0x20.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class AbsorbResourceDescription final : public Description {
    DECLARE_EXE_TYPE(AbsorbResourceDescription)

public:
    AbsorbResourceDescription();

    ~AbsorbResourceDescription() override = default;                       // [0] 0x14002eb90
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140105d90
    bool              Serialize(void* stream) override;                    // [5] 0x140105d50
    Object::External* Reconstruct() override;                              // [6] 0x14002eb30
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14002eb80
    virtual Object::External* Construct();                                 // [9] 0x14002eae0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----

};

static_assert(sizeof(AbsorbResourceDescription) == 0x20, "AbsorbResourceDescription is 0x20 (create 0x14002ea80 MemAlloc(0x20))");
static_assert(alignof(AbsorbResourceDescription) == 8, "AbsorbResourceDescription alignment");

