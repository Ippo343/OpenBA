#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialGoalDialog.h — ENDGAME B.3.8 block GOAL
//
// A tutorial goal met when a named dialog is open (optionally: open on a
// given building description). NOT a DialogInstance — an Object::External
// loaded from Tutorial.rsc like every other TutorialGoal.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialGoalDialog", parent "TutorialGoal", ti 0x1401c64a8
//   create 0x140027440 (MemAlloc(0x40, 8)), createInPlace 0x1400274b0
//   table 0x140160870, 10 slots:
//     [0] 0x140027590 scalar deleting dtor: stamp, ~String +0x28, ~String
//         +0x20, stamp TutorialGoal, ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400df080 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400defd0 Serialize
//     [6] 0x140027520 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400deee0 IsComplete  [9] 0x1400274b0 Construct
//   sizeof 0x40.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialGoal.h"
#include "Object/exe_type.h"
#include "Goal/goal_exe_helpers.h"   // NamedExternalRef

namespace System { class String; }

class TutorialGoalDialog final : public TutorialGoal {
    DECLARE_EXE_TYPE(TutorialGoalDialog)

public:
    TutorialGoalDialog();                                          // inlined in 0x140027440

    ~TutorialGoalDialog() override;                                // [0] 0x140027590
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400df080
    bool              Serialize(IO::Stream& stream) override;            // [5] 0x1400defd0
    Object::External* Reconstruct() override;                      // [6] 0x140027520
    void              Destruct() override;                         // [7] 0x140001640
    bool              IsComplete(void* time, void* instance) override;  // [8] 0x1400deee0
    virtual Object::External* Construct();                         // [9] 0x1400274b0

public:
    uint64_t dialogName;         // +0x20: System::String — the dialog resource to wait for
    // +0x28..+0x3f: the building description, by name: String, factory
    //   generation (-1 = unresolved), cached External* (null = any); resolved
    //   by 0x14001b1d0
    NamedExternalRef target;

    // The +0x20 String (H5 raw storage). Defined in tutorialgoaldialog.cpp.
    System::String& DialogName();
};

static_assert(offsetof(TutorialGoalDialog, dialogName) == 0x20, "dialogName offset");
static_assert(offsetof(TutorialGoalDialog, target) == 0x28, "+0x28");
static_assert(offsetof(TutorialGoalDialog, target) + offsetof(NamedExternalRef, generation) == 0x30, "+0x30");
static_assert(offsetof(TutorialGoalDialog, target) + offsetof(NamedExternalRef, resolved) == 0x38, "+0x38");
static_assert(sizeof(TutorialGoalDialog) == 0x40, "TutorialGoalDialog is 0x40 (create 0x140027440)");
static_assert(alignof(TutorialGoalDialog) == 8, "qword-aligned");
