#pragma once
// ============================================================================
// runtime/types/ui/ComboDescription.h — UI::ComboDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout POD: `RtUI::ComboDescription` in
// DescriptionsBatch1.h (not edited, RULE B6-4.4); every offset cross-asserted.
//
// PARENT `UI::ButtonDescription` — READ, an INTERMEDIATE leaf (RULE B6-9):
//     ?_ParentTypeName@ComboDescription@UI@@CAPEBDXZ  @0x180074ab0 -> "ButtonDescription"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@ComboDescription@UI@@…`
// 0x1801faa18 stays minted):
//     ?_TypeName@ComboDescription@UI@@CAPEBDXZ        @0x180074aa0 -> "ComboDescription"
//     ?_ParentTypeName@ComboDescription@UI@@CAPEBDXZ  @0x180074ab0 -> "ButtonDescription"
//
// `??_7ComboDescription@UI@@6B@` @ 0x180159090, TEN slots (sizeof from
// `MOV EDX,0xb8` in slot 0 0x180074e70):
//
//   [0] 0x180074e70  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180074ae0  TypeName
//   [2] 0x180074ac0  Type
//   [3] 0x1801129b0  GetFileVersion  (`return 0x2104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared
//   [5] 0x1801129c0  Serialize        (ButtonDescription::Serialize first)
//   [6] 0x180074cc0  Reconstruct
//   [7] 0x180074d70  Destruct
//   [8] 0x180112a70  CreateInstance(const Dialog*)  RETSLOT (B6-2)
//   [9] 0x180074d20  Construct — OVERRIDES ButtonDescription's slot 9
//
// `??0ComboDescription@UI@@QEAA@XZ` IS EXPORTED (0x180112320, B36_SLOTS.md
// section 6: `CALL ??0ButtonDescription`, own table twice); combodescription.cpp's
// old banner said there was none. Nothing registers it; `Construct` and
// `Reconstruct` open-code the same four stores.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::ComboDescription aliases this class

namespace UI {

class ComboDescription : public ButtonDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180074e70
    const char* TypeName() const override;                        // [1] 0x180074ae0
    uint32_t    Type() const override;                            // [2] 0x180074ac0
    int32_t     GetFileVersion() const override;                  // [3] 0x1801129b0
    bool        Serialize(void* stream) override;                 // [5] 0x1801129c0
    Object::External* Reconstruct() override;                     // [6] 0x180074cc0
    void        Destruct() override;                              // [7] 0x180074d70
    // HIDES ButtonDescription's non-virtual CreateInstance — call it QUALIFIED.
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x180112a70  RETSLOT
    Object::External* Construct() override;                       // [9] 0x180074d20

    ComboDescription(const ComboDescription& src);                // 0x180074d80  not ported
    ComboDescription& operator=(const ComboDescription& src);     // 0x180074df0  not ported

    static uint32_t TypeId();                                     // 0x180074ad0  not ported
    static Object::Base* CreateNewObject();                       // 0x180074bd0  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180074c70  factory_ui1.cpp door

protected:
    // NOT retail's `??0ComboDescription@UI@@QEAA@XZ` (0x180112320, unported).
    ComboDescription() = default;
    // `??1ComboDescription@UI@@UEAA@XZ` (0x180112370), declared only; the
    // retail body is the door `RtUi_ComboDescription_Dtor`.
    ~ComboDescription();

private:
    static const char* _TypeName();                               // 0x180074aa0 -> "ComboDescription"
    static const char* _ParentTypeName();                         // 0x180074ab0 -> "ButtonDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801faa18 (minted)

public:
    // RtUI::ComboDescription's names, unchanged.
    //
    // NAME-HIDING NOTE: `_stringTable` here (+0xb0) HIDES
    // `ElementDescription::_stringTable` (+0x58). Every body in this class
    // means the combo's own item table; spell the base's
    // `ElementDescription::_stringTable` if it is ever needed.
    void* _listDesc;               // +0xa0  ExternalPtr<ElementDescription> (0x180038f90 @ 0x180112a0f)
    void* _itemDesc;               // +0xa8  ExternalPtr<ElementDescription> (0x180038f90 @ 0x180112a20)
    void* _stringTable;            // +0xb0  ExternalPtr<StringTable>        (0x1800042a0 @ 0x180112a31)
};

static_assert(sizeof(ComboDescription) == 0xb8,
              "UI::ComboDescription is 0xb8 bytes (MOV EDX,0xb8 in 0x180074e70)");
static_assert(alignof(ComboDescription) == 8, "UI::ComboDescription aligns to 8");
static_assert(!std::has_virtual_destructor<ComboDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(ComboDescription, _listDesc) == sizeof(ButtonDescription), "");
static_assert(offsetof(ComboDescription, _listDesc)    == 0xa0, "");
static_assert(offsetof(ComboDescription, _itemDesc)    == 0xa8, "");
static_assert(offsetof(ComboDescription, _stringTable) == 0xb0, "");

}  // namespace UI
