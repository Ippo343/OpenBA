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
// Decompiled from: StatGoal_Factory (0x1400264d0) -> StatGoal_Ctor
namespace StatGoalAddr {
    constexpr uintptr_t Factory = 0x1400264d0;
}

struct StatGoal {
    void*    vtable;             // +0x00
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    uint8_t  _fields[0x50];      // +0x30: stat-specific fields (0x50 bytes)
};
static_assert(sizeof(StatGoal) == 0x80, "StatGoal size mismatch");

// TimeGoal — goal requiring time elapsed. Size = 0x38.
// Decompiled from: TimeGoal_Factory (0x1400266f0, inline ctor)
namespace TimeGoalAddr {
    constexpr uintptr_t Factory = 0x1400266f0;
    constexpr uintptr_t Vtable  = 0x140160540;
}

struct TimeGoal {
    void*    vtable;             // +0x00: -> 0x140160540
    uint8_t  _goalBase[0x28];    // +0x08: Goal base fields
    int32_t  _field30;           // +0x30: 1 (years?)
    uint8_t  _field34;           // +0x34: zeroed (bool?)
    uint8_t  _pad35[3];          // +0x35: padding
};
static_assert(sizeof(TimeGoal) == 0x38, "TimeGoal size mismatch");

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
    void*    _field20;           // +0x20: zeroed
};
static_assert(sizeof(TimeGoalInstance) == 0x28, "TimeGoalInstance size mismatch");
