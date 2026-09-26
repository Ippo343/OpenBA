#pragma once
// ============================================================================
// runtime/types/ui/NumberElement.h — UI::NumberElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. NumberElement is a SIBLING of NumberEditElement, not related
// to it — it is a read-only numeric display deriving from `UI::Element`
// directly (`?_ParentTypeName@NumberElement@UI@@` returns "Element" and the
// ctor calls `??0Element`; the old "extends TextElement" here was wrong, block
// C3), not from EditElement, proven by its own SetValue pair
// (0x180118d50 int / 0x180118db0 float), which never touch +0xC8/+0xE0/+0xF8
// the way EditElement's family does. It stores a TAGGED UNION of an int and a
// float value plus a discriminant:
//
//   +0xC8  _mode       int32   0 == _intValue is the live value, 1 == _floatValue
//   +0xD8  _intValue   int32   ?SetValue@NumberElement@UI@@QEAAXH@Z (0x180118d50)
//   +0xDC  _floatValue float   ?SetValue@NumberElement@UI@@QEAAXM@Z (0x180118db0)
//
// Both setters write BOTH fields on every real change (the "other" one is
// zeroed, not left stale) and flip _mode; both early-out when the incoming
// value equals the field already holding it AND _mode already agrees — see
// widgets_input.cpp for the exact guard, preserved byte-for-byte.
//
// The gap +0xC0..+0xC7 (TextElement's own leading field, e.g. its display
// System::String) and +0xCC..+0xD7 are not reversed by this lane.
//
// *** +0xD0 IS NOT PADDING. Wave 9 lane M, MEASURED. ***
// `_pad_cc_d8` covers +0xCC..+0xD7, and the eight bytes at +0xD0 inside it hold
// an OWNING, REFCOUNTED System::String data pointer — a live heap address with
// a {length, refcount} header at [-8]. Measured by probing every divergence of
// UI::NumberElement::SetValueInt on the tutorial routes: after the call, that
// qword points at a freshly allocated block whose header reads len/rc and whose
// payload is the formatted number ("4" for value 4, "272" for 272, and so on
// for all ten observed calls). See docs/runtime_port/WAVE9_LANE_M.md Finding 1.
//
// THREE HEADERS IN THIS DIRECTORY DISAGREE ABOUT THESE BYTES, and that
// disagreement is its own defect, recorded here rather than silently resolved:
//   - THIS header             : +0xCC..+0xD7 unreversed padding
//   - TextElement.h           : +0xC8 Array<System::String>.data, +0xD0 its
//                               int32 `size`, +0xD4 its int32 `capacity`
//                               (derived from call sites at 0x18011e9b0)
//   - LabelElement.h          : an older "TextElement base (0xC0..0xFF)" table
//                               that TextElement.h already says was never
//                               cross-checked
//
// *** THE DISAGREEMENT IS RESOLVED. Wave 9 lane N, from the DISASSEMBLY. ***
// It was never a contradiction: **UI::NumberElement is NOT a UI::TextElement.**
// Its default ctor (0x180118C70) calls the UI::Element base ctor at
// 0x180115E50 — not TextElement's — and then writes its own fields starting at
// +0xC0; and it has its OWN private CacheText at 0x1801189F0, distinct from
// TextElement::CacheText at 0x18011EC10. So TextElement.h's call-site-derived
// table and this header's owning pointer are statements about TWO DIFFERENT
// CLASSES that happen to share an offset, and both are right.
// **TextElement.h is deliberately NOT edited**, for exactly the reason lane M
// gives above: a written-extent fact belongs to a WRITER, not to an offset
// (reference_never_written_needs_by_whom.md), and one function's behaviour on a
// NumberElement is no authority over TextElement's call sites.
//
// The layout below is now complete and static-evidenced. Lane M's warning to
// any lane that MINTS one of these still stands and is now answerable: the
// object is 0xE8 bytes and its dtor releases exactly one thing, +0xD0.
//
//   +0xC0  Font* _font        ComputeSize (0x180119000) loads it into RCX for
//                             the text-measure call 0x180116C70 and then reads
//                             its +0x50/+0x54; Draw (0x1801191E0) does the same.
//   +0xC8  int32 _mode        GetNumberType (0x18004A9C0) is
//                             `mov eax,[rcx+0xc8]; ret`.
//   +0xD0  char* _cachedText  the owning refcounted System::String lane M
//                             measured. CacheText (0x1801189F0) assigns into it
//                             with retail's inlined String::operator= — release
//                             the old (`dec [p-4]`; on zero `p -= 8`, MemFree),
//                             store the new, `inc [p-4]`. The DTOR
//                             (0x180118CE0) runs that same release on it AND
//                             NOTHING ELSE, which is the second, independent
//                             proof of ownership. ComputeSize reads the LENGTH
//                             at [p-8]; Draw passes `&this->_cachedText` as a
//                             System::String& (`lea r8,[rbx+0xd0]`).
//                             Block layout: runtime/types/system/String.h.
//   +0xD8  int32 _intValue    GetIntValue   (0x18004A9A0).
//   +0xDC  float _floatValue  GetFloatValue (0x18004A9B0).
//   +0xE0  bool  _showSign    CacheText's format-string select:
//                             _showSign ? "%+i" : "%i" (mode 0),
//                             _showSign ? "%+.2f" : "%.2f" (mode 1) — the four
//                             literals are at 0x180159144 / 0x180154848 /
//                             0x180159148 / 0x180159150. Written as a BYTE by
//                             the ctor (`mov [rbx+0xe0],al`).
//   +0xE1..+0xE7             tail padding to the aligned object size.
//
// SIZE 0xE8, from CreateNewObject (0x18004A910), which calls
// MemoryAllocator::Allocate(_singleton, **0xE8**, 8) and then the default ctor.
// The old struct was 0xE0 — EIGHT BYTES SHORT — and that was a second harness
// defect on top of Finding 1: the oracle's stack `replay` object was 0xE0 bytes
// while the DLL's CacheText READS +0xE0 on it, so the replay was choosing its
// format string from a garbage stack byte. That replay is gone now (both
// setters are NO-ORACLE ports, widgets_input.cpp), so the out-of-bounds read
// went with it; the size is recorded because it is the object's true size.
// ============================================================================
#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "NumberDescription.h"   // NumberDescription::NumberType

