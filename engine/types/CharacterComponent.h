#pragma once
// CharacterComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x80.
//
// Decompiled from: CharacterComponent_Ctor (0x140109d20)
//
// Extends Component (0x28). Has a real Update at vtable[9].
// Handles character rendering/animation state (position, mesh, animation).
// Flags: base | 0x8F
//
// Ctor init pattern:
//   byte(+0x28)=0, *(u64)(+0x2C)=0, *(u64)(+0x34)=0,
//   *(u32)(+0x40)=0xFFFFFFFF, *(u64)(+0x48)=0, *(u64)(+0x50)=0,
//   *(u64)(+0x58)=0, *(u32)(+0x60)=0xFFFFFFFF, *(u64)(+0x68)=0,
//   *(u64)(+0x70)=0, *(u64)(+0x78)=0

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace CharacterComponentAddr {
    constexpr uintptr_t Ctor       = 0x140109d20;
    constexpr uintptr_t Vtable     = 0x140162370;
    constexpr uintptr_t ScalarDtor = 0x14002faa0; // vtable[0]
    constexpr uintptr_t Startup    = 0x14002f940; // vtable[2]
    constexpr uintptr_t Shutdown   = 0x140109ab0; // vtable[3]
    constexpr uintptr_t Serialize  = 0x140109a30; // vtable[4]
    constexpr uintptr_t Draw       = 0x1401099c0; // vtable[6]
    constexpr uintptr_t OnCreate   = 0x140109980; // vtable[7]
    constexpr uintptr_t OnRemove   = 0x1401094c0; // vtable[8]
    constexpr uintptr_t Update     = 0x140109480; // vtable[9]
}

// ENDGAME B.3.8 CIT: the POD is gone — `CharacterComponent` is a real class now
// (code/Game/Component/CharacterComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/CharacterComponent.h"

