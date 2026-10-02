#pragma once
// ============================================================================
// code/Game/Goal/TimeGoal.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// A goal met after N game years (tracked by a TimeGoalInstance).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TimeGoal", parent "Achievement::Goal", ti 0x1401c62e8
//   create 0x1400266f0 (MemAlloc(0x38, 8)), createInPlace 0x140026750
//   ctor 0x1400dabb0, copy ctor 0x1400dabe0
//   table 0x140160540, 11 slots:
//     [0] 0x140026850 scalar deleting dtor: stamp, ~Goal, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400daba0 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400dab20 Serialize
//     [6] 0x1400267a0 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400dab10 CheckGoalState `XOR EAX,EAX` (the instance decides)
//     [9] 0x1400267f0 GetInstanceType — RETSLOT, TimeGoalInstance's type id
//     [10] 0x140026750 Construct
//   The old tree table had a 12th entry (the TimeGoalInstance dtor, i.e. the
//   first slot of the NEXT retail table); it is gone (B38_RULES D3.5).
//   sizeof 0x38.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "achievement/Goal.h"
#include "Object/exe_type.h"

class TimeGoal final : public Achievement::Goal {
    DECLARE_EXE_TYPE(TimeGoal)

public:
    TimeGoal();                                                    // 0x1400dabb0

    ~TimeGoal() override = default;                                // [0] 0x140026850
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400daba0
    bool              Serialize(void* stream) override;            // [5] 0x1400dab20
    Object::External* Reconstruct() override;                      // [6] 0x1400267a0
    void              Destruct() override;                         // [7] 0x140001640
    GoalState         CheckGoalState() const override { return kNotMet; }  // [8] 0x1400dab10
    Object::Type*     GetInstanceType(Object::Type* ret) const override;   // [9] 0x1400267f0
    virtual Object::External* Construct();                         // [10] 0x140026750

public:
    int32_t  requiredYears;        // +0x30: years to survive (default 1; x 3600 seconds)
    bool     requireOtherGoalsMet; // +0x34: (default 0; Serialize(bool&))
    uint8_t  _pad35[3];            // +0x35: padding
};

static_assert(offsetof(TimeGoal, requiredYears) == 0x30, "TimeGoal::requiredYears offset");
static_assert(offsetof(TimeGoal, requireOtherGoalsMet) == 0x34, "TimeGoal::requireOtherGoalsMet offset");
static_assert(sizeof(TimeGoal) == 0x38, "TimeGoal is 0x38 (create 0x1400266f0 MemAlloc(0x38))");
static_assert(alignof(TimeGoal) == 8, "TimeGoal is qword-aligned");
