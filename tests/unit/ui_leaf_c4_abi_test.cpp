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
#include "b39_uib_test_util.h"   // ENDGAME B.3.9 UI-B: the concrete-class checks

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

// Slots 1/2 (ENDGAME B.3.9 TI-UI): the record of the class `tag` names.
const ::Object::BaseTypeInfo* WhoRecord(int tag);


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
    UI::CLS::~CLS() { B39NoteDtor(this); }
C4_DESC_MARKERS(ContainerDescription, kContainerD)
C4_DESC_MARKERS(LayoutDescription,    kLayoutD)
C4_DESC_MARKERS(ListDescription,      kListD)
C4_DESC_MARKERS(ScrollDescription,    kScrollD)
C4_DESC_MARKERS(TableDescription,     kTableD)
#undef C4_DESC_MARKERS

#define C4_ELEM_MARKERS(CLS, TAG)                                                          \
    void UI::CLS::ComputeSize() { Note(this, TAG * 100 + 4); }                              \
    void UI::CLS::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {                   \
        Note(this, TAG * 100 + 5, (long long)x * 1000 + h); }                               \
    void UI::CLS::Draw(UI::SpriteContext&, uint8_t alpha) const {                           \
        Note(this, TAG * 100 + 6, alpha); }                                                 \
    UI::CLS::~CLS() { B39NoteDtor(this); }
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

// ENDGAME B.3.9 TI-UI: slots 1/2 are DECLARE_TYPE's inline bodies over each
// class's own record, so the host defines the records (the literals are the
// ones runtime/src/ui passes) and the factory pair they point at; the
// factories are never called here.
#define C4_TEST_FACTORIES(CLS)                                              \
    ::Object::Base* UI::CLS::CreateNewObject() { return nullptr; }              \
    ::Object::Base* UI::CLS::CreateInPlaceObject(void*) { return nullptr; }
C4_TEST_FACTORIES(ContainerDescription)
DEFINE_TYPE(UI::ContainerDescription, "ElementDescription", nullptr)
C4_TEST_FACTORIES(LayoutDescription)
DEFINE_TYPE(UI::LayoutDescription, "ElementDescription", nullptr)
C4_TEST_FACTORIES(ListDescription)
DEFINE_TYPE(UI::ListDescription, "ElementDescription", nullptr)
C4_TEST_FACTORIES(ScrollDescription)
DEFINE_TYPE(UI::ScrollDescription, "ElementDescription", nullptr)
C4_TEST_FACTORIES(TableDescription)
DEFINE_TYPE(UI::TableDescription, "ElementDescription", nullptr)
C4_TEST_FACTORIES(ContainerElement)
DEFINE_TYPE(UI::ContainerElement, "Element", nullptr)
C4_TEST_FACTORIES(LayoutElement)
DEFINE_TYPE(UI::LayoutElement, "Element", nullptr)
C4_TEST_FACTORIES(ListElement)
DEFINE_TYPE(UI::ListElement, "Element", nullptr)
C4_TEST_FACTORIES(ScrollElement)
DEFINE_TYPE(UI::ScrollElement, "Element", nullptr)
C4_TEST_FACTORIES(TableElement)
DEFINE_TYPE(UI::TableElement, "Element", nullptr)
#undef C4_TEST_FACTORIES

namespace {
const ::Object::BaseTypeInfo* WhoRecord(int tag) {
    switch (tag) {
    case kContainerD: return &UI::ContainerDescription::GetTypeInfo();
    case kLayoutD: return &UI::LayoutDescription::GetTypeInfo();
    case kListD: return &UI::ListDescription::GetTypeInfo();
    case kScrollD: return &UI::ScrollDescription::GetTypeInfo();
    case kTableD: return &UI::TableDescription::GetTypeInfo();
    case kContainerE: return &UI::ContainerElement::GetTypeInfo();
    case kLayoutE: return &UI::LayoutElement::GetTypeInfo();
    case kListE: return &UI::ListElement::GetTypeInfo();
    case kScrollE: return &UI::ScrollElement::GetTypeInfo();
    case kTableE: return &UI::TableElement::GetTypeInfo();
    default: return nullptr;
    }
}
}  // namespace

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
    // ENDGAME B.3.9 UI-B: the converted classes are CONCRETE (their own
    // test below); the rest stay abstract until their commit.
