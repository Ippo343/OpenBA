#pragma once
// ============================================================================
// runtime/types/ui/BorderElement.h — UI::BorderElement, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout MOVED here from the file-local
// constants of runtime/src/ui/borderelement.cpp (RULE B6-8).
//
// PARENT `UI::Element` — READ (B36_SLOTS.md section 3):
//     ?_ParentTypeName@BorderElement@UI@@CAPEBDXZ  @0x180041080 -> "Element"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@BorderElement@UI@@…`
// 0x1801fa8c8 stays the minted record):
//     ?_TypeName@BorderElement@UI@@CAPEBDXZ        @0x180041070 -> "BorderElement"
//     ?_ParentTypeName@BorderElement@UI@@CAPEBDXZ  @0x180041080 -> "Element"
//
// `??_7BorderElement@UI@@6B@` @ 0x180156920, TWELVE slots (sizeof from
// `MOV EDX,0x150` in slot 0 0x1800414f0):
//
//   [ 0] 0x1800414f0  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [ 1] 0x1800410b0  TypeName
//   [ 2] 0x180041090  Type
//   [ 3] 0x180116870  Element::Persist     INHERITED
//   [ 4] 0x1801101d0  ComputeSize
//   [ 5] 0x180110360  ComputeLayout
//   [ 6] 0x1801103e0  Draw
//   [ 7] 0x180116270  Element::Update      INHERITED
//   [ 8] 0x180116910  Element::Enable      INHERITED
//   [ 9] 0x1801169e0  Element::Contains    INHERITED
//   [10] 0x180035620  Element::SetColor    INHERITED
//   [11] 0x180035630  Element::GetBorder   INHERITED (RETSLOT)
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0BorderElement@UI@@QEAA@XZ` (0x18010fd40) minus its vptr
// store; slot 0 is the class's own vector deleting dtor; ~BorderElement is
// protected and non-virtual. BorderDescription::CreateInstance and the class's
// own factory pair (borderelement.cpp) build it; the minted
// `??_7BorderElement@UI@@6B@`, its RT_DELDTOR row, the FACTUI1 row and the
// doors are retired.
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x150 bytes = UI::Element's 0xC0 + NINE UI::Sprite values
// ---------------------------------------------------------------------------
// `??0BorderElement@UI@@QEAA@XZ` (0x18010fd40) writes each of the nine as
// {sheet = qword 0, index = dword -1} (the eighteen rows of kBorderElement in
// runtime/src/ui/factory_ui1_tables.inc, srcVa 0x18010fd68..0x18010fdf7), so
// the count is enumerated, not inferred from the size. Each Sprite's pad dword
// (+0x0c) is written by NOTHING in the ctor; the copy ctor (0x180041250)
// copies all eighteen qwords.
//
// THE NINE ARE A 3x3 NINE-SLICE, ROW MAJOR — read out of Draw (0x1801103e0):
// the corner sprites are the ones drawn with the point-form AddSprite at the
// four extreme coordinates, and the edges are the ones stretched between them.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"
#include "Sprite.h"      // ::Sprite (block D folds it into UI::Sprite)
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class BorderElement : public Element {
public:
    // ??0BorderElement@UI@@QEAA@XZ 0x18010fd40 — the REAL constructor: CALL
    // ??0Element (Element()), then the nine Sprites {sheet 0, index -1}
    // (0x18010fd68..0x18010fdf7) — exactly `UI::Sprite()`'s two stores, so the
    // members' own default ctors are the body. Each pad dword stays unwritten.
    BorderElement() : Element() {}

    // ---- the twelve slots, in measured order ------------------------------
    // [0] 0x1800414f0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x150`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<BorderElement*>(self + static_cast<int64_t>(i) * 0x150)
                    ->~BorderElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~BorderElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    const char* TypeName() const override;                        // [1] 0x1800410b0
    uint32_t    Type() const override;                            // [2] 0x180041090
    void ComputeSize() override;                                  // [4] 0x1801101d0
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x180110360
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1801103e0
    // [3], [7] .. [11] — Element's, inherited.

    BorderElement(const BorderElement& src);                      // 0x180041250  not ported (door retired, no caller)
    BorderElement& operator=(const BorderElement& src);           // 0x180041390  not ported

    static uint32_t TypeId();                                     // 0x1800410a0  borderelement.cpp
    // Retail's factory pair (B39_RULES D2), borderelement.cpp.
    static Object::Base* CreateNewObject();                       // 0x1800411a0
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180041220

protected:
    // `??1BorderElement@UI@@UEAA@XZ` (0x180041230). NON-VIRTUAL. Retail's body
    // is only the tail into ~Element, which the compiler chains.
    ~BorderElement();

private:
    static const char* _TypeName();                               // 0x180041070 -> "BorderElement"
    static const char* _ParentTypeName();                         // 0x180041080 -> "Element"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa8c8 (minted)

public:
    // ---- nine UI::Sprite, each ctor-written {0, -1} (0x18010fd40) ---------
    ::Sprite _topLeft;         // +0x0c0  Draw: point form at (L, T)
    ::Sprite _top;             // +0x0d0  Draw: stretched between the top corners
    ::Sprite _topRight;        // +0x0e0  Draw: point form at (R - w, T)
    ::Sprite _left;            // +0x0f0
    ::Sprite _centre;          // +0x100
    ::Sprite _right;           // +0x110
    ::Sprite _bottomLeft;      // +0x120
    ::Sprite _bottom;          // +0x130
    ::Sprite _bottomRight;     // +0x140  Draw: point form at (R - w, B - h)
};

static_assert(sizeof(BorderElement) == 0x150,
              "UI::BorderElement is 0x150 bytes (MOV EDX,0x150 in 0x1800414f0)");
static_assert(alignof(BorderElement) == 8, "UI::BorderElement aligns to 8");
static_assert(!std::has_virtual_destructor<BorderElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<BorderElement>::value, "B39 D1: concrete");
static_assert(offsetof(BorderElement, _topLeft) == sizeof(Element),
              "BorderElement's own fields begin at the end of UI::Element");
static_assert(offsetof(BorderElement, _topLeft)     == 0x0c0, "");
static_assert(offsetof(BorderElement, _top)         == 0x0d0, "");
static_assert(offsetof(BorderElement, _topRight)    == 0x0e0, "");
static_assert(offsetof(BorderElement, _left)        == 0x0f0, "");
static_assert(offsetof(BorderElement, _centre)      == 0x100, "");
static_assert(offsetof(BorderElement, _right)       == 0x110, "");
static_assert(offsetof(BorderElement, _bottomLeft)  == 0x120, "");
static_assert(offsetof(BorderElement, _bottom)      == 0x130, "");
static_assert(offsetof(BorderElement, _bottomRight) == 0x140, "");

}  // namespace UI
