// ============================================================================
// runtime/src/ui/radiodescription.cpp — UI::RadioDescription, the whole class
// ============================================================================
//
// Vtable batch 2. THIS CLASS DERIVES FROM UI::GroupDescription, and that is the
// one thing about it that is not shaped like its siblings: three of its bodies
// are one line of its own plus a call into GroupDescription's copy
// (runtime/src/ui/groupdescription.cpp).
//
// ??_7RadioDescription@UI@@6B@ (retail 0x180157420), ten slots:
//   0  deleting dtor      0x180047930  deleting_dtor.cpp row (sizeof 0x80)
//   1  TypeName           0x180047670  here
//   2  Type               0x180047650  here
//   3  GetFileVersion     0x1800d5b10  here  (returns 0x1104 — per class)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d5a80  here
//   6  Reconstruct        0x1800477f0  here
//   7  Destruct           0x180047820  here
//   8  CreateInstance     0x1800d58d0  here
//   9  Construct          0x180047810  here
// plus ??0RadioDescription@UI@@QEAA@XZ 0x1800d5b50 (OUT OF LINE — both
// Construct and Reconstruct tail-jump to it) and
// ??1RadioDescription@UI@@UEAA@XZ 0x1800d5b20 — both DOORS (B4b-2).
//
// LAYOUT: runtime/types/ui/RadioDescription.h — GroupDescription (0x78) plus
// `_activeRadio` (+0x78, ctor -1); sizeof 0x80.
//
// ENDGAME B.3.6 block C2: every slot body is a MEMBER of `UI::RadioDescription`;
// the registered doors are one-line QUALIFIED delegations. Serialize's base
// call (by address 0x180117d00 before) is the qualified member
// `GroupDescription::Serialize`.
//
// DOORS THIS CLASS STILL REACHES BY ADDRESS, each because its member is not
// defined in the real build yet:
//   0x180115ad0 ElementDescription::CreateInstance, 0x180112ed0
//       Dialog::CreateElement (dialog.cpp, fragment pass / block B)
//   0x180115e50 ??0Element (element.cpp, a ctor door)
//   0x180076900 Array<InternalPtr>::SetSize (array.cpp)
//   0x1800d5750 RadioElement::FindCheckBoxes (private; widgets_input.cpp)
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/ui/RadioDescription.h"
#include "../../types/ui/RadioElement.h"

namespace {

using namespace rtdesc;

// RULE B6-5: namespace-scope atomic door caches for the member bodies.
void* AtomicResolve(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}

std::atomic<void*> g_vtGroupElement{nullptr};
std::atomic<void*> g_vtRadioElement{nullptr};
std::atomic<void*> g_radioDescriptionTypeInfo{nullptr};

void* AtVtGroupElement() { return AtomicResolve(g_vtGroupElement, "??_7GroupElement@UI@@6B@"); }
void* AtVtRadioElement() { return AtomicResolve(g_vtRadioElement, "??_7RadioElement@UI@@6B@"); }
const uint8_t* RadioDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_radioDescriptionTypeInfo,
        "?_typeInfo@RadioDescription@UI@@0VBaseTypeInfo@Object@@A"));
}

// Door-only (the ctor/dtor doors are not members).
void* VtGroupDescription() { return RT_DATA_SLOT(void*, "??_7GroupDescription@UI@@6B@"); }
void* VtRadioDescription() { return RT_DATA_SLOT(void*, "??_7RadioDescription@UI@@6B@"); }

}  // namespace

// ============================================================================
// ??0RadioDescription@UI@@QEAA@XZ   retail 0x1800d5b50   — a CTOR DOOR (B4b-2)
// ============================================================================
// The whole GroupDescription constructor INLINED (the shipped body does not
// call it), then the RadioDescription vtable and the one new field.
extern "C" void* RtUi_RadioDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d5b61
    *v = VtExternal();                                          // 1800d5b6b
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d5b80
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d5b83
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d5b87
    *v = VtElementDescription();                                // 1800d5b8b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d5b95
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d5b99
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d5b9d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d5ba1  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d5ba4  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d5bab  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d5bb3
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d5bb7
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d5bbb
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d5bbf
    *v = VtGroupDescription();                                  // 1800d5bc3
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d5bcd  child array
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d5bd1  size+cap
    *v = VtRadioDescription();                                  // 1800d5bd5
    *reinterpret_cast<int32_t*>(p + 0x78)  = -1;                // 1800d5bdf
    return p;                                                   // 1800d5be6
}

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

