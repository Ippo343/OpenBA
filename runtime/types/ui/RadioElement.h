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
//   [0] 0x180047d50  (unexported) vector deleting dtor — RT_DELDTOR row, kept
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
// ABSTRACT (slot 0 `= 0`); no real ctor.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "Element.h"
#include "GroupElement.h"
#include "../collection/Collection.h"

namespace System { class Time; }

namespace UI {

class RadioElement : public GroupElement {
public:
    // ---- the slots it owns ------------------------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180047d50
    const char* TypeName() const override;                        // [1] 0x180047a00
    uint32_t    Type() const override;                            // [2] 0x1800479e0
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
    static uint32_t TypeId();                                     // 0x1800479f0  not ported
    static Object::Base* CreateNewObject();                       // 0x180047af0  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180047ba0  factory door

protected:
    // NOT retail's `??0RadioElement@UI@@QEAA@XZ` (0x1800d5870, a door).
    RadioElement() = default;
    // `??1RadioElement@UI@@UEAA@XZ` (0x1800d5820). NON-VIRTUAL, NOT defined in
    // the real build; the retail body is the door `RtUi_RadioElement_Dtor`.
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
    static const char* _TypeName();                               // 0x1800479c0 -> "RadioElement"
    static const char* _ParentTypeName();                         // 0x1800479d0 -> "GroupElement"
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    Collection::Array<Element*> _checkBoxes;   // +0xC0
    int32_t  _activeIndex;                     // +0xD0  ctor -1
    uint32_t _padD4;                           // +0xD4  written by nothing
};

static_assert(sizeof(RadioElement) == 0xd8,
              "UI::RadioElement is 0xd8 bytes (MOV EDX,0xd8 in 0x180047d50)");
static_assert(alignof(RadioElement) == 8, "UI::RadioElement aligns to 8");
static_assert(!std::has_virtual_destructor<RadioElement>::value, "ENDGAME decision 2");
static_assert(offsetof(RadioElement, _checkBoxes)  == sizeof(GroupElement),
              "RadioElement's own fields begin at the end of GroupElement");
static_assert(offsetof(RadioElement, _checkBoxes)  == 0xC0, "RadioElement::_checkBoxes @ +0xC0");
static_assert(offsetof(RadioElement, _activeIndex) == 0xD0, "RadioElement::_activeIndex @ +0xD0");

} // namespace UI
