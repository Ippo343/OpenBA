#pragma once
// CropFieldComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: CropFieldComponent_Ctor
// Parent: ControllerComponent (pattern — secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140162ec0
//   controllerVtable = 0x140162e70
//   +0x48 = 0x100 (uint16, likely two packed bytes: 0x00, 0x01)
//   +0x50 = 0, +0x58 = 0, +0x60 = 0
//   flags |= 0x80

#include <cstdint>
#include <cstddef>

namespace CropFieldComponentAddr {
    constexpr uintptr_t Vtable            = 0x140162ec0;
    constexpr uintptr_t ControllerVtable  = 0x140162e70;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `CropFieldComponent` is a real class now
// (code/Game/Component/CropFieldComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/CropFieldComponent.h"
