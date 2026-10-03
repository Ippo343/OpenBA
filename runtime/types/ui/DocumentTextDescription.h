#pragma once
// ============================================================================
// runtime/types/ui/DocumentTextDescription.h — UI::DocumentTextDescription,
// the REAL class
// ============================================================================
//
// ENDGAME B.3 step 6, block C5. The layout was pinned by the Wave 7 batch as
// the POD `RtUI::DocumentTextDescription` in DescriptionsBatch1.h, which four
// leaf clusters share and none may edit (RULE B6-4.4); this header carries the
// class and cross-asserts every offset against that POD. The field NAMES here
// follow what the applier (0x1800cf520, documenttextdescription.cpp) copies
// each one into on the DocumentTextElement (DocumentTextElement.h), the
// convention ElementDescription.h's banner uses.
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@DocumentTextDescription@UI@@CAPEBDXZ       @0x18003b8c0 -> "DocumentTextDescription"
//     ?_ParentTypeName@DocumentTextDescription@UI@@CAPEBDXZ @0x18003b8d0 -> "ElementDescription"
//
// `??_7DocumentTextDescription@UI@@6B@` @ 0x180156750, TEN slots; sizeof 0xe8
// (`MOV EDX,0xe8` in the slot-0 vector deleting dtor 0x18003bd90):
//   [0] 0x18003bd90 deleting dtor (RT_DELDTOR row)  [1] 0x18003b900 TypeName
//   [2] 0x18003b8e0 Type            [3] 0x1800cfb20 GetFileVersion (0x204)
//   [4] External::Compile (inherited, NOT declared — B6-1.5)
//   [5] 0x1800cf8a0 Serialize       [6] 0x18003ba80 Reconstruct
//   [7] 0x18003bab0 Destruct        [8] 0x1800cf7e0 CreateInstance RETSLOT (B6-2)
//   [9] 0x18003baa0 Construct — INTRODUCED here (B6-1.1)
//
// A SECOND, NON-VIRTUAL `CreateInstance` — the applier:
//   ?CreateInstance@DocumentTextDescription@UI@@QEBAXV?$InternalPtr@
//       VDocumentTextElement@UI@@@Object@@PEBVDialog@2@@Z      0x1800cf520
// It IS EXPORTED (B36_SLOTS.md section 6 lists it at 0x1800cf520); the port's
// banner calling it "UNEXPORTED, address-only" was wrong, and its registration
// stays address-only. It returns void and takes the InternalPtr BY VALUE, which
// retail passes as the ADDRESS of the caller's handle (block A's finding), so
// it is declared with `InternalPtr<DocumentTextElement>*`.
//
// ENDGAME B.3 step 9 (block UI-B, B39_RULES D1/D2): CONCRETE. A real public
// ctor (retail's 0x1800cfc30 stores), its own slot 0, the factory pair as the
// class's own statics; ~DocumentTextDescription holds the retired ??1 door's
// body. The minted `??_7DocumentTextDescription@UI@@6B@`, its RT_DELDTOR row
// 0x18003bd90, the FACTUI1 row and the ctor / dtor / slot doors are retired.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "ElementDescription.h"
#include "DescriptionsBatch1.h"   // kVtbl* names; RtUI::DocumentTextDescription aliases this class

namespace UI {

class DocumentTextElement;

class SpriteSheet;
class StringTable;
class Font;
class ElementDescription;

class DocumentTextDescription : public ElementDescription {
public:
    // ??0DocumentTextDescription@UI@@QEAA@XZ 0x1800cfc30 -- the REAL ctor:
    // ElementDescription's (inlined in retail), then +0x68..+0x97 zero, the
    // three link colours, tab width 4, wrap width 300, +0xb0..+0xe7 zero.
    // +0xac is written by nothing.
    DocumentTextDescription() : ElementDescription() {
        _documentStringTable = nullptr;
        _documentTextName    = nullptr;
        _spriteSheet         = nullptr;
        _underlineSpriteName = nullptr;
        _frameNamesData      = nullptr;
        _frameNamesCount     = 0;
        _frameNamesCapacity  = 0;
        _linkColor           = 0xffeeee00u;
        _linkHoverColor      = 0xffffff00u;
        _linkPressedColor    = 0xffcccc00u;
        _tabWidth            = 4;
        _wrapWidth           = 0x12c;
        _fontsData = nullptr;  _fontsCount = 0;  _fontsCapacity = 0;
        _sheetsData = nullptr; _sheetsCount = 0; _sheetsCapacity = 0;
        _colorsData = nullptr; _colorsCount = 0; _colorsCapacity = 0;
        _resourceList        = nullptr;
    }

    // [0] 0x18003bd90 -- MSVC's VECTOR deleting destructor (retail; here the
    // virtual destructor, B3X W1) (`MOV EDX,0xe8` = the element size).
    // ??1 0x1800cfb30 -- VIRTUAL (B3X W1); retail's body (documenttextdescription.cpp),
    // the compiler chains ~ElementDescription and ~External.
    ~DocumentTextDescription() override;
    // ENDGAME B.3.9 TI-UI: ONE reflection block (B39 D6); `_typeInfo` is the
    // class's own record (DEFINE_TYPE in the class's TU), no longer minted.
    // retail: TypeName 0x18003b900, Type 0x18003b8e0, _TypeName 0x18003b8c0, _ParentTypeName 0x18003b8d0
    //         CreateNewObject 0x18003b9f0, CreateInPlaceObject 0x18003ba70
    DECLARE_TYPE(DocumentTextDescription)
public:
    int32_t     GetFileVersion() const override;                  // [3] 0x1800cfb20
    bool        Serialize(IO::Stream& stream) override;                 // [5] 0x1800cf8a0 (IO::Stream&)
    Object::External* Reconstruct() override;                     // [6] 0x18003ba80
    void        Destruct() override;                              // [7] 0x18003bab0
    Object::InternalPtr<Element>* CreateInstance(
        Object::InternalPtr<Element>* ret,
        const Dialog* dialog) const override;                     // [8] 0x1800cf7e0 RETSLOT
    virtual Object::External* Construct();                        // [9] 0x18003baa0

