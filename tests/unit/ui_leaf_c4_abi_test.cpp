// ============================================================================
// tests/unit/ui_leaf_c4_abi_test.cpp — ENDGAME B.3 step 6, block C4
//   Container / Layout / List / Scroll / Table, both sides, as real classes
//
//   UI::ContainerDescription 0x90 / UI::ContainerElement 0xd8
//   UI::LayoutDescription    0x78 / UI::LayoutElement    0xd0
//   UI::ListDescription      0x90 / UI::ListElement      0x100
//   UI::ScrollDescription    0xc8 / UI::ScrollElement    0x178
//   UI::TableDescription     0x98 / UI::TableElement     0xf0
//
// Built to the contract of ui_element_abi_test.cpp (block A); only what
// differs is spelled out.
//
// PART A — LAYOUT. Sizes, alignments, every offset the headers claim, the
//   abstract / no-virtual-dtor / single-inheritance claims, and the
//   ContainerDescription class-vs-POD agreement (DescriptionsBatch1.h).
//
// PART B — SLOT ORDER. A probe subclass of each leaf overrides only slot 0.
//   Its GCC table is read back by BYTE OFFSET and each slot raw-called; the
//   marker that runs names the member. The slots a leaf INHERITS are proved by
//   pointer identity against a probe of the base (UI::Element's are the
//   markers ui_element_abi_test.cpp defines; `Object::External::Compile` is the
//   real body). The leaves' own out-of-line members are defined AS MARKERS at
//   the bottom of this file (runtime/src/ui does not link into a host test).
//
// PART C — THE CLASS-VALUED SLOTS, raw-called the RETAIL way: slot 8
//   `CreateInstance` on every Description (RULE B6-2 hidden return slot), and
//   slot 7 `Update` on the three Elements that override it (the by-value
//   InternalPtr<DialogInstance> arrives as a POINTER to the caller's temporary).
//
// PART D — THE QUALIFIED CALL. A door reached through a minted table must
//   delegate `->UI::X::M(…)` or it re-enters itself.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/ContainerDescription.h"
#include "runtime/types/ui/ContainerElement.h"
#include "runtime/types/ui/LayoutDescription.h"
#include "runtime/types/ui/LayoutElement.h"
#include "runtime/types/ui/ListDescription.h"
#include "runtime/types/ui/ListElement.h"
#include "runtime/types/ui/ScrollDescription.h"
#include "runtime/types/ui/ScrollElement.h"
#include "runtime/types/ui/TableDescription.h"
#include "runtime/types/ui/TableElement.h"

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
// Observation channel. A mark is `classTag * 100 + slot`.
// ---------------------------------------------------------------------------
const void* g_self = nullptr;
int         g_mark = -1;
long long   g_arg  = 0;

void Note(const void* self, int mark, long long arg = 0) { g_self = self; g_mark = mark; g_arg = arg; }
void Reset() { g_self = nullptr; g_mark = -1; g_arg = 0; }

enum Tag : int {
    kContainerD = 1, kContainerE = 2, kLayoutD = 3, kLayoutE = 4, kListD = 5,
    kListE = 6, kScrollD = 7, kScrollE = 8, kTableD = 9, kTableE = 10,
};

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

// ---- probes: override slot 0 only ------------------------------------------
#define C4_PROBE(NAME, BASE)                                                   \
    struct NAME final : BASE {                                                 \
        Object::Base* DeletingDtor(unsigned flags) override {                  \
            Note(this, 0, flags); return this;                                 \
        }                                                                      \
    };
C4_PROBE(ContainerDProbe, UI::ContainerDescription)
C4_PROBE(LayoutDProbe,    UI::LayoutDescription)
C4_PROBE(ListDProbe,      UI::ListDescription)
C4_PROBE(ScrollDProbe,    UI::ScrollDescription)
C4_PROBE(TableDProbe,     UI::TableDescription)
C4_PROBE(ContainerEProbe, UI::ContainerElement)
C4_PROBE(LayoutEProbe,    UI::LayoutElement)
C4_PROBE(ListEProbe,      UI::ListElement)
C4_PROBE(ScrollEProbe,    UI::ScrollElement)
C4_PROBE(TableEProbe,     UI::TableElement)
#undef C4_PROBE

