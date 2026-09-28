#pragma once
// ============================================================================
// runtime/types/ui/LabelDescription.h — UI::LabelDescription, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C3. The POD `RtUI::LabelDescription` stays in
// DescriptionsBatch1.h untouched (RULE B6-4.4 — the closing pass turns it into
// an alias); this class is cross-asserted against it at the bottom.
//
// PARENT `UI::TextDescription` — READ (B36_SLOTS.md section 3), RECORDED, NOT USED:
//     ?_TypeName@LabelDescription@UI@@CAPEBDXZ       @0x180042370 -> "LabelDescription"
//     ?_ParentTypeName@LabelDescription@UI@@CAPEBDXZ @0x180042380 -> "TextDescription"
//
// `??_7LabelDescription@UI@@6B@` @ 0x180156b30, TEN slots:
//   [0] 0x180042730 vector deleting dtor · [1] 0x1800423b0 TypeName
//   [2] 0x180042390 Type · [3] 0x1800d2950 GetFileVersion (0x2304)
//   [4] External::Compile, inherited · [5] 0x1800d2820 Serialize
//   [6] 0x180042530 Reconstruct · [7] 0x180042560 Destruct
//   [8] 0x1800d26b0 CreateInstance RETSLOT (B6-2) · [9] 0x180042550 Construct
//   — OVERRIDES TextDescription's slot 9 (`override`, not `virtual`).
//
// SIZE 0xb8 (`MOV EDX,0xb8` in 0x180042730). LAYOUT, from the ctor
// `??0LabelDescription@UI@@QEAA@XZ` (0x1800d29d0, read as DISASSEMBLY by block
// C3: `CALL ??0TextDescription` then):
//     +0x90 dword 3     _word90  kScalarUInt  -> LabelElement +0x110 (_align)
//     +0x94 QWORD 1     _word94  kScalarUInt  -> +0x114 (_mode)     } one store
//                       _word98  kScalarInt   -> +0x118 (_minTextWidth)
//     +0x9c dword 2     _word9c  kScalarInt   -> +0x11c (_gap)
//     +0xa0 QWORD -1    _worda0  kScalarInt   -> +0x120 (_iconWidth) } one store
//                       _worda4  kScalarInt   -> +0x124 (_iconHeight)
//     +0xa8 qword 0     _spriteSheet  ExternalPtr<UI::SpriteSheet> (0x180004590)
//     +0xb0 qword 0     _spriteName   System::String (0x1800c9930)
// The field names are the POD's (DescriptionsBatch1.h); where each one lands on
// the element is CreateInstance's copy (labeldescription.cpp), recorded above.
//
// ENDGAME B.3 step 9 (block UI-A, B39_RULES D1/D2): CONCRETE. The real public
// ctor is retail's `??0LabelDescription@UI@@QEAA@XZ` (0x1800d29d0) minus its vptr stores;
// slot 0 is the class's own vector deleting dtor; ~LabelDescription is protected and
// non-virtual. The factory pair is the class's own; the minted
// `??_7LabelDescription@UI@@6B@`, its RT_DELDTOR row, the factory-interpreter row and
// the doors are retired.
// ============================================================================

#include <type_traits>
#include <cstdint>
#include <cstddef>

#include "TextDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::LabelDescription aliases this class
#include "../object/Internal.h"   // Object::detail::InternalMemFree

namespace UI {

class SpriteSheet;

class LabelDescription : public TextDescription {
public:
    // ??0LabelDescription@UI@@QEAA@XZ 0x1800d29d0 — the REAL constructor: CALL
    // ??0TextDescription (TextDescription()), then +0x90 DWORD 3, ONE QWORD 1
    // over +0x94/+0x98, +0x9c DWORD 2, ONE QWORD -1 over +0xa0/+0xa4, +0xa8
    // and +0xb0 QWORD zero.
    LabelDescription() : TextDescription() {
        _word90 = 3;
        _word94 = 1;
        _word98 = 0;
        _word9c = 2;
        _worda0 = -1;
        _worda4 = -1;
        _spriteSheet = nullptr;
        _spriteName  = nullptr;
    }

