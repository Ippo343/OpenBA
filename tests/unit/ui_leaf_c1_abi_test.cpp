// ============================================================================
// tests/unit/ui_leaf_c1_abi_test.cpp — ENDGAME B.3 step 6, block C1
//   the ten Button / Check / Combo / Border / ClipRegion leaf classes
//
//   UI::ButtonDescription      : ElementDescription  0xa0  10 slots
//   UI::CheckDescription       : ButtonDescription   0xa8  10 slots
//   UI::ButtonElement          : Element             0xd8  12 slots
//   UI::CheckElement           : ButtonElement       0xe0  12 slots
//   (Combo / Border / ClipRegion appended below as each pair lands)
//
// Built to the contract of ui_element_abi_test.cpp (block A); only what
// differs is spelled out.
//
// PART A — LAYOUT: sizeof / alignof / every offset the headers claim, the
//   class-vs-POD agreement for the Descriptions (DescriptionsBatch1.h), the
//   parent edges `_ParentTypeName` proves, no virtual destructor, abstract.
// PART B — SLOT ORDER: probe subclasses of the REAL classes; each own slot is
//   raw-called by byte offset and the marker that runs names the member; each
//   INHERITED slot is compared, pointer for pointer, with the parent's entry
//   (the `inherited-from` markers of B36_SLOTS.md section 1).
// PART C — THE RETSLOT SLOT: Description slot 8 `CreateInstance` raw-called the
//   retail way `(this, ret, dialog)` (RULE B6-2).
// PART D — THE QUALIFIED BASE CALL: a door reached through a minted table
//   delegates `->UI::X::M(…)`, and X::M's own call into its PARENT's body is
//   qualified too — neither re-enters the door.
//
// `UI::Element` / `UI::ElementDescription`'s own out-of-line members are the
// MARKERS ui_element_abi_test.cpp defines; this file defines the ten leaves'
// members as its own markers (runtime/src/ui does not link into a host test).
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/Element.h"
#include "runtime/types/ui/ElementDescription.h"
#include "runtime/types/ui/ButtonDescription.h"
#include "runtime/types/ui/ButtonElement.h"
#include "runtime/types/ui/CheckDescription.h"
#include "runtime/types/ui/CheckElement.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
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
// Observation channel: WHICH class's member ran, and which slot it stands for.
// ---------------------------------------------------------------------------
enum Who : int { kNone = 0, kButtonD, kCheckD, kButtonE, kCheckE };

const void* g_self = nullptr;
int         g_who  = kNone;
int         g_slot = -1;
long long   g_arg  = 0;
int         g_trail[8];   // the order of marker entries inside one call
int         g_trailN = 0;

void Note(const void* self, int who, int slot, long long arg = 0) {
    g_self = self; g_who = who; g_slot = slot; g_arg = arg;
    if (g_trailN < 8) g_trail[g_trailN++] = who * 100 + slot;
}
void Reset() { g_self = nullptr; g_who = kNone; g_slot = -1; g_arg = 0; g_trailN = 0; }

template <typename Fn>
Fn SlotAt(const void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
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

// Byte offsets of the two table shapes (B36_SLOTS.md section 1).
enum : int {
    kE0 = 0x00, kE1_TypeName = 0x08, kE2_Type = 0x10, kE3_Persist = 0x18,
    kE4_ComputeSize = 0x20, kE5_ComputeLayout = 0x28, kE6_Draw = 0x30,
    kE7_Update = 0x38, kE8_Enable = 0x40, kE9_Contains = 0x48,
    kE10_SetColor = 0x50, kE11_GetBorder = 0x58,
};
enum : int {
    kD0 = 0x00, kD1_TypeName = 0x08, kD2_Type = 0x10, kD3_GetFileVersion = 0x18,
    kD4_Compile = 0x20, kD5_Serialize = 0x28, kD6_Reconstruct = 0x30,
    kD7_Destruct = 0x38, kD8_CreateInstance = 0x40, kD9_Construct = 0x48,
};

// ---------------------------------------------------------------------------
// Probes. Each overrides ONLY slot 0 (the one pure slot a leaf keeps), so every
// other entry in its table is the leaf's (or an ancestor's) member.
// ---------------------------------------------------------------------------
struct BareElementProbe final : UI::Element {          // the parent's entries
    Object::Base* DeletingDtor(unsigned) override { return this; }
    void ComputeSize() override {}
};
struct ButtonElementProbe final : UI::ButtonElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct CheckElementProbe final : UI::CheckElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ButtonDescriptionProbe final : UI::ButtonDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct CheckDescriptionProbe final : UI::CheckDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};

