#pragma once
// MineComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: MineComponent_Ctor
// Parent: Component (direct, minimal)
//
// Simplest possible component: just sets vtable, no additional fields.
// No flag modification. Size is exactly Component base (0x28).

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace MineComponentAddr {
    constexpr uintptr_t Vtable = 0x140159ed0;
}

// ENDGAME B.3.8 VIS: the POD is gone — `MineComponent` is a real class now
// (code/Game/Component/MineComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/MineComponent.h"
