#pragma once
// ============================================================================
// runtime/types/ui/NumberDescription.h — UI::NumberDescription, the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C3. No header existed; the layout lived as raw
// offsets in runtime/src/ui/numberdescription.cpp (RULE B6-8), MOVED here.
//
// PARENT `UI::ElementDescription` — READ (B36_SLOTS.md section 3), RECORDED:
//     ?_TypeName@NumberDescription@UI@@CAPEBDXZ       @0x18004a200 -> "NumberDescription"
//     ?_ParentTypeName@NumberDescription@UI@@CAPEBDXZ @0x18004a210 -> "ElementDescription"
//
// `??_7NumberDescription@UI@@6B@` @ 0x180157530, TEN slots: [0] 0x18004a760 ·
// [1] 0x18004a240 TypeName · [2] 0x18004a220 Type · [3] 0x180118e20
// GetFileVersion (0x104) · [4] External::Compile, inherited · [5] 0x180118e30
// Serialize · [6] 0x18004a4c0 Reconstruct · [7] 0x18004a610 Destruct ·
// [8] 0x180118f10 CreateInstance RETSLOT (B6-2) · [9] 0x18004a570 Construct.
//
// SIZE 0x88 (`MOV EDX,0x88` at 0x18004a77f). The ctor is INLINED into
// Construct and Reconstruct (no ??0 export is called by either; the exported
// `??0NumberDescription@UI@@QEAA@XZ` 0x180118b60 is ui_factory_wave28.cpp's):
//     +0x68 qword 0   _font  ExternalPtr<UI::Font> (0x18003f1e0) -> NumberElement::_font
//     +0x70 qword 0   _u70   kScalarUInt  -> NumberElement::_mode      } one
//                     _i74   kScalarInt   -> NumberElement::_intValue  } store
//     +0x78 dword 0   _f78   kScalarFloat -> NumberElement::_floatValue
//     +0x7c byte  0   _b7c   kScalarBool  -> NumberElement::_showSign
//     +0x7d..+0x87 never written
// The `NumberType` enum `?GetNumberType@NumberElement@UI@@` returns is nested
// here (`W4NumberType@NumberDescription@2@`); its enumerator names are not in
// the binary, and none is invented (0 = int, 1 = float per CacheText).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"

namespace UI {

class Dialog;
class Element;
class Font;

class NumberDescription : public ElementDescription {
public:
    // A `W4` enum: four bytes, returned in EAX.
    enum NumberType : int32_t {};

    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x18004a760
    const char* TypeName() const override;                       // [1] 0x18004a240
    uint32_t    Type() const override;                           // [2] 0x18004a220
    int32_t     GetFileVersion() const override;                 // [3] 0x180118e20
    bool        Serialize(void* stream) override;                // [5] 0x180118e30
    Object::External* Reconstruct() override;                    // [6] 0x18004a4c0
    void        Destruct() override;                             // [7] 0x18004a610
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                    // [8] 0x180118f10 RETSLOT (B6-2)
    virtual Object::External* Construct();                       // [9] 0x18004a570 — INTRODUCED

    NumberDescription(const NumberDescription& src);             // 0x18004a620  not ported
    NumberDescription& operator=(const NumberDescription& src);  // 0x18004a690  not ported
    static uint32_t TypeId();                                    // 0x18004a230  not ported
    static Object::Base* CreateNewObject();                      // 0x18004a330  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x18004a420  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x18004a250  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x18004a2c0  not ported
    static void  operator delete(void* p);                       // 0x18004a2d0  not ported

protected:
    NumberDescription() = default;   // NOT retail's 0x180118b60 (ui_factory_wave28.cpp door)
    ~NumberDescription();            // 0x180118c00 — NOT defined (~ElementDescription is not)

private:
    static const char* _TypeName();                              // 0x18004a200 -> "NumberDescription"
    static const char* _ParentTypeName();                        // 0x18004a210 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801fac80 (minted)

public:
    Font*    _font;       // +0x68  ExternalPtr<UI::Font>
    uint32_t _u70;        // +0x70  kScalarUInt
    int32_t  _i74;        // +0x74  kScalarInt
    float    _f78;        // +0x78  kScalarFloat (the ctor stores integer 0 = 0.0f)
    uint8_t  _b7c;        // +0x7c  kScalarBool
    uint8_t  _pad7d[11];  // +0x7d  never written
};

static_assert(sizeof(NumberDescription) == 0x88,
              "UI::NumberDescription is 0x88 (MOV EDX,0x88 at 0x18004a77f)");
static_assert(alignof(NumberDescription) == 8, "UI::NumberDescription aligns to 8");
static_assert(sizeof(NumberDescription::NumberType) == 4, "a W4 enum is four bytes");
static_assert(offsetof(NumberDescription, _font) == sizeof(ElementDescription), "");
static_assert(offsetof(NumberDescription, _font) == 0x68, "");
static_assert(offsetof(NumberDescription, _u70)  == 0x70, "");
static_assert(offsetof(NumberDescription, _i74)  == 0x74, "");
static_assert(offsetof(NumberDescription, _f78)  == 0x78, "");
static_assert(offsetof(NumberDescription, _b7c)  == 0x7c, "");

}  // namespace UI
