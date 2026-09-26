#pragma once
// AttendanceComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: AttendanceComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x38
//
// UNUSUAL: Has own data at +0x28/+0x30, then controller vtable at +0x38.
// Constructor sets:
//   vtable = 0x140158ce0 (overwrites 0x140162b80)
//   +0x28 = 0, +0x30 = 0
//   controllerVtable at +0x38 = 0x140158c90
//   selfPtr at +0x50 = this
//   flags |= 0xA (10 decimal)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace AttendanceComponentAddr {
    constexpr uintptr_t Vtable            = 0x140158ce0;
    constexpr uintptr_t ControllerVtable  = 0x140158c90;
}

// ENDGAME B.3.8 MI-A: the POD is gone — `AttendanceComponent` is a real class now
// (code/Game/Component/AttendanceComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/AttendanceComponent.h"
