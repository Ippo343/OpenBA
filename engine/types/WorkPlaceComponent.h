#pragma once
// WorkPlaceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: WorkPlaceComponent_Ctor (0x14012f2e0)
// Parent: ControllerComponent (pattern — has secondary vtable at +0x28)
//
// Constructor sets:
//   vtable = 0x140164d30 (overwrites ControllerComponent base 0x140162b80)
//   controllerVtable = 0x140164ce0 (overwrites purecall placeholder)
//   +0x48 = 0 (qword, ptr/data)
//   +0x50 = 1 (byte, flag/bool — likely "is accepting workers")
//   +0x58 = 0 (qword)
//   +0x60 = 0 (qword)
//   flags |= 0x2

#include <cstdint>
#include <cstddef>

namespace WorkPlaceComponentAddr {
    constexpr uintptr_t Ctor              = 0x14012f2e0;
    constexpr uintptr_t Vtable            = 0x140164d30;
    constexpr uintptr_t ControllerVtable  = 0x140164ce0;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `WorkPlaceComponent` is a real class now
// (code/Game/Component/WorkPlaceComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/WorkPlaceComponent.h"