// The bases, for the inherited-slot identity checks.
struct BaseElementProbe final : UI::Element {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    void ComputeSize() override {}
};
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
#define C4_DESC_MARKERS(CLS, TAG)                                                          \
    const char* UI::CLS::TypeName() const { Note(this, TAG * 100 + 1); return #CLS; }       \
    uint32_t UI::CLS::Type() const { Note(this, TAG * 100 + 2); return TAG; }               \
    int32_t UI::CLS::GetFileVersion() const { Note(this, TAG * 100 + 3); return TAG; }      \
    bool UI::CLS::Serialize(void* s) {                                                      \
        Note(this, TAG * 100 + 5, reinterpret_cast<intptr_t>(s)); return true; }           \
    Object::External* UI::CLS::Reconstruct() { Note(this, TAG * 100 + 6); return this; }    \
    void UI::CLS::Destruct() { Note(this, TAG * 100 + 7); }                                 \
    Object::InternalPtr<UI::Element>* UI::CLS::CreateInstance(                              \
            Object::InternalPtr<UI::Element>* ret, const UI::Dialog* dialog) const {        \
        Note(this, TAG * 100 + 8, reinterpret_cast<intptr_t>(dialog));                      \
        new (static_cast<void*>(ret)) Object::InternalPtr<UI::Element>(                     \
            reinterpret_cast<UI::Element*>(static_cast<uintptr_t>(0xC4000 + TAG)));         \
        return ret;                                                                         \
    }                                                                                       \
    Object::External* UI::CLS::Construct() { Note(this, TAG * 100 + 9); return this; }      \
    UI::CLS::~CLS() {}
C4_DESC_MARKERS(ContainerDescription, kContainerD)
C4_DESC_MARKERS(LayoutDescription,    kLayoutD)
C4_DESC_MARKERS(ListDescription,      kListD)
C4_DESC_MARKERS(ScrollDescription,    kScrollD)
C4_DESC_MARKERS(TableDescription,     kTableD)
#undef C4_DESC_MARKERS

#define C4_ELEM_MARKERS(CLS, TAG)                                                          \
    const char* UI::CLS::TypeName() const { Note(this, TAG * 100 + 1); return #CLS; }       \
    uint32_t UI::CLS::Type() const { Note(this, TAG * 100 + 2); return TAG; }               \
    void UI::CLS::ComputeSize() { Note(this, TAG * 100 + 4); }                              \
    void UI::CLS::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {                   \
        Note(this, TAG * 100 + 5, (long long)x * 1000 + h); }                               \
    void UI::CLS::Draw(UI::SpriteContext&, uint8_t alpha) const {                           \
        Note(this, TAG * 100 + 6, alpha); }                                                 \
    UI::CLS::~CLS() {}
#define C4_UPDATE_MARKER(CLS, TAG)                                                         \
    void UI::CLS::Update(const System::Time&, UI::UserInterface*,                           \
                         Object::InternalPtr<UI::DialogInstance>* dialog,                   \
                         const UpdateState& state) {                                        \
        Note(this, TAG * 100 + 7,                                                           \
             static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX); \
    }
C4_ELEM_MARKERS(ContainerElement, kContainerE)
C4_ELEM_MARKERS(LayoutElement,    kLayoutE)
C4_ELEM_MARKERS(ListElement,      kListE)
C4_ELEM_MARKERS(ScrollElement,    kScrollE)
C4_ELEM_MARKERS(TableElement,     kTableE)
C4_UPDATE_MARKER(ContainerElement, kContainerE)
C4_UPDATE_MARKER(ListElement,      kListE)
C4_UPDATE_MARKER(ScrollElement,    kScrollE)
#undef C4_ELEM_MARKERS
#undef C4_UPDATE_MARKER

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(ui_c4_sizes_match_retail) {
    // `MOV EDX,imm` in each class's own slot-0 vector deleting destructor.
    CHECK_EQ(sizeof(UI::ContainerDescription), (size_t)0x90);
    CHECK_EQ(sizeof(UI::ContainerElement),     (size_t)0xd8);
    CHECK_EQ(sizeof(UI::LayoutDescription),    (size_t)0x78);
    CHECK_EQ(sizeof(UI::LayoutElement),        (size_t)0xd0);
    CHECK_EQ(sizeof(UI::ListDescription),      (size_t)0x90);
    CHECK_EQ(sizeof(UI::ListElement),          (size_t)0x100);
    CHECK_EQ(sizeof(UI::ScrollDescription),    (size_t)0xc8);
    CHECK_EQ(sizeof(UI::ScrollElement),        (size_t)0x178);
    CHECK_EQ(sizeof(UI::TableDescription),     (size_t)0x98);
    CHECK_EQ(sizeof(UI::TableElement),         (size_t)0xf0);
    CHECK_EQ(alignof(UI::ContainerElement), (size_t)8);
    CHECK_EQ(alignof(UI::ScrollElement), (size_t)8);
    CHECK_EQ(alignof(UI::LayoutDescription), (size_t)8);
    CHECK_EQ(sizeof(UI::LayoutDescription::Entry), (size_t)0x38);
    CHECK_EQ(sizeof(UI::LayoutElement::ChildRecord), (size_t)0x44);
    CHECK_EQ(sizeof(UI::TableDescription::Cell), (size_t)0x10);
}

UNIT_TEST(ui_c4_classes_are_abstract_single_base_no_virtual_dtor) {
#define C4_SHAPE(CLS, BASE)                                                 \
    CHECK(std::is_abstract<UI::CLS>::value);                                \
    CHECK(!std::has_virtual_destructor<UI::CLS>::value);                    \
    CHECK((std::is_base_of<UI::BASE, UI::CLS>::value));
    C4_SHAPE(ContainerDescription, ElementDescription)
    C4_SHAPE(LayoutDescription,    ElementDescription)
    C4_SHAPE(ListDescription,      ElementDescription)
    C4_SHAPE(ScrollDescription,    ElementDescription)
    C4_SHAPE(TableDescription,     ElementDescription)
    C4_SHAPE(ContainerElement,     Element)
    C4_SHAPE(LayoutElement,        Element)
    C4_SHAPE(ListElement,          Element)
    C4_SHAPE(ScrollElement,        Element)
    C4_SHAPE(TableElement,         Element)
#undef C4_SHAPE
}

UNIT_TEST(ui_c4_description_field_offsets) {
    CHECK_EQ(offsetof(UI::ContainerDescription, _requestedWidth),  (size_t)0x68);
    CHECK_EQ(offsetof(UI::ContainerDescription, _requestedHeight), (size_t)0x6c);
    CHECK_EQ(offsetof(UI::ContainerDescription, _clampWidth),      (size_t)0x70);
    CHECK_EQ(offsetof(UI::ContainerDescription, _clampHeight),     (size_t)0x71);
    CHECK_EQ(offsetof(UI::ContainerDescription, _barSize),         (size_t)0x74);
    CHECK_EQ(offsetof(UI::ContainerDescription, _partDescs),       (size_t)0x78);
    // RtUI::ContainerDescription is an ALIAS of the class since fragment pass
    // F2 (DescriptionsBatch1.h): the old POD names are gone, the offsets above
    // are the ones it pinned.
    CHECK((std::is_same<RtUI::ContainerDescription, UI::ContainerDescription>::value));

    CHECK_EQ(offsetof(UI::LayoutDescription, _entries), (size_t)0x68);
    CHECK_EQ(offsetof(UI::LayoutDescription::Entry, name),        (size_t)0x08);
    CHECK_EQ(offsetof(UI::LayoutDescription::Entry, relativeToX), (size_t)0x10);
    CHECK_EQ(offsetof(UI::LayoutDescription::Entry, relativeToY), (size_t)0x18);
    CHECK_EQ(offsetof(UI::LayoutDescription::Entry, horizontal),  (size_t)0x20);
    CHECK_EQ(offsetof(UI::LayoutDescription::Entry, force),       (size_t)0x30);

    CHECK_EQ(offsetof(UI::ListDescription, _partDescs),   (size_t)0x68);
    CHECK_EQ(offsetof(UI::ListDescription, _spriteSheet), (size_t)0x78);
    CHECK_EQ(offsetof(UI::ListDescription, _spriteName),  (size_t)0x80);
    CHECK_EQ(offsetof(UI::ListDescription, _hoverColor),  (size_t)0x88);
    CHECK_EQ(offsetof(UI::ListDescription, _selectColor), (size_t)0x8c);

    CHECK_EQ(offsetof(UI::ScrollDescription, _spriteSheet), (size_t)0x68);
    CHECK_EQ(offsetof(UI::ScrollDescription, _spriteNames), (size_t)0x70);
    CHECK_EQ(offsetof(UI::ScrollDescription, _vertical),    (size_t)0xb8);
    CHECK_EQ(offsetof(UI::ScrollDescription, _inset),       (size_t)0xbc);
    CHECK_EQ(offsetof(UI::ScrollDescription, _barColor),    (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ScrollDescription, _unknownC4),   (size_t)0xc4);

    CHECK_EQ(offsetof(UI::TableDescription, _columnCount), (size_t)0x68);
    CHECK_EQ(offsetof(UI::TableDescription, _rowCount),    (size_t)0x6c);
    CHECK_EQ(offsetof(UI::TableDescription, _spacing),     (size_t)0x70);
    CHECK_EQ(offsetof(UI::TableDescription, _cells),       (size_t)0x78);
    CHECK_EQ(offsetof(UI::TableDescription, _unknown88),   (size_t)0x88);
    CHECK_EQ(offsetof(UI::TableDescription, _unknown90),   (size_t)0x90);
}

UNIT_TEST(ui_c4_element_field_offsets) {
    CHECK_EQ(offsetof(UI::ContainerElement, _requestedWidth),  (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ContainerElement, _requestedHeight), (size_t)0xc4);
    CHECK_EQ(offsetof(UI::ContainerElement, _barSize),         (size_t)0xc8);
    CHECK_EQ(offsetof(UI::ContainerElement, _clampWidth),      (size_t)0xcc);
    CHECK_EQ(offsetof(UI::ContainerElement, _clampHeight),     (size_t)0xcd);
    CHECK_EQ(offsetof(UI::ContainerElement, _wheelImpulse),    (size_t)0xd0);

    CHECK_EQ(offsetof(UI::LayoutElement, _childRecords), (size_t)0xc0);
    CHECK_EQ(offsetof(UI::LayoutElement::ChildRecord, sizeX),  (size_t)0x1c);
    CHECK_EQ(offsetof(UI::LayoutElement::ChildRecord, left),   (size_t)0x24);
    CHECK_EQ(offsetof(UI::LayoutElement::ChildRecord, bottom), (size_t)0x3c);

    CHECK_EQ(offsetof(UI::ListElement, _highlight),     (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ListElement, _hoverColor),    (size_t)0xd0);
    CHECK_EQ(offsetof(UI::ListElement, _selectedIndex), (size_t)0xd8);
    CHECK_EQ(offsetof(UI::ListElement, _hoverIndex),    (size_t)0xdc);
    CHECK_EQ(offsetof(UI::ListElement, _userData),      (size_t)0xe0);
    CHECK_EQ(offsetof(UI::ListElement, _ribbon),        (size_t)0xf0);
    CHECK_EQ(offsetof(UI::ListElement, _clip),          (size_t)0xf8);

    CHECK_EQ(offsetof(UI::ScrollElement, _trackStart),     (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ScrollElement, _thumbStart),     (size_t)0xf0);
    CHECK_EQ(offsetof(UI::ScrollElement, _hotEnd),         (size_t)0x140);
    CHECK_EQ(offsetof(UI::ScrollElement, _vertical),       (size_t)0x150);
    CHECK_EQ(offsetof(UI::ScrollElement, _barColor),       (size_t)0x154);
    CHECK_EQ(offsetof(UI::ScrollElement, _inset),          (size_t)0x15c);
    CHECK_EQ(offsetof(UI::ScrollElement, _currentValue),   (size_t)0x160);
    CHECK_EQ(offsetof(UI::ScrollElement, _displayPercent), (size_t)0x164);
    CHECK_EQ(offsetof(UI::ScrollElement, _maxValue),       (size_t)0x168);
    CHECK_EQ(offsetof(UI::ScrollElement, _dragValue),      (size_t)0x170);

    CHECK_EQ(offsetof(UI::TableElement, _rowCount),    (size_t)0xc0);
    CHECK_EQ(offsetof(UI::TableElement, _columnCount), (size_t)0xc4);
    CHECK_EQ(offsetof(UI::TableElement, _spacing),     (size_t)0xc8);
    CHECK_EQ(offsetof(UI::TableElement, _rowSizes),    (size_t)0xd0);
    CHECK_EQ(offsetof(UI::TableElement, _columnSizes), (size_t)0xe0);
}

// ===========================================================================
// PART B — SLOT ORDER
// ===========================================================================
namespace {

// Ten description slots: own 1,2,3,5,6,7,8(retslot, PART C),9; 4 inherited.
template <typename Probe>
void CheckDescriptionSlots(int tag) {
    Probe probe;
    void* raw = static_cast<void*>(&probe);

    Reset(); SlotAt<void* (*)(void*, unsigned)>(raw, 0)(raw, 0u);
    CHECK_EQ(g_mark, 0); CHECK(g_self == raw);
    Reset(); SlotAt<const char* (*)(const void*)>(raw, 1)(raw);   CHECK_EQ(g_mark, tag * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw);      CHECK_EQ(g_mark, tag * 100 + 2);
    Reset(); SlotAt<int32_t (*)(const void*)>(raw, 3)(raw);       CHECK_EQ(g_mark, tag * 100 + 3);
    Reset(); SlotAt<bool (*)(void*, void*)>(raw, 5)(raw, (void*)0x55);
    CHECK_EQ(g_mark, tag * 100 + 5); CHECK_EQ(g_arg, 0x55);
    Reset(); SlotAt<void* (*)(void*)>(raw, 6)(raw);               CHECK_EQ(g_mark, tag * 100 + 6);
    Reset(); SlotAt<void (*)(void*)>(raw, 7)(raw);                CHECK_EQ(g_mark, tag * 100 + 7);
    // Slot 9 is Construct — BEHIND CreateInstance (RULE B6-1.1).
    Reset(); SlotAt<void* (*)(void*)>(raw, 9)(raw);               CHECK_EQ(g_mark, tag * 100 + 9);

    // Slot 4 is `Object::External::Compile`, inherited (RULE B6-1.5).
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[4] == VTableOf(static_cast<void*>(&bare))[4]);

    // Ten distinct entries.
    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 10; ++i)
        for (int j = i + 1; j < 10; ++j)
            CHECK(vt[i] != vt[j]);
    CHECK(static_cast<void*>(static_cast<UI::ElementDescription*>(&probe)) == raw);
}

// Twelve element slots: own 1,2,4,5,6 (+7 when `ownsUpdate`); 3,8,9,10,11 (and
// 7 otherwise) are UI::Element's.
template <typename Probe>
void CheckElementSlots(int tag, bool ownsUpdate) {
    FactoryScope scope;
    Probe probe;
    BaseElementProbe base;
    void* raw = static_cast<void*>(&probe);
    const void* const* vt  = VTableOf(raw);
    const void* const* bvt = VTableOf(static_cast<void*>(&base));

    Reset(); SlotAt<void* (*)(void*, unsigned)>(raw, 0)(raw, 0u);
    CHECK_EQ(g_mark, 0); CHECK(g_self == raw);
    Reset(); SlotAt<const char* (*)(const void*)>(raw, 1)(raw);   CHECK_EQ(g_mark, tag * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw);      CHECK_EQ(g_mark, tag * 100 + 2);
    Reset(); SlotAt<void (*)(void*)>(raw, 4)(raw);                CHECK_EQ(g_mark, tag * 100 + 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(raw, 5)(raw, 7, 0, 0, 9);
    CHECK_EQ(g_mark, tag * 100 + 5); CHECK_EQ(g_arg, 7009);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(raw, 6)(raw, nullptr, 200);
    CHECK_EQ(g_mark, tag * 100 + 6); CHECK_EQ(g_arg, 200);

    for (int s : {3, 8, 9, 10, 11}) CHECK(vt[s] == bvt[s]);
    if (ownsUpdate) CHECK(vt[7] != bvt[7]);
    else            CHECK(vt[7] == bvt[7]);
    for (int s : {1, 2, 4, 5, 6}) CHECK(vt[s] != bvt[s]);

    CHECK(static_cast<void*>(static_cast<UI::Element*>(&probe)) == raw);
    CHECK(static_cast<void*>(static_cast<Object::Base*>(&probe)) == raw);
}

}  // namespace

UNIT_TEST(ui_c4_description_slot_order_is_retails) {
    CheckDescriptionSlots<ContainerDProbe>(kContainerD);
    CheckDescriptionSlots<LayoutDProbe>(kLayoutD);
    CheckDescriptionSlots<ListDProbe>(kListD);
    CheckDescriptionSlots<ScrollDProbe>(kScrollD);
    CheckDescriptionSlots<TableDProbe>(kTableD);
}

UNIT_TEST(ui_c4_element_slot_order_is_retails) {
    // B36_SLOTS.md section 1: Container/List/Scroll override Update (slot 7),
    // Layout/Table inherit Element's.
    CheckElementSlots<ContainerEProbe>(kContainerE, true);
    CheckElementSlots<LayoutEProbe>(kLayoutE, false);
    CheckElementSlots<ListEProbe>(kListE, true);
    CheckElementSlots<ScrollEProbe>(kScrollE, true);
    CheckElementSlots<TableEProbe>(kTableE, false);
}

// ===========================================================================
// PART C — the class-valued slots, raw-called the RETAIL way
// ===========================================================================
namespace {
template <typename Probe>
void CheckCreateInstanceRetslot(int tag) {
    Probe probe;
    void* raw = static_cast<void*>(&probe);
    const void* const* vtBefore = VTableOf(raw);

    // Retail order: RCX = this, RDX = the caller's slot, R8 = Dialog*.
    void* slot = nullptr;
    const void* dialog = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xD1A1));
    Reset();
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(raw, 8)(raw, &slot, dialog);
    CHECK_EQ(g_mark, tag * 100 + 8);
    CHECK(g_self == raw);
    CHECK_EQ(g_arg, 0xD1A1);
    CHECK(back == &slot);
    CHECK(slot == reinterpret_cast<void*>(static_cast<uintptr_t>(0xC4000 + tag)));
    CHECK(VTableOf(raw) == vtBefore);
}

