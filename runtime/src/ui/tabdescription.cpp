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
// LAYOUT: runtime/types/ui/TabDescription.h (sizeof 0xb8; RULE B6-8 moved
// this file's offsets there under the retail name). +0xa0 IS NEVER WRITTEN
// BY THE CONSTRUCTOR (0x1800daa9e -> 0x1800daaa5): it is serialized and
// copied to the element's _gap, so a description that was never
// deserialized hands the element whatever the allocator left. Retail's.
//
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::TabDescription`; each registered door is a one-line QUALIFIED
// delegation. CreateInstance's by-address UpdateContainers (0x1800d9710)
// is now the member call. The ctor / dtor stay doors (B4b-2).
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
#include "../../types/ui/TabDescription.h"
#include "../../types/ui/TabElement.h"

#include <atomic>
#include <new>

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

void* VtBase()               { return RT_DATA_SLOT(void*, "??_7Base@Object@@6B@"); }
void* VtExternal()           { return RT_DATA_SLOT(void*, "??_7External@Object@@6B@"); }
void* VtElementDescription() { return RT_DATA_SLOT(void*, "??_7ElementDescription@UI@@6B@"); }
void* VtTabDescription()     { return RT_DATA_SLOT(void*, "??_7TabDescription@UI@@6B@"); }
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
void AddChild(const void* dialog, void** slot, UI::Element* elem, void* childDesc) {
    // UI::Dialog::CreateElement (block B's class) — a door, by address.
    using CreateElementFn = void** (*)(const void* dlg, void** ret, void* desc);
    void** made = RtAddr<CreateElementFn>(0x180112ed0ull)(dialog, slot, childDesc);

    UI::Element* child = *reinterpret_cast<UI::Element**>(made);
    if (child != nullptr)
        child->_parent = elem;

    using PushFn = void (*)(void* array, int n, const void* fill);
    RtAddr<PushFn>(0x180076900ull)(&elem->_children, elem->_children.GetSize() + 1, made);

    for (UI::Element* n = elem; n != nullptr; n = n->_parent)
        n->_flags |= UI::kElementFlagNeedsSizeCalc;
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
// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
// RULE B6-5: namespace-scope atomics, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_getSpriteIndex{nullptr};
const uint8_t* TabTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@TabDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ — block D's class, a door.
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    using GetIdxFn = int32_t (*)(const void* sheet, const void* nameStr);
    return reinterpret_cast<GetIdxFn>(AtomicExport(
        g_getSpriteIndex,
        "?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z"))(sheet, nameStr);
}
}  // namespace

namespace UI {

// [1] 0x18004c1f0 / [2] 0x18004c1d0 / [3] 0x1800da8d0
const char* TabDescription::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, TabTypeInfo() + 8, sizeof v); return v;
}
uint32_t TabDescription::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, TabTypeInfo(), sizeof v); return v;
}
int32_t TabDescription::GetFileVersion() const { return 0x104; }

// [9] 0x18004c390 — the null-`this` test stays in the door.
Object::External* TabDescription::Construct() {
    return reinterpret_cast<Object::External*>(RtUi_TabDescription_Ctor(this));
}
// [6] 0x18004c370 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* TabDescription::Reconstruct() {
    DeletingDtor(0);
    return reinterpret_cast<Object::External*>(RtUi_TabDescription_Ctor(this));
}
// [7] 0x18004c3a0
void TabDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// [5] ?Serialize@TabDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800da760
// ============================================================================
// The scalar count is read into a LOCAL (`MOV EAX,[RDI+0xb0]` ->
// `MOV [RSP+0x50],EAX`) and the local is what gets serialized and what the
// read-path SetSize and the element loop then use — not `_tabs._size`, which
// SetSize is in the middle of changing.
bool TabDescription::Serialize(void* streamArg) {
    Stream* stream = static_cast<Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 1800da77a

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = void* (*)(void* factory, Serializer* ser, int version);
    void* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800da7aa

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_stripBackdropDescription, &ser);   // 1800da7b8
    RtAddr<FieldSerFn>(0x180038f90ull)(&_pageBackdropDescription, &ser);    // 1800da7c6
    RtAddr<FieldSerFn>(0x180038f90ull)(&_pageFrameDescription, &ser);       // 1800da7d4
    RtAddr<FieldSerFn>(0x180004590ull)(&_spriteSheet, &ser);            // 1800da7e5 ExternalPtr<SpriteSheet>
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_edgeLeftName, &ser);           // 1800da7f6
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_edgeMiddleName, &ser);         // 1800da807
    RtAddr<FieldSerFn>(0x1800c9930ull)(&_edgeRightName, &ser);          // 1800da818

    using ScalarFn = void (*)(void* rw, void* value, const char* name);
    void** rvt = VTableOf(rw);
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &_gap, nullptr);

    int32_t n = _tabsCount;                                             // 1800da835
    reinterpret_cast<ScalarFn>(rvt[rt::io::rw_vtbl::kScalarInt / 8])(rw, &n, nullptr);

    if (ser.stream->mode == 0) {                                        // 1800da85a (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        RtAddr<SetSizeFn>(0x18004cdf0ull)(&_tabsData, n);               // 1800da86b
    }

    // The pair loop: BOTH halves of every element, through ONE array-data
    // pointer read BEFORE the loop (0x1800da870) — retail does not re-read it,
    // which is safe here only because SetSize has already finished.
    uint8_t* tab = reinterpret_cast<uint8_t*>(_tabsData);
    for (int32_t i = 0; i < n; ++i, tab += 0x10) {                      // 1800da87d / 1800da8a3
        RtAddr<FieldSerFn>(0x180038f90ull)(tab,       &ser);            // 1800da889
        RtAddr<FieldSerFn>(0x180038f90ull)(tab + 8,   &ser);            // 1800da897
    }

    if (rw != nullptr) {                                                // 1800da8ad
        using DeleteFn = void (*)(void* rw, int flag);
        reinterpret_cast<DeleteFn>(rvt[rt::io::rw_vtbl::kDtor / 8])(rw, 1);
    }
    return true;
}

// ============================================================================
// [8] ?CreateInstance@TabDescription@UI@@...   retail 0x1800da410
// ============================================================================
// TabElement is 0x108 bytes (`MOV EDX,0x108` at 0x1800da454) and its ctor is
// CALLED out of line (0x1800da390, tabelement.cpp's door).
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
// +0xb0/+0xa0 — note the CROSS: count goes to _tabCount and the never-initialised
// +0xa0 goes to _gap.
Object::InternalPtr<Element>* TabDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    using MemAllocFn = void* (*)(uint64_t size, int align);
    void* raw = RtAddr<MemAllocFn>(0x1800c7a50ull)(0x108, 8);          // 1800da454 (inlined)

    TabElement* elem = nullptr;
    if (raw != nullptr) {                                               // 1800da481
        using TabElementCtorFn = void* (*)(void*);
        elem = static_cast<TabElement*>(
            RtAddr<TabElementCtorFn>(0x1800da390ull)(raw));             // 1800da486
    }

    void* slotA = elem;                                                 // [RSP+0x70]
    void* slotB = nullptr;                                              // [RSP+0x78]

    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 1800da4a0
        reinterpret_cast<Object::InternalPtr<Element>*>(&slotA));  // B.3.6 bridge

    AddChild(dialog, &slotA, elem, _stripBackdropDescription);          // 1800da4b1
    AddChild(dialog, &slotA, elem, _pageBackdropDescription);           // 1800da502
    AddChild(dialog, &slotA, elem, _pageFrameDescription);              // 1800da552

    const int32_t tabs = _tabsCount;
    for (int32_t i = 0; i < tabs; ++i) {                                // 1800da596 / 1800da658
        void* const* row = _tabsData + i * 2;                           // 1800da5a8
        AddChild(dialog, &slotA, elem, row[0]);                         // 1800da5bb
        // THE OTHER SLOT — see the banner.
        void* const* row2 = _tabsData + i * 2;                          // 1800da5f8 (re-read)
        AddChild(dialog, &slotB, elem, row2[1]);                        // 1800da60c
    }

    // The three {sheet, index} pairs. ONE stack slot, written 4 bytes wide and
    // read 8 — the same deliberate width mismatch imagedescription.cpp records.
    uint64_t idxSlot;                                                   // [RSP+0x30]
    char* const* const names[3] = { &_edgeLeftName, &_edgeMiddleName, &_edgeRightName };
    ::Sprite* const sprites[3] = { &elem->_edgeLeft, &elem->_edgeMiddle, &elem->_edgeRight };
    for (int i = 0; i < 3; ++i) {
        void* sheet = _spriteSheet;                                     // RE-READ each time
        if (sheet != nullptr) {
            const int32_t idx = GetSpriteIndexFromName(sheet, names[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);               // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        uint8_t* s = reinterpret_cast<uint8_t*>(sprites[i]);
        *reinterpret_cast<void**>(s)        = sheet;
        *reinterpret_cast<uint64_t*>(s + 8) = idxSlot;                  // QWORD store
    }

    elem->_tabCount = _tabsCount;                                       // 1800da71e
    elem->_gap      = _gap;                                             // 1800da72a

    elem->UpdateContainers();                                           // 1800da733 (was by address)

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800da738
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_TabDescription_Type(const void* self) {
    return reinterpret_cast<const UI::TabDescription*>(self)->UI::TabDescription::Type();
}
extern "C" const char* RtUi_TabDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::TabDescription*>(self)->UI::TabDescription::TypeName();
}
extern "C" int32_t RtUi_TabDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::TabDescription*>(self)
        ->UI::TabDescription::GetFileVersion();
}
extern "C" void* RtUi_TabDescription_Construct(void* self) {
    if (self == nullptr) return nullptr;                                // 18004c390 (retail null test)
    return reinterpret_cast<UI::TabDescription*>(self)->UI::TabDescription::Construct();
}
extern "C" void* RtUi_TabDescription_Reconstruct(void* self) {
    return reinterpret_cast<UI::TabDescription*>(self)->UI::TabDescription::Reconstruct();
}
extern "C" void RtUi_TabDescription_Destruct(void* self) {
    reinterpret_cast<UI::TabDescription*>(self)->UI::TabDescription::Destruct();
}
extern "C" bool RtUi_TabDescription_Serialize(void* self, void* stream) {
    return reinterpret_cast<UI::TabDescription*>(self)->UI::TabDescription::Serialize(stream);
}
extern "C" void** RtUi_TabDescription_CreateInstance(const void* self, void** ret,
                                                     const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::TabDescription*>(self)
            ->UI::TabDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
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
