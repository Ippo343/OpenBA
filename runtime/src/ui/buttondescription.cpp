// ============================================================================
// runtime/src/ui/buttondescription.cpp — UI::ButtonDescription, the whole class
// ============================================================================
//
// ??_7ButtonDescription@UI@@6B@ (retail 0x180157370), ten slots:
//   0  deleting dtor      0x1800481a0  ButtonDescription.h (the class's own)
//   1  TypeName           0x180047e30  here
//   2  Type               0x180047e10  here
//   3  GetFileVersion     0x1801113d0  here  (returns 0x104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1801113e0  here
//   6  Reconstruct        0x180047fb0  here
//   7  Destruct           0x180047fe0  here
//   8  CreateInstance     0x180111510  here
//   9  Construct          0x180047fd0  here
// plus ??1ButtonDescription@UI@@UEAA@XZ (0x180110bd0) and the shared applier
// 0x1801108d0 — `?CreateInstance@ButtonDescription@UI@@QEBAXV?$InternalPtr@
// VButtonElement@UI@@@Object@@PEBVDialog@2@@Z`, EXPORTED (B36_SLOTS.md
// section 6; this file used to call it "unexported" and still registers it by
// address only, which is enough: a by-name lookup lands on the same address).
//
// ENDGAME B.3.6 block C1: every body is a MEMBER of `UI::ButtonDescription`
// (runtime/types/ui/ButtonDescription.h); the registered doors below are
// one-line QUALIFIED delegations (B34_BRIEFS A.1 — a live description wears
// the MINTED table, whose slots resolve back to these doors).
//
// THE APPLIER IS THE REASON THIS CLASS IS THE ROOT OF A SUBTREE. 0x1801108d0 is
// called by THREE CreateInstance bodies — this one (0x1801115b6),
// CheckDescription's (0x180111a77) and ComboDescription's (0x180112b0c) — which
// is why it is a separate function in the shipped binary and is one here,
// owned by the base class rather than copied into each subclass file.
//
// The constructor is OUT OF LINE (`??0ButtonDescription@UI@@QEAA@XZ`,
// 0x180110b10). ENDGAME B.3.9 UI-A: it is the real ctor (ButtonDescription.h),
// `Construct` and `Reconstruct` placement-construct with it, and the factory
// pair is the class's own (the FACTUI1 row and the slot doors are retired).
//
// Layout: runtime/types/ui/ButtonDescription.h (class) over
// DescriptionsBatch1.h (POD). sizeof 0xa0.
//
// ~ButtonDescription RELEASES NOTHING BUT THE BASE'S _textName. The five
// ExternalPtr<ElementDescription> at +0x68..+0x88 are left alone, and so are
// the four ints. Retail's, transcribed.
// ============================================================================
#include <new>

#include "description_common.h"
#include "../../types/ui/ButtonDescription.h"
#include "../../types/ui/ButtonElement.h"

