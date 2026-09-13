// ============================================================================
// runtime/src/ui/tabdescription.cpp — UI::TabDescription, the whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7TabDescription@UI@@6B@ (retail 0x1801577f0), ten slots:
//   0  deleting dtor      0x18004c640  deleting_dtor.cpp row (sizeof 0xb8)
//   1  TypeName           0x18004c1f0  here
//   2  Type               0x18004c1d0  here
//   3  GetFileVersion     0x1800da8d0  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800da760  here
//   6  Reconstruct        0x18004c370  here
//   7  Destruct           0x18004c3a0  here
//   8  CreateInstance     0x1800da410  here
//   9  Construct          0x18004c390  here
// plus ??0TabDescription@UI@@QEAA@XZ 0x1800daa00 (out of line) and
// ??1TabDescription@UI@@UEAA@XZ 0x1800da8e0.
//
// LAYOUT, sizeof 0xb8 (`MOV EDX,0xb8` at 0x18004c65f). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::ElementDescription>   \  the three fixed
//     +0x70  ExternalPtr<UI::ElementDescription>    } sub-elements
//     +0x78  ExternalPtr<UI::ElementDescription>   /
//     +0x80  ExternalPtr<UI::SpriteSheet>
//     +0x88  System::String   \
//     +0x90  System::String    }  three sprite names
//     +0x98  System::String   /
//     +0xa0  int32   _i a0   kScalarInt
//     +0xa8  Array<{ExternalPtr<ElementDescription>, ExternalPtr<ElementDescription>}>
//     +0xb0  _size, +0xb4 _capacity
//
// *** +0xa0 IS NEVER WRITTEN BY THE CONSTRUCTOR. *** The ctor's stores run
// ..., +0x98, then jump straight to +0xa8 (0x1800daa9e -> 0x1800daaa5). It is
// serialized and it is copied to the element's +0xc4, so a description that was
// never deserialized hands the element whatever the allocator left. Retail's;
// not "fixed" here.
//
// TWO NEW COLLECTION HELPERS, both appended to
// runtime/src/collection/array.cpp rather than written here (that file is the
// one place the family lives):
//   0x18004cdf0  Array<Tab>::SetSize, two-argument, zeroed tail  -> kSpec_ExtPtrPair10
//   0x18004cc90  ~Array<Tab>, the canonical free-and-zero body   -> J1_ENTRY
// Everything else this class calls was already ported, including
// ?UpdateContainers@TabElement@UI@@ (0x1800d9710, widgets_layout.cpp) and
// ??0TabElement@UI@@ (0x1800da390, ui_factory_wave28.cpp).
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
void* VtTabDescription()     { return RT_DATA_SLOT(void*, "??_7TabDescription@UI@@6B@"); }
const uint8_t* TabDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@TabDescription@UI@@0VBaseTypeInfo@Object@@A");
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

// One child: create it from `desc` through the dialog, parent it, push it onto
// the element's child array and dirty the chain. Retail writes this out SIX
// times in CreateInstance (three fixed children plus two per tab); `slot` is
// the caller's InternalPtr local, and WHICH local matters — see the note in
// CreateInstance.
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
// ??0TabDescription@UI@@QEAA@XZ   retail 0x1800daa00
// ============================================================================
extern "C" void* RtUi_TabDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800daa11
    *v = VtExternal();                                          // 1800daa1b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800daa30
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800daa33
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800daa37
    *v = VtElementDescription();                                // 1800daa3b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800daa45
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800daa49
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800daa4d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800daa51  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800daa54  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800daa5b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800daa63
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800daa67
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800daa6b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800daa6f
    *v = VtTabDescription();                                    // 1800daa73
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800daa7d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800daa81
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;                 // 1800daa85
    *reinterpret_cast<uint64_t*>(p + 0x80) = 0;                 // 1800daa89
    *reinterpret_cast<uint64_t*>(p + 0x88) = 0;                 // 1800daa90
    *reinterpret_cast<uint64_t*>(p + 0x90) = 0;                 // 1800daa97
    *reinterpret_cast<uint64_t*>(p + 0x98) = 0;                 // 1800daa9e
    // +0xa0 SKIPPED — see the banner.
    *reinterpret_cast<uint64_t*>(p + 0xa8) = 0;                 // 1800daaa5
    *reinterpret_cast<uint64_t*>(p + 0xb0) = 0;                 // 1800daaac  _size + _cap
    return p;                                                   // 1800daab3
}

