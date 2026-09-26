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
// ENDGAME B.3.6 block C3: every slot body is a `UI::TextDescription` member
// (runtime/types/ui/TextDescription.h, which now carries the layout this file
// used to spell as raw offsets — RULE B6-8). The registered doors keep their
// names and addresses and delegate with a QUALIFIED call: a live
// TextDescription wears the MINTED table. The ctor and dtor stay door bodies
// (no real ctor on a polymorphic class, B4b-2).
//
// ??0TextElement@UI@@QEAA@XZ (0x18011eb80) is owned by
// runtime/src/ui/element_bodies_b5.cpp and reached here by its export name so
// there is exactly one registration per address. The applier 0x18011ef70 is
// owned by element_bodies_b5.cpp too (declared on the class, undefined until
// the fragment pass) and reached by address.
//
// THE CONSTRUCTOR IS OUT OF LINE HERE. Unlike Group/Ribbon/Image, whose ctor
// bodies are inlined into Construct/Reconstruct, TextDescription's is a real
// function at 0x18011ea20 that both Construct (`JNZ 0x18011ea20`, a TAIL jump)
// and Reconstruct (`JMP 0x18011ea20`) reach, and it IS exported
// (??0TextDescription@UI@@QEAA@XZ), so it is registered here by both doors.
// runtime/src/ui/ui_factory_wave28.cpp's Ctor_ forwards to the body below.
//
// NOT HERE: `?CreateNewObject@TextDescription@UI@@` / `?CreateInPlaceObject@`
// are ported and registered, table-driven, by runtime/src/ui/ui_factory_wave28.cpp.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "element_common.h"   // rtuielem::AtomicExport (RULE B6-5), ResolveAddr
#include "../../types/ui/TextDescription.h"
#include "../../types/io/Serializer.h"

namespace {

void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtTextDescription()    { return RT_DATA_SLOT(void*, "??_7TextDescription@UI@@6B@"); }

// RULE B6-5: the members below resolve through namespace-scope atomics, never
// a function-local first-call cache (UI is built on the loader thread).
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_textElementCtorSlot{nullptr};

const uint8_t* TextDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(rtuielem::AtomicExport(
        g_typeInfoSlot, "?_typeInfo@TextDescription@UI@@0VBaseTypeInfo@Object@@A"));
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

}  // namespace

// ============================================================================
// ??0TextDescription@UI@@QEAA@XZ   retail 0x18011ea20 — the out-of-line ctor
// ============================================================================
// A DOOR BODY, not a member (B4b-2: no real ctor on a polymorphic class).
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
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 18011ea9d  _font
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18011eaa1  _stringTable2
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 18011eaa5  _name
    *reinterpret_cast<int32_t*>(p + 0x80)  = 0;                 // 18011eaa9  _u80 DWORD
    *reinterpret_cast<uint16_t*>(p + 0x84) = 0;                 // 18011eaaf  _b84/_b85 WORD
    *reinterpret_cast<int32_t*>(p + 0x88)  = 0xc8;              // 18011eab6  _i88
    *reinterpret_cast<int32_t*>(p + 0x8c)  = -1;                // 18011eac0  _i8c
    return p;                                                   // 18011eaca
}

namespace UI {

// 0x180041ab0 / 0x180041ad0: the first dword and the +8 qword of the minted
// BaseTypeInfo. TypeName is the KEY FUNCTION, so GCC emits this class's table
// here; nothing stamps it (every live one wears the minted table).
uint32_t TextDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, TextDescriptionTypeInfo(), sizeof v); return v;
}
const char* TextDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, TextDescriptionTypeInfo() + 8, sizeof v); return v;
}
// 0x18011ee20: `MOV EAX,0x304 ; RET`. 0x304, not the 0x204 the Image/Ribbon
// siblings return — the file version is per class and must not be shared.
int32_t TextDescription::GetFileVersion() const { return 0x304; }

// 0x180041c70: `TEST RCX,RCX ; JNZ ctor ; XOR EAX,EAX ; RET` — a TAIL jump, so
// the ctor's return value (this) is Construct's. The null test is in the DOOR.
Object::External* TextDescription::Construct() {
    RtUi_TextDescription_Ctor(this);
    return this;
}
// 0x180041c50: slot 0 with flag 0, then a TAIL jump to the ctor.
Object::External* TextDescription::Reconstruct() {
    DeletingDtor(0);                                           // 180041c5e
    RtUi_TextDescription_Ctor(this);                           // 180041c68
    return this;
}
// 0x180041c80 is three instructions: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`.
void TextDescription::Destruct() { DeletingDtor(0); }

// ~TextDescription is declared and deliberately NOT defined: an empty body
// would chain to `~ElementDescription`, which block A leaves undefined in the
// real build (its body is a dialog.cpp door). Nothing destroys a
// TextDescription as a C++ object; the registered dtor door below keeps
// retail's two releases and the chain to ~External.

