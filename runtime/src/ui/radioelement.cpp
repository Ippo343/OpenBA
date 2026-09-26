// ============================================================================
// runtime/src/ui/radioelement.cpp — UI::RadioElement
// ============================================================================
//
// `??_7RadioElement@UI@@6B@` (retail 0x1801573c0). Slots 4/5/6 are
// `UI::GroupElement`'s (groupelement.cpp), which the class does not override:
//
//   0  vector deleting dtor  0x180047d50  -> deleting_dtor.cpp row
//   1  TypeName              0x180047a00  here
//   2  Type                  0x1800479e0  here
//   4..6                     GroupElement's
//   7  Update                0x1800d5580  here
//
// plus ??0RadioElement (0x1800d5870) and ??1RadioElement (0x1800d5820), both
// DOORS (B4b-2).
//
// NOT HERE: the two `?FindCheckBoxes@RadioElement@UI@@` (public 0x180047c00,
// private 0x1800d5750) and `?SetActiveRadio@` (0x1800d56f0) — widgets_input.cpp
// doors (fragment pass); declared on the class.
//
// LAYOUT: runtime/types/ui/RadioElement.h — `_checkBoxes` (+0xC0),
// `_activeIndex` (+0xD0, ctor -1); sizeof 0xD8.
//
// ENDGAME B.3.6 block C2: slots 1/2/7 are MEMBERS of `UI::RadioElement`; the
// registered doors are one-line QUALIFIED delegations. Update's base call (the
// by-address `ElementUpdate` door before) is the qualified member
// `GroupElement::Update`, i.e. `Element::Update` (0x180116270).
// ============================================================================

#include <atomic>
#include <cstring>

#include "widget_common.h"
#include "../../types/ui/RadioElement.h"

using namespace rtui;

namespace {

constexpr int kCheckBoxes  = 0xC0;
constexpr int kActiveIndex = 0xD0;
static_assert(kCheckBoxes  == offsetof(UI::RadioElement, _checkBoxes), "");
static_assert(kActiveIndex == offsetof(UI::RadioElement, _activeIndex), "");

constexpr int kCheckBoxChecked = 0xD8;   // CheckElement's own "is checked" byte

// UI::UserInterface's pending-event Array<Event>, stride 0x30
// (runtime/src/collection/array.cpp's kSpec_Event30 names the same stride for
// the same append at 0x180033690). UserInterface is block E's class.
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

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

// RULE B6-5: a namespace-scope atomic cache for the member bodies.
std::atomic<void*> g_radioElementTypeInfo{nullptr};
const uint8_t* RadioElementTypeInfo() {
    void* p = g_radioElementTypeInfo.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached("?_typeInfo@RadioElement@UI@@0VBaseTypeInfo@Object@@A", &scratch);
        if (p != nullptr) g_radioElementTypeInfo.store(p, std::memory_order_release);
    }
    return static_cast<const uint8_t*>(p);
}

// Door-only.
void* Vtable()      { return RT_DATA_SLOT(void*, "??_7RadioElement@UI@@6B@"); }
void* VtableGroup() { return RT_DATA_SLOT(void*, "??_7GroupElement@UI@@6B@"); }

}  // namespace

namespace UI {

uint32_t RadioElement::Type() const {                           // 0x1800479e0
    const uint8_t* ti = RadioElementTypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
// The key function: GCC emits `vtable for UI::RadioElement` here.
const char* RadioElement::TypeName() const {                    // 0x180047a00
    const uint8_t* ti = RadioElementTypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@RadioElement@UI@@ ...   0x1800d5580
//
// Poll the checkboxes; whichever one reports "was activated" this frame either
// becomes the selection (if it is now checked) or clears it (-1). On a change
// the other boxes are unchecked and ONE event of kind 0x16 is queued.
//
// The base call is QUALIFIED (this object wears the minted table, whose slot
// 7 is this very function). The handle is copied into a local and the local's
// ADDRESS forwarded, as retail's frame does (RULE B6-11).
// ---------------------------------------------------------------------------
void RadioElement::Update(const System::Time& time, UserInterface* ui,
                          Object::InternalPtr<DialogInstance>* dialog,
                          const UpdateState& state) {
    Object::InternalPtr<DialogInstance> slot = *dialog;
    GroupElement::Update(time, ui, &slot, state);   // Element::Update 0x180116270

    const int32_t count = _checkBoxes._size;
    int32_t active = _activeIndex;
    int32_t value  = -1;

    if (count > 0) {
        for (int32_t i = 0; i < count; ++i) {
            Element* box = _checkBoxes._data[i];
            if ((box->_flags & kElementFlagWasActivated) != 0) {
                active = -1;
                if (reinterpret_cast<const uint8_t*>(box)[kCheckBoxChecked] != 0) active = i;
            }
        }
    }

    if (_activeIndex == active) return;

    _activeIndex = active;

    // Uncheck everything that is not the new selection. The loop re-reads the
    // count from +0xC8 each iteration, as shipped.
    for (int32_t i = 0; i < _checkBoxes._size; ++i) {
        if (i != _activeIndex)
            reinterpret_cast<uint8_t*>(_checkBoxes._data[i])[kCheckBoxChecked] = 0;
    }

    if (_activeIndex >= 0)
        value = _checkBoxes._data[_activeIndex]->_elementId;

    WidgetEvent ev;
    std::memcpy(&ev.dialog, dialog, sizeof ev.dialog);   // the handle's raw 8 bytes
    ev.element   = this;
    ev.kind      = 0x16;
    ev.elementId = _elementId;
    ev.mouseX    = state.mouseX;
    ev.mouseY    = state.mouseY;
    ev.value     = value;
    ev.tail      = 0;
    RtAddr<ArraySetSizeFillFn>(0x180033690ull)(
        B(ui, kUiEvents), I32(ui, kUiEventCount) + 1, &ev);
}

}  // namespace UI

// ============================================================================
// The DOORS — each a one-line QUALIFIED delegation.
// ============================================================================
extern "C" uint32_t RtUi_RadioElement_Type(const void* self) {
    return reinterpret_cast<const UI::RadioElement*>(self)->UI::RadioElement::Type();       // B.3.6 bridge
}
extern "C" const char* RtUi_RadioElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::RadioElement*>(self)->UI::RadioElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_RadioElement_Update(void* self, const void* time, void* ui,
                                         void** dlgPtr, const void* state) {
    reinterpret_cast<UI::RadioElement*>(self)->UI::RadioElement::Update(                   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr),
        *static_cast<const UI::Element::UpdateState*>(state));
}

// ??0RadioElement@UI@@QEAA@XZ   0x1800d5870   — a CTOR DOOR (B4b-2)
extern "C" void* RtUi_RadioElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtableGroup();
    PTR(self, 0x00) = Vtable();
    PTR(self, kCheckBoxes)     = nullptr;
    PTR(self, kCheckBoxes + 8) = nullptr;   // size and capacity, one qword
    I32(self, kActiveIndex)    = -1;
    return self;
}

// ??1RadioElement@UI@@UEAA@XZ   0x1800d5820   — a DTOR DOOR (B4b-2)
extern "C" void RtUi_RadioElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ArrayDtorFn>(0x1800490e0ull)(B(self, kCheckBoxes));
    PTR(self, 0x00) = VtableGroup();
    ElementDtor(self);
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
