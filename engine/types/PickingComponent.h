#pragma once
// PickingComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: PickingComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140164040
//   +0x28 = -1 (int32, sentinel/invalid picking ID)
//   +0x30 = 0 (qword)
//   flags |= 0xC3 (Draw | Update | Create | Serialize | bit6)

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace PickingComponentAddr {
    constexpr uintptr_t Vtable = 0x140164040;
}

// ENDGAME B.3.8 VIS: the POD is gone — `PickingComponent` is a real class now
// (code/Game/Component/PickingComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/PickingComponent.h"
