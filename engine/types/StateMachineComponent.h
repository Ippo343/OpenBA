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

struct StateMachineComponent {
    // Component base (0x28 bytes)
    void*    vtable;              // +0x00
    void*    next;                // +0x08
    void*    owner;               // +0x10
    void*    _unk18;              // +0x18
    uint32_t flags;               // +0x20
    uint8_t  _pad24[4];           // +0x24

    // Controller sub-object at +0x28
    void*    controllerVtable;    // +0x28: secondary vtable (purecall -> 0x1401644d0)
    uint64_t controllerData0;     // +0x30: zeroed
    uint64_t controllerData1;     // +0x38: zeroed

    // StateMachineComponent fields
    void*    ownerComponent;      // +0x40: back-pointer to self (set to param_1)
    void*    controllerSelf;      // +0x48: set to param_1 + 0x28
    uint32_t stateFlags;          // +0x50: initialized to 0xFF00FFFF (state bitmask)
    uint8_t  _pad54[4];           // +0x54: padding
};

static_assert(sizeof(StateMachineComponent) == 0x58, "StateMachineComponent size mismatch");
static_assert(offsetof(StateMachineComponent, vtable)            == 0x00, "");
static_assert(offsetof(StateMachineComponent, next)              == 0x08, "");
static_assert(offsetof(StateMachineComponent, owner)             == 0x10, "");
static_assert(offsetof(StateMachineComponent, flags)             == 0x20, "");
static_assert(offsetof(StateMachineComponent, controllerVtable)  == 0x28, "");
static_assert(offsetof(StateMachineComponent, controllerData0)   == 0x30, "");
static_assert(offsetof(StateMachineComponent, controllerData1)   == 0x38, "");
static_assert(offsetof(StateMachineComponent, ownerComponent)    == 0x40, "");
static_assert(offsetof(StateMachineComponent, controllerSelf)    == 0x48, "");
static_assert(offsetof(StateMachineComponent, stateFlags)        == 0x50, "");
