#pragma once
// StateMachineComponent -- binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x58.
//
// Decompiled from: StateMachineComponent_Ctor (0x14012ba30)
//
// Extends Component (0x28) with a secondary vtable sub-object at +0x28
// (same ControllerComponent pattern: secondary vtable + back-pointer).
// Also has a state word at +0x50 initialized to 0xFF00FFFF.
// Has a real Update at vtable[9].
// Flags: base | 0xCD

#include <cstdint>
#include <cstddef>
#include "Component.h"

namespace StateMachineComponentAddr {
    constexpr uintptr_t Ctor             = 0x14012ba30;
    constexpr uintptr_t Vtable           = 0x140164520;
    constexpr uintptr_t SecondaryVtable  = 0x1401644d0; // controller sub-vtable at +0x28
    constexpr uintptr_t ScalarDtor       = 0x1400384a0; // vtable[0]
    constexpr uintptr_t Startup          = 0x1400382b0; // vtable[2]
    constexpr uintptr_t Shutdown         = 0x14012b940; // vtable[3]
    constexpr uintptr_t Serialize        = 0x14012b6a0; // vtable[4]
    constexpr uintptr_t Draw             = 0x14012b660; // vtable[6]
    constexpr uintptr_t OnRemove         = 0x14012b640; // vtable[8]
    constexpr uintptr_t Update           = 0x14012b620; // vtable[9]
    constexpr uintptr_t OnUpdate2        = 0x14012b7d0; // vtable[11]
}

// ENDGAME B.3.8 VIS: the POD is gone — `StateMachineComponent` is a real class now
// (code/Game/Component/StateMachineComponent.h, compiler vtable). This forwarder keeps the
// old include spelling working until the closing pass deletes it (B8-4).
#include "Component/StateMachineComponent.h"
