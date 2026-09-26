#pragma once
// MultiObjectComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x60.
//
// Decompiled from: MultiObjectComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140163cf0
//   controllerVtable = 0x140163ca0
//   +0x48 = 0, +0x50 = 0, +0x58 = 1 (dword)
//   flags |= 0x2

#include <cstdint>
#include <cstddef>

namespace MultiObjectComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163cf0;
    constexpr uintptr_t ControllerVtable  = 0x140163ca0;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `MultiObjectComponent` is a real class now
// (code/Game/Component/MultiObjectComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/MultiObjectComponent.h"
