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
void* VtNumberDescription()  { return RT_DATA_SLOT(void*, "??_7NumberDescription@UI@@6B@"); }
const uint8_t* NumberDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@NumberDescription@UI@@0VBaseTypeInfo@Object@@A");
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

// Transcribed from 0x18004a586..0x18004a600 (Construct's copy).
void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18004a586
    *v = VtExternal();                                          // 18004a590
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18004a5a5
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18004a5a8
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18004a5ac
    *v = VtElementDescription();                                // 18004a5b0
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
    *v = VtNumberDescription();                                 // 18004a5e8
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 18004a5f2
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18004a5f6  _u70 + _i74
    *reinterpret_cast<int32_t*>(p + 0x78)  = 0;                 // 18004a5fa  DWORD
    p[0x7c] = 0;                                                // 18004a5fd  BYTE
}

}  // namespace

extern "C" uint32_t RtUi_NumberDescription_Type(const void*) {
    const uint8_t* ti = NumberDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_NumberDescription_TypeName(const void*) {
    const uint8_t* ti = NumberDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_NumberDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_NumberDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 18004a581
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;                                              // 18004a600
}
extern "C" void* RtUi_NumberDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 18004a4d7
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;                                              // 18004a558
}
extern "C" void RtUi_NumberDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1NumberDescription@UI@@UEAA@XZ   0x180118c00
// ============================================================================
// ONE String release — ElementDescription's _textName at +0x60, under the BASE
// vtable — then a tail JMP to ??1External@Object@@. The ExternalPtr<Font> at
// +0x68 is not released; that is retail's.
extern "C" void RtUi_NumberDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);
    using MemFreeFn = void (*)(void*);

    *v = VtNumberDescription();                                // 180118c16
    *v = VtElementDescription();                               // 180118c20

    char*& text = *reinterpret_cast<char**>(p + 0x60);
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
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180118c5c JMP
}

// ============================================================================
// ?Serialize@NumberDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180118e30
// ============================================================================
//   ElementDescription::Serialize                   0x180116b00, ported
//   ExternalPtr<UI::Font>::Serialize(&_font, &pair) 0x18003f1e0
//   kScalarUInt  (vt+0x98)  +0x70
//   kScalarInt   (vt+0xa0)  +0x74
//   kScalarFloat (vt+0x80)  +0x78
//   kScalarBool  (vt+0x78)  +0x7c
//
// FOUR DIFFERENT SCALAR SLOTS IN FOUR CONSECUTIVE CALLS. This is the body that
// says what the four fields' types are; nothing else in the class does.
extern "C" bool RtUi_NumberDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 180118e4a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 180118e7a

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x18003f1e0ull)(p + 0x68, &ser);        // 180118e88

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt  / 8])(rw, p + 0x70, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt   / 8])(rw, p + 0x74, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarFloat / 8])(rw, p + 0x78, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool  / 8])(rw, p + 0x7c, nullptr);

    if (rw != nullptr) {                                       // 180118ee7
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@NumberDescription@UI@@...   retail 0x180118f10
// ============================================================================
// RCX = this, RDX = hidden return slot; R8 (the Dialog) is never read.
//
// NumberElement is 0xe8 bytes (`MOV EDX,0xe8` at 0x180118f51) and its
// constructor is CALLED out of line (0x180118c70), not inlined.
//
// THE FIVE COPIES ARE NOT AT MATCHING OFFSETS on the two objects, and the gap
// is not uniform: desc +0x68 -> elem +0xc0, +0x70 -> +0xc8, +0x74 -> +0xd8,
// +0x78 -> +0xdc, +0x7c -> +0xe0. The jump from +0xc8 to +0xd8 is retail's; a
// "tidied" contiguous copy would write two element fields this body never
// touches.
extern "C" void** RtUi_NumberDescription_CreateInstance(const void* selfv, void** ret) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xe8, 8);   // 180118f51 (inlined)

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 180118f7c
        using NumberElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<NumberElementCtorFn>(0x180118c70ull)(raw)); // 180118f81
    }

    void* slot = elem;                                         // 180118f8d
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 180118f9a

    *reinterpret_cast<void**>(elem + 0xc0) =
        *reinterpret_cast<void* const*>(self + 0x68);          // 180118fa3
    *reinterpret_cast<int32_t*>(elem + 0xc8) =
        *reinterpret_cast<const int32_t*>(self + 0x70);        // 180118fad
    *reinterpret_cast<int32_t*>(elem + 0xd8) =
        *reinterpret_cast<const int32_t*>(self + 0x74);        // 180118fb6
    *reinterpret_cast<int32_t*>(elem + 0xdc) =
        *reinterpret_cast<const int32_t*>(self + 0x78);        // 180118fbf  (the float,
                                                               //  moved as 4 raw bytes)
    elem[0xe0] = self[0x7c];                                   // 180118fc9

    *ret = elem;                                               // 180118fcf
    return ret;
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
