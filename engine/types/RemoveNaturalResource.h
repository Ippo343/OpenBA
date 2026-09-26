#pragma once
// engine/types/RemoveNaturalResource.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `RemoveNaturalResource` is a real class now (code/Game/Action/RemoveNaturalResource.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/RemoveNaturalResource.h"

namespace RemoveNaturalResourceAddr {
    constexpr uintptr_t Factory = 0x140022f70;
    constexpr uintptr_t Vtable  = 0x140160050;
}
