#pragma once
// ============================================================================
// runtime/types/ui/EditDescription.h — UI::EditDescription, the REAL C++ class
// ============================================================================
//
// ENDGAME B.3 step 6, block C3. The POD `RtUI::EditDescription` stays in
// DescriptionsBatch1.h untouched (RULE B6-4.4); this class is cross-asserted
// against it at the bottom.
//
// PARENT `UI::ElementDescription` — READ (B36_SLOTS.md section 3), RECORDED:
//     ?_TypeName@EditDescription@UI@@CAPEBDXZ       @0x18003f690 -> "EditDescription"
//     ?_ParentTypeName@EditDescription@UI@@CAPEBDXZ @0x18003f6a0 -> "ElementDescription"
//
// `??_7EditDescription@UI@@6B@` @ 0x180156880, TEN slots: [0] 0x18003fa20 ·
// [1] 0x18003f6d0 TypeName · [2] 0x18003f6b0 Type · [3] 0x1800d11e0
// GetFileVersion (0x204) · [4] External::Compile, inherited · [5] 0x1800d1110
// Serialize · [6] 0x18003f850 Reconstruct · [7] 0x18003f880 Destruct ·
// [8] 0x1800d0f90 CreateInstance RETSLOT (B6-2) · [9] 0x18003f870 Construct
// (INTRODUCED).
//
// SIZE 0x88 (`MOV EDX,0x88` in 0x18003fa20). LAYOUT, the ctor
// `??0EditDescription@UI@@QEAA@XZ` (0x1800d12c0, read as DISASSEMBLY by block
// C3) after ElementDescription's +0x60:
//     +0x68 qword 0   _ptr68     ExternalPtr<UI::Font> (0x18003f1e0) -> EditElement::_font
//     +0x70 qword 0   _string70  System::String                      -> EditElement::_text
//     +0x78 dword 0   _word78    kScalarInt                          -> EditElement::_maxLength
//     +0x7c byte  0   _flag7c    kScalarBool                         -> EditElement::_password
//     +0x7d..+0x7f    NOT written
//     +0x80 qword 0   _string80  System::String                      -> EditElement::_filter
// Names: the POD's. Where each lands is CreateInstance's copy (editdescription.cpp).
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // RtUI::EditDescription — cross-asserted below

namespace UI {

class Dialog;
class Element;
class Font;

class EditDescription : public ElementDescription {
public:
    Object::Base* DeletingDtor(unsigned flags) override = 0;     // [0] 0x18003fa20
    const char* TypeName() const override;                       // [1] 0x18003f6d0
    uint32_t    Type() const override;                           // [2] 0x18003f6b0
    int32_t     GetFileVersion() const override;                 // [3] 0x1800d11e0
    bool        Serialize(void* stream) override;                // [5] 0x1800d1110
    Object::External* Reconstruct() override;                    // [6] 0x18003f850
    void        Destruct() override;                             // [7] 0x18003f880
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                    // [8] 0x1800d0f90 RETSLOT (B6-2)
    virtual Object::External* Construct();                       // [9] 0x18003f870 — INTRODUCED

    EditDescription(const EditDescription& src);                 // 0x18003f890  not ported
    EditDescription& operator=(const EditDescription& src);      // 0x18003f920  not ported
    static uint32_t TypeId();                                    // 0x18003f6c0  not ported
    static Object::Base* CreateNewObject();                      // 0x18003f7c0  factory_ui1.cpp door
    static Object::Base* CreateInPlaceObject(void* place);       // 0x18003f840  factory_ui1.cpp door
    static void* operator new(std::size_t bytes);                // 0x18003f6e0  not ported
    static void* operator new(std::size_t bytes, void* place);   // 0x18003f750  not ported
    static void  operator delete(void* p);                       // 0x18003f760  not ported

protected:
    EditDescription() = default;   // NOT retail's 0x1800d12c0 (factory_ui1.cpp door)
    // 0x1800d11f0. NON-VIRTUAL, NOT defined (~ElementDescription is not); the
    // retail body is the door `RtUi_EditDescription_Dtor`.
    ~EditDescription();

private:
    static const char* _TypeName();                              // 0x18003f690 -> "EditDescription"
    static const char* _ParentTypeName();                        // 0x18003f6a0 -> "ElementDescription"
    static Object::BaseTypeInfo _typeInfo;                       // 0x1801f9f28 (minted)

public:
    Font*   _ptr68;       // +0x68  ExternalPtr<UI::Font>
    char*   _string70;    // +0x70  System::String (refcounted char*)
    int32_t _word78;      // +0x78  kScalarInt
    uint8_t _flag7c;      // +0x7c  kScalarBool
    uint8_t _pad7d[3];    // +0x7d  never written
    char*   _string80;    // +0x80  System::String (refcounted char*)
};

static_assert(sizeof(EditDescription) == 0x88,
              "UI::EditDescription is 0x88 (MOV EDX,0x88 in 0x18003fa20)");
static_assert(alignof(EditDescription) == 8, "UI::EditDescription aligns to 8");
static_assert(offsetof(EditDescription, _ptr68) == sizeof(ElementDescription), "");
static_assert(offsetof(EditDescription, _ptr68)    == 0x68, "");
static_assert(offsetof(EditDescription, _string70) == 0x70, "");
static_assert(offsetof(EditDescription, _word78)   == 0x78, "");
static_assert(offsetof(EditDescription, _flag7c)   == 0x7c, "");
static_assert(offsetof(EditDescription, _string80) == 0x80, "");
// The class and the DescriptionsBatch1.h POD describe the same bytes.
static_assert(sizeof(EditDescription) == sizeof(RtUI::EditDescription), "");
static_assert(offsetof(EditDescription, _ptr68)    == offsetof(RtUI::EditDescription, _ptr68), "");
static_assert(offsetof(EditDescription, _string70) == offsetof(RtUI::EditDescription, _string70), "");
static_assert(offsetof(EditDescription, _word78)   == offsetof(RtUI::EditDescription, _word78), "");
static_assert(offsetof(EditDescription, _flag7c)   == offsetof(RtUI::EditDescription, _flag7c), "");
static_assert(offsetof(EditDescription, _string80) == offsetof(RtUI::EditDescription, _string80), "");

}  // namespace UI
