#pragma once
// ============================================================================
// code/Game/Goal/PopulationGoal.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "PopulationGoal", parent "Achievement::Goal", ti 0x1401c6208
//   create 0x140025f00 (MemAlloc(0x38, 8)), createInPlace 0x140025f60
//   ctor 0x1400d78f0, copy ctor 0x1400d7920
//   table 0x140160370, 11 slots:
//     [0] 0x140026000 scalar deleting dtor: stamp, ~Goal, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400d78e0 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400d7870 Serialize
//     [6] 0x140025fb0 Reconstruct (DeletingDtor(0) + ctor; read 2026-09-26)
//     [7] 0x140001640 Destruct  [8] 0x1400d7820 CheckGoalState
//     [9] Goal::GetInstanceType (inherited)  [10] 0x140025f60 Construct
//   sizeof 0x38.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "achievement/Goal.h"
#include "Object/exe_type.h"

class PopulationGoal final : public Achievement::Goal {
    DECLARE_EXE_TYPE(PopulationGoal)

public:
    PopulationGoal();                                              // 0x1400d78f0

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140026000
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400d78e0
    bool              Serialize(void* stream) override;            // [5] 0x1400d7870
    Object::External* Reconstruct() override;                      // [6] 0x140025fb0
    void              Destruct() override;                         // [7] 0x140001640
    GoalState         CheckGoalState() const override;             // [8] 0x1400d7820
    virtual Object::External* Construct();                         // [10] 0x140025f60

protected:
    ~PopulationGoal() = default;

public:
    int32_t  targetPop;          // +0x30: required population (default 100)
    uint8_t  _pad34[4];          // +0x34: padding
};

static_assert(offsetof(PopulationGoal, targetPop) == 0x30, "PopulationGoal +0x30 targetPop");
static_assert(sizeof(PopulationGoal) == 0x38, "PopulationGoal is 0x38 (create 0x140025f00 MemAlloc(0x38))");
static_assert(alignof(PopulationGoal) == 8, "PopulationGoal is qword-aligned");
