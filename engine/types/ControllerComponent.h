#pragma once
// engine/types/ControllerComponent.h — ENDGAME B.3.8 pilot: the POD is gone.
// `ControllerComponent` is a real class now (code/Game/Component/
// ControllerComponent.h). The POD modelled the controller as a fixed +0x28
// sub-object of the base: WRONG — it is a MEMBER of each leaf at a per-class
// offset (B38_SLOTS.md §2, B38_RULES.md D6). Leaf POD headers that still spell
// the controller fields inline keep their own offsets until their block
// converts them. This forwarder keeps the old include spelling working.
#include "Component/ControllerComponent.h"

namespace ControllerComponentAddr {
    constexpr uintptr_t BaseVtable     = 0x140162b80; // the ControllerComponent TABLE
    constexpr uintptr_t PurecallVtable = 0x14015f070; // the 9-slot controller interface table
}
