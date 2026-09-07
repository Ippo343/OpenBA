#pragma once
// UI::LabelElement — binary-compatible struct definition.
// Runtime-x64.dll class, 0x130 bytes.
//
// Inheritance: UI::Element (0xC0) -> UI::TextElement (0x100) -> UI::LabelElement
//
// Decompiled from:
//   Default ctor:  0x1800d2620 (??0LabelElement@UI@@QEAA@XZ)
//   Copy ctor:     0x180042a00 (??0LabelElement@UI@@QEAA@AEBV01@@Z)
//   Dtor:          0x1800d25f0 (??1LabelElement@UI@@UEAA@XZ)
//   SetSprite:     0x180042980 (?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z)
//   GetSprite:     0x1800429d0 (?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ)
//
// TextElement base (0xC0..0xFF):
//
// *** CORRECTED by Wave 7 lane E (docs/runtime_port/WAVE7_LANE_E.md) against
// live call sites; the block below is what shipped here before and was WRONG
// about the text string's offset. Kept only as a record of what was disproved
// -- do not use it. ***
//   Default ctor @ 0x18011eb80: Element base, then text fields:
//     +0xC0: 8 bytes = 0  (text string ptr / System::String)   -- WRONG, see below
//     +0xC8: 8 bytes = 0  (text data)                          -- WRONG, see below
//     +0xD0: 8 bytes = 0  (font ptr?)                          -- WRONG, see below
//     +0xD8: 8 bytes = 0  (unknown)                            -- WRONG, see below
//     +0xE0: 4 bytes = -1 (text color? ARGB)                   -- WRONG, see below
//     +0xE8: 2 bytes = 0  (text flags?)
//     +0xEC: 4 bytes = 100 (max width / wrap limit?)
//     +0xF0: 4 bytes = -1 (unknown)
//     +0xF4: 4 bytes = 0  (unknown)
//     +0xF8: 8 bytes = 0  (cached text ptr?)                   -- this one IS right
//
// ACTUAL layout, proven from TextElement::SetParameter (0x18011e9b0) and
// TextElement::SetText (0x180042070) -- see runtime/types/ui/TextElement.h for
// the full evidence and the ported bodies in runtime/src/ui/widgets_layout.cpp:
//     +0xC8: Collection::Array<System::String> _params .data
//     +0xD0: _params .size  (int32_t)   -- read by SetParameter's bounds check
//     +0xD4: _params .capacity (int32_t, inferred, not directly read)
//     +0xD8: UI::String _text, 16 bytes (0xD8, 0xE0)  -- what SetText(UI::String) writes
//     +0xF8: cached formatted-text pointer, managed by the private CacheText
// +0xC0..0xC8 (8 bytes) is not touched by either function this lane ported;
// the old guess ("text string ptr") is neither confirmed nor refuted here.
//
// SetSprite behavior:
//   - Sprite embedded at +0x100 (NOT +0xC0 like ImageElement)
//   - Same dirty-flag walk: sets 0x8000000 at +0x34, walks parent chain via +0x98
//   - Copies 0x10 bytes to +0x100
//
// GetSprite behavior:
//   - Returns 0x10-byte Sprite via hidden return pointer (member function ABI)
//   - RCX=this, RDX=retval pointer

#include <cstdint>
#include <cstddef>

// UI::LabelElement (0x130 bytes)
// Extends TextElement (0x100) with a Sprite and label-specific properties.
struct LabelElement {
    // --- UI::Element base (0xC0 bytes) ---
    void*       vtable;             // +0x00  vtable pointer
    uint8_t     elementBase[0xB8];  // +0x08  UI::Element base data (Runtime DLL)

