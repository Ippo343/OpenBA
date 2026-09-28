// ============================================================================
// runtime/src/ui/listdescription.cpp — UI::ListDescription, whole class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ListDescription@UI@@6B@ (retail 0x180156e20), ten slots:
//   0  deleting dtor      0x180044ed0  deleting_dtor.cpp row (sizeof 0x90)
//   1  TypeName           0x180044ba0  here
//   2  Type               0x180044b80  here
//   3  GetFileVersion     0x1800d4900  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d4810  here
//   6  Reconstruct        0x180044d20  here
//   7  Destruct           0x180044d50  here
//   8  CreateInstance     0x1800d45f0  here
//   9  Construct          0x180044d40  here
// plus ??0ListDescription@UI@@QEAA@XZ 0x1800d49b0 (out of line) and
// ??1ListDescription@UI@@UEAA@XZ 0x1800d4910.
//
// LAYOUT, sizeof 0x90 (`MOV EDX,0x90` at 0x180044eef). ElementDescription
// (0x68) plus:
//     +0x68  ExternalPtr<UI::ElementDescription>   0x180038f90  child 0
//     +0x70  ExternalPtr<UI::ElementDescription>   0x180038f90  child 1
//     +0x78  ExternalPtr<UI::SpriteSheet>          0x180004590
//     +0x80  System::String                        0x1800c9930  the sprite name
//     +0x88  uint32   kScalarUInt   \ the ctor writes ONE QWORD of -0x7f7f80 at
//     +0x8c  uint32   kScalarUInt   / +0x88 (0x1800d4a40), i.e. +0x88 starts at
//                                     0xff808080 and +0x8c at 0xffffffff. The
//                                     low half looks like an ARGB grey and the
//                                     high half like a -1 sentinel; nothing in
//                                     this class's ten slots reads either, so
//                                     the file records the VALUES and not a
//                                     meaning.
//
// Every helper is already ported, including ??0ListElement@UI@@ (0x1800d4570,
// ui_factory_wave28.cpp) and ?IsDerivedFrom@Base@Object@@ (0x1800c0390, reached
// by its export name).
// ============================================================================
#include <new>
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"
#include "../../types/ui/ListDescription.h"
#include "../../types/ui/ListElement.h"
#include "../../types/ui/ClipRegionElement.h"   // B.3.9 TI-UI: X::TypeId()
#include "../../types/ui/RibbonElement.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/system/String.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"

// ENDGAME B.3.6 block C4: the layout moved to runtime/types/ui/
// ListDescription.h (the real class). Every slot body below is a MEMBER; each
// registered door is a one-line QUALIFIED delegation. The ctor and dtor stay
// DOORS (B4b-2). B3R RT-UI: every export the members reach is a direct call.

namespace {

// The two type ids the tail of CreateInstance tests against: retail reads the
// first dword of RibbonElement's (0x1801fa2e0) and ClipRegionElement's
// (0x1801f9d68) `_typeInfo` -- `X::TypeId()` (ENDGAME B.3.9 TI-UI).

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

void AddChild(const void* dialog, void** slot, UI::Element* elem, void* childDesc) {
    // ?CreateElement@Dialog@UI@@ 0x180112ed0: RETSLOT, the member's prvalue
    // placed in the caller's slot.
    void** made = slot;
    new (static_cast<void*>(made)) Object::InternalPtr<UI::Element>(
        static_cast<const UI::Dialog*>(dialog)->CreateElement(               // B.3.x bridge
            Object::ExternalPtr<UI::ElementDescription>(
                static_cast<UI::ElementDescription*>(childDesc))));

    UI::Element* child = static_cast<UI::Element*>(*made);
    if (child != nullptr) child->_parent = elem;

    rtcoll::RtColl_ArraySetSizeFill_180076900(&elem->_children, elem->_children.GetSize() + 1, made);

    for (UI::Element* n = elem; n != nullptr; n = n->_parent)
        n->_flags |= UI::kElementFlagNeedsSizeCalc;
}

}  // namespace

// ============================================================================
// The member bodies (ENDGAME B.3.6 block C4)
// ============================================================================
namespace UI {

int32_t ListDescription::GetFileVersion() const { return 0x104; }

// 0x180044d40 — the ctor, placement (the null test sat in the retired door).
Object::External* ListDescription::Construct() {
    return ::new (this) ListDescription();
}
// 0x180044d20 — slot 0 (VIRTUAL) with flag 0, then the ctor.
Object::External* ListDescription::Reconstruct() {
    this->~ListDescription();
    return ::new (this) ListDescription();
}
// 0x180044d50
void ListDescription::Destruct() { this->~ListDescription(); }

// ?Serialize@ListDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x1800d4810
bool ListDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    UI::ElementDescription::Serialize(stream);                 // 1800d482a (qualified)

    Serializer ser = { stream, nullptr };

    void*  factory = stream->encodingFactory;
    void** fvt     = VTableOf(factory);
    const int which = (stream->mode == 0) ? 0x18 : 0x20;
    using MakeRwFn = IO::ReaderWriter* (*)(void* factory, Serializer* ser, int version);   // B.3.x bridge: the factory returns the class
    IO::ReaderWriter* rw = reinterpret_cast<MakeRwFn>(fvt[which / 8])(factory, &ser, 0);
    ser.readerWriter = rw;                                     // 1800d485a

    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_partDescs[0]), reinterpret_cast<rt::io::Serializer*>(&ser));  // 1800d4868
    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_partDescs[1]), reinterpret_cast<rt::io::Serializer*>(&ser));  // 1800d4876
    RtObject_ExternalPtrSerialize_SpriteSheet(reinterpret_cast<RtObject::External**>(&_spriteSheet), reinterpret_cast<rt::io::Serializer*>(&ser));   // 1800d4884
    reinterpret_cast<System::String*>(&_spriteName)->Serialize(&ser);   // 1800d4895

    rw->SerializeUInt(_hoverColor, nullptr);
    rw->SerializeUInt(_selectColor, nullptr);

    if (rw != nullptr) {                                       // 1800d48d3
        delete rw;
    }
    return true;
}

