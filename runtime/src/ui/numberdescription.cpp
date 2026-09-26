// ============================================================================
// runtime/src/ui/numberdescription.cpp — UI::NumberDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7NumberDescription@UI@@6B@ (retail 0x180157530), ten slots:
//   0  deleting dtor      0x18004a760  deleting_dtor.cpp row (sizeof 0x88)
//   1  TypeName           0x18004a240  here
//   2  Type               0x18004a220  here
//   3  GetFileVersion     0x180118e20  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180118e30  here
//   6  Reconstruct        0x18004a4c0  here
//   7  Destruct           0x18004a610  here
//   8  CreateInstance     0x180118f10  here
//   9  Construct          0x18004a570  here
// plus ??1NumberDescription@UI@@UEAA@XZ 0x180118c00.
//
// THE CONSTRUCTOR IS INLINED, NOT OUT OF LINE. Unlike Text/Radio/Page/
// NumberEdit, there is no ??0NumberDescription export to register: the body
// appears twice in the shipped DLL, once inside Construct (0x18004a586..) and
// once inside Reconstruct (0x18004a4de..). The two were compared instruction by
// instruction for this port and agree; it is written once here and both slots
// call it.
//
// LAYOUT, sizeof 0x88 (`MOV EDX,0x88` at 0x18004a77f). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::Font>   ctor 0, serialized by 0x18003f1e0
//     +0x70  uint32  _u70   kScalarUInt (vt+0x98) — NOT the int slot
//     +0x74  int32   _i74   kScalarInt
//     +0x78  float   _f78   kScalarFloat (vt+0x80); the ctor writes it as an
//                           integer zero (`MOV dword [RCX+0x78],EDX`), which is
//                           0.0f, so nothing is lost by calling it a float —
//                           but the SERIALIZER is what says it is one.
//     +0x7c  bool    _b7c
//     +0x7d..+0x87 never written by the ctor
// The ctor's +0x68 store is ONE QWORD covering +0x68..+0x6f and the +0x70 store
// is another covering +0x70..+0x77, which is what pins _u70 and _i74 as the two
// halves of that second qword.
//
// NOT PORTED HERE: ??0NumberElement@UI@@QEAA@XZ (0x180118c70), reached through
// ResolveAddr — the *Element constructors belong to the Element files.
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::NumberDescription` member
// (runtime/types/ui/NumberDescription.h, the layout above moved out of this
// file — RULE B6-8). The doors delegate with a QUALIFIED call. CreateInstance
// writes a real `UI::NumberElement`.
// ============================================================================
#include <atomic>

#include "element_common.h"   // rtuielem::AtomicExport (RULE B6-5), ResolveAddr
#include "../../types/ui/NumberDescription.h"
#include "../../types/ui/NumberElement.h"
#include "../../types/io/Serializer.h"

namespace {

void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }

// RULE B6-5: the members resolve through namespace-scope atomics.
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_vtBaseSlot{nullptr};
std::atomic<void*> g_vtExternalSlot{nullptr};
std::atomic<void*> g_vtElementDescriptionSlot{nullptr};
std::atomic<void*> g_vtNumberDescriptionSlot{nullptr};

const uint8_t* NumberDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(rtuielem::AtomicExport(
        g_typeInfoSlot, "?_typeInfo@NumberDescription@UI@@0VBaseTypeInfo@Object@@A"));
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

// Transcribed from 0x18004a586..0x18004a600 (Construct's copy); Reconstruct's
// copy at 0x18004a4de.. agrees instruction for instruction. Called from the
// two MEMBERS, so its vtable lookups go through the atomic slots.
void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = rtuielem::AtomicExport(g_vtBaseSlot, "??_7Base@Object@@6B@");          // 18004a586
    *v = rtuielem::AtomicExport(g_vtExternalSlot, "??_7External@Object@@6B@");  // 18004a590
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18004a5a5
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18004a5a8
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18004a5ac
    *v = rtuielem::AtomicExport(g_vtElementDescriptionSlot,
                                "??_7ElementDescription@UI@@6B@");               // 18004a5b0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18004a5ba
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18004a5be
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18004a5c2
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18004a5c6  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18004a5c9  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18004a5d0  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18004a5d8
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18004a5dc
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18004a5e0
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18004a5e4
    *v = rtuielem::AtomicExport(g_vtNumberDescriptionSlot,
                                "??_7NumberDescription@UI@@6B@");                // 18004a5e8
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 18004a5f2  _font
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18004a5f6  _u70 + _i74
    *reinterpret_cast<int32_t*>(p + 0x78)  = 0;                 // 18004a5fa  _f78 DWORD
    p[0x7c] = 0;                                                // 18004a5fd  _b7c BYTE
}

}  // namespace

namespace UI {

uint32_t NumberDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, NumberDescriptionTypeInfo(), sizeof v); return v;
}
const char* NumberDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, NumberDescriptionTypeInfo() + 8, sizeof v); return v;
}
int32_t NumberDescription::GetFileVersion() const { return 0x104; }

// 0x18004a570: null test (in the DOOR), the INLINED ctor, `MOV RAX,RCX`.
Object::External* NumberDescription::Construct() {
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;                                               // 18004a600
}
Object::External* NumberDescription::Reconstruct() {
    DeletingDtor(0);                                           // 18004a4d7
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;                                               // 18004a558
}
void NumberDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180118e30
// ============================================================================
//   ElementDescription::Serialize                   0x180116b00 (qualified)
//   ExternalPtr<UI::Font>::Serialize(&_font, &pair) 0x18003f1e0
//   kScalarUInt  (vt+0x98)  _u70
//   kScalarInt   (vt+0xa0)  _i74
//   kScalarFloat (vt+0x80)  _f78
//   kScalarBool  (vt+0x78)  _b7c
//
// FOUR DIFFERENT SCALAR SLOTS IN FOUR CONSECUTIVE CALLS. This is the body that
// says what the four fields' types are; nothing else in the class does.
bool NumberDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 180118e4a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 180118e7a

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(&_font, &ser);          // 180118e88

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt  / 8])(rw, &_u70, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt   / 8])(rw, &_i74, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarFloat / 8])(rw, &_f78, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool  / 8])(rw, &_b7c, nullptr);

    if (rw != nullptr) {                                       // 180118ee7
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@NumberDescription@UI@@...   retail 0x180118f10  (slot 8)
// ============================================================================
// RETSLOT (B6-2); R8 (the Dialog) is never read.
//
// NumberElement is 0xe8 bytes (`MOV EDX,0xe8` at 0x180118f51) and its
// constructor is CALLED out of line (0x180118c70, a door), not inlined.
//
// THE FIVE COPIES ARE NOT AT MATCHING OFFSETS on the two objects, and the gap
// is not uniform: _font -> +0xc0, _u70 -> _mode (+0xc8), _i74 -> _intValue
// (+0xd8), _f78 -> _floatValue (+0xdc), _b7c -> _showSign (+0xe0).
Object::InternalPtr<Element>* NumberDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog*/) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xe8, 8);   // 180118f51 (inlined)

    NumberElement* elem = nullptr;
    if (raw != nullptr) {                                      // 180118f7c
        using NumberElementCtorFn = void* (*)(void*);
        elem = static_cast<NumberElement*>(
            RtAddr<NumberElementCtorFn>(0x180118c70ull)(raw)); // 180118f81
    }

    void* slot = elem;                                         // 180118f8d
    // The non-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0) — a dialog.cpp door, reached by address.
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(this, &slot);   // 180118f9a

    elem->_font = _font;                                       // 180118fa3
    elem->_mode = static_cast<int32_t>(_u70);                  // 180118fad
    elem->_intValue = _i74;                                    // 180118fb6
    __builtin_memcpy(&elem->_floatValue, &_f78, 4);            // 180118fbf  4 raw bytes
    elem->_showSign = _b7c;                                    // 180118fc9

    *reinterpret_cast<void**>(ret) = elem;                     // 180118fcf
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1NumberDescription@UI@@UEAA@XZ   0x180118c00 — a DOOR BODY
// ============================================================================
// ONE String release — ElementDescription's _textName at +0x60, under the BASE
// vtable — then a tail JMP to ??1External@Object@@. The ExternalPtr<Font> at
// +0x68 is not released; that is retail's.
extern "C" void RtUi_NumberDescription_Dtor(void* selfv) {
    UI::NumberDescription* self = reinterpret_cast<UI::NumberDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);
    using MemFreeFn = void (*)(void*);

    *v = RT_DATA_SLOT(void*, "??_7NumberDescription@UI@@6B@");   // 180118c16
    *v = VtElementDescription();                               // 180118c20

    char*& text = self->_textName;
    if (text != nullptr) {                                     // 180118c2e
        --*reinterpret_cast<int32_t*>(text - 4);               // 180118c33
        if (*reinterpret_cast<int32_t*>(text - 4) == 0) {      // 180118c36
            text = text - 8;                                   // 180118c3c
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(text);
        } else {
            text = nullptr;                                    // 180118c4c
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(selfv);   // 180118c5c JMP
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_NumberDescription_Type(const void* self) {
    return reinterpret_cast<const UI::NumberDescription*>(self)->UI::NumberDescription::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_NumberDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::NumberDescription*>(self)->UI::NumberDescription::TypeName();   // B.3.6 bridge
}
extern "C" int32_t RtUi_NumberDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::NumberDescription*>(self)->UI::NumberDescription::GetFileVersion();   // B.3.6 bridge
}
extern "C" void* RtUi_NumberDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                       // 18004a581
    return reinterpret_cast<UI::NumberDescription*>(self)->UI::NumberDescription::Construct();   // B.3.6 bridge
}
extern "C" void* RtUi_NumberDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::NumberDescription*>(self)->UI::NumberDescription::Reconstruct();   // B.3.6 bridge
}
extern "C" void RtUi_NumberDescription_Destruct(void* self) {
    reinterpret_cast<UI::NumberDescription*>(self)->UI::NumberDescription::Destruct();   // B.3.6 bridge
}
extern "C" bool RtUi_NumberDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::NumberDescription*>(self)->UI::NumberDescription::Serialize(stream);   // B.3.6 bridge
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_NumberDescription_CreateInstance(const void* self, void** ret,
                                                        const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::NumberDescription*>(self)->UI::NumberDescription::CreateInstance(   // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
}

int RtUiNumberDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@NumberDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_NumberDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18004a220ull, (void*)&RtUi_NumberDescription_Type);

    RegisterRuntimeExport("?TypeName@NumberDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_NumberDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18004a240ull, (void*)&RtUi_NumberDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@NumberDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_NumberDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x180118e20ull, (void*)&RtUi_NumberDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_NumberDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18004a570ull, (void*)&RtUi_NumberDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@NumberDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_NumberDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18004a4c0ull, (void*)&RtUi_NumberDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@NumberDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_NumberDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18004a610ull, (void*)&RtUi_NumberDescription_Destruct);

    RegisterRuntimeExport("??1NumberDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_NumberDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180118c00ull, (void*)&RtUi_NumberDescription_Dtor);

    RegisterRuntimeExport("?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_NumberDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180118e30ull, (void*)&RtUi_NumberDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@NumberDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_NumberDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180118f10ull, (void*)&RtUi_NumberDescription_CreateInstance);

    return n;
}
