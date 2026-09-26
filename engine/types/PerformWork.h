#pragma once
// engine/types/PerformWork.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `PerformWork` is a real class now (code/Game/Action/PerformWork.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/PerformWork.h"

namespace PerformWorkAddr {
    constexpr uintptr_t Ctor         = 0x1400c96b0;
    constexpr uintptr_t Factory      = 0x140022b90;
    constexpr uintptr_t Vtable       = 0x14015fea0;
    constexpr uintptr_t RegisterType = 0x14014d290;
}
