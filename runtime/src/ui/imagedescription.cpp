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
// so this file has to land first. Its own vtable is all-traps too and would
// have been the next wall regardless.
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
// plus ??1ImageDescription@UI@@UEAA@XZ 0x180118370 and the UNEXPORTED shared
// applier 0x180118570, which is registered BY ADDRESS ONLY (it has no export
// name; AnimatedImageDescription::CreateInstance reaches it by rel32 and our
// port reaches it through ResolveAddr, which is registry-first).
//
// LAYOUT, sizeof 0x80 (`MOV EDX,0x80` at 0x18003620f, the eh_vector stride in
// the deleting destructor). `ElementDescription` (0x68) plus:
//     +0x68  ExternalPtr<UI::SpriteSheet>   serialized by 0x180004590
//     +0x70  System::String                 the sprite NAME, 0x1800c9930
//     +0x78  int32   +0x7c  int32           two kScalarInt fields
// The ctor writes +0x78 and +0x7c with ONE qword of -1
// (`MOV qword [RCX+0x78],-1` at 0x18003609e), so both start at -1.
//
// THE +0x68 ExternalPtr IS NOT RELEASED BY THE DESTRUCTOR. ~ImageDescription
// releases the String at +0x70 and the ElementDescription String at +0x60 and
// nothing else. That is retail's, transcribed, not an omission.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtImageDescription()   { return RT_DATA_SLOT(void*, "??_7ImageDescription@UI@@6B@"); }
void* VtImageElement()       { return RT_DATA_SLOT(void*, "??_7ImageElement@UI@@6B@"); }
const uint8_t* ImageDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ImageDescription@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;              // +0x08  0 == reading
    int32_t _pad0c;
    void*   encodingFactory;   // +0x10
};
struct Serializer {
    Stream* stream;            // +0x00
    void*   readerWriter;      // +0x08
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// The inlined constructor, transcribed from 0x18003602a..0x18003609e. Identical
// to every other ElementDescription subclass through +0x60, then three stores
// of its own. It appears in FOUR places in the shipped DLL (Construct,
// Reconstruct, the two factory steps, and again inside
// ??0AnimatedImageDescription) and is written once here.
void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18003602a
    *v = VtExternal();                                          // 180036034
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 180036049
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18003604c
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 180036050
    *v = VtElementDescription();                                // 180036054
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
    *v = VtImageDescription();                                  // 18003608c
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 180036096
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18003609a
    *reinterpret_cast<int64_t*>(p + 0x78)  = -1;                // 18003609e  ONE QWORD
}

}  // namespace

