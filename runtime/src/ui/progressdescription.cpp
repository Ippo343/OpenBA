// ============================================================================
// runtime/src/ui/progressdescription.cpp — UI::ProgressDescription, the class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ProgressDescription@UI@@6B@ (retail 0x1801591e0), ten slots:
//   0  deleting dtor      0x180077c80  deleting_dtor.cpp row (sizeof 0xb0)
//   1  TypeName           0x180077770  here
//   2  Type               0x180077750  here
//   3  GetFileVersion     0x18011a070  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180119f60  here
//   6  Reconstruct        0x1800778f0  here
//   7  Destruct           0x180077920  here
//   8  CreateInstance     0x180119d00  here
//   9  Construct          0x180077910  here
// plus ??0ProgressDescription@UI@@QEAA@XZ 0x18011a210 (out of line) and
// ??1ProgressDescription@UI@@UEAA@XZ 0x18011a080.
//
// LAYOUT, sizeof 0xb0 (`MOV EDX,0xb0` at 0x180077c9f). ElementDescription
// (0x68) plus:
//     +0x68  uint32   _u68    kScalarUInt (vt+0x98). SERIALIZED AND NOTHING
//                             ELSE — no slot of this class reads it.
//     +0x6c  (the ctor's +0x68 store is a DWORD, so +0x6c is never written)
//     +0x70  ExternalPtr<UI::SpriteSheet>       0x180004590
//     +0x78  System::String  \
//     +0x80  System::String   |
//     +0x88  System::String   |  SIX sprite names, all looked up against the
//     +0x90  System::String   |  one sheet at +0x70
//     +0x98  System::String   |
//     +0xa0  System::String  /
//     +0xa8  ExternalPtr<UI::ElementDescription>  0x180038f90 — the OPTIONAL
//                             child; CreateInstance skips the child entirely
//                             when it is null, rather than creating one from
//                             a null description.
//
// Every helper is already ported, including ??0ProgressElement@UI@@
// (0x180119c40, ui_factory_wave28.cpp), so this file adds none.
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

void* VtBase()                { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()            { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription()  { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtProgressDescription() { return RT_DATA_SLOT(void*, "??_7ProgressDescription@UI@@6B@"); }
const uint8_t* ProgressDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ProgressDescription@UI@@0VBaseTypeInfo@Object@@A");
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

}  // namespace

// ??0ProgressDescription@UI@@QEAA@XZ  retail 0x18011a210
extern "C" void* RtUi_ProgressDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18011a221
    *v = VtExternal();                                          // 18011a22b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 18011a240
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 18011a243
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 18011a247
    *v = VtElementDescription();                                // 18011a24b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 18011a255
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 18011a259
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 18011a25d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 18011a261  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 18011a264  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 18011a26b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 18011a273
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 18011a277
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 18011a27b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 18011a27f
    *v = VtProgressDescription();                               // 18011a283
    *reinterpret_cast<int32_t*>(p + 0x68)  = 0;                 // 18011a28d  DWORD — +0x6c
                                                                //            stays untouched
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 18011a290
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 18011a294
    *reinterpret_cast<uint64_t*>(p + 0x80) = 0;                 // 18011a298
    *reinterpret_cast<uint64_t*>(p + 0x88) = 0;                 // 18011a29f
    *reinterpret_cast<uint64_t*>(p + 0x90) = 0;                 // 18011a2a6
    *reinterpret_cast<uint64_t*>(p + 0x98) = 0;                 // 18011a2ad
    *reinterpret_cast<uint64_t*>(p + 0xa0) = 0;                 // 18011a2b4
    *reinterpret_cast<uint64_t*>(p + 0xa8) = 0;                 // 18011a2bb
    return p;                                                   // 18011a2c2
}

extern "C" uint32_t RtUi_ProgressDescription_Type(const void*) {
    const uint8_t* ti = ProgressDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ProgressDescription_TypeName(const void*) {
    const uint8_t* ti = ProgressDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_ProgressDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_ProgressDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180077910
    return RtUi_ProgressDescription_Ctor(selfv);
}
extern "C" void* RtUi_ProgressDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtUi_ProgressDescription_Ctor(selfv);
}
extern "C" void RtUi_ProgressDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1ProgressDescription@UI@@UEAA@XZ  0x18011a080 — the six Strings in REVERSE
// order under this class's vtable, then the base's _textName at +0x60. Neither
// ExternalPtr is released.
extern "C" void RtUi_ProgressDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtProgressDescription();                              // 18011a09b

    ReleaseStringField(reinterpret_cast<char**>(p + 0xa0));    // 18011a0a5
    ReleaseStringField(reinterpret_cast<char**>(p + 0x98));    // 18011a0d8
    ReleaseStringField(reinterpret_cast<char**>(p + 0x90));    // 18011a10a
    ReleaseStringField(reinterpret_cast<char**>(p + 0x88));    // 18011a13c
    ReleaseStringField(reinterpret_cast<char**>(p + 0x80));    // 18011a16e
    ReleaseStringField(reinterpret_cast<char**>(p + 0x78));    // 18011a1a0

    *v = VtElementDescription();                               // 18011a1c6
    ReleaseStringField(reinterpret_cast<char**>(p + 0x60));    // 18011a1d0

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 18011a203 JMP
}

