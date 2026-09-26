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

class ProfessionList final : public Object::External {
    DECLARE_EXE_TYPE(ProfessionList)

public:
    ProfessionList();                                             // factory 0x14001e5c0 body

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x14001e760
    int32_t GetFileVersion() const override { return 2; }         // [3]
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(void* stream) override;                        // [5] 0x1400a8e30
    Object::External* Reconstruct() override;                     // [6] 0x14001e690
    void Destruct() override;                                     // [7] 0x140001640 DeletingDtor(0)
    virtual Object::External* Construct();                        // [8] 0x14001e630

protected:
    ~ProfessionList();

public:
    void*    arrayData;          // +0x20: Profession* array data (freed in dtor)
    uint64_t arraySize;          // +0x28: count (low) | capacity (high)
    void*    profRef1;           // +0x30: ExternalPtr<Profession> (serialized via FUN_140018020)
    void*    profRef2;           // +0x38
    void*    profRef3;           // +0x40
    void*    profRef4;           // +0x48
};

static_assert(sizeof(ProfessionList) == 0x50, "ProfessionList is 0x50");
static_assert(offsetof(ProfessionList, arrayData) == 0x20, "+0x20");
static_assert(offsetof(ProfessionList, arraySize) == 0x28, "+0x28");
static_assert(offsetof(ProfessionList, profRef1) == 0x30, "+0x30");
static_assert(offsetof(ProfessionList, profRef2) == 0x38, "+0x38");
static_assert(offsetof(ProfessionList, profRef3) == 0x40, "+0x40");
static_assert(offsetof(ProfessionList, profRef4) == 0x48, "+0x48");
