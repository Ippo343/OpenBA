#pragma once
// ============================================================================
// code/Game/Component/StateMachineDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "StateMachineDescription", parent "Description", ti 0x1401c7dd0
//   create 0x140038330: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140038390.
//   table 0x1401645a0, 10 slots:
//     [0] 0x140038440 scalar deleting dtor   [1] 0x140038320 TypeName   [2] 0x140038310 Type
//     [3] 0x14012bb40 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14012bb00 Serialize (`MOV AL,1`)
//     [6] 0x1400383e0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140038430 GetComponentType: StateMachineComponent's TypeId
//     [9] 0x140038390 Construct (folded with createInPlace)
//   slot 0: the tree's hand-built slot 0 never ran ~External; the compiler chain now does (B38 D2, behaviour)
//   sizeof 0x20.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class StateMachineDescription final : public Description {
    DECLARE_EXE_TYPE(StateMachineDescription)

public:
    StateMachineDescription();

    ~StateMachineDescription() override = default;                         // [0] 0x140038440
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14012bb40
    bool              Serialize(void* stream) override;                    // [5] 0x14012bb00
    Object::External* Reconstruct() override;                              // [6] 0x1400383e0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140038430
    virtual Object::External* Construct();                                 // [9] 0x140038390

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----

};

static_assert(sizeof(StateMachineDescription) == 0x20, "StateMachineDescription is 0x20 (create 0x140038330 MemAlloc(0x20))");
static_assert(alignof(StateMachineDescription) == 8, "StateMachineDescription alignment");

