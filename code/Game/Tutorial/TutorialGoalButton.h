#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialGoalButton.h — ENDGAME B.3.8 block GOAL
//
// A tutorial goal met when a UI element (found by path) reaches a flag state.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialGoalButton", parent "TutorialGoal", ti 0x1401c6470
//   create 0x140027290 (MemAlloc(0x30, 8)), createInPlace 0x140027300
//   ctor 0x1400dee90
//   table 0x140160800, 10 slots:
//     [0] 0x1400273b0 scalar deleting dtor: stamp, ~String +0x20, stamp
//         TutorialGoal, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400dee30 GetFileVersion `MOV EAX,2`
//     [4] External::Compile (inherited)  [5] 0x1400deda0 Serialize
//     [6] 0x140027350 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400dece0 IsComplete  [9] 0x140027300 Construct
//   sizeof 0x30.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialGoal.h"
#include "Object/exe_type.h"

namespace System { class String; }

class TutorialGoalButton final : public TutorialGoal {
    DECLARE_EXE_TYPE(TutorialGoalButton)

public:
    TutorialGoalButton();                                          // 0x1400dee90

    ~TutorialGoalButton() override;                                // [0] 0x1400273b0
    int32_t           GetFileVersion() const override { return 2; }// [3] 0x1400dee30
    bool              Serialize(void* stream) override;            // [5] 0x1400deda0
    Object::External* Reconstruct() override;                      // [6] 0x140027350
    void              Destruct() override;                         // [7] 0x140001640
    bool              IsComplete(void* time, void* instance) override;  // [8] 0x1400dece0
    virtual Object::External* Construct();                         // [9] 0x140027300

    // The +0x20 String (H5 raw storage). Defined in tutorialgoalbutton.cpp.
    System::String& ElementPath();

public:
    uint64_t elementPath;        // +0x20: System::String — UI element path to find
    bool     invertCheck;        // +0x28: true: bit 23 clear; false: bit 22 set (Serialize(bool&))
    uint8_t  _pad29[7];          // +0x29: padding
};

static_assert(offsetof(TutorialGoalButton, elementPath) == 0x20, "elementPath offset");
static_assert(offsetof(TutorialGoalButton, invertCheck) == 0x28, "invertCheck offset");
static_assert(sizeof(TutorialGoalButton) == 0x30, "TutorialGoalButton is 0x30 (create 0x140027290)");
static_assert(alignof(TutorialGoalButton) == 8, "qword-aligned");
