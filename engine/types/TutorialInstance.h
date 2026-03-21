#pragma once
// TutorialInstance — Tutorial runtime instance, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x38.
//
// Decompiled from: TutorialInstance Dtor (0x1400281a0), Serialize (0x1400e1520),
//                  FinishTutorialInstance::Slot4 (0x1400d7a90) which constructs it.
// Base: Object::Internal (0x10 bytes)
// Tracks current tutorial step index and holds reference to the Tutorial definition.

#include <cstdint>
#include <cstddef>

namespace TutorialInstanceAddr {
    constexpr uintptr_t Dtor      = 0x1400281a0;
    constexpr uintptr_t Serialize = 0x1400e1520;
    constexpr uintptr_t Vtable    = 0x140160bf8;
}

struct TutorialInstance {
    // --- Object::Internal base (0x10 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160bf8
    uint8_t  _intBase[0x08];    // +0x08: Internal base fields

    // --- TutorialInstance fields ---
    int32_t  currentStep;       // +0x10: current tutorial step index (-1 = uninitialized, 0+ = active)
    uint8_t  _pad14[4];         // +0x14: padding
    void*    ibpField18;        // +0x18: InternalBasePtr (purpose TBD)
    void*    ibpField20;        // +0x20: InternalBasePtr (purpose TBD)
    uint8_t  _pad28[8];         // +0x28: unknown
    void*    tutorialDef;       // +0x30: ExternalPtr<Tutorial> - tutorial definition reference
};

static_assert(sizeof(TutorialInstance) == 0x38, "TutorialInstance size mismatch");
static_assert(offsetof(TutorialInstance, vtable) == 0x00, "");
static_assert(offsetof(TutorialInstance, currentStep) == 0x10, "");
static_assert(offsetof(TutorialInstance, tutorialDef) == 0x30, "");