    // [0] 0x180042730 — MSVC's VECTOR deleting destructor, both arms exactly as
    // the retired RT_DELDTOR row ran them (`MOV EDX,0xb8`, the element size).
    Object::Base* DeletingDtor(unsigned flags) override {
        if (flags & 2u) {
            uint8_t* const self = reinterpret_cast<uint8_t*>(this);
            const int32_t count = *reinterpret_cast<const int32_t*>(self - 8);
            for (int32_t i = count; --i >= 0;)
                reinterpret_cast<LabelDescription*>(self + static_cast<int64_t>(i) * 0xb8)
                    ->~LabelDescription();
            if (flags & 1u) Object::detail::InternalMemFree(self - 8);
            return reinterpret_cast<Object::Base*>(self - 8);
        }
        this->~LabelDescription();
        if (flags & 1u) Object::detail::InternalMemFree(this);
        return this;
    }
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x1800423b0, Type 0x180042390, TypeId 0x1800423a0, _TypeName 0x180042370
    //         _ParentTypeName 0x180042380, CreateNewObject 0x1800424a0, CreateInPlaceObject 0x180042520
    DECLARE_TYPE(LabelDescription)
public:
    int32_t     GetFileVersion() const override;                 // [3] 0x1800d2950
    bool        Serialize(void* stream) override;                // [5] 0x1800d2820
    Object::External* Reconstruct() override;                    // [6] 0x180042530
    void        Destruct() override;                             // [7] 0x180042560
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                    // [8] 0x1800d26b0 RETSLOT (B6-2)
    Object::External* Construct() override;                      // [9] 0x180042550

    LabelDescription(const LabelDescription& src);               // 0x180042570  not ported
    LabelDescription& operator=(const LabelDescription& src);    // 0x180042630  not ported
    static void* operator new(std::size_t bytes);                // 0x1800423c0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x180042430  not ported
    static void  operator delete(void* p);                       // 0x180042440  not ported

protected:
    // `??1LabelDescription@UI@@UEAA@XZ` (0x1800d2960). NON-VIRTUAL
    // (labeldescription.cpp): releases the sprite name at +0xb0; the compiler
    // chains ~TextDescription. +0xa8 is NOT released.
    ~LabelDescription();

private:

public:
    uint32_t     _word90;        // +0x90  kScalarUInt, ctor 3
    uint32_t     _word94;        // +0x94  kScalarUInt, ctor 1
    int32_t      _word98;        // +0x98  kScalarInt,  ctor 0
    int32_t      _word9c;        // +0x9c  kScalarInt,  ctor 2
    int32_t      _worda0;        // +0xa0  kScalarInt,  ctor -1
    int32_t      _worda4;        // +0xa4  kScalarInt,  ctor -1
    SpriteSheet* _spriteSheet;   // +0xa8  ExternalPtr<UI::SpriteSheet>
    char*        _spriteName;    // +0xb0  System::String (refcounted char*)
};

static_assert(sizeof(LabelDescription) == 0xb8,
              "UI::LabelDescription is 0xb8 (MOV EDX,0xb8 in 0x180042730)");
static_assert(alignof(LabelDescription) == 8, "UI::LabelDescription aligns to 8");
static_assert(!std::has_virtual_destructor<LabelDescription>::value, "ENDGAME decision 2");
static_assert(!std::is_abstract<LabelDescription>::value, "B39 D1: concrete");
static_assert(offsetof(LabelDescription, _word90) == sizeof(TextDescription),
              "LabelDescription's own fields begin at the end of TextDescription");
static_assert(offsetof(LabelDescription, _word90)      == 0x90, "");
static_assert(offsetof(LabelDescription, _word94)      == 0x94, "");
static_assert(offsetof(LabelDescription, _word98)      == 0x98, "");
static_assert(offsetof(LabelDescription, _word9c)      == 0x9c, "");
static_assert(offsetof(LabelDescription, _worda0)      == 0xa0, "");
static_assert(offsetof(LabelDescription, _worda4)      == 0xa4, "");
static_assert(offsetof(LabelDescription, _spriteSheet) == 0xa8, "");
static_assert(offsetof(LabelDescription, _spriteName)  == 0xb0, "");
// RtUI::LabelDescription is an ALIAS of this class since fragment pass F2; the
// POD's offsets are the absolute asserts above.

}  // namespace UI
