#pragma once
// DestroyComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: DestroyComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140163120
//   controllerVtable = 0x1401630d0
//   +0x48 = 0, +0x50 = 0 (uint16), +0x54 = 0 (qword), +0x60 = 0 (qword)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace DestroyComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163120;
    constexpr uintptr_t ControllerVtable  = 0x1401630d0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `DestroyComponent` is a real class now
// (code/Game/Component/DestroyComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/DestroyComponent.h"
