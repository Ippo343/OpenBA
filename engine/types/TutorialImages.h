#pragma once
// Tutorial Image types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// TutorialImage base (vtable 0x140160a40) displays images in tutorial steps.

#include <cstdint>
#include <cstddef>

// TutorialImage — base image display. Size = 0x30.
// Decompiled from: TutorialImage_Factory (0x140027980, inline ctor)
namespace TutorialImageAddr {
    constexpr uintptr_t Factory = 0x140027980;
    constexpr uintptr_t Vtable  = 0x140160a40;
    constexpr uintptr_t Cleanup = 0x1400279e0;
}

struct TutorialImage {
    void*    vtable;             // +0x00: -> 0x140160a40
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed (image resource?)
    void*    _field28;           // +0x28: zeroed
};
static_assert(sizeof(TutorialImage) == 0x30, "TutorialImage size mismatch");

// TutorialImageHotKey — displays hot key image. Size = 0x48.
// Decompiled from: TutorialImageHotKey_Ctor (0x1400e0680)
namespace TutorialImageHotKeyAddr {
    constexpr uintptr_t Ctor    = 0x1400e0680;
    constexpr uintptr_t Factory = 0x140027d00;
    constexpr uintptr_t Vtable  = 0x140160ab0;
}

struct TutorialImageHotKey {
    void*    vtable;             // +0x00: -> 0x140160ab0
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed (image base fields)
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed (hot key ref?)
    int32_t  _field38;           // +0x38: -1 sentinel (key ID?)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _field40;           // +0x40: zeroed
};
static_assert(sizeof(TutorialImageHotKey) == 0x48, "TutorialImageHotKey size mismatch");

// TutorialImageInputKey — displays input key image. Size = 0x38.
// Decompiled from: TutorialImageInputKey_Factory (0x140027ea0, inline ctor)
namespace TutorialImageInputKeyAddr {
    constexpr uintptr_t Factory = 0x140027ea0;
    constexpr uintptr_t Vtable  = 0x140160b20;
    constexpr uintptr_t Cleanup = 0x140027f00;
}

struct TutorialImageInputKey {
    void*    vtable;             // +0x00: -> 0x140160b20
    uint8_t  _extBase[0x18];     // +0x08: External base
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    uint8_t  _pad30[8];          // +0x30: extra fields (extends TutorialImage)
};
static_assert(sizeof(TutorialImageInputKey) == 0x38, "TutorialImageInputKey size mismatch");

// TutorialImageToolbarIcon — displays toolbar icon image. Size = 0x48.
// Decompiled from: TutorialImageToolbarIcon_Ctor (0x1400e0c30)
namespace TutorialImageToolbarIconAddr {
    constexpr uintptr_t Ctor    = 0x1400e0c30;
    constexpr uintptr_t Factory = 0x140028020;
    constexpr uintptr_t Vtable  = 0x140160bb0; // concrete vtable (set after base ctor)
}

// TutorialImageToolbarIcon has same size as TutorialImageHotKey (0x48)
