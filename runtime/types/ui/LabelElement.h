#pragma once
// ============================================================================
// runtime/types/ui/LabelElement.h — UI::LabelElement (0x130 bytes)
// ============================================================================
//
// Inheritance: UI::Element (0xC0) -> UI::TextElement (0x100) -> UI::LabelElement
//
//   Default ctor:  0x1800d2620 (??0LabelElement@UI@@QEAA@XZ)
//   Copy ctor:     0x180042a00 (??0LabelElement@UI@@QEAA@AEBV01@@Z)
//   Dtor:          0x1800d25f0 (??1LabelElement@UI@@UEAA@XZ)
//   SetSprite:     0x180042980 (?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z)
//   GetSprite:     0x1800429d0 (?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ)
//
// ENDGAME B.3 step 6, block C3 — THE DEFECT THIS HEADER CARRIED, AND ITS FIX.
// Until this block the file held a BARE `struct LabelElement` whose TextElement
// part (+0xC0..+0xFF) used an old table this header itself marked WRONG
// (`textStringC0/textDataC8/fontPtrD0/fieldD8/textColorE0`), pinned by twenty
// `static_assert`s at the right OFFSETS under the wrong NAMES. TextElement is a
// real class now (TextElement.h, its layout read off its ctor 0x18011eb80), so:
//   * `UI::LabelElement` below derives from `UI::TextElement` and takes that
//     class's names for the inherited prefix; it pins only its OWN +0x100..+0x12f;
//   * the old bare POD moved to `RtUI::LabelElement` (B4b-1) with its TextElement
//     names CORRECTED to TextElement's and every one of its offset asserts kept.
//     Its one remaining user is runtime/src/ui/element_sprite_debug.cpp, which
//     reads the sprite triple for a debug print (the calls there are members).
//
// LabelElement's OWN LAYOUT, every field a store of `??0LabelElement@UI@@QEAA@XZ`
// (0x1800d2620, read as DISASSEMBLY by block C3; `CALL ??0TextElement` first):
//     +0x100 qword 0     _sprite.spriteSheet   UI::Sprite, the icon
//     +0x108 dword -1    _sprite.spriteIndex   (+0x10c NOT written)
//     +0x110 dword 3     _align        the icon/text pair's inner alignment 0..8
//     +0x114 QWORD 1     _mode         0 icon right, 1 left, 2 below, 3 above
//                        _minTextWidth (the high half of the same store: 0)
//     +0x11c dword 2     _gap          pixels between icon and text
//     +0x120 QWORD -1    _iconWidth    override, <1 = use the sprite
//                        _iconHeight   override, <1 = use the sprite
//     +0x128 dword -1    _iconColor    SetColor / SetSpriteColor write it
//     +0x12c dword 0     _textWidth    an OUTPUT of ComputeSize
// Names: labelelement.cpp's port, which read each one's use in ComputeSize /
// Draw; `_iconColor` is also `?SetSpriteColor@` (0x1800429f0:
// `MOV [RCX+0x128],EDX / RET`).
// SIZE 0x130: `MOV EDX,0x130` in the slot-0 vector deleting dtor 0x180042bb0.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0LabelElement@UI@@QEAA@XZ` (0x1800d2620) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~LabelElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7LabelElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <type_traits>
#include <cstdint>
#include <cstddef>

#include "Element.h"
#include "TextElement.h"
#include "Sprite.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

// ############################################################################
// RtUI::LabelElement — the old bare POD, moved (B4b-1), names corrected.
// ############################################################################
namespace RtUI {

struct LabelElement {
    // --- UI::Element base (0xC0 bytes) ---
    void*       vtable;             // +0x00  vtable pointer
    uint8_t     elementBase[0xB8];  // +0x08  UI::Element base data

    // --- UI::TextElement fields (0xC0..0xFF) — TextElement.h's names ---
    void*       _font;              // +0xC0  ExternalPtr<UI::Font>
    void*       _paramsData;        // +0xC8  Array<System::String> data
    int32_t     _paramsSize;        // +0xD0  Array size
    int32_t     _paramsCapacity;    // +0xD4  Array capacity
    void*       _textTable;         // +0xD8  UI::String table
    int32_t     _textIndex;         // +0xE0  UI::String index (ctor -1)
    uint32_t    _textPad;           // +0xE4  never written
    uint8_t     _drawShadow;        // +0xE8
    uint8_t     _wrap;              // +0xE9
    uint16_t    _padEA;             // +0xEA  never written
    int32_t     _lineSpacing;       // +0xEC  ctor 100
    int32_t     _maxWidth;          // +0xF0  ctor -1
    int32_t     _justify;           // +0xF4  ctor 0
    void*       _cachedText;        // +0xF8  System::String (CacheText)

