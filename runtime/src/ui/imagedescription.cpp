// ============================================================================
// runtime/src/ui/imagedescription.cpp — UI::ImageDescription, the whole class
// ============================================================================
//
// PORTED AS A DEPENDENCY, NOT AS A WALL. The cutover boot stops on
// ??_7AnimatedImageDescription@UI@@6B@, and AnimatedImageDescription DERIVES
// from this class in a way that cannot be worked around:
//     ~AnimatedImageDescription (0x1800cb6b0) TAIL-JUMPS to ~ImageDescription
//         (0x180118370) at 0x1800cb6e2
//     AnimatedImageDescription::Serialize (0x1800cb5c0) CALLS
//         ImageDescription::Serialize (0x1801184b0) at 0x1800cb5da
//     both CreateInstance bodies call the shared applier 0x180118570
//
// ??_7ImageDescription@UI@@6B@ (retail 0x180156110), ten slots:
//   0  deleting dtor      0x1800361f0  ImageDescription.h (the class's own)
//   1  TypeName           0x180035cd0  here
//   2  Type               0x180035cb0  here
//   3  GetFileVersion     0x1801184a0  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1801184b0  here
//   6  Reconstruct        0x180035f60  here
//   7  Destruct           0x1800360c0  here
//   8  CreateInstance     0x180118910  here
//   9  Construct          0x180036010  here
// plus ??1ImageDescription@UI@@UEAA@XZ 0x180118370, the factory pair, and the
// applier 0x180118570 — the NON-VIRTUAL
// `?CreateInstance@ImageDescription@UI@@QEBAXV?$InternalPtr@VImageElement@UI@@@Object@@@Z`.
// It IS EXPORTED (B36_SLOTS.md section 6); this file called it "unexported"
// and still registers it by address only, which is enough: a by-name lookup
// lands on the same address.
//
// LAYOUT: runtime/types/ui/ImageDescription.h — ElementDescription (0x68) plus
// `_spriteSheet` +0x68, `_spriteName` +0x70, `_width` +0x78, `_height` +0x7c.
//
// THE +0x68 ExternalPtr IS NOT RELEASED BY THE DESTRUCTOR. ~ImageDescription
// releases the String at +0x70 and the ElementDescription String at +0x60 and
// nothing else. That is retail's, transcribed, not an omission.
//
// ENDGAME B.3.6 block C2: every slot body and the applier are MEMBERS of
// `UI::ImageDescription`; the registered doors are one-line QUALIFIED
// delegations. The base Serialize (by address 0x180116b00 before) is the
// qualified member `ElementDescription::Serialize`; the applier's
// `GetSpriteIndexFromName` (RT_EXPORT_ONCE before) is the real
// `UI::SpriteSheet` member.
//
// ENDGAME B.3.9 block UI-A (B39_RULES D1/D2): the class is CONCRETE. The real
// ctor (`??0ImageDescription@UI@@QEAA@XZ` 0x1801182d0, the stores this file's
// ConstructBody transcribed) is in the header; the factory pair is defined and
// registered here, replacing the ui_factory_wave28.cpp row. The slot doors and
// the ??1 door, read only by the retired minted table and its RT_DELDTOR row,
// are gone. The applier's door (non-slot) stays.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/system/String.h"
#include "../../types/ui/ImageDescription.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/SpriteSheet.h"

