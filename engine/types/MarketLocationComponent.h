#pragma once
// MarketLocationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: MarketLocationComponent_Ctor (0x14004f060)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159dd0 (overwrites ControllerComponent base)
//   controllerVtable = 0x140159d80
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>

namespace MarketLocationComponentAddr {
    constexpr uintptr_t Ctor              = 0x14004f060;
    constexpr uintptr_t Vtable            = 0x140159dd0;
    constexpr uintptr_t ControllerVtable  = 0x140159d80;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `MarketLocationComponent` is a real class now
// (code/Game/Component/MarketLocationComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/MarketLocationComponent.h"
