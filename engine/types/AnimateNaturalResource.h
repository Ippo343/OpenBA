#pragma once
// engine/types/AnimateNaturalResource.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `AnimateNaturalResource` is a real class now (code/Game/Action/AnimateNaturalResource.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/AnimateNaturalResource.h"

namespace AnimateNaturalResourceAddr {
    constexpr uintptr_t Factory = 0x140021dc0;
    constexpr uintptr_t Vtable  = 0x14015fb10;
}
