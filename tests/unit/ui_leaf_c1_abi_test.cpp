// ============================================================================
// tests/unit/ui_leaf_c1_abi_test.cpp — ENDGAME B.3 step 6, block C1
//   the ten Button / Check / Combo / Border / ClipRegion leaf classes
//
//   UI::ButtonDescription      : ElementDescription  0xa0  10 slots
//   UI::CheckDescription       : ButtonDescription   0xa8  10 slots
//   UI::ButtonElement          : Element             0xd8  12 slots
//   UI::CheckElement           : ButtonElement       0xe0  12 slots
//   UI::ComboDescription       : ButtonDescription   0xb8  10 slots
//   UI::ComboElement           : ButtonElement       0x128 12 slots
//   UI::BorderDescription      : ElementDescription  0xb8  10 slots
//   UI::BorderElement          : Element             0x150 12 slots
//   UI::ClipRegionDescription  : ElementDescription  0x78  10 slots
//   UI::ClipRegionElement      : Element             0xd8  12 slots
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
#include "runtime/types/ui/ComboDescription.h"
#include "runtime/types/ui/ComboElement.h"
#include "runtime/types/ui/BorderDescription.h"
#include "runtime/types/ui/BorderElement.h"
#include "runtime/types/ui/ClipRegionDescription.h"
#include "runtime/types/ui/ClipRegionElement.h"

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
enum Who : int { kNone = 0, kButtonD, kCheckD, kButtonE, kCheckE, kComboD, kComboE,
                 kBorderD, kBorderE, kClipD, kClipE, kElementBase };

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
struct BorderElementProbe final : UI::BorderElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct BorderDescriptionProbe final : UI::BorderDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ClipRegionElementProbe final : UI::ClipRegionElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ClipRegionDescriptionProbe final : UI::ClipRegionDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ComboElementProbe final : UI::ComboElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ComboDescriptionProbe final : UI::ComboDescription {
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

// ENDGAME B.3.9 UI-A: the destructor markers of the CONCRETE classes RECORD
// each call, so the real `DeletingDtor` (inline in each header, the one under
// test) can be seen running ~X once per element and in retail's DESCENDING
// order.
namespace {
int         g_b39DtorCount = 0;
const void* g_b39DtorOrder[8] = {};
void B39NoteDtor(const void* self) {
    if (g_b39DtorCount < 8) g_b39DtorOrder[g_b39DtorCount] = self;
    ++g_b39DtorCount;
}
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

// ---- ComboElement: overrides slots 1, 2 and 7 only.
const char* ComboElement::TypeName() const { Note(this, kComboE, 1); return "ComboElement"; }
uint32_t ComboElement::Type() const { Note(this, kComboE, 2); return 0xCE0u; }
void ComboElement::Update(const System::Time& time, UserInterface* ui,
                          Object::InternalPtr<DialogInstance>* dialog, const UpdateState& state) {
    Note(this, kComboE, 7);
    Object::InternalPtr<DialogInstance> copy = *dialog;
    ButtonElement::Update(time, ui, &copy, state);
}
ComboElement::~ComboElement() {}

// ---- ComboDescription — Serialize calls ButtonDescription's QUALIFIED.
const char* ComboDescription::TypeName() const { Note(this, kComboD, 1); return "ComboDescription"; }
uint32_t ComboDescription::Type() const { Note(this, kComboD, 2); return 0xCD0u; }
int32_t ComboDescription::GetFileVersion() const { Note(this, kComboD, 3); return 0x2104; }
bool ComboDescription::Serialize(void* stream) {
    Note(this, kComboD, 5);
    return ButtonDescription::Serialize(stream);
}
Object::External* ComboDescription::Reconstruct() { Note(this, kComboD, 6); return this; }
void ComboDescription::Destruct() { Note(this, kComboD, 7); }
Object::InternalPtr<Element>* ComboDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kComboD, 8, reinterpret_cast<intptr_t>(dialog));
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        reinterpret_cast<Element*>(static_cast<uintptr_t>(0xC0C0C0C0)));
    return ret;
}
Object::External* ComboDescription::Construct() { Note(this, kComboD, 9); return this; }
ComboDescription::~ComboDescription() {}

