// ============================================================================
// runtime/src/ui/pagedescription.cpp — UI::PageDescription, the whole class
// ============================================================================
//
// Vtable batch 2. Like UI::RadioDescription, this class DERIVES FROM
// UI::GroupDescription: its dtor tail-jumps to ~GroupDescription and its
// Serialize opens with GroupDescription::Serialize, both already ported in
// runtime/src/ui/groupdescription.cpp.
//
// ??_7PageDescription@UI@@6B@ (retail 0x1801570e0), ten slots:
//   0  deleting dtor      0x1800471a0  deleting_dtor.cpp row (sizeof 0x80)
//   1  TypeName           0x180046ee0  here
//   2  Type               0x180046ec0  here
//   3  GetFileVersion     0x1800d54a0  here  (returns 0x1104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d5410  here
//   6  Reconstruct        0x180047060  here
//   7  Destruct           0x180047090  here
//   8  CreateInstance     0x1800d52b0  here
//   9  Construct          0x180047080  here
// plus ??0PageDescription@UI@@QEAA@XZ 0x1800d54e0 (out of line) and
// ??1PageDescription@UI@@UEAA@XZ 0x1800d54b0.
//
// LAYOUT, sizeof 0x80 (`MOV EDX,0x80` at 0x1800471d2). GroupDescription (0x78)
// plus ONE field:
//     +0x78  int32  _activePage   ctor 0, one kScalarInt, and the argument of
//                                 the second SetActivePage call at the tail of
//                                 CreateInstance. THIS ONE HAS A NAME because a
//                                 second body says what it is --
//                                 ?SetActivePage@PageElement@UI@@, ported in
//                                 runtime/src/ui/widgets_layout.cpp; the sibling
//                                 classes' fields do not and are left as offsets.
//
// NO NON-SLOT HELPER IS PORTED HERE. ?SetActivePage@PageElement@UI@@ was, and
// was dropped at the merge -- see the note above CreateInstance.
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
void* VtGroupDescription()   { return RT_DATA_SLOT(void*, "??_7GroupDescription@UI@@6B@"); }
void* VtPageDescription()    { return RT_DATA_SLOT(void*, "??_7PageDescription@UI@@6B@"); }
void* VtGroupElement()       { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }
void* VtPageElement()        { return RT_DATA_SLOT(void*, "??_7PageElement@UI@@6B@"); }
const uint8_t* PageDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@PageDescription@UI@@0VBaseTypeInfo@Object@@A");
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

using Slot0Fn = void* (*)(void* self, unsigned flag);
void CallSlot0(void* self, unsigned flag) {
    reinterpret_cast<Slot0Fn>(VTableOf(self)[0])(self, flag);
}

// The dirty-chain walk this file needs in three places: OR the needs-size-calc
// bit into an element and every ancestor. Retail writes it out each time.
void MarkChainDirty(uint8_t* e) {
    for (uint8_t* n = e; n != nullptr; n = *reinterpret_cast<uint8_t**>(n + 0x98))
        *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
}

}  // namespace

// ============================================================================
// ??0PageDescription@UI@@QEAA@XZ   retail 0x1800d54e0
// ============================================================================
extern "C" void* RtUi_PageDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d54f1
    *v = VtExternal();                                          // 1800d54fb
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d5510
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d5513
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d5517
    *v = VtElementDescription();                                // 1800d551b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d5525
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d5529
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d552d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d5531  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d5534  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d553b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d5543
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d5547
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d554b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d554f
    *v = VtGroupDescription();                                  // 1800d5553
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d555d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d5561
    *v = VtPageDescription();                                   // 1800d5565
    // ZERO, not -1. RadioDescription's identically-placed field is `-1` and
    // this one is `MOV dword [RCX+0x78],EDX` with EDX already zeroed.
    *reinterpret_cast<int32_t*>(p + 0x78)  = 0;                 // 1800d556f
    return p;                                                   // 1800d5572
}

extern "C" uint32_t RtUi_PageDescription_Type(const void*) {
    const uint8_t* ti = PageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_PageDescription_TypeName(const void*) {
    const uint8_t* ti = PageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
extern "C" int32_t RtUi_PageDescription_GetFileVersion(const void*) { return 0x1104; }

extern "C" void* RtUi_PageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180047080
    return RtUi_PageDescription_Ctor(selfv);
}
extern "C" void* RtUi_PageDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);
    return RtUi_PageDescription_Ctor(selfv);
}
extern "C" void RtUi_PageDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ??1PageDescription@UI@@UEAA@XZ  0x1800d54b0 — restamp, then TAIL JMP to
// ~GroupDescription. Nothing of its own to release.
extern "C" void RtUi_PageDescription_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtPageDescription();    // 1800d54c2
    using DtorFn = void (*)(void*);
    RtAddr<DtorFn>(0x180117c00ull)(selfv);                     // 1800d54d0 JMP
}

