// ============================================================================
// runtime/src/ui/groupdescription.cpp — UI::GroupDescription, the whole class
// ============================================================================
//
// WHY THIS CLASS, AND WHY NOW. The 2026-09-12 cutover boot (make cutover, DLL
// absent) stopped here:
//
//   [RtMint] FATAL: call through a NOT-YET-PORTED minted vtable slot
//   [RtMint]   vtable : ??_7GroupDescription@UI@@6B@
//   [RtMint]   slot   : 2
//   [RtMint]   target : ?Type@GroupDescription@UI@@UEBAIXZ
//
// It is the first UI ElementDescription subclass the dialog load path
// dispatches on, and slot 2 is only where it happened to land first — the whole
// table was traps. So the unit ported here is the CLASS, not the slot.
//
// ??_7GroupDescription@UI@@6B@ (retail 0x180157190), ten slots:
//
//   0  deleting dtor       0x180046af0  runtime/src/object/deleting_dtor.cpp row
//   1  TypeName            0x180046660  here
//   2  Type                0x180046640  here
//   3  GetFileVersion      0x180117cf0  here  (returns 0x104)
//   4  External::Compile   0x1800066e0  already ported
//   5  Serialize           0x180117d00  here
//   6  Reconstruct         0x1800468e0  here
//   7  Destruct            0x180046a20  here
//   8  CreateInstance      0x180117dc0  here
//   9  Construct           0x180046980  here
//
// plus ??1GroupDescription@UI@@UEAA@XZ (0x180117c00).
//
// NOT HERE, and deliberately: `?CreateNewObject@GroupDescription@UI@@`
// (0x180046750) and `?CreateInPlaceObject@` (0x180046840) are ALREADY ported
// and registered, table-driven, by runtime/src/ui/factory_ui1.cpp (the
// kGroupDescription row in factory_ui1_tables.inc, whose 19 stores are the same
// inlined constructor `Construct` below transcribes).
//
// Layout: runtime/types/ui/GroupDescription.h. `ElementDescription` + one
// `Array<ExternalPtr<ElementDescription>>` at +0x68; sizeof 0x78.
//
// ENDGAME B.3.6 block C2: every slot body is a MEMBER of `UI::GroupDescription`;
// the registered doors are one-line QUALIFIED delegations (a live description
// wears the MINTED table, whose slots resolve back to these doors — B34_BRIEFS
// A.1). The destructor stays a door (B4b-2).
//
// ----------------------------------------------------------------------------
// THE CONSTRUCTOR IS INLINED EVERYWHERE AND THERE IS NO OUT-OF-LINE ??0.
// ----------------------------------------------------------------------------
// `Construct` (0x180046980), `Reconstruct` (0x1800468e0) and both factory steps
// each open-code the same 19 stores; the DLL exports no `??0GroupDescription`.
// `ConstructBody` below is that sequence written once, transcribed from
// 0x180046996..0x180046a06. Two details Ghidra's C loses:
//   * `+0x40` is written as ONE QWORD holding 3 (`MOV qword [RCX+0x40],3` at
//     0x1800469e0), so it covers `_elementType` AND `_flags` (+0x44).
//   * `+0x38` and `+0x3c` are two SEPARATE DWORDS (0 and -1), because the
//     values differ. Not one qword.
// The three dead base-vtable stamps (Base, then External over it unread, then
// ElementDescription) are reproduced: they are four instructions in the shipped
// bytes and collapsing them would be a divergence for no gain.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/ui/GroupDescription.h"
#include "../../types/ui/GroupElement.h"

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

std::atomic<void*> g_vtBase{nullptr};
std::atomic<void*> g_vtExternal{nullptr};
std::atomic<void*> g_vtElementDescription{nullptr};
std::atomic<void*> g_vtGroupDescription{nullptr};
std::atomic<void*> g_vtGroupElement{nullptr};
std::atomic<void*> g_groupDescriptionTypeInfo{nullptr};

