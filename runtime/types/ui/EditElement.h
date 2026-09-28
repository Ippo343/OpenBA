#pragma once
// ============================================================================
// runtime/types/ui/EditElement.h — UI::EditElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D named `_text` (+0xC8) and `_flagE0` (+0xE0):
//
//   +0xC8  _text    char*    The element's current value, AS TEXT. Read by
//                            `?GetValue@NumberEditElement@UI@@QEBAHXZ`
//                            (0x1800d4b50) and read/written by
//                            `?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z`
//                            (0x1800d0db0). System::String is ONE POINTER.
//   +0xE0  _flagE0  uint8_t  A one-byte flag EditElement::SetText gates its
//                            write on, and which SpinnerElement::IsActive
//                            (0x1800d7c30) returns UNNEGATED.
//
// ENDGAME B.3 step 6, block C3 — THE SIZE DEFECT, CLOSED FROM RETAIL BYTES.
// This header used to say its size was "not asserted", and
// NumberEditElement.h asserted `sizeof(EditElement) == 0xE8` (the compiler's
// padding of a struct that stopped at +0xE0). RETAIL SAYS 0xF8: `MOV EDX,0xf8`
// in the slot-0 vector deleting destructor 0x18003fed0 (B36_RULES 2.3). The
// sixteen bytes +0xE8..+0xF7 are EditElement's own, pinned from
// `??0EditElement@UI@@QEAA@XZ` (0x1800d0f10), read as DISASSEMBLY:
//
//     CALL ??0Element            ; 0x180115e50
//     MOV [RBX],??_7EditElement  ; 0x180156810
//     MOV qword [RBX+0xc0],0     _font        ExternalPtr<UI::Font>
//     MOV qword [RBX+0xc8],0     _text        System::String
//     MOV dword [RBX+0xd0],0     _maxLength   ?GetMaxLength@ 0x18003fc70 = MOV EAX,[RCX+0xd0]
//     MOV byte  [RBX+0xd4],0     _password    Draw/ComputeSize mask with '*'
//     MOV qword [RBX+0xd8],0     _filter      System::String, accepted characters
//     MOV byte  [RBX+0xe0],0     _active      ?IsActive@ 0x18003fc80 = MOVZX EAX,byte [RCX+0xe0]
//     MOV dword [RBX+0xe4],0     _caret       caret index, in characters
//     MOV byte  [RBX+0xe8],0     _blinkOn     caret blink phase
//     MOV qword [RBX+0xf0],0     _blinkAccumulator  ticks (Update adds Time)
//
// +0xD5..+0xD7, +0xE1..+0xE3 and +0xE9..+0xEF are NOT written by the ctor and
// read by no body: explicit pads. The field names are editelement.cpp's port
// (each read at its use in Update / Draw / ComputeSize) and the exported
// accessors' (`GetMaxLength`, `IsActive`, `GetText` = `LEA RAX,[RCX+0xc8]`).
//
// So `NumberEditElement` is EditElement + `_min`/`_max` at +0xF8/+0xFC = 0x100,
// exactly what the old pad arithmetic happened to land on.
//
// The old POD moved to `RtUI::EditElement` (B4b-1), WIDENED to the retail 0xF8
// with the same fields, and keeps its legacy names (`base`, `_text`, `_flagE0`)
// for widgets_input.cpp, which still reaches EditElement / NumberEditElement /
// SpinnerElement's child through the PODs.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0EditElement@UI@@QEAA@XZ` (0x1800d0f10) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~EditElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7EditElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <type_traits>
#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace RtUI {

struct EditElement {
    RtUI::Element base;           // +0x00 .. +0xBF
    void*    _font;               // +0xC0  ExternalPtr<UI::Font>
    char*    _text;               // +0xC8
    int32_t  _maxLength;          // +0xD0
    uint8_t  _password;           // +0xD4
    uint8_t  _padD5[3];           // +0xD5  never written
    char*    _filter;             // +0xD8
    uint8_t  _flagE0;             // +0xE0  (the class's `_active`)
    uint8_t  _padE1[3];           // +0xE1  never written
    int32_t  _caret;              // +0xE4
    uint8_t  _blinkOn;            // +0xE8
    uint8_t  _padE9[7];           // +0xE9  never written
    int64_t  _blinkAccumulator;   // +0xF0
};

