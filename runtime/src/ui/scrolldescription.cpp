// ============================================================================
// runtime/src/ui/scrolldescription.cpp — UI::ScrollDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ScrollDescription@UI@@6B@ (retail 0x180156460), ten slots:
//   0  deleting dtor      0x18003a790  deleting_dtor.cpp row (sizeof 0xc8)
//   1  TypeName           0x18003a120  here
//   2  Type               0x18003a100  here
//   3  GetFileVersion     0x18011a930  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x18011a940  here
//   6  Reconstruct        0x18003a2a0  here
//   7  Destruct           0x18003a2d0  here
//   8  CreateInstance     0x18011aac0  here
//   9  Construct          0x18003a2c0  here
// plus ??0ScrollDescription@UI@@QEAA@XZ 0x18011a4e0 (out of line) and
// ??1ScrollDescription@UI@@UEAA@XZ 0x18011a5d0.
//
// LAYOUT, sizeof 0xc8 (`MOV EDX,0xc8` at 0x18003a7af). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::SpriteSheet>      0x180004590
//     +0x70 .. +0xb0  NINE System::Strings — the batch's largest sprite-name
//                     block; each is looked up against the one sheet at +0x68
//     +0xb8  bool    ctor 1   kScalarBool
//     +0xbc  int32   ctor 1   kScalarInt
//     +0xc0  uint32  \  ctor writes ONE QWORD of -1 at +0xc0 (0x18011a5ab),
//     +0xc4  uint32  /  so BOTH start at 0xffffffff
// and both are kScalarUInt (vt+0x98), not kScalarInt.
//
// Every helper is already ported, including ??0ScrollElement@UI@@ (0x18011a7f0,
// ui_factory_wave28.cpp), so this file adds none.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ElementDescription.h"
#include "../../types/io/Serializer.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()              { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()          { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription(){ return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtScrollDescription() { return RT_DATA_SLOT(void*, "??_7ScrollDescription@UI@@6B@"); }
const uint8_t* ScrollDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ScrollDescription@UI@@0VBaseTypeInfo@Object@@A");
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

using MemFreeFn = void (*)(void*);

void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*field);
    } else {
        *field = nullptr;
    }
}

// The nine name fields, in the order Serialize and CreateInstance use them.
const int kNameOff[9] = { 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xa0, 0xa8, 0xb0 };

}  // namespace

// ??0ScrollDescription@UI@@QEAA@XZ  retail 0x18011a4e0
extern "C" void* RtUi_ScrollDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18011a4f1
    *v = VtExternal();                                          // 18011a4fb
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18011a510
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18011a513
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18011a517
    *v = VtElementDescription();                                // 18011a51b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18011a525
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18011a529
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18011a52d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18011a531  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18011a534  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18011a53b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18011a543
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18011a547
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18011a54b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18011a54f
    *v = VtScrollDescription();                                 // 18011a553
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 18011a55d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18011a561
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 18011a565
    *reinterpret_cast<uint64_t*>(p + 0x80) = 0;                 // 18011a569
    *reinterpret_cast<uint64_t*>(p + 0x88) = 0;                 // 18011a570
    *reinterpret_cast<uint64_t*>(p + 0x90) = 0;                 // 18011a577
    *reinterpret_cast<uint64_t*>(p + 0x98) = 0;                 // 18011a57e
    *reinterpret_cast<uint64_t*>(p + 0xa0) = 0;                 // 18011a585
    *reinterpret_cast<uint64_t*>(p + 0xa8) = 0;                 // 18011a58c
    *reinterpret_cast<uint64_t*>(p + 0xb0) = 0;                 // 18011a593
    p[0xb8] = 1;                                                // 18011a59a  BYTE
    *reinterpret_cast<int32_t*>(p + 0xbc) = 1;                  // 18011a5a1  DWORD
    *reinterpret_cast<int64_t*>(p + 0xc0) = -1;                 // 18011a5ab  ONE QWORD
    return p;                                                   // 18011a5b6
}

extern "C" uint32_t RtUi_ScrollDescription_Type(const void*) {
    const uint8_t* ti = ScrollDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ScrollDescription_TypeName(const void*) {
    const uint8_t* ti = ScrollDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ScrollDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_ScrollDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 18003a2c0
    return RtUi_ScrollDescription_Ctor(selfv);
}
extern "C" void* RtUi_ScrollDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtUi_ScrollDescription_Ctor(selfv);
}
extern "C" void RtUi_ScrollDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1ScrollDescription@UI@@UEAA@XZ  0x18011a5d0 — the nine Strings in REVERSE
// order (+0xb0 down to +0x70) under this class's vtable, then the base's
// _textName at +0x60. The ExternalPtr at +0x68 is not released.
extern "C" void RtUi_ScrollDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtScrollDescription();                                // 18011a5eb
    for (int i = 8; i >= 0; --i)                               // 18011a5f5 .. 18011a77a
        ReleaseStringField(reinterpret_cast<char**>(p + kNameOff[i]));

    *v = VtElementDescription();                               // 18011a7a0
    ReleaseStringField(reinterpret_cast<char**>(p + 0x60));    // 18011a7aa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 18011a7dd JMP
}

