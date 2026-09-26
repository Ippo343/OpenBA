// ============================================================================
// runtime/src/ui/numbereditdescription.cpp — UI::NumberEditDescription
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription (unlike Radio
// and Page, which go through GroupDescription).
//
// ??_7NumberEditDescription@UI@@6B@ (retail 0x180156fe0), ten slots:
//   0  deleting dtor      0x180046190  deleting_dtor.cpp row (sizeof 0x88)
//   1  TypeName           0x180045e90  here
//   2  Type               0x180045e70  here
//   3  GetFileVersion     0x1800d4fd0  here  (returns 0x1004)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d4ee0  here
//   6  Reconstruct        0x180046010  here
//   7  Destruct           0x180046040  here
//   8  CreateInstance     0x1800d4cc0  here
//   9  Construct          0x180046030  here
// plus ??0NumberEditDescription@UI@@QEAA@XZ 0x1800d5050 (out of line) and
// ??1NumberEditDescription@UI@@UEAA@XZ 0x1800d4fe0.
//
// LAYOUT, sizeof 0x88 (`MOV EDX,0x88` at 0x1800461af). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::Font>   ctor 0, serialized by 0x18003f1e0
//     +0x70  int32   _i70    *** NEVER WRITTEN BY THE CONSTRUCTOR *** — the
//                            ctor's qword store at +0x68 stops at +0x6f and the
//                            next store is the dword at +0x74. It IS serialized
//                            (kScalarInt) and it IS read by CreateInstance
//                            (formatted into the element's text with "%i"), so
//                            a fresh, never-deserialized description formats
//                            whatever the allocator left there. That is
//                            retail's; zeroing it here would be a divergence
//                            and would hide the fact if it ever matters.
//     +0x74  int32   _i74    ctor 6
//     +0x78  int32   _i78    ctor 0xffff8001  (-32767)
//     +0x7c  int32   _i7c    ctor 0x7fff
//     +0x80  bool    _b80    ctor 1 — selects the character set (see below)
//     +0x81..+0x87 never written
//
// NOT PORTED HERE, DELIBERATELY: ??0EditElement@UI@@QEAA@XZ (0x1800d0f10), the
// base-element constructor CreateInstance calls. The *Element constructors
// belong to the Element files; this one is reached through ResolveAddr, which
// is registry-first, so it picks up their port the moment it lands.
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::NumberEditDescription`
// member (runtime/types/ui/NumberEditDescription.h, the layout above moved out
// of this file — RULE B6-8). The doors delegate with a QUALIFIED call; the ctor
// and dtor stay DOOR bodies. CreateInstance writes a real
// `UI::NumberEditElement` (EditElement's 0xf8 + `_min`/`_max`).
// ============================================================================
#include <atomic>

#include "element_common.h"   // rtuielem::AtomicExport (RULE B6-5), ResolveAddr
#include "../../types/ui/NumberEditDescription.h"
#include "../../types/ui/NumberEditElement.h"
#include "../../types/io/Serializer.h"

namespace {

void* VtBase()                  { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()              { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription()    { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtNumberEditDescription() { return RT_DATA_SLOT(void*, "??_7NumberEditDescription@UI@@6B@"); }

// RULE B6-5: the members resolve through namespace-scope atomics.
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_vtNumberEditElementSlot{nullptr};
std::atomic<void*> g_memFreeSlot{nullptr};

const uint8_t* NumberEditDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(rtuielem::AtomicExport(
        g_typeInfoSlot, "?_typeInfo@NumberEditDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtNumberEditElement() {
    return rtuielem::AtomicExport(g_vtNumberEditElementSlot, "??_7NumberEditElement@UI@@6B@");
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

using MemFreeFn = void (*)(void*);
void MemFree(void* p) {
    reinterpret_cast<MemFreeFn>(rtuielem::AtomicExport(
        g_memFreeSlot, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(p);
}
int32_t& RefCount(char* s) { return *reinterpret_cast<int32_t*>(s - 4); }

// `System::String::operator=` INLINED, as retail has it at both of this file's
// two assignment sites (0x1800d4d8f and 0x1800d4e37). The `field = nullptr` arm
// on a still-shared string is the asymmetry the sibling *Description
// destructors carry too.
void StringAssignInline(char** field, char* value) {
    if (*field == value) return;
    if (*field != nullptr) {
        if (--RefCount(*field) == 0) {
            *field = *field - 8;
            MemFree(*field);
        } else {
            *field = nullptr;
        }
    }
    *field = value;
    if (value != nullptr) ++RefCount(value);
}

// The matching release of the caller's temporary, also inlined by retail.
void StringReleaseInline(char** slot) {
    char* s = *slot;
    if (s == nullptr) return;
    if (--RefCount(s) == 0) {
        s = s - 8;
        *slot = s;
        MemFree(s);
    } else {
        *slot = nullptr;
    }
}

}  // namespace

// ============================================================================
// ??0NumberEditDescription@UI@@QEAA@XZ   retail 0x1800d5050 — a DOOR BODY
// ============================================================================
extern "C" void* RtUi_NumberEditDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d5061
    *v = VtExternal();                                          // 1800d506b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d5080
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d5083
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d5087
    *v = VtElementDescription();                                // 1800d508b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d5095
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d5099
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d509d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d50a1  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d50a4  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d50ab  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d50b3
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d50b7
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d50bb
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d50bf
    *v = VtNumberEditDescription();                             // 1800d50c3
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d50cd  _font
    // +0x70 (_i70) SKIPPED — see the banner. The next store is at +0x74.
    *reinterpret_cast<int32_t*>(p + 0x74)  = 6;                 // 1800d50d1  _i74
    *reinterpret_cast<int32_t*>(p + 0x78)  = static_cast<int32_t>(0xffff8001u);  // 1800d50d8 _i78
    *reinterpret_cast<int32_t*>(p + 0x7c)  = 0x7fff;            // 1800d50df  _i7c
    p[0x80] = 1;                                                // 1800d50e6  _b80
    return p;                                                   // 1800d50ed
}

namespace UI {

uint32_t NumberEditDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, NumberEditDescriptionTypeInfo(), sizeof v); return v;
}
const char* NumberEditDescription::TypeName() const {
    const char* v = nullptr;
    __builtin_memcpy(&v, NumberEditDescriptionTypeInfo() + 8, sizeof v);
    return v;
}
int32_t NumberEditDescription::GetFileVersion() const { return 0x1004; }

// 0x180046030: null test (in the DOOR), then the ctor.
Object::External* NumberEditDescription::Construct() {
    RtUi_NumberEditDescription_Ctor(this);
    return this;
}
Object::External* NumberEditDescription::Reconstruct() {
    DeletingDtor(0);
    RtUi_NumberEditDescription_Ctor(this);
    return this;
}
void NumberEditDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@NumberEditDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d4ee0
// ============================================================================
//   ElementDescription::Serialize                        0x180116b00 (qualified)
//   ExternalPtr<UI::Font>::Serialize(&_font, &pair)      0x18003f1e0
//   kScalarInt  +0x70, +0x78, +0x7c, +0x74     <- NOT in offset order
//   kScalarBool +0x80
//
// THE FOUR INTS ARE EMITTED 0x70, 0x78, 0x7c, 0x74. The order is the format.
bool NumberEditDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 1800d4efa

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d4f2a

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(&_font, &ser);          // 1800d4f38

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_i70, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_i78, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_i7c, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_i74, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_b80, nullptr);

    if (rw != nullptr) {                                       // 1800d4faf
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@NumberEditDescription@UI@@...   retail 0x1800d4cc0  (slot 8)
// ============================================================================
// RETSLOT (B6-2). R8 (the Dialog) is never read — this class creates no
// children.
//
// NumberEditElement is 0x100 bytes (`MOV EDX,0x100` at 0x1800d4cff). Its
// construction CALLS ??0EditElement (0x1800d0f10, a door), stamps the
// NumberEditElement vtable and writes the two dwords that the tail then
// OVERWRITES with the description's own values. Retail does both; the pair at
// 0x1800d4d41/0x1800d4d4b is not dead, because the null-allocation path skips
// them and the tail's stores then run against a null pointer anyway.
//
// TWO STRING FIELDS ARE BUILT:
//   _text   String::Format("%i", _i70)        the initial text
//   _filter _b80 ? "0123456789-" : "0123456789"   the accepted character set
// (both literals read out of .rdata at 0x180156f50 / 0x180156f60, and the
// format string out of 0x180154848).
//
// NO NULL GUARD ON `elem`. When the allocation fails retail sets RDI to zero
// and falls straight through into `MOV [RDI+0xc0],RAX`. That deref is retail's.
Object::InternalPtr<Element>* NumberEditDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* /*dialog*/) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x100, 8);  // 1800d4cff (inlined)

    NumberEditElement* elem = static_cast<NumberEditElement*>(raw);
    if (raw != nullptr) {                                      // 1800d4d2c
        using EditElementCtorFn = void* (*)(void*);
        RtAddr<EditElementCtorFn>(0x1800d0f10ull)(raw);        // 1800d4d31 ??0EditElement
        *reinterpret_cast<void**>(raw) = VtNumberEditElement();            // 1800d4d37
        elem->_min = static_cast<int32_t>(0xffff8001u);        // 1800d4d41
        elem->_max = 0x8000;                                   // 1800d4d4b
    }

    void* slot = elem;                                         // 1800d4d5a
    // The non-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0) — a dialog.cpp door, reached by address.
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(this, &slot);   // 1800d4d67

    elem->_font = _font;                                       // 1800d4d70

    // --- the initial text ---------------------------------------------------
    // `?Format@String@System@@SA?AV12@PEBDZZ` returns by memory; retail points
    // the hidden return slot at [RSP+0x50], the SAME local that was holding
    // `elem` a moment ago, so the temporary and that slot are one object.
    {
        char* tmp = nullptr;                                   // [RSP+0x50]
        using FormatFn = char** (*)(char** ret, const char* fmt, ...);
        char** made = RtAddr<FormatFn>(0x1800c9730ull)(&tmp, "%i", _i70);  // 1800d4d87

        StringAssignInline(&elem->_text, *made);               // 1800d4d8f
        StringReleaseInline(&tmp);                             // 1800d4dd8
    }

    elem->_maxLength = _i74;                                   // 1800d4e05

    // --- the accepted character set -----------------------------------------
    {
        // The MINUS SIGN is in the set only when _b80 is set, which is what
        // makes that field an "allow negative" switch.
        const char* set = _b80 != 0 ? "0123456789-" : "0123456789";  // 1800d4e1c

        char* tmp = nullptr;                                   // 1800d4e27
        using SetFn = void (*)(char** self, const char* src);
        RtAddr<SetFn>(0x1800c95d0ull)(&tmp, set);              // 1800d4e31

        char* value = tmp;                                     // 1800d4e3e  RBX
        StringAssignInline(&elem->_filter, value);             // 1800d4e37

        // The release works off the CAPTURED value, not a re-read of `tmp`:
        // retail keeps it in RBX across the assignment (0x1800d4e82).
        if (value != nullptr) {
            if (--RefCount(value) == 0) {
                value = value - 8;
                tmp = value;
                MemFree(value);
            } else {
                tmp = nullptr;                                 // 1800d4e9f
            }
        }
        (void)tmp;
    }

    elem->_min = _i78;                                         // 1800d4ea7
    elem->_max = _i7c;                                         // 1800d4eb0

    *reinterpret_cast<void**>(ret) = elem;                     // 1800d4eb6
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1NumberEditDescription@UI@@UEAA@XZ   0x1800d4fe0 — a DOOR BODY
// ============================================================================
// ONE String release — ElementDescription's _textName at +0x60, under the BASE
// vtable. The ExternalPtr<Font> at +0x68 is not released (same asymmetry as
// ~ImageDescription). Tail JMP to ??1External@Object@@.
extern "C" void RtUi_NumberEditDescription_Dtor(void* selfv) {
    UI::NumberEditDescription* self = reinterpret_cast<UI::NumberEditDescription*>(selfv);   // B.3.6 bridge
    void** v = reinterpret_cast<void**>(selfv);

    *v = VtNumberEditDescription();                            // 1800d4ff6
    *v = VtElementDescription();                               // 1800d5000

    char*& text = self->_textName;
    if (text != nullptr) {                                     // 1800d500e
        --RefCount(text);                                      // 1800d5013
        if (RefCount(text) == 0) {                             // 1800d5016
            text = text - 8;                                   // 1800d501c
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(text);
        } else {
            text = nullptr;                                    // 1800d502c
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(selfv);   // 1800d503c JMP
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_NumberEditDescription_Type(const void* self) {
    return reinterpret_cast<const UI::NumberEditDescription*>(self)->UI::NumberEditDescription::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_NumberEditDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::NumberEditDescription*>(self)->UI::NumberEditDescription::TypeName();   // B.3.6 bridge
}
extern "C" int32_t RtUi_NumberEditDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::NumberEditDescription*>(self)->UI::NumberEditDescription::GetFileVersion();   // B.3.6 bridge
}
extern "C" void* RtUi_NumberEditDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                       // 180046030
    return reinterpret_cast<UI::NumberEditDescription*>(self)->UI::NumberEditDescription::Construct();   // B.3.6 bridge
}
extern "C" void* RtUi_NumberEditDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::NumberEditDescription*>(self)->UI::NumberEditDescription::Reconstruct();   // B.3.6 bridge
}
extern "C" void RtUi_NumberEditDescription_Destruct(void* self) {
    reinterpret_cast<UI::NumberEditDescription*>(self)->UI::NumberEditDescription::Destruct();   // B.3.6 bridge
}
extern "C" bool RtUi_NumberEditDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::NumberEditDescription*>(self)->UI::NumberEditDescription::Serialize(stream);   // B.3.6 bridge
}
// RDX is the caller's slot; the member writes it and hands it back (B6-2).
extern "C" void** RtUi_NumberEditDescription_CreateInstance(const void* self, void** ret,
                                                            const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::NumberEditDescription*>(self)->UI::NumberEditDescription::CreateInstance(   // B.3.6 bridge
            reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
            static_cast<const UI::Dialog*>(dialog)));
}

int RtUiNumberEditDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@NumberEditDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_NumberEditDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180045e70ull, (void*)&RtUi_NumberEditDescription_Type);

    RegisterRuntimeExport("?TypeName@NumberEditDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_NumberEditDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180045e90ull, (void*)&RtUi_NumberEditDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@NumberEditDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_NumberEditDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d4fd0ull, (void*)&RtUi_NumberEditDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_NumberEditDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180046030ull, (void*)&RtUi_NumberEditDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@NumberEditDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_NumberEditDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180046010ull, (void*)&RtUi_NumberEditDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@NumberEditDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_NumberEditDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180046040ull, (void*)&RtUi_NumberEditDescription_Destruct);

    RegisterRuntimeExport("??0NumberEditDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_NumberEditDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d5050ull, (void*)&RtUi_NumberEditDescription_Ctor);

    RegisterRuntimeExport("??1NumberEditDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_NumberEditDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d4fe0ull, (void*)&RtUi_NumberEditDescription_Dtor);

    RegisterRuntimeExport("?Serialize@NumberEditDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_NumberEditDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d4ee0ull, (void*)&RtUi_NumberEditDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@NumberEditDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_NumberEditDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d4cc0ull, (void*)&RtUi_NumberEditDescription_CreateInstance);

    return n;
}