static_assert(offsetof(EditElement, base)    == 0x00, "EditElement::base @ +0x00");
static_assert(offsetof(EditElement, _text)   == 0xC8, "EditElement::_text @ +0xC8");
static_assert(offsetof(EditElement, _flagE0) == 0xE0, "EditElement::_flagE0 @ +0xE0");
static_assert(offsetof(EditElement, _blinkAccumulator) == 0xF0, "EditElement @ +0xF0");
static_assert(sizeof(EditElement) == 0xF8,
              "EditElement is 0xF8 (MOV EDX,0xf8 in its deleting dtor 0x18003fed0)");

}  // namespace RtUI

// ############################################################################
// ENDGAME B.3 step 6 (block C3) — `UI::EditElement`, the REAL C++ class
// ############################################################################
//
// PARENT `UI::Element` — READ (B36_SLOTS.md section 3), RECORDED, NOT USED:
//     ?_TypeName@EditElement@UI@@CAPEBDXZ       @0x18003faa0 -> "EditElement"
//     ?_ParentTypeName@EditElement@UI@@CAPEBDXZ @0x18003fab0 -> "Element"
//
// `??_7EditElement@UI@@6B@` @ 0x180156810, THIRTEEN slots: [0] 0x18003fed0 ·
// [1] 0x18003fae0 TypeName · [2] 0x18003fac0 Type · [3] Element::Persist ·
// [4] 0x1800d0c10 ComputeSize · [5] 0x1800d0ab0 ComputeLayout · [6] 0x1800d0820
// Draw · [7] 0x1800d0220 Update · [8..11] Element's · [12] 0x1800d0da0
// FinalizeText — INTRODUCED here (`virtual`, RULE B6-1.6).
//
// CONCRETE since ENDGAME B.3.9 UI-A: the real ctor is ??0EditElement
// (0x1800d0f10); live ones wear the compiler's table.
// ############################################################################
namespace UI {

class Font;

class EditElement : public Element {
public:
    // ??0EditElement@UI@@QEAA@XZ 0x1800d0f10 — the REAL constructor: CALL
    // ??0Element (Element()), then 0x1800d0f38..0x1800d0f6b: +0xc0/+0xc8 QWORD
    // 0, +0xd0 DWORD 0, +0xd4 BYTE 0, +0xd8 QWORD 0, +0xe0 BYTE 0, +0xe4 DWORD
    // 0, +0xe8 BYTE 0, +0xf0 QWORD 0. The pad bytes stay unwritten.
    EditElement() : Element() {
        _font             = nullptr;
        _text             = nullptr;
        _maxLength        = 0;
        _password         = 0;
        _filter           = nullptr;
        _active           = 0;
        _caret            = 0;
        _blinkOn          = 0;
        _blinkAccumulator = 0;
    }

    // [0] 0x18003fed0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xf8`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<EditElement*>(self + static_cast<int64_t>(i) * 0xf8)
                    ->~EditElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~EditElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x18003fae0, Type 0x18003fac0, TypeId 0x18003fad0, _TypeName 0x18003faa0
    //         _ParentTypeName 0x18003fab0, CreateNewObject 0x18003fbd0, CreateInPlaceObject 0x18003fc50
    DECLARE_TYPE(EditElement)
public:
    void ComputeSize() override;                                 // [4] 0x1800d0c10
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;           // [5] 0x1800d0ab0
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800d0820
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;              // [7] 0x1800d0220 (B6-11)
    virtual void FinalizeText();                                 // [12] 0x1800d0da0 — INTRODUCED

