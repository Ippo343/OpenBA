#pragma once
// UI::ImageElement — binary-compatible struct definition.
// Runtime-x64.dll class, 0xE0 bytes.
//
// Inheritance: UI::Element (0xC0) -> UI::ImageElement
//
// Decompiled from:
//   Default ctor:  0x180118410 (??0ImageElement@UI@@QEAA@XZ)
//   Copy ctor:     0x1800364c0 (??0ImageElement@UI@@QEAA@AEBV01@@Z)
//   Dtor:          0x180118470 (??1ImageElement@UI@@UEAA@XZ)
//   SetSprite:     0x180118600 (?SetSprite@ImageElement@UI@@QEAAXVSprite@2@@Z)
//   GetSprite:     0x1801182a0 (?GetSprite@ImageElement@UI@@QEBA?AVSprite@2@XZ)
//
// SetSprite behavior:
//   - Compares new sprite with current at +0xC0/+0xC8
//   - If different: walks parent chain via +0x98, sets dirty flag 0x8000000 at +0x34
//   - Copies 0x10 bytes of Sprite data to +0xC0
//
// GetSprite behavior:
//   - Returns 0x10-byte Sprite via hidden return pointer (member function ABI)
//   - RCX=this, RDX=retval pointer

#include <cstdint>
#include <cstddef>

namespace UI {

// UI::Sprite — 0x10 bytes, embedded in element subclasses
// First 8 bytes = SpriteSheet* pointer
// Next 4 bytes = sprite index (0xFFFFFFFF = none)
// Last 4 bytes = padding/flags
struct Sprite {
    void*       sheet;          // +0x00  SpriteSheet* pointer
    int32_t     index;          // +0x08  sprite index (-1 = none)
    int32_t     pad0C;          // +0x0C  padding
};
static_assert(sizeof(Sprite) == 0x10, "Sprite must be 0x10 bytes");

} // namespace UI

// UI::ImageElement (0xE0 bytes)
// Extends Element (0xC0) with a Sprite and additional image properties.
struct ImageElement {
    // --- UI::Element base (0xC0 bytes) ---
    void*       vtable;             // +0x00  vtable pointer
    uint8_t     elementBase[0xB8];  // +0x08  UI::Element base data (Runtime DLL)

    // --- ImageElement subclass fields ---
    // UI::Sprite (0x10 bytes)
    void*       spriteSheet;        // +0xC0  SpriteSheet* (0 = none)
    int32_t     spriteIndex;        // +0xC8  sprite index (-1 = none)
    uint32_t    spriteFlags;        // +0xCC  sprite flags/padding

    // Additional image properties
    uint32_t    fieldD0;            // +0xD0  zeroed in ctor (image mode/flags?)
    uint32_t    fieldD4;            // +0xD4  init -1 (0xFFFFFFFF)
    uint32_t    fieldD8;            // +0xD8  init -1 (0xFFFFFFFF)
    uint32_t    padDC;              // +0xDC  padding to 0xE0
};

static_assert(sizeof(ImageElement) == 0xE0, "ImageElement must be 0xE0 bytes");
static_assert(offsetof(ImageElement, vtable)       == 0x00, "vtable at +0x00");
static_assert(offsetof(ImageElement, elementBase)  == 0x08, "elementBase at +0x08");
static_assert(offsetof(ImageElement, spriteSheet)  == 0xC0, "spriteSheet at +0xC0");
static_assert(offsetof(ImageElement, spriteIndex)  == 0xC8, "spriteIndex at +0xC8");
static_assert(offsetof(ImageElement, spriteFlags)  == 0xCC, "spriteFlags at +0xCC");
static_assert(offsetof(ImageElement, fieldD0)      == 0xD0, "fieldD0 at +0xD0");
static_assert(offsetof(ImageElement, fieldD4)      == 0xD4, "fieldD4 at +0xD4");
static_assert(offsetof(ImageElement, fieldD8)      == 0xD8, "fieldD8 at +0xD8");
