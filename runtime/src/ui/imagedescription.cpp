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
//   0  deleting dtor      0x1800361f0  deleting_dtor.cpp row (sizeof 0x80)
//   1  TypeName           0x180035cd0  here
//   2  Type               0x180035cb0  here
//   3  GetFileVersion     0x1801184a0  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1801184b0  here
//   6  Reconstruct        0x180035f60  here
//   7  Destruct           0x1800360c0  here
//   8  CreateInstance     0x180118910  here
//   9  Construct          0x180036010  here
// plus ??1ImageDescription@UI@@UEAA@XZ 0x180118370 (a DOOR, B4b-2) and the
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
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/ui/ImageDescription.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/SpriteSheet.h"

namespace {

using namespace rtdesc;

// RULE B6-5: namespace-scope atomic door caches for the member bodies.
void* AtomicResolve(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtElementDescription{nullptr};
std::atomic<void*> g_vtImageDescription{nullptr};
std::atomic<void*> g_vtImageElement{nullptr};
std::atomic<void*> g_imageDescriptionTypeInfo{nullptr};

void* AtVtBase()     { return AtomicResolve(g_vtBase, "??_7Base@Object@@6B@"); }
void* AtVtExternal() { return AtomicResolve(g_vtExternal, "??_7External@Object@@6B@"); }
void* AtVtElementDescription() {
    return AtomicResolve(g_vtElementDescription, "??_7ElementDescription@UI@@6B@");
}
void* AtVtImageDescription() { return AtomicResolve(g_vtImageDescription, "??_7ImageDescription@UI@@6B@"); }
void* AtVtImageElement()     { return AtomicResolve(g_vtImageElement, "??_7ImageElement@UI@@6B@"); }
const uint8_t* ImageDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_imageDescriptionTypeInfo,
        "?_typeInfo@ImageDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// Door-only (the dtor door is not a member).
void* VtImageDescription() { return RT_DATA_SLOT(void*, "??_7ImageDescription@UI@@6B@"); }

// The inlined constructor, transcribed from 0x18003602a..0x18003609e. Identical
// to every other ElementDescription subclass through +0x60, then three stores
// of its own. It appears in FOUR places in the shipped DLL (Construct,
// Reconstruct, the two factory steps, and again inside
// ??0AnimatedImageDescription) and is written once here.
void ConstructBody(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = AtVtBase();                                            // 18003602a
    *v = AtVtExternal();                                        // 180036034
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 180036049
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18003604c
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 180036050
    *v = AtVtElementDescription();                              // 180036054
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18003605e
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 180036062
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 180036066
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18003606a  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18003606d  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 180036074  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18003607c
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 180036080
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 180036084
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 180036088
    *v = AtVtImageDescription();                                // 18003608c
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 180036096
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18003609a
    *reinterpret_cast<int64_t*>(p + 0x78)  = -1;                // 18003609e  ONE QWORD
}

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

uint32_t ImageDescription::Type() const {                      // 0x180035cb0
    const uint8_t* ti = ImageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::ImageDescription` here.
const char* ImageDescription::TypeName() const {               // 0x180035cd0
    const uint8_t* ti = ImageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t ImageDescription::GetFileVersion() const { return 0x204; }   // 0x1801184a0

// 0x180036010: the null test (180036021) is the DOOR's; then the inlined ctor.
Object::External* ImageDescription::Construct() {
    ConstructBody(this);
    return this;
}
// 0x180035f60: slot 0 with flag 0 through the object's own table, then the
// inlined ctor. No null test.
Object::External* ImageDescription::Reconstruct() {
    DeletingDtor(0);                                           // 180035f77
    ConstructBody(this);
    return this;
}
// 0x1800360c0: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void ImageDescription::Destruct() { DeletingDtor(0); }

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
    void* rw = rtdesc::MakeReaderWriter(stream, &ser);         // 1801184fa

    using FieldSerFn = void (*)(void* field, rtdesc::Serializer* ser);
    rtdesc::RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);   // 180118508 ExternalPtr<SpriteSheet>
    rtdesc::RtAddr<FieldSerFn>(0x1800c9930ull)(&_spriteName, &ser);    // 180118516 String

    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &_width);
    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &_height);

    rtdesc::DestroyReaderWriter(rw);                           // 18011854b
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
// `ElementDescription::CreateInstance` (0x180115ad0) is reached through its
// registered door: its member is declared but its body is still dialog.cpp's.
void ImageDescription::CreateInstance(Object::InternalPtr<ImageElement>* element) const {
    void* local = element->Get();                              // 18011857f / 18011858a
    rtdesc::ElementDescriptionApply(this, &local);             // 180118592

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
// INLINED here rather than called: ??0Element (0x180115e50), then the
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
    if (raw != nullptr) {                                      // 180118979
        rtdesc::ElementCtor(raw);                              // 180118981 ??0Element
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = AtVtImageElement();     // 18011898e
        *reinterpret_cast<uint64_t*>(e + 0xc0) = 0;            // 180118991
        *reinterpret_cast<int32_t*>(e + 0xc8)  = -1;           // 18011899c  DWORD
        *reinterpret_cast<int32_t*>(e + 0xd0)  = 0;            // 1801189a6  DWORD
        __builtin_memcpy(e + 0xd4, "\xff\xff\xff\xff\xff\xff\xff\xff", 8);  // 1801189b0
        elem = reinterpret_cast<ImageElement*>(raw);           // B.3.6 bridge
    }

    Object::InternalPtr<ImageElement> slot(elem);              // 1801189bb
    ImageDescription::CreateInstance(&slot);                   // 1801189c8

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1801189cd
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_ImageDescription_Type(const void* self) {
    return reinterpret_cast<const UI::ImageDescription*>(self)          // B.3.6 bridge
        ->UI::ImageDescription::Type();
}
extern "C" const char* RtUi_ImageDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::ImageDescription*>(self)          // B.3.6 bridge
        ->UI::ImageDescription::TypeName();
}
extern "C" int32_t RtUi_ImageDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::ImageDescription*>(self)          // B.3.6 bridge
        ->UI::ImageDescription::GetFileVersion();
}
extern "C" void* RtUi_ImageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180036021
    return reinterpret_cast<UI::ImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::ImageDescription::Construct();
}
extern "C" void* RtUi_ImageDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::ImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::ImageDescription::Reconstruct();
}
extern "C" void RtUi_ImageDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::ImageDescription*>(selfv)                      // B.3.6 bridge
        ->UI::ImageDescription::Destruct();
}
extern "C" bool RtUi_ImageDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::ImageDescription*>(selfv)               // B.3.6 bridge
        ->UI::ImageDescription::Serialize(streamv);
}
// 0x180118570: RCX = this, RDX = &caller's InternalPtr<ImageElement>.
extern "C" void RtUi_ImageDescription_ApplyToElement(const void* selfv, void** elemPtr) {
    reinterpret_cast<const UI::ImageDescription*>(selfv)                // B.3.6 bridge
        ->UI::ImageDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::ImageElement>*>(elemPtr));
}
// Slot 8: RCX = this, RDX = the caller's slot (R8, the Dialog*, is unread).
extern "C" void** RtUi_ImageDescription_CreateInstance(const void* selfv, void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::ImageDescription*>(selfv)            // B.3.6 bridge
            ->UI::ImageDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1ImageDescription@UI@@UEAA@XZ   0x180118370   — a DTOR DOOR (B4b-2)
// ============================================================================
// TWO String releases, both with retail's asymmetry: the sprite name at +0x70
// under this class's own vtable, then ElementDescription's _textName at +0x60
// under the base vtable. Tail JMP to ??1External@Object@@ at 0x1801183f6.
extern "C" void RtUi_ImageDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtImageDescription();                                 // 18011838d
    ReleaseStringField(p, 0x70);                               // 180118394..1801183b2
    *v = VtElementDescription();                               // 1801183c1
    ReleaseStringField(p, 0x60);                               // 1801183c8..1801183e6

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1801183f6 JMP
}

int RtUiImageDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ImageDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ImageDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180035cb0ull, (void*)&RtUi_ImageDescription_Type);

    RegisterRuntimeExport("?TypeName@ImageDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ImageDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180035cd0ull, (void*)&RtUi_ImageDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ImageDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ImageDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1801184a0ull, (void*)&RtUi_ImageDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ImageDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180036010ull, (void*)&RtUi_ImageDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ImageDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180035f60ull, (void*)&RtUi_ImageDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ImageDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ImageDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x1800360c0ull, (void*)&RtUi_ImageDescription_Destruct);

    RegisterRuntimeExport("??1ImageDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ImageDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180118370ull, (void*)&RtUi_ImageDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ImageDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1801184b0ull, (void*)&RtUi_ImageDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ImageDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180118910ull, (void*)&RtUi_ImageDescription_CreateInstance);

    // ADDRESS ONLY, as before: the export name resolves to this address.
    RegisterRuntimeAddr(0x180118570ull, (void*)&RtUi_ImageDescription_ApplyToElement); ++n;

    return n;
}
