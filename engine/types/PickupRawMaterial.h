#pragma once
// engine/types/PickupRawMaterial.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `PickupRawMaterial` is a real class now (code/Game/Action/PickupRawMaterial.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/PickupRawMaterial.h"

namespace PickupRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022c60;
    constexpr uintptr_t Vtable  = 0x14015ff10;
}
