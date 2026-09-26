#pragma once
// ImmigrationComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: ImmigrationComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140159b30
//   controllerVtable = 0x140159ae0
//   +0x48 = 0, +0x50 = 0 (dword), +0x58 = 0, +0x60 = 0
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ImmigrationComponentAddr {
    constexpr uintptr_t Vtable            = 0x140159b30;
    constexpr uintptr_t ControllerVtable  = 0x140159ae0;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `ImmigrationComponent` is a real class now
// (code/Game/Component/ImmigrationComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ImmigrationComponent.h"
