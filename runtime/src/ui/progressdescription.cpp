// ============================================================================
// runtime/src/ui/progressdescription.cpp — UI::ProgressDescription, the class
// ============================================================================
//
// Vtable batch 2. Derives straight from UI::ElementDescription.
//
// ??_7ProgressDescription@UI@@6B@ (retail 0x1801591e0), ten slots:
//   0  deleting dtor      0x180077c80  deleting_dtor.cpp row (sizeof 0xb0)
//   1  TypeName           0x180077770  here
//   2  Type               0x180077750  here
//   3  GetFileVersion     0x18011a070  here  (returns 0x204)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180119f60  here
//   6  Reconstruct        0x1800778f0  here
//   7  Destruct           0x180077920  here
//   8  CreateInstance     0x180119d00  here
//   9  Construct          0x180077910  here
// plus ??0ProgressDescription@UI@@QEAA@XZ 0x18011a210 (out of line) and
// ??1ProgressDescription@UI@@UEAA@XZ 0x18011a080.
//
// LAYOUT: runtime/types/ui/ProgressDescription.h (sizeof 0xb0; RULE B6-8 moved
// this file's offsets there under the retail name).
//
// ENDGAME B.3.6 block C5: every slot body MOVED into the members of the real
// `UI::ProgressDescription`; each registered door is a one-line QUALIFIED
// delegation. The ctor / dtor stay doors (B4b-2).
//
// Every helper is already ported, including ??0ProgressElement@UI@@
// (0x180119c40, ui_factory_wave28.cpp), so this file adds none.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../types/ui/ElementDescription.h"
#include "../../types/ui/Element.h"
#include "../../types/io/Serializer.h"
#include "../../types/ui/ProgressDescription.h"
#include "../../types/ui/ProgressElement.h"
#include "../../types/ui/Dialog.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/system/String.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/collection/Collection.h"

#include <new>

#include "../../types/io/Stream.h"

namespace {


void ReleaseStringField(char** field) {
    char* s = *field;
    if (s == nullptr) return;
    if (--RtSystem::StringRefCount(s) == 0) {
        *field = s - 8;
        System::MemoryAllocator::MemFree(*field);
    } else {
        *field = nullptr;
    }
}

}  // namespace

// ============================================================================
// The MEMBERS
// ============================================================================
namespace {
// ?GetSpriteIndexFromName@SpriteSheet@UI@@ (0x1800031f0), called directly.
// B.4 LEFT: the names are `System::String` fields spelt `char*` (their dtors
// would start to run as the real type, B4_RULES H5).
int32_t GetSpriteIndexFromName(const UI::SpriteSheet* sheet, const void* nameStr) {
    return sheet->GetSpriteIndexFromName(*static_cast<const System::String*>(nameStr));
}
}  // namespace

namespace UI {

// [3] 0x18011a070
int32_t ProgressDescription::GetFileVersion() const { return 0x204; }

// [9] 0x180077910 — the ctor, placement (the null test sat in the retired door).
Object::External* ProgressDescription::Construct() {
    return ::new (this) ProgressDescription();       // ENDGAME B.3.9: the real ctor
}
// [6] 0x1800778f0 — destroy in place through slot 0 (flag 0), re-run the ctor.
Object::External* ProgressDescription::Reconstruct() {
    this->~ProgressDescription();
    return ::new (this) ProgressDescription();
}
// [7] 0x180077920
void ProgressDescription::Destruct() { this->~ProgressDescription(); }

// [5] ?Serialize@ProgressDescription@UI@@UEAA_NAEAVStream@IO@@@Z  0x180119f60
bool ProgressDescription::Serialize(void* streamArg) {
    IO::Stream* stream = static_cast<IO::Stream*>(streamArg);

    // QUALIFIED: this object wears the LEAF's table, whose slot 5 is this body.
    ElementDescription::Serialize(stream);                              // 180119f7a

    // The inlined ??0Serializer (0x1800b9960): CreateReader / CreateWriter
    // of the stream's encoding, version 0.
    IO::Serializer ser(*stream, 0);
    IO::ReaderWriter* const rw = ser._readerWriter;

    // kScalarUInt, NOT kScalarInt — vt+0x98 at 0x180119fbe.
    rw->SerializeUInt(_unknown68, nullptr);

    RtObject_ExternalPtrSerialize_SpriteSheet(reinterpret_cast<RtObject::External**>(&_spriteSheet), reinterpret_cast<rt::io::Serializer*>(&ser));            // 180119fcd ExternalPtr<SpriteSheet>
    reinterpret_cast<System::String*>(&_fillLeftName)->Serialize(&ser);           // 180119fdb
    reinterpret_cast<System::String*>(&_fillMiddleName)->Serialize(&ser);         // 180119fec
    reinterpret_cast<System::String*>(&_fillRightName)->Serialize(&ser);          // 180119ffd
    reinterpret_cast<System::String*>(&_trackLeftName)->Serialize(&ser);          // 18011a00e
    reinterpret_cast<System::String*>(&_trackMiddleName)->Serialize(&ser);        // 18011a01f
    reinterpret_cast<System::String*>(&_trackRightName)->Serialize(&ser);         // 18011a030
    RtObject_ExternalPtrSerialize_ElementDescription(reinterpret_cast<RtObject::External**>(&_childDescription), reinterpret_cast<rt::io::Serializer*>(&ser));       // 18011a041 ExternalPtr<ElementDescription>

    // The inlined ~Serializer deletes the ReaderWriter (18011a04f).
    return true;
}

// ============================================================================
// [8] ?CreateInstance@ProgressDescription@UI@@...   retail 0x180119d00
// ============================================================================
// ProgressElement is 0x130 bytes (`MOV EDX,0x130` at 0x180119d49); its ctor is
// CALLED out of line (0x180119c40, already ported).
//
// SIX {sheet, index} pairs written into +0xc0/+0xc8 .. +0x110/+0x118, with the
// same dword-written/qword-read stack slot the Image, Slider and Tab bodies
// have (0x180119da5 writes `dword [RBP-0x10]`, 0x180119db8 reads
// `qword [RBP-0x10]`), and the sheet pointer RE-READ from the description
// before every one of the six.
//
// THE CHILD IS CONDITIONAL: `CMP qword [RSI+0xa8],0 ; JZ` at 0x180119ed1 skips
// the whole create-parent-push-dirty block when the description has no child.
// Every other class in this batch creates its children unconditionally.
Object::InternalPtr<Element>* ProgressDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = System::MemoryAllocator::MemAlloc(0x130, 8);          // 180119d49 (inlined)

