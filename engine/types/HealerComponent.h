#pragma once
// HealerComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: HealerComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159730
//   controllerVtable = 0x1401596e0
//   flags |= 0x2
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace HealerComponentAddr {
    constexpr uintptr_t Vtable            = 0x140159730;
    constexpr uintptr_t ControllerVtable  = 0x1401596e0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `HealerComponent` is a real class now
// (code/Game/Component/HealerComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/HealerComponent.h"