template <typename Probe>
void CheckUpdatePointerToHandle(int tag) {
    FactoryScope scope;
    Probe probe;
    void* raw = static_cast<void*>(&probe);
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7000));
    UI::Element::UpdateState state;
    state.mouseX = 5;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, 7)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_mark, tag * 100 + 7);
    CHECK_EQ(g_arg, 0x7000 + 5);
}
}  // namespace

UNIT_TEST(ui_c4_slot8_createinstance_carries_the_hidden_return_slot) {
    CheckCreateInstanceRetslot<ContainerDProbe>(kContainerD);
    CheckCreateInstanceRetslot<LayoutDProbe>(kLayoutD);
    CheckCreateInstanceRetslot<ListDProbe>(kListD);
    CheckCreateInstanceRetslot<ScrollDProbe>(kScrollD);
    CheckCreateInstanceRetslot<TableDProbe>(kTableD);
}

UNIT_TEST(ui_c4_slot7_update_receives_a_pointer_to_the_handle) {
    CheckUpdatePointerToHandle<ContainerEProbe>(kContainerE);
    CheckUpdatePointerToHandle<ListEProbe>(kListE);
    CheckUpdatePointerToHandle<ScrollEProbe>(kScrollE);
}

// ===========================================================================
// PART D — the qualified call
// ===========================================================================
namespace {
int g_doorEntries = 0;
void MintedComputeSizeDoor(void* self) {
    ++g_doorEntries;
    // THE QUALIFIED FORM. Without `UI::TableElement::` this dispatches through
    // the stamped table below, back into this door.
    reinterpret_cast<UI::TableElement*>(self)->UI::TableElement::ComputeSize();   // B.3.6 bridge
}
}  // namespace

