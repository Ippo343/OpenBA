#pragma once
// ============================================================================
// code/Game/Component/WorkPlaceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "WorkPlaceDescription", parent "Description", ti 0x1401c8118
//   create 0x14003a090: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14003a100.
//   table 0x140164dc0, 10 slots:
//     [0] 0x14003a1c0 scalar deleting dtor   [1] 0x14003a080 TypeName   [2] 0x14003a070 Type
//     [3] 0x14012f480 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14012f420 Serialize (`MOV AL,1`)
//     [6] 0x14003a160 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14003a1b0 GetComponentType: WorkPlaceComponent's TypeId
//     [9] 0x14003a100 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
class Profession;   // Game/Profession.h

class WorkPlaceDescription final : public Description {
    DECLARE_EXE_TYPE(WorkPlaceDescription)

public:
    WorkPlaceDescription();

    ~WorkPlaceDescription() override = default;                            // [0] 0x14003a1c0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14012f480
    bool              Serialize(void* stream) override;                    // [5] 0x14012f420
    Object::External* Reconstruct() override;                              // [6] 0x14003a160
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14003a1b0
    virtual Object::External* Construct();                                 // [9] 0x14003a100

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    Profession* _profession0;                   // +0x20 ExternalPtr<Profession> (Serialize 0x14012f420: 0x140018020)
    Profession* _profession1;                   // +0x28 ExternalPtr<Profession> (0x140018020)
};

static_assert(sizeof(WorkPlaceDescription) == 0x30, "WorkPlaceDescription is 0x30 (create 0x14003a090 MemAlloc(0x30))");
static_assert(alignof(WorkPlaceDescription) == 8, "WorkPlaceDescription alignment");
static_assert(offsetof(WorkPlaceDescription, _profession0) == 0x20, "WorkPlaceDescription::_profession0 @ +0x20");
static_assert(offsetof(WorkPlaceDescription, _profession1) == 0x28, "WorkPlaceDescription::_profession1 @ +0x28");
