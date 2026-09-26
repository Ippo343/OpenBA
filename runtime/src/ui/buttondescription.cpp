// ============================================================================
// runtime/src/ui/buttondescription.cpp — UI::ButtonDescription, the whole class
// ============================================================================
//
// ??_7ButtonDescription@UI@@6B@ (retail 0x180157370), ten slots:
//   0  deleting dtor      0x1800481a0  runtime/src/object/deleting_dtor.cpp row
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
// 0x180110b10) and is ALREADY ported by runtime/src/ui/factory_ui1.cpp, with
// the two factory steps. `Construct` and `Reconstruct` tail-jump into it.
//
// Layout: runtime/types/ui/ButtonDescription.h (class) over
// DescriptionsBatch1.h (POD). sizeof 0xa0.
//
// ~ButtonDescription RELEASES NOTHING BUT THE BASE'S _textName. The five
// ExternalPtr<ElementDescription> at +0x68..+0x88 are left alone, and so are
// the four ints. Retail's, transcribed.
// ============================================================================
#include <atomic>
#include <new>

#include "description_common.h"
#include "../../types/ui/ButtonDescription.h"
#include "../../types/ui/ButtonElement.h"

namespace {

using namespace rtdesc;

// ---------------------------------------------------------------------------
// RULE B6-5 door caches. UI is constructed on the LOADER thread while the main
// thread draws, so a member body keeps no function-local first-call cache
// (`RT_DATA_SLOT` / `RT_EXPORT_ONCE`'s shape): it resolves through a
// namespace-scope atomic slot. Never caches a null; a miss is retried.
// ---------------------------------------------------------------------------
void* AtomicResolve(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtButtonElement{nullptr};
std::atomic<void*> g_buttonTypeInfo{nullptr};
std::atomic<void*> g_buttonCtor{nullptr};

void* VtButtonElement() {
    return AtomicResolve(g_vtButtonElement, RtUI::kVtblButtonElement);
}
const uint8_t* ButtonTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_buttonTypeInfo, "?_typeInfo@ButtonDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
// ??0ButtonDescription@UI@@QEAA@XZ — ported by factory_ui1.cpp (a ctor door,
// not retired in step 6), reached through the port-aware name lookup.
void* ButtonCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(AtomicResolve(
        g_buttonCtor, "??0ButtonDescription@UI@@QEAA@XZ"))(self);
}

// Doors only (the dtor door is not a member).
void* VtButtonDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblButtonDescription); }

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

// `Type` returns the FIRST DWORD of the BaseTypeInfo static (0x1801fa900) and
// `TypeName` the pointer at +0x08.
uint32_t ButtonDescription::Type() const {
    const uint8_t* ti = ButtonTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* ButtonDescription::TypeName() const {
    const uint8_t* ti = ButtonTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t ButtonDescription::GetFileVersion() const { return 0x104; }

// 0x180047fd0: the null test is the DOOR's (a member cannot test `this`);
// then a tail jump into the constructor, whose return value is this one's.
Object::External* ButtonDescription::Construct() {
    return static_cast<Object::External*>(ButtonCtor(this));   // 180047fc8 JMP
}
// 0x180047fb0: slot 0 with flag 0 (destroy WITHOUT freeing — through the
// object's own table, i.e. the minted row), then the ctor again. No null test.
Object::External* ButtonDescription::Reconstruct() {
    DeletingDtor(0);                                          // 180047fbe
    return static_cast<Object::External*>(ButtonCtor(this));  // 180047fc8 JMP
}
void ButtonDescription::Destruct() { DeletingDtor(0); }

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
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void* field, Serializer* ser);
    for (int i = 0; i < 5; ++i)                            // 18011142f .. 180111476
        RtAddr<FieldSerFn>(0x180038f90ull)(&_childDesc[i], &ser);

    RwScalar(rw, kRwScalarUInt, &_word90);                 // 18011147b .. 1801114d5
    RwScalar(rw, kRwScalarUInt, &_word94);
    RwScalar(rw, kRwScalarUInt, &_word98);
    RwScalar(rw, kRwScalarUInt, &_word9c);

    DestroyReaderWriter(rw);                               // 1801114ee
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
// DWORD stores (+0xc0 = -1, +0xc4 = 0). Note the widths: both are `MOV dword`,
// not one qword, because the values differ.
Object::InternalPtr<Element>* ButtonDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xd8, 8);                         // 180111552

    ButtonElement* elem = nullptr;
    if (raw != nullptr) {                                  // 18011157d
        ElementCtor(raw);                                  // 180111582
        ButtonElement* e = reinterpret_cast<ButtonElement*>(raw);   // B.3.6 bridge
        *reinterpret_cast<void**>(e) = VtButtonElement();  // 18011158f
        e->_keyIndex = -1;                                 // 180111592
        e->_state    = 0;                                  // 18011159c
        elem = e;
    }

    Object::InternalPtr<ButtonElement> slot(elem);         // 1801115a6
    ButtonDescription::CreateInstance(&slot, dialog);      // 1801115b6

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1801115bb
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
extern "C" uint32_t RtUi_ButtonDescription_Type(const void* self) {
    return reinterpret_cast<const UI::ButtonDescription*>(self)          // B.3.6 bridge
        ->UI::ButtonDescription::Type();
}
extern "C" const char* RtUi_ButtonDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::ButtonDescription*>(self)          // B.3.6 bridge
        ->UI::ButtonDescription::TypeName();
}
extern "C" int32_t RtUi_ButtonDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::ButtonDescription*>(self)          // B.3.6 bridge
        ->UI::ButtonDescription::GetFileVersion();
}

