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
//   [0] 0x180037eb0  (unexported) vector deleting dtor — the class's own (B.3.9)
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
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0ClipRegionDescription@UI@@QEAA@XZ` (0x1800cc140) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~ClipRegionDescription is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7ClipRegionDescription@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::ClipRegionDescription aliases this class
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class ClipRegionDescription : public ElementDescription {
public:
    // ??0ClipRegionDescription@UI@@QEAA@XZ 0x1800cc140 — the REAL constructor,
    // the store sequence Construct/Reconstruct and both factory steps inline
    // (0x180037cfa..0x180037d78): ElementDescription()'s stores, then the two
    // extents 0x64 (DWORDs) and the child ExternalPtr zero.
    ClipRegionDescription() : ElementDescription() {
        _width     = 0x64;                                     // 180037d66
        _height    = 0x64;                                     // 180037d6d
        _childDesc = nullptr;                                  // 180037d74
    }

    // ---- the ten slots, in measured order ---------------------------------
    // [0] 0x180037eb0 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0x78`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<ClipRegionDescription*>(self + static_cast<int64_t>(i) * 0x78)
                    ->~ClipRegionDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~ClipRegionDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
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

    static uint32_t TypeId();                                     // 0x180037980  ported over the minted record
    static Object::Base* CreateNewObject();                       // 0x180037a80  the class's own (B39 D2)
    static Object::Base* CreateInPlaceObject(void* place);        // 0x180037b80  the class's own (B39 D2)

protected:
    // `??1ClipRegionDescription@UI@@UEAA@XZ` (0x1800cc0d0). NON-VIRTUAL and
    // EMPTY (clipregiondescription.cpp): retail releases nothing of its own;
    // the compiler chains ~ElementDescription (+0x60) and ~External.
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
static_assert(!std::is_abstract<ClipRegionDescription>::value, "B39 D1: concrete");
static_assert(offsetof(ClipRegionDescription, _width) == sizeof(ElementDescription), "");
static_assert(offsetof(ClipRegionDescription, _width)     == 0x68, "");
static_assert(offsetof(ClipRegionDescription, _height)    == 0x6c, "");
static_assert(offsetof(ClipRegionDescription, _childDesc) == 0x70, "");

}  // namespace UI
