#pragma once
// engine/types/PickupStoredRawMaterial.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `PickupStoredRawMaterial` is a real class now (code/Game/Action/PickupStoredRawMaterial.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/PickupStoredRawMaterial.h"

namespace PickupStoredRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022d60;
    constexpr uintptr_t Vtable  = 0x14015ff80;
}