namespace {

using namespace rtdesc;

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

int32_t ButtonDescription::GetFileVersion() const { return 0x104; }

// 0x180047fd0: the null test sat in the retired door (a virtual call always
// has a `this`); then a tail jump into the constructor.
Object::External* ButtonDescription::Construct() {
    return ::new (this) ButtonDescription();                  // 180047fc8 JMP
}
// 0x180047fb0: slot 0 with flag 0 (destroy WITHOUT freeing, through the
// object's own table), then the ctor again. No null test.
Object::External* ButtonDescription::Reconstruct() {
    this->~ButtonDescription();                                          // 180047fbe
    return ::new (this) ButtonDescription();                  // 180047fc8 JMP
}
void ButtonDescription::Destruct() { this->~ButtonDescription(); }

// ============================================================================
// ?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1801113e0
// ============================================================================
//   ElementDescription::Serialize(this, stream)               0x180116b00
//   five ExternalPtr<ElementDescription>::Serialize over +0x68..+0x88 (0x180038f90)
//   four kScalarUInt (+0x98 in the ReaderWriter vtable) over +0x90..+0x9c
//
// THE FOUR DWORDS GO THROUGH THE UNSIGNED SLOT (+0x98), NOT THE INT SLOT
// (+0xa0). Read off 0x18011148d/0x1801114a5/0x1801114bd/0x1801114d5; the
// sibling classes in this batch use both slots and the choice is per field.
//
// The base call is QUALIFIED: this object wears the leaf's table, whose slot 5
// is this very function.
bool ButtonDescription::Serialize(void* streamv) {
    Stream* stream = static_cast<Stream*>(streamv);

    ElementDescription::Serialize(stream);                 // 1801113fa

    Serializer ser = { stream, nullptr };
    IO::ReaderWriter* rw = MakeReaderWriter(stream, &ser);

    for (int i = 0; i < 5; ++i)                            // 18011142f .. 180111476
        RtObject_ExternalPtrSerialize_ElementDescription(ExtField(&_childDesc[i]), IoSer(&ser));

    rw->SerializeUInt(reinterpret_cast<uint32_t&>(_word90), nullptr); // 18011147b .. 1801114d5
    rw->SerializeUInt(reinterpret_cast<uint32_t&>(_word94), nullptr);
    rw->SerializeUInt(reinterpret_cast<uint32_t&>(_word98), nullptr);
    rw->SerializeUInt(reinterpret_cast<uint32_t&>(_word9c), nullptr);

    if (rw != nullptr) delete rw;                // 1801114ee
    return true;
}

// ============================================================================
// The shared applier, retail 0x1801108d0 — the NON-VIRTUAL CreateInstance
// ============================================================================
// `void CreateInstance(InternalPtr<ButtonElement> element, const Dialog* dialog)
// const`, the handle arriving as the ADDRESS of the caller's copy (RDX).
//
//   local = *elemPtr                                     ; 1801108f5
//   ElementDescription::CreateInstance(self, &local)      ; 1801108fa
//   for i in 0..4:
//       parent = *elemPtr           (RE-READ every iteration, into RBX)
//       made   = Dialog::CreateElement(dialog, &local, self->_childDesc[i])
//       if (*made) (*made)->_parent = parent
//       Array<Element*>::SetSize(&parent->_children, size+1, made)
//       mark parent and every ancestor kElementFlagNeedsSizeCalc
//   e = *elemPtr ; c = e->_children._data[4]
//   if (c) c->vt[0x50](c, self->_word90)
//   e->+0xc8 = self->+0x90 ; e->+0xcc = +0x94 ; e->+0xd0 = +0x98 ; e->+0xd4 = +0x9c
//       (each through a FRESH `*elemPtr` read)
//
// TWO THINGS HERE LOOK LIKE BUGS AND ARE NOT:
//
// 1. ONE STACK SLOT DOES TWO JOBS. [RSP+0x30] holds the new element when it is
//    handed to `ElementDescription::CreateInstance`, and is then REUSED as
//    `Dialog::CreateElement`'s return slot inside the loop. Splitting it into
//    two locals would be tidier and would stop being retail.
// 2. `*elemPtr` IS RE-READ from the CALLER's slot before every single use —
//    five times in the loop and five more at the tail — after
//    ElementDescription::CreateInstance has run on a LOCAL COPY of it. Folding
//    the re-reads into one variable would assert that nothing can change the
//    caller's slot between them. Retail does not assert that.
//
// `ElementDescription::CreateInstance` (0x180115ad0) is a QUALIFIED member
// call since fragment pass F2 defined it (dialog.cpp).
//
// THE FIFTH CHILD IS SPECIAL: `[RCX+0x20]` at 0x180110aa0 indexes the children
// array by 0x20 bytes, i.e. element [4], the last one this loop pushed — and
// only if the array pointer itself is non-null is its vt[0x50] called.
void ButtonDescription::CreateInstance(Object::InternalPtr<ButtonElement>* element,
                                       const Dialog* dialog) const {
    void** elemPtr = reinterpret_cast<void**>(element);

    void* local = *elemPtr;                                // 1801108f5
    UI::ElementDescription::CreateInstance(                   // 1801108fa
        reinterpret_cast<Object::InternalPtr<Element>*>(&local));  // B.3.6 bridge

    for (int i = 0; i < 5; ++i) {                          // 1801108ff .. 180110a94
        uint8_t* parent = *reinterpret_cast<uint8_t**>(elemPtr);
        void**   made   = DialogCreateElement(dialog, &local, _childDesc[i]);
        AttachAndPushChild(parent, made);
    }

    {
        // `MOV RCX,[RAX+0xa0] ; MOV RCX,[RCX+0x20]` — the second deref is
        // UNGUARDED in retail, so an empty children array faults there too.
        // No null check is added here: the loop above always pushed five.
        ButtonElement* e = element->Get();                           // 180110a96
        Element* c = e->_children._data[4];                          // 180110aa0
        if (c != nullptr)                                            // 180110aa4
            c->SetColor(static_cast<uint32_t>(_word90));             // 180110ab2 vt[0x50]
    }

    // Four dwords, each preceded by its own `MOV RCX,[RDI]`.
    element->Get()->_stateColor[0] = static_cast<uint32_t>(_word90);   // 180110ab5 .. 180110afa
    element->Get()->_stateColor[1] = static_cast<uint32_t>(_word94);
    element->Get()->_stateColor[2] = static_cast<uint32_t>(_word98);
    element->Get()->_stateColor[3] = static_cast<uint32_t>(_word9c);
}

// ============================================================================
// ?CreateInstance@ButtonDescription@UI@@...   retail 0x180111510   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*
// (RULE B6-2: the slot is the first declared parameter).
// ButtonElement is 0xd8 bytes and its constructor is INLINED here rather than
// called: ??0Element (0x180115e50), then the ButtonElement vtable, then two
// DWORD stores (+0xc0 = -1, +0xc4 = 0) -- exactly the real ctor's
// (ButtonElement.h), which is what this spells now.
Object::InternalPtr<Element>* ButtonDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xd8, 8);                         // 180111552

    ButtonElement* elem = nullptr;
    if (raw != nullptr)                                    // 18011157d
        elem = ::new (raw) ButtonElement();                // 180111582..18011159c inlined ctor

    Object::InternalPtr<ButtonElement> slot(elem);         // 1801115a6
    ButtonDescription::CreateInstance(&slot, dialog);      // 1801115b6

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1801115bb
    return ret;
}

}  // namespace UI

