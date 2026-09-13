// ============================================================================
// runtime/src/ui/textdescription.cpp — UI::TextDescription, the whole class
// ============================================================================
//
// The cutover boot's wall after the UI::*Description first half landed.
//
// ??_7TextDescription@UI@@6B@ (retail 0x180156be0), ten slots:
//   0  deleting dtor      0x180041e30  deleting_dtor.cpp row (sizeof 0x90)
//   1  TypeName           0x180041ad0  here
//   2  Type               0x180041ab0  here
//   3  GetFileVersion     0x18011ee20  here  (returns 0x304 — per class)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x18011ee30  here
//   6  Reconstruct        0x180041c50  here
//   7  Destruct           0x180041c80  here
//   8  CreateInstance     0x18011f3d0  here
//   9  Construct          0x180041c70  here
// plus ??1TextDescription@UI@@UEAA@XZ 0x18011eae0 and the applier
// ?CreateInstance@TextDescription@UI@@ (0x18011ef70), which the shipped binary
// keeps as its own function — exported, so it is registered by NAME as well as
// by address.
//
// ??0TextElement@UI@@QEAA@XZ (0x18011eb80) WAS ported here and is not any more:
// runtime/src/ui/element_bodies_b5.cpp owns it (vtable batch 5), and this file
// reaches it by its export name so there is exactly one registration per
// address. `?GetIndexFromName@StringTable@UI@@` went the same way at the same
// merge -- runtime/src/ui/sprite.cpp has owned it since Wave 9, and the grep
// that missed it looked for a RegisterRuntimeAddr on 0x180002470 when sprite.cpp
// registers it by NAME only. A second body under one name is the duplicate the
// two merge scans exist to catch.
//
// THE CONSTRUCTOR IS OUT OF LINE HERE. Unlike Group/Ribbon/Image, whose ctor
// bodies are inlined into Construct/Reconstruct, TextDescription's is a real
// function at 0x18011ea20 that both Construct (`JNZ 0x18011ea20`, a TAIL jump)
// and Reconstruct (`JMP 0x18011ea20`) reach, and it IS exported
// (??0TextDescription@UI@@QEAA@XZ), so it is registered here by both doors.
// runtime/src/ui/ui_factory_wave28.cpp carried an inlined copy plus its own
// registration pair for that address; at the merge (2026-09-12) its Ctor_ became
// a one-line forwarder to the body below and its two registration lines were
// deleted, so there is exactly one registration per address.
//
// LAYOUT, sizeof 0x90 (`MOV EDX,0x90` at 0x180041e4f, the eh_vector stride in
// the deleting destructor). `ElementDescription` (0x68) plus:
//     +0x68  ExternalPtr<UI::Font>    serialized by 0x18003f1e0
//     +0x70  ExternalPtr<UI::StringTable>   serialized by 0x1800042a0
//     +0x78  System::String           the string NAME, 0x1800c9930
//     +0x80  uint32   _u80            kScalarUInt (vt+0x98), NOT kScalarInt
//     +0x84  bool     +0x85 bool      ctor: ONE 16-bit store covering both
//     +0x86  (2 bytes never written by the ctor and never read here)
//     +0x88  int32    _i88   ctor 0xc8
//     +0x8c  int32    _i8c   ctor -1   — serialized CONDITIONALLY, see below
//
// The three scalar names are deliberately `_u80`/`_b84`/`_b85`/`_i88`/`_i8c`:
// Serialize proves their WIDTHS and CreateInstance proves where they land on
// the element, but neither body says what any of them MEAN.
//
// NOT HERE: `?CreateNewObject@TextDescription@UI@@` / `?CreateInPlaceObject@`
// are already ported and registered, table-driven, by
// runtime/src/ui/factory_ui1.cpp.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/Element.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtTextDescription()    { return RT_DATA_SLOT(void*, "??_7TextDescription@UI@@6B@"); }
const uint8_t* TextDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@TextDescription@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// The same two-field views the sibling *Description files carry: these bodies
// read exactly `Stream::mode` (+0x08) and `Stream::encodingFactory` (+0x10),
// and the {stream, readerWriter} PAIR must live on this frame because
// `LEA RDX,[RSP+0x28]` is what every callee receives.
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

}  // namespace

// ============================================================================
// ??0TextDescription@UI@@QEAA@XZ   retail 0x18011ea20 — the out-of-line ctor
// ============================================================================
// Transcribed instruction for instruction from 0x18011ea31..0x18011eaca.
// Identical to every other ElementDescription subclass through +0x60, then
// seven stores of its own whose WIDTHS are not uniform.
//
// +0x86..+0x87 IS NEVER WRITTEN. Zeroing it would be a divergence.
extern "C" void* RtUi_TextDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18011ea31
    *v = VtExternal();                                          // 18011ea3b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18011ea50
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18011ea53
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18011ea57
    *v = VtElementDescription();                                // 18011ea5b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18011ea65
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18011ea69
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18011ea6d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18011ea71  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18011ea74  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18011ea7b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18011ea83
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18011ea87
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18011ea8b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18011ea8f
    *v = VtTextDescription();                                   // 18011ea93
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 18011ea9d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18011eaa1
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 18011eaa5
    *reinterpret_cast<int32_t*>(p + 0x80)  = 0;                 // 18011eaa9  DWORD
    *reinterpret_cast<uint16_t*>(p + 0x84) = 0;                 // 18011eaaf  WORD
    *reinterpret_cast<int32_t*>(p + 0x88)  = 0xc8;              // 18011eab6
    *reinterpret_cast<int32_t*>(p + 0x8c)  = -1;                // 18011eac0
    return p;                                                   // 18011eaca
}

