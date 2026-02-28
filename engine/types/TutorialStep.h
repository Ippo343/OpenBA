#pragma once
// TutorialStep — Tutorial step definition, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: TutorialStep_Ctor (0x1400e3480), TutorialStep_Factory (0x140028c50)
// Base: Object::External (0x20 bytes)
// Contains goal references, setup actions, images, and display text.

#include <cstdint>
#include <cstddef>

namespace TutorialStepAddr {
    constexpr uintptr_t Ctor    = 0x1400e3480;
    constexpr uintptr_t Factory = 0x140028c50;
    constexpr uintptr_t Vtable  = 0x140160e90;
}

struct TutorialStep {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160e90
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- TutorialStep fields ---
    uint8_t  _field20;           // +0x20: zeroed (bool, completed?)
    uint8_t  _pad21[7];          // +0x21: padding
    void*    _field28;           // +0x28: zeroed (goal list?)
    void*    _field30;           // +0x30: zeroed (setup actions?)
    void*    _field38;           // +0x38: zeroed (images?)
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
    void*    _field68;           // +0x68: zeroed
};

static_assert(sizeof(TutorialStep) == 0x70, "TutorialStep size mismatch");
static_assert(offsetof(TutorialStep, vtable) == 0x00, "");
