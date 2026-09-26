#pragma once
// engine/types/CompleteTask.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `CompleteTask` is a real class now (code/Game/Action/CompleteTask.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/CompleteTask.h"

namespace CompleteTaskAddr {
    constexpr uintptr_t Factory = 0x140021fc0;
    constexpr uintptr_t Vtable  = 0x14015fbd0;
}