// ---- BorderElement: overrides 1, 2, 4, 5, 6.
const char* BorderElement::TypeName() const { Note(this, kBorderE, 1); return "BorderElement"; }
uint32_t BorderElement::Type() const { Note(this, kBorderE, 2); return 0xBE0u; }
void BorderElement::ComputeSize() { Note(this, kBorderE, 4); }
void BorderElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kBorderE, 5, (long long)x * 1000 + h);
}
void BorderElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kBorderE, 6, alpha); }
BorderElement::~BorderElement() { B39NoteDtor(this); }

// ---- ClipRegionElement: overrides 1, 2, 4, 5, 6, 7 and 9. Contains chains
// to Element::Contains QUALIFIED, as the real body does.
const char* ClipRegionElement::TypeName() const { Note(this, kClipE, 1); return "ClipRegionElement"; }
uint32_t ClipRegionElement::Type() const { Note(this, kClipE, 2); return 0xC1E0u; }
void ClipRegionElement::ComputeSize() { Note(this, kClipE, 4); }
void ClipRegionElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kClipE, 5, (long long)x * 1000 + h);
}
void ClipRegionElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kClipE, 6, alpha); }
void ClipRegionElement::Update(const System::Time&, UserInterface*,
                               Object::InternalPtr<DialogInstance>*, const UpdateState&) {
    Note(this, kClipE, 7);
}
bool ClipRegionElement::Contains(int32_t x, int32_t y, UserInterface* ui) const {
    Note(this, kClipE, 9);
    return Element::Contains(x, y, ui);
}
ClipRegionElement::~ClipRegionElement() {}

// ---- BorderDescription / ClipRegionDescription: direct ElementDescription leaves.
const char* BorderDescription::TypeName() const { Note(this, kBorderD, 1); return "BorderDescription"; }
uint32_t BorderDescription::Type() const { Note(this, kBorderD, 2); return 0xBD0u; }
int32_t BorderDescription::GetFileVersion() const { Note(this, kBorderD, 3); return 0x104; }
bool BorderDescription::Serialize(void* stream) {
    Note(this, kBorderD, 5, reinterpret_cast<intptr_t>(stream)); return true;
}
Object::External* BorderDescription::Reconstruct() { Note(this, kBorderD, 6); return this; }
void BorderDescription::Destruct() { Note(this, kBorderD, 7); }
Object::InternalPtr<Element>* BorderDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kBorderD, 8, reinterpret_cast<intptr_t>(dialog));
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        reinterpret_cast<Element*>(static_cast<uintptr_t>(0xBDBDBDBD)));
    return ret;
}
Object::External* BorderDescription::Construct() { Note(this, kBorderD, 9); return this; }
BorderDescription::~BorderDescription() { B39NoteDtor(this); }

