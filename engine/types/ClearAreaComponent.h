#pragma once
// ClearAreaComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x50.
//
// Decompiled from: ClearAreaComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x1401626d0
//   controllerVtable = 0x140162680
//   +0x48 = 0 (dword)
//   NO flag modification

#include <cstdint>
#include <cstddef>
#include "ControllerComponent.h"

namespace ClearAreaComponentAddr {
    constexpr uintptr_t Vtable            = 0x1401626d0;
    constexpr uintptr_t ControllerVtable  = 0x140162680;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `ClearAreaComponent` is a real class now
// (code/Game/Component/ClearAreaComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ClearAreaComponent.h"
