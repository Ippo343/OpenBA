#pragma once
// HappinessComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: HappinessComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x30
//
// UNUSUAL: Has own data byte at +0x28, then controller vtable at +0x30 (not +0x28).
// Constructor sets:
//   vtable = 0x1401595d0 (overwrites ControllerComponent base 0x140162b80)
//   +0x28 = 0 (byte)
//   controllerVtable at +0x30 = 0x140159580
//   selfPtr at +0x48 = this
//   flags |= 0x2

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace HappinessComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401595d0;
    constexpr uintptr_t ControllerVtable  = 0x140159580;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `HappinessComponent` is a real class now
// (code/Game/Component/HappinessComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/HappinessComponent.h"