const char* ClipRegionDescription::TypeName() const { Note(this, kClipD, 1); return "ClipRegionDescription"; }
uint32_t ClipRegionDescription::Type() const { Note(this, kClipD, 2); return 0xC1D0u; }
int32_t ClipRegionDescription::GetFileVersion() const { Note(this, kClipD, 3); return 0x104; }
bool ClipRegionDescription::Serialize(void* stream) {
    Note(this, kClipD, 5, reinterpret_cast<intptr_t>(stream)); return true;
}
Object::External* ClipRegionDescription::Reconstruct() { Note(this, kClipD, 6); return this; }
void ClipRegionDescription::Destruct() { Note(this, kClipD, 7); }
Object::InternalPtr<Element>* ClipRegionDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kClipD, 8, reinterpret_cast<intptr_t>(dialog));
    new (static_cast<void*>(ret)) Object::InternalPtr<Element>(
        reinterpret_cast<Element*>(static_cast<uintptr_t>(0xC1C2C1C2)));
    return ret;
}
Object::External* ClipRegionDescription::Construct() { Note(this, kClipD, 9); return this; }
ClipRegionDescription::~ClipRegionDescription() {}

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
    // Serialize on a Check/ComboDescription runs its own marker, then the base's.
    if (off == kD5_Serialize && (who == kCheckD || who == kComboD)) {
        CHECK_EQ(g_trailN, 2);
        CHECK_EQ(g_trail[0], who * 100 + 5);
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

// ===========================================================================
// COMBO — layout, slot order, retslot
// ===========================================================================
UNIT_TEST(ui_c1_combo_layout) {
    CHECK_EQ(sizeof(UI::ComboElement),     (size_t)0x128);  // 0x180075390
    CHECK_EQ(sizeof(UI::ComboDescription), (size_t)0xb8);   // 0x180074e70
    CHECK_EQ(alignof(UI::ComboElement), (size_t)8);
    CHECK_EQ(alignof(UI::ComboDescription), (size_t)8);
    CHECK((std::is_base_of<UI::ButtonElement, UI::ComboElement>::value));
    CHECK((std::is_base_of<UI::ButtonDescription, UI::ComboDescription>::value));
    CHECK(std::is_abstract<UI::ComboElement>::value);
    CHECK(std::is_abstract<UI::ComboDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::ComboElement>::value);
    CHECK(!std::has_virtual_destructor<UI::ComboDescription>::value);

    CHECK_EQ(offsetof(UI::ComboElement, _listDesc),    (size_t)0xd8);
    CHECK_EQ(offsetof(UI::ComboElement, _itemDesc),    (size_t)0xe0);
    CHECK_EQ(offsetof(UI::ComboElement, _strings),     (size_t)0xe8);
    CHECK_EQ(offsetof(UI::ComboElement, _sprites),     (size_t)0xf8);
    CHECK_EQ(offsetof(UI::ComboElement, _userData),    (size_t)0x108);
    CHECK_EQ(offsetof(UI::ComboElement, _currentItem), (size_t)0x118);
    CHECK_EQ(offsetof(UI::ComboElement, _openDialog),  (size_t)0x120);

    CHECK_EQ(offsetof(UI::ComboDescription, _listDesc),    (size_t)0xa0);
    CHECK_EQ(offsetof(UI::ComboDescription, _itemDesc),    (size_t)0xa8);
    CHECK_EQ(offsetof(UI::ComboDescription, _stringTable), (size_t)0xb0);
    CHECK_EQ(sizeof(UI::ComboDescription), sizeof(RtUI::ComboDescription));
    CHECK_EQ(offsetof(UI::ComboDescription, _stringTable),
             offsetof(RtUI::ComboDescription, _stringTable));
    // The name-hiding the header documents: the base's table is at +0x58.
    CHECK_EQ(offsetof(UI::ElementDescription, _stringTable), (size_t)0x58);
}

UNIT_TEST(ui_c1_comboelement_slot_order_is_retails) {
    FactoryScope scope;
    ComboElementProbe probe{};
    ButtonElementProbe button{};
    BareElementProbe element{};
    void* raw = static_cast<void*>(&probe);
    const void* const* vt = VTableOf(raw);
    const void* const* bv = VTableOf(static_cast<void*>(&button));
    const void* const* ev = VTableOf(static_cast<void*>(&element));

    ExpectElementSlot(raw, kE1_TypeName, kComboE);
    ExpectElementSlot(raw, kE2_Type, kComboE);
    // 4/5/6/8 are ButtonElement's (B36_SLOTS.md: inherited-from UI::ButtonElement).
    for (int off : { kE4_ComputeSize, kE5_ComputeLayout, kE6_Draw, kE8_Enable }) {
        CHECK(vt[off / 8] == bv[off / 8]);
        ExpectElementSlot(raw, off, kButtonE);
    }
    for (int off : { kE3_Persist, kE9_Contains, kE10_SetColor, kE11_GetBorder })
        CHECK(vt[off / 8] == ev[off / 8]);
    CHECK(vt[kE7_Update / 8] != bv[kE7_Update / 8]);

    // Slot 7 is Combo's own and chains to ButtonElement's, qualified.
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x5000));
    UI::Element::UpdateState state;
    state.mouseX = 2;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, kE7_Update)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_trailN, 2);
    CHECK_EQ(g_trail[0], kComboE * 100 + 7);
    CHECK_EQ(g_trail[1], kButtonE * 100 + 7);
    CHECK_EQ(g_arg, 0x5000 + 2);
}

UNIT_TEST(ui_c1_combodescription_slot_order_is_retails) {
    ComboDescriptionProbe probe;
    ButtonDescriptionProbe button;
    void* raw = static_cast<void*>(&probe);
    for (int off : { kD1_TypeName, kD2_Type, kD3_GetFileVersion, kD5_Serialize,
                     kD6_Reconstruct, kD7_Destruct, kD9_Construct })
        ExpectDescriptionSlot(raw, off, kComboD);
    const void* const* vt = VTableOf(raw);
    const void* const* bv = VTableOf(static_cast<void*>(&button));
    CHECK(vt[kD4_Compile / 8] == bv[kD4_Compile / 8]);
    for (int i = 1; i < 10; ++i)
        if (i != kD4_Compile / 8) CHECK(vt[i] != bv[i]);
    ExpectCreateInstanceRetslot(raw, kComboD, 0xC0C0C0C0u);
}

