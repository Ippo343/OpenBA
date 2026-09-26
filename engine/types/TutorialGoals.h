#pragma once
// engine/types/TutorialGoals.h — ENDGAME B.3.8 block GOAL: the PODs are gone.
// `TutorialGoal` (abstract) and its leaves TutorialGoalButton, TutorialGoalCount
// and TutorialGoalGameSpeed are real classes now (code/Game/Tutorial/, compiler
// vtables). This forwarder keeps the old include spelling working until the
// closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Tutorial/TutorialGoal.h"
#include "Tutorial/TutorialGoalButton.h"
#include "Tutorial/TutorialGoalCount.h"
#include "Tutorial/TutorialGoalGameSpeed.h"

// Retail addresses (documentation; the tables are the compiler's now).
namespace TutorialGoalAddr {
    constexpr uintptr_t Vtable       = 0x1401607a0;
    constexpr uintptr_t RegisterType = 0x14014d790;
    constexpr uintptr_t Dtor         = 0x140027220;
    constexpr uintptr_t Serialize    = 0x1400dec30;
}
namespace TutorialGoalButtonAddr {
    constexpr uintptr_t Factory      = 0x140027290;
    constexpr uintptr_t Vtable       = 0x140160800;
    constexpr uintptr_t RegisterType = 0x14014d7d0;
    constexpr uintptr_t Dtor         = 0x1400273b0;
    constexpr uintptr_t Serialize    = 0x1400deda0;
    constexpr uintptr_t Cleanup      = 0x140027350;
    constexpr uintptr_t Check        = 0x1400dece0;
    constexpr uintptr_t Init         = 0x140027300;
}
namespace TutorialGoalGameSpeedAddr {
    constexpr uintptr_t Factory      = 0x140027630;
    constexpr uintptr_t Vtable       = 0x1401608e0;
    constexpr uintptr_t Dtor         = 0x140027730;
    constexpr uintptr_t Serialize    = 0x1400df170;
    constexpr uintptr_t Cleanup      = 0x1400276e0;
    constexpr uintptr_t Check        = 0x1400df150;
    constexpr uintptr_t Init         = 0x140027690;
}
namespace TutorialGoalCountAddr {
    constexpr uintptr_t Ctor         = 0x1400dfa60;
    constexpr uintptr_t Factory      = 0x1400277b0;
    constexpr uintptr_t Vtable       = 0x140160950;
    constexpr uintptr_t Dtor         = 0x140027820;
    constexpr uintptr_t Serialize    = 0x1400df900;
    constexpr uintptr_t CleanupSlot  = 0x140027800;
    constexpr uintptr_t Check        = 0x1400df290;
    constexpr uintptr_t InitSlot     = 0x1400277f0;
}
