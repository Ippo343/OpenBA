// ============================================================================
// runtime/src/ui/animatedimagedescription.cpp — UI::AnimatedImageDescription
// ============================================================================
//
// The cutover boot's wall after UI::RibbonDescription:
//
//   [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
//   [RtMint]   vtable : ??_7AnimatedImageDescription@UI@@6B@
//   [RtMint]   slot   : 2
//   [RtMint]   target : ?Type@AnimatedImageDescription@UI@@UEBAIXZ
//
// ??_7AnimatedImageDescription@UI@@6B@ (retail 0x180156060), ten slots:
//   0  deleting dtor      0x1800369e0  deleting_dtor.cpp row (sizeof 0x98)
//   1  TypeName           0x1800366d0  here
//   2  Type               0x1800366b0  here
//   3  GetFileVersion     0x1800cb6a0  here  (returns 0x10204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800cb5c0  here
//   6  Reconstruct        0x180036850  here
//   7  Destruct           0x180036880  here
//   8  CreateInstance     0x1800cb480  here
//   9  Construct          0x180036870  here
// plus ??1AnimatedImageDescription@UI@@UEAA@XZ 0x1800cb6b0.
//
// THIS CLASS DERIVES FROM UI::ImageDescription, and the derivation is visible in
// three places rather than inferred from the name: the constructor stamps
// ??_7ImageDescription@UI@@6B@ and fills +0x68..+0x7f before stamping its own
// table (0x1800cb763..0x1800cb795); ~AnimatedImageDescription TAIL-JUMPS to
// ~ImageDescription; and its Serialize CALLS ImageDescription::Serialize.
// runtime/src/ui/imagedescription.cpp had to land first and did.
//
// LAYOUT, sizeof 0x98 (`MOV EDX,0x98` at 0x180036a12). ImageDescription (0x80)
// plus:
//     +0x80  Array<System::String>  the frame-name list
//     +0x88  _size    +0x8c  _capacity
//     +0x90  int32                  ctor: 0x1e
// The ctor's last store is `MOV dword [RCX+0x90],0x1e` at 0x1800cb795 — 30,
// not zero, and it is the only non-zero non-sentinel initialiser in the whole
// ElementDescription family. The field is NOT named here: Serialize proves it
// is one kScalarInt and CreateInstance proves it is copied to the element's
// +0xf0, and neither says what it means. One body is not enough to name a field.
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS OUT OF LINE HERE, UNLIKE EVERY OTHER CLASS IN THIS FAMILY.
// ----------------------------------------------------------------------------
// Group's, Ribbon's and Image's constructors are inlined into Construct; this
// one is a real exported function, `??0AnimatedImageDescription@UI@@QEAA@XZ`
// (0x1800cb6f0), and `Construct` (0x180036870) is a four-instruction thunk:
//     TEST RCX,RCX ; JNZ 0x1800cb6f0 ; XOR EAX,EAX ; RET
// It is ALREADY PORTED AND REGISTERED, table-driven, by
// runtime/src/ui/factory_ui1.cpp (the `kAnimatedImageDescription` row in
// factory_ui1_tables.inc names 0x1800cb6f0 as its ctorVa). So this file calls it
// through `ResolveAddr`, which is registry-first, instead of transcribing a
// fifth copy of the same 24 stores. That is the same decision
// runtime/src/ui/dialog_factory.cpp records for `??0Dialog@UI@@`.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

// The shared applier, ported next door and registered by address.
extern "C" void RtUi_ImageDescription_ApplyToElement(const void* selfv, void** elemPtr);

namespace {

void* VtAnimatedImageDescription() {
    return RT_DATA_SLOT(void*, "??_7AnimatedImageDescription@UI@@6B@");
}
const uint8_t* AnimatedImageDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@AnimatedImageDescription@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

struct Stream {
    uint8_t _head00[0x08];
    int32_t mode;
    int32_t _pad0c;
    void*   encodingFactory;
};
struct Serializer {
    Stream* stream;
    void*   readerWriter;
};
static_assert(offsetof(Stream, mode) == 0x08, "IO::Stream mode @ +0x08");
static_assert(offsetof(Stream, encodingFactory) == 0x10, "IO::Stream factory @ +0x10");
static_assert(offsetof(Serializer, readerWriter) == 0x08, "IO::Serializer rw @ +0x08");

void** VTableOf(void* o) { return *reinterpret_cast<void***>(o); }

using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// ??0AnimatedImageDescription@UI@@QEAA@XZ, 0x1800cb6f0 — see the banner.
void* CallCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return RtAddr<CtorFn>(0x1800cb6f0ull)(self);
}

}  // namespace

