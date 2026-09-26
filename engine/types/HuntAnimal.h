#pragma once
// engine/types/HuntAnimal.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `HuntAnimal` is a real class now (code/Game/Action/HuntAnimal.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/HuntAnimal.h"

namespace HuntAnimalAddr {
    constexpr uintptr_t Factory = 0x140022700;
    constexpr uintptr_t Vtable  = 0x14015fde0;
}
