#pragma once
// OrchardComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: OrchardComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x14015a180
//   controllerVtable = 0x14015a130
//   +0x48 = 0 (byte), +0x4c = -1 (int32), +0x50 = 0, +0x58 = 0, +0x60 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>

namespace OrchardComponentAddr {
    constexpr uintptr_t Vtable            = 0x14015a180;
    constexpr uintptr_t ControllerVtable  = 0x14015a130;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `OrchardComponent` is a real class now
// (code/Game/Component/OrchardComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/OrchardComponent.h"
