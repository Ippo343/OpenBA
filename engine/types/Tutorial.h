#pragma once
// Tutorial — Tutorial definition, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x40.
//
// Decompiled from: Tutorial_Factory (0x140026e50, inline ctor)
// Base: Object::External (0x20 bytes)

#include <cstdint>
#include <cstddef>

namespace TutorialAddr {
    constexpr uintptr_t Factory      = 0x140026e50;
    constexpr uintptr_t Vtable       = 0x140160740;
    constexpr uintptr_t RegisterType = 0x14014d750;
    constexpr uintptr_t Cleanup      = 0x140026eb0;
}

struct Tutorial {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160740
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- Tutorial fields ---
    void*    setup;              // +0x20: ExternalPtr to TutorialSetup
    void*    steps;              // +0x28: array of ExternalPtr to TutorialStep
    int64_t  stepCount;          // +0x30: step count (low 32) + capacity (high 32)
    int32_t  currentStep;        // +0x38: current step index (-1 = none)
    uint8_t  _pad3C[4];          // +0x3C: padding
};

static_assert(sizeof(Tutorial) == 0x40, "Tutorial size mismatch");
static_assert(offsetof(Tutorial, vtable) == 0x00, "");
