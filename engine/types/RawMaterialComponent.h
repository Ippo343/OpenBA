#pragma once
// RawMaterialComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: RawMaterialComponent_Ctor (0x140129400)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x140164170
//   +0x30 = 0 (qword)
//   +0x38 = 0 (qword)
//   flags |= 0x89 (Serialize | bit3 | bit7)
//
// Note: +0x28 is NOT explicitly initialized by the ctor (left as whatever
// Component_Ctor set, which is just past the base). This likely gets
// initialized by Startup() or Serialize().

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace RawMaterialComponentAddr {
    constexpr uintptr_t Ctor    = 0x140129400;
    constexpr uintptr_t Vtable  = 0x140164170;
}

// ENDGAME B.3.8 VIS: the POD is gone — `RawMaterialComponent` is a real class now
// (code/Game/Component/RawMaterialComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/RawMaterialComponent.h"
