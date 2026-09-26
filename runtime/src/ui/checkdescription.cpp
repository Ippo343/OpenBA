// ============================================================================
// runtime/src/ui/checkdescription.cpp — UI::CheckDescription, the whole class
// ============================================================================
//
// ??_7CheckDescription@UI@@6B@ (retail 0x1801572c0), ten slots:
//   0  deleting dtor      0x1800489c0  runtime/src/object/deleting_dtor.cpp row
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

// RULE B6-5: namespace-scope atomic door caches, never a function-local static
// inside a member (see buttondescription.cpp).
void* AtomicResolve(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtCheckDescription{nullptr};
std::atomic<void*> g_vtButtonElement{nullptr};
std::atomic<void*> g_vtCheckElement{nullptr};
std::atomic<void*> g_checkTypeInfo{nullptr};
std::atomic<void*> g_buttonCtor{nullptr};

void* VtCheckDescription() { return AtomicResolve(g_vtCheckDescription, RtUI::kVtblCheckDescription); }
void* VtButtonElement()    { return AtomicResolve(g_vtButtonElement, RtUI::kVtblButtonElement); }
void* VtCheckElement()     { return AtomicResolve(g_vtCheckElement, RtUI::kVtblCheckElement); }
const uint8_t* CheckTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_checkTypeInfo, "?_typeInfo@CheckDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

void* ButtonCtor(void* self) {
    using CtorFn = void* (*)(void*);
    return reinterpret_cast<CtorFn>(AtomicResolve(
        g_buttonCtor, "??0ButtonDescription@UI@@QEAA@XZ"))(self);
}

// The dtor door only.
void* VtButtonDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblButtonDescription); }

}  // namespace

namespace UI {

uint32_t CheckDescription::Type() const {
    const uint8_t* ti = CheckTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
const char* CheckDescription::TypeName() const {
    const uint8_t* ti = CheckTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t CheckDescription::GetFileVersion() const { return 0x1104; }

// 0x1800488a0: null test (the DOOR's), base ctor, own two stores, return this.
// The two stores are transcribed from 0x1800488c2..0x1800488cc (and
// identically from 0x180048877..0x180048881 inside Reconstruct); the vtable
// stamp is retail's own store over the one ??0ButtonDescription just made.
Object::External* CheckDescription::Construct() {
    ButtonCtor(this);                                          // 1800488bc
    *reinterpret_cast<void**>(this) = VtCheckDescription();    // 1800488c9
    _checkDesc = nullptr;                                      // 1800488cc
    return this;                                               // 1800488d7
}
// 0x180048850: slot 0 with flag 0, then the same construction, NO null test.
Object::External* CheckDescription::Reconstruct() {
    DeletingDtor(0);                                           // 180048867
    ButtonCtor(this);                                          // 180048871
    *reinterpret_cast<void**>(this) = VtCheckDescription();    // 18004887e
    _checkDesc = nullptr;                                      // 180048881
    return this;                                               // 18004888c
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
    void* rw = MakeReaderWriter(stream, &ser);

    using FieldSerFn = void (*)(void*, Serializer*);
    RtAddr<FieldSerFn>(0x180038f90ull)(&_checkDesc, &ser); // 18011198b

    DestroyReaderWriter(rw);                               // 1801119a3
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
// so the ButtonElement vtable is written and then overwritten. Kept.
//
// Then the shared ButtonDescription applier (0x1801108d0), then ONE more child
// from `_checkDesc`, attached to the element retail still holds in RDI —
// not re-read from the slot the way the applier's own loop does it. The slot
// is the SAME stack local the applier received and CreateElement returns into.
Object::InternalPtr<Element>* CheckDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = MemAlloc(0xe0, 8);                         // 180111a02

    CheckElement* elem = nullptr;
    if (raw != nullptr) {                                  // 180111a2d
        ElementCtor(raw);                                  // 180111a32
        CheckElement* e = reinterpret_cast<CheckElement*>(raw);   // B.3.6 bridge
        *reinterpret_cast<void**>(e) = VtButtonElement();  // 180111a3f
        e->_keyIndex = -1;                                 // 180111a42
        e->_state    = 0;                                  // 180111a4c
        *reinterpret_cast<void**>(e) = VtCheckElement();   // 180111a5d
        e->_checked  = 0;                                  // 180111a60 BYTE
        elem = e;
    }

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
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_CheckDescription_Type(const void* self) {
    return reinterpret_cast<const UI::CheckDescription*>(self)           // B.3.6 bridge
        ->UI::CheckDescription::Type();
}
extern "C" const char* RtUi_CheckDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::CheckDescription*>(self)           // B.3.6 bridge
        ->UI::CheckDescription::TypeName();
}
extern "C" int32_t RtUi_CheckDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::CheckDescription*>(self)           // B.3.6 bridge
        ->UI::CheckDescription::GetFileVersion();
}

// 0x1800488a0: the retail null test stays in the door (1800488ba).
extern "C" void* RtUi_CheckDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;
    return reinterpret_cast<UI::CheckDescription*>(selfv)                // B.3.6 bridge
        ->UI::CheckDescription::Construct();
}
extern "C" void* RtUi_CheckDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::CheckDescription*>(selfv)                // B.3.6 bridge
        ->UI::CheckDescription::Reconstruct();
}
extern "C" void RtUi_CheckDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::CheckDescription*>(selfv)                       // B.3.6 bridge
        ->UI::CheckDescription::Destruct();
}

// ============================================================================
// ??1CheckDescription@UI@@UEAA@XZ   0x180111620   — a DTOR DOOR (B4b-2), kept
// ============================================================================
// THREE vtable stamps back to back — own, ButtonDescription's, then
// ElementDescription's — with nothing between them, then the base's _textName.
// All three are kept; see buttondescription.cpp's ~ButtonDescription note.
// The +0xa0 child description is NOT released.
extern "C" void RtUi_CheckDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtCheckDescription();                             // 18011163d
    *v = VtButtonDescription();                            // 180111647
    *v = VtElementDescription();                           // 180111651
    ReleaseStringField(p, 0x60);                           // 180111654

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180111686 JMP
}

extern "C" bool RtUi_CheckDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::CheckDescription*>(selfv)                // B.3.6 bridge
        ->UI::CheckDescription::Serialize(streamv);
}

extern "C" void** RtUi_CheckDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::CheckDescription*>(selfv)             // B.3.6 bridge
            ->UI::CheckDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

int RtUiCheckDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@CheckDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_CheckDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180048660ull, (void*)&RtUi_CheckDescription_Type);

    RegisterRuntimeExport("?TypeName@CheckDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_CheckDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180048680ull, (void*)&RtUi_CheckDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@CheckDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_CheckDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x180111920ull, (void*)&RtUi_CheckDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CheckDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x1800488a0ull, (void*)&RtUi_CheckDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@CheckDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_CheckDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180048850ull, (void*)&RtUi_CheckDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@CheckDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_CheckDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x1800488f0ull, (void*)&RtUi_CheckDescription_Destruct);

    RegisterRuntimeExport("??1CheckDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_CheckDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180111620ull, (void*)&RtUi_CheckDescription_Dtor);

    RegisterRuntimeExport("?Serialize@CheckDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_CheckDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180111930ull, (void*)&RtUi_CheckDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@CheckDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_CheckDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1801119c0ull, (void*)&RtUi_CheckDescription_CreateInstance);

    return n;
}
