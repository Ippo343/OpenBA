#pragma once
// QuarryComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: QuarryComponent_Ctor
// Parent: Component (direct, minimal)
//
// Just sets vtable and flags |= 0x3. No additional fields.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace QuarryComponentAddr {
    constexpr uintptr_t Vtable = 0x14015a490;
}

// ENDGAME B.3.8 VIS: the POD is gone — `QuarryComponent` is a real class now
// (code/Game/Component/QuarryComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/QuarryComponent.h"
