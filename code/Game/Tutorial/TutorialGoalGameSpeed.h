#pragma once
// ============================================================================
// code/Game/Tutorial/TutorialGoalGameSpeed.h — ENDGAME B.3.8 block GOAL
//
// A tutorial goal met when the game speed equals a target.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "TutorialGoalGameSpeed", parent "TutorialGoal", ti 0x1401c64e0
//   create 0x140027630 (MemAlloc(0x28, 8)), createInPlace 0x140027690
//   ctor 0x1400df240
//   table 0x1401608e0, 10 slots:
//     [0] 0x140027730 scalar deleting dtor: stamp, stamp TutorialGoal,
//         ~External, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400df1f0 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400df170 Serialize
//     [6] 0x1400276e0 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400df150 IsComplete  [9] 0x140027690 Construct
//   sizeof 0x28.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "Tutorial/TutorialGoal.h"
#include "Object/exe_type.h"

class TutorialGoalGameSpeed final : public TutorialGoal {
    DECLARE_EXE_TYPE(TutorialGoalGameSpeed)

public:
    TutorialGoalGameSpeed();                                       // 0x1400df240

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140027730
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400df1f0
    bool              Serialize(void* stream) override;            // [5] 0x1400df170
    Object::External* Reconstruct() override;                      // [6] 0x1400276e0
    void              Destruct() override;                         // [7] 0x140001640
    bool              IsComplete(void* time, void* instance) override;  // [8] 0x1400df150
    virtual Object::External* Construct();                         // [9] 0x140027690

protected:
    ~TutorialGoalGameSpeed() = default;

public:
    int32_t  targetSpeed;        // +0x20: target game speed (default 1)
    uint8_t  _pad24[4];          // +0x24: padding
};

static_assert(offsetof(TutorialGoalGameSpeed, targetSpeed) == 0x20, "targetSpeed offset");
static_assert(sizeof(TutorialGoalGameSpeed) == 0x28, "TutorialGoalGameSpeed is 0x28 (create 0x140027630)");
static_assert(alignof(TutorialGoalGameSpeed) == 8, "qword-aligned");