    // --- UI::LabelElement's own fields ---
    void*       spriteSheet;        // +0x100  SpriteSheet* (0 = none)
    int32_t     spriteIndex;        // +0x108  sprite index (-1 = none)
    uint32_t    spriteFlags;        // +0x10C  UI::Sprite pad (never written)
    uint32_t    alignment;          // +0x110  _align, ctor 3
    uint32_t    field114;           // +0x114  _mode, ctor 1
    uint32_t    field118;           // +0x118  _minTextWidth, ctor 0
    uint32_t    field11C;           // +0x11C  _gap, ctor 2
    uint32_t    field120;           // +0x120  _iconWidth, ctor -1
    uint32_t    field124;           // +0x124  _iconHeight, ctor -1
    int32_t     field128;           // +0x128  _iconColor, ctor -1
    uint32_t    field12C;           // +0x12C  _textWidth, ctor 0
};

static_assert(sizeof(LabelElement) == 0x130, "LabelElement must be 0x130 bytes");
static_assert(offsetof(LabelElement, vtable)          == 0x00,  "vtable at +0x00");
static_assert(offsetof(LabelElement, elementBase)     == 0x08,  "elementBase at +0x08");
static_assert(offsetof(LabelElement, _font)           == 0xC0,  "_font at +0xC0");
static_assert(offsetof(LabelElement, _paramsData)     == 0xC8,  "_paramsData at +0xC8");
static_assert(offsetof(LabelElement, _paramsSize)     == 0xD0,  "_paramsSize at +0xD0");
static_assert(offsetof(LabelElement, _textTable)      == 0xD8,  "_textTable at +0xD8");
static_assert(offsetof(LabelElement, _textIndex)      == 0xE0,  "_textIndex at +0xE0");
static_assert(offsetof(LabelElement, _lineSpacing)    == 0xEC,  "_lineSpacing at +0xEC");
static_assert(offsetof(LabelElement, _maxWidth)       == 0xF0,  "_maxWidth at +0xF0");
static_assert(offsetof(LabelElement, _cachedText)     == 0xF8,  "_cachedText at +0xF8");
static_assert(offsetof(LabelElement, spriteSheet)     == 0x100, "spriteSheet at +0x100");
static_assert(offsetof(LabelElement, spriteIndex)     == 0x108, "spriteIndex at +0x108");
static_assert(offsetof(LabelElement, spriteFlags)     == 0x10C, "spriteFlags at +0x10C");
static_assert(offsetof(LabelElement, alignment)       == 0x110, "alignment at +0x110");
static_assert(offsetof(LabelElement, field114)        == 0x114, "field114 at +0x114");
static_assert(offsetof(LabelElement, field11C)        == 0x11C, "field11C at +0x11C");
static_assert(offsetof(LabelElement, field120)        == 0x120, "field120 at +0x120");
static_assert(offsetof(LabelElement, field128)        == 0x128, "field128 at +0x128");
static_assert(offsetof(LabelElement, field12C)        == 0x12C, "field12C at +0x12C");

}  // namespace RtUI

// ############################################################################
// ENDGAME B.3 step 6 (block C3) — `UI::LabelElement`, the REAL C++ class
// ############################################################################
//
// PARENT `UI::TextElement` — READ (B36_SLOTS.md section 3), RECORDED, NOT USED:
//     ?_TypeName@LabelElement@UI@@CAPEBDXZ       @0x1800427b0 -> "LabelElement"
//     ?_ParentTypeName@LabelElement@UI@@CAPEBDXZ @0x1800427c0 -> "TextElement"
//
// `??_7LabelElement@UI@@6B@` @ 0x180156ad0, TWELVE slots: [0] 0x180042bb0
// (deleting_dtor.cpp row) · [1] 0x1800427f0 TypeName · [2] 0x1800427d0 Type ·
// [4] 0x1800d2490 ComputeSize · [5] 0x1800d2330 ComputeLayout · [6] 0x1800d1c80
// Draw · [10] 0x180042970 SetColor — the rest inherited through TextElement.
//
// (Until ENDGAME B.3.9:) ABSTRACT in C++; no real ctor: live ones are built by the `??0LabelElement`
// door (ui_factory_wave28.cpp) and wear the MINTED table.
// ############################################################################
namespace UI {

class LabelElement : public TextElement {
public:
    // ??0LabelElement@UI@@QEAA@XZ 0x1800d2620 — the REAL constructor: CALL
    // ??0TextElement (TextElement()), then the icon Sprite's own default ctor
    // ({nullptr, -1}; +0x10c NOT written), +0x110 DWORD 3, ONE QWORD 1 over
    // +0x114/+0x118, +0x11c DWORD 2, ONE QWORD -1 over +0x120/+0x124, +0x128
    // DWORD -1, +0x12c DWORD 0.
    LabelElement() : TextElement() {
        _align        = 3;
        _mode         = 1;
        _minTextWidth = 0;
        _gap          = 2;
        _iconWidth    = -1;
        _iconHeight   = -1;
        _iconColor    = 0xffffffffu;
        _textWidth    = 0;
    }

    // [0] 0x180042bb0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x130`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<LabelElement*>(self + static_cast<int64_t>(i) * 0x130)
                    ->~LabelElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~LabelElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                       // [1] 0x1800427f0
    uint32_t    Type() const override;                           // [2] 0x1800427d0
    void ComputeSize() override;                                 // [4] 0x1800d2490
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;           // [5] 0x1800d2330
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800d1c80
    void SetColor(uint32_t color) override;                      // [10] 0x180042970

