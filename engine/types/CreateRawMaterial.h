#pragma once
// engine/types/CreateRawMaterial.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `CreateRawMaterial` is a real class now (code/Game/Action/CreateRawMaterial.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/CreateRawMaterial.h"

namespace CreateRawMaterialAddr {
    constexpr uintptr_t Factory = 0x140022240;
    constexpr uintptr_t Vtable  = 0x14015fd10;
}