#define C4_SHAPE(CLS, BASE)                                                 \
    CHECK(std::is_abstract<UI::CLS>::value);                                \
    CHECK(!std::has_virtual_destructor<UI::CLS>::value);                    \
    CHECK((std::is_base_of<UI::BASE, UI::CLS>::value));
#define C4_CONCRETE(CLS, BASE)                                              \
    CHECK(B39IsConcreteShape<UI::CLS>());                                   \
    CHECK((std::is_base_of<UI::BASE, UI::CLS>::value));
    C4_CONCRETE(ContainerDescription, ElementDescription)
    C4_CONCRETE(LayoutDescription, ElementDescription)
    C4_CONCRETE(ListDescription,   ElementDescription)
    C4_CONCRETE(ScrollDescription, ElementDescription)
    C4_CONCRETE(TableDescription,  ElementDescription)
    C4_CONCRETE(ContainerElement,  Element)
    C4_CONCRETE(LayoutElement,     Element)
    C4_CONCRETE(ListElement,       Element)
    C4_CONCRETE(ScrollElement,     Element)
    C4_CONCRETE(TableElement,      Element)
#undef C4_SHAPE
#undef C4_CONCRETE
}

// ===========================================================================
// ENDGAME B.3.9 block UI-B (B39_RULES D1, rule B9-16): the concrete classes —
// slot order by PMF on the real header, the real ctor's retail stores, both
// DeletingDtor arms.
// ===========================================================================
UNIT_TEST(b39_uib_c4_slot_order_by_pmf) {
    B39CheckDescriptionPmf<UI::ContainerDescription>();
    B39CheckElementPmf<UI::ContainerElement>();
    B39CheckDescriptionPmf<UI::LayoutDescription>();
    B39CheckElementPmf<UI::LayoutElement>();
    B39CheckDescriptionPmf<UI::ListDescription>();
    B39CheckElementPmf<UI::ListElement>();
}

UNIT_TEST(b39_uib_c4_layout_list_ctor_stores) {
    FactoryScope scope;
    // ??0LayoutDescription 0x1800d3a90 / ??0LayoutElement 0x1800d32e0: the
    // array headers zero.
    alignas(8) uint8_t ld[0x78];
    std::memset(ld, 0xCD, sizeof ld);
    auto* lds = ::new (static_cast<void*>(ld)) UI::LayoutDescription();
    CHECK(lds->_entries._data == nullptr);
    CHECK_EQ(lds->_entries._size, 0);
    CHECK_EQ(lds->_entries._capacity, 0);
    CHECK_EQ(lds->_id, -1);
    lds->DeletingDtor(0);
    alignas(8) uint8_t le[0xd0];
    std::memset(le, 0xCD, sizeof le);
    auto* les = ::new (static_cast<void*>(le)) UI::LayoutElement();
    CHECK(les->_childRecords._data == nullptr);
    CHECK_EQ(les->_childRecords._size, 0);
    CHECK_EQ(les->_childRecords._capacity, 0);
    les->DeletingDtor(0);

    // ??0ListDescription 0x1800d49b0.
    alignas(8) uint8_t d[0x90];
    std::memset(d, 0xCD, sizeof d);
    auto* lsd = ::new (static_cast<void*>(d)) UI::ListDescription();
    CHECK(lsd->_partDescs[0] == nullptr);
    CHECK(lsd->_partDescs[1] == nullptr);
    CHECK(lsd->_spriteSheet == nullptr);
    CHECK(lsd->_spriteName == nullptr);
    CHECK_EQ(lsd->_hoverColor, 0xff808080u);
    CHECK_EQ(lsd->_selectColor, 0xffffffffu);
    lsd->DeletingDtor(0);
    // ??0ListElement 0x1800d4570.
    alignas(8) uint8_t e[0x100];
    std::memset(e, 0xCD, sizeof e);
    auto* lse = ::new (static_cast<void*>(e)) UI::ListElement();
    CHECK(lse->_highlight.spriteSheet == nullptr);
    CHECK_EQ(lse->_highlight.spriteIndex, -1);
    CHECK_EQ(lse->_hoverColor, 0xff808080u);
    CHECK_EQ(lse->_selectColor, 0xffffffffu);
    CHECK_EQ(lse->_selectedIndex, -1);
    CHECK_EQ(lse->_hoverIndex, -1);
    CHECK(lse->_userData._data == nullptr);
    CHECK_EQ(lse->_userData._size, 0);
    CHECK(lse->_ribbon == nullptr);
    CHECK(lse->_clip == nullptr);
    lse->DeletingDtor(0);
}

