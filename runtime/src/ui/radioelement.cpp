// ============================================================================
// runtime/src/ui/radioelement.cpp — UI::RadioElement
// ============================================================================
//
// `??_7RadioElement@UI@@6B@` (retail 0x1801573c0). Slots 4/5/6 are
// `UI::GroupElement`'s (elem_A) and are not duplicated here:
//
//   0  vector deleting dtor  0x180047d50  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180047a00  here
//   2  Type                  0x1800479e0  here
//   4..6                     GroupElement's, elem_A
//   7  Update                0x1800d5580  here
//
// plus ??0RadioElement (0x1800d5870) and ??1RadioElement (0x1800d5820).
//
// NOT HERE: the two `?FindCheckBoxes@RadioElement@UI@@` (public 0x180047c00,
// private 0x1800d5750) — not vtable slots, and lane D already declined them;
// nothing in this file calls them.
//
// LAYOUT (runtime/types/ui/RadioElement.h, and the ctor confirms every field):
//   +0xC0  _checkBoxes  Array<UI::Element*>   (the ONE member ~RadioElement
//                                              destroys, via 0x1800490e0)
//   +0xD0  _activeIndex int32, ctor -1
// sizeof == 0xD8, from the deleting destructor's `MOV EDX,0xd8`.
//
// The per-checkbox byte at +0xD8 belongs to the CHECKBOX class, not to
// RadioElement — the same byte ?SetActiveRadio@ (0x1800d56f0) clears.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/RadioElement.h"

using namespace rtui;

namespace {

constexpr int kCheckBoxes  = 0xC0;
constexpr int kCheckCount  = 0xC8;
constexpr int kActiveIndex = 0xD0;

static_assert(kCheckBoxes  == offsetof(UI::RadioElement, _checkBoxes), "");
constexpr int kCheckBoxChecked = 0xD8;   // CheckElement's own "is checked" byte

// UI::UserInterface's pending-event Array<Event>, stride 0x30
// (runtime/src/collection/array.cpp's kSpec_Event30 names the same stride for
// the same append at 0x180033690).
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

// UpdateState
constexpr int kUsMouseX = 0x18;
constexpr int kUsMouseY = 0x1c;

// The 0x30-byte UI event record the widgets append.
struct WidgetEvent {
    void*    dialog;     // +0x00  InternalPtr<DialogInstance>, copied from the arg
    void*    element;    // +0x08  InternalPtr<Element>, `this`
    int32_t  kind;       // +0x10
    int32_t  elementId;  // +0x14
    int32_t  mouseX;     // +0x18
    int32_t  mouseY;     // +0x1c
    int32_t  value;      // +0x20
    int32_t  pad24;      // +0x24  never written
    int64_t  tail;       // +0x28  zeroed
};
static_assert(sizeof(WidgetEvent) == 0x30, "UI event record is 0x30 bytes");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@RadioElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable()      { return RT_DATA_SLOT(void*, "??_7RadioElement@UI@@6B@"); }
void* VtableGroup() { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }

}  // namespace

extern "C" uint32_t RtUi_RadioElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_RadioElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0RadioElement@UI@@QEAA@XZ   0x1800d5870
extern "C" void* RtUi_RadioElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtableGroup();
    PTR(self, 0x00) = Vtable();
    PTR(self, kCheckBoxes)     = nullptr;
    PTR(self, kCheckBoxes + 8) = nullptr;   // size and capacity, one qword
    I32(self, kActiveIndex)    = -1;
    return self;
}

// ??1RadioElement@UI@@UEAA@XZ   0x1800d5820
extern "C" void RtUi_RadioElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ArrayDtorFn>(0x1800490e0ull)(B(self, kCheckBoxes));
    PTR(self, 0x00) = VtableGroup();
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@RadioElement@UI@@ ...   0x1800d5580
//
// Poll the checkboxes; whichever one reports "was activated" this frame either
// becomes the selection (if it is now checked) or clears it (-1). On a change
// the other boxes are unchecked and ONE event of kind 0x16 is queued.
// ---------------------------------------------------------------------------
extern "C" void RtUi_RadioElement_Update(void* self, const void* time, void* ui,
                                         void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;
    ElementUpdate(self, time, ui, &slot, state);

    const int32_t count = I32(self, kCheckCount);
    int32_t active = I32(self, kActiveIndex);
    int32_t value  = -1;

    if (count > 0) {
        void** boxes = reinterpret_cast<void**>(PTR(self, kCheckBoxes));
        for (int32_t i = 0; i < count; ++i) {
            if ((U32(boxes[i], kElFlags) & kFlagWasActivated) != 0) {
                active = -1;
                if (U8(boxes[i], kCheckBoxChecked) != 0) active = i;
            }
        }
    }

    if (I32(self, kActiveIndex) == active) return;

    I32(self, kActiveIndex) = active;

    // Uncheck everything that is not the new selection. The loop re-reads the
    // count from +0xC8 each iteration, as shipped.
    for (int32_t i = 0; i < I32(self, kCheckCount); ++i) {
        if (i != I32(self, kActiveIndex))
            U8(reinterpret_cast<void**>(PTR(self, kCheckBoxes))[i], kCheckBoxChecked) = 0;
    }

    if (I32(self, kActiveIndex) >= 0) {
        void* box = reinterpret_cast<void**>(PTR(self, kCheckBoxes))[I32(self, kActiveIndex)];
        value = I32(box, kElElementId);
    }

    WidgetEvent ev;
    ev.dialog    = *dlgPtr;
    ev.element   = self;
    ev.kind      = 0x16;
    ev.elementId = I32(self, kElElementId);
    ev.mouseX    = I32c(state, kUsMouseX);
    ev.mouseY    = I32c(state, kUsMouseY);
    ev.value     = value;
    ev.tail      = 0;
    RtAddr<ArraySetSizeFillFn>(0x180033690ull)(
        B(ui, kUiEvents), I32(ui, kUiEventCount) + 1, &ev);
}

// ============================================================================
int RtUiRadioElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RadioElement@UI@@UEBAIXZ",
                          (void*)&RtUi_RadioElement_Type);        ++n;
    RegisterRuntimeAddr(0x1800479e0ull, (void*)&RtUi_RadioElement_Type);

    RegisterRuntimeExport("?TypeName@RadioElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_RadioElement_TypeName);    ++n;
    RegisterRuntimeAddr(0x180047a00ull, (void*)&RtUi_RadioElement_TypeName);

    RegisterRuntimeExport("??0RadioElement@UI@@QEAA@XZ",
                          (void*)&RtUi_RadioElement_Ctor);        ++n;
    RegisterRuntimeAddr(0x1800d5870ull, (void*)&RtUi_RadioElement_Ctor);

    RegisterRuntimeExport("??1RadioElement@UI@@UEAA@XZ",
                          (void*)&RtUi_RadioElement_Dtor);        ++n;
    RegisterRuntimeAddr(0x1800d5820ull, (void*)&RtUi_RadioElement_Dtor);

    RegisterRuntimeExport(
        "?Update@RadioElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_RadioElement_Update);                        ++n;
    RegisterRuntimeAddr(0x1800d5580ull, (void*)&RtUi_RadioElement_Update);

    return n;
}
