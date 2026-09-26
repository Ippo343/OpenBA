#pragma once
// engine/types/DropOffRawMaterial.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `DropOffRawMaterial` is a real class now (code/Game/Action/DropOffRawMaterial.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/DropOffRawMaterial.h"

namespace DropOffRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022600;
    constexpr uintptr_t Vtable  = 0x14015fd80;
}
