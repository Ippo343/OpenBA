#pragma once
// BuildComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x78.
//
// Decompiled from: BuildComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162260
//   controllerVtable = 0x140162210
//   +0x48 = 0 (dword), +0x4c = 1 (dword), +0x50 = 0 (byte)
//   +0x58 = 0, +0x60 = 0, +0x68 = 0, +0x70 = 0
//   flags |= 0x83

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace BuildComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162260;
    constexpr uintptr_t ControllerVtable  = 0x140162210;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `BuildComponent` is a real class now
// (code/Game/Component/BuildComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/BuildComponent.h"
