#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialInstance.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5)
//
// The live tutorial: which step is running, the step dialog and its controller.
// FinishTutorialInstance::Startup builds one and owns it through +0x20.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialInstance", parent "Object::Internal", ti 0x1401c6668
//   create 0x140028120 (MemAlloc(0x38, 8)), createInPlace = the IMPORTED
//   Internal::CreateInPlaceObject (null; the tree keeps its in-place build,
//   B8-13). The ctor is inlined in create and in FinishTutorialInstance's
//   Startup 0x1400d7a90: Internal(), stamp, +0x10 = -1, InternalBasePtr(+0x18,
//   null), InternalBasePtr(+0x20, null), +0x30 = 0 (+0x28 is not written).
//   table 0x140160bf8, 4 slots:
//     [0] 0x1400281a0 scalar deleting dtor: stamp, ~Internal, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400e1520 Persist
//   sizeof 0x38.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "object/Internal.h"
#include "Object/exe_type.h"

namespace UI { class DialogInstance; }
class TutorialUI;
class Tutorial;

class TutorialInstance final : public Object::Internal {
    DECLARE_EXE_TYPE(TutorialInstance)

public:
    TutorialInstance();                                            // inlined in 0x140028120

    ~TutorialInstance() override = default;                        // [0] 0x1400281a0
    void          Persist(void* stream) override;                  // [3] 0x1400e1520

public:
    int32_t  currentStep;        // +0x10: current step index (-1 = uninitialized)
    uint8_t  _pad14[4];          // +0x14: padding
    UI::DialogInstance* stepDialog; // +0x18: InternalPtr<UI::DialogInstance> — the step dialog
    TutorialUI* stepUI;          // +0x20: InternalPtr<TutorialUI> — its controller
    uint8_t  _pad28[8];          // +0x28: never written by the ctor
    Tutorial* tutorialDef;       // +0x30: ExternalPtr<Tutorial> — the definition

    // ---- non-virtual members (tutorial_wave.cpp / tutorial_next_scenario.cpp) ----
    // 0x1400e1db0: the per-tick step driver FinishTutorialInstance::Update calls.
    void StepExecute(void* time, void* param3, uint64_t param4);
    void RefreshStepUI();        // 0x1400e1260: push the current step into the TutorialUI panel
    void LoadNextScenario();     // 0x1400e0d70: the tutorial finished, boot the next scenario
};

static_assert(offsetof(TutorialInstance, currentStep) == 0x10, "+0x10");
static_assert(offsetof(TutorialInstance, stepDialog) == 0x18, "+0x18");
static_assert(offsetof(TutorialInstance, stepUI) == 0x20, "+0x20");
static_assert(offsetof(TutorialInstance, tutorialDef) == 0x30, "+0x30");
static_assert(sizeof(TutorialInstance) == 0x38, "TutorialInstance is 0x38 (create 0x140028120 MemAlloc(0x38))");
static_assert(alignof(TutorialInstance) == 8, "qword-aligned");