    // ?GetSprite@LabelElement@UI@@QEBA?AVSprite@2@XZ  0x1800429d0 — RETSLOT in
    // retail; by value here. DEFINED in widgets_layout.cpp (fragment pass),
    // whose registered door placement-news the result.
    Sprite GetSprite() const;
    // ?SetSprite@LabelElement@UI@@QEAAXVSprite@2@@Z  0x180042980. `VSprite@2@`
    // is the 0x10-byte UI::Sprite BY VALUE, which MSVC passes by HIDDEN POINTER
    // (the RULE B6-11 shape): declared with the pointer, in retail order. Its
    // body is DEFINED in widgets_layout.cpp (fragment pass).
    void SetSprite(const Sprite* sprite);
    // ?SetSpriteColor@LabelElement@UI@@QEAAXI@Z  0x1800429f0 —
    // `MOV [RCX+0x128],EDX / RET`, the whole body.
    void SetSpriteColor(uint32_t color) { _iconColor = color; }

    LabelElement(const LabelElement& src);                       // 0x180042a00  not ported (door retired: no caller)
    LabelElement& operator=(const LabelElement& src);            // 0x180042ac0  not ported
    static uint32_t TypeId();                                    // 0x1800427e0  ported over the minted record
    static Object::Base* CreateNewObject();                      // 0x1800428e0  the class's own (B39 D2)
    static Object::Base* CreateInPlaceObject(void* place);       // 0x180042960  the class's own (B39 D2)
    static void* operator new(std::size_t bytes);                // 0x180042800  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x180042870  not ported
    static void  operator delete(void* p);                       // 0x180042880  not ported

protected:
    // `??1LabelElement@UI@@UEAA@XZ` (0x1800d25f0). NON-VIRTUAL and EMPTY
    // (labelelement.cpp): restamp, then the tail JMP to ~TextElement, which the
    // compiler chains.
    ~LabelElement();

private:
    static const char* _TypeName();                              // 0x1800427b0 -> "LabelElement"
    static const char* _ParentTypeName();                        // 0x1800427c0 -> "TextElement"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801fa040 (minted)

public:
    // ---- the 0x30 bytes LabelElement adds -----------------------------------
    Sprite   _sprite;         // +0x100  the icon
    int32_t  _align;          // +0x110  ctor 3
    uint32_t _mode;           // +0x114  ctor 1
    int32_t  _minTextWidth;   // +0x118  ctor 0
    int32_t  _gap;            // +0x11c  ctor 2
    int32_t  _iconWidth;      // +0x120  ctor -1
    int32_t  _iconHeight;     // +0x124  ctor -1
    uint32_t _iconColor;      // +0x128  ctor -1
    int32_t  _textWidth;      // +0x12c  ctor 0
};

static_assert(sizeof(LabelElement) == 0x130,
              "UI::LabelElement is 0x130 (MOV EDX,0x130 in 0x180042bb0)");
static_assert(alignof(LabelElement) == 8, "UI::LabelElement aligns to 8");
static_assert(!std::has_virtual_destructor<LabelElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<LabelElement>::value, "B39 D1: concrete");
static_assert(offsetof(LabelElement, _sprite) == sizeof(TextElement),
              "LabelElement's own fields begin at the end of TextElement");
static_assert(offsetof(LabelElement, _sprite)       == 0x100, "");
static_assert(offsetof(LabelElement, _align)        == 0x110, "");
static_assert(offsetof(LabelElement, _mode)         == 0x114, "");
static_assert(offsetof(LabelElement, _minTextWidth) == 0x118, "");
static_assert(offsetof(LabelElement, _gap)          == 0x11c, "");
static_assert(offsetof(LabelElement, _iconWidth)    == 0x120, "");
static_assert(offsetof(LabelElement, _iconHeight)   == 0x124, "");
static_assert(offsetof(LabelElement, _iconColor)    == 0x128, "");
static_assert(offsetof(LabelElement, _textWidth)    == 0x12c, "");
// The class and the RtUI POD describe the same bytes.
static_assert(sizeof(LabelElement) == sizeof(RtUI::LabelElement), "");
static_assert(offsetof(LabelElement, _font)       == offsetof(RtUI::LabelElement, _font), "");
static_assert(offsetof(LabelElement, _text)       == offsetof(RtUI::LabelElement, _textTable), "");
static_assert(offsetof(LabelElement, _cachedText) == offsetof(RtUI::LabelElement, _cachedText), "");
static_assert(offsetof(LabelElement, _sprite)     == offsetof(RtUI::LabelElement, spriteSheet), "");
static_assert(offsetof(LabelElement, _iconColor)  == offsetof(RtUI::LabelElement, field128), "");
static_assert(offsetof(LabelElement, _textWidth)  == offsetof(RtUI::LabelElement, field12C), "");

}  // namespace UI
