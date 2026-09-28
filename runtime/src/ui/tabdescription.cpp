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
#include "../../types/ui/Dialog.h"
#include "../../types/system/String.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"
#include "../../types/ui/SpriteSheet.h"

#include <atomic>
#include <new>

namespace {

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



void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--*reinterpret_cast<int32_t*>(s - 4) == 0) {
        *field = s - 8;
        System::MemoryAllocator::MemFree(*field);
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
    // ?CreateElement@Dialog@UI@@ 0x180112ed0: RETSLOT, the member's prvalue
    // placed in the caller's slot.
    void** made = slot;
    new (static_cast<void*>(made)) Object::InternalPtr<UI::Element>(
        static_cast<const UI::Dialog*>(dialog)->CreateElement(               // B.3.x bridge
            Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(childDesc))));

    UI::Element* child = *reinterpret_cast<UI::Element**>(made);
    if (child != nullptr)
        child->_parent = elem;

    rtcoll::RtColl_ArraySetSizeFill_180076900(&elem->_children, elem->_children.GetSize() + 1, made);

    for (UI::Element* n = elem; n != nullptr; n = n->_parent)
        n->_flags |= UI::kElementFlagNeedsSizeCalc;
}

}  // namespace



// ============================================================================
// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ (0x1800031f0), called directly.
int32_t GetSpriteIndexFromName(const void* sheet, const void* nameStr) {
    return static_cast<const UI::SpriteSheet*>(sheet)                       // B.3.x bridge
        ->GetSpriteIndexFromName(*static_cast<const System::String*>(nameStr));
}
}  // namespace

namespace UI {

// [3] 0x1800da8d0
int32_t TabDescription::GetFileVersion() const { return 0x104; }

// [9] 0x18004c390 — the ctor, placement (the null test sat in the retired door).
Object::External* TabDescription::Construct() {
    return ::new (this) TabDescription();
}
// [6] 0x18004c370 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* TabDescription::Reconstruct() {
    this->~TabDescription();
    return ::new (this) TabDescription();
}
// [7] 0x18004c3a0
void TabDescription::Destruct() { this->~TabDescription(); }

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
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int version);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                              // 1800da7aa

    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_stripBackdropDescription), reinterpret_cast<rt::io::Serializer*>(&ser));   // 1800da7b8
    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_pageBackdropDescription), reinterpret_cast<rt::io::Serializer*>(&ser));    // 1800da7c6
    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_pageFrameDescription), reinterpret_cast<rt::io::Serializer*>(&ser));       // 1800da7d4
    RtObject_ExternalPtrSerialize_SpriteSheet(reinterpret_cast<RtObject::External**>(&_spriteSheet), reinterpret_cast<rt::io::Serializer*>(&ser));            // 1800da7e5 ExternalPtr<SpriteSheet>
    reinterpret_cast<System::String*>(&_edgeLeftName)->Serialize(&ser);           // 1800da7f6
    reinterpret_cast<System::String*>(&_edgeMiddleName)->Serialize(&ser);         // 1800da807
    reinterpret_cast<System::String*>(&_edgeRightName)->Serialize(&ser);          // 1800da818

    rw->SerializeInt(_gap, nullptr);

    int32_t n = _tabsCount;                                             // 1800da835
    rw->SerializeInt(n, nullptr);

    if (ser.stream->mode == 0) {                                        // 1800da85a (re-read)
        rtcoll::RtColl_ArraySetSize_18004cdf0(&_tabsData, n);               // 1800da86b
    }

    // The pair loop: BOTH halves of every element, through ONE array-data
    // pointer read BEFORE the loop (0x1800da870) — retail does not re-read it,
    // which is safe here only because SetSize has already finished.
    uint8_t* tab = reinterpret_cast<uint8_t*>(_tabsData);
    for (int32_t i = 0; i < n; ++i, tab += 0x10) {                      // 1800da87d / 1800da8a3
        RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(tab), reinterpret_cast<rt::io::Serializer*>(&ser));            // 1800da889
        RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(tab + 8), reinterpret_cast<rt::io::Serializer*>(&ser));            // 1800da897
    }

    if (rw != nullptr) {                                                // 1800da8ad
        delete rw;
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
    void* raw = System::MemoryAllocator::MemAlloc(0x108, 8);          // 1800da454 (inlined)

    TabElement* elem = nullptr;
    if (raw != nullptr)                                                 // 1800da481
        elem = ::new (raw) TabElement();                                // 1800da486 ??0 (out of line)

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

// ??1TabDescription@UI@@UEAA@XZ   0x1800da8e0 — retail's body, moved from the
// retired door (ENDGAME B.3.9 UI-B): destroy the tab Array (0x18004cc90 —
// frees the buffer and does NOT visit the pairs), then the three Strings in
// REVERSE order (+0x98, +0x90, +0x88). The restamps, the base's _textName
// (+0x60) and the tail JMP into ~External are the compiler's chain. The four
// ExternalPtrs at +0x68..+0x80 are not released.
TabDescription::~TabDescription() {
    uint8_t* p = reinterpret_cast<uint8_t*>(this);
    rtcoll::RtColl_ArrayDtor_18004cc90(p + 0xa8);             // 1800da90c
    ReleaseStringField(reinterpret_cast<char**>(p + 0x98));    // 1800da912
    ReleaseStringField(reinterpret_cast<char**>(p + 0x90));    // 1800da945
    ReleaseStringField(reinterpret_cast<char**>(p + 0x88));    // 1800da977
}

// ?CreateNewObject@TabDescription@UI@@SAPEAVBase@Object@@XZ 0x18004c2e0 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0xb8, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* TabDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TabDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) TabDescription();
}
// ?CreateInPlaceObject@TabDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x18004c360
Object::Base* TabDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) TabDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7TabDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// TabDescription now wears the compiler's table.
// ============================================================================

int RtUiTabDescriptionRegister() {
    int n = 0;
    // The factory pair (B39_RULES D2), the class's own statics, by name AND
    // address; the minted `?_typeInfo@TabDescription` record resolves them.
    RegisterRuntimeExport("?CreateNewObject@TabDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::TabDescription::CreateNewObject);      ++n;
    RegisterRuntimeAddr(0x18004c2e0ull, (void*)&UI::TabDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@TabDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::TabDescription::CreateInPlaceObject);  ++n;
    RegisterRuntimeAddr(0x18004c360ull, (void*)&UI::TabDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::TabDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@TabDescription@UI@@ (0x1801fa430): name "TabDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@TabDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::TabDescription, "ElementDescription", nullptr)