// ?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x180119f60
extern "C" bool RtUi_ProgressDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 180119f7a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 180119faa

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    // kScalarUInt, NOT kScalarInt — vt+0x98 at 0x180119fbe.
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarUInt / 8])(rw, p + 0x68, nullptr);

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x70, &ser);        // 180119fcd ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x78, &ser);        // 180119fdb
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x80, &ser);        // 180119fec
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x88, &ser);        // 180119ffd
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x90, &ser);        // 18011a00e
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x98, &ser);        // 18011a01f
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0xa0, &ser);        // 18011a030
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0xa8, &ser);        // 18011a041 ExternalPtr<ElementDescription>

    if (rw != nullptr) {                                       // 18011a04f
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@ProgressDescription@UI@@...   retail 0x180119d00
// ============================================================================
// ProgressElement is 0x130 bytes (`MOV EDX,0x130` at 0x180119d49); its ctor is
// CALLED out of line (0x180119c40, already ported).
//
// SIX {sheet, index} pairs written into +0xc0/+0xc8 .. +0x110/+0x118, with the
// same dword-written/qword-read stack slot the Image, Slider and Tab bodies
// have (0x180119da5 writes `dword [RBP-0x10]`, 0x180119db8 reads
// `qword [RBP-0x10]`), and the sheet pointer RE-READ from the description
// before every one of the six.
//
// THE CHILD IS CONDITIONAL: `CMP qword [RSI+0xa8],0 ; JZ` at 0x180119ed1 skips
// the whole create-parent-push-dirty block when the description has no child.
// Every other class in this batch creates its children unconditionally.
extern "C" void** RtUi_ProgressDescription_CreateInstance(const void* selfv, void** ret,
                                                          const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x130, 8);  // 180119d49 (inlined)

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                      // 180119d73
        using ProgressElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<ProgressElementCtorFn>(0x180119c40ull)(raw));  // 180119d78
    }

    void* slot = elem;                                         // 180119d80
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 180119d8b

    uint64_t idxSlot;                                          // [RBP-0x10]
    static const int kNameOff[6] = { 0x78, 0x80, 0x88, 0x90, 0x98, 0xa0 };
    static const int kElemOff[6] = { 0xc0, 0xd0, 0xe0, 0xf0, 0x100, 0x110 };
    for (int i = 0; i < 6; ++i) {
        void* sheet = *reinterpret_cast<void* const*>(self + 0x70);   // RE-READ each time
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
        *reinterpret_cast<void**>(elem + kElemOff[i])        = sheet;
        *reinterpret_cast<uint64_t*>(elem + kElemOff[i] + 8) = idxSlot;  // QWORD store
    }

    if (*reinterpret_cast<void* const*>(self + 0xa8) != nullptr) {     // 180119ed1
        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(
            dialog, &slot, *reinterpret_cast<void* const*>(self + 0xa8));  // 180119ee9

        uint8_t* child = *reinterpret_cast<uint8_t**>(made);   // 180119eee
        if (child != nullptr)
            *reinterpret_cast<void**>(child + 0x98) = elem;    // 180119ef6

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                       *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                       made);                  // 180119f0c

        for (uint8_t* n = elem; n != nullptr;                   // 180119f20..180119f34
             n = *reinterpret_cast<uint8_t**>(n + 0x98))
            *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
    }

    *ret = elem;                                               // 180119f39
    return ret;
}

int RtUiProgressDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ProgressDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ProgressDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180077750ull, (void*)&RtUi_ProgressDescription_Type);

    RegisterRuntimeExport("?TypeName@ProgressDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ProgressDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180077770ull, (void*)&RtUi_ProgressDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ProgressDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ProgressDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x18011a070ull, (void*)&RtUi_ProgressDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ProgressDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180077910ull, (void*)&RtUi_ProgressDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ProgressDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ProgressDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800778f0ull, (void*)&RtUi_ProgressDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ProgressDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ProgressDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180077920ull, (void*)&RtUi_ProgressDescription_Destruct);

    RegisterRuntimeExport("??0ProgressDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_ProgressDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x18011a210ull, (void*)&RtUi_ProgressDescription_Ctor);

    RegisterRuntimeExport("??1ProgressDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ProgressDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011a080ull, (void*)&RtUi_ProgressDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ProgressDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180119f60ull, (void*)&RtUi_ProgressDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ProgressDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ProgressDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180119d00ull, (void*)&RtUi_ProgressDescription_CreateInstance);

    return n;
}
