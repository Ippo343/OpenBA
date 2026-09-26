#pragma once
// CemeteryComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: CemeteryComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401590a0
//   controllerVtable = 0x140159050
//   +0x48 = 0 (plot array), +0x50 = 0 (count), +0x54 = 0 (capacity)
//   flags |= 0x83

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace CemeteryComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401590a0;
    constexpr uintptr_t ControllerVtable  = 0x140159050;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `CemeteryComponent` is a real class now
// (code/Game/Component/CemeteryComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/CemeteryComponent.h"