// ===========================================================================
// BORDER and CLIPREGION — layout, slot order, retslot, qualified Contains
// ===========================================================================
UNIT_TEST(ui_c1_border_clipregion_layout) {
    CHECK_EQ(sizeof(UI::BorderElement),         (size_t)0x150);  // 0x1800414f0
    CHECK_EQ(sizeof(UI::BorderDescription),     (size_t)0xb8);   // 0x180040ff0
    CHECK_EQ(sizeof(UI::ClipRegionElement),     (size_t)0xd8);   // 0x180038370
    CHECK_EQ(sizeof(UI::ClipRegionDescription), (size_t)0x78);   // 0x180037eb0
    CHECK_EQ(alignof(UI::BorderElement), (size_t)8);
    CHECK_EQ(alignof(UI::BorderDescription), (size_t)8);
    CHECK_EQ(alignof(UI::ClipRegionElement), (size_t)8);
    CHECK_EQ(alignof(UI::ClipRegionDescription), (size_t)8);
    CHECK((std::is_base_of<UI::Element, UI::BorderElement>::value));
    CHECK((std::is_base_of<UI::Element, UI::ClipRegionElement>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::BorderDescription>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::ClipRegionDescription>::value));
    CHECK(!std::is_abstract<UI::BorderElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::BorderElement>::value);  // protected non-virtual dtor
    CHECK(std::is_abstract<UI::ClipRegionElement>::value);
    CHECK(!std::is_abstract<UI::BorderDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::BorderDescription>::value);  // protected non-virtual dtor
    CHECK(std::is_abstract<UI::ClipRegionDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::BorderElement>::value);
    CHECK(!std::has_virtual_destructor<UI::ClipRegionElement>::value);

    // Nine 0x10-byte sprites, row major, fill BorderElement to its end.
    CHECK_EQ(offsetof(UI::BorderElement, _topLeft),     (size_t)0x0c0);
    CHECK_EQ(offsetof(UI::BorderElement, _centre),      (size_t)0x100);
    CHECK_EQ(offsetof(UI::BorderElement, _bottomRight), (size_t)0x140);
    CHECK_EQ(offsetof(UI::BorderElement, _bottomRight) + 0x10, sizeof(UI::BorderElement));

    CHECK_EQ(offsetof(UI::ClipRegionElement, _reqWidth),    (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ClipRegionElement, _innerWidth),  (size_t)0xc8);
    CHECK_EQ(offsetof(UI::ClipRegionElement, _offsetX),     (size_t)0xd0);
    CHECK_EQ(offsetof(UI::ClipRegionElement, _offsetY),     (size_t)0xd4);

    CHECK_EQ(offsetof(UI::BorderDescription, _spriteSheet), (size_t)0x68);
    CHECK_EQ(offsetof(UI::BorderDescription, _spriteName),  (size_t)0x70);
    CHECK_EQ(sizeof(UI::BorderDescription), sizeof(RtUI::BorderDescription));
    CHECK_EQ(offsetof(UI::ClipRegionDescription, _width),     (size_t)0x68);
    CHECK_EQ(offsetof(UI::ClipRegionDescription, _height),    (size_t)0x6c);
    CHECK_EQ(offsetof(UI::ClipRegionDescription, _childDesc), (size_t)0x70);
    CHECK_EQ(sizeof(UI::ClipRegionDescription), sizeof(RtUI::ClipRegionDescription));
    // The three names ClipRegionDescription hides live elsewhere in the base.
    CHECK_EQ(offsetof(UI::ElementDescription, _width),  (size_t)0x28);
    CHECK_EQ(offsetof(UI::ElementDescription, _height), (size_t)0x2c);
}

UNIT_TEST(ui_c1_borderelement_slot_order_is_retails) {
    FactoryScope scope;
    BorderElementProbe probe{};
    BareElementProbe element{};
    void* raw = static_cast<void*>(&probe);
    const void* const* vt = VTableOf(raw);
    const void* const* ev = VTableOf(static_cast<void*>(&element));
    for (int off : { kE1_TypeName, kE2_Type, kE4_ComputeSize, kE5_ComputeLayout, kE6_Draw })
        ExpectElementSlot(raw, off, kBorderE);
    for (int off : { kE3_Persist, kE7_Update, kE8_Enable, kE9_Contains, kE10_SetColor,
                     kE11_GetBorder })
        CHECK(vt[off / 8] == ev[off / 8]);
}