extern "C" uint32_t RtUi_AnimatedImageDescription_Type(const void*) {
    const uint8_t* ti = AnimatedImageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_AnimatedImageDescription_TypeName(const void*) {
    const uint8_t* ti = AnimatedImageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800cb6a0: `MOV EAX,0x10204 ; RET`. 0x10204 — a THIRD distinct value in
// this family (Group 0x104, Ribbon and Image 0x204). The high half is not
// shared with the base's.
extern "C" int32_t RtUi_AnimatedImageDescription_GetFileVersion(const void*) { return 0x10204; }

// 0x180036870: `TEST RCX,RCX ; JNZ ctor ; XOR EAX,EAX ; RET` — a tail jump, so
// the ctor's return value (this) IS this function's return value.
extern "C" void* RtUi_AnimatedImageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180036870
    return CallCtor(selfv);                                    // 180036873 JNZ
}
// 0x180036850: slot0(this, 0), then a TAIL JUMP to the same ctor.
extern "C" void* RtUi_AnimatedImageDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 18003685e
    return CallCtor(selfv);                                    // 180036868 JMP
}
// 0x180036880: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_AnimatedImageDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1AnimatedImageDescription@UI@@UEAA@XZ   0x1800cb6b0
// ============================================================================
// Three instructions of its own and then the base:
//     stamp ??_7AnimatedImageDescription@UI@@6B@             ; 1800cb6cd
//     Array<String>::~Array(this + 0x80)                     ; 1800cb6d4
//     JMP ~ImageDescription                                  ; 1800cb6e2
// `SUB RCX,-0x80` at 0x1800cb6d0 is `ADD RCX,0x80` written as a subtraction of
// a sign-extended byte -- a code-size trick, not an offset of -0x80.
//
// 0x180010ab0 is the Array destructor that ALSO destroys its elements (it is
// array_destroy.cpp's J2 entry point, already ported and registered); the plain
// Array::~Array the other classes in this family use is 0x180039440. Using the
// wrong one here would leak a System::String per frame name.
extern "C" void RtUi_AnimatedImageDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);

    *reinterpret_cast<void**>(p) = VtAnimatedImageDescription();   // 1800cb6cd

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(p + 0x80);                 // 1800cb6d4

    using BaseDtorFn = void (*)(void*);
    RtAddr<BaseDtorFn>(0x180118370ull)(p);                         // 1800cb6e2 JMP
}

