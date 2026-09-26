#pragma once
// ============================================================================
// runtime/types/ui/BorderDescription.h — UI::BorderDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout POD: `RtUI::BorderDescription` in
// DescriptionsBatch1.h (not edited, RULE B6-4.4); every offset cross-asserted.
//
// PARENT `UI::ElementDescription` — READ:
//     ?_ParentTypeName@BorderDescription@UI@@CAPEBDXZ  @0x1800409b0 -> "ElementDescription"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@BorderDescription@UI@@…`
// 0x1801fa890 stays minted):
//     ?_TypeName@BorderDescription@UI@@CAPEBDXZ        @0x1800409a0 -> "BorderDescription"
//     ?_ParentTypeName@BorderDescription@UI@@CAPEBDXZ  @0x1800409b0 -> "ElementDescription"
//
// `??_7BorderDescription@UI@@6B@` @ 0x180156980, TEN slots (sizeof from
// `MOV EDX,0xb8` in slot 0 0x180040ff0):
//
//   [0] 0x180040ff0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x1800409e0  TypeName
//   [2] 0x1800409c0  Type
//   [3] 0x18010fe10  GetFileVersion  (`return 0x104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared
//   [5] 0x18010fe20  Serialize
//   [6] 0x180040b60  Reconstruct
//   [7] 0x180040b90  Destruct
//   [8] 0x18010ff40  CreateInstance(const Dialog*)  RETSLOT (B6-2)
//   [9] 0x180040b80  Construct — INTRODUCED here (`virtual`, B6-1.1)
//
// ABSTRACT, no real ctor (`??0BorderDescription@UI@@QEAA@XZ` 0x18010fa50 is
// factory_ui1.cpp's door; Construct/Reconstruct tail-jump into it).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::BorderDescription aliases this class

namespace UI {

class BorderDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180040ff0
    const char* TypeName() const override;                        // [1] 0x1800409e0
    uint32_t    Type() const override;                            // [2] 0x1800409c0
    int32_t     GetFileVersion() const override;                  // [3] 0x18010fe10
    bool        Serialize(void* stream) override;                 // [5] 0x18010fe20
    Object::External* Reconstruct() override;                     // [6] 0x180040b60
    void        Destruct() override;                              // [7] 0x180040b90
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x18010ff40  RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180040b80  INTRODUCED

    BorderDescription(const BorderDescription& src);              // 0x180040ba0  not ported
    BorderDescription& operator=(const BorderDescription& src);   // 0x180040ce0  not ported

    static uint32_t TypeId();                                     // 0x1800409d0  not ported
    static Object::Base* CreateNewObject();                       // 0x180040ad0  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180040b50  factory_ui1.cpp door

protected:
    // NOT retail's `??0BorderDescription@UI@@QEAA@XZ` (0x18010fa50).
    BorderDescription() = default;
    // `??1BorderDescription@UI@@UEAA@XZ` (0x18010fb20), declared only; the
    // retail body is the door `RtUi_BorderDescription_Dtor`.
    ~BorderDescription();

private:
    static const char* _TypeName();                               // 0x1800409a0 -> "BorderDescription"
    static const char* _ParentTypeName();                         // 0x1800409b0 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801fa890 (minted)

public:
    // RtUI::BorderDescription's names, unchanged: one ExternalPtr<SpriteSheet>
    // and NINE System::String sprite names, the pieces of a nine-slice border,
    // in BorderElement's row-major order.
    void* _spriteSheet;            // +0x68  LEA RCX,[RDI+0x68] @ 0x18010fe6f
    char* _spriteName[9];          // +0x70 .. +0xb0  @ 0x18010fe7d .. 0x18010feff
};

static_assert(sizeof(BorderDescription) == 0xb8,
              "UI::BorderDescription is 0xb8 bytes (MOV EDX,0xb8 in 0x180040ff0)");
static_assert(alignof(BorderDescription) == 8, "UI::BorderDescription aligns to 8");
static_assert(!std::has_virtual_destructor<BorderDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(BorderDescription, _spriteSheet) == sizeof(ElementDescription), "");
static_assert(offsetof(BorderDescription, _spriteSheet) == 0x68, "");
static_assert(offsetof(BorderDescription, _spriteName)  == 0x70, "");

}  // namespace UI