UNIT_TEST(ui_c4_qualified_call_does_not_reenter_the_door) {
    FactoryScope scope;
    TableEProbe probe;
    void* raw = static_cast<void*>(&probe);

    const void* minted[12] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 12; ++i) minted[i] = real[i];
    minted[4] = reinterpret_cast<const void*>(&MintedComputeSizeDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    SlotAt<void (*)(void*)>(raw, 4)(raw);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_mark, kTableE * 100 + 4);

    *reinterpret_cast<const void* const**>(raw) = real;
}

// The one-instruction accessors, from the disassembly quoted in the headers.
UNIT_TEST(ui_c4_inline_accessors_read_the_retail_offsets) {
    FactoryScope scope;
    ContainerEProbe c;
    c._requestedWidth = 11; c._requestedHeight = 22;
    CHECK_EQ(c.GetWidth(), 11);   // 0x180039ee0 MOV EAX,[RCX+0xc0]
    CHECK_EQ(c.GetHeight(), 22);  // 0x180039ef0 MOV EAX,[RCX+0xc4]

    TableEProbe t;
    t._rowCount = 3; t._columnCount = 4;
    CHECK_EQ(t.GetRowCount(), 3);     // 0x180079450
    CHECK_EQ(t.GetColumnCount(), 4);  // 0x180079460

    ListEProbe l;
    l._selectedIndex = 9;
    CHECK_EQ(l.GetCurrentSelection(), 9);   // 0x180045130

    ScrollEProbe s;
    s.SetCurrentValue(2.5f);
    CHECK(s._currentValue == 2.5f);          // 0x18003aa10
    CHECK(s.GetCurrentValue() == 2.5f);      // 0x18003aa20
}
