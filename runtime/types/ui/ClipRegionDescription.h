#pragma once
// ============================================================================
// runtime/types/ui/ClipRegionDescription.h — UI::ClipRegionDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C1. Layout POD: `RtUI::ClipRegionDescription` in
// DescriptionsBatch1.h (not edited, RULE B6-4.4); every offset cross-asserted.
//
// PARENT `UI::ElementDescription` — READ:
//     ?_ParentTypeName@ClipRegionDescription@UI@@CAPEBDXZ  @0x180037960 -> "ElementDescription"
// RULE B5-15 / B6-7 — RECORDED, NOT USED (`?_typeInfo@ClipRegionDescription@UI@@…`
// 0x1801f9d30 stays minted):
//     ?_TypeName@ClipRegionDescription@UI@@CAPEBDXZ        @0x180037950 -> "ClipRegionDescription"
//     ?_ParentTypeName@ClipRegionDescription@UI@@CAPEBDXZ  @0x180037960 -> "ElementDescription"
//
// `??_7ClipRegionDescription@UI@@6B@` @ 0x180156350, TEN slots (sizeof from
// `MOV EDX,0x78` in slot 0 0x180037eb0):
//
//   [0] 0x180037eb0  (unexported) vector deleting dtor — RT_DELDTOR row, kept
//   [1] 0x180037990  TypeName
//   [2] 0x180037970  Type
//   [3] 0x1800cc0c0  GetFileVersion  (`return 0x104`)
//   [4] 0x1800066e0  Object::External::Compile  INHERITED — NOT declared
//   [5] 0x1800cc010  Serialize
//   [6] 0x180037c30  Reconstruct
//   [7] 0x180037d90  Destruct
//   [8] 0x1800cbec0  CreateInstance(const Dialog*)  RETSLOT (B6-2)
//   [9] 0x180037ce0  Construct — INTRODUCED here (`virtual`, B6-1.1)
//
// `??0ClipRegionDescription@UI@@QEAA@XZ` IS EXPORTED (0x1800cc140,
// B36_SLOTS.md sections 6 and 7); clipregiondescription.cpp's old banner said
// the DLL exported none. Nothing registers it; `Construct`/`Reconstruct`
// open-code the same store sequence.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::ClipRegionDescription aliases this class

namespace UI {

class ClipRegionDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x180037eb0
    const char* TypeName() const override;                        // [1] 0x180037990
    uint32_t    Type() const override;                            // [2] 0x180037970
    int32_t     GetFileVersion() const override;                  // [3] 0x1800cc0c0
    bool        Serialize(void* stream) override;                 // [5] 0x1800cc010
    Object::External* Reconstruct() override;                     // [6] 0x180037c30
    void        Destruct() override;                              // [7] 0x180037d90
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800cbec0  RETSLOT
    virtual Object::External* Construct();                        // [9] 0x180037ce0  INTRODUCED

    ClipRegionDescription(const ClipRegionDescription& src);            // 0x180037da0  not ported
    ClipRegionDescription& operator=(const ClipRegionDescription& src); // 0x180037df0  not ported

    static uint32_t TypeId();                                     // 0x180037980  not ported
    static Object::Base* CreateNewObject();                       // 0x180037a80  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180037b80  factory_ui1.cpp door

protected:
    // NOT retail's `??0ClipRegionDescription@UI@@QEAA@XZ` (0x1800cc140, unported).
    ClipRegionDescription() = default;
    // `??1ClipRegionDescription@UI@@UEAA@XZ` (0x1800cc0d0), declared only; the
    // retail body is the door `RtUi_ClipRegionDescription_Dtor`.
    ~ClipRegionDescription();

private:
    static const char* _TypeName();                               // 0x180037950 -> "ClipRegionDescription"
    static const char* _ParentTypeName();                         // 0x180037960 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                        // 0x1801f9d30 (minted)

public:
    // RtUI::ClipRegionDescription's names, unchanged.
    //
    // NAME-HIDING NOTE — ALL THREE: `_width` (+0x68) hides
    // `ElementDescription::_width` (+0x28), `_height` (+0x6c) hides
    // `ElementDescription::_height` (+0x2c), and `_childDesc` (+0x70) hides
    // `ElementDescription::_childDesc` (+0x48). Every body in this class means
    // its own three; spell the base's `ElementDescription::_width` etc.
    int32_t _width;                // +0x68  ctor 0x64; kScalarInt @ 0x1800cc067; to elem +0xc0
    int32_t _height;               // +0x6c  ctor 0x64; kScalarInt @ 0x1800cc07c; to elem +0xc4
    void*   _childDesc;            // +0x70  ExternalPtr<ElementDescription> (0x180038f90 @ 0x1800cc089)
};

static_assert(sizeof(ClipRegionDescription) == 0x78,
              "UI::ClipRegionDescription is 0x78 bytes (MOV EDX,0x78 in 0x180037eb0)");
static_assert(alignof(ClipRegionDescription) == 8, "UI::ClipRegionDescription aligns to 8");
static_assert(!std::has_virtual_destructor<ClipRegionDescription>::value, "ENDGAME decision 2");
static_assert(offsetof(ClipRegionDescription, _width) == sizeof(ElementDescription), "");
static_assert(offsetof(ClipRegionDescription, _width)     == 0x68, "");
static_assert(offsetof(ClipRegionDescription, _height)    == 0x6c, "");
static_assert(offsetof(ClipRegionDescription, _childDesc) == 0x70, "");

}  // namespace UI