// ============================================================================
// ??1ButtonDescription@UI@@UEAA@XZ   0x180110bd0
// ============================================================================
// TWO vtable stamps back to back -- own, then ElementDescription's -- then the
// base's _textName and the tail JMP into ~External. ENDGAME B.3.9 UI-A: the
// body is EMPTY; the restamps and the chain into ~ElementDescription and
// ~External are the compiler's.
namespace UI {
ButtonDescription::~ButtonDescription() {}

// ?CreateNewObject@ButtonDescription@UI@@SAPEAVBase@Object@@XZ  0x180047f20
// The FACTUI1 row this replaces: MemAlloc(0xa0, 8), retail's null branch, then
// the out-of-line ctor.
Object::Base* ButtonDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ButtonDescription), 8);
    if (p == nullptr) return nullptr;
    return ::new (p) ButtonDescription();
}
// ?CreateInPlaceObject@ButtonDescription@UI@@SAPEAVBase@Object@@PEAX@Z  0x180047fa0
Object::Base* ButtonDescription::CreateInPlaceObject(void* place) {
    if (place == nullptr) return nullptr;
    return ::new (place) ButtonDescription();
}
}  // namespace UI

// ============================================================================
// The shared applier's door (a non-slot member, kept). ENDGAME B.3.9 UI-A: the
// slot doors and the ??1 door, read only by the retired minted table and
// RT_DELDTOR row, are gone.
// ============================================================================
// 0x1801108d0: RCX = this, RDX = &caller's InternalPtr<ButtonElement>, R8 = Dialog*.
extern "C" void RtUi_ButtonDescription_ApplyToElement(const void* selfv,
                                                      void** elemPtr,
                                                      const void* dialog) {
    reinterpret_cast<const UI::ButtonDescription*>(selfv)                // B.3.6 bridge
        ->UI::ButtonDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::ButtonElement>*>(elemPtr),
            static_cast<const UI::Dialog*>(dialog));
}

int RtUiButtonDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?CreateNewObject@ButtonDescription@UI@@SAPEAVBase@Object@@XZ",
                          (void*)&UI::ButtonDescription::CreateNewObject);        ++n;
    RegisterRuntimeAddr(0x180047f20ull, (void*)&UI::ButtonDescription::CreateNewObject);
    RegisterRuntimeExport("?CreateInPlaceObject@ButtonDescription@UI@@SAPEAVBase@Object@@PEAX@Z",
                          (void*)&UI::ButtonDescription::CreateInPlaceObject);    ++n;
    RegisterRuntimeAddr(0x180047fa0ull, (void*)&UI::ButtonDescription::CreateInPlaceObject);

    // ADDRESS ONLY, as before: the export name resolves to this address.
    RegisterRuntimeAddr(0x1801108d0ull, (void*)&RtUi_ButtonDescription_ApplyToElement); ++n;

    return n;
}

// ============================================================================
// ENDGAME B.3.9 TI-UI (B39 D6): UI::ButtonDescription's own type record, built at CRT init
// (head-insert into Object::BaseTypeInfo::_firstTypeInfo). Literals copied from
// the retired minted row ?_typeInfo@ButtonDescription@UI@@ (0x1801fa900): name "ButtonDescription", parent
// "ElementDescription" (retail ?_ParentTypeName@ButtonDescription@UI@@), no file extension; factories:
// the class's own.
// ============================================================================
DEFINE_TYPE(UI::ButtonDescription, "ElementDescription", nullptr)