extern "C" uint32_t RtUi_TextDescription_Type(const void*) {
    const uint8_t* ti = TextDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_TextDescription_TypeName(const void*) {
    const uint8_t* ti = TextDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x18011ee20: `MOV EAX,0x304 ; RET`. 0x304, not the 0x204 the Image/Ribbon
// siblings return — the file version is per class and must not be shared.
extern "C" int32_t RtUi_TextDescription_GetFileVersion(const void*) { return 0x304; }

// 0x180041c70: `TEST RCX,RCX ; JNZ ctor ; XOR EAX,EAX ; RET` — a TAIL jump, so
// the ctor's return value (this) is Construct's return value.
extern "C" void* RtUi_TextDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180041c70
    return RtUi_TextDescription_Ctor(selfv);
}
// 0x180041c50: slot 0 with flag 0, then a TAIL jump to the ctor.
extern "C" void* RtUi_TextDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 180041c5e
    return RtUi_TextDescription_Ctor(selfv);                   // 180041c68
}
// 0x180041c80 is three instructions: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
extern "C" void RtUi_TextDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1TextDescription@UI@@UEAA@XZ   0x18011eae0
// ============================================================================
// TWO String releases, both with retail's asymmetry (field moved to `data-8`
// and freed on the last reference, NULLED otherwise): this class's own name
// String at +0x78 under its own vtable, then ElementDescription's _textName at
// +0x60 under the base vtable. Tail JMP to ??1External@Object@@ at 0x18011eb66.
//
// THE TWO ExternalPtrs AT +0x68 AND +0x70 ARE NOT RELEASED. That is retail's,
// transcribed, not an omission — the same asymmetry ~ImageDescription has.
extern "C" void RtUi_TextDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    using MemFreeFn = void (*)(void*);

    *v = VtTextDescription();                                  // 18011eaf6

    char*& name = *reinterpret_cast<char**>(p + 0x78);
    if (name != nullptr) {                                     // 18011eb04
        --*reinterpret_cast<int32_t*>(name - 4);               // 18011eb09
        if (*reinterpret_cast<int32_t*>(name - 4) == 0) {      // 18011eb0c
            name = name - 8;                                   // 18011eb12
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(name);
        } else {
            name = nullptr;                                    // 18011eb22
        }
    }

    *v = VtElementDescription();                               // 18011eb2a

    char*& text = *reinterpret_cast<char**>(p + 0x60);
    if (text != nullptr) {                                     // 18011eb38
        --*reinterpret_cast<int32_t*>(text - 4);               // 18011eb3d
        if (*reinterpret_cast<int32_t*>(text - 4) == 0) {      // 18011eb40
            text = text - 8;                                   // 18011eb46
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(text);
        } else {
            text = nullptr;                                    // 18011eb56
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 18011eb66 JMP
}

// ============================================================================
// ?Serialize@TextDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x18011ee30
// ============================================================================
//   ElementDescription::Serialize(this, stream)      0x180116b00, ported
//   rw = encoding factory, read side or write side by stream->mode
//   ExternalPtr<UI::Font>::Serialize(&_font, &pair)        0x18003f1e0
//   ExternalPtr<UI::StringTable>::Serialize(&_tbl, &pair)  0x1800042a0
//   System::String::Serialize(&_name, &pair)               0x1800c9930
//   rw->vt[0x78](rw, &_b84, 0)   kScalarBool
//   rw->vt[0x78](rw, &_b85, 0)   kScalarBool
//   rw->vt[0xa0](rw, &_i88, 0)   kScalarInt
//   rw->vt[0x98](rw, &_u80, 0)   kScalarUInt   <- NOT in offset order, and
//                                                 NOT the same slot as _i88
//   if (rw->GetVersion() >= 1 || !encoding->vt[0]())
//       rw->vt[0xa0](rw, &_i8c, 0)   kScalarInt
//   if (rw) rw->vt[0](rw, 1)
//
// *** THIS BODY PASSES 1, NOT 0, AS THE VERSION TO THE ENCODING FACTORY. ***
// `MOV R8D,0x1` at 0x18011ee61, where Group/Ribbon/Image all have `XOR R8D,R8D`.
// R8 is the `int version` the Serializer ctor forwards to
// ??0Binary{Reader,Writer}@IO@@, and BinaryWriter WRITES it into the stream
// (types/io/Serializer.h). Copying the siblings' 0 here would stamp the wrong
// format version into every saved TextDescription.
//
// THE LAST FIELD IS CONDITIONAL. `_i8c` is serialized only when the reader's
// own version is >= 1, OR when the encoding's vtable slot 0 predicate says no.
// A port that emitted it unconditionally would desync every pre-version-1
// stream. The two are read in THAT order and the `||` short-circuits, which is
// why the predicate call sits inside the `if` and not before it.
extern "C" bool RtUi_TextDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 18011ee4a

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;       // Create{Reader,Writer}
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 1);  // R8D=1
    ser.readerWriter = rw;                                     // 18011ee7d

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(p + 0x68, &ser);        // 18011ee8b ExternalPtr<Font>
    RtAddr<FieldSerFn>(0x1800042a0ull)(p + 0x70, &ser);        // 18011ee99 ExternalPtr<StringTable>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x78, &ser);        // 18011eea7 String

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, p + 0x84, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, p + 0x85, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, p + 0x88, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p + 0x80, nullptr);

    using GetVersionFn = int32_t (*)(void* rw);
    const int32_t rwVersion =
        reinterpret_cast<GetVersionFn>(rvt[rt::io::rw_vtbl::kGetVersion / 8])(rw);  // 18011ef0e

    bool emitTail = (rwVersion >= 1);                          // 18011ef11 CMP EAX,1 / JGE
    if (!emitTail) {
        using EncPredFn = bool (*)(void* encoding);
        void** evt = VTableOf(stream->encodingFactory);        // 18011ef16
        emitTail = !reinterpret_cast<EncPredFn>(evt[0])(stream->encodingFactory);
    }
    if (emitTail) {
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x8c, nullptr);
    }

    if (rw != nullptr) {                                       // 18011ef41
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// THE APPLIER (0x18011ef70) IS NOT HERE.
// ============================================================================
// `?CreateInstance@TextDescription@UI@@QEBAXV?$InternalPtr@VTextElement@UI@@@
// Object@@PEBVDialog@2@@Z` -- the TextElement-returning overload the vtable
// slot below calls -- is ported and registered by
// runtime/src/ui/element_bodies_b5.cpp (vtable batch 5, which owns
// UI::TextElement). This file had a second copy of it; it was dropped at the
// merge (2026-09-12) so there is one registration per address, and the slot
// below reaches it through ResolveAddr, which is registry-first.
// ============================================================================

// ============================================================================
// ?CreateInstance@TextDescription@UI@@...   retail 0x18011f3d0  (vtable slot 8)
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*.
//
// The allocation is the MemoryAllocator::Alloc wrapper (0x1800c7a50) INLINED —
// the shipped body enters the same critical section at `g_heap+0x640`, calls
// the same 0x1800c6e60 with (0x100, 8), and leaves it. Calling the wrapper is
// the same sequence and keeps the one allocator door this tree already has.
extern "C" void** RtUi_TextDescription_CreateInstanceElement(const void* selfv, void** ret,
                                                      const void* dialog) {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x100, 8);  // 18011f412 (inlined)

    void* elem = nullptr;
    if (raw != nullptr) {                                      // 18011f43d
        // ??0TextElement@UI@@QEAA@XZ, ported by
        // runtime/src/ui/element_bodies_b5.cpp and reached BY NAME so this
        // file opens no second door onto it.
        using TextElementCtorFn = void* (*)(void*);
        elem = RT_EXPORT_ONCE(TextElementCtorFn, "??0TextElement@UI@@QEAA@XZ")(raw);
    }

    void* slot = elem;                                         // 18011f44e
    using ApplyFn = void (*)(const void* self, void** elemPtr, const void* dlg);
    RtAddr<ApplyFn>(0x18011ef70ull)(selfv, &slot, dialog);     // 18011f45e

    *ret = elem;                                               // 18011f463
    return ret;
}

int RtUiTextDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TextDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_TextDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180041ab0ull, (void*)&RtUi_TextDescription_Type);

    RegisterRuntimeExport("?TypeName@TextDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TextDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180041ad0ull, (void*)&RtUi_TextDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@TextDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_TextDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18011ee20ull, (void*)&RtUi_TextDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TextDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180041c70ull, (void*)&RtUi_TextDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@TextDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TextDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180041c50ull, (void*)&RtUi_TextDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@TextDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_TextDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180041c80ull, (void*)&RtUi_TextDescription_Destruct);

    RegisterRuntimeExport("??0TextDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_TextDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011ea20ull, (void*)&RtUi_TextDescription_Ctor);

    RegisterRuntimeExport("??1TextDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_TextDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011eae0ull, (void*)&RtUi_TextDescription_Dtor);

    RegisterRuntimeExport("?Serialize@TextDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_TextDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x18011ee30ull, (void*)&RtUi_TextDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@TextDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_TextDescription_CreateInstanceElement);             ++n;
    RegisterRuntimeAddr(0x18011f3d0ull, (void*)&RtUi_TextDescription_CreateInstanceElement);

    return n;
}