    // --- UI::TextElement fields (0xC0..0xFF, 0x40 bytes) ---
    uint64_t    textStringC0;       // +0xC0  text string (System::String or ptr)
    uint64_t    textDataC8;         // +0xC8  text data
    uint64_t    fontPtrD0;          // +0xD0  font pointer
    uint64_t    fieldD8;            // +0xD8  unknown
    int32_t     textColorE0;        // +0xE0  text color ARGB (-1 = white/default)
    uint32_t    fieldE4;            // +0xE4  gap (implicitly zeroed)
    uint16_t    textFlagsE8;        // +0xE8  text flags (2 bytes, init 0)
    uint16_t    padEA;              // +0xEA  padding
    uint32_t    maxWidthEC;         // +0xEC  max width / wrap limit (init 100)
    int32_t     fieldF0;            // +0xF0  unknown (init -1)
    uint32_t    fieldF4;            // +0xF4  unknown (init 0)
    uint64_t    cachedTextF8;       // +0xF8  cached text ptr (init 0)

    // --- UI::LabelElement subclass fields ---
    // UI::Sprite (0x10 bytes) at +0x100
    void*       spriteSheet;        // +0x100  SpriteSheet* (0 = none)
    int32_t     spriteIndex;        // +0x108  sprite index (-1 = none)
    uint32_t    spriteFlags;        // +0x10C  sprite flags/padding

    // Additional label properties
    uint32_t    alignment;          // +0x110  alignment mode (init 3)
    uint32_t    field114;           // +0x114  init 1 (from 8-byte write: low 4 bytes)
    uint32_t    field118;           // +0x118  init 0 (from 8-byte write: high 4 bytes)
    uint32_t    field11C;           // +0x11C  init 2
    uint32_t    field120;           // +0x120  init -1 (from 8-byte write: low 4 bytes)
    uint32_t    field124;           // +0x124  init -1 (from 8-byte write: high 4 bytes)
    int32_t     field128;           // +0x128  init -1
    uint32_t    field12C;           // +0x12C  init 0
};

static_assert(sizeof(LabelElement) == 0x130, "LabelElement must be 0x130 bytes");
static_assert(offsetof(LabelElement, vtable)        == 0x00,  "vtable at +0x00");
static_assert(offsetof(LabelElement, elementBase)   == 0x08,  "elementBase at +0x08");
static_assert(offsetof(LabelElement, textStringC0)  == 0xC0,  "textStringC0 at +0xC0");
static_assert(offsetof(LabelElement, textDataC8)    == 0xC8,  "textDataC8 at +0xC8");
static_assert(offsetof(LabelElement, fontPtrD0)     == 0xD0,  "fontPtrD0 at +0xD0");
static_assert(offsetof(LabelElement, fieldD8)       == 0xD8,  "fieldD8 at +0xD8");
static_assert(offsetof(LabelElement, textColorE0)   == 0xE0,  "textColorE0 at +0xE0");
static_assert(offsetof(LabelElement, maxWidthEC)    == 0xEC,  "maxWidthEC at +0xEC");
static_assert(offsetof(LabelElement, fieldF0)       == 0xF0,  "fieldF0 at +0xF0");
static_assert(offsetof(LabelElement, cachedTextF8)  == 0xF8,  "cachedTextF8 at +0xF8");
static_assert(offsetof(LabelElement, spriteSheet)   == 0x100, "spriteSheet at +0x100");
static_assert(offsetof(LabelElement, spriteIndex)   == 0x108, "spriteIndex at +0x108");
static_assert(offsetof(LabelElement, spriteFlags)   == 0x10C, "spriteFlags at +0x10C");
static_assert(offsetof(LabelElement, alignment)     == 0x110, "alignment at +0x110");
static_assert(offsetof(LabelElement, field114)      == 0x114, "field114 at +0x114");
static_assert(offsetof(LabelElement, field11C)      == 0x11C, "field11C at +0x11C");
static_assert(offsetof(LabelElement, field120)      == 0x120, "field120 at +0x120");
static_assert(offsetof(LabelElement, field128)      == 0x128, "field128 at +0x128");
static_assert(offsetof(LabelElement, field12C)      == 0x12C, "field12C at +0x12C");
