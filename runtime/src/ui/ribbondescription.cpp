// ============================================================================
// runtime/src/ui/ribbondescription.cpp — UI::RibbonDescription, the whole class
// ============================================================================
//
// The cutover boot's wall after UI::GroupDescription was ported:
//
//   [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
//   [RtMint]   vtable : ??_7RibbonDescription@UI@@6B@
//   [RtMint]   slot   : 2
//   [RtMint]   target : ?Type@RibbonDescription@UI@@UEBAIXZ
//
// Same shape as groupdescription.cpp, which is this file's sibling and the
// place the shared reasoning is written down (the Serializer pair, the retail
// ReaderWriter offsets, the hidden return slot on CreateInstance). What is NOT
// shared is every constant below, and each one was read out of THIS class's own
// disassembly rather than carried across:
//
//   ??_7RibbonDescription@UI@@6B@ (retail 0x180156ed0), ten slots:
//     0  deleting dtor      0x180044620  deleting_dtor.cpp row (sizeof 0x80)
//     1  TypeName           0x180044130  here
//     2  Type               0x180044110  here
//     3  GetFileVersion     0x1800d5ef0  here  (returns 0x204, NOT Group's 0x104)
//     4  External::Compile  0x1800066e0  already ported
//     5  Serialize          0x1800d5f00  here
//     6  Reconstruct        0x1800443c0  here
//     7  Destruct           0x180044520  here
//     8  CreateInstance     0x1800d6000  here
//     9  Construct          0x180044470  here
//   ??1RibbonDescription@UI@@UEAA@XZ     0x1800d5c90
//
// LAYOUT, sizeof 0x80 (`MOV EDX,0x80` at 0x18004463f, the eh_vector stride in
// the deleting destructor). `ElementDescription` (0x68) plus:
//     +0x68  bool    _b68        ctor: `MOV word [RCX+0x68],DX` — ONE 16-bit
//     +0x69  bool    _b69        store covering BOTH bools
//     +0x6a  (2 bytes never written by the ctor and never read here)
//     +0x6c  int32   _i6c        ctor: `MOV dword [RCX+0x6c],EDX`
//     +0x70  Array<ExternalPtr<ElementDescription>>   _children
//     +0x78  _size, +0x7c _capacity
// The three names are deliberately `_b68`/`_b69`/`_i6c`: Serialize proves their
// WIDTHS (two kScalarBool calls at +0x68/+0x69 and one kScalarInt at +0x6c) and
// CreateInstance proves where they are copied to on the element (+0xc0, +0xc1,
// +0xc4), but nothing in either body says what they MEAN. One body is not
// enough to name a field, and a guessed name in a header whose contract is that
// it is measured would be worse than none.
//
// NOT HERE: `?CreateNewObject@RibbonDescription@UI@@` / `?CreateInPlaceObject@`
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
void* VtRibbonDescription()  { return RT_DATA_SLOT(void*, "??_7RibbonDescription@UI@@6B@"); }
void* VtRibbonElement()      { return RT_DATA_SLOT(void*, "??_7RibbonElement@UI@@6B@"); }
const uint8_t* RibbonDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@RibbonDescription@UI@@0VBaseTypeInfo@Object@@A");
}

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

// The same two-field views groupdescription.cpp carries, for the same reason:
// these bodies read exactly `Stream::mode` (+0x08) and `Stream::encodingFactory`
// (+0x10), and the {stream, readerWriter} PAIR must live on this frame because
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

// The inlined constructor, transcribed from 0x18004448a..0x180044501. Identical
// to GroupDescription's through +0x60 (the ElementDescription part), then:
//   0x1800444f6  MOV word  [RCX+0x68],DX      ONE 16-bit store, both bools
//   0x1800444fa  MOV dword [RCX+0x6c],EDX
//   0x1800444fd  MOV qword [RCX+0x70],RDX
//   0x180044501  MOV qword [RCX+0x78],RDX     _size + _capacity
// +0x6a..+0x6b IS NEVER WRITTEN. Zeroing it would be a divergence and comparing
// it would be the "diff bytes neither side wrote" mistake.
void ConstructBody(uint8_t* p) {
    void** v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 18004448a
    *v = VtExternal();                                          // 180044494
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800444a9
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800444ac
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800444b0
    *v = VtElementDescription();                                // 1800444b4
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800444be
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800444c2
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800444c6
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800444ca  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800444cd  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800444d4  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800444dc
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800444e0
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800444e4
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800444e8
    *v = VtRibbonDescription();                                 // 1800444ec
    *reinterpret_cast<uint16_t*>(p + 0x68) = 0;                 // 1800444f6  WORD
    *reinterpret_cast<int32_t*>(p + 0x6c)  = 0;                 // 1800444fa
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800444fd
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 180044501
}

}  // namespace

