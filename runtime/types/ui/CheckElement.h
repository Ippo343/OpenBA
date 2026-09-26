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
//   [ 0] 0x180048d80  (unexported) vector deleting dtor — RT_DELDTOR row, kept
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
// ABSTRACT, no real ctor (the `??0CheckElement` door, and
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
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonElement.h"

namespace UI {

class CheckElement : public ButtonElement {
public:
    // ---- the twelve slots, in measured order ------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180048d80
    const char* TypeName() const override;                        // [1] 0x180048a80
    uint32_t    Type() const override;                            // [2] 0x180048a60
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

    CheckElement(const CheckElement& src);                        // 0x180048ca0  door (checkelement.cpp)
    CheckElement& operator=(const CheckElement& src);             // 0x180048cf0  not ported

    static uint32_t TypeId();                                     // 0x180048a70  not ported
    static Object::Base* CreateNewObject();                       // 0x180048b70  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180048c20  factory_ui1.cpp door

protected:
    // NOT retail's `??0CheckElement@UI@@QEAA@XZ` (0x1801116a0, the door
    // `RtUi_CheckElement_Ctor`).
    CheckElement() = default;
    // `??1CheckElement@UI@@UEAA@XZ` (0x180111700), NON-VIRTUAL, declared only;
    // the retail body is the door `RtUi_CheckElement_Dtor`.
    ~CheckElement();

private:
    static const char* _TypeName();                               // 0x180048a40 -> "CheckElement"
    static const char* _ParentTypeName();                         // 0x180048a50 -> "ButtonElement"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa9a8 (minted)

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

}  // namespace UI
