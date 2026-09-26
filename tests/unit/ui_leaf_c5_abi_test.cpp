// ============================================================================
// tests/unit/ui_leaf_c5_abi_test.cpp — ENDGAME B.3.6 block C5
//   the seven leaf pairs as real C++ classes:
//   DocumentText, Graph, Progress, Ribbon, Slider, Spinner, Tab
//   (`UI::XDescription : UI::ElementDescription`, 10 slots;
//    `UI::XElement : UI::Element`, 12 slots)
// ============================================================================
//
// PART A — LAYOUT. Retail `sizeof` (the `MOV EDX,imm` in each class's slot-0
//   vector deleting destructor, B36_SLOTS.md section 1), `alignof`, abstract,
//   no virtual destructor, derived fields starting at sizeof(base).
//
// PART B — SLOT ORDER. A probe subclass of each leaf overrides only slot 0;
//   every slot the leaf OWNS is raw-called at its retail byte offset and must
//   land in that leaf's member (the markers below); every slot the leaf
//   INHERITS must carry the very pointer a bare-base probe carries. That pins
//   `Construct` at slot 9 BEHIND `CreateInstance` (RULE B6-1.1), `Compile`
//   inherited at slot 4 (B6-1.5), and which Elements override `Update`.
//
// PART C — the two class-valued slots, raw-called the RETAIL way: every
//   Description's slot 8 `CreateInstance` takes the hidden return slot in RDX
//   and the Dialog in R8 and hands the slot back in RAX (RULE B6-2); the
//   Elements that override slot 7 `Update` receive the ADDRESS of the caller's
//   `InternalPtr<DialogInstance>` temporary (Element.h's banner).
//
// WHAT IS NOT PROVEN HERE: the ported bodies (runtime/src/ui/*.cpp do not link
// into the unit build) — those are verified by the real run. The members are
// defined below as MARKERS.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/Element.h"
#include "runtime/types/ui/ElementDescription.h"
#include "runtime/types/ui/GraphDescription.h"
#include "runtime/types/ui/GraphElement.h"
#include "runtime/types/ui/ProgressDescription.h"
#include "runtime/types/ui/ProgressElement.h"
#include "runtime/types/ui/SpinnerDescription.h"
#include "runtime/types/ui/SpinnerElement.h"
#include "runtime/types/ui/SliderDescription.h"
#include "runtime/types/ui/SliderElement.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

namespace Object {
namespace detail {
extern InternalFactory* g_unitInternalFactory;
extern uint32_t (*g_unitInternalRandHook)(::Random*);
}  // namespace detail
}  // namespace Object

namespace {

// ---------------------------------------------------------------------------
// Observation channel: which class's member, which slot, on which object.
// ---------------------------------------------------------------------------
enum Cls : int {
    kNone = 0,
    kGraphD, kGraphE,
    kProgressD, kProgressE,
    kSpinnerD, kSpinnerE,
    kSliderD, kSliderE,
};
const void* g_self = nullptr;
int         g_cls  = kNone;
int         g_slot = -1;
long long   g_arg  = 0;

void Hit(const void* self, int cls, int slot, long long arg = 0) {
    g_self = self; g_cls = cls; g_slot = slot; g_arg = arg;
}
void Reset() { g_self = nullptr; g_cls = kNone; g_slot = -1; g_arg = 0; }

template <typename Fn>
Fn SlotAt(const void* obj, int index) {
    const void* const* vt = *reinterpret_cast<const void* const* const*>(obj);
    Fn f = nullptr;
    const void* raw = vt[index];
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(const void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

uint32_t RandHook(::Random*) { return 7u; }
struct FactoryScope {
    ::Object::InternalFactory  factory;
    ::Object::InternalFactory* saved;
    uint32_t (*savedRand)(::Random*);
    FactoryScope()
        : factory(),
          saved(::Object::detail::g_unitInternalFactory),
          savedRand(::Object::detail::g_unitInternalRandHook) {
        ::Object::detail::g_unitInternalFactory  = &factory;
        ::Object::detail::g_unitInternalRandHook = &RandHook;
    }
    ~FactoryScope() {
        ::Object::detail::g_unitInternalFactory  = saved;
        ::Object::detail::g_unitInternalRandHook = savedRand;
    }
};

const void* const kFakeElement = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xE1E1E1E1));
const void* const kFakeDialog  = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xD1A1));
const void* const kFakeHandle  = reinterpret_cast<const void*>(static_cast<uintptr_t>(0x7000));

