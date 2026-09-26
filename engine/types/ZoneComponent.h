#pragma once
// ZoneComponent — binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x28.
//
// Decompiled from: ZoneComponent_Ctor
// Parent: Component (direct, minimal)
//
// Just sets vtable and flags |= 0x3. No additional fields.

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace ZoneComponentAddr {
    constexpr uintptr_t Vtable = 0x140164e30;
}

// ENDGAME B.3.8 VIS: the POD is gone — `ZoneComponent` is a real class now
// (code/Game/Component/ZoneComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/ZoneComponent.h"
