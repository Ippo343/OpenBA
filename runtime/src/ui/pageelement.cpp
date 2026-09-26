// ============================================================================
// runtime/src/ui/pageelement.cpp — UI::PageElement
// ============================================================================
//
// `??_7PageElement@UI@@6B@` (retail 0x180157080). Only three slots are its
// own; 4/5/6 are `UI::GroupElement`'s bodies (0x180117f00 / 0x180118050 /
// 0x1801181c0) in groupelement.cpp, which the class simply does not override:
//
//   0  vector deleting dtor  0x180047540  -> deleting_dtor.cpp row
//   1  TypeName              0x180047270  here
//   2  Type                  0x180047250  here
//   4..6                     GroupElement's
//
// plus ??0PageElement (0x1800d5260) and ??1PageElement (0x1800d5220), both
// DOORS (B4b-2). `GetActivePage` is inline in PageElement.h; `SetActivePage`
// is widgets_layout.cpp's door (fragment pass).
//
// THE DOUBLE VTABLE STAMP IS REAL AND IS REPRODUCED. Both the ctor and the
// dtor write `??_7GroupElement@UI@@6B@` and `??_7PageElement@UI@@6B@` into
// +0x00 back to back (ctor: Group then Page; dtor: Page then Group). That is
// MSVC emitting the base's stamp and the derived's over it.
//
// LAYOUT: runtime/types/ui/PageElement.h — sizeof 0xC8, `_activePage` at +0xC0.
//
// ENDGAME B.3.6 block C2: slots 1/2 are MEMBERS of `UI::PageElement`; the
// registered doors are one-line QUALIFIED delegations.
// ============================================================================

#include <atomic>

#include "widget_common.h"
#include "../../types/ui/PageElement.h"

using namespace rtui;

namespace {

// RULE B6-5: a namespace-scope atomic cache for the member bodies.
std::atomic<void*> g_pageElementTypeInfo{nullptr};
const uint8_t* PageElementTypeInfo() {
    void* p = g_pageElementTypeInfo.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached("?_typeInfo@PageElement@UI@@0VBaseTypeInfo@Object@@A", &scratch);
        if (p != nullptr) g_pageElementTypeInfo.store(p, std::memory_order_release);
    }
    return static_cast<const uint8_t*>(p);
}

// Door-only.
void* Vtable()      { return RT_DATA_SLOT(void*, "??_7PageElement@UI@@6B@"); }
void* VtableGroup() { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }

}  // namespace

namespace UI {

uint32_t PageElement::Type() const {                            // 0x180047250
    const uint8_t* ti = PageElementTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::PageElement` here.
const char* PageElement::TypeName() const {                     // 0x180047270
    const uint8_t* ti = PageElementTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

}  // namespace UI

extern "C" uint32_t RtUi_PageElement_Type(const void* self) {
    return reinterpret_cast<const UI::PageElement*>(self)->UI::PageElement::Type();       // B.3.6 bridge
}
extern "C" const char* RtUi_PageElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::PageElement*>(self)->UI::PageElement::TypeName();   // B.3.6 bridge
}

// ??0PageElement@UI@@QEAA@XZ   0x1800d5260   — a CTOR DOOR (B4b-2)
extern "C" void* RtUi_PageElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtableGroup();
    PTR(self, 0x00) = Vtable();
    I32(self, UI::PageElementOffsets::kActivePage) = 0;
    return self;
}

// ??1PageElement@UI@@UEAA@XZ   0x1800d5220   — a DTOR DOOR (B4b-2)
extern "C" void RtUi_PageElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    PTR(self, 0x00) = VtableGroup();
    ElementDtor(self);
}

// ============================================================================
int RtUiPageElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@PageElement@UI@@UEBAIXZ",
                          (void*)&RtUi_PageElement_Type);        ++n;
    RegisterRuntimeAddr(0x180047250ull, (void*)&RtUi_PageElement_Type);

    RegisterRuntimeExport("?TypeName@PageElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_PageElement_TypeName);    ++n;
    RegisterRuntimeAddr(0x180047270ull, (void*)&RtUi_PageElement_TypeName);

    RegisterRuntimeExport("??0PageElement@UI@@QEAA@XZ",
                          (void*)&RtUi_PageElement_Ctor);        ++n;
    RegisterRuntimeAddr(0x1800d5260ull, (void*)&RtUi_PageElement_Ctor);

    RegisterRuntimeExport("??1PageElement@UI@@UEAA@XZ",
                          (void*)&RtUi_PageElement_Dtor);        ++n;
    RegisterRuntimeAddr(0x1800d5220ull, (void*)&RtUi_PageElement_Dtor);

    return n;
}
