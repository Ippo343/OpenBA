#pragma once
// engine/types/TutorialInstance.h — ENDGAME B.3.8 block GOAL: the POD is gone.
// `TutorialInstance` is a real class now (code/Game/Tutorial/TutorialInstance.h,
// compiler vtable on Object::Internal). This forwarder keeps the old include
// spelling working until the closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Tutorial/TutorialInstance.h"

namespace TutorialInstanceAddr {
    constexpr uintptr_t Dtor      = 0x1400281a0;
    constexpr uintptr_t Serialize = 0x1400e1520;
    constexpr uintptr_t Vtable    = 0x140160bf8;   // retail table (documentation)
}
