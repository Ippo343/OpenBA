#pragma once
// ClearResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: ClearResourceComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162840
//   controllerVtable = 0x1401627f0
//   NO flag modification
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ClearResourceComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162840;
    constexpr uintptr_t ControllerVtable  = 0x1401627f0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `ClearResourceComponent` is a real class now
// (code/Game/Component/ClearResourceComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ClearResourceComponent.h"
