// ============================================================================
// runtime/src/ui/pageelement.cpp — UI::PageElement
// ============================================================================
//
// `??_7PageElement@UI@@6B@` (retail 0x180157080). Only four slots are its own;
// 4/5/6 are `UI::GroupElement`'s bodies (0x180117f00 / 0x180118050 /
// 0x1801181c0), which belong to the elem_A batch and are not duplicated here:
//
//   0  vector deleting dtor  0x180047540  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180047270  here
//   2  Type                  0x180047250  here
//   4..6                     GroupElement's, elem_A
//
// plus ??0PageElement (0x1800d5260) and ??1PageElement (0x1800d5220).
//
// THE DOUBLE VTABLE STAMP IS REAL AND IS REPRODUCED. Both the ctor and the
// dtor write `??_7GroupElement@UI@@6B@` and `??_7PageElement@UI@@6B@` into
// +0x00 back to back (ctor: Group then Page; dtor: Page then Group). That is
// MSVC emitting the base's stamp and the derived's over it, four instructions
// in the shipped bytes; collapsing them would be a divergence for no gain,
// exactly as runtime/src/ui/dialog_ctor_dtor.cpp keeps Dialog's two.
//
// sizeof == 0xC8 (the deleting destructor's `MOV EDX,0xc8`): UI::Element's
// 0xC0 plus one int32 `_activePage` at +0xC0, which is what
// runtime/types/ui/PageElement.h already records from ?GetActivePage@
// (0x1800d5100).
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/PageElement.h"

using namespace rtui;

namespace {

constexpr int kActivePage = 0xC0;
static_assert(kActivePage == UI::PageElementOffsets::kActivePage, "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@PageElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable()      { return RT_DATA_SLOT(void*, "??_7PageElement@UI@@6B@"); }
void* VtableGroup() { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }

}  // namespace

extern "C" uint32_t RtUi_PageElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_PageElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0PageElement@UI@@QEAA@XZ   0x1800d5260
extern "C" void* RtUi_PageElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtableGroup();
    PTR(self, 0x00) = Vtable();
    I32(self, kActivePage) = 0;
    return self;
}

// ??1PageElement@UI@@UEAA@XZ   0x1800d5220
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
