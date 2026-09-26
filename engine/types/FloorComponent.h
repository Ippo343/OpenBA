#pragma once
// FloorComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x30.
//
// Decompiled from: FloorComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140163360
//   +0x28 = 0 (byte), +0x2c = -1 (int32)
//   flags |= 0x43

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace FloorComponentAddr {
    constexpr uintptr_t Vtable = 0x140163360;
}

// ENDGAME B.3.8 VIS: the POD is gone — `FloorComponent` is a real class now
// (code/Game/Component/FloorComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/FloorComponent.h"
