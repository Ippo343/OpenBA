#pragma once
// ============================================================================
// code/Game/Goal/StatGoal.h — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8)
//
// An achievement goal that compares a game statistic against a constant or a
// second statistic.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md section 6):
//   reflected "StatGoal", parent "Achievement::Goal", ti 0x1401c62b0
//   create 0x1400264d0 (MemAlloc(0x80, 8)), createInPlace 0x140026510
//   ctor 0x1400daa80
//   table 0x1401604c0, 11 slots (Ghidra's labels on it are wrong):
//     [0] 0x140026540 scalar deleting dtor: stamp, ~String +0x60, ~String +0x48,
//         ~Goal, MemFree on &1
//     [1] TypeName  [2] Type  [3] 0x1400daa10 GetFileVersion `MOV EAX,1`
//     [4] External::Compile (inherited)  [5] 0x1400da8b0 Serialize
//     [6] 0x140026520 Reconstruct  [7] 0x140001640 Destruct
//     [8] 0x1400da720 CheckGoalState
//     [9] Goal::GetInstanceType (inherited)  [10] 0x140026510 Construct
//   sizeof 0x80.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include "achievement/Goal.h"
#include "Object/exe_type.h"

class StatGoal final : public Achievement::Goal {
    DECLARE_EXE_TYPE(StatGoal)

public:
    StatGoal();                                                    // 0x1400daa80

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140026540
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x1400daa10
    bool              Serialize(void* stream) override;            // [5] 0x1400da8b0
    Object::External* Reconstruct() override;                      // [6] 0x140026520
    void              Destruct() override;                         // [7] 0x140001640
    GoalState         CheckGoalState() const override;             // [8] 0x1400da720
    virtual Object::External* Construct();                         // [10] 0x140026510

    // Not retail: the two operands CheckGoalState compares (diagnostics).
    void ComputeOperands(int& statValueA, int& comparisonValue) const;

protected:
    ~StatGoal();

public:
    int32_t  statTypeA;          // +0x30: stat type for the first operand (default 5)
    int32_t  statTypeB;          // +0x34: stat type for the second operand (default 5)
    int32_t  comparisonType;     // +0x38: 0-11: EQ/NE/LT/LE/GT/GE, odd = against statB (default 10)
    int32_t  fixedValue;         // +0x3c: constant operand when comparisonType is even (default 1)
    uint8_t  requireOtherGoalsMet; // +0x40: result + 1 when set
    uint8_t  _pad41[7];          // +0x41: padding
    uint8_t  statAString[8];     // +0x48: ExternalPtr triple A: System::String name
    int32_t  statAGeneration;    // +0x50: ExternalFactory generation (default -1)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    statAExternal;      // +0x58: resolved External*
    uint8_t  statBString[8];     // +0x60: ExternalPtr triple B: System::String name
    int32_t  statBGeneration;    // +0x68: ExternalFactory generation (default -1)
    uint8_t  _pad6c[4];          // +0x6c: padding
    void*    statBExternal;      // +0x70: resolved External*
    int32_t  statFlags;          // +0x78: (default 0x20)
    int32_t  statSubType;        // +0x7c: (default 1)
};

static_assert(offsetof(StatGoal, statTypeA) == 0x30, "StatGoal::statTypeA offset");
static_assert(offsetof(StatGoal, statTypeB) == 0x34, "StatGoal::statTypeB offset");
static_assert(offsetof(StatGoal, comparisonType) == 0x38, "StatGoal::comparisonType offset");
static_assert(offsetof(StatGoal, fixedValue) == 0x3c, "StatGoal::fixedValue offset");
static_assert(offsetof(StatGoal, requireOtherGoalsMet) == 0x40, "StatGoal::requireOtherGoalsMet offset");
static_assert(offsetof(StatGoal, statAString) == 0x48, "StatGoal::statAString offset");
static_assert(offsetof(StatGoal, statAGeneration) == 0x50, "StatGoal::statAGeneration offset");
static_assert(offsetof(StatGoal, statAExternal) == 0x58, "StatGoal::statAExternal offset");
static_assert(offsetof(StatGoal, statBString) == 0x60, "StatGoal::statBString offset");
static_assert(offsetof(StatGoal, statBGeneration) == 0x68, "StatGoal::statBGeneration offset");
static_assert(offsetof(StatGoal, statBExternal) == 0x70, "StatGoal::statBExternal offset");
static_assert(offsetof(StatGoal, statFlags) == 0x78, "StatGoal::statFlags offset");
static_assert(offsetof(StatGoal, statSubType) == 0x7c, "StatGoal::statSubType offset");
static_assert(sizeof(StatGoal) == 0x80, "StatGoal is 0x80 (create 0x1400264d0 MemAlloc(0x80))");
static_assert(alignof(StatGoal) == 8, "StatGoal is qword-aligned");
