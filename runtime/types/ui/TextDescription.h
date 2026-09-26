#pragma once
// ============================================================================
// runtime/types/ui/TextDescription.h — UI::TextDescription, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C3. Before this block the class had no header: its
// layout lived as raw `p + 0x..` offsets in runtime/src/ui/textdescription.cpp
// (RULE B6-8). Those offsets are MOVED here under the retail name; the bodies
// now read the named fields.
//
// PARENT `UI::ElementDescription` — READ, not inferred (B36_SLOTS.md section 3):
//     ?_TypeName@TextDescription@UI@@CAPEBDXZ       @0x180041a90 -> "TextDescription"
//     ?_ParentTypeName@TextDescription@UI@@CAPEBDXZ @0x180041aa0 -> "ElementDescription"
// RECORDED, NOT USED (B5-15 / B6-7): no DEFINE_TYPE in step 6; slots 1/2 read
// the minted `?_typeInfo@TextDescription@UI@@0VBaseTypeInfo@Object@@A`
// (0x1801faeb0).
//
// `??_7TextDescription@UI@@6B@` @ 0x180156be0, TEN slots (B36_SLOTS.md sec. 1):
//   [0] +0x00 0x180041e30 vector deleting dtor  (deleting_dtor.cpp row)
//   [1] +0x08 0x180041ad0 TypeName
//   [2] +0x10 0x180041ab0 Type
//   [3] +0x18 0x18011ee20 GetFileVersion   (0x304)
//   [4] +0x20 0x1800066e0 Object::External::Compile — inherited, NOT declared
//   [5] +0x28 0x18011ee30 Serialize
//   [6] +0x30 0x180041c50 Reconstruct
//   [7] +0x38 0x180041c80 Destruct
//   [8] +0x40 0x18011f3d0 CreateInstance(const Dialog*) -> InternalPtr<Element>
//                          RETSLOT (RULE B6-2)
//   [9] +0x48 0x180041c70 Construct — INTRODUCED here (RULE B6-1.1)
//
// SIZE 0x90: `MOV EDX,0x90` in the slot-0 vector deleting destructor
// (0x180041e4f). LAYOUT: the ctor `??0TextDescription@UI@@QEAA@XZ`
// (0x18011ea20) writes, after ElementDescription's 0x68:
//     +0x68 qword 0  ExternalPtr<UI::Font>         Serialize: 0x18003f1e0
//     +0x70 qword 0  ExternalPtr<UI::StringTable>  Serialize: 0x1800042a0
//     +0x78 qword 0  System::String (the string NAME)  0x1800c9930
//     +0x80 dword 0  kScalarUInt
//     +0x84 WORD  0  two kScalarBool bytes (+0x84, +0x85) in ONE store
//     +0x86 ..+0x87  NEVER WRITTEN by the ctor and read by no body
//     +0x88 dword 0xc8  kScalarInt
//     +0x8c dword -1    kScalarInt, serialized CONDITIONALLY (textdescription.cpp)
// The scalar names are the ported file's (`_u80`/`_b84`/`_b85`/`_i88`/`_i8c`):
// Serialize proves the widths, nothing proves a meaning. Not invented here.
//
// ABSTRACT (slot 0 `= 0`); no real ctor (B4b-2): every live one is built by the
// `??0TextDescription` DOOR (textdescription.cpp) and wears the MINTED table,
// so every door delegates with a QUALIFIED call.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"
#include "../object/Internal.h"

namespace UI {

class Dialog;
class Element;
class Font;
class StringTable;
class TextElement;

class TextDescription : public ElementDescription {
public:
    // ---- the ten slots, in measured order ---------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x180041e30
    const char* TypeName() const override;                       // [1] 0x180041ad0
    uint32_t    Type() const override;                           // [2] 0x180041ab0
    int32_t     GetFileVersion() const override;                 // [3] 0x18011ee20
    // [4] Compile — Object::External's own body, inherited (B6-1.5).
    bool        Serialize(void* stream) override;                // [5] 0x18011ee30 (IO::Stream&)
    Object::External* Reconstruct() override;                    // [6] 0x180041c50
    void        Destruct() override;                             // [7] 0x180041c80
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                    // [8] 0x18011f3d0 RETSLOT (B6-2)
    virtual Object::External* Construct();                       // [9] 0x180041c70 — INTRODUCED

