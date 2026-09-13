// ============================================================================
// runtime/src/ui/spinnerdescription.cpp — UI::SpinnerDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7SpinnerDescription@UI@@6B@ (retail 0x1801576c0), ten slots:
//   0  deleting dtor      0x18004b050  deleting_dtor.cpp row (sizeof 0x88)
//   1  TypeName           0x18004ad60  here
//   2  Type               0x18004ad40  here
//   3  GetFileVersion     0x1800d8940  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d8880  here
//   6  Reconstruct        0x18004aee0  here
//   7  Destruct           0x18004af10  here
//   8  CreateInstance     0x1800d86b0  here
//   9  Construct          0x18004af00  here
// plus ??0SpinnerDescription@UI@@QEAA@XZ 0x1800d89c0 (out of line, both
// Construct and Reconstruct tail-jump to it) and
// ??1SpinnerDescription@UI@@UEAA@XZ 0x1800d8950.
//
// LAYOUT, sizeof 0x88 (`MOV EDX,0x88` at 0x18004b06f). ElementDescription
// (0x68) plus THREE child descriptions and one int:
//     +0x68  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x70  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x78  ExternalPtr<UI::ElementDescription>   0x180038f90
//     +0x80  int32  _i80   ctor 1, kScalarInt, copied to the element's +0xc0
//     +0x84..+0x87 never written
//
// The three children are NOT an Array — they are three separate fields, each
// serialized by its own call and each turned into one child element by its own
// copy of the create-parent-push-dirty sequence in CreateInstance. Three
// literal repetitions, not a loop, and transcribed as three.
//
// Every helper is already ported: 0x180038f90 (ExternalPtr<ElementDescription>
// ::Serialize, external_ptr_serialize.cpp), 0x180116b00, 0x180115ad0,
// 0x180112ed0, 0x180115e50, 0x180076900.
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
void* VtSpinnerDescription() { return RT_DATA_SLOT(void*, "??_7SpinnerDescription@UI@@6B@"); }
void* VtSpinnerElement()     { return RT_DATA_SLOT(void*, "??_7SpinnerElement@UI@@6B@"); }
const uint8_t* SpinnerDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SpinnerDescription@UI@@0VBaseTypeInfo@Object@@A");
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

// One of CreateInstance's three identical child blocks (0x1800d874e,
// 0x1800d87a6, 0x1800d87f6 — the same instructions with a different source
// field each time).
void AddChild(const void* dialog, void** slot, uint8_t* elem, void* childDesc) {
    using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
    void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, slot, childDesc);

    uint8_t* child = *reinterpret_cast<uint8_t**>(made);
    if (child != nullptr)
        *reinterpret_cast<void**>(child + 0x98) = elem;

    using PushFn = void (*)(void* array, int n, const void* fill);
    RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                   *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                   made);

    for (uint8_t* n = elem; n != nullptr; n = *reinterpret_cast<uint8_t**>(n + 0x98))
        *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
}

}  // namespace

// ============================================================================
// ??0SpinnerDescription@UI@@QEAA@XZ   retail 0x1800d89c0
// ============================================================================
extern "C" void* RtUi_SpinnerDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d89d1
    *v = VtExternal();                                          // 1800d89db
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d89f0
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d89f3
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d89f7
    *v = VtElementDescription();                                // 1800d89fb
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d8a05
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d8a09
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d8a0d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d8a11  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d8a14  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d8a1b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d8a23
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d8a27
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d8a2b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d8a2f
    *v = VtSpinnerDescription();                                // 1800d8a33
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d8a3d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d8a41
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 1800d8a45
    *reinterpret_cast<int32_t*>(p + 0x80)  = 1;                 // 1800d8a49
    return p;                                                   // 1800d8a53
}