// Plain `Object::External` leaf: its slot 4 is `External::Compile`.
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

}  // namespace

// ===========================================================================
// The leaves' out-of-line members, AS MARKERS.
// ===========================================================================
namespace UI {

// ---- ButtonElement ----
const char* ButtonElement::TypeName() const { Note(this, kButtonE, 1); return "ButtonElement"; }
uint32_t ButtonElement::Type() const { Note(this, kButtonE, 2); return 0xB0u; }
void ButtonElement::ComputeSize() { Note(this, kButtonE, 4); }
void ButtonElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kButtonE, 5, (long long)x * 1000 + h);
}
void ButtonElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kButtonE, 6, alpha); }
void ButtonElement::Update(const System::Time&, UserInterface*,
                           Object::InternalPtr<DialogInstance>* dialog, const UpdateState& state) {
    Note(this, kButtonE, 7,
         static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX);
}
void ButtonElement::Enable(bool enable) { Note(this, kButtonE, 8, enable ? 1 : 0); }
ButtonElement::~ButtonElement() {}

// ---- CheckElement — Update calls its PARENT's body QUALIFIED, as the real one does.
const char* CheckElement::TypeName() const { Note(this, kCheckE, 1); return "CheckElement"; }
uint32_t CheckElement::Type() const { Note(this, kCheckE, 2); return 0xC0u; }
void CheckElement::ComputeSize() { Note(this, kCheckE, 4); }
void CheckElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kCheckE, 5, (long long)x * 1000 + h);
}
void CheckElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kCheckE, 6, alpha); }
void CheckElement::Update(const System::Time& time, UserInterface* ui,
                          Object::InternalPtr<DialogInstance>* dialog, const UpdateState& state) {
    Note(this, kCheckE, 7);
    Object::InternalPtr<DialogInstance> copy = *dialog;
    ButtonElement::Update(time, ui, &copy, state);
}
CheckElement::~CheckElement() {}

// ---- ButtonDescription ----
const char* ButtonDescription::TypeName() const { Note(this, kButtonD, 1); return "ButtonDescription"; }
uint32_t ButtonDescription::Type() const { Note(this, kButtonD, 2); return 0xBDu; }
int32_t ButtonDescription::GetFileVersion() const { Note(this, kButtonD, 3); return 0x104; }
bool ButtonDescription::Serialize(void* stream) {
    Note(this, kButtonD, 5, reinterpret_cast<intptr_t>(stream)); return true;
}
Object::External* ButtonDescription::Reconstruct() { Note(this, kButtonD, 6); return this; }
void ButtonDescription::Destruct() { Note(this, kButtonD, 7); }
Object::InternalPtr<Element>* ButtonDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kButtonD, 8, reinterpret_cast<intptr_t>(dialog));
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        reinterpret_cast<Element*>(static_cast<uintptr_t>(0xB1B1B1B1)));
    return ret;
}
Object::External* ButtonDescription::Construct() { Note(this, kButtonD, 9); return this; }
ButtonDescription::~ButtonDescription() {}

// ---- CheckDescription — Serialize calls ButtonDescription's QUALIFIED.
const char* CheckDescription::TypeName() const { Note(this, kCheckD, 1); return "CheckDescription"; }
uint32_t CheckDescription::Type() const { Note(this, kCheckD, 2); return 0xCDu; }
int32_t CheckDescription::GetFileVersion() const { Note(this, kCheckD, 3); return 0x1104; }
bool CheckDescription::Serialize(void* stream) {
    Note(this, kCheckD, 5);
    return ButtonDescription::Serialize(stream);
}
Object::External* CheckDescription::Reconstruct() { Note(this, kCheckD, 6); return this; }
void CheckDescription::Destruct() { Note(this, kCheckD, 7); }
Object::InternalPtr<Element>* CheckDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kCheckD, 8, reinterpret_cast<intptr_t>(dialog));
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        reinterpret_cast<Element*>(static_cast<uintptr_t>(0xC1C1C1C1)));
    return ret;
}
Object::External* CheckDescription::Construct() { Note(this, kCheckD, 9); return this; }
CheckDescription::~CheckDescription() {}

}  // namespace UI

