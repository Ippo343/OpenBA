#pragma once
// engine/types/CompleteRoad.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `CompleteRoad` is a real class now (code/Game/Action/CompleteRoad.h). This
// forwarder keeps the old include spelling working until the closing pass.
#include "Action/CompleteRoad.h"

namespace CompleteRoadAddr {
    constexpr uintptr_t Factory = 0x140021ec0;
    constexpr uintptr_t Vtable  = 0x14015fb70;   // retail table (documentation)
}
