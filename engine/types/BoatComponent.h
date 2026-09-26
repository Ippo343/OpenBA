#pragma once
// BoatComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x98.
//
// Decompiled from: BoatComponent_Ctor (0x140046800)
//
// Extends Component (0x28). NO real Update (uses base no-op).
// Has a secondary vtable sub-object at +0x78 (ControllerComponent pattern).
// Two vtables are set: first ControllerComponent base (0x140162b80, then 0x140158f40),
// and secondary at +0x78 (purecall -> 0x140158ef0).
// No flags OR -- uses base flags only (0x80).
//
// Ctor init pattern:
//   *(u64)(+0x28..+0x40) = 0 (zeroed)
//   *(u32)(+0x48) = 0
//   *(u64)(+0x50) = 0, *(u32)(+0x58) = 0
//   *(u64)(+0x60) = 0, *(u64)(+0x68) = 0
//   *(u32)(+0x70) = 0xFFFFFFFF
//   secondary vtable at +0x78, zeroed data at +0x80, +0x88
//   back-pointer to self at +0x90

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace BoatComponentAddr {
    constexpr uintptr_t Ctor             = 0x140046800;
    constexpr uintptr_t Vtable           = 0x140158f40;
    constexpr uintptr_t SecondaryVtable  = 0x140158ef0; // controller sub-vtable at +0x78
    constexpr uintptr_t ScalarDtor       = 0x140003970; // vtable[0]
    constexpr uintptr_t Startup          = 0x1400036c0; // vtable[2]
    constexpr uintptr_t Shutdown         = 0x1400465d0; // vtable[3]
    // Update = base no-op (0x140118a80)
    // Serialize = base no-op (0x140118a50)
    // Draw = base no-op (0x140118820)
}

// ENDGAME B.3.8 MI-A: the POD is gone — `BoatComponent` is a real class now
// (code/Game/Component/BoatComponent.h, compiler vtable, controller member). This forwarder
// keeps the old include spelling working until the closing pass deletes it (B8-4).
#include "Component/BoatComponent.h"
