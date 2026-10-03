#pragma once
// ============================================================================
// code/Game/Goal/TimeGoalInstance.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// The live tracker of a TimeGoal: accumulates game time while the other goals
// hold.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TimeGoalInstance", parent "Achievement::GoalInstance",
//   ti 0x1401c6320, create 0x140026670 (MemAlloc(0x28, 8)), createInPlace =
//   the IMPORTED Internal::CreateInPlaceObject (returns null; the tree keeps
//   its in-place build, B8-13); ctor 0x1400dad90, copy ctor 0x1400dadc0
//   table 0x140160598, 7 slots:
//     [0] 0x140026800 scalar deleting dtor: stamp, ~GoalInstance, MemFree on &1
//     [1] TypeName  [2] Type
//     [3] 0x1400dad30 Persist
//     [4] 0x1400daca0 Startup `RET 0`
//     [5] 0x1400dac30 Update
//     [6] 0x1400dacb0 CheckGoalState
//   sizeof 0x28.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "achievement/Goal.h"
#include "system/Value.h"   // System::Time (one int64)
#include "Object/exe_type.h"

class TimeGoalInstance final : public Achievement::GoalInstance {
    DECLARE_EXE_TYPE(TimeGoalInstance)

public:
    TimeGoalInstance();                                            // 0x1400dad90

    ~TimeGoalInstance() override = default;                        // [0] 0x140026800
    void Persist(IO::Stream& stream) override;                           // [3] 0x1400dad30
    void Startup() override {}                                     // [4] 0x1400daca0 RET
    void Update(const System::Time& time,
                const Achievement::GoalList& list) override;       // [5] 0x1400dac30
    Achievement::Goal::GoalState CheckGoalState() const override;  // [6] 0x1400dacb0

public:
    System::Time elapsedTime;    // +0x20: accumulated time (Time::Serialize, 0x1400dac30 adds the frame's)
};

static_assert(offsetof(TimeGoalInstance, elapsedTime) == 0x20, "TimeGoalInstance::elapsedTime offset");
static_assert(sizeof(TimeGoalInstance) == 0x28, "TimeGoalInstance is 0x28 (create 0x140026670 MemAlloc(0x28))");
static_assert(alignof(TimeGoalInstance) == 8, "TimeGoalInstance is qword-aligned");