extern "C" uint32_t RtUi_RibbonDescription_Type(const void*) {
    const uint8_t* ti = RibbonDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_RibbonDescription_TypeName(const void*) {
    const uint8_t* ti = RibbonDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800d5ef0: `MOV EAX,0x204 ; RET`. 0x204, not GroupDescription's 0x104 —
// the file version is per class and the two must not be shared.
extern "C" int32_t RtUi_RibbonDescription_GetFileVersion(const void*) { return 0x204; }

extern "C" void* RtUi_RibbonDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180044481
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;
}
extern "C" void* RtUi_RibbonDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 1800443d7
    ConstructBody(static_cast<uint8_t*>(selfv));
    return selfv;
}
// 0x180044520 is three instructions: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`
// — a tail jump into slot 0 with flag 0, i.e. destroy without freeing.
extern "C" void RtUi_RibbonDescription_Destruct(void* selfv) {
    CallSlot0(selfv, 0);
}

// ============================================================================
// ??1RibbonDescription@UI@@UEAA@XZ   0x1800d5c90
// ============================================================================
// The child array is at +0x70 here (GroupDescription's is at +0x68); everything
// else is the same sequence, including retail's asymmetric String release at
// ElementDescription::_textName (+0x60) — freed field moved to `data-8` on the
// last reference, NULLED otherwise — and the tail JMP to ??1External@Object@@.
extern "C" void RtUi_RibbonDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtRibbonDescription();                                // 1800d5cad

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180039440ull)(p + 0x70);             // 1800d5cb4

    *v = VtElementDescription();                               // 1800d5cc1

    char*& s = *reinterpret_cast<char**>(p + 0x60);
    if (s != nullptr) {                                        // 1800d5cc8
        --*reinterpret_cast<int32_t*>(s - 4);                  // 1800d5ccd
        if (*reinterpret_cast<int32_t*>(s - 4) == 0) {         // 1800d5cd0
            s = s - 8;                                         // 1800d5cd6
            using MemFreeFn = void (*)(void*);
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(s);
        } else {
            s = nullptr;                                       // 1800d5ce6
        }
    }

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800d5cf6 JMP
}