// ============================================================================
// ?Serialize@TextDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x18011ee30
// ============================================================================
//   ElementDescription::Serialize(this, stream)      0x180116b00 (qualified)
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
// ??0Binary{Reader,Writer}@IO@@, and BinaryWriter WRITES it into the stream.
//
// THE LAST FIELD IS CONDITIONAL. `_i8c` is serialized only when the reader's
// own version is >= 1, OR when the encoding's vtable slot 0 predicate says no.
// The two are read in THAT order and the `||` short-circuits.
bool TextDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 18011ee4a

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;       // Create{Reader,Writer}
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 1);  // R8D=1
    ser.readerWriter = rw;                                     // 18011ee7d

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(&_font, &ser);          // 18011ee8b ExternalPtr<Font>
    RtAddr<FieldSerFn>(0x1800042a0ull)(&_stringTable2, &ser);  // 18011ee99 ExternalPtr<StringTable>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_name, &ser);          // 18011eea7 String

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_b84, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_b85, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_i88, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, &_u80, nullptr);

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
        reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_i8c, nullptr);
    }

    if (rw != nullptr) {                                       // 18011ef41
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@TextDescription@UI@@...   retail 0x18011f3d0  (vtable slot 8)
// ============================================================================
// RETSLOT (RULE B6-2): `ret` is the caller's InternalPtr<Element>, written with
// the raw element pointer and handed back.
//
// The allocation is the MemoryAllocator::Alloc wrapper (0x1800c7a50) INLINED —
// the shipped body enters the same critical section at `g_heap+0x640`, calls
// the same 0x1800c6e60 with (0x100, 8), and leaves it. Calling the wrapper is
// the same sequence and keeps the one allocator door this tree already has.
//
// The applier 0x18011ef70 is the non-virtual `CreateInstance(InternalPtr<
// TextElement>, const Dialog*)`, whose body is element_bodies_b5.cpp's door:
// reached by address (registry-first) with the handle by ADDRESS (B6-11).
Object::InternalPtr<Element>* TextDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x100, 8);  // 18011f412 (inlined)

    void* elem = nullptr;
    if (raw != nullptr) {                                      // 18011f43d
        // ??0TextElement@UI@@QEAA@XZ — a ctor DOOR (element_bodies_b5.cpp).
        using TextElementCtorFn = void* (*)(void*);
        elem = reinterpret_cast<TextElementCtorFn>(rtuielem::AtomicExport(
            g_textElementCtorSlot, "??0TextElement@UI@@QEAA@XZ"))(raw);
    }

    void* slot = elem;                                         // 18011f44e
    using ApplyFn = void (*)(const void* self, void** elemPtr, const void* dlg);
    RtAddr<ApplyFn>(0x18011ef70ull)(this, &slot, dialog);      // 18011f45e

    *reinterpret_cast<void**>(ret) = elem;                     // 18011f463
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1TextDescription@UI@@UEAA@XZ   0x18011eae0 — a DOOR BODY (no real dtor)
// ============================================================================
// TWO String releases, both with retail's asymmetry (field moved to `data-8`
// and freed on the last reference, NULLED otherwise): this class's own name
// String at +0x78 under its own vtable, then ElementDescription's _textName at
// +0x60 under the base vtable. Tail JMP to ??1External@Object@@ at 0x18011eb66.
//
// THE TWO ExternalPtrs AT +0x68 AND +0x70 ARE NOT RELEASED. That is retail's,
// transcribed, not an omission — the same asymmetry ~ImageDescription has.
extern "C" void RtUi_TextDescription_Dtor(void* selfv) {
    UI::TextDescription* self = reinterpret_cast<UI::TextDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);

    using MemFreeFn = void (*)(void*);

    *v = VtTextDescription();                                  // 18011eaf6

    char*& name = self->_name;
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

    char*& text = self->_textName;
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
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(selfv);   // 18011eb66 JMP
}

// ============================================================================
// The registered doors: one QUALIFIED delegation each (a live TextDescription
// wears the MINTED table, so an unqualified call would re-enter the door).
// ============================================================================
extern "C" uint32_t RtUi_TextDescription_Type(const void* self) {
    return reinterpret_cast<const UI::TextDescription*>(self)->UI::TextDescription::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_TextDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::TextDescription*>(self)->UI::TextDescription::TypeName();   // B.3.6 bridge
}
extern "C" int32_t RtUi_TextDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::TextDescription*>(self)->UI::TextDescription::GetFileVersion();   // B.3.6 bridge
}
extern "C" void* RtUi_TextDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                       // 180041c70
    return reinterpret_cast<UI::TextDescription*>(self)->UI::TextDescription::Construct();   // B.3.6 bridge
}
extern "C" void* RtUi_TextDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::TextDescription*>(self)->UI::TextDescription::Reconstruct();   // B.3.6 bridge
}
extern "C" void RtUi_TextDescription_Destruct(void* self) {
    reinterpret_cast<UI::TextDescription*>(self)->UI::TextDescription::Destruct();   // B.3.6 bridge
}
extern "C" bool RtUi_TextDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::TextDescription*>(self)->UI::TextDescription::Serialize(stream);   // B.3.6 bridge
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_TextDescription_CreateInstanceElement(const void* self, void** ret,
                                                             const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::TextDescription*>(self)->UI::TextDescription::CreateInstance(   // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
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
