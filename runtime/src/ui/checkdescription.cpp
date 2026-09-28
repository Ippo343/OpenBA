// ============================================================================
// runtime/src/ui/checkdescription.cpp — UI::CheckDescription, the whole class
// ============================================================================
//
// ??_7CheckDescription@UI@@6B@ (retail 0x1801572c0), ten slots:
//   0  deleting dtor      0x1800489c0  CheckDescription.h (the class's own)
//   1  TypeName           0x180048680  here
//   2  Type               0x180048660  here
//   3  GetFileVersion     0x180111920  here  (returns 0x1104 — NOT 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x180111930  here
//   6  Reconstruct        0x180048850  here
//   7  Destruct           0x1800488f0  here
//   8  CreateInstance     0x1801119c0  here
//   9  Construct          0x1800488a0  here
// plus ??1CheckDescription@UI@@UEAA@XZ (0x180111620).
//
// ENDGAME B.3.6 block C1: the bodies are MEMBERS of `UI::CheckDescription`
// (runtime/types/ui/CheckDescription.h); the doors are one-line QUALIFIED
// delegations.
//
// DERIVES FROM UI::ButtonDescription, and the derivation is load-bearing in
// three places, all of them now MEMBER calls into
// runtime/src/ui/buttondescription.cpp's class — QUALIFIED, because this
// class's own slot-8 override hides the non-virtual applier and slot 5 would
// dispatch back here:
//   Construct/Reconstruct call ??0ButtonDescription (0x180110b10, a door);
//   Serialize calls ButtonDescription::Serialize (0x1801113e0);
//   CreateInstance calls ButtonDescription::CreateInstance (0x1801108d0).
//
// `??0CheckDescription@UI@@QEAA@XZ` IS exported (0x1801115e0, B36_SLOTS.md
// section 6) — the old banner here said it was not. Nothing registers it; the
// kCheckDescription row in runtime/src/ui/factory_ui1_tables.inc has
// `??0ButtonDescription@UI@@QEAA@XZ` as its BASE ctor plus two stores of its
// own — exactly the two stores `Construct` open-codes below.
//
// GetFileVersion is 0x1104, read from this class's own body (0x180111920). Its
// base returns 0x104. The constant is per class and was read per class.
//
// Layout: runtime/types/ui/CheckDescription.h over DescriptionsBatch1.h.
// sizeof 0xa8.
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/CheckDescription.h"
#include "../../types/ui/CheckElement.h"

namespace {

using namespace rtdesc;

}  // namespace

namespace UI {

int32_t CheckDescription::GetFileVersion() const { return 0x1104; }

// 0x1800488a0: null test (the DOOR's), base ctor, own two stores, return this.
// The two stores are transcribed from 0x1800488c2..0x1800488cc (and
// identically from 0x180048877..0x180048881 inside Reconstruct); the vtable
// stamp is retail's own store over the one ??0ButtonDescription just made.
// ENDGAME B.3.9 UI-A: that construction is the real ctor (CheckDescription.h).
Object::External* CheckDescription::Construct() {
    return ::new (this) CheckDescription();                    // 1800488bc..1800488d7
}
// 0x180048850: slot 0 with flag 0, then the same construction, NO null test.
Object::External* CheckDescription::Reconstruct() {
    DeletingDtor(0);                                           // 180048867
    return ::new (this) CheckDescription();                    // 180048871..18004888c
}
void CheckDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180111930
// ============================================================================
// Note the base call is ButtonDescription::Serialize (0x1801113e0), NOT
// ElementDescription::Serialize — this is the one place in this batch where the
// base serializer is a sibling class's rather than the root's.
bool CheckDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    ButtonDescription::Serialize(stream);                  // 18011194a

    Serializer ser = { stream, nullptr };
    IO::ReaderWriter* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_checkDesc, &ser); // 18011198b

    if (rw != nullptr) rw->DeletingDtor(1);                // 1801119a3
    return true;
}

// ============================================================================
// ?CreateInstance@CheckDescription@UI@@...   retail 0x1801119c0   (slot 8)
// ============================================================================
// CheckElement is 0xe0 bytes, and its constructor is inlined here as the
// BUTTON element's inline constructor followed by two more stores:
//     ??0Element (0x180115e50)
//     ButtonElement vtable, +0xc0 = -1 (dword), +0xc4 = 0 (dword)
//     CheckElement vtable, +0xd8 = 0 (BYTE — `MOV byte`, not dword)
// so the ButtonElement vtable is written and then overwritten -- the real
// CheckElement ctor's stores (CheckElement.h), which is what this spells now.
//
// Then the shared ButtonDescription applier (0x1801108d0), then ONE more child
// from `_checkDesc`, attached to the element retail still holds in RDI —
// not re-read from the slot the way the applier's own loop does it. The slot
// is the SAME stack local the applier received and CreateElement returns into.
Object::InternalPtr<Element>* CheckDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xe0, 8);                         // 180111a02

    CheckElement* elem = nullptr;
    if (raw != nullptr)                                    // 180111a2d
        elem = ::new (raw) CheckElement();                 // 180111a32..180111a60 inlined ctor

    Object::InternalPtr<ButtonElement> slot(elem);         // 180111a67
    ButtonDescription::CreateInstance(&slot, dialog);      // 180111a77

    void** made = DialogCreateElement(dialog, reinterpret_cast<void**>(&slot),
                                      _checkDesc);         // 180111a8b
    AttachAndPushChild(reinterpret_cast<uint8_t*>(elem), made);   // 180111a98 .. 180111ad4

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 180111ad6
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1CheckDescription@UI@@UEAA@XZ   0x180111620
// ============================================================================
// THREE vtable stamps back to back -- own, ButtonDescription's, then
// ElementDescription's -- then the base's _textName and the tail JMP into
// ~External. ENDGAME B.3.9 UI-A: EMPTY; all of it is the compiler's chain. The
// +0xa0 child description is NOT released.
namespace UI {
CheckDescription::~CheckDescription() {}
}  // namespace UI

namespace UI {
// ?CreateNewObject@CheckDescription@UI@@SAPEAVBase@Object@@XZ  0x180048770
// The factory-interpreter row this replaces: MemAlloc(0xa8, 8), retail's
// null branch, then ??0ButtonDescription and the +0xa0 store.
Object::Base* CheckDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CheckDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) CheckDescription();
}
// ?CreateInPlaceObject@CheckDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x180048800
Object::Base* CheckDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) CheckDescription();
}
}  // namespace UI

// ============================================================================
// Registration: the factory pair only (B39_RULES D2). The slot doors and the
// ??1 door, read only by the retired minted table / RT_DELDTOR row, are gone.
// ============================================================================
int RtUiCheckDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@CheckDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::CheckDescription::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x180048770ull, (void*)&UI::CheckDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@CheckDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::CheckDescription::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x180048800ull, (void*)&UI::CheckDescription::CreateInPlaceObject);
    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::CheckDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@CheckDescription@UI@@ (0x1801fa970): name "CheckDescription", parent
// "ButtonDescription" (retail ?_ParentTypeName@CheckDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::CheckDescription, "ButtonDescription", nullptr)
