// ============================================================================
// runtime/src/ui/pagedescription.cpp — UI::PageDescription, the whole class
// ============================================================================
//
// Vtable batch 2. Like UI::RadioDescription, this class DERIVES FROM
// UI::GroupDescription: its dtor tail-jumps to ~GroupDescription and its
// Serialize opens with GroupDescription::Serialize, both in
// runtime/src/ui/groupdescription.cpp.
//
// ??_7PageDescription@UI@@6B@ (retail 0x1801570e0), ten slots:
//   0  deleting dtor      0x1800471a0  deleting_dtor.cpp row (sizeof 0x80)
//   1  TypeName           0x180046ee0  here
//   2  Type               0x180046ec0  here
//   3  GetFileVersion     0x1800d54a0  here  (returns 0x1104)
//   4  External::Compile  0x1800066e0  already ported
//   5  Serialize          0x1800d5410  here
//   6  Reconstruct        0x180047060  here
//   7  Destruct           0x180047090  here
//   8  CreateInstance     0x1800d52b0  here
//   9  Construct          0x180047080  here
// plus ??0PageDescription@UI@@QEAA@XZ 0x1800d54e0 (out of line) and
// ??1PageDescription@UI@@UEAA@XZ 0x1800d54b0 — both DOORS (B4b-2).
//
// LAYOUT: runtime/types/ui/PageDescription.h — GroupDescription (0x78) plus
// `_activePage` (+0x78); sizeof 0x80.
//
// ENDGAME B.3.6 block C2: every slot body is a MEMBER of `UI::PageDescription`;
// the registered doors are one-line QUALIFIED delegations. Serialize's base
// call (by address 0x180117d00 before) is the qualified member
// `GroupDescription::Serialize`.
// ============================================================================
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>

#include "description_common.h"
#include "../../types/ui/PageDescription.h"
#include "../../types/ui/PageElement.h"

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
std::atomic<void*> g_vtPageElement{nullptr};
std::atomic<void*> g_pageDescriptionTypeInfo{nullptr};
std::atomic<void*> g_setActivePage{nullptr};

void* AtVtGroupElement() { return AtomicResolve(g_vtGroupElement, "??_7GroupElement@UI@@6B@"); }
void* AtVtPageElement()  { return AtomicResolve(g_vtPageElement, "??_7PageElement@UI@@6B@"); }
const uint8_t* PageDescriptionTypeInfo() {
    return static_cast<const uint8_t*>(AtomicResolve(
        g_pageDescriptionTypeInfo,
        "?_typeInfo@PageDescription@UI@@0VBaseTypeInfo@Object@@A"));
}
// ?SetActivePage@PageElement@UI@@QEAAXH@Z (0x1800d5110) — the member is
// declared in PageElement.h, but its body is widgets_layout.cpp's registered
// door (fragment pass), so a member call would not link: reached by name.
void CallSetActivePage(UI::PageElement* elem, int32_t page) {
    using SetActivePageFn = void (*)(void*, int32_t);
    reinterpret_cast<SetActivePageFn>(AtomicResolve(
        g_setActivePage, "?SetActivePage@PageElement@UI@@QEAAXH@Z"))(elem, page);
}

// Door-only (the ctor/dtor doors are not members).
void* VtGroupDescription() { return RT_DATA_SLOT(void*, "??_7GroupDescription@UI@@6B@"); }
void* VtPageDescription()  { return RT_DATA_SLOT(void*, "??_7PageDescription@UI@@6B@"); }

}  // namespace

