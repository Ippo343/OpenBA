#pragma once
// TutorialStep — Tutorial step definition, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x70.
//
// Decompiled from: TutorialStep_Ctor (0x1400e3480), TutorialStep_Factory (0x140028c50),
//                  TutorialStep_Serialize (0x1400e32a0), TutorialStep_Dtor (0x1400e33f0)
// Base: Object::External (0x20 bytes)
// Contains goal references, setup actions, images, and display text.

#include <cstdint>
#include <cstddef>

namespace TutorialStepAddr {
    constexpr uintptr_t Ctor      = 0x1400e3480;
    constexpr uintptr_t Factory   = 0x140028c50;
    constexpr uintptr_t Vtable    = 0x140160e90;
    constexpr uintptr_t Dtor      = 0x1400e33f0;  // actual dtor body (not scalar)
    constexpr uintptr_t Serialize = 0x1400e32a0;
}

struct TutorialStep {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160e90
    uint8_t  _extBase[0x18];    // +0x08: External base fields

    // --- TutorialStep fields ---
    uint8_t  completed;         // +0x20: bool, step completed flag (serialized)
    uint8_t  _pad21[7];         // +0x21: padding
    void*    stringTable;       // +0x28: ExternalPtr<StringTable> (serialized via ExternalPtr_Serialize2)
    void*    titleStr;          // +0x30: System::String (title text, serialized)
    void*    descStr;           // +0x38: System::String (description text, serialized)
    void*    images;            // +0x40: DynArray<ExternalPtr<TutorialImage>> data ptr
    int32_t  imageCount;        // +0x48: DynArray count
    uint8_t  _pad4C[4];        // +0x4C: padding / DynArray capacity
    void*    setups;            // +0x50: DynArray<ExternalPtr<TutorialSetup>> data ptr
    int32_t  setupCount;        // +0x58: DynArray count
    uint8_t  _pad5C[4];        // +0x5C: padding / DynArray capacity
    void*    goals;             // +0x60: DynArray<ExternalPtr<TutorialGoal>> data ptr
    int32_t  goalCount;         // +0x68: DynArray count
    uint8_t  _pad6C[4];        // +0x6C: padding / DynArray capacity
};

static_assert(sizeof(TutorialStep) == 0x70, "TutorialStep size mismatch");
static_assert(offsetof(TutorialStep, vtable) == 0x00, "");
static_assert(offsetof(TutorialStep, completed) == 0x20, "");
static_assert(offsetof(TutorialStep, stringTable) == 0x28, "");
static_assert(offsetof(TutorialStep, titleStr) == 0x30, "");
static_assert(offsetof(TutorialStep, descStr) == 0x38, "");
static_assert(offsetof(TutorialStep, images) == 0x40, "");
static_assert(offsetof(TutorialStep, imageCount) == 0x48, "");
static_assert(offsetof(TutorialStep, setups) == 0x50, "");
static_assert(offsetof(TutorialStep, setupCount) == 0x58, "");
static_assert(offsetof(TutorialStep, goals) == 0x60, "");
static_assert(offsetof(TutorialStep, goalCount) == 0x68, "");
