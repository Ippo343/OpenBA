#pragma once
// engine/types/ConsumeRawMaterial.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `ConsumeRawMaterial` is a real class now (code/Game/Action/ConsumeRawMaterial.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/ConsumeRawMaterial.h"

namespace ConsumeRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022090;
    constexpr uintptr_t Vtable  = 0x14015fc20;
}