extern "C" uint32_t RtUi_ImageDescription_Type(const void*) {
    const uint8_t* ti = ImageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ImageDescription_TypeName(const void*) {
    const uint8_t* ti = ImageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ImageDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_ImageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180036021
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;
}
extern "C" void* RtUi_ImageDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 180035f77
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;
}
// 0x1800360c0: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_ImageDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1ImageDescription@UI@@UEAA@XZ   0x180118370
// ============================================================================
// TWO String releases, both with retail's asymmetry (field moved to `data-8`
// and freed on the last reference, NULLED otherwise): the sprite name at +0x70
// under this class's own vtable, then ElementDescription's _textName at +0x60
// under the base vtable. Tail JMP to ??1External@Object@@ at 0x1801183f6.
extern "C" void RtUi_ImageDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    using MemFreeFn = void (*)(void*);

    *v = VtImageDescription();                                 // 18011838d

    char*& sprite = *reinterpret_cast<char**>(p + 0x70);
    if (sprite != nullptr) {                                   // 180118394
        --*reinterpret_cast<int32_t*>(sprite - 4);             // 180118399
        if (*reinterpret_cast<int32_t*>(sprite - 4) == 0) {    // 18011839c
            sprite = sprite - 8;                               // 1801183a2
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(sprite);
        } else {
            sprite = nullptr;                                  // 1801183b2
        }
    }

    *v = VtElementDescription();                               // 1801183c1

    char*& text = *reinterpret_cast<char**>(p + 0x60);
    if (text != nullptr) {                                     // 1801183c8
        --*reinterpret_cast<int32_t*>(text - 4);               // 1801183cd
        if (*reinterpret_cast<int32_t*>(text - 4) == 0) {      // 1801183d0
            text = text - 8;                                   // 1801183d6
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(text);
        } else {
            text = nullptr;                                    // 1801183e6
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1801183f6 JMP
}

// ============================================================================
// ?Serialize@ImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801184b0
// ============================================================================
//   ElementDescription::Serialize(this, stream)        0x180116b00, ported
//   rw = encoding factory, read or write side by stream->mode
//   ExternalPtr<SpriteSheet>::Serialize(&_spriteSheet, &pair)   0x180004590
//   System::String::Serialize(&_spriteName, &pair)              0x1800c9930
//   rw->vt[0xa0](rw, &_i78, 0)                                  kScalarInt
//   rw->vt[0xa0](rw, &_i7c, 0)                                  kScalarInt
//   if (rw) rw->vt[0](rw, 1)
//
// NOTE THE TWO FIELD SERIALIZERS TAKE THE PAIR, NOT THE READER: both are
// `LEA RDX,[RSP+0x28]` (0x180118503, 0x180118511), the {stream, readerWriter}
// pair, which is why it has to live on this frame.
//
// THIS BODY HAS NO `if (reading)` ARM AND NO ARRAY. Unlike Group/Ribbon there is
// nothing to resize; every field is fixed.
extern "C" bool RtUi_ImageDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 1801184ca

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1801184fa

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x68, &ser);        // 180118508 ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x70, &ser);        // 180118516 String

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x78, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x7c, nullptr);

    if (rw != nullptr) {                                       // 18011854b
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// The shared applier, retail 0x180118570 — UNEXPORTED, address-only.
// ============================================================================
// `void Apply(const ImageDescription* self, InternalPtr<Element>* elemPtr)`.
// Called by BOTH ImageDescription::CreateInstance (0x1801189c8) and
// AnimatedImageDescription::CreateInstance (0x1800cb50d), which is why it is a
// separate function in the shipped binary and is one here.
//
//   local = *elemPtr                                    ; 18011857f/18011858a
//   ElementDescription::CreateInstance(self, &local)     ; 180118592
//   sheet = self->_spriteSheet                           ; 180118597
//   idx   = sheet ? SpriteSheet::GetSpriteIndexFromName(sheet, &self->_spriteName)
//                 : -1                                  ; 1801185a7 / 1801185b2
//   e = *elemPtr        (RE-READ, not the local)         ; 1801185ba
//   e->+0xc0 = sheet ; e->+0xc8 = idx
//   e = *elemPtr ; e->+0xd4 = self->+0x78
//   e = *elemPtr ; e->+0xd8 = self->+0x7c
//
// *** TWO THINGS HERE ARE DELIBERATE AND BOTH LOOK LIKE BUGS. ***
//
// 1. `*elemPtr` IS RE-READ THREE TIMES, from the CALLER's slot, after
//    ElementDescription::CreateInstance has run on a LOCAL COPY of it. Folding
//    the three re-reads into one variable would assert that nothing can change
//    the caller's slot between them. Retail does not assert that, and this port
//    does not either.
//
// 2. THE INDEX IS STORED AS A QWORD READ FROM A SLOT ONLY A DWORD WAS WRITTEN
//    TO. 0x1801185ac and 0x1801185b2 write `dword [RSP+0x28]`; 0x1801185bd
//    reads `qword [RSP+0x28]` and 0x1801185ce stores all eight bytes to
//    `e->+0xc8`. So `e->+0xcc` receives four bytes of whatever was on retail's
//    stack. That is reproduced here with an uninitialised local rather than
//    "cleaned up" to a 4-byte store: a 4-byte store would leave +0xcc holding
//    the ImageElement constructor's own value, and this body's whole observable
//    effect on +0xcc is that it overwrites it with something arbitrary. Our
//    arbitrary value is ours, not retail's; what is faithful is the WIDTH.
extern "C" void RtUi_ImageDescription_ApplyToElement(const void* selfv, void** elemPtr) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    void* local = *elemPtr;                                    // 18011857f / 18011858a
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &local);  // 180118592

    void* sheet = *reinterpret_cast<void* const*>(self + 0x68);

    // See note 2 above: written 4 bytes wide, read 8.
    uint64_t idxSlot;                                          // [RSP+0x28]
    if (sheet != nullptr) {
        using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
        const int32_t idx = RT_EXPORT_ONCE(
            GetIdxFn,
            "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z")(
                sheet, self + 0x70);
        __builtin_memcpy(&idxSlot, &idx, sizeof idx);          // 1801185ac  DWORD store
    } else {
        const int32_t minusOne = -1;
        __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);// 1801185b2  DWORD store
    }

    uint8_t* e = static_cast<uint8_t*>(*elemPtr);              // 1801185ba
    *reinterpret_cast<void**>(e + 0xc0)   = sheet;             // 1801185c2
    *reinterpret_cast<uint64_t*>(e + 0xc8) = idxSlot;          // 1801185ce  QWORD store

    e = static_cast<uint8_t*>(*elemPtr);                       // 1801185d5
    *reinterpret_cast<int32_t*>(e + 0xd4) =
        *reinterpret_cast<const int32_t*>(self + 0x78);        // 1801185db

    e = static_cast<uint8_t*>(*elemPtr);                       // 1801185e1
    *reinterpret_cast<int32_t*>(e + 0xd8) =
        *reinterpret_cast<const int32_t*>(self + 0x7c);        // 1801185ec
}