// ============================================================================
// ??0PageDescription@UI@@QEAA@XZ   retail 0x1800d54e0   — a CTOR DOOR (B4b-2)
// ============================================================================
extern "C" void* RtUi_PageDescription_Ctor(void* selfv) {
    uint8_t* p = static_cast<uint8_t*>(selfv);
    void**   v = reinterpret_cast<void**>(p);

    *v = VtBase();                                              // 1800d54f1
    *v = VtExternal();                                          // 1800d54fb
    *reinterpret_cast<uint64_t*>(p + 0x08) = 0;                 // 1800d5510
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;                 // 1800d5513
    *reinterpret_cast<uint64_t*>(p + 0x18) = 0;                 // 1800d5517
    *v = VtElementDescription();                                // 1800d551b
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;                 // 1800d5525
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;                 // 1800d5529
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;                 // 1800d552d
    *reinterpret_cast<int32_t*>(p + 0x38)  = 0;                 // 1800d5531  DWORD
    *reinterpret_cast<int32_t*>(p + 0x3c)  = -1;                // 1800d5534  DWORD
    *reinterpret_cast<uint64_t*>(p + 0x40) = 3;                 // 1800d553b  ONE QWORD
    *reinterpret_cast<uint64_t*>(p + 0x48) = 0;                 // 1800d5543
    *reinterpret_cast<uint64_t*>(p + 0x50) = 0;                 // 1800d5547
    *reinterpret_cast<uint64_t*>(p + 0x58) = 0;                 // 1800d554b
    *reinterpret_cast<uint64_t*>(p + 0x60) = 0;                 // 1800d554f
    *v = VtGroupDescription();                                  // 1800d5553
    *reinterpret_cast<uint64_t*>(p + 0x68) = 0;                 // 1800d555d
    *reinterpret_cast<uint64_t*>(p + 0x70) = 0;                 // 1800d5561
    *v = VtPageDescription();                                   // 1800d5565
    // ZERO, not -1. RadioDescription's identically-placed field is `-1` and
    // this one is `MOV dword [RCX+0x78],EDX` with EDX already zeroed.
    *reinterpret_cast<int32_t*>(p + 0x78)  = 0;                 // 1800d556f
    return p;                                                   // 1800d5572
}

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