// ENDGAME B.3 step 6, block C3: the POD below moved from `UI::` to `RtUI::`
// (B4b-1) when the real class took the retail name; widgets_input.cpp's two
// NumberElement setters still reach the object through it.
namespace RtUI {

// The STRUCT now says what the PROSE above already said. Until the elem_B
// batch ported the class (runtime/src/ui/numberelement.cpp, 2026-09-12) it
// stopped at +0xE0 with two anonymous pads and no size assert, i.e. it was a
// weaker statement than its own banner — `_font`, `_cachedText` and
// `_showSign` were described in the comment and absent from the type. They are
// fields now, and the size is asserted, because the ctor (0x180118C70), the
// dtor (0x180118CE0), ComputeSize (0x180119000), Draw (0x1801191E0) and
// CacheText (0x1801189F0) were all transcribed against them.
struct NumberElement {
    RtUI::Element  base;         // +0x00 .. +0xBF
    void*    _font;              // +0xC0  ExternalPtr<UI::Font>
    int32_t  _mode;              // +0xC8
    uint32_t _padCC;             // +0xCC  never written by the ctor
    char*    _cachedText;        // +0xD0  owning, refcounted System::String
    int32_t  _intValue;          // +0xD8
    float    _floatValue;        // +0xDC
    uint8_t  _showSign;          // +0xE0
    uint8_t  _padE1[0x07];       // +0xE1 .. +0xE7  tail padding
};

static_assert(sizeof(NumberElement) == 0xE8, "CreateNewObject 0x18004A910 allocates 0xE8");
static_assert(offsetof(NumberElement, base)       == 0x00, "NumberElement::base @ +0x00");
static_assert(offsetof(NumberElement, _font)      == 0xC0, "NumberElement::_font @ +0xC0");
static_assert(offsetof(NumberElement, _mode)      == 0xC8, "NumberElement::_mode @ +0xC8");
static_assert(offsetof(NumberElement, _cachedText)== 0xD0, "NumberElement::_cachedText @ +0xD0");
static_assert(offsetof(NumberElement, _intValue)  == 0xD8, "NumberElement::_intValue @ +0xD8");
static_assert(offsetof(NumberElement, _floatValue)== 0xDC, "NumberElement::_floatValue @ +0xDC");
static_assert(offsetof(NumberElement, _showSign)  == 0xE0, "NumberElement::_showSign @ +0xE0");

}  // namespace RtUI