UNIT_TEST(ui_c1_clipregionelement_slot_order_is_retails) {
    FactoryScope scope;
    ClipRegionElementProbe probe{};
    BareElementProbe element{};
    void* raw = static_cast<void*>(&probe);
    const void* const* vt = VTableOf(raw);
    const void* const* ev = VTableOf(static_cast<void*>(&element));
    for (int off : { kE1_TypeName, kE2_Type, kE4_ComputeSize, kE5_ComputeLayout, kE6_Draw })
        ExpectElementSlot(raw, off, kClipE);
    void* handle = nullptr;
    UI::Element::UpdateState state;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, kE7_Update)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_who, (int)kClipE); CHECK_EQ(g_slot, 7);
    // Slot 9 Contains is ClipRegion's own (the only one in this block).
    CHECK(vt[kE9_Contains / 8] != ev[kE9_Contains / 8]);
    for (int off : { kE3_Persist, kE8_Enable, kE10_SetColor, kE11_GetBorder })
        CHECK(vt[off / 8] == ev[off / 8]);
}

UNIT_TEST(ui_c1_border_clipregion_description_slots) {
    BorderDescriptionProbe border;
    ClipRegionDescriptionProbe clip;
    BareExternalProbe bare;
    const void* compile = VTableOf(static_cast<void*>(&bare))[kD4_Compile / 8];
    for (int off : { kD1_TypeName, kD2_Type, kD3_GetFileVersion, kD5_Serialize,
                     kD6_Reconstruct, kD7_Destruct, kD9_Construct }) {
        ExpectDescriptionSlot(static_cast<void*>(&border), off, kBorderD);
        ExpectDescriptionSlot(static_cast<void*>(&clip), off, kClipD);
    }
    CHECK(VTableOf(static_cast<void*>(&border))[kD4_Compile / 8] == compile);
    CHECK(VTableOf(static_cast<void*>(&clip))[kD4_Compile / 8] == compile);
    ExpectCreateInstanceRetslot(static_cast<void*>(&border), kBorderD, 0xBDBDBDBDu);
    ExpectCreateInstanceRetslot(static_cast<void*>(&clip), kClipD, 0xC1C2C1C2u);
}

namespace {
int g_containsDoorEntries = 0;
bool MintedClipContainsDoor(const void* self, int x, int y, void* ui) {
    ++g_containsDoorEntries;
    return reinterpret_cast<const UI::ClipRegionElement*>(self)            // B.3.6 bridge
        ->UI::ClipRegionElement::Contains(x, y, static_cast<UI::UserInterface*>(ui));
}
}  // namespace

UNIT_TEST(ui_c1_clipregion_contains_reaches_the_base_without_reentry) {
    FactoryScope scope;
    ClipRegionElementProbe probe{};
    void* raw = static_cast<void*>(&probe);

    const void* minted[12] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 12; ++i) minted[i] = real[i];
    minted[kE9_Contains / 8] = reinterpret_cast<const void*>(&MintedClipContainsDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_containsDoorEntries = 0;
    Reset();
    // Element::Contains is ui_element_abi_test.cpp's marker, which returns
    // true: a true here is the base body answering, reached exactly once.
    const bool r = SlotAt<bool (*)(const void*, int, int, void*)>(raw, kE9_Contains)(
        raw, 3, 4, nullptr);
    CHECK(r);
    CHECK_EQ(g_containsDoorEntries, 1);
    CHECK_EQ(g_trailN, 1);
    CHECK_EQ(g_trail[0], kClipE * 100 + 9);

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ===========================================================================
// ENDGAME B.3.9 UI-A (B39_RULES D1/D2, rule B9-16) — the CONCRETE classes.
// ===========================================================================
namespace {
// Itanium PMF -> vtable byte offset: 1 + offset for a virtual, adjustment 0.
template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting
    return words[0] - 1;
}

// Build three elements behind an array cookie, run the ARRAY arm of slot 0 and
// check it destroys downward, frees and returns the block; then the scalar
// arm with a free. `Make` placement-constructs one real object.
template <typename T, typename Make>
void B39ExpectBothArms(size_t elemSize, Make make) {
    void* p = Object::detail::InternalMemAlloc(elemSize, 8);
    CHECK(p != nullptr);
    T* one = make(p);
    g_b39DtorCount = 0;
    CHECK(one->DeletingDtor(1) == one);
    CHECK_EQ(g_b39DtorCount, 1);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * elemSize, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = make(block + 8 + i * elemSize);
    g_b39DtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_b39DtorCount, 3);
    CHECK(g_b39DtorOrder[0] == e[2]);
    CHECK(g_b39DtorOrder[1] == e[1]);
    CHECK(g_b39DtorOrder[2] == e[0]);
}
}  // namespace