uint32_t PageDescription::Type() const {                       // 0x180046ec0
    const uint8_t* ti = PageDescriptionTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::PageDescription` here.
const char* PageDescription::TypeName() const {                // 0x180046ee0
    const uint8_t* ti = PageDescriptionTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}
int32_t PageDescription::GetFileVersion() const { return 0x1104; }   // 0x1800d54a0

// 0x180047080: the null test is the DOOR's; then the out-of-line ctor (a door).
Object::External* PageDescription::Construct() {
    return static_cast<Object::External*>(RtUi_PageDescription_Ctor(this));
}
// 0x180047060: slot 0 with flag 0 (destroy WITHOUT freeing, through the
// object's own — minted — table), then the ctor again. No null test.
Object::External* PageDescription::Reconstruct() {
    DeletingDtor(0);
    return static_cast<Object::External*>(RtUi_PageDescription_Ctor(this));
}
void PageDescription::Destruct() { DeletingDtor(0); }          // 0x180047090

// ============================================================================
// ?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z   0x1800d5410
// ============================================================================
// GroupDescription::Serialize (NOT ElementDescription's — using the
// grandparent would drop every child), QUALIFIED (this object wears the
// leaf's table, whose slot 5 is this function), then one kScalarInt over
// `_activePage` through a SECOND ReaderWriter opened on the same stream.
bool PageDescription::Serialize(void* streamv) {
    rtdesc::Stream* stream = static_cast<rtdesc::Stream*>(streamv);

    GroupDescription::Serialize(stream);                       // 1800d542a

    rtdesc::Serializer ser = { stream, nullptr };
    void* rw = rtdesc::MakeReaderWriter(stream, &ser);         // 1800d545a

    rtdesc::RwScalar(rw, rtdesc::kRwScalarInt, &_activePage);

    rtdesc::DestroyReaderWriter(rw);                           // 1800d547d
    return true;
}

// ============================================================================
// ?CreateInstance@PageDescription@UI@@...   retail 0x1800d52b0   (slot 8)
// ============================================================================
// RCX = this, RDX = hidden return slot, R8 = Dialog* (RULE B6-2).
//
// PageElement is 0xc8 bytes (`MOV EDX,0xc8` at 0x1800d52f8); construction
// stamps GroupElement then PageElement and zeroes ONE dword at +0xc0.
//
// `ElementDescription::CreateInstance` (0x180115ad0), `??0Element` and
// `Dialog::CreateElement` are reached through their registered doors (their
// members are not defined in the real build yet — fragment pass / block B).
//
// THE TAIL CALLS SetActivePage TWICE, WITH -1 FIRST. The first call forces the
// field away from whatever the child loop left and hides every page; the second
// applies the description's own value. Collapsing them to one call with the
// final value would skip the hide pass whenever the field already equalled it,
// because SetActivePage's own first guard returns early on equality.
Object::InternalPtr<Element>* PageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    void* raw = rtdesc::MemAlloc(0xc8, 8);                     // 1800d52f8 (inlined)

    PageElement* elem = nullptr;
    if (raw != nullptr) {                                      // 1800d5323
        rtdesc::ElementCtor(raw);                              // 1800d5328
        *static_cast<void**>(raw) = AtVtGroupElement();        // 1800d532e
        *static_cast<void**>(raw) = AtVtPageElement();         // 1800d5338
        elem = reinterpret_cast<PageElement*>(raw);            // B.3.6 bridge
        elem->_activePage = 0;                                 // 1800d5344  DWORD
    }

    void* slot = elem;                                         // 1800d5350
    rtdesc::ElementDescriptionApply(this, &slot);              // 1800d535d

    const int32_t count = _children._size;
    for (int32_t i = 0; i < count; ++i) {                      // 1800d5362 / 1800d53c9
        void* desc = _children._data[i];                       // 1800d5377

        void** made = rtdesc::DialogCreateElement(dialog, &slot, desc);

        Element* child = *reinterpret_cast<Element**>(made);   // 1800d5388
        if (child != nullptr)
            child->_parent = elem;                             // 1800d5390

        using PushFn = void (*)(void* array, int n, const void* fill);
        rtdesc::RtAddr<PushFn>(0x180076900ull)(&elem->_children,
                                               elem->_children._size + 1,
                                               made);          // 1800d53a9

        for (Element* n = elem; n != nullptr; n = n->_parent)  // 1800d53b1..1800d53c5
            n->_flags |= kElementFlagNeedsSizeCalc;
    }

    CallSetActivePage(elem, -1);                               // 1800d53d4
    CallSetActivePage(elem, _activePage);                      // 1800d53df

    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(elem);   // 1800d53e4
    return ret;
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_PageDescription_Type(const void* self) {
    return reinterpret_cast<const UI::PageDescription*>(self)           // B.3.6 bridge
        ->UI::PageDescription::Type();
}
extern "C" const char* RtUi_PageDescription_TypeName(const void* self) {
    return reinterpret_cast<const UI::PageDescription*>(self)           // B.3.6 bridge
        ->UI::PageDescription::TypeName();
}
extern "C" int32_t RtUi_PageDescription_GetFileVersion(const void* self) {
    return reinterpret_cast<const UI::PageDescription*>(self)           // B.3.6 bridge
        ->UI::PageDescription::GetFileVersion();
}
extern "C" void* RtUi_PageDescription_Construct(void* selfv) {
    if (selfv == nullptr) return nullptr;                      // 180047080
    return reinterpret_cast<UI::PageDescription*>(selfv)                // B.3.6 bridge
        ->UI::PageDescription::Construct();
}
extern "C" void* RtUi_PageDescription_Reconstruct(void* selfv) {
    return reinterpret_cast<UI::PageDescription*>(selfv)                // B.3.6 bridge
        ->UI::PageDescription::Reconstruct();
}
extern "C" void RtUi_PageDescription_Destruct(void* selfv) {
    reinterpret_cast<UI::PageDescription*>(selfv)                       // B.3.6 bridge
        ->UI::PageDescription::Destruct();
}
extern "C" bool RtUi_PageDescription_Serialize(void* selfv, void* streamv) {
    return reinterpret_cast<UI::PageDescription*>(selfv)                // B.3.6 bridge
        ->UI::PageDescription::Serialize(streamv);
}
extern "C" void** RtUi_PageDescription_CreateInstance(const void* selfv, void** ret,
                                                      const void* dialog) {
    return reinterpret_cast<void**>(
        reinterpret_cast<const UI::PageDescription*>(selfv)             // B.3.6 bridge
            ->UI::PageDescription::CreateInstance(
                reinterpret_cast<Object::InternalPtr<UI::Element>*>(ret),
                static_cast<const UI::Dialog*>(dialog)));
}

// ??1PageDescription@UI@@UEAA@XZ  0x1800d54b0 — a DTOR DOOR: restamp, then
// TAIL JMP to ~GroupDescription. Nothing of its own to release.
extern "C" void RtUi_PageDescription_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtPageDescription();    // 1800d54c2
    using DtorFn = void (*)(void*);
    RtAddr<DtorFn>(0x180117c00ull)(selfv);                     // 1800d54d0 JMP
}

int RtUiPageDescriptionRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@PageDescription@UI@@UEBAIXZ",
                          (void*)&RtUi_PageDescription_Type);            ++n;
    RegisterRuntimeAddr(0x180046ec0ull, (void*)&RtUi_PageDescription_Type);

    RegisterRuntimeExport("?TypeName@PageDescription@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_PageDescription_TypeName);        ++n;
    RegisterRuntimeAddr(0x180046ee0ull, (void*)&RtUi_PageDescription_TypeName);

    RegisterRuntimeExport("?GetFileVersion@PageDescription@UI@@UEBAHXZ",
                          (void*)&RtUi_PageDescription_GetFileVersion);  ++n;
    RegisterRuntimeAddr(0x1800d54a0ull, (void*)&RtUi_PageDescription_GetFileVersion);

    RegisterRuntimeExport("?Construct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_PageDescription_Construct);       ++n;
    RegisterRuntimeAddr(0x180047080ull, (void*)&RtUi_PageDescription_Construct);

    RegisterRuntimeExport("?Reconstruct@PageDescription@UI@@UEAAPEAVExternal@Object@@XZ",
                          (void*)&RtUi_PageDescription_Reconstruct);     ++n;
    RegisterRuntimeAddr(0x180047060ull, (void*)&RtUi_PageDescription_Reconstruct);

    RegisterRuntimeExport("?Destruct@PageDescription@UI@@UEAAXXZ",
                          (void*)&RtUi_PageDescription_Destruct);        ++n;
    RegisterRuntimeAddr(0x180047090ull, (void*)&RtUi_PageDescription_Destruct);

    RegisterRuntimeExport("??0PageDescription@UI@@QEAA@XZ",
                          (void*)&RtUi_PageDescription_Ctor);            ++n;
    RegisterRuntimeAddr(0x1800d54e0ull, (void*)&RtUi_PageDescription_Ctor);

    RegisterRuntimeExport("??1PageDescription@UI@@UEAA@XZ",
                          (void*)&RtUi_PageDescription_Dtor);            ++n;
    RegisterRuntimeAddr(0x1800d54b0ull, (void*)&RtUi_PageDescription_Dtor);

    RegisterRuntimeExport("?Serialize@PageDescription@UI@@UEAA_NAEAVStream@IO@@@Z",
                          (void*)&RtUi_PageDescription_Serialize);       ++n;
    RegisterRuntimeAddr(0x1800d5410ull, (void*)&RtUi_PageDescription_Serialize);

    RegisterRuntimeExport(
        "?CreateInstance@PageDescription@UI@@UEBA?AV?$InternalPtr@VElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_PageDescription_CreateInstance);                    ++n;
    RegisterRuntimeAddr(0x1800d52b0ull, (void*)&RtUi_PageDescription_CreateInstance);

    return n;
}