// ============================================================================
// ?CreateInstance@ImageDescription@UI@@...   retail 0x180118910
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot. NOTE there is
// NO Dialog parameter on this one -- unlike Group's and Ribbon's, this body
// creates no children, so R8 is never read. The mangled name still carries
// `PEBVDialog@2@` because the virtual's signature is fixed by the base.
//
// ImageElement is 0xe0 bytes (`MOV EDX,0xe0` at 0x180118951). Its constructor is
// INLINED here rather than called: ??0Element (0x180115e50), then the
// ImageElement vtable, then four stores whose widths are not uniform:
//     0x180118991  MOV qword  [RDI+0xc0],0
//     0x18011899c  MOV dword  [RDI+0xc8],0xffffffff
//     0x1801189a6  MOV dword  [RDI+0xd0],0
//     0x1801189b0  MOV qword  [RDI+0xd4],-1      <- UNALIGNED qword at +0xd4,
//                                                   covering +0xd4 AND +0xd8
extern "C" void** RtUi_ImageDescription_CreateInstance(const void* selfv, void** ret) {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xe0, 8);   // 180118951

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 180118979
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 180118981 ??0Element
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtImageElement();       // 18011898e
        *reinterpret_cast<uint64_t*>(e + 0xc0) = 0;            // 180118991
        *reinterpret_cast<int32_t*>(e + 0xc8)  = -1;           // 18011899c  DWORD
        *reinterpret_cast<int32_t*>(e + 0xd0)  = 0;            // 1801189a6  DWORD
        __builtin_memcpy(e + 0xd4, "\xff\xff\xff\xff\xff\xff\xff\xff", 8);  // 1801189b0
        elem = e;
    }

    void* slot = elem;                                         // 1801189bb
    RtUi_ImageDescription_ApplyToElement(selfv, &slot);        // 1801189c8

    *ret = elem;                                               // 1801189cd
    return ret;
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

    // ADDRESS ONLY: the applier has no export name.
    RegisterRuntimeAddr(0x180118570ull, (void*)&RtUi_ImageDescription_ApplyToElement); ++n;

    return n;
}
