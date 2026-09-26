// ============================================================================
// tests/unit/ui_leaf_c3_abi_test.cpp — ENDGAME B.3 step 6, block C3
//   Text / Label / Edit / NumberEdit / Number, both sides, as real C++ classes
//
//   UI::TextDescription       : ElementDescription  0x90  10 slots
//   UI::LabelDescription      : TextDescription     0xb8  10 slots
//   UI::EditDescription       : ElementDescription  0x88  10 slots
//   UI::NumberEditDescription : ElementDescription  0x88  10 slots (NOT Edit's)
//   UI::NumberDescription     : ElementDescription  0x88  10 slots
//   UI::TextElement           : Element             0x100 12 slots
//   UI::LabelElement          : TextElement         0x130 12 slots
//   UI::EditElement           : Element             0xf8  13 slots (introduces 12)
//   UI::NumberEditElement     : EditElement         0x100 13 slots
//   UI::NumberElement         : Element             0xe8  12 slots
//
// Built to the contract of ui_element_abi_test.cpp (block A), whose file
// defines `UI::Element` / `UI::ElementDescription`'s out-of-line members as
// markers — that is what makes the base vtables link here. This file defines
// its ten classes' own out-of-line virtuals AS MARKERS the same way
// (runtime/src/ui cannot link into a host test).
//
// PART A — LAYOUT: sizes, alignments, the parents (the `_ParentTypeName`
//   literals), every offset the headers claim, class vs POD.
// PART B — SLOT ORDER: probe subclasses read back BY BYTE OFFSET and raw-called;
//   the marker that runs names the member. Slots a class INHERITS are proved
//   by pointer identity with the parent's probe (their markers live in block
//   A's TU and report to its own channel).
// PART C — ABI: slot 8 `CreateInstance` carries the hidden return slot
//   (B6-2); `EditElement::Update` receives a POINTER to the handle (B6-11).
// PART D — the QUALIFIED call on the slot this block introduces (12).
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/TextDescription.h"
#include "runtime/types/ui/LabelDescription.h"
#include "runtime/types/ui/EditDescription.h"
#include "runtime/types/ui/NumberEditDescription.h"
#include "runtime/types/ui/NumberDescription.h"
#include "runtime/types/ui/TextElement.h"
#include "runtime/types/ui/LabelElement.h"
#include "runtime/types/ui/EditElement.h"
#include "runtime/types/ui/NumberEditElement.h"
#include "runtime/types/ui/NumberElement.h"

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

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
long long   g_lastArg  = 0;

void Note(const void* self, int mark, long long arg = 0) {
    g_lastSelf = self; g_lastMark = mark; g_lastArg = arg;
}
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; g_lastArg = 0; }

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

// Class tags: mark = tag * 100 + slot index.
enum : int {
    kTD = 1, kLD = 2, kED = 3, kNED = 4, kND = 5,
    kTE = 6, kLE = 7, kEE = 8, kNEE = 9, kNE = 10,
};
constexpr int kProbeDtor = 99;
constexpr int kProbeFinalize = 98;

// Byte offsets (B36_SLOTS.md section 1).
enum : int {
    kS0 = 0x00, kS1 = 0x08, kS2 = 0x10, kS3 = 0x18, kS4 = 0x20, kS5 = 0x28,
    kS6 = 0x30, kS7 = 0x38, kS8 = 0x40, kS9 = 0x48, kS10 = 0x50, kS11 = 0x58,
    kS12 = 0x60,
};

// ---- probes: each overrides only slot 0 (pure in every real class) ---------
#define C3_PROBE(NAME, BASE)                                                   \
    struct NAME final : BASE {                                                 \
        Object::Base* DeletingDtor(unsigned flags) override {                  \
            Note(this, kProbeDtor, flags); return this;                        \
        }                                                                      \
    };
C3_PROBE(TextDescriptionProbe,       UI::TextDescription)
C3_PROBE(LabelDescriptionProbe,      UI::LabelDescription)
C3_PROBE(EditDescriptionProbe,       UI::EditDescription)
C3_PROBE(NumberEditDescriptionProbe, UI::NumberEditDescription)
C3_PROBE(NumberDescriptionProbe,     UI::NumberDescription)
C3_PROBE(TextElementProbe,           UI::TextElement)
C3_PROBE(LabelElementProbe,          UI::LabelElement)
C3_PROBE(EditElementProbe,           UI::EditElement)
C3_PROBE(NumberEditElementProbe,     UI::NumberEditElement)
C3_PROBE(NumberElementProbe,         UI::NumberElement)
#undef C3_PROBE