uint32_t RadioDescription::Type() const {                      // 0x180047650
    const uint8_t* ti = RadioDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::RadioDescription` here.
const char* RadioDescription::TypeName() const {               // 0x180047670
    const uint8_t* ti = RadioDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
// 0x1800d5b10: `MOV EAX,0x1104 ; RET`.
int32_t RadioDescription::GetFileVersion() const { return 0x1104; }

// 0x180047810: the null test is the DOOR's; then the out-of-line ctor.
Object::External* RadioDescription::Construct() {
    return static_cast<Object::External*>(RtUi_RadioDescription_Ctor(this));
}
// 0x1800477f0: slot 0 with flag 0 through the object's own table, then the
// ctor (tail jump at 0x180047808). No null test.
Object::External* RadioDescription::Reconstruct() {
    DeletingDtor(0);                                           // 1800477fe
    return static_cast<Object::External*>(RtUi_RadioDescription_Ctor(this));
}
void RadioDescription::Destruct() { DeletingDtor(0); }         // 0x180047820

// ============================================================================
// ?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5a80
// ============================================================================
//   GroupDescription::Serialize(this, stream)     0x180117d00, QUALIFIED
//   rw = encoding factory, read side or write side by stream->mode, version 0
//   rw->vt[0xa0](rw, &_activeRadio, 0)   kScalarInt
//   if (rw) rw->vt[0](rw, 1)
//
// THE BASE CALL IS THE PARENT'S Serialize, NOT ElementDescription's — using the
// grandparent here would silently drop every child. It ALSO opens a SECOND
// ReaderWriter over the same stream, after GroupDescription::Serialize has
// already opened and closed its own. That is retail.
bool RadioDescription::Serialize(void* streamv) {
    rtdesc::Stream* stream = static_cast<rtdesc::Stream*>(streamv);

    GroupDescription::Serialize(stream);                       // 1800d5a9a

    rtdesc::Serializer ser = { stream, nullptr };              // [RSP+0x28], [RSP+0x30]
    void* rw = rtdesc::MakeReaderWriter(stream, &ser);         // 1800d5aca

    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &_activeRadio);

    rtdesc::DestroyReaderWriter(rw);                           // 1800d5aed
    return true;
}

// ============================================================================
// ?CreateInstance@RadioDescription@UI@@...   retail 0x1800d58d0   (slot 8)
// ============================================================================
// RCX = this, RDX = the hidden InternalPtr<Element> return slot, R8 = Dialog*.
//
// RadioElement is 0xd8 bytes (`MOV EDX,0xd8` at 0x1800d5916) and its
// construction stamps TWO vtables in a row — GroupElement (0x1800d594c) and
// then RadioElement (0x1800d5956) — before zeroing its own fields:
//     +0xc0 _checkBoxes._data, +0xc8 ._size/._capacity, +0xd0 _activeIndex = -1
//
// THE TAIL IS THIS CLASS'S WHOLE POINT:
//   FindCheckBoxes(elem, &elem)                        0x1800d5750
//   if (_activeRadio != elem->_activeIndex) {
//       elem->_activeIndex = _activeRadio;
//       for (i = 0; i < elem->_checkBoxes._size; ++i)
//           if (i != elem->_activeIndex) checkBoxes[i]->+0xd8 = 0;   // UNCHECK
//   }
// The inner test re-reads `elem->_activeIndex` (0x1800d5a30) rather than the
// register it was just written from. Kept.
Object::InternalPtr<Element>* RadioDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = rtdesc::MemAlloc(0xd8, 8);                     // 1800d5916 (inlined)

    RadioElement* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d5941
        rtdesc::ElementCtor(raw);                              // 1800d5946 ??0Element
        *static_cast<void**>(raw) = AtVtGroupElement();        // 1800d594c
        *static_cast<void**>(raw) = AtVtRadioElement();        // 1800d5956
        uint8_t* e = static_cast<uint8_t*>(raw);
        *reinterpret_cast<uint64_t*>(e + 0xc0) = 0;            // 1800d5962  _checkBoxes._data
        *reinterpret_cast<uint64_t*>(e + 0xc8) = 0;            // 1800d5969  _size + _capacity
        elem = reinterpret_cast<RadioElement*>(raw);           // B.3.6 bridge
        elem->_activeIndex = -1;                               // 1800d5970  DWORD
    }

    void* slot = elem;                                         // 1800d5980
    UI::ElementDescription::CreateInstance(                   // 1800d598d
        reinterpret_cast<Object::InternalPtr<Element>*>(&slot));  // B.3.6 bridge

    const int32_t count = _children._size;
    for (int32_t i = 0; i < count; ++i) {                      // 1800d5994 / 1800d59f9
        void* desc = _children._data[i];                       // 1800d59a7

        void** made = rtdesc::DialogCreateElement(dialog, &slot, desc);

        Element* child = *reinterpret_cast<Element**>(made);   // 1800d59b8
        if (child != nullptr)
            child->_parent = elem;                             // 1800d59c0

        using PushFn = void (*)(void* array, int n, const void* fill);
        rtdesc::RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                               elem->_children._size + 1,
                                               made);          // 1800d59d9

        for (Element* n = elem; n != nullptr; n = n->_parent)  // 1800d59e1..1800d59f5
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    // The PRIVATE `FindCheckBoxes(InternalPtr<Element>)` (0x1800d5750) — a
    // widgets_input.cpp door (fragment pass); retail passes the address of a
    // SECOND stack slot holding the element (RULE B6-11).
    void* self2 = elem;                                        // 1800d59fe
    using FindCheckBoxesFn = void (*)(void* elem, void** root);
    rtdesc::RtAddr<FindCheckBoxesFn>(0x1800d5750ull)(elem, &self2);   // 1800d5a0b

    const int32_t want = _activeRadio;
    if (want != elem->_activeIndex) {                          // 1800d5a13
        elem->_activeIndex = want;                             // 1800d5a1b
        const int32_t boxes = elem->_checkBoxes._size;
        for (int32_t i = 0; i < boxes; ++i) {                  // 1800d5a21 / 1800d5a50
            if (i == elem->_activeIndex) continue;             // 1800d5a30 RE-READ
            uint8_t* box = reinterpret_cast<uint8_t*>(elem->_checkBoxes._data[i]);
            box[0xd8] = 0;                                     // 1800d5a43  the CHECKBOX's byte
        }
    }

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d5a58
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_RadioDescription_Type(const void* self) {
    return reinterpret_cast<const UI::RadioDescription*>(self)          // B.3.6 bridge
        ->UI::RadioDescription::Type();
}
extern "C" const char* RtUi_RadioDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::RadioDescription*>(self)          // B.3.6 bridge
        ->UI::RadioDescription::TypeName();
}
extern "C" int32_t RtUi_RadioDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::RadioDescription*>(self)          // B.3.6 bridge
        ->UI::RadioDescription::GetFileVersion();
}
extern "C" void* RtUi_RadioDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180047810
    return reinterpret_cast<UI::RadioDescription*>(selfv)               // B.3.6 bridge
        ->UI::RadioDescription::Construct();
}
extern "C" void* RtUi_RadioDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::RadioDescription*>(selfv)               // B.3.6 bridge
        ->UI::RadioDescription::Reconstruct();
}
extern "C" void RtUi_RadioDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::RadioDescription*>(selfv)                      // B.3.6 bridge
        ->UI::RadioDescription::Destruct();
}
extern "C" bool RtUi_RadioDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::RadioDescription*>(selfv)               // B.3.6 bridge
        ->UI::RadioDescription::Serialize(streamv);
}
extern "C" void** RtUi_RadioDescription_CreateInstance(const void* selfv, void** ret,
                                                       const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::RadioDescription*>(selfv)            // B.3.6 bridge
            ->UI::RadioDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ============================================================================
// ??1RadioDescription@UI@@UEAA@XZ   0x1800d5b20   — a DTOR DOOR (B4b-2)
// ============================================================================
// Four instructions: restamp this class's vtable, then a TAIL JMP to
// ~GroupDescription (0x180117c00). It owns nothing of its own.
extern "C" void RtUi_RadioDescription_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtRadioDescription();   // 1800d5b32
    using DtorFn = void (*)(void*);
    RtAddr<DtorFn>(0x180117c00ull)(selfv);                     // 1800d5b40 JMP
}

int RtUiRadioDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RadioDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_RadioDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180047650ull, (void*)&RtUi_RadioDescription_Type);

    RegisterRuntimeExport("?TypeName@RadioDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_RadioDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180047670ull, (void*)&RtUi_RadioDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@RadioDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_RadioDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d5b10ull, (void*)&RtUi_RadioDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RadioDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047810ull, (void*)&RtUi_RadioDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@RadioDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_RadioDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x1800477f0ull, (void*)&RtUi_RadioDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@RadioDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_RadioDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047820ull, (void*)&RtUi_RadioDescription_Destruct);

    RegisterRuntimeExport("??0RadioDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_RadioDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d5b50ull, (void*)&RtUi_RadioDescription_Ctor);

    RegisterRuntimeExport("??1RadioDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_RadioDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d5b20ull, (void*)&RtUi_RadioDescription_Dtor);

    RegisterRuntimeExport("?Serialize@RadioDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_RadioDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d5a80ull, (void*)&RtUi_RadioDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@RadioDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_RadioDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d58d0ull, (void*)&RtUi_RadioDescription_CreateInstance);

    return n;
}