// ---------------------------------------------------------------------------
// Bare-base probes: their inherited slots are the BASE members' pointers.
// ---------------------------------------------------------------------------
struct BaseElementProbe final : UI::Element {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    void ComputeSize() override {}
};
struct BaseDescriptionProbe final : UI::ElementDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Object::InternalPtr<UI::Element>* CreateInstance(
            Object::InternalPtr<UI::Element>* ret, const UI::Dialog*) const override {
        return ret;
    }
};

// Raw-call one Element slot with retail-shaped arguments.
void CallElementSlot(void* raw, int slot) {
    switch (slot) {
    case 1:  SlotAt<const char* (*)(const void*)>(raw, 1)(raw); break;
    case 2:  SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw); break;
    case 3:  SlotAt<void (*)(void*, void*)>(raw, 3)(raw, nullptr); break;
    case 4:  SlotAt<void (*)(void*)>(raw, 4)(raw); break;
    case 5:  SlotAt<void (*)(void*, int, int, int, int)>(raw, 5)(raw, 7, 0, 0, 9); break;
    case 6:  SlotAt<void (*)(const void*, void*, uint8_t)>(raw, 6)(raw, nullptr, 200); break;
    case 7: {
        // Retail passes the ADDRESS of the caller's handle temporary in R9.
        const void* handle = kFakeHandle;
        UI::Element::UpdateState state;
        state.mouseX = 5;
        SlotAt<void (*)(void*, const void*, void*, const void**, const void*)>(raw, 7)(
            raw, nullptr, nullptr, &handle, &state);
        break;
    }
    case 8:  SlotAt<void (*)(void*, bool)>(raw, 8)(raw, true); break;
    case 9:  SlotAt<bool (*)(const void*, int, int, void*)>(raw, 9)(raw, 3, 4, nullptr); break;
    case 10: SlotAt<void (*)(void*, uint32_t)>(raw, 10)(raw, 0x11223344u); break;
    case 11: { void* s = nullptr; SlotAt<void** (*)(const void*, void**)>(raw, 11)(raw, &s); break; }
    default: break;
    }
}
void CallDescriptionSlot(void* raw, int slot) {
    switch (slot) {
    case 1: SlotAt<const char* (*)(const void*)>(raw, 1)(raw); break;
    case 2: SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw); break;
    case 3: SlotAt<int32_t (*)(const void*)>(raw, 3)(raw); break;
    case 5: SlotAt<bool (*)(void*, void*)>(raw, 5)(raw, nullptr); break;
    case 6: SlotAt<void* (*)(void*)>(raw, 6)(raw); break;
    case 7: SlotAt<void (*)(void*)>(raw, 7)(raw); break;
    case 8: { void* s = nullptr;
              SlotAt<void** (*)(const void*, void**, const void*)>(raw, 8)(raw, &s, kFakeDialog);
              break; }
    case 9: SlotAt<void* (*)(void*)>(raw, 9)(raw); break;
    default: break;
    }
}

