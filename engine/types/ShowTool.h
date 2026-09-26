#pragma once
// engine/types/ShowTool.h — ENDGAME B.3.8 block ACT: the POD is gone.
// `ShowTool` is a real class now (code/Game/Action/ShowTool.h). This forwarder
// keeps the old include spelling working until the closing pass.
#include "Action/ShowTool.h"

namespace ShowToolAddr {
    constexpr uintptr_t Factory      = 0x1400013b0;
    constexpr uintptr_t Vtable       = 0x1401589b0;
    constexpr uintptr_t RegisterType = 0x14014a5c0;
}
