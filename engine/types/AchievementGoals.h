#pragma once
// engine/types/AchievementGoals.h — ENDGAME B.3.8 block GOAL: the PODs are gone.
// The five exe `Achievement::Goal` subclasses and the two
// `Achievement::GoalInstance` subclasses are real classes now, each with a
// compiler vtable, in code/Game/Goal/ and code/Game/Tutorial/. This forwarder
// keeps the old include spelling working until the closing pass deletes it
// (B6-4.4).
#include <cstdint>
#include "Goal/BuildingGoal.h"
#include "Goal/PopulationGoal.h"
#include "Goal/StatGoal.h"
#include "Goal/TimeGoal.h"
#include "Goal/TimeGoalInstance.h"
#include "Tutorial/FinishTutorialGoal.h"
#include "Tutorial/FinishTutorialInstance.h"

// Retail addresses (documentation; the tables are the compiler's now).
namespace BuildingGoalAddr {
    constexpr uintptr_t Factory = 0x140025d80;
    constexpr uintptr_t Vtable  = 0x140160300;
}
namespace PopulationGoalAddr {
    constexpr uintptr_t Factory = 0x140025f00;
    constexpr uintptr_t Vtable  = 0x140160370;
}
namespace StatGoalAddr {
    constexpr uintptr_t Factory = 0x1400264d0;
    constexpr uintptr_t Vtable  = 0x1401604c0;
    constexpr uintptr_t Ctor    = 0x1400daa80;
}
namespace TimeGoalAddr {
    constexpr uintptr_t Factory = 0x1400266f0;
    constexpr uintptr_t Vtable  = 0x140160540;
}
namespace FinishTutorialGoalAddr {
    constexpr uintptr_t Factory      = 0x1400260f0;
    constexpr uintptr_t Vtable       = 0x140160420;
    constexpr uintptr_t RegisterType = 0x14014d550;
    constexpr uintptr_t Cleanup      = 0x140026150;
}
namespace FinishTutorialInstanceAddr {
    constexpr uintptr_t Factory      = 0x140026070;
    constexpr uintptr_t Vtable       = 0x140160478;
    constexpr uintptr_t RegisterType = 0x14014d590;
}
namespace TimeGoalInstanceAddr {
    constexpr uintptr_t Factory = 0x140026670;
    constexpr uintptr_t Vtable  = 0x140160598;
}
