#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialStep.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5)
//
// One tutorial step: its text, images, setups and goals.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialStep", parent "Object::External", ti 0x1401c67b8
//   create 0x140028c50 (MemAlloc(0x70, 8)), createInPlace 0x140028c90
//   ctor 0x1400e3480; dtor body 0x1400e33f0
//   table 0x140160e90, 9 slots:
//     [0] 0x140028cc0 scalar deleting dtor: the dtor 0x1400e33f0 (stamp, free
//         the three arrays, ~String +0x38, ~String +0x30, ~External), MemFree
//         on &1
//     [1] TypeName  [2] Type  [3] 0x1400e33e0 GetFileVersion `MOV EAX,1`
//     [4] Object::External::Compile (import) — inherited
//     [5] 0x1400e32a0 Serialize
//     [6] 0x140028ca0 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x140028c90 Construct (null test, then the ctor 0x1400e3480)
//   sizeof 0x70.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "object/External.h"
#include "Object/exe_type.h"

class TutorialStep final : public Object::External {
    DECLARE_EXE_TYPE(TutorialStep)

public:
    TutorialStep();                                                // 0x1400e3480

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140028cc0
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400e33e0
    bool              Serialize(void* stream) override;            // [5] 0x1400e32a0
    Object::External* Reconstruct() override;                      // [6] 0x140028ca0
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x140028c90

protected:
    ~TutorialStep();                                               // 0x1400e33f0

public:
    uint8_t  completed;         // +0x20: bool, step completed flag (serialized)
    uint8_t  _pad21[7];         // +0x21: padding
    void*    stringTable;       // +0x28: ExternalPtr<StringTable>
    void*    titleStr;          // +0x30: System::String (title text)
    void*    descStr;           // +0x38: System::String (description text)
    void*    images;            // +0x40: Array<ExternalPtr<TutorialImage>> data
    int32_t  imageCount;        // +0x48: count
    uint8_t  _pad4C[4];         // +0x4C: capacity
    void*    setups;            // +0x50: Array<ExternalPtr<TutorialSetup>> data
    int32_t  setupCount;        // +0x58: count
    uint8_t  _pad5C[4];         // +0x5C: capacity
    void*    goals;             // +0x60: Array<ExternalPtr<TutorialGoal>> data
    int32_t  goalCount;         // +0x68: count
    uint8_t  _pad6C[4];         // +0x6C: capacity
};

static_assert(offsetof(TutorialStep, completed) == 0x20, "+0x20");
static_assert(offsetof(TutorialStep, stringTable) == 0x28, "+0x28");
static_assert(offsetof(TutorialStep, titleStr) == 0x30, "+0x30");
static_assert(offsetof(TutorialStep, descStr) == 0x38, "+0x38");
static_assert(offsetof(TutorialStep, images) == 0x40, "+0x40");
static_assert(offsetof(TutorialStep, imageCount) == 0x48, "+0x48");
static_assert(offsetof(TutorialStep, setups) == 0x50, "+0x50");
static_assert(offsetof(TutorialStep, setupCount) == 0x58, "+0x58");
static_assert(offsetof(TutorialStep, goals) == 0x60, "+0x60");
static_assert(offsetof(TutorialStep, goalCount) == 0x68, "+0x68");
static_assert(sizeof(TutorialStep) == 0x70, "TutorialStep is 0x70 (create 0x140028c50 MemAlloc(0x70))");
static_assert(alignof(TutorialStep) == 8, "qword-aligned");