    // 0x1800cf520 — the applier (see the banner). Dialog is never read.
    void CreateInstance(Object::InternalPtr<DocumentTextElement>* element,
                        const Dialog* dialog) const;

    DocumentTextDescription(const DocumentTextDescription& src);  // 0x18003bac0  not ported
    DocumentTextDescription& operator=(const DocumentTextDescription& src); // 0x18003bc00 not ported

private:

public:
    // (named apart from ElementDescription's own _stringTable/_textName at
    // +0x58/+0x60, which they would otherwise hide)
    StringTable* _documentStringTable; // +0x68 (B4 H4: typed)  ExternalPtr<UI::StringTable> -> element text's table
    char*    _documentTextName;     // +0x70  System::String -> the text's string-table index
    SpriteSheet* _spriteSheet;      // +0x78 (B4 H4: typed)  ExternalPtr<UI::SpriteSheet> -> element +0xd0
    char*    _underlineSpriteName;  // +0x80  System::String -> element _underlineSprite
    // +0x88 Array<System::String>: one sprite name per frame-table slot (^li).
    char**   _frameNamesData;       // +0x88 (B4 H4: Array<System::String>, each a char*)
    int32_t  _frameNamesCount;      // +0x90
    int32_t  _frameNamesCapacity;   // +0x94
    uint32_t _linkColor;            // +0x98  kScalarUInt -> element +0xf8
    uint32_t _linkHoverColor;       // +0x9c  kScalarUInt -> +0xfc
    uint32_t _linkPressedColor;     // +0xa0  kScalarUInt -> +0x100
    int32_t  _tabWidth;             // +0xa4  kScalarInt  -> +0x104
    int32_t  _wrapWidth;            // +0xa8  kScalarInt  -> +0x108
    int32_t  _padAC;                // +0xac
    // Three arrays and a container the applier hands on whole (0x18003ce30,
    // 0x18003cf20, the open-coded Array<int> copy, and a qword copy).
    Font**   _fontsData;            // +0xb0 (B4 H4: typed)  -> element _fonts (+0x110)
    int32_t  _fontsCount;           // +0xb8
    int32_t  _fontsCapacity;        // +0xbc
    SpriteSheet** _sheetsData;      // +0xc0 (B4 H4: typed)  -> element _sheets (+0x120)
    int32_t  _sheetsCount;          // +0xc8
    int32_t  _sheetsCapacity;       // +0xcc
    uint32_t* _colorsData;          // +0xd0  Array<uint32> (B4: SerializeUInt, retail's type), serialized inline -> element _colors (+0x130)
    int32_t  _colorsCount;          // +0xd8
    int32_t  _colorsCapacity;       // +0xdc
    Object::ExternalList* _resourceList; // +0xe0 (B4 H4: typed)  0x18003dc90 -> element _resourceList (+0x140)
};

static_assert(sizeof(DocumentTextDescription) == 0xe8,
              "UI::DocumentTextDescription is 0xe8 (MOV EDX,0xe8 in 0x18003bd90)");
static_assert(alignof(DocumentTextDescription) == 8, "UI::DocumentTextDescription aligns to 8");
static_assert(offsetof(DocumentTextDescription, _documentStringTable) == sizeof(ElementDescription), "");
// RtUI::DocumentTextDescription is an ALIAS of this class since fragment
// pass F2; these are the offsets the POD pinned.
static_assert(offsetof(DocumentTextDescription, _documentStringTable) == 0x68, "");
static_assert(offsetof(DocumentTextDescription, _documentTextName)    == 0x70, "");
static_assert(offsetof(DocumentTextDescription, _spriteSheet)         == 0x78, "");
static_assert(offsetof(DocumentTextDescription, _underlineSpriteName) == 0x80, "");
static_assert(offsetof(DocumentTextDescription, _frameNamesData)      == 0x88, "");
static_assert(offsetof(DocumentTextDescription, _frameNamesCount)     == 0x90, "");
static_assert(offsetof(DocumentTextDescription, _linkColor)           == 0x98, "");
static_assert(offsetof(DocumentTextDescription, _linkHoverColor)      == 0x9c, "");
static_assert(offsetof(DocumentTextDescription, _linkPressedColor)    == 0xa0, "");
static_assert(offsetof(DocumentTextDescription, _tabWidth)            == 0xa4, "");
static_assert(offsetof(DocumentTextDescription, _wrapWidth)           == 0xa8, "");
static_assert(offsetof(DocumentTextDescription, _fontsData)           == 0xb0, "");
static_assert(offsetof(DocumentTextDescription, _fontsCount)          == 0xb8, "");
static_assert(offsetof(DocumentTextDescription, _sheetsData)          == 0xc0, "");
static_assert(offsetof(DocumentTextDescription, _sheetsCount)         == 0xc8, "");
static_assert(offsetof(DocumentTextDescription, _colorsData)          == 0xd0, "");
static_assert(offsetof(DocumentTextDescription, _colorsCount)         == 0xd8, "");
static_assert(offsetof(DocumentTextDescription, _resourceList)        == 0xe0, "");

}  // namespace UI