UNIT_TEST(b39_uib_c4_layout_list_deletingdtor_arms) {
    FactoryScope scope;
    LayoutDProbe ldp;
    B39CheckDeletingDtorArms<UI::LayoutDescription, 0x78>(VTableOf(&ldp));
    LayoutEProbe lep;
    B39CheckDeletingDtorArms<UI::LayoutElement, 0xd0>(VTableOf(&lep));
    ListDProbe sdp;
    B39CheckDeletingDtorArms<UI::ListDescription, 0x90>(VTableOf(&sdp));
    ListEProbe sep;
    B39CheckDeletingDtorArms<UI::ListElement, 0x100>(VTableOf(&sep));
}

UNIT_TEST(b39_uib_c4_container_ctor_stores) {
    FactoryScope scope;
    // ??0ContainerDescription 0x1800cd060: +0x68/+0x6c = 0x64, +0x70 WORD 0,
    // +0x74 0, +0x78..+0x8f 0; +0x72/+0x73 written by nothing.
    alignas(8) uint8_t d[0x90];
    std::memset(d, 0xCD, sizeof d);
    auto* cd = ::new (static_cast<void*>(d)) UI::ContainerDescription();
    CHECK_EQ(cd->_requestedWidth, 0x64);
    CHECK_EQ(cd->_requestedHeight, 0x64);
    CHECK_EQ((int)cd->_clampWidth, 0);
    CHECK_EQ((int)cd->_clampHeight, 0);
    CHECK_EQ((int)d[0x72], 0xCD);
    CHECK_EQ((int)d[0x73], 0xCD);
    CHECK_EQ(cd->_barSize, 0);
    for (int i = 0; i < 3; ++i) CHECK(cd->_partDescs[i] == nullptr);
    CHECK_EQ(cd->_id, -1);                  // ElementDescription's real ctor
    CHECK_EQ((int)cd->_elementType, 3);
    cd->DeletingDtor(0);

    // ??0ContainerElement 0x1800ccb40: +0xc8 0, +0xcc WORD 0, +0xd0 0;
    // +0xc0/+0xc4 and the pads are NOT written.
    alignas(8) uint8_t e[0xd8];
    std::memset(e, 0xCD, sizeof e);
    auto* ce = ::new (static_cast<void*>(e)) UI::ContainerElement();
    CHECK_EQ(ce->_barSize, 0);
    CHECK_EQ((int)ce->_clampWidth, 0);
    CHECK_EQ((int)ce->_clampHeight, 0);
    CHECK_EQ(ce->_wheelImpulse, 0);
    CHECK_EQ((int)e[0xc0], 0xCD);
    CHECK_EQ((int)e[0xc4], 0xCD);
    CHECK_EQ((int)e[0xce], 0xCD);
    CHECK_EQ((int)e[0xd4], 0xCD);
    ce->DeletingDtor(0);
}

UNIT_TEST(b39_uib_c4_container_deletingdtor_arms) {
    FactoryScope scope;
    ContainerDProbe dp;
    B39CheckDeletingDtorArms<UI::ContainerDescription, 0x90>(VTableOf(&dp));
    ContainerEProbe ep;
    B39CheckDeletingDtorArms<UI::ContainerElement, 0xd8>(VTableOf(&ep));
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
    CHECK(SlotAt<const char* (*)(const void*)>(raw, 1)(raw) == WhoRecord(tag)->name);
    CHECK_EQ(SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw), WhoRecord(tag)->typeId);
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
    CHECK(SlotAt<const char* (*)(const void*)>(raw, 1)(raw) == WhoRecord(tag)->name);
    CHECK_EQ(SlotAt<uint32_t (*)(const void*)>(raw, 2)(raw), WhoRecord(tag)->typeId);
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

