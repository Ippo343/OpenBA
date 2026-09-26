#pragma once
// engine/types/RemoveAnimal.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `RemoveAnimal` is a real class now (code/Game/Action/RemoveAnimal.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/RemoveAnimal.h"

namespace RemoveAnimalAddr {
    constexpr uintptr_t Factory = 0x140022e70;
    constexpr uintptr_t Vtable  = 0x14015ffe0;
}
