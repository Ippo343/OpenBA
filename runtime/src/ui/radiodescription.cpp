// ============================================================================
// runtime/src/ui/radiodescription.cpp — UI::RadioDescription, the whole class
// ============================================================================
//
// Vtable batch 2. THIS CLASS DERIVES FROM UI::GroupDescription, and that is the
// one thing about it that is not shaped like its siblings: three of its bodies
// are one line of its own plus a call into GroupDescription's already-ported
// copy (runtime/src/ui/groupdescription.cpp).
//
// ??_7RadioDescription@UI@@6B@ (retail 0x180157420), ten slots:
//   0  deleting dtor      0x180047930  deleting_dtor.cpp row (sizeof 0x80)
//   1  TypeName           0x180047670  here
//   2  Type               0x180047650  here
//   3  GetFileVersion     0x1800d5b10  here  (returns 0x1104 — per class; the
//                                      siblings return 0x204 / 0x304, so this
//                                      one is emphatically NOT shareable)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d5a80  here
//   6  Reconstruct        0x1800477f0  here
//   7  Destruct           0x180047820  here
//   8  CreateInstance     0x1800d58d0  here
//   9  Construct          0x180047810  here
// plus ??0RadioDescription@UI@@QEAA@XZ 0x1800d5b50 (OUT OF LINE — both
// Construct and Reconstruct tail-jump to it) and
// ??1RadioDescription@UI@@UEAA@XZ 0x1800d5b20.
//
// LAYOUT, sizeof 0x80 (`MOV EDX,0x80` at 0x180047962). GroupDescription (0x78)
// plus ONE field:
//     +0x78  int32  _i78    ctor -1, one kScalarInt in Serialize, and in
//                           CreateInstance the index of the checked box.
//
// EVERY HELPER THIS CLASS CALLS WAS ALREADY PORTED (grepped before writing):
//   0x180117c00 ~GroupDescription, 0x180117d00 GroupDescription::Serialize
//       (groupdescription.cpp)
//   0x180115ad0 ElementDescription::CreateInstance, 0x180112ed0
//       Dialog::CreateElement (dialog.cpp)
//   0x180115e50 ??0Element (element.cpp)
//   0x180076900 Array<InternalPtr>::SetSize (array.cpp)
//   0x1800d5750 RadioElement::FindCheckBoxes (widgets_input.cpp)
// so this file adds no helper of its own.
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
void* VtRadioDescription()   { return RT_DATA_SLOT(void*, "??_7RadioDescription@UI@@6B@"); }
void* VtGroupElement()       { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }
void* VtRadioElement()       { return RT_DATA_SLOT(void*, "??_7RadioElement@UI@@6B@"); }
const uint8_t* RadioDescriptionTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@RadioDescription@UI@@0VBaseTypeInfo@Object@@A");
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

}  // namespace

// ============================================================================
// ??0RadioDescription@UI@@QEAA@XZ   retail 0x1800d5b50
// ============================================================================
// The whole GroupDescription constructor INLINED (the shipped body does not
// call it), then the RadioDescription vtable and the one new field. Written out
// here rather than delegating to groupdescription.cpp's ConstructBody for the
// reason that file's banner gives about its own inlined copy: the sequence is
// the evidence, and the two copies are only equal until one of them is not.
extern "C" void* RtUi_RadioDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d5b61
    *v = VtExternal();                                          // 1800d5b6b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d5b80
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d5b83
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d5b87
    *v = VtElementDescription();                                // 1800d5b8b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d5b95
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d5b99
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d5b9d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d5ba1  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d5ba4  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d5bab  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d5bb3
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d5bb7
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d5bbb
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d5bbf
    *v = VtGroupDescription();                                  // 1800d5bc3
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d5bcd  child array
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d5bd1  size+cap
    *v = VtRadioDescription();                                  // 1800d5bd5
    *reinterpret_cast<int32_t*>(p + 0x78)  = -1;                // 1800d5bdf
    return p;                                                   // 1800d5be6
}

