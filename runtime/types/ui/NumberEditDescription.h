#pragma once
// ============================================================================
// runtime/types/ui/NumberEditDescription.h — UI::NumberEditDescription
// ============================================================================
//
// ENDGAME B.3 step 6, block C3. No header existed; the layout lived as raw
// offsets in runtime/src/ui/numbereditdescription.cpp (RULE B6-8), MOVED here.
//
// PARENT `UI::ElementDescription` — NOT EditDescription. READ (B36_SLOTS.md
// section 3), RECORDED, NOT USED:
//     ?_TypeName@NumberEditDescription@UI@@CAPEBDXZ       @0x180045e50 -> "NumberEditDescription"
//     ?_ParentTypeName@NumberEditDescription@UI@@CAPEBDXZ @0x180045e60 -> "ElementDescription"
// (Its ELEMENT derives from EditElement; the description does not derive from
// EditDescription — the two hierarchies differ here, and the literal decides.)
//
// `??_7NumberEditDescription@UI@@6B@` @ 0x180156fe0, TEN slots: [0] 0x180046190
// · [1] 0x180045e90 TypeName · [2] 0x180045e70 Type · [3] 0x1800d4fd0
// GetFileVersion (0x1004) · [4] External::Compile, inherited · [5] 0x1800d4ee0
// Serialize · [6] 0x180046010 Reconstruct · [7] 0x180046040 Destruct ·
// [8] 0x1800d4cc0 CreateInstance RETSLOT (B6-2) · [9] 0x180046030 Construct.
//
// SIZE 0x88 (`MOV EDX,0x88` at 0x1800461af). LAYOUT, from the ctor
// `??0NumberEditDescription@UI@@QEAA@XZ` (0x1800d5050, transcribed in
// numbereditdescription.cpp) after ElementDescription's 0x68:
//     +0x68 qword 0      _font  ExternalPtr<UI::Font> (0x18003f1e0)
//     +0x70              _i70   *** NEVER WRITTEN BY THE CONSTRUCTOR *** —
//                               serialized (kScalarInt) and formatted into the
//                               element's initial text with "%i"
//     +0x74 dword 6      _i74   -> EditElement::_maxLength
//     +0x78 dword -32767 _i78   -> NumberEditElement::_min
//     +0x7c dword 0x7fff _i7c   -> NumberEditElement::_max
//     +0x80 byte  1      _b80   "allow negative": selects "0123456789-"
//     +0x81..+0x87 never written
// Names: the ported file's; meanings where CreateInstance proves them.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"

namespace UI {

class Dialog;
class Element;
class Font;

class NumberEditDescription : public ElementDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x180046190
    const char* TypeName() const override;                       // [1] 0x180045e90
    uint32_t    Type() const override;                           // [2] 0x180045e70
    int32_t     GetFileVersion() const override;                 // [3] 0x1800d4fd0
    bool        Serialize(void* stream) override;                // [5] 0x1800d4ee0
    Object::External* Reconstruct() override;                    // [6] 0x180046010
    void        Destruct() override;                             // [7] 0x180046040
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                    // [8] 0x1800d4cc0 RETSLOT (B6-2)
    virtual Object::External* Construct();                       // [9] 0x180046030 — INTRODUCED

    NumberEditDescription(const NumberEditDescription& src);             // 0x180046050  not ported
    NumberEditDescription& operator=(const NumberEditDescription& src);  // 0x1800460c0  not ported
    static uint32_t TypeId();                                    // 0x180045e80  not ported
    static Object::Base* CreateNewObject();                      // 0x180045f80  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x180046000  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x180045ea0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x180045f10  not ported
    static void  operator delete(void* p);                       // 0x180045f20  not ported

protected:
    NumberEditDescription() = default;   // NOT retail's 0x1800d5050 (the door)
    ~NumberEditDescription();            // 0x1800d4fe0 — NOT defined (~ElementDescription is not)

private:
    static const char* _TypeName();                              // 0x180045e50 -> "NumberEditDescription"
    static const char* _ParentTypeName();                        // 0x180045e60 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801fa158 (minted)

public:
    Font*   _font;       // +0x68  ExternalPtr<UI::Font>
    int32_t _i70;        // +0x70  NEVER written by the ctor
    int32_t _i74;        // +0x74  ctor 6
    int32_t _i78;        // +0x78  ctor 0xffff8001
    int32_t _i7c;        // +0x7c  ctor 0x7fff
    uint8_t _b80;        // +0x80  ctor 1
    uint8_t _pad81[7];   // +0x81  never written
};

static_assert(sizeof(NumberEditDescription) == 0x88,
              "UI::NumberEditDescription is 0x88 (MOV EDX,0x88 at 0x1800461af)");
static_assert(alignof(NumberEditDescription) == 8, "UI::NumberEditDescription aligns to 8");
static_assert(offsetof(NumberEditDescription, _font) == sizeof(ElementDescription), "");
static_assert(offsetof(NumberEditDescription, _font) == 0x68, "");
static_assert(offsetof(NumberEditDescription, _i70)  == 0x70, "");
static_assert(offsetof(NumberEditDescription, _i74)  == 0x74, "");
static_assert(offsetof(NumberEditDescription, _i78)  == 0x78, "");
static_assert(offsetof(NumberEditDescription, _i7c)  == 0x7c, "");
static_assert(offsetof(NumberEditDescription, _b80)  == 0x80, "");

}  // namespace UI