// A probe that overrides the slot EditElement INTRODUCES, for PART D.
struct EditFinalizeProbe final : UI::EditElement {
    Object::Base* DeletingDtor(unsigned flags) override {
        Note(this, kProbeDtor, flags); return this;
    }
    void FinalizeText() override { Note(this, kProbeFinalize); }
};

void* const kRetSentinel = reinterpret_cast<void*>(static_cast<uintptr_t>(0xE1E1E1E1));

}  // namespace

// ===========================================================================
// The ten classes' out-of-line members, AS MARKERS.
// ===========================================================================
namespace UI {

#define C3_DESC_MARKERS(CLS, TAG)                                                     \
    const char* CLS::TypeName() const { Note(this, TAG * 100 + 1); return #CLS; }     \
    uint32_t CLS::Type() const { Note(this, TAG * 100 + 2); return TAG; }             \
    int32_t CLS::GetFileVersion() const { Note(this, TAG * 100 + 3); return TAG; }    \
    bool CLS::Serialize(void* s) {                                                    \
        Note(this, TAG * 100 + 5, reinterpret_cast<intptr_t>(s)); return true;       \
    }                                                                                 \
    Object::External* CLS::Reconstruct() { Note(this, TAG * 100 + 6); return this; }  \
    void CLS::Destruct() { Note(this, TAG * 100 + 7); }                               \
    Object::InternalPtr<Element>* CLS::CreateInstance(                                \
            Object::InternalPtr<Element>* ret, const Dialog* dialog) const {         \
        Note(this, TAG * 100 + 8, reinterpret_cast<intptr_t>(dialog));               \
        *reinterpret_cast<void**>(ret) = kRetSentinel;                               \
        return ret;                                                                   \
    }                                                                                 \
    Object::External* CLS::Construct() { Note(this, TAG * 100 + 9); return this; }    \
    CLS::~CLS() {}

C3_DESC_MARKERS(TextDescription, kTD)
C3_DESC_MARKERS(LabelDescription, kLD)
C3_DESC_MARKERS(EditDescription, kED)
C3_DESC_MARKERS(NumberEditDescription, kNED)
C3_DESC_MARKERS(NumberDescription, kND)
#undef C3_DESC_MARKERS

#define C3_ELEM_TYPE_MARKERS(CLS, TAG)                                                \
    const char* CLS::TypeName() const { Note(this, TAG * 100 + 1); return #CLS; }     \
    uint32_t CLS::Type() const { Note(this, TAG * 100 + 2); return TAG; }             \
    CLS::~CLS() {}
#define C3_ELEM_LAYOUT_MARKERS(CLS, TAG)                                              \
    void CLS::ComputeSize() { Note(this, TAG * 100 + 4); }                            \
    void CLS::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {                 \
        Note(this, TAG * 100 + 5, (long long)x * 1000 + h);                           \
    }                                                                                 \
    void CLS::Draw(SpriteContext&, uint8_t alpha) const { Note(this, TAG * 100 + 6, alpha); }

C3_ELEM_TYPE_MARKERS(TextElement, kTE)
C3_ELEM_LAYOUT_MARKERS(TextElement, kTE)
C3_ELEM_TYPE_MARKERS(LabelElement, kLE)
C3_ELEM_LAYOUT_MARKERS(LabelElement, kLE)
void LabelElement::SetColor(uint32_t color) { Note(this, kLE * 100 + 10, color); }
C3_ELEM_TYPE_MARKERS(EditElement, kEE)
C3_ELEM_LAYOUT_MARKERS(EditElement, kEE)
void EditElement::Update(const System::Time&, UserInterface*,
                         Object::InternalPtr<DialogInstance>* dialog,
                         const UpdateState& state) {
    // The HANDLE the member saw through the pointer, plus mouseX (B6-11).
    Note(this, kEE * 100 + 7,
         static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX);
}
void EditElement::FinalizeText() { Note(this, kEE * 100 + 12); }
C3_ELEM_TYPE_MARKERS(NumberEditElement, kNEE)
void NumberEditElement::FinalizeText() { Note(this, kNEE * 100 + 12); }
C3_ELEM_TYPE_MARKERS(NumberElement, kNE)
C3_ELEM_LAYOUT_MARKERS(NumberElement, kNE)
#undef C3_ELEM_TYPE_MARKERS
#undef C3_ELEM_LAYOUT_MARKERS

}  // namespace UI

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(ui_c3_sizes_and_parents_match_retail) {
    // `MOV EDX,imm` in each class's slot-0 vector deleting destructor.
    CHECK_EQ(sizeof(UI::TextDescription),       (size_t)0x90);
    CHECK_EQ(sizeof(UI::LabelDescription),      (size_t)0xb8);
    CHECK_EQ(sizeof(UI::EditDescription),       (size_t)0x88);
    CHECK_EQ(sizeof(UI::NumberEditDescription), (size_t)0x88);
    CHECK_EQ(sizeof(UI::NumberDescription),     (size_t)0x88);
    CHECK_EQ(sizeof(UI::TextElement),           (size_t)0x100);
    CHECK_EQ(sizeof(UI::LabelElement),          (size_t)0x130);
    CHECK_EQ(sizeof(UI::EditElement),           (size_t)0xf8);    // NOT 0xe8
    CHECK_EQ(sizeof(UI::NumberEditElement),     (size_t)0x100);
    CHECK_EQ(sizeof(UI::NumberElement),         (size_t)0xe8);
    CHECK_EQ(alignof(UI::EditElement), (size_t)8);
    CHECK_EQ(alignof(UI::LabelElement), (size_t)8);
    CHECK_EQ(alignof(UI::NumberDescription), (size_t)8);

    // The parents, as the `_ParentTypeName` literals say.
    CHECK((std::is_base_of<UI::ElementDescription, UI::TextDescription>::value));
    CHECK((std::is_base_of<UI::TextDescription, UI::LabelDescription>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::EditDescription>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::NumberEditDescription>::value));
    CHECK(!(std::is_base_of<UI::EditDescription, UI::NumberEditDescription>::value));
    CHECK((std::is_base_of<UI::ElementDescription, UI::NumberDescription>::value));
    CHECK((std::is_base_of<UI::Element, UI::TextElement>::value));
    CHECK((std::is_base_of<UI::TextElement, UI::LabelElement>::value));
    CHECK((std::is_base_of<UI::Element, UI::EditElement>::value));
    CHECK((std::is_base_of<UI::EditElement, UI::NumberEditElement>::value));
    CHECK((std::is_base_of<UI::Element, UI::NumberElement>::value));
    CHECK(!(std::is_base_of<UI::TextElement, UI::NumberElement>::value));

    // Abstract, no virtual destructor (ENDGAME decision 2).
    CHECK(std::is_abstract<UI::TextDescription>::value);
    CHECK(std::is_abstract<UI::LabelElement>::value);
    CHECK(std::is_abstract<UI::NumberEditElement>::value);
    CHECK(!std::has_virtual_destructor<UI::TextElement>::value);
    CHECK(!std::has_virtual_destructor<UI::NumberEditDescription>::value);
}

UNIT_TEST(ui_c3_field_offsets_and_pod_agreement) {
    // EditElement's sixteen bytes past +0xe7 (ctor 0x1800d0f10).
    CHECK_EQ(offsetof(UI::EditElement, _active),           (size_t)0xe0);
    CHECK_EQ(offsetof(UI::EditElement, _caret),            (size_t)0xe4);
    CHECK_EQ(offsetof(UI::EditElement, _blinkOn),          (size_t)0xe8);
    CHECK_EQ(offsetof(UI::EditElement, _blinkAccumulator), (size_t)0xf0);
    CHECK_EQ(offsetof(UI::NumberEditElement, _min), sizeof(UI::EditElement));
    CHECK_EQ(offsetof(UI::NumberEditElement, _max), (size_t)0xfc);
    CHECK_EQ(sizeof(RtUI::EditElement), (size_t)0xf8);
    CHECK_EQ(offsetof(RtUI::NumberEditElement, _min), (size_t)0xf8);

    // TextElement (ctor 0x18011eb80), under TextElement's names.
    CHECK_EQ(offsetof(UI::TextElement, _font),        (size_t)0xc0);
    CHECK_EQ(offsetof(UI::TextElement, _params),      (size_t)0xc8);
    CHECK_EQ(offsetof(UI::TextElement, _text),        (size_t)0xd8);
    CHECK_EQ(offsetof(UI::TextElement, _drawShadow),  (size_t)0xe8);
    CHECK_EQ(offsetof(UI::TextElement, _lineSpacing), (size_t)0xec);
    CHECK_EQ(offsetof(UI::TextElement, _cachedText),  (size_t)0xf8);

    // LabelElement inherits those names; its own fields (ctor 0x1800d2620).
    CHECK_EQ(offsetof(UI::LabelElement, _cachedText), (size_t)0xf8);
    CHECK_EQ(offsetof(UI::LabelElement, _sprite),     (size_t)0x100);
    CHECK_EQ(offsetof(UI::LabelElement, _align),      (size_t)0x110);
    CHECK_EQ(offsetof(UI::LabelElement, _mode),       (size_t)0x114);
    CHECK_EQ(offsetof(UI::LabelElement, _iconColor),  (size_t)0x128);
    CHECK_EQ(offsetof(UI::LabelElement, _textWidth),  (size_t)0x12c);
    CHECK_EQ(sizeof(UI::LabelElement), sizeof(RtUI::LabelElement));

    CHECK_EQ(offsetof(UI::NumberElement, _cachedText), (size_t)0xd0);
    CHECK_EQ(offsetof(UI::NumberElement, _showSign),   (size_t)0xe0);

    CHECK_EQ(offsetof(UI::TextDescription, _name),        (size_t)0x78);
    CHECK_EQ(offsetof(UI::TextDescription, _i8c),         (size_t)0x8c);
    CHECK_EQ(offsetof(UI::LabelDescription, _spriteName), (size_t)0xb0);
    CHECK_EQ(offsetof(UI::LabelDescription, _spriteName),
             offsetof(RtUI::LabelDescription, _spriteName));
    CHECK_EQ(offsetof(UI::EditDescription, _string80),
             offsetof(RtUI::EditDescription, _string80));
    CHECK_EQ(offsetof(UI::NumberEditDescription, _b80), (size_t)0x80);
    CHECK_EQ(offsetof(UI::NumberDescription, _b7c),     (size_t)0x7c);
}

// ===========================================================================
// PART B — SLOT ORDER
// ===========================================================================
namespace {

// Slots 1,2,3,5,6,7,8,9 of a leaf Description reach the class's own markers,
// slot 0 the probe, slot 4 `Object::External::Compile` (inherited).
void CheckDescriptionSlots(void* raw, int tag, const void* compileEntry) {
    Reset(); SlotAt<void* (*)(void*, unsigned)>(raw, kS0)(raw, 0u);
    CHECK_EQ(g_lastMark, kProbeDtor); CHECK(g_lastSelf == raw);
    Reset(); SlotAt<const char* (*)(const void*)>(raw, kS1)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(raw, kS2)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 2);
    Reset(); SlotAt<int32_t (*)(const void*)>(raw, kS3)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 3);
    CHECK(VTableOf(raw)[kS4 / 8] == compileEntry);
    Reset(); SlotAt<bool (*)(void*, void*)>(raw, kS5)(raw, (void*)0x55);
    CHECK_EQ(g_lastMark, tag * 100 + 5); CHECK_EQ(g_lastArg, 0x55);
    Reset(); SlotAt<void* (*)(void*)>(raw, kS6)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 6);
    Reset(); SlotAt<void (*)(void*)>(raw, kS7)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 7);
    void* slot = nullptr;
    Reset(); SlotAt<void** (*)(const void*, void**, const void*)>(raw, kS8)(raw, &slot, nullptr);
    CHECK_EQ(g_lastMark, tag * 100 + 8);
    // RULE B6-1.1: Construct is slot 9, BEHIND CreateInstance.
    Reset(); SlotAt<void* (*)(void*)>(raw, kS9)(raw);
    CHECK_EQ(g_lastMark, tag * 100 + 9);
}

}  // namespace

