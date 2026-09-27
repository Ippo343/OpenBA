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
//   [0] 0x180074e70  (unexported) vector deleting dtor — the class's own (B.3.9)
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
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0ComboDescription@UI@@QEAA@XZ` (0x180074bd0 (the factory's inlined copy)) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~ComboDescription is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7ComboDescription@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ButtonDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::ComboDescription aliases this class
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class ComboDescription : public ButtonDescription {
public:
    // The REAL constructor, the stores Construct / Reconstruct inline
    // (0x180074d42..0x180074d63): CALL ??0ButtonDescription
    // (ButtonDescription()), then +0xa0, +0xa8, +0xb0 zero.
    ComboDescription() : ButtonDescription() {
        _listDesc    = nullptr;
        _itemDesc    = nullptr;
        _stringTable = nullptr;
    }

    // ---- the ten slots, in measured order ---------------------------------
    // [0] 0x180074e70 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xb8`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ComboDescription*>(self + static_cast<int64_t>(i) * 0xb8)
                    ->~ComboDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ComboDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
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

    static uint32_t TypeId();                                     // 0x180074ad0  ported over the minted record
    static Object::Base* CreateNewObject();                       // 0x180074bd0  the class's own (B39 D2)
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180074c70  the class's own (B39 D2)

protected:
    // `??1ComboDescription@UI@@UEAA@XZ` (0x180112370). NON-VIRTUAL and EMPTY
    // (combodescription.cpp): three restamps and the base's +0x60, all the
    // compiler's chain.
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
static_assert(!std::is_abstract<ComboDescription>::value, "B39 D1: concrete");
static_assert(offsetof(ComboDescription, _listDesc) == sizeof(ButtonDescription), "");
static_assert(offsetof(ComboDescription, _listDesc)    == 0xa0, "");
static_assert(offsetof(ComboDescription, _itemDesc)    == 0xa8, "");
static_assert(offsetof(ComboDescription, _stringTable) == 0xb0, "");

}  // namespace UI
