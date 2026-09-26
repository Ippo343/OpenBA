#pragma once
// ============================================================================
// runtime/types/ui/CheckDescription.h — UI::CheckDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout POD: `RtUI::CheckDescription` in
// DescriptionsBatch1.h (not edited, RULE B6-4.4); every offset cross-asserted.
//
// PARENT `UI::ButtonDescription` — READ, an INTERMEDIATE leaf (RULE B6-9):
//     ?_ParentTypeName@CheckDescription@UI@@CAPEBDXZ  @0x180048650 -> "ButtonDescription"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@CheckDescription@UI@@…`
// 0x1801fa970 stays minted):
//     ?_TypeName@CheckDescription@UI@@CAPEBDXZ        @0x180048640 -> "CheckDescription"
//     ?_ParentTypeName@CheckDescription@UI@@CAPEBDXZ  @0x180048650 -> "ButtonDescription"
//
// `??_7CheckDescription@UI@@6B@` @ 0x1801572c0, TEN slots (sizeof from
// `MOV EDX,0xa8` in slot 0 0x1800489c0):
//
//   [0] 0x1800489c0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180048680  TypeName
//   [2] 0x180048660  Type
//   [3] 0x180111920  GetFileVersion  (`return 0x1104` — NOT the base's 0x104)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared
//   [5] 0x180111930  Serialize        (ButtonDescription::Serialize first)
//   [6] 0x180048850  Reconstruct
//   [7] 0x1800488f0  Destruct
//   [8] 0x1801119c0  CreateInstance(const Dialog*)  RETSLOT (B6-2)
//   [9] 0x1800488a0  Construct — OVERRIDES ButtonDescription's slot 9
//
// `??0CheckDescription@UI@@QEAA@XZ` IS EXPORTED (0x1801115e0, B36_SLOTS.md
// section 6; its stamp chain is `CALL ??0ButtonDescription`, own table twice,
// section 7). checkdescription.cpp's banner said the DLL exports none — that
// was wrong; nothing registers it, and `Construct`/`Reconstruct` open-code the
// same two stores.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::CheckDescription aliases this class

namespace UI {

class CheckDescription : public ButtonDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x1800489c0
    const char* TypeName() const override;                        // [1] 0x180048680
    uint32_t    Type() const override;                            // [2] 0x180048660
    int32_t     GetFileVersion() const override;                  // [3] 0x180111920
    bool        Serialize(void* stream) override;                 // [5] 0x180111930
    Object::External* Reconstruct() override;                     // [6] 0x180048850
    void        Destruct() override;                              // [7] 0x1800488f0
    // HIDES ButtonDescription's non-virtual CreateInstance(InternalPtr<ButtonElement>*,
    // …) and ElementDescription's — call those QUALIFIED.
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1801119c0  RETSLOT
    Object::External* Construct() override;                       // [9] 0x1800488a0

    CheckDescription(const CheckDescription& src);                // 0x180048900  not ported
    CheckDescription& operator=(const CheckDescription& src);     // 0x180048950  not ported

    static uint32_t TypeId();                                     // 0x180048670  not ported
    static Object::Base* CreateNewObject();                       // 0x180048770  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180048800  factory_ui1.cpp door

protected:
    // NOT retail's `??0CheckDescription@UI@@QEAA@XZ` (0x1801115e0, unported).
    CheckDescription() = default;
    // `??1CheckDescription@UI@@UEAA@XZ` (0x180111620), declared only; the
    // retail body is the door `RtUi_CheckDescription_Dtor`.
    ~CheckDescription();

private:
    static const char* _TypeName();                               // 0x180048640 -> "CheckDescription"
    static const char* _ParentTypeName();                         // 0x180048650 -> "ButtonDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa970 (minted)

public:
    void* _checkDesc;              // +0xa0  ExternalPtr<ElementDescription>, the tick.
                                   //        Construct: MOV [RBX+0xa0],0 @ 0x1800488cc
};

static_assert(sizeof(CheckDescription) == 0xa8,
              "UI::CheckDescription is 0xa8 bytes (MOV EDX,0xa8 in 0x1800489c0)");
static_assert(alignof(CheckDescription) == 8, "UI::CheckDescription aligns to 8");
static_assert(!std::has_virtual_destructor<CheckDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(CheckDescription, _checkDesc) == sizeof(ButtonDescription), "");
static_assert(offsetof(CheckDescription, _checkDesc) == 0xa0, "");

}  // namespace UI