// ============================================================================
// ?Serialize@RibbonDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5f00
// ============================================================================
//   ElementDescription::Serialize(this, stream)     0x180116b00, ported
//   rw = encoding factory, read side or write side by stream->mode
//   rw->vt[0x78](rw, &_b68, 0)      kScalarBool   1800d5f5e
//   rw->vt[0x78](rw, &_b69, 0)      kScalarBool   1800d5f70
//   rw->vt[0xa0](rw, &_i6c, 0)      kScalarInt    1800d5f82
//   n = _children._size ; rw->vt[0xa0](rw, &n, 0)              1800d5f9f
//   if (pair.stream->mode == 0) SetSize(&_children, n)         0x1800492d0
//   ElementDescriptionPtrArray_Serialize(&pair, n, _children._data)  0x1800493d0
//   if (rw) rw->vt[0](rw, 1)
//
// THE FIELD ORDER IS 0x68, 0x69, 0x6c — NOT 0x68, 0x6c, 0x69. CreateInstance
// copies them in the OTHER order (0x68, 0x6c, 0x69 at
// 1800d60bf..1800d60d6) and a port that took its order from there would emit
// the two bools and the int in the wrong sequence into every saved layout,
// which the reader would then mis-parse. Each body's order is its own.
extern "C" bool RtUi_RibbonDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 1800d5f1a

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;       // Create{Reader,Writer}
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d5f4a

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, p + 0x68, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, p + 0x69, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, p + 0x6c, nullptr);

    int32_t n = *reinterpret_cast<int32_t*>(p + 0x78);         // 1800d5f88
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 1800d5fa5 (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x1800492d0ull)(p + 0x70, n);        // 1800d5fb8
    }

    using ElemSerFn = void (*)(Serializer* ser, int count, void* data);
    RtAddr<ElemSerFn>(0x1800493d0ull)(&ser, n, *reinterpret_cast<void**>(p + 0x70));

    if (rw != nullptr) {                                       // 1800d5fd5
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@RibbonDescription@UI@@...   retail 0x1800d6000
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = the
// Dialog*. (See groupdescription.cpp for why Ghidra prints only two of the
// three.)
//
// RibbonElement is 0xd8 bytes (`MOV EDX,0xd8` at 0x1800d6048), i.e. Element's
// 0xc0 plus four of its own fields, which the tail of this body initialises and
// then fills from the description:
//     +0xc0 bool, +0xc1 bool, +0xc4 int32, +0xc8 Array (data/size/cap)
// The ctor half zeroes +0xc0..+0xc1 with ONE 16-bit store, then +0xc4, +0xc8,
// +0xd0 (1800d6088..1800d60a0).
//
// The LAST call is the one that makes this class different from Group's:
// `Array<int32>::SetSize(&elem->+0xc8, desc->_children._size)` (0x18002fea0,
// array.cpp kSpec_I32 — the instantiation whose new tail is UNINITIALISED).
// It runs AFTER the child loop and OUTSIDE it.
extern "C" void** RtUi_RibbonDescription_CreateInstance(const void* selfv,
                                                        void** ret,
                                                        const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xd8, 8);   // 1800d6048

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d6070
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 1800d6078 ??0Element
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtRibbonElement();      // 1800d6085
        *reinterpret_cast<uint16_t*>(e + 0xc0) = 0;            // 1800d6088  WORD
        *reinterpret_cast<int32_t*>(e + 0xc4)  = 0;            // 1800d6093
        *reinterpret_cast<uint64_t*>(e + 0xc8) = 0;            // 1800d6099
        *reinterpret_cast<uint64_t*>(e + 0xd0) = 0;            // 1800d60a0
        elem = e;
    }

    void* slot = elem;                                         // 1800d60ad [RSP+0x50]

    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 1800d60ba

    elem[0xc0] = self[0x68];                                   // 1800d60bf
    *reinterpret_cast<int32_t*>(elem + 0xc4) =
        *reinterpret_cast<const int32_t*>(self + 0x6c);        // 1800d60c9
    elem[0xc1] = self[0x69];                                   // 1800d60d2

    const int32_t count = *reinterpret_cast<const int32_t*>(self + 0x78);
    for (int32_t i = 0; i < count; ++i) {                      // 1800d60dc / 1800d6149
        void* const* descArray = *reinterpret_cast<void* const* const*>(self + 0x70);
        void*        desc      = descArray[i];                 // 1800d60f7

        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, desc);

        uint8_t* child = *reinterpret_cast<uint8_t**>(made);   // 1800d6108
        if (child != nullptr)
            *reinterpret_cast<void**>(child + 0x98) = elem;    // 1800d6110

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                       *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                       made);                  // 1800d6129

        for (uint8_t* n = elem; n != nullptr;                   // 1800d6131..1800d6145
             n = *reinterpret_cast<uint8_t**>(n + 0x98)) {
            *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
        }
    }

    using SetSizeFn = void (*)(void* array, int n);
    RtAddr<SetSizeFn>(0x18002fea0ull)(elem + 0xc8, count);     // 1800d6158

    *ret = elem;                                               // 1800d615d
    return ret;
}

int RtUiRibbonDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RibbonDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_RibbonDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180044110ull, (void*)&RtUi_RibbonDescription_Type);

    RegisterRuntimeExport("?TypeName@RibbonDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_RibbonDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180044130ull, (void*)&RtUi_RibbonDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@RibbonDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_RibbonDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d5ef0ull, (void*)&RtUi_RibbonDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RibbonDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180044470ull, (void*)&RtUi_RibbonDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@RibbonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RibbonDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800443c0ull, (void*)&RtUi_RibbonDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@RibbonDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_RibbonDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180044520ull, (void*)&RtUi_RibbonDescription_Destruct);

    RegisterRuntimeExport("??1RibbonDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_RibbonDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d5c90ull, (void*)&RtUi_RibbonDescription_Dtor);

    RegisterRuntimeExport("?Serialize@RibbonDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_RibbonDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d5f00ull, (void*)&RtUi_RibbonDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@RibbonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_RibbonDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d6000ull, (void*)&RtUi_RibbonDescription_CreateInstance);

    return n;
}
