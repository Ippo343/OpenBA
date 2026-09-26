#pragma once
// DecalComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: DecalComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x140162fd0
//   +0x28 = 0 (dword), +0x2C = -1 (int32)
//   +0x30 = 0x3f800000 (float 1.0)
//   flags |= 0xC3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace DecalComponentAddr {
    constexpr uintptr_t Vtable = 0x140162fd0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `DecalComponent` is a real class now
// (code/Game/Component/DecalComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/DecalComponent.h"
