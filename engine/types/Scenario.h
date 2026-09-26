#pragma once
// engine/types/Scenario.h — ENDGAME B.3.8 block GOAL: the POD is gone.
// `Scenario` is a real class now (code/Game/Game/Scenario.h, compiler vtable
// on Achievement::Achievement). This forwarder keeps the old include spelling
// working until the closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Game/Scenario.h"

namespace ScenarioAddr {
    constexpr uintptr_t Factory      = 0x14001e9f0;
    constexpr uintptr_t Vtable       = 0x14015f020;   // retail table (documentation)
    constexpr uintptr_t RegisterType = 0x14014cd10;
    constexpr uintptr_t Cleanup      = 0x14001ea70;  // Construct / createInPlace
    constexpr uintptr_t Dtor         = 0x14001eb40;
    constexpr uintptr_t Slot3        = 0x1400a9460;  // GetFileVersion
    constexpr uintptr_t Slot5        = 0x1400a9370;  // Serialize
    constexpr uintptr_t Slot6        = 0x14001ead0;  // Reconstruct
}