namespace {

using namespace rtdesc;

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

int32_t ImageDescription::GetFileVersion() const { return 0x204; }   // 0x1801184a0

// 0x180036010: the null test (180036021) sat in the retired door; then the
// inlined ctor (0x18003602a..0x18003609e) -- the real ctor (ENDGAME B.3.9 UI-A).
Object::External* ImageDescription::Construct() {
    return ::new (this) ImageDescription();
}
// 0x180035f60: slot 0 with flag 0 through the object's own table, then the
// inlined ctor. No null test.
Object::External* ImageDescription::Reconstruct() {
    this->~ImageDescription();                                           // 180035f77
    return ::new (this) ImageDescription();
}
// 0x1800360c0: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void ImageDescription::Destruct() { this->~ImageDescription(); }

// ============================================================================
// ?Serialize@ImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801184b0
// ============================================================================
//   ElementDescription::Serialize(this, stream)        0x180116b00, QUALIFIED
//   rw = encoding factory, read or write side by stream->mode
//   ExternalPtr<SpriteSheet>::Serialize(&_spriteSheet, &pair)   0x180004590
//   System::String::Serialize(&_spriteName, &pair)              0x1800c9930
//   rw->vt[0xa0](rw, &_width, 0)                                kScalarInt
//   rw->vt[0xa0](rw, &_height, 0)                               kScalarInt
//   if (rw) rw->vt[0](rw, 1)
//
// NOTE THE TWO FIELD SERIALIZERS TAKE THE PAIR, NOT THE READER: both are
// `LEA RDX,[RSP+0x28]` (0x180118503, 0x180118511), the {stream, readerWriter}
// pair, which is why it has to live on this frame.
bool ImageDescription::Serialize(void* streamv) {
    rtdesc::Stream* stream = static_cast<rtdesc::Stream*>(streamv);

    ElementDescription::Serialize(stream);                     // 1801184ca

    rtdesc::Serializer ser = { stream, nullptr };              // [RSP+0x28], [RSP+0x30]
    IO::ReaderWriter* rw = rtdesc::MakeReaderWriter(stream, &ser); // 1801184fa

    RtObject_ExternalPtrSerialize_SpriteSheet(ExtField(&_spriteSheet), IoSer(&ser));   // 180118508 ExternalPtr<SpriteSheet>
    StringSerialize(&_spriteName, &ser);    // 180118516 String

    rw->SerializeInt(_width, nullptr);
    rw->SerializeInt(_height, nullptr);

    if (rw != nullptr) delete rw;                    // 18011854b
    return true;
}

// ============================================================================
// The applier, retail 0x180118570 — the NON-VIRTUAL CreateInstance
// ============================================================================
// `void CreateInstance(InternalPtr<ImageElement> element) const`, the handle
// arriving as the ADDRESS of the caller's copy (RDX, RULE B6-11).
//
//   local = *elemPtr                                    ; 18011857f/18011858a
//   ElementDescription::CreateInstance(self, &local)     ; 180118592
//   sheet = _spriteSheet                                 ; 180118597
//   idx   = sheet ? sheet->GetSpriteIndexFromName(_spriteName) : -1
//   e = *elemPtr        (RE-READ, not the local)         ; 1801185ba
//   e->_sprite.sheet = sheet ; QWORD at e->_sprite.index = idx slot
//   e = *elemPtr ; e->_width  = _width
//   e = *elemPtr ; e->_height = _height
//
// *** TWO THINGS HERE ARE DELIBERATE AND BOTH LOOK LIKE BUGS. ***
//
// 1. `*elemPtr` IS RE-READ THREE TIMES, from the CALLER's slot, after
//    ElementDescription::CreateInstance has run on a LOCAL COPY of it.
//
// 2. THE INDEX IS STORED AS A QWORD READ FROM A SLOT ONLY A DWORD WAS WRITTEN
//    TO. 0x1801185ac and 0x1801185b2 write `dword [RSP+0x28]`; 0x1801185bd
//    reads `qword [RSP+0x28]` and 0x1801185ce stores all eight bytes, so the
//    Sprite's pad receives four bytes of whatever was on the stack. What is
//    faithful is the WIDTH (description_common.h's SpriteIndexSlot note).
//
// `ElementDescription::CreateInstance` (0x180115ad0) is a QUALIFIED member
// call since fragment pass F2 defined it (dialog.cpp).
void ImageDescription::CreateInstance(Object::InternalPtr<ImageElement>* element) const {
    void* local = element->Get();                              // 18011857f / 18011858a
    UI::ElementDescription::CreateInstance(                   // 180118592
        reinterpret_cast<Object::InternalPtr<Element>*>(&local));  // B.3.6 bridge

    void* const sheet = _spriteSheet;                          // 180118597

    uint64_t idxSlot;                                          // [RSP+0x28], see note 2
    if (sheet != nullptr) {
        const int32_t idx = static_cast<const SpriteSheet*>(sheet)->GetSpriteIndexFromName(
            *reinterpret_cast<const System::String*>(&_spriteName));   // B.3.6 bridge — String's one pointer
        __builtin_memcpy(&idxSlot, &idx, sizeof idx);          // 1801185ac  DWORD store
    } else {
        const int32_t minusOne = -1;
        __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);// 1801185b2  DWORD store
    }