// Element: slots in `ownMask` must hit (cls, slot); every other slot 1..11 must
// equal the bare Element probe's entry.
void CheckElementSlots(void* raw, int cls, unsigned ownMask) {
    FactoryScope scope;
    BaseElementProbe base{};
    const void* const* vb = VTableOf(&base);
    const void* const* vl = VTableOf(raw);
    for (int s = 1; s <= 11; ++s) {
        if (ownMask & (1u << s)) {
            Reset(); CallElementSlot(raw, s);
            CHECK_EQ(g_cls, cls); CHECK_EQ(g_slot, s); CHECK(g_self == raw);
            CHECK(vl[s] != vb[s]);
        } else {
            CHECK(vl[s] == vb[s]);
        }
    }
    CHECK(vl[4] != vb[4]);   // every leaf overrides (or inherits an override of) ComputeSize
}
// Description: 1,2,3,5,6,7,8,9 are the leaf's own; 4 is External::Compile.
void CheckDescriptionSlots(void* raw, int cls) {
    BaseDescriptionProbe base;
    const void* const* vb = VTableOf(&base);
    const void* const* vl = VTableOf(raw);
    const int own[] = {1, 2, 3, 5, 6, 7, 8, 9};
    for (int s : own) {
        Reset(); CallDescriptionSlot(raw, s);
        CHECK_EQ(g_cls, cls); CHECK_EQ(g_slot, s); CHECK(g_self == raw);
    }
    CHECK(vl[4] == vb[4]);   // Compile inherited (RULE B6-1.5)
    // Slot 8 is CreateInstance and 9 is Construct, not the other way round.
    Reset(); CallDescriptionSlot(raw, 8); CHECK_EQ(g_slot, 8); CHECK_EQ(g_arg, 0xD1A1);
    Reset(); CallDescriptionSlot(raw, 9); CHECK_EQ(g_slot, 9);
}
// RULE B6-2: raw-call slot 8 the retail way and check the slot comes back.
void CheckCreateInstanceRetslot(void* raw) {
    const void* const* vtBefore = VTableOf(raw);
    void* slot = nullptr;
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(raw, 8)(
        raw, &slot, kFakeDialog);
    CHECK(back == &slot);
    CHECK(slot == kFakeElement);
    CHECK(VTableOf(raw) == vtBefore);
}
// Slot 7 on a leaf that overrides Update: the member sees the handle THROUGH
// the pointer (the marker records the handle it read, plus mouseX).
void CheckUpdateTakesHandleAddress(void* raw, int cls) {
    FactoryScope scope;
    Reset(); CallElementSlot(raw, 7);
    CHECK_EQ(g_cls, cls);
    CHECK_EQ(g_arg, 0x7000 + 5);
}

// Marker helpers for the member definitions below.
Object::InternalPtr<UI::Element>* FillRet(Object::InternalPtr<UI::Element>* ret) {
    new (static_cast<void*>(ret)) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(const_cast<void*>(kFakeElement)));
    return ret;
}
long long UpdateArg(Object::InternalPtr<UI::DialogInstance>* dialog,
                    const UI::Element::UpdateState& state) {
    return static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX;
}

}  // namespace

