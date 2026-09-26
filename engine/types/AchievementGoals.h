#pragma once
// Achievement Goal types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// Hierarchy: Achievement::Goal -> [BuildingGoal, PopulationGoal, StatGoal, TimeGoal, FinishTutorialGoal]
// Hierarchy: Achievement::GoalInstance -> [FinishTutorialInstance, TimeGoalInstance]
//
// Goal base has 0x30 bytes (constructed by Achievement::Goal::Goal)
// GoalInstance base has 0x20 bytes (constructed by Achievement::GoalInstance::GoalInstance)

#include <cstdint>
#include <cstddef>

// BuildingGoal — goal requiring building count. Size = 0x48.
// Decompiled from: BuildingGoal_Factory (0x140025d80, inline ctor)
namespace BuildingGoalAddr {
    constexpr uintptr_t Factory = 0x140025d80;
    constexpr uintptr_t Vtable  = 0x140160300;
}

struct BuildingGoal {
    void*    vtable;             // +0x00: -> 0x140160300
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    uint8_t  _field30;           // +0x30: 1 (bool, active?)
    uint8_t  _pad31[7];          // +0x31: padding
    void*    _field38;           // +0x38: zeroed (building type?)
    void*    _field40;           // +0x40: zeroed (target count?)
};
static_assert(sizeof(BuildingGoal) == 0x48, "BuildingGoal size mismatch");

// PopulationGoal — goal requiring population count. Size = 0x38.
// Decompiled from: PopulationGoal_Factory (0x140025f00, inline ctor)
namespace PopulationGoalAddr {
    constexpr uintptr_t Factory = 0x140025f00;
    constexpr uintptr_t Vtable  = 0x140160370;
}

struct PopulationGoal {
    void*    vtable;             // +0x00: -> 0x140160370
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    int32_t  targetPop;          // +0x30: 100 (target population)
    uint8_t  _pad34[4];          // +0x34: padding
};
static_assert(sizeof(PopulationGoal) == 0x38, "PopulationGoal size mismatch");

// StatGoal — goal requiring stat threshold. Size = 0x80.
// Decompiled from: StatGoal_Factory (0x1400264d0) -> StatGoal_Ctor (0x1400daa80)
//
// StatGoal::Check (Slot8, 0x1400da720) evaluates a stat comparison:
//   Reads statA (type at +0x30, resolved via ExternalPtr at +0x48..+0x58)
//   Optionally reads statB if comparisonType is odd (uses +0x34, ExternalPtr at +0x60..+0x70)
//   Otherwise uses fixedValue at +0x3c
//   Switch on comparisonType: 0/1=EQ, 2/3=NE, 4/5=LT, 6/7=LE, 8/9=GT, 10/11=GE
//   If requireOtherGoalsMet (+0x40), adds 1 to result (0->1 not met, 1->2 met)
//
// The sub-function FUN_1400da4e0 is a giant switch on statTypeId that reads
// various game world stats (population, deaths, buildings, etc.)
namespace StatGoalAddr {
    constexpr uintptr_t Factory = 0x1400264d0;
    constexpr uintptr_t Vtable  = 0x1401604c0;
    constexpr uintptr_t Ctor    = 0x1400daa80;
}

struct StatGoal {
    void*    vtable;             // +0x00: -> 0x1401604c0
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields

    // --- Stat-specific fields (0x50 bytes) ---
    int32_t  statTypeA;          // +0x30: stat type ID for first operand (default 5)
    int32_t  statTypeB;          // +0x34: stat type ID for second operand (default 5)
    int32_t  comparisonType;     // +0x38: comparison enum (0-11, see above) (default 10)
    int32_t  fixedValue;         // +0x3c: fixed comparison value when not using statB (default 1)
    uint8_t  requireOtherGoalsMet; // +0x40: if true, adds 1 to result
    uint8_t  _pad41[7];          // +0x41: padding

    // ExternalPtr<StatDescription> for statA (18 bytes: String + uint32 + ptr)
    uint8_t  statAString[8];     // +0x48: System::String (stat resource name)
    int32_t  statAGeneration;    // +0x50: ExternalFactory generation counter (default -1)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    statAExternal;      // +0x58: resolved External* pointer

    // ExternalPtr<StatDescription> for statB (18 bytes: String + uint32 + ptr)
    uint8_t  statBString[8];     // +0x60: System::String (stat resource name)
    int32_t  statBGeneration;    // +0x68: ExternalFactory generation counter (default -1)
    uint8_t  _pad6c[4];          // +0x6c: padding
    void*    statBExternal;      // +0x70: resolved External* pointer

    int32_t  statFlags;          // +0x78: flags/enum for stat category (default 0x20)
    int32_t  statSubType;        // +0x7c: sub-type index (default 1)
};
static_assert(sizeof(StatGoal) == 0x80, "StatGoal size mismatch");
static_assert(offsetof(StatGoal, statTypeA) == 0x30, "StatGoal::statTypeA offset");
static_assert(offsetof(StatGoal, comparisonType) == 0x38, "StatGoal::comparisonType offset");
static_assert(offsetof(StatGoal, fixedValue) == 0x3c, "StatGoal::fixedValue offset");
static_assert(offsetof(StatGoal, requireOtherGoalsMet) == 0x40, "StatGoal::requireOtherGoalsMet offset");
static_assert(offsetof(StatGoal, statAString) == 0x48, "StatGoal::statAString offset");
static_assert(offsetof(StatGoal, statAGeneration) == 0x50, "StatGoal::statAGeneration offset");
static_assert(offsetof(StatGoal, statAExternal) == 0x58, "StatGoal::statAExternal offset");
static_assert(offsetof(StatGoal, statBString) == 0x60, "StatGoal::statBString offset");
static_assert(offsetof(StatGoal, statFlags) == 0x78, "StatGoal::statFlags offset");
static_assert(offsetof(StatGoal, statSubType) == 0x7c, "StatGoal::statSubType offset");