namespace {

// Raw-call every OWN slot of an Element-shaped probe and check the marker.
void ExpectElementSlot(void* raw, int off, int who) {
    const int slot = off / 8;
    Reset();
    switch (off) {
    case kE1_TypeName: SlotAt<const char* (*)(const void*)>(raw, off)(raw); break;
    case kE2_Type:     SlotAt<uint32_t (*)(const void*)>(raw, off)(raw); break;
    case kE4_ComputeSize: SlotAt<void (*)(void*)>(raw, off)(raw); break;
    case kE5_ComputeLayout:
        SlotAt<void (*)(void*, int, int, int, int)>(raw, off)(raw, 7, 0, 0, 9);
        CHECK_EQ(g_arg, 7009); break;
    case kE6_Draw:
        SlotAt<void (*)(const void*, void*, uint8_t)>(raw, off)(raw, nullptr, 200);
        CHECK_EQ(g_arg, 200); break;
    case kE8_Enable:
        SlotAt<void (*)(void*, bool)>(raw, off)(raw, true);
        CHECK_EQ(g_arg, 1); break;
    default: CHECK(false); break;
    }
    CHECK_EQ(g_who, who);
    CHECK_EQ(g_slot, slot);
    CHECK(g_self == raw);
}

// Raw-call every OWN slot of a Description-shaped probe and check the marker.
void ExpectDescriptionSlot(void* raw, int off, int who) {
    const int slot = off / 8;
    Reset();
    switch (off) {
    case kD1_TypeName: SlotAt<const char* (*)(const void*)>(raw, off)(raw); break;
    case kD2_Type:     SlotAt<uint32_t (*)(const void*)>(raw, off)(raw); break;
    case kD3_GetFileVersion: SlotAt<int32_t (*)(const void*)>(raw, off)(raw); break;
    case kD5_Serialize:
        SlotAt<bool (*)(void*, void*)>(raw, off)(raw, (void*)0x55); break;
    case kD6_Reconstruct: SlotAt<void* (*)(void*)>(raw, off)(raw); break;
    case kD7_Destruct:    SlotAt<void (*)(void*)>(raw, off)(raw); break;
    case kD9_Construct:   SlotAt<void* (*)(void*)>(raw, off)(raw); break;
    default: CHECK(false); break;
    }
    // Serialize on a CheckDescription runs its own marker, then the base's.
    if (off == kD5_Serialize && who == kCheckD) {
        CHECK_EQ(g_trailN, 2);
        CHECK_EQ(g_trail[0], kCheckD * 100 + 5);
        CHECK_EQ(g_trail[1], kButtonD * 100 + 5);
    } else {
        CHECK_EQ(g_who, who);
        CHECK_EQ(g_slot, slot);
    }
    CHECK(g_self == raw);
}

}  // namespace

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(ui_c1_button_check_sizes_and_parents) {
    // `MOV EDX,imm` in each class's own slot-0 vector deleting destructor.
    CHECK_EQ(sizeof(UI::ButtonElement),     (size_t)0xd8);   // 0x1800485b0
    CHECK_EQ(sizeof(UI::CheckElement),      (size_t)0xe0);   // 0x180048d80
    CHECK_EQ(sizeof(UI::ButtonDescription), (size_t)0xa0);   // 0x1800481a0
    CHECK_EQ(sizeof(UI::CheckDescription),  (size_t)0xa8);   // 0x1800489c0
    CHECK_EQ(alignof(UI::ButtonElement), (size_t)8);
    CHECK_EQ(alignof(UI::CheckElement), (size_t)8);
    CHECK_EQ(alignof(UI::ButtonDescription), (size_t)8);
    CHECK_EQ(alignof(UI::CheckDescription), (size_t)8);

    // The parent edges `_ParentTypeName` proves (B36_SLOTS.md section 3).
    CHECK((std::is_base_of<UI::Element, UI::ButtonElement>::value));
    CHECK((std::is_base_of<UI::ButtonElement, UI::CheckElement>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::ButtonDescription>::value));
    CHECK((std::is_base_of<UI::ButtonDescription, UI::CheckDescription>::value));

    CHECK(std::is_abstract<UI::ButtonElement>::value);
    CHECK(std::is_abstract<UI::CheckElement>::value);
    CHECK(std::is_abstract<UI::ButtonDescription>::value);
    CHECK(std::is_abstract<UI::CheckDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::ButtonElement>::value);
    CHECK(!std::has_virtual_destructor<UI::CheckElement>::value);
    CHECK(!std::has_virtual_destructor<UI::ButtonDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::CheckDescription>::value);
}