UNIT_TEST(ui_c3_description_slot_order_is_retails) {
    TextDescriptionProbe td;
    LabelDescriptionProbe ld;
    EditDescriptionProbe ed;
    NumberEditDescriptionProbe ned;
    NumberDescriptionProbe nd;

    // Every one inherits the same External::Compile at slot 4 (B6-1.5).
    const void* compile = VTableOf(&td)[kS4 / 8];
    CheckDescriptionSlots(&td, kTD, compile);
    CheckDescriptionSlots(&ld, kLD, compile);
    CheckDescriptionSlots(&ed, kED, compile);
    CheckDescriptionSlots(&ned, kNED, compile);
    CheckDescriptionSlots(&nd, kND, compile);

    // Label OVERRIDES all of Text's 1..9 but 4: none of its entries is Text's.
    for (int s = 1; s < 10; ++s) {
        if (s == 4) continue;
        CHECK(VTableOf(&ld)[s] != VTableOf(&td)[s]);
    }
    CHECK(static_cast<void*>(static_cast<UI::TextDescription*>(&ld)) == static_cast<void*>(&ld));
}

UNIT_TEST(ui_c3_text_and_label_element_slot_order_is_retails) {
    FactoryScope scope;
    TextElementProbe te{};
    LabelElementProbe le{};
    void* t = static_cast<void*>(&te);
    void* l = static_cast<void*>(&le);

    Reset(); SlotAt<const char* (*)(const void*)>(t, kS1)(t); CHECK_EQ(g_lastMark, kTE * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(t, kS2)(t);    CHECK_EQ(g_lastMark, kTE * 100 + 2);
    Reset(); SlotAt<void (*)(void*)>(t, kS4)(t);              CHECK_EQ(g_lastMark, kTE * 100 + 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(t, kS5)(t, 7, 0, 0, 9);
    CHECK_EQ(g_lastMark, kTE * 100 + 5); CHECK_EQ(g_lastArg, 7009);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(t, kS6)(t, nullptr, 200);
    CHECK_EQ(g_lastMark, kTE * 100 + 6); CHECK_EQ(g_lastArg, 200);

    // Label overrides 1,2,4,5,6 and SetColor (10).
    Reset(); SlotAt<const char* (*)(const void*)>(l, kS1)(l); CHECK_EQ(g_lastMark, kLE * 100 + 1);
    Reset(); SlotAt<void (*)(void*)>(l, kS4)(l);              CHECK_EQ(g_lastMark, kLE * 100 + 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(l, kS5)(l, 1, 0, 0, 2);
    CHECK_EQ(g_lastMark, kLE * 100 + 5);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(l, kS6)(l, nullptr, 9);
    CHECK_EQ(g_lastMark, kLE * 100 + 6);
    Reset(); SlotAt<void (*)(void*, uint32_t)>(l, kS10)(l, 0xABCDu);
    CHECK_EQ(g_lastMark, kLE * 100 + 10); CHECK_EQ(g_lastArg, 0xABCD);

    // ...and INHERITS 3, 7, 8, 9, 11 through TextElement (Element's bodies).
    const int inherited[] = { 3, 7, 8, 9, 11 };
    for (int s : inherited) CHECK(VTableOf(l)[s] == VTableOf(t)[s]);
    // Text does NOT override SetColor; Label does.
    CHECK(VTableOf(l)[10] != VTableOf(t)[10]);
}

UNIT_TEST(ui_c3_edit_and_numberedit_element_slot_order_is_retails) {
    FactoryScope scope;
    EditElementProbe ee{};
    NumberEditElementProbe ne{};
    TextElementProbe te{};
    void* e = static_cast<void*>(&ee);
    void* n = static_cast<void*>(&ne);

    Reset(); SlotAt<const char* (*)(const void*)>(e, kS1)(e); CHECK_EQ(g_lastMark, kEE * 100 + 1);
    Reset(); SlotAt<void (*)(void*)>(e, kS4)(e);              CHECK_EQ(g_lastMark, kEE * 100 + 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(e, kS5)(e, 3, 0, 0, 4);
    CHECK_EQ(g_lastMark, kEE * 100 + 5);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(e, kS6)(e, nullptr, 1);
    CHECK_EQ(g_lastMark, kEE * 100 + 6);
    // SLOT 12 — introduced by EditElement (B6-1.6): thirteen slots.
    Reset(); SlotAt<void (*)(void*)>(e, kS12)(e);             CHECK_EQ(g_lastMark, kEE * 100 + 12);

    // NumberEdit: own 1, 2 and 12; INHERITS 4..7 from EditElement.
    Reset(); SlotAt<const char* (*)(const void*)>(n, kS1)(n); CHECK_EQ(g_lastMark, kNEE * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(n, kS2)(n);    CHECK_EQ(g_lastMark, kNEE * 100 + 2);
    Reset(); SlotAt<void (*)(void*)>(n, kS12)(n);             CHECK_EQ(g_lastMark, kNEE * 100 + 12);
    for (int s = 3; s <= 11; ++s) CHECK(VTableOf(n)[s] == VTableOf(e)[s]);
    Reset(); SlotAt<void (*)(void*)>(n, kS4)(n);              CHECK_EQ(g_lastMark, kEE * 100 + 4);

    // Edit overrides Update (7); Text inherits Element's.
    CHECK(VTableOf(e)[7] != VTableOf(&te)[7]);
    CHECK(VTableOf(e)[8] == VTableOf(&te)[8]);
}

UNIT_TEST(ui_c3_number_element_slot_order_is_retails) {
    FactoryScope scope;
    NumberElementProbe np{};
    TextElementProbe te{};
    void* n = static_cast<void*>(&np);
    Reset(); SlotAt<const char* (*)(const void*)>(n, kS1)(n); CHECK_EQ(g_lastMark, kNE * 100 + 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(n, kS2)(n);    CHECK_EQ(g_lastMark, kNE * 100 + 2);
    Reset(); SlotAt<void (*)(void*)>(n, kS4)(n);              CHECK_EQ(g_lastMark, kNE * 100 + 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(n, kS5)(n, 5, 0, 0, 6);
    CHECK_EQ(g_lastMark, kNE * 100 + 5); CHECK_EQ(g_lastArg, 5006);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(n, kS6)(n, nullptr, 3);
    CHECK_EQ(g_lastMark, kNE * 100 + 6);
    // 7..11 are Element's, shared with TextElement.
    for (int s = 7; s <= 11; ++s) CHECK(VTableOf(n)[s] == VTableOf(&te)[s]);
}

// ===========================================================================
// PART C — ABI of the class-valued arguments / returns
// ===========================================================================
UNIT_TEST(ui_c3_slot8_createinstance_carries_the_hidden_return_slot) {
    LabelDescriptionProbe probe;
    void* raw = static_cast<void*>(&probe);
    const void* const* vtBefore = VTableOf(raw);

    void* slot = nullptr;
    const void* dialog = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xD1A1));
    Reset();
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(raw, kS8)(raw, &slot, dialog);
    CHECK_EQ(g_lastMark, kLD * 100 + 8);
    CHECK(g_lastSelf == raw);
    CHECK_EQ(g_lastArg, 0xD1A1);
    CHECK(back == &slot);
    CHECK(slot == kRetSentinel);
    CHECK(VTableOf(raw) == vtBefore);
}

UNIT_TEST(ui_c3_editelement_update_receives_a_pointer_to_the_handle) {
    FactoryScope scope;
    EditElementProbe probe{};
    void* raw = static_cast<void*>(&probe);

    // Retail order: RCX this, RDX time, R8 ui, R9 = &handle, [RSP+0x28] state.
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x4000));
    UI::Element::UpdateState state;
    state.mouseX = 5;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(raw, kS7)(
        raw, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_lastMark, kEE * 100 + 7);
    CHECK_EQ(g_lastArg, 0x4000 + 5);
}

// ===========================================================================
// PART D — the QUALIFIED call on slot 12
// ===========================================================================
UNIT_TEST(ui_c3_qualified_finalizetext_does_not_redispatch) {
    FactoryScope scope;
    EditFinalizeProbe probe{};
    UI::EditElement* e = &probe;

    Reset(); e->FinalizeText();                        // virtual: the override
    CHECK_EQ(g_lastMark, kProbeFinalize);
    Reset(); e->UI::EditElement::FinalizeText();       // qualified: the base body
    CHECK_EQ(g_lastMark, kEE * 100 + 12);
}
