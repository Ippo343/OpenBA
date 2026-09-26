#pragma once
// ============================================================================
// runtime/types/ui/SliderElement.h — UI::SliderElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D saw only `?SetParameter@SliderElement@UI@@` (0x1800d6ca0),
// which touches nothing of SliderElement's own, and wrote "exactly an Element
// with no extra state". The elem_B batch (2026-09-12) disproved that from
// ??0SliderElement (0x1800D7570), ComputeSize (0x1800D7380), ComputeLayout
// (0x1800D6FC0), Draw (0x1800D6D40) and Update (0x1800D67E0): SliderElement
// has 0x70 bytes of its own state, and its _children[0] IS a real child (the
// optional numeric readout `?UpdateNumber@` refreshes) — so the old "its
// _children is empty" note beside SpinnerElement::SetMinMax describes a slider
// with no readout configured, not the class. `?UpdateNumber@` (0x1800d6c20)
// reads the float at +0x128 (`CVTTSS2SI EDX,[RDI+0x128]` at 0x1800d6c5f,
// `MOVSS XMM1,[RDI+0x128]` at 0x1800d6c76), agreeing on offset and width.
//
// ENDGAME B.3.6 block C5: `UI::SliderElement` is now the REAL class below.
// The Wave 7 POD that occupied the name had NO user left (sliderelement.cpp
// read raw offsets; element_bodies_b5.cpp reads SliderElementOffsets), so it
// is replaced rather than moved to RtUI (B4b-1 moves a POD only to keep its
// users compiling). SliderElementOffsets stays for element_bodies_b5.cpp.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "Sprite.h"

// ############################################################################
// ENDGAME B.3 step 6 (block C5) — `UI::SliderElement`, the REAL C++ class
// ############################################################################
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@SliderElement@UI@@CAPEBDXZ       @0x180049ce0 -> "SliderElement"
//     ?_ParentTypeName@SliderElement@UI@@CAPEBDXZ @0x180049cf0 -> "Element"
//
// `??_7SliderElement@UI@@6B@` @ 0x180157580, TWELVE slots; sizeof 0x130
// (`MOV EDX,0x130` in the slot-0 vector deleting dtor 0x18004a180):
//   [0] 0x18004a180 deleting dtor (RT_DELDTOR row)  [1] 0x180049d20 TypeName
//   [2] 0x180049d00 Type        [3] Element::Persist (inherited)
//   [4] 0x1800d7380 ComputeSize [5] 0x1800d6fc0 ComputeLayout
//   [6] 0x1800d6d40 Draw        [7] 0x1800d67e0 Update — OVERRIDDEN
//   [8..11] Element's (inherited)
//
// ABSTRACT (slot 0 `= 0`), NO real ctor (the `??0SliderElement` door
// 0x1800d7570 builds every live one; it wears the MINTED table).
// ############################################################################
namespace UI {

class SliderElement : public Element {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18004a180
    const char* TypeName() const override;                        // [1] 0x180049d20
    uint32_t    Type() const override;                            // [2] 0x180049d00
    void ComputeSize() override;                                  // [4] 0x1800d7380
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x1800d6fc0
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800d6d40
    // [7] 0x1800d67e0 — the handle arrives as the ADDRESS of the caller's
    // temporary (Element.h's banner), hence the pointer.
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;

    // ---- non-virtual members ----------------------------------------------
    // One instruction each, read as disassembly this block.
    void  SetMinimumValue(float v) { _minimum = v; }   // 0x180049ea0 MOVSS [RCX+0x118],XMM1
    void  SetMaximumValue(float v) { _maximum = v; }   // 0x180049eb0 MOVSS [RCX+0x11c],XMM1
    void  SetCurrentValue(float v) { _value = v; }     // 0x180049ec0 MOVSS [RCX+0x128],XMM1
    float GetCurrentValue() const  { return _value; }  // 0x180049ed0 MOVSS XMM0,[RCX+0x128]
    // Body in widgets_input.cpp (door RtUi_SliderElement_SetParameter).
    void SetParameter(int32_t index, const System::String& text); // 0x1800d6ca0