UNIT_TEST(ui_c1_button_check_offsets) {
    CHECK_EQ(offsetof(UI::ButtonElement, _keyIndex),   (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ButtonElement, _state),      (size_t)0xc4);
    CHECK_EQ(offsetof(UI::ButtonElement, _stateColor), (size_t)0xc8);
    CHECK_EQ(offsetof(UI::CheckElement, _checked),     (size_t)0xd8);

    CHECK_EQ(offsetof(UI::ButtonDescription, _childDesc), (size_t)0x68);
    CHECK_EQ(offsetof(UI::ButtonDescription, _word90),    (size_t)0x90);
    CHECK_EQ(offsetof(UI::ButtonDescription, _word9c),    (size_t)0x9c);
    CHECK_EQ(offsetof(UI::CheckDescription, _checkDesc),  (size_t)0xa0);
    // Class and POD are one layout (DescriptionsBatch1.h is not edited).
    CHECK_EQ(sizeof(UI::ButtonDescription), sizeof(RtUI::ButtonDescription));
    CHECK_EQ(sizeof(UI::CheckDescription),  sizeof(RtUI::CheckDescription));
    CHECK_EQ(offsetof(UI::CheckDescription, _checkDesc), offsetof(RtUI::CheckDescription, _checkDesc));
    // The name-hiding the header documents: the five hide the base's one.
    CHECK_EQ(offsetof(UI::ElementDescription, _childDesc), (size_t)0x48);
}

// ===========================================================================
// PART B — SLOT ORDER
// ===========================================================================
UNIT_TEST(ui_c1_buttonelement_slot_order_is_retails) {
    FactoryScope scope;
    ButtonElementProbe probe{};
    BareElementProbe parent{};
    void* raw = static_cast<void*>(&probe);
    const void* const* vt = VTableOf(raw);
    const void* const* pv = VTableOf(static_cast<void*>(&parent));

    for (int off : { kE1_TypeName, kE2_Type, kE4_ComputeSize, kE5_ComputeLayout,
                     kE6_Draw, kE8_Enable })
        ExpectElementSlot(raw, off, kButtonE);
    // Slot 7 through the retail pointer-to-handle shape.
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7000));
    UI::Element::UpdateState state;
    state.mouseX = 5;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, kE7_Update)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_who, (int)kButtonE); CHECK_EQ(g_slot, 7); CHECK_EQ(g_arg, 0x7000 + 5);

    // INHERITED from UI::Element: 3 Persist, 9 Contains, 10 SetColor, 11 GetBorder.
    for (int off : { kE3_Persist, kE9_Contains, kE10_SetColor, kE11_GetBorder })
        CHECK(vt[off / 8] == pv[off / 8]);
    // ... and the six own ones are NOT the parent's.
    for (int off : { kE1_TypeName, kE2_Type, kE4_ComputeSize, kE5_ComputeLayout,
                     kE6_Draw, kE7_Update, kE8_Enable })
        CHECK(vt[off / 8] != pv[off / 8]);

    CHECK(static_cast<void*>(static_cast<UI::Element*>(&probe)) == raw);
}

UNIT_TEST(ui_c1_checkelement_slot_order_is_retails) {
    FactoryScope scope;
    CheckElementProbe probe{};
    ButtonElementProbe button{};
    BareElementProbe element{};
    void* raw = static_cast<void*>(&probe);
    const void* const* vt = VTableOf(raw);
    const void* const* bv = VTableOf(static_cast<void*>(&button));
    const void* const* ev = VTableOf(static_cast<void*>(&element));

    for (int off : { kE1_TypeName, kE2_Type, kE4_ComputeSize, kE5_ComputeLayout, kE6_Draw })
        ExpectElementSlot(raw, off, kCheckE);
    // Slot 8 is ButtonElement::Enable, INHERITED (0x180110cc0 in both tables).
    CHECK(vt[kE8_Enable / 8] == bv[kE8_Enable / 8]);
    ExpectElementSlot(raw, kE8_Enable, kButtonE);
    for (int off : { kE3_Persist, kE9_Contains, kE10_SetColor, kE11_GetBorder })
        CHECK(vt[off / 8] == ev[off / 8]);
    CHECK(vt[kE7_Update / 8] != bv[kE7_Update / 8]);
    CHECK(static_cast<void*>(static_cast<UI::ButtonElement*>(&probe)) == raw);
}

