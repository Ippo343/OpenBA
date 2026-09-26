#pragma once
// SpawnComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: SpawnComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a670
//   +0x28 = 0, +0x30 = 0
//   flags |= 0x88

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace SpawnComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a670;
}

// ENDGAME B.3.8 CIT: the POD is gone — `SpawnComponent` is a real class now
// (code/Game/Component/SpawnComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/SpawnComponent.h"

