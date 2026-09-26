#pragma once
// engine/types/MoveTo.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `MoveTo` is a real class now (code/Game/Action/MoveTo.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/MoveTo.h"

namespace MoveToAddr {
    constexpr uintptr_t Ctor         = 0x1400c90e0;
    constexpr uintptr_t Factory      = 0x1400228c0;
    constexpr uintptr_t Vtable       = 0x14015fe40;
    constexpr uintptr_t RegisterType = 0x14014d250;
}