UNIT_TEST(ui_c1_buttondescription_slot_order_is_retails) {
    ButtonDescriptionProbe probe;
    BareExternalProbe bare;
    void* raw = static_cast<void*>(&probe);
    for (int off : { kD1_TypeName, kD2_Type, kD3_GetFileVersion, kD5_Serialize,
                     kD6_Reconstruct, kD7_Destruct, kD9_Construct })
        ExpectDescriptionSlot(raw, off, kButtonD);
    // Slot 4 is Object::External::Compile, inherited (RULE B6-1.5).
    CHECK(VTableOf(raw)[kD4_Compile / 8] == VTableOf(static_cast<void*>(&bare))[kD4_Compile / 8]);
    // Ten distinct entries; Construct (9) is BEHIND CreateInstance (8) — B6-1.1.
    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 10; ++i)
        for (int j = i + 1; j < 10; ++j)
            CHECK(vt[i] != vt[j]);
}

UNIT_TEST(ui_c1_checkdescription_slot_order_is_retails) {
    CheckDescriptionProbe probe;
    ButtonDescriptionProbe button;
    void* raw = static_cast<void*>(&probe);
    for (int off : { kD1_TypeName, kD2_Type, kD3_GetFileVersion, kD5_Serialize,
                     kD6_Reconstruct, kD7_Destruct, kD9_Construct })
        ExpectDescriptionSlot(raw, off, kCheckD);
    // Every slot but 4 (and 0, the probe's) is CheckDescription's OWN, not the
    // parent's: B36_SLOTS.md marks only slot 4 inherited.
    const void* const* vt = VTableOf(raw);
    const void* const* bv = VTableOf(static_cast<void*>(&button));
    CHECK(vt[kD4_Compile / 8] == bv[kD4_Compile / 8]);
    for (int i = 1; i < 10; ++i)
        if (i != kD4_Compile / 8) CHECK(vt[i] != bv[i]);
}

// ===========================================================================
// PART C — slot 8, raw-called the RETAIL way (RULE B6-2)
// ===========================================================================
namespace {
void ExpectCreateInstanceRetslot(void* raw, int who, uintptr_t made) {
    const void* const* vtBefore = VTableOf(raw);
    void* slot = nullptr;
    const void* dialog = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xD1A1));
    Reset();
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(
        raw, kD8_CreateInstance)(raw, &slot, dialog);
    CHECK_EQ(g_who, who);
    CHECK_EQ(g_slot, 8);
    CHECK(g_self == raw);
    CHECK_EQ(g_arg, 0xD1A1);
    CHECK(back == &slot);
    CHECK(slot == reinterpret_cast<void*>(made));
    CHECK(VTableOf(raw) == vtBefore);
}
}  // namespace

UNIT_TEST(ui_c1_button_check_createinstance_carries_the_hidden_return_slot) {
    ButtonDescriptionProbe b;
    CheckDescriptionProbe c;
    ExpectCreateInstanceRetslot(static_cast<void*>(&b), kButtonD, 0xB1B1B1B1u);
    ExpectCreateInstanceRetslot(static_cast<void*>(&c), kCheckD, 0xC1C1C1C1u);
}

// ===========================================================================
// PART D — the qualified call, one level of inheritance deep
// ===========================================================================
namespace {
int g_doorEntries = 0;
// The shape of RtUi_CheckElement_Update: a door the minted table's slot 7
// points at, delegating QUALIFIED. CheckElement::Update's own call into
// ButtonElement::Update is qualified too; either one unqualified would land
// back here through the stamped table.
void MintedCheckUpdateDoor(void* self, const void* time, void* ui, void** dlg,
                           const void* state) {
    ++g_doorEntries;
    reinterpret_cast<UI::CheckElement*>(self)->UI::CheckElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),
        *static_cast<const UI::Element::UpdateState*>(state));
}
}  // namespace

UNIT_TEST(ui_c1_qualified_calls_do_not_reenter_the_door) {
    FactoryScope scope;
    CheckElementProbe probe{};
    void* raw = static_cast<void*>(&probe);

    const void* minted[12] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 12; ++i) minted[i] = real[i];
    minted[kE7_Update / 8] = reinterpret_cast<const void*>(&MintedCheckUpdateDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x9000));
    UI::Element::UpdateState state;
    state.mouseX = 3;
    g_doorEntries = 0;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, kE7_Update)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_trailN, 2);
    CHECK_EQ(g_trail[0], kCheckE * 100 + 7);
    CHECK_EQ(g_trail[1], kButtonE * 100 + 7);
    CHECK_EQ(g_arg, 0x9000 + 3);   // the base saw the handle through its own copy

    *reinterpret_cast<const void* const**>(raw) = real;
}
