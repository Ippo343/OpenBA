#pragma once
// engine/types/SendComponentMessage.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `SendComponentMessage` is a real class now (code/Game/Action/SendComponentMessage.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/SendComponentMessage.h"

namespace SendComponentMessageAddr {
    constexpr uintptr_t Factory = 0x140023070;
    constexpr uintptr_t Vtable  = 0x1401600c0;
}