void* AtVtBase()     { return AtomicResolve(g_vtBase, "??_7Base@Object@@6B@"); }
void* AtVtExternal() { return AtomicResolve(g_vtExternal, "??_7External@Object@@6B@"); }
void* AtVtElementDescription() {
    return AtomicResolve(g_vtElementDescription, "??_7ElementDescription@UI@@6B@");
}
void* AtVtGroupDescription() { return AtomicResolve(g_vtGroupDescription, RtUI::kVtblGroupDescription); }
void* AtVtGroupElement()     { return AtomicResolve(g_vtGroupElement, RtUI::kVtblGroupElement); }
const uint8_t* GroupDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_groupDescriptionTypeInfo,
        "?_typeInfo@GroupDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// Door-only (the dtor door is not a member).
void* VtGroupDescription() { return RT_DATA_SLOT(void*, RtUI::kVtblGroupDescription); }

// ============================================================================
// The inlined constructor, transcribed once (0x180046996..0x180046a06).
// ============================================================================
void ConstructBody(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = AtVtBase();                                            // 180046996
    *v = AtVtExternal();                                        // 1800469a0
    // `LEA RAX,[RCX+8]` then `MOV [RAX],RDX` -- retail writes +0x08 through an
    // intermediate pointer (the SEH frame's visible temporary) and +0x10/+0x18
    // directly. Byte-identical; the temporary costs nothing to drop.
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800469b5
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800469b8
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800469bc
    *v = AtVtElementDescription();                              // 1800469c0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800469ca
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800469ce
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800469d2
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800469d6  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800469d9  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800469e0  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800469e8
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800469ec
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800469f0
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800469f4
    *v = AtVtGroupDescription();                                // 1800469f8
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 180046a02
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 180046a06  _size+_capacity
}

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

