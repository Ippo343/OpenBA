#pragma once
// ============================================================================
// code/Game/Game/ProfessionList.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `ProfessionList` (0x50): the container of profession references, a real
// `Object::External` subclass. Layout from the inline ctor in factory
// 0x14001e5c0.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "ProfessionList", parent "Object::External", table 0x14015ef50,
//   9 slots, sizeof 0x50.
//   [0] 0x14001e760 scalar deleting dtor   [1] TypeName  [2] Type
//   [3] GetFileVersion `MOV EAX,2`         [4] IMPORT Object::External::Compile
//   [5] 0x1400a8e30 Serialize (AL=1)       [6] 0x14001e690 Reconstruct
//   [7] 0x140001640 Destruct               [8] 0x14001e630 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class Profession;

class ProfessionList final : public Object::External {
    DECLARE_EXE_TYPE(ProfessionList)

public:
    ProfessionList();                                             // factory 0x14001e5c0 body

    ~ProfessionList() override;                                   // [0] 0x14001e760
    int32_t GetFileVersion() const override { return 2; }         // [3]
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(void* stream) override;                        // [5] 0x1400a8e30
    Object::External* Reconstruct() override;                     // [6] 0x14001e690
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x14001e630

public:
    // B.4 W0-H1: the ctor (inline in the factory 0x14001e5c0) zeroes +0x20..+0x4F
    // with six qword stores; the serializer 0x1400a8e30 writes only the four
    // ExternalPtr<Profession> at +0x30..+0x48 (0x140018020); the array at +0x20 is
    // built at Startup (ProfessionList_Startup_Impl) and freed by the dtor
    // 0x14001e760 (H5: kept as raw fields, the tree's dtor runs ~Array on them by
    // hand). +0x40 / +0x48 are named from PostLifeEvent, which titles a child
    // with +0x40's name and a student with +0x48's; +0x30 / +0x38 keep typed
    // guess names.
    Profession** arrayData;      // +0x20: Collection::Array<Profession*> data (H5 raw; B4 H4: typed, DLG-c)
    int32_t  arrayCount;         // +0x28: Array count
    int32_t  arrayCapacity;      // +0x2C: Array capacity
    Profession* field_0x30;      // +0x30: ExternalPtr<Profession>
    Profession* field_0x38;      // +0x38: ExternalPtr<Profession>
    Profession* childProfession;   // +0x40: ExternalPtr<Profession>: a child's title (PostLifeEvent)
    Profession* studentProfession; // +0x48: ExternalPtr<Profession>: a student's title (PostLifeEvent)
};

static_assert(sizeof(ProfessionList) == 0x50, "ProfessionList is 0x50");
static_assert(offsetof(ProfessionList, arrayData) == 0x20, "+0x20");
static_assert(offsetof(ProfessionList, arrayCount) == 0x28, "+0x28");
static_assert(offsetof(ProfessionList, arrayCapacity) == 0x2C, "+0x2C");
static_assert(offsetof(ProfessionList, field_0x30) == 0x30, "+0x30");
static_assert(offsetof(ProfessionList, field_0x38) == 0x38, "+0x38");
static_assert(offsetof(ProfessionList, childProfession) == 0x40, "+0x40");
static_assert(offsetof(ProfessionList, studentProfession) == 0x48, "+0x48");