extern "C" uint32_t RtUi_TabDescription_Type(const void*) {
    const uint8_t* ti = TabDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_TabDescription_TypeName(const void*) {
    const uint8_t* ti = TabDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_TabDescription_GetFileVersion(const void*) { return 0x104; }

extern "C" void* RtUi_TabDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 18004c390
    return RtUi_TabDescription_Ctor(selfv);
}
extern "C" void* RtUi_TabDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtUi_TabDescription_Ctor(selfv);
}
extern "C" void RtUi_TabDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1TabDescription@UI@@UEAA@XZ   0x1800da8e0
// ============================================================================
// Restamp, destroy the tab Array (0x18004cc90 — frees the buffer and does NOT
// visit the pairs), release the three Strings in REVERSE order (+0x98, +0x90,
// +0x88), restamp the base vtable, release _textName at +0x60, tail JMP.
// The four ExternalPtrs at +0x68..+0x80 are not released.
extern "C" void RtUi_TabDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtTabDescription();                                   // 1800da8fb

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x18004cc90ull)(p + 0xa8);             // 1800da90c

    ReleaseStringField(reinterpret_cast<char**>(p + 0x98));    // 1800da912
    ReleaseStringField(reinterpret_cast<char**>(p + 0x90));    // 1800da945
    ReleaseStringField(reinterpret_cast<char**>(p + 0x88));    // 1800da977

    *v = VtElementDescription();                               // 1800da9a9
    ReleaseStringField(reinterpret_cast<char**>(p + 0x60));    // 1800da9b3

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 1800da9e6 JMP
}

