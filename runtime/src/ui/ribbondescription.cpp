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
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

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

}  // namespace

// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
}  // namespace

namespace UI {

// [3] 0x1800d5ef0: `MOV EAX,0x204 ; RET`. 0x204, not GroupDescription's 0x104 —
// the file version is per class and the two must not be shared.
int32_t RibbonDescription::GetFileVersion() const { return 0x204; }

// [9] 0x180044470 — the ctor INLINED in retail: placement-new the class (the
// null-`this` test sat in the retired door; a virtual call has a `this`).
Object::External* RibbonDescription::Construct() {
    return ::new (this) RibbonDescription();
}
// [6] 0x1800443c0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* RibbonDescription::Reconstruct() {
    DeletingDtor(0);                                                    // 1800443d7
    return ::new (this) RibbonDescription();
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
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int zero);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800d5f4a

    rw->SerializeBool(reinterpret_cast<bool&>(_vertical), nullptr);
    rw->SerializeBool(reinterpret_cast<bool&>(_uniform), nullptr);
    rw->SerializeInt(_spacing, nullptr);

    int32_t n = _itemsCount;                                            // 1800d5f88
    rw->SerializeInt(n, nullptr);

    if (ser.stream->mode == 0) {                                        // 1800d5fa5 (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x1800492d0ull)(&_itemsData, n);              // 1800d5fb8
    }

    using ElemSerFn = void (*)(Serializer* ser, int count, void* data);
    RtAddr<ElemSerFn>(0x1800493d0ull)(&ser, n, _itemsData);

    if (rw != nullptr) {                                                // 1800d5fd5
        rw->DeletingDtor(1);
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

    // Retail inlines ??0RibbonElement (0x1800d6078 .. 0x1800d60a0): the real ctor.
    RibbonElement* elem = nullptr;
    if (raw != nullptr)                                                 // 1800d6070
        elem = ::new (raw) RibbonElement();                             // 1800d6078 ??0Element + stores

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

// ??1RibbonDescription@UI@@UEAA@XZ   0x1800d5c90 — retail's body, moved from
// the retired door (ENDGAME B.3.9 UI-B): the child Array at +0x70 through
// 0x180039440. The restamps, the base's asymmetric _textName (+0x60) release
// and the tail JMP into ~External are the compiler's chain.
RibbonDescription::~RibbonDescription() {
    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180039440ull)(reinterpret_cast<uint8_t*>(this) + 0x70);   // 1800d5cb4
}

// ?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ 0x180044220 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x80, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* RibbonDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(RibbonDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) RibbonDescription();
}
// ?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180044310
Object::Base* RibbonDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) RibbonDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7RibbonDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// RibbonDescription now wears the compiler's table.
// ============================================================================

int RtUiRibbonDescriptionRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@RibbonDescription` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@RibbonDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::RibbonDescription::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x180044220ull, (void*)&UI::RibbonDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@RibbonDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::RibbonDescription::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x180044310ull, (void*)&UI::RibbonDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::RibbonDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@RibbonDescription@UI@@ (0x1801fa2a8): name "RibbonDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@RibbonDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::RibbonDescription, "ElementDescription", nullptr)
