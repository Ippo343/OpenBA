#pragma once
// NomadComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x78.
//
// Decompiled from: NomadComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x58
//
// UNUSUAL: Has own data fields +0x28..+0x50, then controller vtable at +0x58.
// Similar pattern to TradeComponent (controller sub-object not at +0x28).
//
// Constructor sets:
//   vtable = 0x14015a020 (overwrites 0x140162b80)
//   +0x28..+0x50 = 0 (zeroed, 6 fields)
//   controllerVtable at +0x58 = 0x140159fd0
//   selfPtr at +0x70 = this
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace NomadComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015a020;
    constexpr uintptr_t ControllerVtable  = 0x140159fd0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `NomadComponent` is a real class now
// (code/Game/Component/NomadComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/NomadComponent.h"
