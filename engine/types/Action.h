#pragma once
// engine/types/Action.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `Action` is a real class now (code/Game/Action/Action.h, compiler vtable,
// retail slot order, +0x10 InternalBasePtr / +0x18 status). This forwarder
// keeps the old include spelling working until the closing pass deletes it.
#include "Action/Action.h"

namespace ActionAddr {
    constexpr uintptr_t Vtable = 0x14015faa0;   // retail table (served by Action::Vtable(), D5)
}