// ============================================================================
// ?CreateInstance@ListDescription@UI@@...   retail 0x1800d45f0
// ============================================================================
// ListElement is 0x100 bytes (`MOV EDX,0x100` at 0x1800d4634); its ctor is
// CALLED out of line (0x1800d4570, the listelement.cpp door). TWO children,
// one sprite lookup, and then the part that makes this class unusual:
//
// *** THE TAIL WALKS INTO THE CHILDREN IT JUST BUILT AND CACHES TWO POINTERS
//     IT FINDS THERE, THROUGH FIVE UNGUARDED DEREFERENCES. ***
//
//   elem->_ribbon = IsDerivedFrom(children[1]->children[0]->children[0],
//                                 RibbonElement)     ? that : nullptr
//   elem->_clip   = IsDerivedFrom(children[1]->children[0],
//                                 ClipRegionElement) ? that : nullptr
//
// (0x1800d4777..0x1800d47d5.) Every step reads the child Array's data pointer
// and indexes it WITHOUT checking the count, so this body relies entirely on
// the two child descriptions producing exactly the shape the engine ships in
// its own .rsc. Retail does not guard it and neither does this: a null here
// means the description is not a list description, and the guard would
// silently produce a List element with no ribbon and no clip region rather than
// stopping at the real problem [[feedback_root_cause_not_null_guard]].
//
// Note the FIRST of the two chains is THREE levels deep and the SECOND is TWO;
// they are not the same walk with an extra step appended, and the deeper one
// runs first.
Object::InternalPtr<Element>* ListDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = System::MemoryAllocator::MemAlloc(0x100, 8);   // 1800d4634 (inlined)

    ListElement* elem = nullptr;
    if (raw != nullptr)                                        // 1800d4662
        elem = ::new (raw) ListElement();                      // 1800d4667 ??0 (out of line)

    void* slot = elem;                                         // 1800d4674
    UI::ElementDescription::CreateInstance(                   // 1800d4681
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    AddChild(dialog, &slot, elem, _partDescs[0]);              // 1800d4692
    AddChild(dialog, &slot, elem, _partDescs[1]);              // 1800d46e3

    // The one {sheet, index} pair, with the batch's usual 4-byte write / 8-byte
    // read of the index slot.
    {
        void* sheet = _spriteSheet;
        uint64_t idxSlot;                                      // [RSP+0x30]
        if (sheet != nullptr) {                                // 1800d472d
            const int32_t idx = static_cast<const SpriteSheet*>(sheet)       // B.3.x bridge
                ->GetSpriteIndexFromName(*reinterpret_cast<const System::String*>(&_spriteName));
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);      // 1800d473e  DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        elem->_highlight.spriteSheet = sheet;                  // 1800d474c
        __builtin_memcpy(&elem->_highlight.spriteIndex, &idxSlot, 8);   // 1800d4758  QWORD store
    }

    elem->_hoverColor  = _hoverColor;                          // 1800d4765
    elem->_selectColor = _selectColor;                         // 1800d4771

    {   // THREE levels down: children[1]->children[0]->children[0]
        Element* const k1   = elem->_children[1];              // 1800d4777 / 1800d477a
        Element* const kk0  = k1->_children[0];                // 1800d477e / 1800d4785
        Element* const cand = kk0->_children[0];               // 1800d4788 / 1800d478f
        const bool ok = reinterpret_cast<const Object::Base*>(cand)          // B.3.x bridge
            ->IsDerivedFrom(UI::RibbonElement::TypeId());                        // 1800d479b
        elem->_ribbon = ok ? cand : nullptr;                   // 1800d47a9
    }

    {   // TWO levels down: children[1]->children[0]
        Element* const k1   = elem->_children[1];              // 1800d47b0 / 1800d47b3
        Element* const cand = k1->_children[0];                // 1800d47b7 / 1800d47be
        const bool ok = reinterpret_cast<const Object::Base*>(cand)          // B.3.x bridge
            ->IsDerivedFrom(UI::ClipRegionElement::TypeId());                    // 1800d47ca
        elem->_clip = ok ? cand : nullptr;                     // 1800d47d5
    }

    *reinterpret_cast<void**>(ret) = elem;                     // 1800d47dc
    return ret;
}

// ??1ListDescription@UI@@UEAA@XZ  0x1800d4910 — retail's body, moved from the
// retired door (ENDGAME B.3.9 UI-B): the sprite-name String at +0x80. The
// restamps, the base's _textName (+0x60) and the tail JMP are the compiler's
// chain. The three ExternalPtrs are not released.
ListDescription::~ListDescription() {
    ReleaseStringField(&_spriteName);                          // 1800d4930
}

// ?CreateNewObject@ListDescription@UI@@SAPEAVBase@Object@@XZ 0x180044c90 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0x90, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* ListDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ListDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ListDescription();
}
// ?CreateInPlaceObject@ListDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x180044d10
Object::Base* ListDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ListDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7ListDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// ListDescription now wears the compiler's table.
// ============================================================================

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ListDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ListDescription@UI@@ (0x1801fa0e8): name "ListDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ListDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ListDescription, "ElementDescription", nullptr)
