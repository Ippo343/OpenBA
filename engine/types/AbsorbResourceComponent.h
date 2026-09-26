#pragma once
// AbsorbResourceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: AbsorbResourceComponent_Ctor (0x140105cc0)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162000 (overwrites ControllerComponent base 0x140162b80)
//   controllerVtable = 0x140161fb0 (overwrites purecall placeholder)
//   selfPtr at +0x40 = this
//   NO flag modification (no flags |= N)
//
// No additional fields beyond ControllerComponent base (exactly 0x48).

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace AbsorbResourceComponentAddr {
    constexpr uintptr_t Ctor              = 0x140105cc0;
    constexpr uintptr_t Vtable            = 0x140162000;
    constexpr uintptr_t ControllerVtable  = 0x140161fb0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `AbsorbResourceComponent` is a real class now
// (code/Game/Component/AbsorbResourceComponent.h, compiler vtable, controller member).
// This forwarder keeps the old include spelling working until the closing pass
// deletes it (B8-4).
#include "Component/AbsorbResourceComponent.h"