// TimeGoal — goal requiring time elapsed. Size = 0x38.
// Decompiled from: TimeGoal_Factory (0x1400266f0, inline ctor)
namespace TimeGoalAddr {
    constexpr uintptr_t Factory = 0x1400266f0;
    constexpr uintptr_t Vtable  = 0x140160540;
}

struct TimeGoal {
    void*    vtable;             // +0x00: -> 0x140160540
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    int32_t  requiredYears;      // +0x30: years to survive (default 1, multiplied by 3600 for seconds)
    uint8_t  requireOtherGoalsMet; // +0x34: _requireOtherGoalsMet (default 0)
    uint8_t  _pad35[3];          // +0x35: padding
};
static_assert(sizeof(TimeGoal) == 0x38, "TimeGoal size mismatch");
static_assert(offsetof(TimeGoal, requiredYears) == 0x30, "TimeGoal::requiredYears offset");
static_assert(offsetof(TimeGoal, requireOtherGoalsMet) == 0x34, "TimeGoal::requireOtherGoalsMet offset");

// FinishTutorialGoal — goal for completing tutorial. Size = 0x38.
// Decompiled from: FinishTutorialGoal_Factory (0x1400260f0, inline ctor)
namespace FinishTutorialGoalAddr {
    constexpr uintptr_t Factory      = 0x1400260f0;
    constexpr uintptr_t Vtable       = 0x140160420;
    constexpr uintptr_t RegisterType = 0x14014d550;
    constexpr uintptr_t Cleanup      = 0x140026150;
}

struct FinishTutorialGoal {
    void*    vtable;             // +0x00: -> 0x140160420
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    void*    _field30;           // +0x30: zeroed (tutorial ref?)
};
static_assert(sizeof(FinishTutorialGoal) == 0x38, "FinishTutorialGoal size mismatch");

// FinishTutorialInstance — goal instance for tutorial completion. Size = 0x28.
// Decompiled from: FinishTutorialInstance_Factory (0x140026070, inline ctor)
namespace FinishTutorialInstanceAddr {
    constexpr uintptr_t Factory      = 0x140026070;
    constexpr uintptr_t Vtable       = 0x140160478;
    constexpr uintptr_t RegisterType = 0x14014d590;
}

struct FinishTutorialInstance {
    void*    vtable;             // +0x00: -> 0x140160478
    uint8_t  _instBase[0x18];    // +0x08: GoalInstance base fields
    void*    _field20;           // +0x20: zeroed
};
static_assert(sizeof(FinishTutorialInstance) == 0x28, "FinishTutorialInstance size mismatch");

// TimeGoalInstance — goal instance for time goals. Size = 0x28.
// Decompiled from: TimeGoalInstance_Factory (0x140026670, inline ctor)
namespace TimeGoalInstanceAddr {
    constexpr uintptr_t Factory = 0x140026670;
    constexpr uintptr_t Vtable  = 0x140160598;
}

struct TimeGoalInstance {
    void*    vtable;             // +0x00: -> 0x140160598
    uint8_t  _instBase[0x18];    // +0x08: GoalInstance base fields
    int64_t  elapsedTime;        // +0x20: accumulated time in engine ticks (Time value)
};
static_assert(sizeof(TimeGoalInstance) == 0x28, "TimeGoalInstance size mismatch");
static_assert(offsetof(TimeGoalInstance, elapsedTime) == 0x20, "TimeGoalInstance::elapsedTime offset");

// ============================================================================
// ENDGAME B.3.7 — the Runtime bases are real classes now
// (runtime/types/achievement/Goal.h). These seven exe structs STAY (step 8
// converts them into C++ subclasses) and keep their opaque `_goalBase` /
// `_instBase` blobs; what is pinned here is that each blob ends exactly where
// the real base ends, so an exe field can never silently overlap a base field.
// ============================================================================
#include "achievement/Goal.h"

static_assert(sizeof(void*) + sizeof(BuildingGoal::_goalBase) == sizeof(Achievement::Goal),
              "the exe Goal prefix is Achievement::Goal (0x30)");
static_assert(offsetof(BuildingGoal, _field30)          == sizeof(Achievement::Goal), "BuildingGoal extends Goal");
static_assert(offsetof(PopulationGoal, targetPop)       == sizeof(Achievement::Goal), "PopulationGoal extends Goal");
static_assert(offsetof(StatGoal, statTypeA)             == sizeof(Achievement::Goal), "StatGoal extends Goal");
static_assert(offsetof(TimeGoal, requiredYears)         == sizeof(Achievement::Goal), "TimeGoal extends Goal");
static_assert(offsetof(FinishTutorialGoal, _field30)    == sizeof(Achievement::Goal), "FinishTutorialGoal extends Goal");
static_assert(offsetof(FinishTutorialInstance, _field20) == sizeof(Achievement::GoalInstance),
              "FinishTutorialInstance extends GoalInstance (0x20)");
static_assert(offsetof(TimeGoalInstance, elapsedTime)   == sizeof(Achievement::GoalInstance),
              "TimeGoalInstance extends GoalInstance (0x20)");
