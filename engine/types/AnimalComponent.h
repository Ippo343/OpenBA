#pragma once
// AnimalComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: AnimalComponent_Ctor (0x140106fc0)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Flags: base | 0xC9
//
// Ctor init pattern:
//   *(u32)(+0x28)=0, *(u64)(+0x30)=0, *(u64)(+0x38)=0,
//   byte(+0x40)=0, *(u64)(+0x44)=0, *(u64)(+0x50)=0

#include <cstdint>
#include <cstddef>
#include "Component.h"

// Vtable slots, read back from .rdata at 0x140162110 (2026-07-28). Slots 4 and 6
// were previously labelled "Serialize" and "Draw"; Ghidra and the decompiled
// bodies say otherwise — 0x140106d50 rolls the death age off the description
// (Startup) and 0x140106c20 is the register-time OnAdded one-shot (the project's
// established +0x30 / index 6 mapping). Every slot not listed is a shared
// Component base stub in 0x140118xxx.
namespace AnimalComponentAddr {
    constexpr uintptr_t Ctor       = 0x140106fc0;
    constexpr uintptr_t Vtable     = 0x140162110;
    constexpr uintptr_t ScalarDtor = 0x14002f170; // vtable[0]
    constexpr uintptr_t BaseInit   = 0x14002f000; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140106e40; // vtable[3]
    constexpr uintptr_t Startup    = 0x140106d50; // vtable[4]  (was "Serialize")
    constexpr uintptr_t OnAdded    = 0x140106c20; // vtable[6]  (was "Draw")
    constexpr uintptr_t Update     = 0x140106420; // vtable[9]
    constexpr uintptr_t OnUpdate2  = 0x140106dd0; // vtable[11] (not ported)

    // Private helpers of Update, all reimplemented in animalcomponent.cpp.
    constexpr uintptr_t SetBounds      = 0x140105e20; // ported as PastureComponent_AnimalSetBounds
    constexpr uintptr_t FindHabitat    = 0x140106020;
    constexpr uintptr_t SetWanderTimer = 0x140106250;
    constexpr uintptr_t UpdateBehavior = 0x140107320;
}

// Field meanings recovered from the Update/Startup/OnAdded bodies. Names are
// deliberately left as dataNN so sibling lanes that already reference them keep
// compiling; animalcomponent.cpp accesses them by byte offset.
// ENDGAME B.3.8 CIT: the POD is gone — `AnimalComponent` is a real class now
// (code/Game/Component/AnimalComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/AnimalComponent.h"

