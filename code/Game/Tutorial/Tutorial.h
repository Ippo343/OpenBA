#pragma once
// ============================================================================
// code/Game/Tutorial/Tutorial.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5)
//
// A tutorial definition: its steps, the setup and the start step.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "Tutorial", parent "Object::External", ti 0x1401c6400
//   create 0x140026e50 (MemAlloc(0x40, 8)), createInPlace 0x140026eb0
//   ctor 0x1400deb90, copy ctor 0x1400debe0
//   table 0x140160740, 9 slots:
//     [0] 0x140026f50 scalar deleting dtor: stamp, free the steps array
//         (+0x28), ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400deb80 GetFileVersion `MOV EAX,2`
//     [4] Object::External::Compile (import) — inherited
//     [5] 0x1400dead0 Serialize
//     [6] 0x140026f00 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x140026eb0 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "object/External.h"
#include "Object/exe_type.h"

class Tutorial final : public Object::External {
    DECLARE_EXE_TYPE(Tutorial)

public:
    Tutorial();                                                    // 0x1400deb90

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140026f50
    int32_t           GetFileVersion() const override { return 2; }// [3] 0x1400deb80
    bool              Serialize(void* stream) override;            // [5] 0x1400dead0
    Object::External* Reconstruct() override;                      // [6] 0x140026f00
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x140026eb0

protected:
    ~Tutorial();

public:
    void*    setup;              // +0x20: ExternalPtr (0x140002340 serializes it; StepExecute keys the dialog lookup on it)
    void*    steps;              // +0x28: Array<ExternalPtr<TutorialStep>> data
    int64_t  stepCount;          // +0x30: count (low 32) + capacity (high 32)
    int32_t  currentStep;        // +0x38: -1 by default (tutorial_next_scenario.cpp reads it as the next scenario index)
    uint8_t  _pad3C[4];          // +0x3C: padding
};

static_assert(offsetof(Tutorial, setup) == 0x20, "+0x20");
static_assert(offsetof(Tutorial, steps) == 0x28, "+0x28");
static_assert(offsetof(Tutorial, stepCount) == 0x30, "+0x30");
static_assert(offsetof(Tutorial, currentStep) == 0x38, "+0x38");
static_assert(sizeof(Tutorial) == 0x40, "Tutorial is 0x40 (create 0x140026e50 MemAlloc(0x40))");
static_assert(alignof(Tutorial) == 8, "qword-aligned");
