#pragma once
// CitizenAIComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0xD0.
//
// Decompiled from: CitizenAIComponent_Ctor (0x14010fb70)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Core AI component for citizen decision-making.
// Flags: base | 0x89
//
// Contains 16 Time objects (0x40..0xBF, 8 bytes each = 128 bytes).
// Ctor init pattern:
//   *(u32)(+0x28) = 0x0C (AI state enum/index)
//   *(u32)(+0x2C) = 0xFFFFFFFF (invalid ID sentinel)
//   *(u16)(+0x30) = 0
//   *(u64)(+0x38) = 0
//   Time::Time() x16 at +0x40..+0xBF
//   *(u64)(+0xC0) = 0
//   *(u64)(+0xC8) = 0

#include <cstdint>
#include <cstddef>
#include "common.h"
#include "Component.h"

namespace CitizenAIComponentAddr {
    constexpr uintptr_t Ctor       = 0x14010fb70;
    constexpr uintptr_t Vtable     = 0x140162470;
    constexpr uintptr_t ScalarDtor = 0x14002fcf0; // vtable[0]
    constexpr uintptr_t Startup    = 0x14002fb00; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x14010f960; // vtable[3]
    constexpr uintptr_t Serialize  = 0x14010f920; // vtable[4]
    constexpr uintptr_t Draw       = 0x14010f880; // vtable[6]
    constexpr uintptr_t Update     = 0x14010f3e0; // vtable[9]
}

// ENDGAME B.3.8 CIT: the POD is gone — `CitizenAIComponent` is a real class now
// (code/Game/Component/CitizenAIComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/CitizenAIComponent.h"