extern "C" uint32_t RtUi_SpinnerDescription_Type(const void*) {
    const uint8_t* ti = SpinnerDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_SpinnerDescription_TypeName(const void*) {
    const uint8_t* ti = SpinnerDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_SpinnerDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_SpinnerDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 18004af00
    return RtUi_SpinnerDescription_Ctor(selfv);
}
extern "C" void* RtUi_SpinnerDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtUi_SpinnerDescription_Ctor(selfv);
}
extern "C" void RtUi_SpinnerDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1SpinnerDescription@UI@@UEAA@XZ  0x1800d8950 — ONE String release (the
// base's _textName at +0x60, under the BASE vtable), then a tail JMP to
// ??1External@Object@@. The three ExternalPtrs are not released.
extern "C" void RtUi_SpinnerDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);
    using MemFreeFn = void (*)(void*);

    *v = VtSpinnerDescription();                               // 1800d8966
    *v = VtElementDescription();                               // 1800d8970

    char*& text = *reinterpret_cast<char**>(p + 0x60);
    if (text != nullptr) {                                     // 1800d897e
        --*reinterpret_cast<int32_t*>(text - 4);
        if (*reinterpret_cast<int32_t*>(text - 4) == 0) {
            text = text - 8;
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(text);
        } else {
            text = nullptr;                                    // 1800d899c
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d89ac JMP
}

// ?Serialize@SpinnerDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800d8880
extern "C" bool RtUi_SpinnerDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 1800d889a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d88ca

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x68, &ser);        // 1800d88d8
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x70, &ser);        // 1800d88e6
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x78, &ser);        // 1800d88f4

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x80, nullptr);

    if (rw != nullptr) {                                       // 1800d891a
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@SpinnerDescription@UI@@...   retail 0x1800d86b0
// ============================================================================
// SpinnerElement is 0xc8 bytes (`MOV EDX,0xc8` at 0x1800d86f2); construction
// calls ??0Element, stamps the SpinnerElement vtable and writes ONE dword,
// +0xc0 = 1 — which the tail then overwrites with the description's +0x80. Both
// stores are retail's and both are here.
extern "C" void** RtUi_SpinnerDescription_CreateInstance(const void* selfv, void** ret,
                                                         const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xc8, 8);   // 1800d86f2 (inlined)

    uint8_t* elem = static_cast<uint8_t*>(raw);
    if (raw != nullptr) {                                      // 1800d871d
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 1800d8722
        *reinterpret_cast<void**>(elem) = VtSpinnerElement();  // 1800d8728
        *reinterpret_cast<int32_t*>(elem + 0xc0) = 1;          // 1800d8732
    }

    void* slot = elem;                                         // 1800d873c
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 1800d8749

    AddChild(dialog, &slot, elem, *reinterpret_cast<void* const*>(self + 0x68));  // 1800d874e
    AddChild(dialog, &slot, elem, *reinterpret_cast<void* const*>(self + 0x70));  // 1800d87a6
    AddChild(dialog, &slot, elem, *reinterpret_cast<void* const*>(self + 0x78));  // 1800d87f6

    *reinterpret_cast<int32_t*>(elem + 0xc0) =
        *reinterpret_cast<const int32_t*>(self + 0x80);        // 1800d884c

    *ret = elem;                                               // 1800d8852
    return ret;
}

int RtUiSpinnerDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SpinnerDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_SpinnerDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18004ad40ull, (void*)&RtUi_SpinnerDescription_Type);

    RegisterRuntimeExport("?TypeName@SpinnerDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SpinnerDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18004ad60ull, (void*)&RtUi_SpinnerDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@SpinnerDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_SpinnerDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d8940ull, (void*)&RtUi_SpinnerDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpinnerDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18004af00ull, (void*)&RtUi_SpinnerDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@SpinnerDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_SpinnerDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18004aee0ull, (void*)&RtUi_SpinnerDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@SpinnerDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_SpinnerDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18004af10ull, (void*)&RtUi_SpinnerDescription_Destruct);

    RegisterRuntimeExport("??0SpinnerDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_SpinnerDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d89c0ull, (void*)&RtUi_SpinnerDescription_Ctor);

    RegisterRuntimeExport("??1SpinnerDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_SpinnerDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d8950ull, (void*)&RtUi_SpinnerDescription_Dtor);

    RegisterRuntimeExport("?Serialize@SpinnerDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_SpinnerDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d8880ull, (void*)&RtUi_SpinnerDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@SpinnerDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_SpinnerDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d86b0ull, (void*)&RtUi_SpinnerDescription_CreateInstance);

    return n;
}