// ---- Scroll (ENDGAME B.3.9 UI-B) -------------------------------------------
UNIT_TEST(b39_uib_c4_scroll) {
    B39CheckDescriptionPmf<UI::ScrollDescription>();
    B39CheckElementPmf<UI::ScrollElement>();

    FactoryScope scope;
    // ??0ScrollDescription 0x18011a4e0: +0xb9..+0xbb written by nothing.
    alignas(8) uint8_t d[0xc8];
    std::memset(d, 0xCD, sizeof d);
    auto* sd = ::new (static_cast<void*>(d)) UI::ScrollDescription();
    CHECK(sd->_spriteSheet == nullptr);
    for (int i = 0; i < 9; ++i) CHECK(sd->_spriteNames[i] == nullptr);
    CHECK_EQ((int)sd->_vertical, 1);
    CHECK_EQ((int)d[0xb9], 0xCD);
    CHECK_EQ(sd->_inset, 1);
    CHECK_EQ(sd->_barColor, 0xffffffffu);
    CHECK_EQ(sd->_unknownC4, 0xffffffffu);
    sd->DeletingDtor(0);
    // ??0ScrollElement 0x18011a7f0: _inset (+0x15c) NOT written.
    alignas(8) uint8_t e[0x178];
    std::memset(e, 0xCD, sizeof e);
    auto* se = ::new (static_cast<void*>(e)) UI::ScrollElement();
    CHECK(se->_trackStart.spriteSheet == nullptr);
    CHECK_EQ(se->_hotEnd.spriteIndex, -1);
    CHECK_EQ((int)se->_vertical, 1);
    CHECK_EQ(se->_barColor, 0xffffffffu);
    CHECK_EQ(se->_unknown158, 0xffffffffu);
    CHECK_EQ((int)e[0x15c], 0xCD);
    CHECK(se->_currentValue == 0.0f);
    CHECK(se->_displayPercent == 0.33333334f);
    CHECK(se->_maxValue == 4.0f);
    CHECK(se->_dragAnchor == 0.0f);
    CHECK(se->_dragValue == 0.0f);
    se->DeletingDtor(0);

    ScrollDProbe dp;
    B39CheckDeletingDtorArms<UI::ScrollDescription, 0xc8>(VTableOf(&dp));
    ScrollEProbe ep;
    B39CheckDeletingDtorArms<UI::ScrollElement, 0x178>(VTableOf(&ep));
}

// ---- Table (ENDGAME B.3.9 UI-B) --------------------------------------------
UNIT_TEST(b39_uib_c4_table) {
    B39CheckDescriptionPmf<UI::TableDescription>();
    B39CheckElementPmf<UI::TableElement>();

    FactoryScope scope;
    // ??0TableDescription 0x18011e7a0: +0x74 not written.
    alignas(8) uint8_t d[0x98];
    std::memset(d, 0xCD, sizeof d);
    auto* td = ::new (static_cast<void*>(d)) UI::TableDescription();
    CHECK_EQ(td->_columnCount, 1);
    CHECK_EQ(td->_rowCount, 1);
    CHECK_EQ(td->_spacing, 0);
    CHECK_EQ((int)d[0x74], 0xCD);
    CHECK(td->_cells._data == nullptr);
    CHECK_EQ(td->_cells._size, 0);
    CHECK_EQ(td->_unknown88, 0ull);
    CHECK(td->_unknown90 == nullptr);
    td->DeletingDtor(0);
    // ??0TableElement 0x18011e2f0: +0xcc not written.
    alignas(8) uint8_t e[0xf0];
    std::memset(e, 0xCD, sizeof e);
    auto* te = ::new (static_cast<void*>(e)) UI::TableElement();
    CHECK_EQ(te->_rowCount, 0);
    CHECK_EQ(te->_columnCount, 0);
    CHECK_EQ(te->_spacing, 0);
    CHECK_EQ((int)e[0xcc], 0xCD);
    CHECK(te->_rowSizes._data == nullptr);
    CHECK(te->_columnSizes._data == nullptr);
    CHECK_EQ(te->_columnSizes._capacity, 0);
    te->DeletingDtor(0);

    TableDProbe dp;
    B39CheckDeletingDtorArms<UI::TableDescription, 0x98>(VTableOf(&dp));
    TableEProbe ep;
    B39CheckDeletingDtorArms<UI::TableElement, 0xf0>(VTableOf(&ep));
}