UNIT_TEST(b39_uia_border_slot_order_by_pmf) {
    using BE = UI::BorderElement;
    CHECK_EQ(SlotOffset(&BE::DeletingDtor),  (uintptr_t)0x00);   // 0x1800414f0
    CHECK_EQ(SlotOffset(&BE::TypeName),      (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&BE::Type),          (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&BE::ComputeSize),   (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&BE::ComputeLayout), (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&BE::Draw),          (uintptr_t)0x30);
    using BD = UI::BorderDescription;
    CHECK_EQ(SlotOffset(&BD::DeletingDtor),   (uintptr_t)0x00);  // 0x180040ff0
    CHECK_EQ(SlotOffset(&BD::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&BD::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&BD::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&BD::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&BD::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&BD::Destruct),       (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&BD::CreateInstance), (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&BD::Construct),      (uintptr_t)0x48);
}

// ??0BorderElement 0x18010fd40: Element(), then nine {sheet 0, index -1}; each
// Sprite's pad dword untouched. The compiler's table, one per class.
UNIT_TEST(b39_uia_borderelement_ctor_makes_retails_stores) {
    FactoryScope scope;
    alignas(8) uint8_t buf[0x150];
    std::memset(buf, 0xCD, sizeof buf);
    UI::BorderElement* e = ::new (buf) UI::BorderElement();
    CHECK(static_cast<void*>(e) == static_cast<void*>(buf));
    CHECK_EQ(e->_alignment, 3);                    // Element() ran (marker)
    const ::Sprite* s = &e->_topLeft;
    for (int i = 0; i < 9; ++i) {
        CHECK(s[i].spriteSheet == nullptr);
        CHECK_EQ(s[i].spriteIndex, -1);
        CHECK_EQ((uint32_t)s[i].pad0C, 0xCDCDCDCDu);
    }
    BorderElementProbe probe{};
    alignas(8) uint8_t buf2[0x150];
    UI::BorderElement* e2 = ::new (buf2) UI::BorderElement();
    CHECK(VTableOf(e) == VTableOf(e2));
    CHECK(VTableOf(e) != VTableOf(&probe));
    g_b39DtorCount = 0;
    CHECK(e->DeletingDtor(0) == e);
    CHECK(e2->DeletingDtor(0) == e2);
    CHECK_EQ(g_b39DtorCount, 2);
}

// ??0BorderDescription 0x18010fa50: ElementDescription(), then +0x68..+0xb0 zero.
UNIT_TEST(b39_uia_borderdescription_ctor_makes_retails_stores) {
    alignas(8) uint8_t buf[0xb8];
    std::memset(buf, 0xCD, sizeof buf);
    UI::BorderDescription* d = ::new (buf) UI::BorderDescription();
    CHECK_EQ(d->_id, -1);                          // ElementDescription() ran (marker)
    CHECK(d->_spriteSheet == nullptr);
    for (int i = 0; i < 9; ++i) CHECK(d->_spriteName[i] == nullptr);
    BorderDescriptionProbe probe;
    CHECK(VTableOf(d) != VTableOf(&probe));
    g_b39DtorCount = 0;
    CHECK(d->DeletingDtor(0) == d);
    CHECK_EQ(g_b39DtorCount, 1);
}

UNIT_TEST(b39_uia_border_deletingdtor_both_arms) {
    FactoryScope scope;
    B39ExpectBothArms<UI::BorderElement>(0x150, [](void* p) { return ::new (p) UI::BorderElement(); });
    B39ExpectBothArms<UI::BorderDescription>(0xb8, [](void* p) { return ::new (p) UI::BorderDescription(); });
}