    SliderElement(const SliderElement& src);                      // 0x180049f00  not ported
    SliderElement& operator=(const SliderElement& src);           // 0x18004a030  not ported
    static uint32_t TypeId();                                     // not ported
    static Object::Base* CreateNewObject();                       // factory door
    static Object::Base* CreateInPlaceObject(void* place);        // factory door

protected:
    SliderElement() = default;   // NOT ??0SliderElement (0x1800d7570, a door)
    ~SliderElement();            // ??1SliderElement 0x180049ee0 — the door; declared only

private:
    // Body in element_bodies_b5.cpp (door RtUi_SliderElement_UpdateNumber);
    // Update reaches it by address until the fragment pass moves it.
    void UpdateNumber();                                          // 0x1800d6c20
    void GetSliderBounds(int32_t& left, int32_t& top,
                         int32_t& right, int32_t& bottom) const;  // 0x1800d7270  not ported

    static const char* _TypeName();                               // 0x180049ce0 -> "SliderElement"
    static const char* _ParentTypeName();                         // 0x180049cf0 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    // Every field a store in the ctor 0x1800d7570 (sliderelement.cpp's door).
    ::Sprite _trackStart;       // +0xC0   the groove, 3-slice
    ::Sprite _trackMiddle;      // +0xD0
    ::Sprite _trackEnd;         // +0xE0
    ::Sprite _thumb;            // +0xF0
    ::Sprite _thumbHot;         // +0x100  drawn instead of _thumb while hovered
    int32_t  _trackOffsetY;     // +0x110  ctor 0; the groove's y offset inside the row
    int32_t  _thumbMargin;      // +0x114  ctor 0; inset at both ends of the travel
    float    _minimum;          // +0x118  ctor 0.0f
    float    _maximum;          // +0x11C  ctor 1.0f
    int32_t  _steps;            // +0x120  ctor 10; a page click moves range/steps
    uint8_t  _hovered;          // +0x124  ctor 0
    uint8_t  _pad125[3];        // +0x125  written by nothing
    float    _value;            // +0x128  ctor 0.25f
    float    _dragAnchor;       // +0x12C  _value at the moment the drag began
};

static_assert(sizeof(SliderElement) == 0x130, "deleting dtor 0x18004a180: MOV EDX,0x130");
static_assert(alignof(SliderElement) == 8, "UI::SliderElement aligns to 8");
static_assert(offsetof(SliderElement, _trackStart)  == sizeof(Element), "");
static_assert(offsetof(SliderElement, _trackStart)  == 0xC0,  "");
static_assert(offsetof(SliderElement, _thumb)       == 0xF0,  "");
static_assert(offsetof(SliderElement, _thumbHot)    == 0x100, "");
static_assert(offsetof(SliderElement, _trackOffsetY)== 0x110, "");
static_assert(offsetof(SliderElement, _thumbMargin) == 0x114, "");
static_assert(offsetof(SliderElement, _minimum)     == 0x118, "");
static_assert(offsetof(SliderElement, _maximum)     == 0x11C, "");
static_assert(offsetof(SliderElement, _steps)       == 0x120, "");
static_assert(offsetof(SliderElement, _hovered)     == 0x124, "");
static_assert(offsetof(SliderElement, _value)       == 0x128, "");
static_assert(offsetof(SliderElement, _dragAnchor)  == 0x12C, "");

// element_bodies_b5.cpp's UpdateNumber reads the value through this constant.
namespace SliderElementOffsets {

constexpr int kValue = 0x128;  // float — the slider's current position.

static_assert(kValue >= (int)sizeof(UI::Element),
              "SliderElement::_value must lie past the end of UI::Element (0xC0)");

} // namespace SliderElementOffsets

static_assert(SliderElementOffsets::kValue == offsetof(SliderElement, _value), "");

} // namespace UI
