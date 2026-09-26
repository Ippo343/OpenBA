#pragma once
// ParticleComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x48.
//
// Decompiled from: ParticleComponent_Ctor
// Parent: Component (direct)
//
// Constructor sets:
//   vtable = 0x14015a290
//   +0x28 = 0, +0x30 = 0, +0x38 = 0, +0x40 = 0
//   flags |= 0xC3

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ParticleComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a290;
}

// ENDGAME B.3.8 CIT: the POD is gone — `ParticleComponent` is a real class now
// (code/Game/Component/ParticleComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ParticleComponent.h"

