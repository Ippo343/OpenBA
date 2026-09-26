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
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "TextDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::LabelDescription aliases this class

namespace UI {

class SpriteSheet;

class LabelDescription : public TextDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x180042730
    const char* TypeName() const override;                       // [1] 0x1800423b0
    uint32_t    Type() const override;                           // [2] 0x180042390
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
    static uint32_t TypeId();                                    // 0x1800423a0  not ported
    static Object::Base* CreateNewObject();                      // 0x1800424a0  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x180042520  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x1800423c0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x180042430  not ported
    static void  operator delete(void* p);                       // 0x180042440  not ported

protected:
    // NOT retail's 0x1800d29d0 (ui_factory_wave28.cpp's `Ctor_LabelDescription` door).
    LabelDescription() = default;
    // 0x1800d2960. NON-VIRTUAL, NOT defined (its base's is not); the retail body
    // is the door `RtUi_LabelDescription_Dtor` (labeldescription.cpp).
    ~LabelDescription();

private:
    static const char* _TypeName();                              // 0x180042370 -> "LabelDescription"
    static const char* _ParentTypeName();                        // 0x180042380 -> "TextDescription"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801fa008 (minted)

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