// `Type` returns the FIRST DWORD of the BaseTypeInfo static and `TypeName` the
// pointer at +0x08 — retail reads them as two separate globals
// (`?_typeInfo@GroupDescription@UI@@` and the label Ghidra prints as
// `PTR_s_GroupDescription_1801faba8`, which is that static + 8), the same two
// fields runtime/types/object/TypeInfo.h pins.
uint32_t GroupDescription::Type() const {
    const uint8_t* ti = GroupDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::GroupDescription` here. Nothing
// stamps it; every live object wears the minted table.
const char* GroupDescription::TypeName() const {
    const uint8_t* ti = GroupDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x180117cf0: `MOV EAX,0x104 ; RET`. 0x104 == 260, the dialog resource file
// version this class understands.
int32_t GroupDescription::GetFileVersion() const { return 0x104; }

// ============================================================================
// Construct / Reconstruct / Destruct
// ============================================================================
// `Construct` is the ctor with a null test (the test is the DOOR's — a member
// cannot test `this`); `Reconstruct` is `slot0(this, 0)` — the DELETING
// DESTRUCTOR with flag 0, i.e. destroy WITHOUT freeing, through the object's
// own (minted) table — and then the ctor again, with NO null test (retail
// derefs `this` to reach the vtable before it could test it).
Object::External* GroupDescription::Construct() {
    ConstructBody(this);
    return this;
}
Object::External* GroupDescription::Reconstruct() {
    DeletingDtor(0);                                           // 1800468f7
    ConstructBody(this);
    return this;
}
// 0x180046a20 is a TAIL JUMP into slot 0 with flag 0 (`JMP [RAX]`).
void GroupDescription::Destruct() { DeletingDtor(0); }

// ============================================================================
// ?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x180117d00
// ============================================================================
//   ElementDescription::Serialize(this, stream)            0x180116b00
//   pair = { stream, nullptr }
//   rw   = stream->mode == 0 ? factory->vt[0x18](factory,&pair,0)
//                            : factory->vt[0x20](factory,&pair,0)
//   n    = _children._size
//   rw->vt[0xa0](rw, &n, 0)
//   if (pair.stream->mode == 0) SetSize(&_children, n)      0x1800492d0, ported
//   ElementDescriptionPtrArray_Serialize(&pair, n, _children._data)
//                                                          0x1800493d0, ported
//   if (rw) rw->vt[0](rw, 1)
//   return true
//
// The base call is QUALIFIED: this object wears the leaf's table, whose slot 5
// is this very function.
//
// THE MODE IS RE-READ FROM THE PAIR'S COPY, not from the argument: the
// disassembly is `MOV RAX,[RSP+0x28] ; CMP dword [RAX+8],0` at
// 0x180117d6c..0x180117d71, one deref deeper than the first test at
// 0x180117d3c. Transcribed rather than folded.
bool GroupDescription::Serialize(void* streamv) {
    rtdesc::Stream* stream = static_cast<rtdesc::Stream*>(streamv);

    ElementDescription::Serialize(stream);                     // 180117d1a

    rtdesc::Serializer ser = { stream, nullptr };              // [RSP+0x28], [RSP+0x30]
    void* rw = rtdesc::MakeReaderWriter(stream, &ser);         // 180117d4a

    int32_t n = _children._size;                               // 180117d4f
    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &n);            // 180117d66

    if (ser.stream->mode == 0) {                               // 180117d6c (re-read)
        using SetSizeFn = void (*)(void* array, int n);
        rtdesc::RtAddr<SetSizeFn>(0x1800492d0ull)(&_children, n);   // 180117d7f
    }

    using ElemSerFn = void (*)(rtdesc::Serializer* ser, int count, void* data);
    rtdesc::RtAddr<ElemSerFn>(0x1800493d0ull)(&ser, n, _children._data);

    rtdesc::DestroyReaderWriter(rw);                           // 180117d9c
    return true;                                               // MOV AL,1
}

// ============================================================================
// ?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z
// retail 0x180117dc0   (slot 8, RULE B6-2)
// ============================================================================
// RCX = this, RDX = the hidden return slot, R8 = the Dialog*. Ghidra prints
// `CreateInstance(GroupDescription*, Dialog*)` and DROPS the third register;
// the disassembly is unambiguous: `MOV R15,R8` (the Dialog, used only as
// CreateElement's `this`), `MOV R14,RDX` (written through at 0x180117ed1),
// `MOV RSI,RCX`.
//
// ONE STACK SLOT DOES TWO JOBS, and it must stay one variable here. Retail's
// [RSP+0x50] holds the new GroupElement when it is handed to
// `ElementDescription::CreateInstance` (which applies the description to it),
// and is then REUSED as `Dialog::CreateElement`'s return slot inside the loop.
//
// `ElementDescription::CreateInstance` (0x180115ad0) is reached through its
// registered door: its member is declared in ElementDescription.h but its body
// is still dialog.cpp's door (fragment pass), so a member call would not link.
// `??0Element` and `Dialog::CreateElement` are doors for the same reason.
//
// The parent-chain walk at the end of each iteration is retail's: set
// `kElementFlagNeedsSizeCalc` on the group and on every ancestor through
// `Element::_parent`, until the chain ends.
Object::InternalPtr<Element>* GroupDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    // The locked wrapper, which is the Enter/Allocate/Leave triple retail
    // inlines at 0x180117deb..0x180117e25.
    void* raw = rtdesc::MemAlloc(0xc0, 8);

    GroupElement* elem = nullptr;
    if (raw != nullptr) {                                      // 180117e32
        rtdesc::ElementCtor(raw);                              // 180117e3a ??0Element
        *static_cast<void**>(raw) = AtVtGroupElement();        // 180117e47
        elem = reinterpret_cast<GroupElement*>(raw);           // B.3.6 bridge
    }

    void* slot = elem;                                         // 180117e4f [RSP+0x50]
    rtdesc::ElementDescriptionApply(this, &slot);              // 180117e5c

    const int32_t count = _children._size;
    for (int32_t i = 0; i < count; ++i) {                      // 180117e61 / 180117ec9
        void* desc = _children._data[i];                       // 180117e77

        void** made = rtdesc::DialogCreateElement(dialog, &slot, desc);

        Element* child = *reinterpret_cast<Element**>(made);   // 180117e88
        if (child != nullptr)
            child->_parent = elem;                             // 180117e90  Element::_parent

        // Array<Element*>::SetSize(&elem->_children, size + 1, made) — the
        // three-argument (fill) form, ported as array.cpp's kSpec_Fill8Z.
        using PushFn = void (*)(void* array, int n, const void* fill);
        rtdesc::RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                               elem->_children._size + 1,
                                               made);          // 180117ea9

        for (Element* n = elem; n != nullptr; n = n->_parent)  // 180117eb1..180117ec5
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 180117ed1
    return ret;                                                // MOV RAX,R14
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
extern "C" uint32_t RtUi_GroupDescription_Type(const void* self) {
    return reinterpret_cast<const UI::GroupDescription*>(self)          // B.3.6 bridge
        ->UI::GroupDescription::Type();
}
extern "C" const char* RtUi_GroupDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::GroupDescription*>(self)          // B.3.6 bridge
        ->UI::GroupDescription::TypeName();
}
extern "C" int32_t RtUi_GroupDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::GroupDescription*>(self)          // B.3.6 bridge
        ->UI::GroupDescription::GetFileVersion();
}
// Retail's null test (180046991) stays in the door.
extern "C" void* RtUi_GroupDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180046991
    return reinterpret_cast<UI::GroupDescription*>(selfv)               // B.3.6 bridge
        ->UI::GroupDescription::Construct();
}
extern "C" void* RtUi_GroupDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::GroupDescription*>(selfv)               // B.3.6 bridge
        ->UI::GroupDescription::Reconstruct();
}
extern "C" void RtUi_GroupDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::GroupDescription*>(selfv)                      // B.3.6 bridge
        ->UI::GroupDescription::Destruct();
}
extern "C" bool RtUi_GroupDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::GroupDescription*>(selfv)               // B.3.6 bridge
        ->UI::GroupDescription::Serialize(streamv);
}
// Slot 8: RCX = this, RDX = the caller's slot, R8 = Dialog*; slot back in RAX.
extern "C" void** RtUi_GroupDescription_CreateInstance(const void* selfv,
                                                       void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::GroupDescription*>(selfv)            // B.3.6 bridge
            ->UI::GroupDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1GroupDescription@UI@@UEAA@XZ   0x180117c00   — a DTOR DOOR (B4b-2), kept
// ============================================================================
// Re-stamp own vtable, destroy the child array, re-stamp the BASE vtable, then
// release `ElementDescription::_textName` at +0x60 and TAIL-JUMP to
// `??1External@Object@@` (0x1800bc950 — `JMP`, not `CALL`, at 0x180117c66).
//
// THE STRING RELEASE KEEPS RETAIL'S ASYMMETRY (description_common.h fragment
// 3): when the refcount reaches zero retail rewrites THE FIELD to `data - 8`
// and frees THAT; otherwise it NULLS the field.
extern "C" void RtUi_GroupDescription_Dtor(void* selfv) {
    uint8_t* p = reinterpret_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtGroupDescription();                                 // 180117c1d

    using ArrayDtorFn = void* (*)(void*);
    RtAddr<ArrayDtorFn>(0x180039440ull)(p + 0x68);             // 180117c24  Array::~Array (J1)

    *v = VtElementDescription();                               // 180117c31

    ReleaseStringField(p, 0x60);                               // 180117c38..180117c56

    using ExtDtorFn = void (*)(void*);
    RT_EXPORT_ONCE(ExtDtorFn, "??1External@Object@@UEAA@XZ")(p);   // 180117c66 JMP
}

// ============================================================================
// Registration. Both doors for every row: the mangled name (which is what the
// minted BaseTypeInfo and the minted vtable ask for first) and the address
// (which is what a ported caller's `ResolveAddr` literal asks for).
// ============================================================================
int RtUiGroupDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@GroupDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_GroupDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180046640ull, (void*)&RtUi_GroupDescription_Type);

    RegisterRuntimeExport("?TypeName@GroupDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_GroupDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180046660ull, (void*)&RtUi_GroupDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@GroupDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_GroupDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x180117cf0ull, (void*)&RtUi_GroupDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GroupDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180046980ull, (void*)&RtUi_GroupDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@GroupDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_GroupDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800468e0ull, (void*)&RtUi_GroupDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@GroupDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_GroupDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180046a20ull, (void*)&RtUi_GroupDescription_Destruct);

    RegisterRuntimeExport("??1GroupDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_GroupDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x180117c00ull, (void*)&RtUi_GroupDescription_Dtor);

    RegisterRuntimeExport("?Serialize@GroupDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_GroupDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x180117d00ull, (void*)&RtUi_GroupDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@GroupDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_GroupDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x180117dc0ull, (void*)&RtUi_GroupDescription_CreateInstance);

    return n;
}
