#pragma once
// ============================================================================
// code/Game/Goal/BuildingGoal.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// An achievement / scenario goal met when the listed building types exist.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "BuildingGoal", parent "Achievement::Goal", ti 0x1401c61d0
//   create 0x140025d80 (MemAlloc(0x48, 8)), createInPlace 0x140025de0
//   ctor 0x1400d77a0, copy ctor 0x1400d77e0
//   table 0x140160300, 11 slots:
//     [0] 0x140025e80 scalar deleting dtor: stamp, destroy the entry array at
//         +0x38 (0x14001b030), ~Goal, MemFree on &1
//     [1] TypeName  [2] Type
//     [3] 0x1400d7790 GetFileVersion `MOV EAX,2`
//     [4] Object::External::Compile (import) — inherited
//     [5] 0x1400d76a0 Serialize
//     [6] 0x140025e30 Reconstruct: this->~T(), then the ctor on `this`
//     [7] 0x140001640 Destruct: this->~T()
//     [8] 0x1400d7580 CheckGoalState
//     [9] Achievement::Goal::GetInstanceType (import) — inherited
//     [10] 0x140025de0 Construct (folded with createInPlace)
//   sizeof 0x48.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "achievement/Goal.h"
#include "Object/exe_type.h"
#include "Goal/goal_exe_helpers.h"   // NamedExternalRefArray

class BuildingGoal final : public Achievement::Goal {
    DECLARE_EXE_TYPE(BuildingGoal)

public:
    BuildingGoal();                                                // 0x1400d77a0

    ~BuildingGoal() override;                                      // [0] 0x140025e80
    int32_t           GetFileVersion() const override { return 2; }// [3] 0x1400d7790
    bool              Serialize(IO::Stream& stream) override;            // [5] 0x1400d76a0
    Object::External* Reconstruct() override;                      // [6] 0x140025e30
    void              Destruct() override;                         // [7] 0x140001640
    GoalState         CheckGoalState() const override;             // [8] 0x1400d7580
    virtual Object::External* Construct();                         // [10] 0x140025de0

public:
    bool     _field30;           // +0x30: requireAll (1 = every listed type, 0 = any); Serialize(bool&)
    uint8_t  _pad31[7];          // +0x31: padding
    // +0x38: Array<by-name ComponentDescription reference> (stride 0x18;
    //   resolved by 0x14001b1d0; H5 raw triple, freed by 0x14001b030)
    NamedExternalRefArray _field38;
};

static_assert(offsetof(BuildingGoal, _field30) == 0x30, "BuildingGoal +0x30 requireAll");
static_assert(offsetof(BuildingGoal, _field38) == 0x38, "BuildingGoal +0x38 entry array");
static_assert(offsetof(BuildingGoal, _field38) + offsetof(NamedExternalRefArray, count) == 0x40, "BuildingGoal +0x40 entry count");
static_assert(sizeof(BuildingGoal) == 0x48, "BuildingGoal is 0x48 (create 0x140025d80 MemAlloc(0x48))");
static_assert(alignof(BuildingGoal) == 8, "BuildingGoal is qword-aligned");