    ImageElement* e = element->Get();                          // 1801185ba
    e->_sprite.spriteSheet = sheet;                            // 1801185c2
    // index AND the Sprite's pad, one 8-byte store.
    __builtin_memcpy(reinterpret_cast<uint8_t*>(&e->_sprite) + offsetof(Sprite, spriteIndex),
                     &idxSlot, sizeof idxSlot);                // 1801185ce  QWORD store

    element->Get()->_width  = _width;                          // 1801185d5 / 1801185db
    element->Get()->_height = _height;                         // 1801185e1 / 1801185ec
}

// ============================================================================
// ?CreateInstance@ImageDescription@UI@@...   retail 0x180118910   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot. R8 (the
// Dialog*) is never read: this body creates no children. The mangled name
// still carries `PEBVDialog@2@` because the virtual's signature is fixed by
// the base.
//
// ImageElement is 0xe0 bytes (`MOV EDX,0xe0` at 0x180118951). Its constructor is
// INLINED here rather than called -- the real ImageElement ctor's stores
// (ImageElement.h, ENDGAME B.3.9 UI-A): ??0Element (0x180115e50), then the
// ImageElement vtable, then four stores whose widths are not uniform:
//     0x180118991  MOV qword  [RDI+0xc0],0
//     0x18011899c  MOV dword  [RDI+0xc8],0xffffffff
//     0x1801189a6  MOV dword  [RDI+0xd0],0
//     0x1801189b0  MOV qword  [RDI+0xd4],-1      <- UNALIGNED qword at +0xd4,
//                                                   covering +0xd4 AND +0xd8
Object::InternalPtr<Element>* ImageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog*) const {
    void* raw = rtdesc::MemAlloc(0xe0, 8);                     // 180118951

    ImageElement* elem = nullptr;
    if (raw != nullptr)                                        // 180118979
        elem = ::new (raw) ImageElement();                     // 180118981..1801189b0 inlined ctor

    Object::InternalPtr<ImageElement> slot(elem);              // 1801189bb
    ImageDescription::CreateInstance(&slot);                   // 1801189c8

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1801189cd
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1ImageDescription@UI@@UEAA@XZ   0x180118370 -- retail's body, moved here
// from the retired door (ENDGAME B.3.9 UI-A)
// ============================================================================
// The sprite name at +0x70 under this class's own vtable (the entry restamp is
// the compiler's), through System::String's own Release (the idiom retail
// inlines at 0x180118394..0x1801183b2, with its asymmetry: the freeing branch
// leaves the field at `data - 8`, the sharing branch nulls it). Then the
// compiler chains ~ElementDescription (the base stamp and _textName at +0x60)
// and ~External (retail's tail JMP at 0x1801183f6). +0x68 is NOT released.
namespace UI {
ImageDescription::~ImageDescription() {
    System::StringPrivateAccess::Release(
        *reinterpret_cast<System::String*>(&_spriteName));     // 180118394
}
}  // namespace UI

namespace UI {
// ?CreateNewObject@ImageDescription@UI@@SAPEAVBase@Object@@XZ  0x180035dc0
// The factory-interpreter row this replaces: MemAlloc(0x80, 8), retail's
// null branch, then the inlined ctor (ElementDescription's stores, then +0x68/+0x70 zero and +0x78 QWORD -1).
Object::Base* ImageDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ImageDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ImageDescription();
}
// ?CreateInPlaceObject@ImageDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x180035eb0
Object::Base* ImageDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ImageDescription();
}
}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ImageDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ImageDescription@UI@@ (0x1801fac10): name "ImageDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ImageDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ImageDescription, "ElementDescription", nullptr)
