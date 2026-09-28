#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialGoalCount.h — ENDGAME B.3.8 block GOAL
//
// A tutorial goal met when a counted game metric reaches a target.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialGoalCount", parent "TutorialGoal", ti 0x1401c6518
//   create 0x1400277b0 (MemAlloc(0x50, 8)), createInPlace 0x1400277f0
//   ctor 0x1400dfa60
//   table 0x140160950, 10 slots:
//     [0] 0x140027820 scalar deleting dtor: stamp, ~String +0x48, ~String
//         +0x30, stamp TutorialGoal, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400df9f0 GetFileVersion `MOV EAX,5`
//     [4] External::Compile (inherited)  [5] 0x1400df900 Serialize
//     [6] 0x140027800 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400df290 IsComplete (the 11-mode count switch)
//     [9] 0x1400277f0 Construct
//   sizeof 0x50.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialGoal.h"
#include "Object/exe_type.h"

class TutorialGoalCount final : public TutorialGoal {
    DECLARE_EXE_TYPE(TutorialGoalCount)

public:
    TutorialGoalCount();                                           // 0x1400dfa60

    ~TutorialGoalCount() override;                                 // [0] 0x140027820
    int32_t           GetFileVersion() const override { return 5; }// [3] 0x1400df9f0
    bool              Serialize(void* stream) override;            // [5] 0x1400df900
    Object::External* Reconstruct() override;                      // [6] 0x140027800
    void              Destruct() override;                         // [7] 0x140001640
    bool              IsComplete(void* time, void* instance) override;  // [8] 0x1400df290
    virtual Object::External* Construct();                         // [9] 0x1400277f0

public:
    uint32_t countMode;          // +0x20: count mode (0-10, selects what to count)
    int32_t  targetCount;        // +0x24: required count to complete goal
    uint8_t  greaterOrEqual;     // +0x28: bool — count >= target, else count == target
    uint8_t  _pad29[7];          // +0x29: padding
    uint64_t typeName;           // +0x30: ExternalPtr triple: System::String name
    int32_t  _field38;           // +0x38: ExternalFactory generation (-1)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _field40;           // +0x40: resolved External* (building/profession)
    uint64_t elementName;        // +0x48: System::String — UI element path (mode 9)
};

static_assert(offsetof(TutorialGoalCount, countMode) == 0x20, "countMode offset");
static_assert(offsetof(TutorialGoalCount, targetCount) == 0x24, "targetCount offset");
static_assert(offsetof(TutorialGoalCount, greaterOrEqual) == 0x28, "greaterOrEqual offset");
static_assert(offsetof(TutorialGoalCount, typeName) == 0x30, "typeName offset");
static_assert(offsetof(TutorialGoalCount, _field38) == 0x38, "_field38 offset");
static_assert(offsetof(TutorialGoalCount, _field40) == 0x40, "_field40 offset");
static_assert(offsetof(TutorialGoalCount, elementName) == 0x48, "elementName offset");
static_assert(sizeof(TutorialGoalCount) == 0x50, "TutorialGoalCount is 0x50 (create 0x1400277b0)");
static_assert(alignof(TutorialGoalCount) == 8, "qword-aligned");