// 0x180047fd0: `TEST RCX,RCX ; JNZ <ctor> ; XOR EAX,EAX ; RET` — retail's null
// test stays in the door.
extern "C" void* RtUi_ButtonDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                  // 180047fd3
    return reinterpret_cast<UI::ButtonDescription*>(selfv)               // B.3.6 bridge
        ->UI::ButtonDescription::Construct();
}
extern "C" void* RtUi_ButtonDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::ButtonDescription*>(selfv)               // B.3.6 bridge
        ->UI::ButtonDescription::Reconstruct();
}
extern "C" void RtUi_ButtonDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::ButtonDescription*>(selfv)                      // B.3.6 bridge
        ->UI::ButtonDescription::Destruct();
}

// ============================================================================
// ??1ButtonDescription@UI@@UEAA@XZ   0x180110bd0   — a DTOR DOOR (B4b-2), kept
// ============================================================================
// TWO vtable stamps back to back — own, then ElementDescription's — with
// nothing between them. Both are kept: a port that writes only the second is a
// different function under any interruption (the reason
// runtime/src/graphics/wave24_lane_gfxmat.cpp gives for the same choice).
extern "C" void RtUi_ButtonDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtButtonDescription();                            // 180110bed
    *v = VtElementDescription();                           // 180110bf7
    ReleaseStringField(p, 0x60);                           // 180110bfa

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180110c2c JMP
}

extern "C" bool RtUi_ButtonDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::ButtonDescription*>(selfv)               // B.3.6 bridge
        ->UI::ButtonDescription::Serialize(streamv);
}

// 0x1801108d0: RCX = this, RDX = &caller's InternalPtr<ButtonElement>, R8 = Dialog*.
extern "C" void RtUi_ButtonDescription_ApplyToElement(const void* selfv,
                                                      void** elemPtr,
                                                      const void* dialog) {
    reinterpret_cast<const UI::ButtonDescription*>(selfv)                // B.3.6 bridge
        ->UI::ButtonDescription::CreateInstance(
            reinterpret_cast<Object::InternalPtr<UI::ButtonElement>*>(elemPtr),
            static_cast<const UI::Dialog*>(dialog));
}

// Slot 8: RCX = this, RDX = the caller's slot, R8 = Dialog*; the slot comes back in RAX.
extern "C" void** RtUi_ButtonDescription_CreateInstance(const void* selfv,
                                                        void** ret,
                                                        const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::ButtonDescription*>(selfv)            // B.3.6 bridge
            ->UI::ButtonDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

int RtUiButtonDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ButtonDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_ButtonDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180047e10ull, (void*)&RtUi_ButtonDescription_Type);

    RegisterRuntimeExport("?TypeName@ButtonDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ButtonDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180047e30ull, (void*)&RtUi_ButtonDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@ButtonDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_ButtonDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1801113d0ull, (void*)&RtUi_ButtonDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ButtonDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047fd0ull, (void*)&RtUi_ButtonDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@ButtonDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_ButtonDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180047fb0ull, (void*)&RtUi_ButtonDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@ButtonDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_ButtonDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047fe0ull, (void*)&RtUi_ButtonDescription_Destruct);

    RegisterRuntimeExport("??1ButtonDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_ButtonDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180110bd0ull, (void*)&RtUi_ButtonDescription_Dtor);

    RegisterRuntimeExport("?Serialize@ButtonDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_ButtonDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1801113e0ull, (void*)&RtUi_ButtonDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@ButtonDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_ButtonDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180111510ull, (void*)&RtUi_ButtonDescription_CreateInstance);

    // ADDRESS ONLY, as before: the export name resolves to this address.
    RegisterRuntimeAddr(0x1801108d0ull, (void*)&RtUi_ButtonDescription_ApplyToElement); ++n;

    return n;
}
