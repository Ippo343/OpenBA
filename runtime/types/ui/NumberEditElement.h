#pragma once
// ============================================================================
// runtime/types/ui/NumberEditElement.h — UI::NumberEditElement layout
// ============================================================================
// Wave 7 lane D. NumberEditElement extends EditElement with exactly two proven
// fields:
//
//   +0xF8  _min  int32   ?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z (0x1800d4b70) arg 1
//                        ?GetMinValue@ 0x180046440 = MOV EAX,[RCX+0xf8]
//   +0xFC  _max  int32   ?SetMinMax@ arg 2; ?GetMaxValue@ 0x180046430 = MOV EAX,[RCX+0xfc]
//
// ENDGAME B.3 step 6, block C3: this header used to pad from a compiler-sized
// `sizeof(EditElement) == 0xE8` up to +0xF8. Retail's EditElement is 0xF8
// (EditElement.h), so `_min` begins EXACTLY at the end of EditElement and the
// pad is gone. Size 0x100 (`MOV EDX,0x100` in the deleting dtor 0x180046540).
// The POD moved to `RtUI::NumberEditElement` (B4b-1) for widgets_input.cpp.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "EditElement.h"

namespace RtUI {

struct NumberEditElement {
    EditElement base;          // +0x00 .. +0xF7  (RtUI::EditElement)
    int32_t     _min;          // +0xF8
    int32_t     _max;          // +0xFC
};

static_assert(sizeof(EditElement) == 0xF8, "EditElement is 0xF8 (retail, EditElement.h)");
static_assert(offsetof(NumberEditElement, base) == 0x00, "NumberEditElement::base @ +0x00");
static_assert(offsetof(NumberEditElement, _min) == 0xF8, "NumberEditElement::_min @ +0xF8");
static_assert(offsetof(NumberEditElement, _max) == 0xFC, "NumberEditElement::_max @ +0xFC");
static_assert(sizeof(NumberEditElement) == 0x100, "NumberEditElement is 0x100");

}  // namespace RtUI

// ############################################################################
// ENDGAME B.3 step 6 (block C3) — `UI::NumberEditElement`, the REAL C++ class
// ############################################################################
//
// PARENT `UI::EditElement` — READ (B36_SLOTS.md section 3), RECORDED, NOT USED:
//     ?_TypeName@NumberEditElement@UI@@CAPEBDXZ       @0x180046210 -> "NumberEditElement"
//     ?_ParentTypeName@NumberEditElement@UI@@CAPEBDXZ @0x180046220 -> "EditElement"
//
// `??_7NumberEditElement@UI@@6B@` @ 0x180156f70, THIRTEEN slots: [0] 0x180046540
// · [1] 0x180046250 TypeName · [2] 0x180046230 Type · [3..11] INHERITED
// (4..7 are EditElement's own bodies) · [12] 0x1800d4a60 FinalizeText — the
// override.
//
// ABSTRACT in C++; no real ctor: live ones are built by the `??0NumberEditElement`
// door (numbereditelement.cpp) or inline by NumberEditDescription::CreateInstance.
// ############################################################################
namespace UI {

class NumberEditElement : public EditElement {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x180046540
    const char* TypeName() const override;                       // [1] 0x180046250
    uint32_t    Type() const override;                           // [2] 0x180046230
    void FinalizeText() override;                                // [12] 0x1800d4a60

    // ---- non-virtual ---------------------------------------------------------
    int32_t GetMaxValue() const { return _max; }                 // 0x180046430
    int32_t GetMinValue() const { return _min; }                 // 0x180046440
    // The three bodies below are widgets_input.cpp's doors (fragment pass);
    // declared, left undefined.
    int32_t GetValue() const;                                    // 0x1800d4b50
    void    SetMinMax(int32_t minValue, int32_t maxValue);       // 0x1800d4b70
    void    SetValue(int32_t value);                             // 0x1800d4bb0

    NumberEditElement(const NumberEditElement& src);             // 0x180046450  not ported
    NumberEditElement& operator=(const NumberEditElement& src);  // 0x1800464b0  not ported
    static uint32_t TypeId();                                    // 0x180046240  not ported
    static Object::Base* CreateNewObject();                      // 0x180046340  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x1800463e0  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x180046260  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x1800462d0  not ported
    static void  operator delete(void* p);                       // 0x1800462e0  not ported

protected:
    NumberEditElement() = default;   // NOT retail's 0x1800d4c70 (the door)
    ~NumberEditElement();            // 0x1800d4c40 — empty; the door keeps the body

private:
    static const char* _TypeName();                              // 0x180046210 -> "NumberEditElement"
    static const char* _ParentTypeName();                        // 0x180046220 -> "EditElement"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801fa190 (minted)

public:
    int32_t _min;   // +0xf8  ctor 0xffff8001 (-32767)
    int32_t _max;   // +0xfc  ctor 0x8000
};

static_assert(sizeof(NumberEditElement) == 0x100,
              "UI::NumberEditElement is 0x100 (MOV EDX,0x100 in 0x180046540)");
static_assert(alignof(NumberEditElement) == 8, "UI::NumberEditElement aligns to 8");
static_assert(offsetof(NumberEditElement, _min) == sizeof(EditElement),
              "_min begins exactly at the end of the 0xf8-byte EditElement");
static_assert(offsetof(NumberEditElement, _min) == 0xf8, "");
static_assert(offsetof(NumberEditElement, _max) == 0xfc, "");
static_assert(sizeof(NumberEditElement) == sizeof(RtUI::NumberEditElement), "");
static_assert(offsetof(NumberEditElement, _min) == offsetof(RtUI::NumberEditElement, _min), "");
static_assert(offsetof(NumberEditElement, _max) == offsetof(RtUI::NumberEditElement, _max), "");

}  // namespace UI