    // ---- non-virtual members ------------------------------------------------
    // ?CreateInstance@TextDescription@UI@@QEBAXV?$InternalPtr@VTextElement@UI@@@Object@@PEBVDialog@2@@Z
    //   0x18011ef70 — the "stamp this description onto that element" applier.
    //   `V?$InternalPtr` BY VALUE, which retail passes by HIDDEN POINTER
    //   (RULE B6-11), so the parameter is spelled as the pointer. Its body is
    //   the registered door `RtUi_TextDescription_CreateInstance` in
    //   element_bodies_b5.cpp (NOT this block's file — fragment pass); declared
    //   here, left undefined. Slot 8 and LabelDescription reach it by address.
    void CreateInstance(Object::InternalPtr<TextElement>* element,
                        const Dialog* dialog) const;

    TextDescription(const TextDescription& src);                 // 0x180041c90  not ported
    TextDescription& operator=(const TextDescription& src);      // 0x180041d40  not ported

    // Reflection statics / allocation. `_typeInfo` is the MINTED record.
    static uint32_t TypeId();                                    // 0x180041ac0  not ported
    static Object::Base* CreateNewObject();                      // 0x180041bc0  ui_factory_wave28.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x180041c40  ui_factory_wave28.cpp door
    static void* operator new(std::size_t bytes);                // 0x180041ae0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x180041b50  not ported
    static void  operator delete(void* p);                       // 0x180041b60  not ported

protected:
    // NOT retail's `??0TextDescription@UI@@QEAA@XZ` (0x18011ea20 — the door
    // `RtUi_TextDescription_Ctor`, textdescription.cpp).
    TextDescription() = default;
    // `??1TextDescription@UI@@UEAA@XZ` (0x18011eae0). NON-VIRTUAL and NOT
    // defined (a body would chain to ~ElementDescription, undefined in the real
    // build); the retail body is the door `RtUi_TextDescription_Dtor`.
    ~TextDescription();

private:
    static const char* _TypeName();                              // 0x180041a90 -> "TextDescription"
    static const char* _ParentTypeName();                        // 0x180041aa0 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801faeb0 (minted)

public:
    // ---- the 0x28 bytes TextDescription adds (public: doors and the applier
    // in element_bodies_b5.cpp read them) ------------------------------------
    Font*        _font;          // +0x68  ExternalPtr<UI::Font>
    StringTable* _stringTable2;  // +0x70  ExternalPtr<UI::StringTable> (the base's +0x58 is another)
    char*        _name;          // +0x78  System::String (refcounted char*)
    uint32_t     _u80;           // +0x80  kScalarUInt
    uint8_t      _b84;           // +0x84  kScalarBool
    uint8_t      _b85;           // +0x85  kScalarBool
    uint8_t      _pad86[2];      // +0x86  never written
    int32_t      _i88;           // +0x88  kScalarInt, ctor 0xc8
    int32_t      _i8c;           // +0x8c  kScalarInt, ctor -1, conditional
};

static_assert(sizeof(TextDescription) == 0x90,
              "UI::TextDescription is 0x90 (MOV EDX,0x90 in 0x180041e30)");
static_assert(alignof(TextDescription) == 8, "UI::TextDescription aligns to 8");
static_assert(offsetof(TextDescription, _font) == sizeof(ElementDescription),
              "TextDescription's own fields begin at the end of ElementDescription");
static_assert(offsetof(TextDescription, _font)         == 0x68, "");
static_assert(offsetof(TextDescription, _stringTable2) == 0x70, "");
static_assert(offsetof(TextDescription, _name)         == 0x78, "");
static_assert(offsetof(TextDescription, _u80)          == 0x80, "");
static_assert(offsetof(TextDescription, _b84)          == 0x84, "");
static_assert(offsetof(TextDescription, _b85)          == 0x85, "");
static_assert(offsetof(TextDescription, _i88)          == 0x88, "");
static_assert(offsetof(TextDescription, _i8c)          == 0x8c, "");

}  // namespace UI
