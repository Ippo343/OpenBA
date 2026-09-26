#pragma once
// WorkComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: WorkComponent_Ctor (0x14012e820)
// Parent: Component (direct, NOT ControllerComponent)
//
// Constructor sets:
//   vtable = 0x140164be0
//   +0x28 = 0xFFFFFFFF (int32, invalid ID / sentinel)
//   +0x2c = 0x80000000 (uint32, flag or sentinel)
//   +0x30 = 0 (int32)
//   flags |= 0x3 (Serialize | Draw?)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace WorkComponentAddr {
    constexpr uintptr_t Ctor    = 0x14012e820;
    constexpr uintptr_t Vtable  = 0x140164be0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `WorkComponent` is a real class now
// (code/Game/Component/WorkComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/WorkComponent.h"
