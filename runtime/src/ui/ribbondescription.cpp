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
// LAYOUT: runtime/types/ui/RibbonDescription.h (sizeof 0x80; RULE B6-8 moved
// this file's offsets there under the retail name, named after the
// RibbonElement fields each one seeds).
//
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::RibbonDescription`; each registered door is a one-line QUALIFIED
// delegation. The dtor stays a door (B4b-2).
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
#include "../../types/ui/RibbonDescription.h"
#include "../../types/ui/RibbonElement.h"

#include <atomic>
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// RULE B6-5: the members below (Construct/Reconstruct run ConstructBody) may
// run on the loader thread, so every export they resolve lives in a
// namespace-scope atomic, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtElementDescription{nullptr};
std::atomic<void*> g_vtRibbonDescription{nullptr};
std::atomic<void*> g_vtRibbonElement{nullptr};
void* VtBase()               { return AtomicExport(g_vtBase, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return AtomicExport(g_vtExternal, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return AtomicExport(g_vtElementDescription, "??_7ElementDescription@UI@@6B@"); }
void* VtRibbonDescription()  { return AtomicExport(g_vtRibbonDescription, "??_7RibbonDescription@UI@@6B@"); }
void* VtRibbonElement()      { return AtomicExport(g_vtRibbonElement, "??_7RibbonElement@UI@@6B@"); }

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
// The MEMBERS
// ============================================================================
namespace {
std::atomic<void*> g_typeInfo{nullptr};
const uint8_t* RibbonTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@RibbonDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
}  // namespace

namespace UI {

// [1] 0x180044130 / [2] 0x180044110
const char* RibbonDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, RibbonTypeInfo() + 8, sizeof v); return v;
}
uint32_t RibbonDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, RibbonTypeInfo(), sizeof v); return v;
}
// [3] 0x1800d5ef0: `MOV EAX,0x204 ; RET`. 0x204, not GroupDescription's 0x104 —
// the file version is per class and the two must not be shared.
int32_t RibbonDescription::GetFileVersion() const { return 0x204; }

// [9] 0x180044470 — the ctor INLINED (ConstructBody); the null-`this` test
// stays in the door.
Object::External* RibbonDescription::Construct() {
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;
}
// [6] 0x1800443c0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* RibbonDescription::Reconstruct() {
    DeletingDtor(0);                                                    // 1800443d7
    ConstructBody(reinterpret_cast<uint8_t*>(this));
    return this;
}
// [7] 0x180044520 is three instructions: `MOV RAX,[RCX] ; XOR EDX,EDX ; JMP [RAX]`
// — a tail jump into slot 0 with flag 0, i.e. destroy without freeing.
void RibbonDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// [5] ?Serialize@RibbonDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5f00
// ============================================================================
//   ElementDescription::Serialize(this, stream)     0x180116b00, ported
//   rw = encoding factory, read side or write side by stream->mode
//   rw->vt[0x78](rw, &_vertical, 0)  kScalarBool   1800d5f5e
//   rw->vt[0x78](rw, &_uniform, 0)   kScalarBool   1800d5f70
//   rw->vt[0xa0](rw, &_spacing, 0)   kScalarInt    1800d5f82
//   n = _items._size ; rw->vt[0xa0](rw, &n, 0)                 1800d5f9f
//   if (pair.stream->mode == 0) SetSize(&_items, n)            0x1800492d0
//   ElementDescriptionPtrArray_Serialize(&pair, n, _items._data)  0x1800493d0
//   if (rw) rw->vt[0](rw, 1)
//
// THE FIELD ORDER IS 0x68, 0x69, 0x6c — NOT 0x68, 0x6c, 0x69. CreateInstance
// copies them in the OTHER order (0x68, 0x6c, 0x69 at
// 1800d60bf..1800d60d6) and a port that took its order from there would emit
// the two bools and the int in the wrong sequence into every saved layout,
// which the reader would then mis-parse. Each body's order is its own.
bool RibbonDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 1800d5f1a

    Serializer ser = { stream, nullptr };                               // [RSP+0x28], [RSP+0x30]

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;                // Create{Reader,Writer}
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int zero);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800d5f4a

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_vertical, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarBool / 8])(rw, &_uniform, nullptr);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt  / 8])(rw, &_spacing, nullptr);

    int32_t n = _itemsCount;                                            // 1800d5f88
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                                        // 1800d5fa5 (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x1800492d0ull)(&_itemsData, n);              // 1800d5fb8
    }

    using ElemSerFn = void (*)(Serializer* ser, int count, void* data);
    RtAddr<ElemSerFn>(0x1800493d0ull)(&ser, n, _itemsData);

    if (rw != nullptr) {                                                // 1800d5fd5
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// [8] ?CreateInstance@RibbonDescription@UI@@...   retail 0x1800d6000
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = the
// Dialog*. (See groupdescription.cpp for why Ghidra prints only two of the
// three.)
//
// RibbonElement is 0xd8 bytes (`MOV EDX,0xd8` at 0x1800d6048); retail INLINES
// its ctor: ??0Element, the vtable stamp, ONE 16-bit store over +0xc0..+0xc1,
// then +0xc4, +0xc8, +0xd0 (1800d6088..1800d60a0).
//
// The LAST call is the one that makes this class different from Group's:
// `Array<int32>::SetSize(&elem->_extents, desc->_items._size)` (0x18002fea0,
// array.cpp kSpec_I32 — the instantiation whose new tail is UNINITIALISED).
// It runs AFTER the child loop and OUTSIDE it.
Object::InternalPtr<Element>* RibbonDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0xd8, 8);           // 1800d6048

    RibbonElement* elem = nullptr;
    if (raw != nullptr) {                                               // 1800d6070
        using ElementCtorFn = void* (*)(void*);
        RtAddr<ElementCtorFn>(0x180115e50ull)(raw);                     // 1800d6078 ??0Element
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<void**>(e) = VtRibbonElement();              // 1800d6085
        *reinterpret_cast<uint16_t*>(e + 0xc0) = 0;                     // 1800d6088  WORD
        elem = static_cast<RibbonElement*>(raw);
        elem->_spacing         = 0;                                     // 1800d6093
        elem->_extentsData     = nullptr;                               // 1800d6099 (qword)
        *reinterpret_cast<uint64_t*>(&elem->_extentsCount) = 0;         // 1800d60a0 (qword)
    }

    void* slot = elem;                                                  // 1800d60ad [RSP+0x50]

    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 1800d60ba
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    elem->_vertical = _vertical;                                        // 1800d60bf
    elem->_spacing  = _spacing;                                         // 1800d60c9
    elem->_uniform  = _uniform;                                         // 1800d60d2

    const int32_t count = _itemsCount;
    for (int32_t i = 0; i < count; ++i) {                               // 1800d60dc / 1800d6149
        void* desc = _itemsData[i];                                     // 1800d60f7

        // UI::Dialog::CreateElement (block B's class) — a door, by address.
        using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
        void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, &slot, desc);

        Element* child = *reinterpret_cast<Element**>(made);            // 1800d6108
        if (child != nullptr)
            child->_parent = elem;                                      // 1800d6110

        using PushFn = void (*)(void* array, int n, const void* fill);
        RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                       elem->_children.GetSize() + 1,
                                       made);                           // 1800d6129

        for (Element* n = elem; n != nullptr; n = n->_parent)           // 1800d6131..1800d6145
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    using SetSizeFn = void (*)(void* array, int n);
    RtAddr<SetSizeFn>(0x18002fea0ull)(&elem->_extentsData, count);      // 1800d6158

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d615d
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_RibbonDescription_Type(const void* self) {
    return reinterpret_cast<const UI::RibbonDescription*>(self)->UI::RibbonDescription::Type();
}
extern "C" const char* RtUi_RibbonDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::RibbonDescription*>(self)->UI::RibbonDescription::TypeName();
}
extern "C" int32_t RtUi_RibbonDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::RibbonDescription*>(self)
        ->UI::RibbonDescription::GetFileVersion();
}
extern "C" void* RtUi_RibbonDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                                // 180044481 (retail null test)
    return reinterpret_cast<UI::RibbonDescription*>(self)->UI::RibbonDescription::Construct();
}
extern "C" void* RtUi_RibbonDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::RibbonDescription*>(self)->UI::RibbonDescription::Reconstruct();
}
extern "C" void RtUi_RibbonDescription_Destruct(void* self) {
    reinterpret_cast<UI::RibbonDescription*>(self)->UI::RibbonDescription::Destruct();
}
extern "C" bool RtUi_RibbonDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::RibbonDescription*>(self)->UI::RibbonDescription::Serialize(stream);
}
extern "C" void** RtUi_RibbonDescription_CreateInstance(const void* self, void** ret,
                                                        const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::RibbonDescription*>(self)
            ->UI::RibbonDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
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
