#pragma once
// engine/types/PlayAnimation.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `PlayAnimation` is a real class now (code/Game/Action/PlayAnimation.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/PlayAnimation.h"

namespace PlayAnimationAddr {
    constexpr uintptr_t Factory      = 0x1400011f0;
    constexpr uintptr_t Vtable       = 0x140158950;
    constexpr uintptr_t RegisterType = 0x14014a5c0; // shares with ShowTool
}
