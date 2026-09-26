#pragma once
// PastureComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x68.
//
// Decompiled from: PastureComponent_Ctor
// Parent: Component (direct) with SHIFTED controller sub-object at +0x48
//
// UNUSUAL: Has own data at +0x28..+0x44, controller vtable at +0x48, selfPtr at +0x60.
// Constructor sets:
//   vtable = 0x140163f60 (overwrites 0x140162b80)
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x44 = 0 (dword)
//   controllerVtable at +0x48 = 0x140163f10
//   selfPtr at +0x60 = this
//   flags |= 0x8B

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PastureComponentAddr {
    constexpr uintptr_t Vtable            = 0x140163f60;
    constexpr uintptr_t ControllerVtable  = 0x140163f10;
}

// ENDGAME B.3.8 MI-B: the POD is gone — `PastureComponent` is a real class now
// (code/Game/Component/PastureComponent.h, compiler vtable). This forwarder keeps the old
// include spelling working until the closing pass deletes it (B8-4).
#include "Component/PastureComponent.h"