    // ---- non-virtual ---------------------------------------------------------
    // ?SetText@EditElement@UI@@QEAAXAEBVString@System@@@Z  0x1800d0db0 — the
    // body is widgets_input.cpp's door (fragment pass); declared, undefined.
    void SetText(const System::String& text);
    // ?SelectCharacter@EditElement@UI@@AEBADH@Z  0x1800cfd40 — `AEBA` private;
    // the body is element_bodies_b5.cpp's door; declared, undefined.
    char SelectCharacter(int32_t keyIndex) const;
    // The three one-instruction accessors, whole bodies (disassembly above).
    const System::String& GetText() const {                      // 0x18003fc60
        return *reinterpret_cast<const System::String*>(&_text);
    }
    int32_t GetMaxLength() const { return _maxLength; }          // 0x18003fc70
    bool    IsActive() const     { return _active != 0; }        // 0x18003fc80

    EditElement(const EditElement& src);                         // 0x18003fc90  not ported (door retired: no caller)
    EditElement& operator=(const EditElement& src);              // 0x18003fd70  not ported
    static void* operator new(std::size_t bytes);                // 0x18003faf0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x18003fb60  not ported
    static void  operator delete(void* p);                       // 0x18003fb70  not ported

protected:
    // `??1EditElement@UI@@UEAA@XZ` (0x1800d0e50). NON-VIRTUAL; editelement.cpp
    // holds retail's body (drop the text-input latch if held, release the
    // filter and the text); the compiler chains ~Element.
    ~EditElement();

private:

public:
    // ---- the 0x38 bytes EditElement adds ------------------------------------
    Font*    _font;               // +0xc0  ExternalPtr<UI::Font>
    char*    _text;               // +0xc8  System::String (refcounted char*)
    int32_t  _maxLength;          // +0xd0
    uint8_t  _password;           // +0xd4
    uint8_t  _padD5[3];           // +0xd5  never written
    char*    _filter;             // +0xd8  System::String; empty = any character
    uint8_t  _active;             // +0xe0  focused; holds the text-input latch
    uint8_t  _padE1[3];           // +0xe1  never written
    int32_t  _caret;              // +0xe4
    uint8_t  _blinkOn;            // +0xe8
    uint8_t  _padE9[7];           // +0xe9  never written
    int64_t  _blinkAccumulator;   // +0xf0  System::Time ticks
};

static_assert(sizeof(EditElement) == 0xf8,
              "UI::EditElement is 0xf8 (MOV EDX,0xf8 in 0x18003fed0) — NOT 0xe8");
static_assert(alignof(EditElement) == 8, "UI::EditElement aligns to 8");
static_assert(!std::has_virtual_destructor<EditElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<EditElement>::value, "B39 D1: concrete");
static_assert(offsetof(EditElement, _font) == sizeof(Element),
              "EditElement's own fields begin at the end of Element");
static_assert(offsetof(EditElement, _font)             == 0xc0, "");
static_assert(offsetof(EditElement, _text)             == 0xc8, "");
static_assert(offsetof(EditElement, _maxLength)        == 0xd0, "");
static_assert(offsetof(EditElement, _password)         == 0xd4, "");
static_assert(offsetof(EditElement, _filter)           == 0xd8, "");
static_assert(offsetof(EditElement, _active)           == 0xe0, "");
static_assert(offsetof(EditElement, _caret)            == 0xe4, "");
static_assert(offsetof(EditElement, _blinkOn)          == 0xe8, "");
static_assert(offsetof(EditElement, _blinkAccumulator) == 0xf0, "");
// The class and the RtUI POD describe the same bytes.
static_assert(sizeof(EditElement) == sizeof(RtUI::EditElement), "");
static_assert(offsetof(EditElement, _text)    == offsetof(RtUI::EditElement, _text), "");
static_assert(offsetof(EditElement, _active)  == offsetof(RtUI::EditElement, _flagE0), "");
static_assert(offsetof(EditElement, _caret)   == offsetof(RtUI::EditElement, _caret), "");
static_assert(offsetof(EditElement, _blinkAccumulator)
              == offsetof(RtUI::EditElement, _blinkAccumulator), "");

}  // namespace UI
