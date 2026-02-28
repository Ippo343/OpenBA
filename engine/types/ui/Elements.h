#pragma once
// Game UI Elements -- binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// 2 Element classes registered in the exe (beyond the 22+ base elements in Runtime-x64.dll).
// Both extend UI::Element and embed a UI::Sprite at +0xC0.
// Construction is inline in Factory (no separate Ctor function).
//
// UI::Element base: 0xC0 bytes (initialized by UI::Element::Element in Runtime DLL)
// UI::Sprite: initialized by UI::Sprite::Sprite at +0xC0

#include <cstdint>
#include <cstddef>

namespace CaptureBoxElementAddr {
    constexpr uintptr_t Factory = 0x140013eb0;
    constexpr uintptr_t Vtable  = 0x14015d440;
}

namespace FrustumOutlineElementAddr {
    constexpr uintptr_t Factory = 0x140012400;
    constexpr uintptr_t Vtable  = 0x14015c2a0;
}

// --- CaptureBoxElement (0xE0) ---
// Factory: UI::Element::Element(this), vtable, UI::Sprite::Sprite@+0xC0, +0xD0=0, +0xD8=0
struct CaptureBoxElement {
    void*    vtable;            // +0x00
    uint8_t  elementBase[0xB8]; // +0x08: UI::Element base data (Runtime DLL)
    uint8_t  sprite[0x10];      // +0xC0: UI::Sprite (initialized by Sprite::Sprite)
    uint64_t dataD0;            // +0xD0: zeroed
    uint64_t dataD8;            // +0xD8: zeroed
};
static_assert(sizeof(CaptureBoxElement) == 0xE0, "CaptureBoxElement size mismatch");
static_assert(offsetof(CaptureBoxElement, sprite)  == 0xC0, "");
static_assert(offsetof(CaptureBoxElement, dataD0)  == 0xD0, "");

// --- FrustumOutlineElement (0xD8) ---
// Factory: UI::Element::Element(this), vtable, UI::Sprite::Sprite@+0xC0, +0xD0=-1
struct FrustumOutlineElement {
    void*    vtable;            // +0x00
    uint8_t  elementBase[0xB8]; // +0x08: UI::Element base data (Runtime DLL)
    uint8_t  sprite[0x10];      // +0xC0: UI::Sprite (initialized by Sprite::Sprite)
    int64_t  dataD0;            // +0xD0: initialized to -1 (0xFFFFFFFFFFFFFFFF)
};
static_assert(sizeof(FrustumOutlineElement) == 0xD8, "FrustumOutlineElement size mismatch");
static_assert(offsetof(FrustumOutlineElement, sprite) == 0xC0, "");
static_assert(offsetof(FrustumOutlineElement, dataD0) == 0xD0, "");
