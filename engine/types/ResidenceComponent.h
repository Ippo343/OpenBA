#pragma once
// ResidenceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: ResidenceComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401642c0
//   controllerVtable = 0x140164270
//   +0x48 = 0, +0x50 = 0, +0x58 = 0
//   flags |= 0x82

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ResidenceComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401642c0;
    constexpr uintptr_t ControllerVtable  = 0x140164270;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `ResidenceComponent` is a real class now
// (code/Game/Component/ResidenceComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ResidenceComponent.h"