// ============================================================================
// ?Serialize@TabDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800da760
// ============================================================================
// The scalar count is read into a LOCAL (`MOV EAX,[RDI+0xb0]` ->
// `MOV [RSP+0x50],EAX`) and the local is what gets serialized and what the
// read-path SetSize and the element loop then use — not `_tabs._size`, which
// SetSize is in the middle of changing.
extern "C" bool RtUi_TabDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180116b00ull)(p, stream);              // 1800da77a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800da7aa

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x68, &ser);        // 1800da7b8
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x70, &ser);        // 1800da7c6
    RtAddr<FieldSerFn>(0x180038f90ull)(p + 0x78, &ser);        // 1800da7d4
    RtAddr<FieldSerFn>(0x180004590ull)(p + 0x80, &ser);        // 1800da7e5 ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x88, &ser);        // 1800da7f6
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x90, &ser);        // 1800da807
    RtAddr<FieldSerFn>(0x1800c9930ull)(p + 0x98, &ser);        // 1800da818

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0xa0, nullptr);

    int32_t n = *reinterpret_cast<int32_t*>(p + 0xb0);         // 1800da835
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                               // 1800da85a (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x18004cdf0ull)(p + 0xa8, n);        // 1800da86b
    }

    // The pair loop: BOTH halves of every element, through ONE array-data
    // pointer read BEFORE the loop (0x1800da870) — retail does not re-read it,
    // which is safe here only because SetSize has already finished.
    uint8_t* tab = *reinterpret_cast<uint8_t**>(p + 0xa8);
    for (int32_t i = 0; i < n; ++i, tab += 0x10) {             // 1800da87d / 1800da8a3
        RtAddr<FieldSerFn>(0x180038f90ull)(tab,       &ser);   // 1800da889
        RtAddr<FieldSerFn>(0x180038f90ull)(tab + 8,   &ser);   // 1800da897
    }

    if (rw != nullptr) {                                       // 1800da8ad
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@TabDescription@UI@@...   retail 0x1800da410
// ============================================================================
// TabElement is 0x108 bytes (`MOV EDX,0x108` at 0x1800da454) and its ctor is
// CALLED out of line (0x1800da390, already ported by ui_factory_wave28.cpp).
//
// SIX child creations: the three fixed sub-elements from +0x68/+0x70/+0x78,
// then per tab the pair at tabs[i][0] and tabs[i][8].
//
// *** THE SECOND CHILD OF EACH TAB USES A DIFFERENT STACK SLOT. *** Five of the
// six `Dialog::CreateElement` calls pass `LEA RDX,[RSP+0x70]`; the one at
// 0x1800da604 passes `LEA RDX,[RSP+0x78]`. Those are two distinct
// InternalPtr<Element> locals, so the two are kept as two locals here. Using one
// would make the second call overwrite a slot retail leaves alone, and
// InternalPtr construction is not free of side effects on the handle table.
//
// Then three sprite lookups with the SAME dword-written/qword-read slot the
// Image and Slider appliers have, and finally +0xc0/+0xc4 from the description's
// +0xb0/+0xa0 — note the CROSS: count goes to +0xc0 and the never-initialised
// +0xa0 goes to +0xc4.
extern "C" void** RtUi_TabDescription_CreateInstance(const void* selfv, void** ret,
                                                     const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x108, 8);  // 1800da454 (inlined)

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 1800da481
        using TabElementCtorFn = void* (*)(void*);
        elem = static_cast<uint8_t*>(
            RtAddr<TabElementCtorFn>(0x1800da390ull)(raw));    // 1800da486
    }

    void* slotA = elem;                                        // [RSP+0x70]
    void* slotB = nullptr;                                     // [RSP+0x78]

    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slotA);  // 1800da4a0

    AddChild(dialog, &slotA, elem, *reinterpret_cast<void* const*>(self + 0x68));  // 1800da4b1
    AddChild(dialog, &slotA, elem, *reinterpret_cast<void* const*>(self + 0x70));  // 1800da502
    AddChild(dialog, &slotA, elem, *reinterpret_cast<void* const*>(self + 0x78));  // 1800da552

    const int32_t tabs = *reinterpret_cast<const int32_t*>(self + 0xb0);
    for (int32_t i = 0; i < tabs; ++i) {                        // 1800da596 / 1800da658
        void* const* row =
            *reinterpret_cast<void* const* const*>(self + 0xa8) + i * 2;  // 1800da5a8
        AddChild(dialog, &slotA, elem, row[0]);                // 1800da5bb
        // THE OTHER SLOT — see the banner.
        void* const* row2 =
            *reinterpret_cast<void* const* const*>(self + 0xa8) + i * 2;  // 1800da5f8 (re-read)
        AddChild(dialog, &slotB, elem, row2[1]);               // 1800da60c
    }

    // The three {sheet, index} pairs. ONE stack slot, written 4 bytes wide and
    // read 8 — the same deliberate width mismatch imagedescription.cpp records.
    uint64_t idxSlot;                                          // [RSP+0x30]
    static const int kNameOff[3] = { 0x88, 0x90, 0x98 };
    static const int kElemOff[3] = { 0xc8, 0xd8, 0xe8 };
    for (int i = 0; i < 3; ++i) {
        void* sheet = *reinterpret_cast<void* const*>(self + 0x80);  // RE-READ each time
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

    *reinterpret_cast<int32_t*>(elem + 0xc0) =
        *reinterpret_cast<const int32_t*>(self + 0xb0);        // 1800da71e
    *reinterpret_cast<int32_t*>(elem + 0xc4) =
        *reinterpret_cast<const int32_t*>(self + 0xa0);        // 1800da72a

    using UpdateContainersFn = void (*)(void*);
    RtAddr<UpdateContainersFn>(0x1800d9710ull)(elem);          // 1800da733

    *ret = elem;                                               // 1800da738
    return ret;
}

int RtUiTabDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TabDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_TabDescription_Type);            ++n;
    RegisterRuntimeAddr(0x18004c1d0ull, (void*)&RtUi_TabDescription_Type);

    RegisterRuntimeExport("?TypeName@TabDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TabDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x18004c1f0ull, (void*)&RtUi_TabDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@TabDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_TabDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800da8d0ull, (void*)&RtUi_TabDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TabDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x18004c390ull, (void*)&RtUi_TabDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@TabDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_TabDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x18004c370ull, (void*)&RtUi_TabDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@TabDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_TabDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x18004c3a0ull, (void*)&RtUi_TabDescription_Destruct);

    RegisterRuntimeExport("??0TabDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_TabDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800daa00ull, (void*)&RtUi_TabDescription_Ctor);

    RegisterRuntimeExport("??1TabDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_TabDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800da8e0ull, (void*)&RtUi_TabDescription_Dtor);

    RegisterRuntimeExport("?Serialize@TabDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_TabDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800da760ull, (void*)&RtUi_TabDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@TabDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_TabDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800da410ull, (void*)&RtUi_TabDescription_CreateInstance);

    return n;
}