extern "C" uint32_t RtUi_RadioDescription_Type(const void*) {
    const uint8_t* ti = RadioDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_RadioDescription_TypeName(const void*) {
    const uint8_t* ti = RadioDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800d5b10: `MOV EAX,0x1104 ; RET`.
extern "C" int32_t RtUi_RadioDescription_GetFileVersion(const void*) { return 0x1104; }

extern "C" void* RtUi_RadioDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180047810
    return RtUi_RadioDescription_Ctor(selfv);
}
extern "C" void* RtUi_RadioDescription_Reconstruct(void* selfv) {
    CallSlot0(selfv, 0);                                       // 1800477fe
    return RtUi_RadioDescription_Ctor(selfv);                  // 180047808
}
extern "C" void RtUi_RadioDescription_Destruct(void* selfv) { CallSlot0(selfv, 0); }

// ============================================================================
// ??1RadioDescription@UI@@UEAA@XZ   0x1800d5b20
// ============================================================================
// Four instructions: restamp this class's vtable, then a TAIL JMP to
// ~GroupDescription (0x180117c00). It owns nothing of its own — +0x78 is an
// int — so there is nothing else to release.
extern "C" void RtUi_RadioDescription_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtRadioDescription();   // 1800d5b32
    using DtorFn = void (*)(void*);
    RtAddr<DtorFn>(0x180117c00ull)(selfv);                     // 1800d5b40 JMP
}