    ProgressElement* elem = nullptr;
    if (raw != nullptr)                                                 // 180119d73
        elem = ::new (raw) ProgressElement();                           // 180119d78 ??0 (out of line)

    Object::InternalPtr<Element> slot(elem);                                                  // 180119d80
    // The NON-virtual ElementDescription::CreateInstance(InternalPtr<Element>)
    // (0x180115ad0): a QUALIFIED member call since fragment pass F2.
    UI::ElementDescription::CreateInstance(                   // 180119d8b
        &slot);

    uint64_t idxSlot;                                                   // [RBP-0x10]
    char* const* const names[6] = { &_fillLeftName,  &_fillMiddleName,  &_fillRightName,
                                    &_trackLeftName, &_trackMiddleName, &_trackRightName };
    ::Sprite* const sprites[6] = { &elem->_fillLeft,  &elem->_fillMiddle,  &elem->_fillRight,
                                   &elem->_trackLeft, &elem->_trackMiddle, &elem->_trackRight };
    for (int i = 0; i < 6; ++i) {
        SpriteSheet* sheet = _spriteSheet;                                     // RE-READ each time
        if (sheet != nullptr) {
            const int32_t idx = GetSpriteIndexFromName(sheet, names[i]);
            __builtin_memcpy(&idxSlot, &idx, sizeof idx);               // DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&idxSlot, &minusOne, sizeof minusOne);
        }
        // One QWORD store over `spriteIndex` and `pad0C`: the index in the low
        // dword, the slot's never-written high dword above it.
        sprites[i]->spriteSheet = sheet;
        sprites[i]->spriteIndex = static_cast<int32_t>(idxSlot);
        sprites[i]->pad0C       = static_cast<int32_t>(idxSlot >> 32);
    }

    if (_childDescription != nullptr) {                                 // 180119ed1
        // ?CreateElement@Dialog@UI@@ 0x180112ed0: RETSLOT, the member's prvalue
        // placed in the caller's slot.                          // 180119ee9
        Object::InternalPtr<Element>* made = &slot;
        *made = dialog->CreateElement(
            Object::ExternalPtr<UI::ElementDescription>(_childDescription));

        Element* child = made->Get();            // 180119eee
        if (child != nullptr)
            child->_parent = elem;                                      // 180119ef6

        rtcoll::RtColl_ArraySetSizeFill_180076900(&elem->_children,
                                       elem->_children.GetSize() + 1,
                                       made);                           // 180119f0c

        for (Element* n = elem; n != nullptr; n = n->_parent)           // 180119f20..180119f34
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 180119f39
    return ret;
}

// ??1ProgressDescription@UI@@UEAA@XZ  0x18011a080 — retail's body, moved from
// the retired door (ENDGAME B.3.9 UI-B): the six Strings in REVERSE order. The
// restamps, the base's _textName (+0x60) and the tail JMP into ~External are
// the compiler's chain. Neither ExternalPtr is released.
ProgressDescription::~ProgressDescription() {
    ReleaseStringField(&_trackRightName);    // +0xa0// 18011a0a5
    ReleaseStringField(&_trackMiddleName);    // +0x98// 18011a0d8
    ReleaseStringField(&_trackLeftName);    // +0x90// 18011a10a
    ReleaseStringField(&_fillRightName);    // +0x88// 18011a13c
    ReleaseStringField(&_fillMiddleName);    // +0x80// 18011a16e
    ReleaseStringField(&_fillLeftName);    // +0x78// 18011a1a0
}

// ?CreateNewObject@ProgressDescription@UI@@SAPEAVBase@Object@@XZ 0x180077860 --
// `?MemAlloc@MemoryAllocator` (0x1800c7a50) of 0xb0, align 8; retail's null
// branch; the ctor. Replaces the factory interpreter row (B39_RULES D2).
Object::Base* ProgressDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ProgressDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ProgressDescription();
}
// ?CreateInPlaceObject@ProgressDescription@UI@@SAPEAVBase@Object@@PEAX@Z 0x1800778e0
Object::Base* ProgressDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ProgressDescription();
}

}  // namespace UI

// ============================================================================
// ENDGAME B.3.9 UI-B: the slot doors, the ctor / dtor doors and the
// factory-interpreter row are gone. Their only readers were the minted
// `??_7ProgressDescription@UI@@6B@`, its RT_DELDTOR row and FACTUI2: every live
// ProgressDescription now wears the compiler's table.
// ============================================================================

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ProgressDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ProgressDescription@UI@@ (0x1801facf0): name "ProgressDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ProgressDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ProgressDescription, "ElementDescription", nullptr)
