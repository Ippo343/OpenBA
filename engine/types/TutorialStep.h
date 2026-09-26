#pragma once
// engine/types/TutorialStep.h — ENDGAME B.3.8 block GOAL: the POD is gone.
// `TutorialStep` is a real class now (code/Game/Tutorial/TutorialStep.h,
// compiler vtable on Object::External). This forwarder keeps the old include
// spelling working until the closing pass deletes it (B6-4.4).
#include <cstdint>
#include "Tutorial/TutorialStep.h"

namespace TutorialStepAddr {
    constexpr uintptr_t Ctor      = 0x1400e3480;
    constexpr uintptr_t Factory   = 0x140028c50;
    constexpr uintptr_t Vtable    = 0x140160e90;   // retail table (documentation)
    constexpr uintptr_t Dtor      = 0x1400e33f0;   // the dtor body (not the scalar dtor)
    constexpr uintptr_t Serialize = 0x1400e32a0;
}