// ===========================================================================
// The members, AS MARKERS (the real bodies are in runtime/src/ui/, which does
// not link here). One macro per shape so every class gets the same set.
// ===========================================================================
#define C5_DESCRIPTION_MARKERS(C, K)                                                         \
    const char* UI::C::TypeName() const { Hit(this, K, 1); return #C; }                     \
    uint32_t UI::C::Type() const { Hit(this, K, 2); return 0; }                              \
    int32_t UI::C::GetFileVersion() const { Hit(this, K, 3); return 0; }                     \
    bool UI::C::Serialize(void*) { Hit(this, K, 5); return true; }                           \
    Object::External* UI::C::Reconstruct() { Hit(this, K, 6); return this; }                 \
    void UI::C::Destruct() { Hit(this, K, 7); }                                              \
    Object::InternalPtr<UI::Element>* UI::C::CreateInstance(                                 \
            Object::InternalPtr<UI::Element>* ret, const UI::Dialog* d) const {              \
        Hit(this, K, 8, reinterpret_cast<intptr_t>(d)); return FillRet(ret);                 \
    }                                                                                        \
    Object::External* UI::C::Construct() { Hit(this, K, 9); return this; }                   \
    UI::C::~C() {}

#define C5_ELEMENT_MARKERS(C, K)                                                             \
    const char* UI::C::TypeName() const { Hit(this, K, 1); return #C; }                     \
    uint32_t UI::C::Type() const { Hit(this, K, 2); return 0; }                              \
    void UI::C::ComputeSize() { Hit(this, K, 4); }                                           \
    void UI::C::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {                      \
        Hit(this, K, 5, (long long)x * 1000 + h);                                            \
    }                                                                                        \
    void UI::C::Draw(UI::SpriteContext&, uint8_t a) const { Hit(this, K, 6, a); }            \
    UI::C::~C() {}

#define C5_ELEMENT_UPDATE_MARKER(C, K)                                                       \
    void UI::C::Update(const System::Time&, UI::UserInterface*,                              \
                       Object::InternalPtr<UI::DialogInstance>* dialog,                      \
                       const UpdateState& state) {                                           \
        Hit(this, K, 7, UpdateArg(dialog, state));                                           \
    }

C5_DESCRIPTION_MARKERS(GraphDescription, kGraphD)
C5_ELEMENT_MARKERS(GraphElement, kGraphE)

namespace {
struct GraphDescriptionProbe final : UI::GraphDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct GraphElementProbe final : UI::GraphElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};

// Slot-ownership masks for the Elements (bit n = slot n is the leaf's own).
constexpr unsigned kOwn456  = (1u << 1) | (1u << 2) | (1u << 4) | (1u << 5) | (1u << 6);
constexpr unsigned kOwn4567 = kOwn456 | (1u << 7);
}  // namespace

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
template <class C, class Base>
static void CheckShape(size_t size) {
    CHECK_EQ(sizeof(C), size);
    CHECK_EQ(alignof(C), (size_t)8);
    CHECK(std::is_abstract<C>::value);
    CHECK(!std::has_virtual_destructor<C>::value);
    CHECK((std::is_base_of<Base, C>::value));
    CHECK(!std::is_default_constructible<C>::value);   // protected ctor: never built as C++
}

UNIT_TEST(ui_c5_graph_layout) {
    CheckShape<UI::GraphDescription, UI::ElementDescription>(0x88);   // 0x1800403a0
    CheckShape<UI::GraphElement, UI::Element>(0x100);                 // 0x1800408b0
    CHECK_EQ(offsetof(UI::GraphDescription, _width), sizeof(UI::ElementDescription));
    CHECK_EQ(offsetof(UI::GraphDescription, _spriteName), (size_t)0x80);
    CHECK_EQ(offsetof(UI::GraphElement, _width), sizeof(UI::Element));
    CHECK_EQ(offsetof(UI::GraphElement, _lineSprite), (size_t)0xd0);
    CHECK_EQ(offsetof(UI::GraphElement, _yRange), (size_t)0xe0);
    CHECK_EQ(offsetof(UI::GraphElement, _seriesData), (size_t)0xf0);
}

// ===========================================================================
// PART B / C
// ===========================================================================
UNIT_TEST(ui_c5_graph_slots) {
    GraphDescriptionProbe d;
    CheckDescriptionSlots(&d, kGraphD);
    CheckCreateInstanceRetslot(&d);

    FactoryScope scope;
    GraphElementProbe e{};
    CheckElementSlots(&e, kGraphE, kOwn456);        // Update inherited from Element

    // The three one-instruction setters (0x1800405e0 / 0x1800405f0 /
    // 0x180040600): SetYDivisions writes +0xe4, like SetXDivisions (retail).
    e._yDivisions = 4;
    e.SetYRange(2.5f);  CHECK(e._yRange == 2.5f);
    e.SetXDivisions(9); CHECK_EQ(e._xDivisions, 9);
    e.SetYDivisions(3); CHECK_EQ(e._xDivisions, 3); CHECK_EQ(e._yDivisions, 4);
}

// ===========================================================================
// Progress
// ===========================================================================
C5_DESCRIPTION_MARKERS(ProgressDescription, kProgressD)
C5_ELEMENT_MARKERS(ProgressElement, kProgressE)

namespace {
struct ProgressDescriptionProbe final : UI::ProgressDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ProgressElementProbe final : UI::ProgressElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
}  // namespace

UNIT_TEST(ui_c5_progress_layout) {
    CheckShape<UI::ProgressDescription, UI::ElementDescription>(0xb0);   // 0x180077c80
    CheckShape<UI::ProgressElement, UI::Element>(0x130);                 // 0x180078150
    CHECK_EQ(offsetof(UI::ProgressDescription, _unknown68), sizeof(UI::ElementDescription));
    CHECK_EQ(offsetof(UI::ProgressDescription, _spriteSheet), (size_t)0x70);
    CHECK_EQ(offsetof(UI::ProgressDescription, _childDescription), (size_t)0xa8);
    CHECK_EQ(offsetof(UI::ProgressElement, _fillLeft), sizeof(UI::Element));
    CHECK_EQ(offsetof(UI::ProgressElement, _backgroundColor), (size_t)0x120);
    CHECK_EQ(sizeof(UI::ProgressElement), sizeof(RtUI::ProgressElement));
}

UNIT_TEST(ui_c5_progress_slots) {
    ProgressDescriptionProbe d;
    CheckDescriptionSlots(&d, kProgressD);
    CheckCreateInstanceRetslot(&d);

    FactoryScope scope;
    ProgressElementProbe e{};
    CheckElementSlots(&e, kProgressE, kOwn456);   // Update inherited from Element
    e.SetMaximumValue(2.0f);        CHECK(e._maxValue == 2.0f);        // 0x180077ec0
    e.SetCurrentValue(0.5f);        CHECK(e._currentValue == 0.5f);    // 0x180077ed0
    e.SetBackgroundColor(0xAABBCCDDu); CHECK_EQ(e._backgroundColor, 0xAABBCCDDu);  // 0x180077ee0
}

// ===========================================================================
// Spinner — the Element OVERRIDES slot 7 Update
// ===========================================================================
C5_DESCRIPTION_MARKERS(SpinnerDescription, kSpinnerD)
C5_ELEMENT_MARKERS(SpinnerElement, kSpinnerE)
C5_ELEMENT_UPDATE_MARKER(SpinnerElement, kSpinnerE)

namespace {
struct SpinnerDescriptionProbe final : UI::SpinnerDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct SpinnerElementProbe final : UI::SpinnerElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
}  // namespace

UNIT_TEST(ui_c5_spinner_layout) {
    CheckShape<UI::SpinnerDescription, UI::ElementDescription>(0x88);   // 0x18004b050
    CheckShape<UI::SpinnerElement, UI::Element>(0xc8);                  // 0x18004b3b0
    CHECK_EQ(offsetof(UI::SpinnerDescription, _editDescription), sizeof(UI::ElementDescription));
    CHECK_EQ(offsetof(UI::SpinnerDescription, _increment), (size_t)0x80);
    CHECK_EQ(offsetof(UI::SpinnerElement, _increment), sizeof(UI::Element));
}

UNIT_TEST(ui_c5_spinner_slots) {
    SpinnerDescriptionProbe d;
    CheckDescriptionSlots(&d, kSpinnerD);
    CheckCreateInstanceRetslot(&d);

    FactoryScope scope;
    SpinnerElementProbe e{};
    CheckElementSlots(&e, kSpinnerE, kOwn4567);    // Update overridden
    CheckUpdateTakesHandleAddress(&e, kSpinnerE);
    e.SetIncrement(5); CHECK_EQ(e._increment, 5);   // 0x1800d7d30
}

// ===========================================================================
// Slider — the Element OVERRIDES slot 7 Update
// ===========================================================================
C5_DESCRIPTION_MARKERS(SliderDescription, kSliderD)
C5_ELEMENT_MARKERS(SliderElement, kSliderE)
C5_ELEMENT_UPDATE_MARKER(SliderElement, kSliderE)

namespace {
struct SliderDescriptionProbe final : UI::SliderDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct SliderElementProbe final : UI::SliderElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
}  // namespace

UNIT_TEST(ui_c5_slider_layout) {
    CheckShape<UI::SliderDescription, UI::ElementDescription>(0xb8);   // 0x180049c60
    CheckShape<UI::SliderElement, UI::Element>(0x130);                 // 0x18004a180
    CHECK_EQ(offsetof(UI::SliderDescription, _childDescription), sizeof(UI::ElementDescription));
    CHECK_EQ(offsetof(UI::SliderDescription, _steps), (size_t)0xb0);
    CHECK_EQ(offsetof(UI::SliderElement, _trackStart), sizeof(UI::Element));
    CHECK_EQ(offsetof(UI::SliderElement, _value), (size_t)UI::SliderElementOffsets::kValue);
}

UNIT_TEST(ui_c5_slider_slots) {
    SliderDescriptionProbe d;
    CheckDescriptionSlots(&d, kSliderD);
    CheckCreateInstanceRetslot(&d);

    FactoryScope scope;
    SliderElementProbe e{};
    CheckElementSlots(&e, kSliderE, kOwn4567);     // Update overridden
    CheckUpdateTakesHandleAddress(&e, kSliderE);
    e.SetMinimumValue(-1.0f); CHECK(e._minimum == -1.0f);   // 0x180049ea0
    e.SetMaximumValue(3.0f);  CHECK(e._maximum == 3.0f);    // 0x180049eb0
    e.SetCurrentValue(0.5f);  CHECK(e._value == 0.5f);      // 0x180049ec0
    CHECK(e.GetCurrentValue() == 0.5f);                     // 0x180049ed0
}