// ============================================================================
// ?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5410
// ============================================================================
// GroupDescription::Serialize (NOT ElementDescription's — using the
// grandparent would drop every child), then one kScalarInt over +0x78 through
// a SECOND ReaderWriter opened on the same stream.
extern "C" bool RtUi_PageDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180117d00ull)(p, stream);              // 1800d542a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d545a

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x78, nullptr);

    if (rw != nullptr) {                                       // 1800d547d
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?SetActivePage@PageElement@UI@@QEAAXH@Z (0x1800d5110) IS NOT HERE.
// ============================================================================
// It was ported in this file and dropped at the merge (2026-09-12):
// runtime/src/ui/widgets_layout.cpp has owned it since Wave 7 lane E. The grep
// that missed it looked for a RegisterRuntimeAddr on 0x1800d5110, and that file
// registers it by NAME only -- which is exactly the shape the duplicate-export
// scan catches. CreateInstance below calls it by that name.
// ============================================================================

// ============================================================================
// ?CreateInstance@PageDescription@UI@@...   retail 0x1800d52b0
// ============================================================================
// RCX = this, RDX = hidden return slot, R8 = Dialog*.
//
// PageElement is 0xc8 bytes (`MOV EDX,0xc8` at 0x1800d52f8); construction
// stamps GroupElement then PageElement and zeroes ONE dword at +0xc0.
//
// THE TAIL CALLS SetActivePage TWICE, WITH -1 FIRST. The first call forces the
// field away from whatever the child loop left and hides every page; the second
// applies the description's own value. Collapsing them to one call with the
// final value would skip the hide pass whenever the field already equalled it,
// because SetActivePage's own first guard returns early on equality.
extern "C" void** RtUi_PageDescription_CreateInstance(const void* selfv, void** ret,
                                                      const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xc8, 8);   // 1800d52f8 (inlined)

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d5323
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 1800d5328
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtGroupElement();       // 1800d532e
        *reinterpret_cast<void**>(e) = VtPageElement();        // 1800d5338
        *reinterpret_cast<int32_t*>(e + 0xc0) = 0;             // 1800d5344  DWORD
        elem = e;
    }

    void* slot = elem;                                         // 1800d5350
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 1800d535d

    const int32_t count = *reinterpret_cast<const int32_t*>(self + 0x70);
    for (int32_t i = 0; i < count; ++i) {                      // 1800d5362 / 1800d53c9
        void* const* descArray = *reinterpret_cast<void* const* const*>(self + 0x68);
        void*        desc      = descArray[i];                 // 1800d5377

        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, desc);

        uint8_t* child = *reinterpret_cast<uint8_t**>(made);   // 1800d5388
        if (child != nullptr)
            *reinterpret_cast<void**>(child + 0x98) = elem;    // 1800d5390

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                       *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                       made);                  // 1800d53a9

        MarkChainDirty(elem);                                  // 1800d53b1..1800d53c5
    }

    using SetActivePageFn = void (*)(void*, int32_t);
    SetActivePageFn setActivePage =
        RT_EXPORT_ONCE(SetActivePageFn, "?SetActivePage@PageElement@UI@@QEAAXH@Z");
    setActivePage(elem, -1);                                   // 1800d53d4
    setActivePage(elem,
                  *reinterpret_cast<const int32_t*>(self + 0x78));  // 1800d53df

    *ret = elem;                                               // 1800d53e4
    return ret;
}

int RtUiPageDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@PageDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_PageDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180046ec0ull, (void*)&RtUi_PageDescription_Type);

    RegisterRuntimeExport("?TypeName@PageDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_PageDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180046ee0ull, (void*)&RtUi_PageDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@PageDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_PageDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d54a0ull, (void*)&RtUi_PageDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_PageDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047080ull, (void*)&RtUi_PageDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_PageDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180047060ull, (void*)&RtUi_PageDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@PageDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_PageDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047090ull, (void*)&RtUi_PageDescription_Destruct);

    RegisterRuntimeExport("??0PageDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_PageDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d54e0ull, (void*)&RtUi_PageDescription_Ctor);

    RegisterRuntimeExport("??1PageDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_PageDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d54b0ull, (void*)&RtUi_PageDescription_Dtor);

    RegisterRuntimeExport("?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_PageDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d5410ull, (void*)&RtUi_PageDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_PageDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d52b0ull, (void*)&RtUi_PageDescription_CreateInstance);

    return n;
}
