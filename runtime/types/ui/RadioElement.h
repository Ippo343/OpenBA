#pragma once
// ============================================================================
// runtime/types/ui/RadioElement.h — UI::RadioElement, the REAL C++ class
// ============================================================================
// Wave 7 lane D wrote the layout; ENDGAME B.3 step 6 block C2 turned the POD
// (`RtUI::Element base;` + two fields) into the class, deriving from its
// PROVED parent. Nothing embedded the POD, so no `RtUI::` copy survives; the
// two field names are unchanged, so widgets_input.cpp's three bodies (fragment
// pass) compile untouched through `using UI::RadioElement;`.
//
// PARENT `UI::GroupElement` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_ParentTypeName@RadioElement@UI@@CAPEBDXZ  @0x1800479d0 -> "GroupElement"
// RULE B5-15 / B6-7 — the two literals, RECORDED, NOT USED:
//     ?_TypeName@RadioElement@UI@@CAPEBDXZ        @0x1800479c0 -> "RadioElement"
//     ?_ParentTypeName@RadioElement@UI@@CAPEBDXZ  @0x1800479d0 -> "GroupElement"
//
// `??_7RadioElement@UI@@6B@` @ 0x1801573c0, TWELVE slots; it overrides ONLY
// Update among GroupElement's 4..11:
//   [0] 0x180047d50  (unexported) vector deleting dtor — the class's own (B.3.9)
//   [1] 0x180047a00  TypeName
//   [2] 0x1800479e0  Type
//   [4..6]           GroupElement's (0x180117f00 / 0x180118050 / 0x1801181c0)
//   [7] 0x1800d5580  Update
//   [3], [8..11]     Element's
//
// LAYOUT — 0xd8 bytes (`MOV EDX,0xd8` in the deleting dtor 0x180047d50 and in
// RadioDescription::CreateInstance's allocation at 0x1800d5916).
// `??0RadioElement@UI@@QEAA@XZ` (0x1800d5870) — and its inlined copy at
// 0x1800d5962..0x1800d5970 — store a qword 0 at +0xc0, a qword 0 at +0xc8 and a
// DWORD -1 at +0xd0; +0xd4 is written by nothing.
//   +0xC0  _checkBoxes  Collection::Array<UI::Element*>  data+0xC0/size+0xC8/cap+0xCC
//          (`?FindCheckBoxes@` 0x1800d5750 grows it; `~RadioElement` 0x1800d5820
//          destroys it through 0x1800490e0)
//   +0xD0  _activeIndex int32, ctor -1 (`?GetActiveRadio@` 0x1800d56e0 is
//          `MOV EAX,[RCX+0xd0] ; RET`; `?SetActiveRadio@` 0x1800d56f0 writes it)
//   +0xD4  _padD4       written by nothing
//
// The per-checkbox byte SetActiveRadio / Update clear (`checkbox+0xd8`) belongs
// to the CHECKBOX class, not to RadioElement.
//
// (Until ENDGAME B.3.9:) ABSTRACT (slot 0 `= 0`); no real ctor.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0RadioElement@UI@@QEAA@XZ` (0x1800d5870) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~RadioElement is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7RadioElement@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"
#include "GroupElement.h"
#include "../collection/Collection.h"
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace System { class Time; }

namespace UI {

class RadioElement : public GroupElement {
public:
    // ??0RadioElement@UI@@QEAA@XZ 0x1800d5870 — the REAL constructor: ??0Element,
    // the GroupElement stamp (GroupElement()), its own stamp, the check-box
    // array's two QWORD zeros (Collection::Array's own default ctor) and
    // `MOV dword [+0xd0],-1`. +0xd4 NOT written.
    RadioElement() : GroupElement() {
        _activeIndex = -1;
    }

    // ---- the slots it owns ------------------------------------------------
    // [0] 0x180047d50 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xd8`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<RadioElement*>(self + static_cast<int64_t>(i) * 0xd8)
                    ->~RadioElement();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~RadioElement();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180047a00, Type 0x1800479e0, TypeId 0x1800479f0, _TypeName 0x1800479c0
    //         _ParentTypeName 0x1800479d0, CreateNewObject 0x180047af0, CreateInPlaceObject 0x180047ba0
    DECLARE_TYPE(RadioElement)
public:
    // [4..6] GroupElement's, inherited.
    // [7] `InternalPtr<DialogInstance>` arrives as a POINTER to the caller's
    // temporary (RULE B6-11; see Element.h's banner).
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;               // [7] 0x1800d5580
    // [3], [8..11] Element's, inherited.

    // ---- non-virtual ------------------------------------------------------
    // 0x1800d56e0 — `MOV EAX,[RCX+0xd0] ; RET`.
    int32_t GetActiveRadio() const { return _activeIndex; }
    // PORTED; both DEFINED in widgets_input.cpp (fragment pass), where the
    // registered doors `RtUi_RadioElement_SetActiveRadio` (0x1800d56f0) and
    // `RtUi_RadioElement_FindCheckBoxes` (0x180047c00) delegate to them.
    void SetActiveRadio(int32_t index);                           // 0x1800d56f0
    void FindCheckBoxes();                                        // 0x180047c00

    // ---- exported, NOT PORTED as members ----------------------------------
    RadioElement(const RadioElement& src);                        // 0x180047c20  not ported
    RadioElement& operator=(const RadioElement& src);             // 0x180047c90  not ported

protected:
    // `??1RadioElement@UI@@UEAA@XZ` (0x1800d5820). NON-VIRTUAL and EMPTY
    // (radioelement.cpp): the check-box array's J1 ~Array (retail 0x1800490e0)
    // is the compiler's member ~Array; the restamps and ~Element are the
    // compiler's chain through ~GroupElement.
    ~RadioElement();

public:
    // `?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z`
    // (0x1800d5750, `AEAA` = PRIVATE). The handle is by value in the source, so
    // retail passes the ADDRESS of the caller's copy (RULE B6-11) — declared as
    // that pointer. PUBLIC here only because its registered door
    // `RtUi_RadioElement_FindCheckBoxesPrivate` delegates to it (fragment pass
    // F1; access is not part of the GCC ABI). Body: widgets_input.cpp.
    void FindCheckBoxes(Object::InternalPtr<Element>* root);      // 0x1800d5750

private:

public:
    Collection::Array<Element*> _checkBoxes;   // +0xC0
    int32_t  _activeIndex;                     // +0xD0  ctor -1
    uint32_t _padD4;                           // +0xD4  written by nothing
};

static_assert(sizeof(RadioElement) == 0xd8,
              "UI::RadioElement is 0xd8 bytes (MOV EDX,0xd8 in 0x180047d50)");
static_assert(alignof(RadioElement) == 8, "UI::RadioElement aligns to 8");
static_assert(!std::has_virtual_destructor<RadioElement>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<RadioElement>::value, "B39 D1: concrete");
static_assert(offsetof(RadioElement, _checkBoxes)  == sizeof(GroupElement),
              "RadioElement's own fields begin at the end of GroupElement");
static_assert(offsetof(RadioElement, _checkBoxes)  == 0xC0, "RadioElement::_checkBoxes @ +0xC0");
static_assert(offsetof(RadioElement, _activeIndex) == 0xD0, "RadioElement::_activeIndex @ +0xD0");

} // namespace UI
