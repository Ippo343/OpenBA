#pragma once
// ============================================================================
// code/Game/Component/CitizenAIDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "CitizenAIDescription", parent "Description", ti 0x1401c7120
//   create 0x14002fb80: MemAlloc(0x20, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14002fbe0.
//   table 0x1401624f0, 10 slots:
//     [0] 0x14002fc90 scalar deleting dtor   [1] 0x14002fb70 TypeName   [2] 0x14002fb60 Type
//     [3] 0x14010fc80 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14010fc40 Serialize (`MOV AL,1`)
//     [6] 0x14002fc30 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14002fc80 GetComponentType: CitizenAIComponent's TypeId
//     [9] 0x14002fbe0 Construct (folded with createInPlace)
//   slot 0: the tree's hand-built slot 0 never ran ~External; the compiler chain now does (B38 D2, behaviour)
//   sizeof 0x20.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class CitizenAIDescription final : public Description {
    DECLARE_EXE_TYPE(CitizenAIDescription)

public:
    CitizenAIDescription();

    ~CitizenAIDescription() override = default;                            // [0] 0x14002fc90
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14010fc80
    bool              Serialize(void* stream) override;                    // [5] 0x14010fc40
    Object::External* Reconstruct() override;                              // [6] 0x14002fc30
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14002fc80
    virtual Object::External* Construct();                                 // [9] 0x14002fbe0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----

};

static_assert(sizeof(CitizenAIDescription) == 0x20, "CitizenAIDescription is 0x20 (create 0x14002fb80 MemAlloc(0x20))");
static_assert(alignof(CitizenAIDescription) == 8, "CitizenAIDescription alignment");