// ############################################################################
// ENDGAME B.3 step 6 (block C3) — `UI::NumberElement`, the REAL C++ class
// ############################################################################
//
// PARENT `UI::Element` — READ (B36_SLOTS.md section 3), RECORDED, NOT USED:
//     ?_TypeName@NumberElement@UI@@CAPEBDXZ       @0x18004a7e0 -> "NumberElement"
//     ?_ParentTypeName@NumberElement@UI@@CAPEBDXZ @0x18004a7f0 -> "Element"
// (NOT TextElement — the banner above's own first line said otherwise; the
// literal and the ctor's `CALL ??0Element` agree.)
//
// `??_7NumberElement@UI@@6B@` @ 0x1801574d0, TWELVE slots: [0] 0x18004ab80 ·
// [1] 0x18004a820 TypeName · [2] 0x18004a800 Type · [4] 0x180119000
// ComputeSize · [5] 0x180119080 ComputeLayout · [6] 0x1801191e0 Draw · the
// rest Element's.
//
// SIZE 0xE8: `MOV EDX,0xe8`-sized per B36_SLOTS.md section 1 and
// CreateNewObject's `Allocate(0xE8, 8)`.
//
// ABSTRACT in C++; no real ctor: live ones are built by the `??0NumberElement`
// door (numberelement.cpp) and wear the MINTED table.
// ############################################################################
namespace UI {

class Font;

class NumberElement : public Element {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x18004ab80
    const char* TypeName() const override;                       // [1] 0x18004a820
    uint32_t    Type() const override;                           // [2] 0x18004a800
    void ComputeSize() override;                                 // [4] 0x180119000
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;           // [5] 0x180119080
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1801191e0

    // ?CacheText@NumberElement@UI@@AEAAXXZ  0x1801189f0 — `AEAA` PRIVATE in
    // retail; PUBLIC here because its door and the ctor door call it.
    void CacheText();

    // The three one-instruction getters, whole bodies (banner above).
    int32_t GetIntValue() const   { return _intValue; }          // 0x18004a9a0
    float   GetFloatValue() const { return _floatValue; }        // 0x18004a9b0
    NumberDescription::NumberType GetNumberType() const {        // 0x18004a9c0
        return static_cast<NumberDescription::NumberType>(_mode);
    }
    // ?SetValue@NumberElement@UI@@QEAAXH@Z / QEAAXM@Z — a non-virtual overload
    // pair (THE DOOR DECIDES). Both DEFINED in widgets_input.cpp (fragment pass).
    void SetValue(int32_t value);                                // 0x180118d50
    void SetValue(float value);                                  // 0x180118db0

    NumberElement(const NumberElement& src);                     // 0x18004a9d0  not ported
    NumberElement& operator=(const NumberElement& src);          // 0x18004aa80  not ported
    static uint32_t TypeId();                                    // 0x18004a810  not ported
    static Object::Base* CreateNewObject();                      // 0x18004a910  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x18004a990  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x18004a830  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x18004a8a0  not ported
    static void  operator delete(void* p);                       // 0x18004a8b0  not ported

protected:
    NumberElement() = default;   // NOT retail's 0x180118c70 (numberelement.cpp door)
    ~NumberElement();            // 0x180118ce0 — empty; the door keeps the body

private:
    static const char* _TypeName();                              // 0x18004a7e0 -> "NumberElement"
    static const char* _ParentTypeName();                        // 0x18004a7f0 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801facb8 (minted)

public:
    Font*    _font;          // +0xc0  ExternalPtr<UI::Font>
    int32_t  _mode;          // +0xc8  0 int, 1 float (GetNumberType)
    uint32_t _padCC;         // +0xcc  never written by the ctor
    char*    _cachedText;    // +0xd0  owning, refcounted System::String
    int32_t  _intValue;      // +0xd8
    float    _floatValue;    // +0xdc
    uint8_t  _showSign;      // +0xe0
    uint8_t  _padE1[7];      // +0xe1  tail padding
};

static_assert(sizeof(NumberElement) == 0xe8, "UI::NumberElement is 0xe8");
static_assert(alignof(NumberElement) == 8, "UI::NumberElement aligns to 8");
static_assert(offsetof(NumberElement, _font) == sizeof(Element),
              "NumberElement's own fields begin at the end of Element");
static_assert(offsetof(NumberElement, _font)       == 0xc0, "");
static_assert(offsetof(NumberElement, _mode)       == 0xc8, "");
static_assert(offsetof(NumberElement, _cachedText) == 0xd0, "");
static_assert(offsetof(NumberElement, _intValue)   == 0xd8, "");
static_assert(offsetof(NumberElement, _floatValue) == 0xdc, "");
static_assert(offsetof(NumberElement, _showSign)   == 0xe0, "");
static_assert(sizeof(NumberElement) == sizeof(RtUI::NumberElement), "");
static_assert(offsetof(NumberElement, _cachedText) == offsetof(RtUI::NumberElement, _cachedText), "");
static_assert(offsetof(NumberElement, _showSign)   == offsetof(RtUI::NumberElement, _showSign), "");

}  // namespace UI