// ?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x18011a940
extern "C" bool RtUi_ScrollDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 18011a95a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 18011a98a

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x68, &ser);        // 18011a998
    for (int i = 0; i < 9; ++i)                                // 18011a9a6 .. 18011aa2b
        RtAddr<FieldSerFn>(0x1800c9930ull)(p + kNameOff[i], &ser);

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, p + 0xb8, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, p + 0xbc, nullptr);
    // BOTH of the last two are kScalarUInt (vt+0x98), not kScalarInt.
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p + 0xc0, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p + 0xc4, nullptr);

    if (rw != nullptr) {                                       // 18011aa96
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@ScrollDescription@UI@@...   retail 0x18011aac0
// ============================================================================
// ScrollElement is 0x178 bytes (`MOV EDX,0x178` at 0x18011ab00); its ctor is
// CALLED out of line (0x18011a7f0, already ported). R8 (the Dialog) is never
// read — this class creates no children.
//
// NINE {sheet, index} pairs into +0xc0/+0xc8 .. +0x140/+0x148, with the same
// dword-written/qword-read stack slot the other appliers in this batch have,
// and the sheet RE-READ from the description before each of the nine.
//
// THE LAST THREE COPIES ARE NOT IN ORDER: +0xbc -> element +0x15c, +0xc0 ->
// +0x154, +0xc4 -> +0x158 (0x18011ad34, 0x18011ad40, 0x18011ad4c). Copying them
// in destination order would swap two element fields.
extern "C" void** RtUi_ScrollDescription_CreateInstance(const void* selfv, void** ret) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x178, 8);  // 18011ab00 (inlined)

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                      // 18011ab2a
        using ScrollElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<ScrollElementCtorFn>(0x18011a7f0ull)(raw)); // 18011ab2f
    }

    void* slot = elem;                                         // 18011ab37
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 18011ab42

    uint64_t idxSlot;                                          // [RBP-0x10]
    for (int i = 0; i < 9; ++i) {
        void* sheet = *reinterpret_cast<void* const*>(self + 0x68);   // RE-READ each time
        if (sheet != nullptr) {
            using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
            const int32_t idx = RT_EXPORT_ONCE(
                GetIdxFn,
                "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z")(
                    sheet, self + kNameOff[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);      // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        const int off = 0xc0 + i * 0x10;
        *reinterpret_cast<void**>(elem + off)          = sheet;
        *reinterpret_cast<uint64_t*>(elem + off + 8)   = idxSlot;   // QWORD store
    }

    elem[0x150] = self[0xb8];                                  // 18011ad2e
    *reinterpret_cast<int32_t*>(elem + 0x15c) =
        *reinterpret_cast<const int32_t*>(self + 0xbc);        // 18011ad3a
    *reinterpret_cast<int32_t*>(elem + 0x154) =
        *reinterpret_cast<const int32_t*>(self + 0xc0);        // 18011ad46
    *reinterpret_cast<int32_t*>(elem + 0x158) =
        *reinterpret_cast<const int32_t*>(self + 0xc4);        // 18011ad52

    *ret = elem;                                               // 18011ad58
    return ret;
}

int RtUiScrollDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ScrollDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ScrollDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18003a100ull, (void*)&RtUi_ScrollDescription_Type);

    RegisterRuntimeExport("?TypeName@ScrollDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ScrollDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18003a120ull, (void*)&RtUi_ScrollDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ScrollDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ScrollDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18011a930ull, (void*)&RtUi_ScrollDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ScrollDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18003a2c0ull, (void*)&RtUi_ScrollDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ScrollDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ScrollDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18003a2a0ull, (void*)&RtUi_ScrollDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ScrollDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ScrollDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18003a2d0ull, (void*)&RtUi_ScrollDescription_Destruct);

    RegisterRuntimeExport("??0ScrollDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_ScrollDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011a4e0ull, (void*)&RtUi_ScrollDescription_Ctor);

    RegisterRuntimeExport("??1ScrollDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ScrollDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011a5d0ull, (void*)&RtUi_ScrollDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ScrollDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ScrollDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x18011a940ull, (void*)&RtUi_ScrollDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ScrollDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ScrollDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x18011aac0ull, (void*)&RtUi_ScrollDescription_CreateInstance);

    return n;
}