// ============================================================================
// ?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5a80
// ============================================================================
//   GroupDescription::Serialize(this, stream)     0x180117d00, ported
//   rw = encoding factory, read side or write side by stream->mode, version 0
//   rw->vt[0xa0](rw, &_i78, 0)   kScalarInt
//   if (rw) rw->vt[0](rw, 1)
//
// THE BASE CALL IS THE CLASS'S OWN Serialize, NOT ElementDescription's. Every
// other file in this batch opens with 0x180116b00; this one opens with
// 0x180117d00, which itself calls 0x180116b00 and then writes the child array.
// Using the grandparent here would silently drop every child.
//
// It ALSO opens a SECOND ReaderWriter over the same stream, after
// GroupDescription::Serialize has already opened and closed its own. That is
// retail: the pair is per-body, not per-object.
extern "C" bool RtUi_RadioDescription_Serialize(void* selfv, void* streamv) {
    uint8_t* p      = reinterpret_cast<uint8_t*>(selfv);
    Stream*  stream = static_cast<Stream*>(streamv);

    using BaseSerFn = void (*)(void*, void*);
    RtAddr<BaseSerFn>(0x180117d00ull)(p, stream);              // 1800d5a9a

    Serializer ser = { stream, nullptr };                      // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d5aca

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, p + 0x78, nullptr);

    if (rw != nullptr) {                                       // 1800d5aed
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// ?CreateInstance@RadioDescription@UI@@...   retail 0x1800d58d0
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*.
//
// RadioElement is 0xd8 bytes (`MOV EDX,0xd8` at 0x1800d5916) and its
// construction stamps TWO vtables in a row — GroupElement (0x1800d594c) and
// then RadioElement (0x1800d5956) — before zeroing its own three fields:
//     +0xc0 Array<CheckBoxElement*>.data, +0xc8 .size/.cap, +0xd0 int32 = -1
//
// The child loop is GroupDescription's, transcribed here rather than shared:
// the shipped body has its own copy and the two differ in which element the
// children are parented to.
//
// THE TAIL IS THIS CLASS'S WHOLE POINT and it is easy to get subtly wrong:
//   FindCheckBoxes(elem, &elem)                        0x1800d5750, ported
//   if (desc->_i78 != elem->+0xd0) {
//       elem->+0xd0 = desc->_i78;
//       for (i = 0; i < elem->_checkBoxes.size; ++i)
//           if (i != elem->+0xd0) checkBoxes[i]->+0xd8 = 0;   // UNCHECK
//   }
// The guard compares against the element's CURRENT value and skips the whole
// loop when they already agree, and the inner test re-reads `elem->+0xd0`
// (0x1800d5a30) rather than the register it was just written from. Both are
// retail's and both are kept: the second one means the loop reads a field the
// first line of the body wrote, not a stale copy.
extern "C" void** RtUi_RadioDescription_CreateInstance(const void* selfv, void** ret,
                                                       const void* dialog) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xd8, 8);   // 1800d5916 (inlined)

    uint8_t* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d5941
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);            // 1800d5946 ??0Element
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtGroupElement();       // 1800d594c
        *reinterpret_cast<void**>(e) = VtRadioElement();       // 1800d5956
        *reinterpret_cast<uint64_t*>(e + 0xc0) = 0;            // 1800d5962
        *reinterpret_cast<uint64_t*>(e + 0xc8) = 0;            // 1800d5969
        *reinterpret_cast<int32_t*>(e + 0xd0)  = -1;           // 1800d5970  DWORD
        elem = e;
    }

    void* slot = elem;                                         // 1800d5980
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 1800d598d

    const int32_t count = *reinterpret_cast<const int32_t*>(self + 0x70);
    for (int32_t i = 0; i < count; ++i) {                      // 1800d5994 / 1800d59f9
        void* const* descArray = *reinterpret_cast<void* const* const*>(self + 0x68);
        void*        desc      = descArray[i];                 // 1800d59a7

        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, desc);

        uint8_t* child = *reinterpret_cast<uint8_t**>(made);   // 1800d59b8
        if (child != nullptr)
            *reinterpret_cast<void**>(child + 0x98) = elem;    // 1800d59c0

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(elem + 0xa0,
                                       *reinterpret_cast<int32_t*>(elem + 0xa8) + 1,
                                       made);                  // 1800d59d9

        for (uint8_t* n = elem; n != nullptr;                   // 1800d59e1..1800d59f5
             n = *reinterpret_cast<uint8_t**>(n + 0x98)) {
            *reinterpret_cast<uint32_t*>(n + 0x34) |= UI::kElementFlagNeedsSizeCalc;
        }
    }

    void* self2 = elem;                                        // 1800d59fe  (a SECOND slot)
    using FindCheckBoxesFn = void (*)(void* elem, void** root);
    RtAddr<FindCheckBoxesFn>(0x1800d5750ull)(elem, &self2);    // 1800d5a0b

    const int32_t want = *reinterpret_cast<const int32_t*>(self + 0x78);
    if (want != *reinterpret_cast<int32_t*>(elem + 0xd0)) {    // 1800d5a13
        *reinterpret_cast<int32_t*>(elem + 0xd0) = want;       // 1800d5a1b
        const int32_t boxes = *reinterpret_cast<int32_t*>(elem + 0xc8);
        for (int32_t i = 0; i < boxes; ++i) {                  // 1800d5a21 / 1800d5a50
            if (i == *reinterpret_cast<int32_t*>(elem + 0xd0)) continue;  // 1800d5a30 RE-READ
            uint8_t* const* arr = *reinterpret_cast<uint8_t* const* const*>(elem + 0xc0);
            arr[i][0xd8] = 0;                                  // 1800d5a43
        }
    }

    *ret = elem;                                               // 1800d5a58
    return ret;
}

int RtUiRadioDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RadioDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_RadioDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180047650ull, (void*)&RtUi_RadioDescription_Type);

    RegisterRuntimeExport("?TypeName@RadioDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_RadioDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180047670ull, (void*)&RtUi_RadioDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@RadioDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_RadioDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d5b10ull, (void*)&RtUi_RadioDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RadioDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047810ull, (void*)&RtUi_RadioDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RadioDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800477f0ull, (void*)&RtUi_RadioDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@RadioDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_RadioDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047820ull, (void*)&RtUi_RadioDescription_Destruct);

    RegisterRuntimeExport("??0RadioDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_RadioDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d5b50ull, (void*)&RtUi_RadioDescription_Ctor);

    RegisterRuntimeExport("??1RadioDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_RadioDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d5b20ull, (void*)&RtUi_RadioDescription_Dtor);

    RegisterRuntimeExport("?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_RadioDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d5a80ull, (void*)&RtUi_RadioDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_RadioDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d58d0ull, (void*)&RtUi_RadioDescription_CreateInstance);

    return n;
}
