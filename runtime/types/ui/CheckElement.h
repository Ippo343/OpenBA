#pragma once
// ============================================================================
// runtime/types/ui/CheckElement.h — UI::CheckElement, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout MOVED here from the file-local
// constants of runtime/src/ui/checkelement.cpp (RULE B6-8).
//
// PARENT `UI::ButtonElement` — READ (B36_SLOTS.md section 3), an INTERMEDIATE
// leaf (RULE B6-9: ButtonElement converts first):
//     ?_ParentTypeName@CheckElement@UI@@CAPEBDXZ  @0x180048a50 -> "ButtonElement"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@CheckElement@UI@@…`
// 0x1801fa9a8 stays the minted record slots 1/2 read):
//     ?_TypeName@CheckElement@UI@@CAPEBDXZ        @0x180048a40 -> "CheckElement"
//     ?_ParentTypeName@CheckElement@UI@@CAPEBDXZ  @0x180048a50 -> "ButtonElement"
//
// `??_7CheckElement@UI@@6B@` @ 0x180157260, TWELVE slots (sizeof from
// `MOV EDX,0xe0` in slot 0 0x180048d80):
//
//   [ 0] 0x180048d80  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [ 1] 0x180048a80  TypeName
//   [ 2] 0x180048a60  Type
//   [ 3] 0x180116870  Element::Persist         INHERITED
//   [ 4] 0x180111740  ComputeSize
//   [ 5] 0x1801117b0  ComputeLayout
//   [ 6] 0x180111800  Draw
//   [ 7] 0x180111870  Update
//   [ 8] 0x180110cc0  ButtonElement::Enable    INHERITED
//   [ 9] 0x1801169e0  Element::Contains        INHERITED
//   [10] 0x180035620  Element::SetColor        INHERITED
//   [11] 0x180035630  Element::GetBorder       INHERITED (RETSLOT)
//
// (Until ENDGAME B.3.9:) ABSTRACT, no real ctor (the `??0CheckElement` door, and
// CheckDescription::CreateInstance's inlined copy, build live ones wearing the
// MINTED table).
//
// LAYOUT — 0xe0: ButtonElement's 0xd8, then ONE BYTE, the check state.
// `??0CheckElement@UI@@QEAA@XZ` (0x1801116a0) inlines ButtonElement's ctor
// (stamp ??_7ButtonElement, +0xc0 = -1, +0xc4 = 0 — B36_SLOTS.md section 7's
// stamp chain), stamps its own table and clears +0xd8 with a BYTE store — the
// same `MOV byte [..+0xd8],0` the two inlined copies carry
// (`?CreateInPlaceObject@CheckElement@` 0x180048bff, factory_ui1_tables.inc;
// CheckDescription::CreateInstance 0x180111a60). It is the only store past
// 0xd8: +0xd9..+0xdf are written by nothing (tail padding to alignment 8).
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0CheckElement@UI@@QEAA@XZ` (0x1801116a0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~CheckElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7CheckElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonElement.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class CheckElement : public ButtonElement {
public:
    // ??0CheckElement@UI@@QEAA@XZ 0x1801116a0 — the REAL constructor: retail
    // INLINES ButtonElement's body (Element ctor, base stamp, -1, 0) — the
    // same stores ButtonElement() makes — then its own stamp and +0xd8 BYTE 0.
    CheckElement() : ButtonElement() {
        _checked = 0;
    }

    // ---- the twelve slots, in measured order ------------------------------
    // [0] 0x180048d80 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xe0`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<CheckElement*>(self + static_cast<int64_t>(i) * 0xe0)
                    ->~CheckElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~CheckElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180048a80, Type 0x180048a60, TypeId 0x180048a70, _TypeName 0x180048a40
    //         _ParentTypeName 0x180048a50, CreateNewObject 0x180048b70, CreateInPlaceObject 0x180048c20
    DECLARE_TYPE(CheckElement)
public:
    void ComputeSize() override;                                  // [4] 0x180111740
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x1801117b0
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x180111800
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x180111870
    // [8] Enable — ButtonElement's, inherited.

    // ---- exported, NOT PORTED anywhere in the tree: declared, deliberately
    // undefined (nothing in the tree calls them through a member).
    void EnableCheck(bool check);                                 // 0x180048c90
    bool IsChecked() const;                                       // 0x180048c80

    CheckElement(const CheckElement& src);                        // 0x180048ca0  not ported (door retired: no caller)
    CheckElement& operator=(const CheckElement& src);             // 0x180048cf0  not ported


protected:
    // `??1CheckElement@UI@@UEAA@XZ` (0x180111700). NON-VIRTUAL and EMPTY
    // (checkelement.cpp): two restamps, then ~Element — the compiler's chain
    // through ~ButtonElement.
    ~CheckElement();

private:

public:
    uint8_t _checked;          // +0xd8  ctor 0 (BYTE store in 0x1801116a0). Toggled by
                               //        Update; Draw shows `_children[5]` (the tick)
                               //        only while it is non-zero.
    uint8_t _padD9[7];         // +0xd9  written by nothing
};

static_assert(sizeof(CheckElement) == 0xe0,
              "UI::CheckElement is 0xe0 bytes (MOV EDX,0xe0 in 0x180048d80)");
static_assert(alignof(CheckElement) == 8, "UI::CheckElement aligns to 8");
static_assert(offsetof(CheckElement, _checked) == sizeof(ButtonElement),
              "CheckElement's byte begins at the end of ButtonElement");
static_assert(offsetof(CheckElement, _checked) == 0xd8, "");
static_assert(!std::has_virtual_destructor<CheckElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<CheckElement>::value, "B39 D1: concrete");

}  // namespace UI
