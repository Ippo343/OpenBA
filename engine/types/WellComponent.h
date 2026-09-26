#pragma once
// engine/types/WellComponent.h — ENDGAME B.3.8 pilot: the POD is gone.
// `WellComponent` is a real class now (code/Game/Component/WellComponent.h,
// compiler vtable, embedded controller member at +0x28). This forwarder keeps
// the old include spelling working until the closing pass deletes it (B6-4.4).
#include "Component/WellComponent.h"

namespace WellComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015ae90;   // retail table (documentation)
    constexpr uintptr_t ControllerVtable  = 0x14015ae40;   // retail member table (documentation)
}
