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
//   [0] 0x1800489c0  (unexported) vector deleting dtor — the class's own (B.3.9)
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
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0CheckDescription@UI@@QEAA@XZ` (0x1801115e0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~CheckDescription is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7CheckDescription@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::CheckDescription aliases this class
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class CheckDescription : public ButtonDescription {
public:
    // ??0CheckDescription@UI@@QEAA@XZ 0x1801115e0 — the REAL constructor, the
    // same stores Construct / Reconstruct inline: CALL ??0ButtonDescription
    // (ButtonDescription()), then +0xa0 zero (0x1800488cc).
    CheckDescription() : ButtonDescription() {
        _checkDesc = nullptr;
    }

    // ---- the ten slots, in measured order ---------------------------------
    // [0] 0x1800489c0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xa8`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<CheckDescription*>(self + static_cast<int64_t>(i) * 0xa8)
                    ->~CheckDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~CheckDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x180048680, Type 0x180048660, TypeId 0x180048670, _TypeName 0x180048640
    //         _ParentTypeName 0x180048650, CreateNewObject 0x180048770, CreateInPlaceObject 0x180048800
    DECLARE_TYPE(CheckDescription)
public:
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


protected:
    // `??1CheckDescription@UI@@UEAA@XZ` (0x180111620). NON-VIRTUAL and EMPTY
    // (checkdescription.cpp): three restamps and the base's +0x60, all the
    // compiler's chain; the +0xa0 child description is NOT released.
    ~CheckDescription();

private:

public:
    void* _checkDesc;              // +0xa0  ExternalPtr<ElementDescription>, the tick.
                                   //        Construct: MOV [RBX+0xa0],0 @ 0x1800488cc
};

static_assert(sizeof(CheckDescription) == 0xa8,
              "UI::CheckDescription is 0xa8 bytes (MOV EDX,0xa8 in 0x1800489c0)");
static_assert(alignof(CheckDescription) == 8, "UI::CheckDescription aligns to 8");
static_assert(!std::has_virtual_destructor<CheckDescription>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<CheckDescription>::value, "B39 D1: concrete");
static_assert(offsetof(CheckDescription, _checkDesc) == sizeof(ButtonDescription), "");
static_assert(offsetof(CheckDescription, _checkDesc) == 0xa0, "");

}  // namespace UI