// ============================================================================
// ?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800cb5c0
// ============================================================================
//   ImageDescription::Serialize(this, stream)          0x1801184b0, ported here
//   rw = encoding factory, read or write side by stream->mode
//   n = _frames._size ; rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) Array<String>::SetSize(&_frames, n)  0x180029a80
//   Array<String> element walk (&pair, n, _frames._data)             0x180029bb0
//   rw->vt[0xa0](rw, &_i90, 0)
//   if (rw) rw->vt[0](rw, 1)
//
// The int at +0x90 is serialized AFTER the array, not before. 0x180029a80 is
// array.cpp's kSpec_StringRefZ (the two-argument Array<System::String> whose new
// tail is ZEROED) and 0x180029bb0 is the text-codec string-array walker ported
// in runtime/src/io/text_codec.cpp; both are registered unconditionally.
extern "C" bool RtUi_AnimatedImageDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x1801184b0ull)(p, stream);              // 1800cb5da

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800cb60a

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);

    int32_t n = *reinterpret_cast<int32_t*>(p + 0x88);         // 1800cb60f
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 1800cb62f (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x180029a80ull)(p + 0x80, n);        // 1800cb645
    }

    using StrArrayFn = void (*)(Serializer* ser, int count, void* data);
    RtAddr<StrArrayFn>(0x180029bb0ull)(&ser, n, *reinterpret_cast<void**>(p + 0x80));

    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x90, nullptr);

    if (rw != nullptr) {                                       // 1800cb67d
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@AnimatedImageDescription@UI@@...   retail 0x1800cb480
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot. As with
// ImageDescription's, R8 (the Dialog*) is never read: this class creates no
// child elements.
//
// AnimatedImageElement is 0x100 bytes (`MOV EDX,0x100` at 0x1800cb4c1) and its
// constructor is CALLED, not inlined: `??0AnimatedImageElement@UI@@QEAA@XZ`
// (0x1800cb3f0), already ported and registered by factory_ui1.cpp. Note the
// ctor RETURNS `this` and retail keeps the returned RAX (0x1800cb4f8), rather
// than reusing the allocation pointer.
//
// Then the shared applier (0x180118570), then the per-frame table:
//     Array<{SpriteSheet*, int}>::SetSize(&elem->+0xe0, _frames._size)
//                                                        ; 0x1800370c0
//     for i in [0, _frames._size):
//         sheet = this->_spriteSheet                     ; RE-READ each iteration
//         idx   = sheet ? GetSpriteIndexFromName(sheet, &_frames._data[i]) : -1
//         entry = elem->+0xe0 data + i*0x10
//         entry[0] = sheet ; entry[1] = idx              ; QWORD, see below
//     elem->+0xf0 = this->+0x90
//
// THE SAME QWORD-FROM-A-DWORD-SLOT AS THE APPLIER. 0x1800cb54f and 0x1800cb555
// write `dword [RSP+0x30]`; 0x1800cb568 reads `qword [RSP+0x30]` and 0x1800cb56d
// stores all eight bytes at entry+0x08. Reproduced with an uninitialised local
// for the same reason the applier's is: the WIDTH is the faithful part.
//
// `self->_spriteSheet` IS RE-READ INSIDE THE LOOP (0x1800cb530), not hoisted.
extern "C" void** RtUi_AnimatedImageDescription_CreateInstance(const void* selfv, void** ret) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x100, 8);  // 1800cb4c1

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 1800cb4eb
        using ElemCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<ElemCtorFn>(0x1800cb3f0ull)(raw));          // 1800cb4f3, keeps RAX
    }

    void* slot = elem;                                         // 1800cb500
    RtUi_ImageDescription_ApplyToElement(self, &slot);         // 1800cb50d

    const int32_t frames = *reinterpret_cast<const int32_t*>(self + 0x88);

    using SetSizeFn = void (*)(void* array, int n);
    RtAddr<SetSizeFn>(0x1800370c0ull)(elem + 0xe0, frames);    // 1800cb51f

    using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
    for (int32_t i = 0; i < frames; ++i) {                     // 1800cb524 / 1800cb578
        void* sheet = *reinterpret_cast<void* const*>(self + 0x68);   // 1800cb530, re-read

        uint64_t idxSlot;                                      // [RSP+0x30], see the note
        if (sheet != nullptr) {                                // 1800cb534
            void* const* names = *reinterpret_cast<void* const* const*>(self + 0x80);
            const int32_t idx = RT_EXPORT_ONCE(
                GetIdxFn,
                "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z")(
                    sheet, &names[i]);                         // 1800cb54a
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);      // 1800cb54f  DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);   // 1800cb555
        }

        uint8_t* table = *reinterpret_cast<uint8_t**>(elem + 0xe0);   // 1800cb55d
        uint8_t* entry = table + static_cast<size_t>(i) * 0x10;
        *reinterpret_cast<void**>(entry)         = sheet;      // 1800cb564
        *reinterpret_cast<uint64_t*>(entry + 8)  = idxSlot;    // 1800cb56d  QWORD store
    }

    *reinterpret_cast<int32_t*>(elem + 0xf0) =
        *reinterpret_cast<const int32_t*>(self + 0x90);        // 1800cb586

    *ret = elem;                                               // 1800cb58c
    return ret;
}

int RtUiAnimatedImageDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@AnimatedImageDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_AnimatedImageDescription_Type);            ++n;
    RegisterRuntimeAddr(0x1800366b0ull, (void*)&RtUi_AnimatedImageDescription_Type);

    RegisterRuntimeExport("?TypeName@AnimatedImageDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_AnimatedImageDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x1800366d0ull, (void*)&RtUi_AnimatedImageDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@AnimatedImageDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_AnimatedImageDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800cb6a0ull, (void*)&RtUi_AnimatedImageDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180036870ull, (void*)&RtUi_AnimatedImageDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@AnimatedImageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180036850ull, (void*)&RtUi_AnimatedImageDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@AnimatedImageDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_AnimatedImageDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180036880ull, (void*)&RtUi_AnimatedImageDescription_Destruct);

    RegisterRuntimeExport("??1AnimatedImageDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_AnimatedImageDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800cb6b0ull, (void*)&RtUi_AnimatedImageDescription_Dtor);

    RegisterRuntimeExport("?Serialize@AnimatedImageDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_AnimatedImageDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800cb5c0ull, (void*)&RtUi_AnimatedImageDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@AnimatedImageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_AnimatedImageDescription_CreateInstance);            ++n;
    RegisterRuntimeAddr(0x1800cb480ull, (void*)&RtUi_AnimatedImageDescription_CreateInstance);

    return n;
}
