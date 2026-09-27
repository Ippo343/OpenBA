// ============================================================================
// tests/unit/ui_leaf_c2_abi_test.cpp — ENDGAME B.3 step 6, block C2
//   the ten Group / Page / Radio / Image / AnimatedImage leaf classes
//
//   UI::GroupDescription         : ElementDescription  0x78  10 slots
//   UI::PageDescription          : GroupDescription    0x80  10 slots
//   UI::RadioDescription         : GroupDescription    0x80  10 slots
//   UI::GroupElement             : Element             0xc0  12 slots
//   UI::PageElement              : GroupElement        0xc8  12 slots
//   UI::RadioElement             : GroupElement        0xd8  12 slots
//   UI::ImageDescription         : ElementDescription  0x80  10 slots
//   UI::AnimatedImageDescription : ImageDescription    0x98  10 slots
//   UI::ImageElement             : Element             0xe0  12 slots
//   UI::AnimatedImageElement     : ImageElement        0x100 12 slots
//
// Built to the contract of ui_element_abi_test.cpp (block A); only what
// differs is spelled out.
//
// PART A — LAYOUT: sizeof / alignof / every offset the headers claim, the
//   parent edges `_ParentTypeName` proves, no virtual destructor, abstract.
// PART B — SLOT ORDER: probe subclasses of the REAL classes; each OWN slot is
//   raw-called by byte offset and the marker that runs names the member; each
//   INHERITED slot is compared, pointer for pointer, with the parent probe's
//   entry (the `inherited-from` markers of B36_SLOTS.md section 1).
// PART C — THE RETSLOT SLOT: Description slot 8 `CreateInstance` raw-called the
//   retail way `(this, ret, dialog)` (RULE B6-2).
// PART D — THE QUALIFIED CALL: a door reached through a minted table delegates
//   `->UI::X::M(…)`, and X::M's own call into its PARENT's body is qualified
//   too — neither re-enters the door.
//
// `UI::Element` / `UI::ElementDescription`'s own out-of-line members are the
// MARKERS ui_element_abi_test.cpp defines; this file defines the ten leaves'
// members as its own markers (runtime/src/ui does not link into a host test).
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/Element.h"
#include "runtime/types/ui/ElementDescription.h"
#include "runtime/types/ui/GroupDescription.h"
#include "runtime/types/ui/GroupElement.h"
#include "runtime/types/ui/PageDescription.h"
#include "runtime/types/ui/PageElement.h"
#include "runtime/types/ui/RadioDescription.h"
#include "runtime/types/ui/RadioElement.h"
#include "runtime/types/ui/ImageDescription.h"
#include "runtime/types/ui/ImageElement.h"
#include "runtime/types/ui/AnimatedImageDescription.h"
#include "runtime/types/ui/AnimatedImageElement.h"

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
enum Who : int {
    kNone = 0, kGroupD, kPageD, kRadioD, kGroupE, kPageE, kRadioE,
    kImageD, kAnimD, kImageE, kAnimE,
};

const void* g_self = nullptr;
int         g_who  = kNone;
int         g_slot = -1;
long long   g_arg  = 0;
int         g_trail[8];
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
struct BareElementProbe final : UI::Element {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    void ComputeSize() override {}
};
struct GroupElementProbe final : UI::GroupElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct PageElementProbe final : UI::PageElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct RadioElementProbe final : UI::RadioElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct GroupDescriptionProbe final : UI::GroupDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct PageDescriptionProbe final : UI::PageDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct RadioDescriptionProbe final : UI::RadioDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};

struct ImageElementProbe final : UI::ImageElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct AnimatedImageElementProbe final : UI::AnimatedImageElement {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct ImageDescriptionProbe final : UI::ImageDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct AnimatedImageDescriptionProbe final : UI::AnimatedImageDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};

struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

Object::InternalPtr<UI::Element>* WriteMarkerHandle(Object::InternalPtr<UI::Element>* ret,
                                                    uintptr_t tag) {
    new (static_cast<void*>(ret)) Object::InternalPtr<UI::Element>(
        reinterpret_cast<UI::Element*>(tag));
    return ret;
}

}  // namespace

// ENDGAME B.3.9 UI-A: the destructor markers RECORD each call, so the real
// `DeletingDtor` of a concrete class (inline in its header) can be seen
// running ~X once per element, in retail's DESCENDING order.
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

// ---- GroupElement ----
const char* GroupElement::TypeName() const { Note(this, kGroupE, 1); return "GroupElement"; }
uint32_t GroupElement::Type() const { Note(this, kGroupE, 2); return 0x6Eu; }
void GroupElement::ComputeSize() { Note(this, kGroupE, 4); }
void GroupElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kGroupE, 5, (long long)x * 1000 + h);
}
void GroupElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kGroupE, 6, alpha); }
GroupElement::~GroupElement() { B39NoteDtor(this); }

// ---- PageElement ----
const char* PageElement::TypeName() const { Note(this, kPageE, 1); return "PageElement"; }
uint32_t PageElement::Type() const { Note(this, kPageE, 2); return 0x9Eu; }
PageElement::~PageElement() { B39NoteDtor(this); }

// ---- RadioElement — Update calls its parent's (Element's) body QUALIFIED.
const char* RadioElement::TypeName() const { Note(this, kRadioE, 1); return "RadioElement"; }
uint32_t RadioElement::Type() const { Note(this, kRadioE, 2); return 0xAEu; }
void RadioElement::Update(const System::Time&, UserInterface*,
                          Object::InternalPtr<DialogInstance>* dialog, const UpdateState& state) {
    Note(this, kRadioE, 7,
         static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX);
}
RadioElement::~RadioElement() { B39NoteDtor(this); }

// ---- GroupDescription ----
const char* GroupDescription::TypeName() const { Note(this, kGroupD, 1); return "GroupDescription"; }
uint32_t GroupDescription::Type() const { Note(this, kGroupD, 2); return 0x6Du; }
int32_t GroupDescription::GetFileVersion() const { Note(this, kGroupD, 3); return 0x104; }
bool GroupDescription::Serialize(void* stream) {
    Note(this, kGroupD, 5, reinterpret_cast<intptr_t>(stream)); return true;
}
Object::External* GroupDescription::Reconstruct() { Note(this, kGroupD, 6); return this; }
void GroupDescription::Destruct() { Note(this, kGroupD, 7); }
Object::InternalPtr<Element>* GroupDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kGroupD, 8, reinterpret_cast<intptr_t>(dialog));
    return WriteMarkerHandle(ret, 0x6D6D6D6D);
}
Object::External* GroupDescription::Construct() { Note(this, kGroupD, 9); return this; }
GroupDescription::~GroupDescription() { B39NoteDtor(this); }

// ---- PageDescription — Serialize calls the PARENT's body QUALIFIED, as the real one does.
const char* PageDescription::TypeName() const { Note(this, kPageD, 1); return "PageDescription"; }
uint32_t PageDescription::Type() const { Note(this, kPageD, 2); return 0x9Du; }
int32_t PageDescription::GetFileVersion() const { Note(this, kPageD, 3); return 0x1104; }
bool PageDescription::Serialize(void* stream) {
    Note(this, kPageD, 5, reinterpret_cast<intptr_t>(stream));
    return GroupDescription::Serialize(stream);
}
Object::External* PageDescription::Reconstruct() { Note(this, kPageD, 6); return this; }
void PageDescription::Destruct() { Note(this, kPageD, 7); }
Object::InternalPtr<Element>* PageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kPageD, 8, reinterpret_cast<intptr_t>(dialog));
    return WriteMarkerHandle(ret, 0x9D9D9D9D);
}
Object::External* PageDescription::Construct() { Note(this, kPageD, 9); return this; }
PageDescription::~PageDescription() { B39NoteDtor(this); }

// ---- RadioDescription ----
const char* RadioDescription::TypeName() const { Note(this, kRadioD, 1); return "RadioDescription"; }
uint32_t RadioDescription::Type() const { Note(this, kRadioD, 2); return 0xADu; }
int32_t RadioDescription::GetFileVersion() const { Note(this, kRadioD, 3); return 0x1104; }
bool RadioDescription::Serialize(void* stream) {
    Note(this, kRadioD, 5, reinterpret_cast<intptr_t>(stream));
    return GroupDescription::Serialize(stream);
}
Object::External* RadioDescription::Reconstruct() { Note(this, kRadioD, 6); return this; }
void RadioDescription::Destruct() { Note(this, kRadioD, 7); }
Object::InternalPtr<Element>* RadioDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kRadioD, 8, reinterpret_cast<intptr_t>(dialog));
    return WriteMarkerHandle(ret, 0xADADADAD);
}
Object::External* RadioDescription::Construct() { Note(this, kRadioD, 9); return this; }
RadioDescription::~RadioDescription() { B39NoteDtor(this); }

// ---- ImageElement ----
const char* ImageElement::TypeName() const { Note(this, kImageE, 1); return "ImageElement"; }
uint32_t ImageElement::Type() const { Note(this, kImageE, 2); return 0x1Eu; }
void ImageElement::ComputeSize() { Note(this, kImageE, 4); }
void ImageElement::ComputeLayout(int32_t x, int32_t, int32_t, int32_t h) {
    Note(this, kImageE, 5, (long long)x * 1000 + h);
}
void ImageElement::Draw(SpriteContext&, uint8_t alpha) const { Note(this, kImageE, 6, alpha); }
ImageElement::~ImageElement() { B39NoteDtor(this); }

// ---- AnimatedImageElement ----
const char* AnimatedImageElement::TypeName() const { Note(this, kAnimE, 1); return "AnimatedImageElement"; }
uint32_t AnimatedImageElement::Type() const { Note(this, kAnimE, 2); return 0xA1u; }
void AnimatedImageElement::Update(const System::Time&, UserInterface*,
                                  Object::InternalPtr<DialogInstance>* dialog,
                                  const UpdateState& state) {
    Note(this, kAnimE, 7,
         static_cast<long long>(reinterpret_cast<intptr_t>(dialog->Get())) + state.mouseX);
}
AnimatedImageElement::~AnimatedImageElement() { B39NoteDtor(this); }

// ---- ImageDescription (+ the non-virtual applier, marker slot 80) ----
const char* ImageDescription::TypeName() const { Note(this, kImageD, 1); return "ImageDescription"; }
uint32_t ImageDescription::Type() const { Note(this, kImageD, 2); return 0x1Du; }
int32_t ImageDescription::GetFileVersion() const { Note(this, kImageD, 3); return 0x204; }
bool ImageDescription::Serialize(void* stream) {
    Note(this, kImageD, 5, reinterpret_cast<intptr_t>(stream)); return true;
}
Object::External* ImageDescription::Reconstruct() { Note(this, kImageD, 6); return this; }
void ImageDescription::Destruct() { Note(this, kImageD, 7); }
Object::InternalPtr<Element>* ImageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kImageD, 8, reinterpret_cast<intptr_t>(dialog));
    return WriteMarkerHandle(ret, 0x1D1D1D1D);
}
void ImageDescription::CreateInstance(Object::InternalPtr<ImageElement>* element) const {
    Note(this, kImageD, 80, reinterpret_cast<intptr_t>(element->Get()));
}
Object::External* ImageDescription::Construct() { Note(this, kImageD, 9); return this; }
ImageDescription::~ImageDescription() { B39NoteDtor(this); }

// ---- AnimatedImageDescription — Serialize and slot 8 call the parent QUALIFIED.
const char* AnimatedImageDescription::TypeName() const { Note(this, kAnimD, 1); return "AnimatedImageDescription"; }
uint32_t AnimatedImageDescription::Type() const { Note(this, kAnimD, 2); return 0xADu; }
int32_t AnimatedImageDescription::GetFileVersion() const { Note(this, kAnimD, 3); return 0x10204; }
bool AnimatedImageDescription::Serialize(void* stream) {
    Note(this, kAnimD, 5, reinterpret_cast<intptr_t>(stream));
    return ImageDescription::Serialize(stream);
}
Object::External* AnimatedImageDescription::Reconstruct() { Note(this, kAnimD, 6); return this; }
void AnimatedImageDescription::Destruct() { Note(this, kAnimD, 7); }
Object::InternalPtr<Element>* AnimatedImageDescription::CreateInstance(
        Object::InternalPtr<Element>* ret, const Dialog* dialog) const {
    Note(this, kAnimD, 8, reinterpret_cast<intptr_t>(dialog));
    // The name-hidden applier, reached QUALIFIED as the real body does.
    Object::InternalPtr<ImageElement> slot(
        reinterpret_cast<ImageElement*>(static_cast<uintptr_t>(0xA1A1A1A1)));
    ImageDescription::CreateInstance(&slot);
    return WriteMarkerHandle(ret, 0xA1A1A1A1);
}
Object::External* AnimatedImageDescription::Construct() { Note(this, kAnimD, 9); return this; }
AnimatedImageDescription::~AnimatedImageDescription() { B39NoteDtor(this); }

}  // namespace UI

namespace {

// Raw-call every Description slot a leaf owns (1,2,3,5,6,7,8,9) and check the
// marker names the expected class and slot.
void CheckDescriptionOwnSlots(void* raw, int who) {
    Reset(); SlotAt<const char* (*)(const void*)>(raw, kD1_TypeName)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 1); CHECK(g_self == raw);
    Reset(); SlotAt<uint32_t (*)(const void*)>(raw, kD2_Type)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 2);
    Reset(); SlotAt<int32_t (*)(const void*)>(raw, kD3_GetFileVersion)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 3);
    Reset(); SlotAt<bool (*)(void*, void*)>(raw, kD5_Serialize)(raw, (void*)0x55);
    CHECK_EQ(g_trail[0], who * 100 + 5); CHECK_EQ(g_arg, 0x55);
    Reset(); SlotAt<void* (*)(void*)>(raw, kD6_Reconstruct)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 6);
    Reset(); SlotAt<void (*)(void*)>(raw, kD7_Destruct)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 7);
    Reset(); SlotAt<void* (*)(void*)>(raw, kD9_Construct)(raw);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 9);

    // Slot 4 is `Object::External::Compile`, INHERITED (RULE B6-1.5).
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kD4_Compile / 8] ==
          VTableOf(static_cast<void*>(&bare))[kD4_Compile / 8]);
}

// Slot 8 raw-called the retail way: RCX = this, RDX = the caller's storage,
// R8 = the Dialog*, the storage handed back in RAX, `this` untouched.
void CheckSlot8Retslot(void* raw, int who, uintptr_t tag) {
    const void* const* vtBefore = VTableOf(raw);
    void* slot = nullptr;
    const void* dialog = reinterpret_cast<const void*>(static_cast<uintptr_t>(0xD1A1));
    Reset();
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(
        raw, kD8_CreateInstance)(raw, &slot, dialog);
    CHECK_EQ(g_who, who); CHECK_EQ(g_slot, 8);
    CHECK(g_self == raw);
    CHECK_EQ(g_arg, 0xD1A1);
    CHECK(back == &slot);
    CHECK(slot == reinterpret_cast<void*>(tag));
    CHECK(VTableOf(raw) == vtBefore);
}

}  // namespace

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(ui_leaf_c2_group_chain_sizes_match_retail) {
    // `MOV EDX,imm` in each class's own slot-0 vector deleting destructor.
    CHECK_EQ(sizeof(UI::GroupDescription), (size_t)0x78);   // 0x180046af0
    CHECK_EQ(sizeof(UI::PageDescription),  (size_t)0x80);   // 0x1800471a0
    CHECK_EQ(sizeof(UI::RadioDescription), (size_t)0x80);   // 0x180047930
    CHECK_EQ(sizeof(UI::GroupElement),     (size_t)0xc0);   // 0x180046e10
    CHECK_EQ(sizeof(UI::PageElement),      (size_t)0xc8);   // 0x180047540
    CHECK_EQ(sizeof(UI::RadioElement),     (size_t)0xd8);   // 0x180047d50
    CHECK_EQ(alignof(UI::GroupDescription), (size_t)8);
    CHECK_EQ(alignof(UI::PageDescription),  (size_t)8);
    CHECK_EQ(alignof(UI::RadioDescription), (size_t)8);
    CHECK_EQ(alignof(UI::GroupElement),     (size_t)8);
    CHECK_EQ(alignof(UI::PageElement),      (size_t)8);
    CHECK_EQ(alignof(UI::RadioElement),     (size_t)8);

    CHECK_EQ(offsetof(UI::GroupDescription, _children),    (size_t)0x68);
    CHECK_EQ(offsetof(UI::PageDescription,  _activePage),  (size_t)0x78);
    CHECK_EQ(offsetof(UI::RadioDescription, _activeRadio), (size_t)0x78);
    CHECK_EQ(offsetof(UI::PageElement,  _activePage),      (size_t)0xc0);
    CHECK_EQ(offsetof(UI::RadioElement, _checkBoxes),      (size_t)0xc0);
    CHECK_EQ(offsetof(UI::RadioElement, _activeIndex),     (size_t)0xd0);

    // The parent edges, from the `_ParentTypeName` literals.
    CHECK((std::is_base_of<UI::ElementDescription, UI::GroupDescription>::value));
    CHECK((std::is_base_of<UI::GroupDescription,   UI::PageDescription>::value));
    CHECK((std::is_base_of<UI::GroupDescription,   UI::RadioDescription>::value));
    CHECK((std::is_base_of<UI::Element,      UI::GroupElement>::value));
    CHECK((std::is_base_of<UI::GroupElement, UI::PageElement>::value));
    CHECK((std::is_base_of<UI::GroupElement, UI::RadioElement>::value));

    CHECK(!std::is_abstract<UI::GroupDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::GroupDescription>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::PageDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::PageDescription>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::RadioDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::RadioDescription>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::GroupElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::GroupElement>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::PageElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::PageElement>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::RadioElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::RadioElement>::value);  // protected non-virtual dtor
    CHECK(!std::has_virtual_destructor<UI::GroupDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::PageElement>::value);
    CHECK(!std::has_virtual_destructor<UI::RadioElement>::value);

    // PageElementOffsets (widgets_layout.cpp's view) agrees with the class.
    CHECK_EQ((size_t)UI::PageElementOffsets::kActivePage, offsetof(UI::PageElement, _activePage));
}

// ===========================================================================
// PART B — SLOT ORDER
// ===========================================================================
UNIT_TEST(ui_leaf_c2_group_descriptions_slot_order) {
    GroupDescriptionProbe group;
    PageDescriptionProbe  page;
    RadioDescriptionProbe radio;
    CheckDescriptionOwnSlots(static_cast<void*>(&group), kGroupD);
    CheckDescriptionOwnSlots(static_cast<void*>(&page),  kPageD);
    CheckDescriptionOwnSlots(static_cast<void*>(&radio), kRadioD);

    // Page and Radio OVERRIDE every one of 1..9 except Compile (B36_SLOTS.md:
    // no `inherited-from` marker besides slot 4).
    const void* const* g = VTableOf(static_cast<void*>(&group));
    const void* const* p = VTableOf(static_cast<void*>(&page));
    const void* const* r = VTableOf(static_cast<void*>(&radio));
    for (int i = 1; i <= 9; ++i) {
        if (i == kD4_Compile / 8) { CHECK(p[i] == g[i]); CHECK(r[i] == g[i]); continue; }
        CHECK(p[i] != g[i]);
        CHECK(r[i] != g[i]);
    }
}

UNIT_TEST(ui_leaf_c2_group_elements_slot_order) {
    FactoryScope scope;
    BareElementProbe  base{};
    GroupElementProbe group{};
    PageElementProbe  page{};
    RadioElementProbe radio{};
    void* g = static_cast<void*>(&group);

    Reset(); SlotAt<const char* (*)(const void*)>(g, kE1_TypeName)(g);
    CHECK_EQ(g_who, (int)kGroupE); CHECK_EQ(g_slot, 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(g, kE2_Type)(g);
    CHECK_EQ(g_who, (int)kGroupE); CHECK_EQ(g_slot, 2);
    Reset(); SlotAt<void (*)(void*)>(g, kE4_ComputeSize)(g);
    CHECK_EQ(g_who, (int)kGroupE); CHECK_EQ(g_slot, 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(g, kE5_ComputeLayout)(g, 7, 0, 0, 9);
    CHECK_EQ(g_who, (int)kGroupE); CHECK_EQ(g_slot, 5); CHECK_EQ(g_arg, 7009);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(g, kE6_Draw)(g, nullptr, 200);
    CHECK_EQ(g_who, (int)kGroupE); CHECK_EQ(g_slot, 6); CHECK_EQ(g_arg, 200);

    // GroupElement inherits 3 and 7..11 from Element.
    const void* const* vb = VTableOf(static_cast<void*>(&base));
    const void* const* vg = VTableOf(g);
    const void* const* vp = VTableOf(static_cast<void*>(&page));
    const void* const* vr = VTableOf(static_cast<void*>(&radio));
    for (int i : {3, 7, 8, 9, 10, 11}) CHECK(vg[i] == vb[i]);
    for (int i : {4, 5, 6}) CHECK(vg[i] != vb[i]);

    // Page inherits ALL of GroupElement's 3..11; Radio all but 7 (Update).
    for (int i = 3; i <= 11; ++i) CHECK(vp[i] == vg[i]);
    for (int i = 3; i <= 11; ++i) {
        if (i == kE7_Update / 8) CHECK(vr[i] != vg[i]);
        else                     CHECK(vr[i] == vg[i]);
    }

    // Their own 1 and 2.
    void* p = static_cast<void*>(&page);
    void* r = static_cast<void*>(&radio);
    Reset(); SlotAt<const char* (*)(const void*)>(p, kE1_TypeName)(p);
    CHECK_EQ(g_who, (int)kPageE); CHECK_EQ(g_slot, 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(p, kE2_Type)(p);
    CHECK_EQ(g_who, (int)kPageE); CHECK_EQ(g_slot, 2);
    Reset(); SlotAt<const char* (*)(const void*)>(r, kE1_TypeName)(r);
    CHECK_EQ(g_who, (int)kRadioE); CHECK_EQ(g_slot, 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(r, kE2_Type)(r);
    CHECK_EQ(g_who, (int)kRadioE); CHECK_EQ(g_slot, 2);

    // Radio's slot 7 receives the handle BY POINTER (RULE B6-11), retail's
    // `LEA R9,[…]` shape.
    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x7000));
    UI::Element::UpdateState state;
    state.mouseX = 5;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(r, kE7_Update)(
        r, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_who, (int)kRadioE); CHECK_EQ(g_slot, 7);
    CHECK_EQ(g_arg, 0x7000 + 5);

    // Single inheritance, `this` unadjusted.
    CHECK(static_cast<void*>(static_cast<UI::Element*>(&radio)) == r);
    CHECK(static_cast<void*>(static_cast<UI::GroupElement*>(&page)) == p);
}

// ===========================================================================
// PART C — slot 8, raw-called the RETAIL way
// ===========================================================================
UNIT_TEST(ui_leaf_c2_group_slot8_createinstance_retslot) {
    GroupDescriptionProbe group;
    PageDescriptionProbe  page;
    RadioDescriptionProbe radio;
    CheckSlot8Retslot(static_cast<void*>(&group), kGroupD, 0x6D6D6D6D);
    CheckSlot8Retslot(static_cast<void*>(&page),  kPageD,  0x9D9D9D9D);
    CheckSlot8Retslot(static_cast<void*>(&radio), kRadioD, 0xADADADAD);
}

// ===========================================================================
// PART D — the qualified call, two levels deep
// ===========================================================================
namespace {
int g_doorEntries = 0;
bool MintedPageSerializeDoor(void* self, void* stream) {
    ++g_doorEntries;
    return reinterpret_cast<UI::PageDescription*>(self)                    // B.3.6 bridge
        ->UI::PageDescription::Serialize(stream);
}
}  // namespace

UNIT_TEST(ui_leaf_c2_page_serialize_qualified_does_not_reenter) {
    PageDescriptionProbe page;
    void* raw = static_cast<void*>(&page);

    const void* minted[10] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 10; ++i) minted[i] = real[i];
    minted[kD5_Serialize / 8] = reinterpret_cast<const void*>(&MintedPageSerializeDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    SlotAt<bool (*)(void*, void*)>(raw, kD5_Serialize)(raw, (void*)0x77);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_trailN, 2);
    CHECK_EQ(g_trail[0], kPageD * 100 + 5);    // the leaf's body…
    CHECK_EQ(g_trail[1], kGroupD * 100 + 5);   // …then the PARENT's, not the door again

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ===========================================================================
// The Image chain (commit 2)
// ===========================================================================
UNIT_TEST(ui_leaf_c2_image_chain_sizes_match_retail) {
    CHECK_EQ(sizeof(UI::ImageDescription),         (size_t)0x80);    // 0x1800361f0
    CHECK_EQ(sizeof(UI::AnimatedImageDescription), (size_t)0x98);    // 0x1800369e0
    CHECK_EQ(sizeof(UI::ImageElement),             (size_t)0xe0);    // 0x180036600
    CHECK_EQ(sizeof(UI::AnimatedImageElement),     (size_t)0x100);   // 0x180036db0
    CHECK_EQ(alignof(UI::ImageDescription),         (size_t)8);
    CHECK_EQ(alignof(UI::AnimatedImageDescription), (size_t)8);
    CHECK_EQ(alignof(UI::ImageElement),             (size_t)8);
    CHECK_EQ(alignof(UI::AnimatedImageElement),     (size_t)8);

    CHECK_EQ(offsetof(UI::ImageDescription, _spriteSheet), (size_t)0x68);
    CHECK_EQ(offsetof(UI::ImageDescription, _spriteName),  (size_t)0x70);
    CHECK_EQ(offsetof(UI::ImageDescription, _width),       (size_t)0x78);
    CHECK_EQ(offsetof(UI::ImageDescription, _height),      (size_t)0x7c);
    CHECK_EQ(offsetof(UI::AnimatedImageDescription, _frameNames),      (size_t)0x80);
    CHECK_EQ(offsetof(UI::AnimatedImageDescription, _framesPerSecond), (size_t)0x90);
    CHECK_EQ(offsetof(UI::ImageElement, _sprite),   (size_t)0xc0);
    CHECK_EQ(offsetof(UI::ImageElement, _rotation), (size_t)0xd0);
    CHECK_EQ(offsetof(UI::ImageElement, _width),    (size_t)0xd4);
    CHECK_EQ(offsetof(UI::ImageElement, _height),   (size_t)0xd8);
    CHECK_EQ(offsetof(UI::AnimatedImageElement, _frames),          (size_t)0xe0);
    CHECK_EQ(offsetof(UI::AnimatedImageElement, _framesPerSecond), (size_t)0xf0);
    CHECK_EQ(offsetof(UI::AnimatedImageElement, _accumTicks),      (size_t)0xf8);
    CHECK_EQ(sizeof(UI::Sprite), (size_t)0x10);   // held by value at +0xc0 (B6-10)

    CHECK((std::is_base_of<UI::ElementDescription, UI::ImageDescription>::value));
    CHECK((std::is_base_of<UI::ImageDescription,   UI::AnimatedImageDescription>::value));
    CHECK((std::is_base_of<UI::Element,      UI::ImageElement>::value));
    CHECK((std::is_base_of<UI::ImageElement, UI::AnimatedImageElement>::value));
    CHECK(!std::is_abstract<UI::ImageDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::ImageDescription>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::AnimatedImageDescription>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::AnimatedImageDescription>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::ImageElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::ImageElement>::value);  // protected non-virtual dtor
    CHECK(!std::is_abstract<UI::AnimatedImageElement>::value);      // B.3.9 UI-A: concrete
    CHECK(!std::is_destructible<UI::AnimatedImageElement>::value);  // protected non-virtual dtor
    CHECK(!std::has_virtual_destructor<UI::ImageElement>::value);
    CHECK(!std::has_virtual_destructor<UI::AnimatedImageDescription>::value);
}

UNIT_TEST(ui_leaf_c2_image_descriptions_slot_order) {
    ImageDescriptionProbe         image;
    AnimatedImageDescriptionProbe anim;
    CheckDescriptionOwnSlots(static_cast<void*>(&image), kImageD);
    CheckDescriptionOwnSlots(static_cast<void*>(&anim),  kAnimD);

    const void* const* vi = VTableOf(static_cast<void*>(&image));
    const void* const* va = VTableOf(static_cast<void*>(&anim));
    for (int i = 1; i <= 9; ++i) {
        if (i == kD4_Compile / 8) CHECK(va[i] == vi[i]);
        else                      CHECK(va[i] != vi[i]);
    }

    CheckSlot8Retslot(static_cast<void*>(&image), kImageD, 0x1D1D1D1D);

    // AnimatedImageDescription's slot 8: retail order, and the name-hidden
    // applier reached QUALIFIED (it lands in ImageDescription's marker, not
    // back in slot 8).
    void* raw = static_cast<void*>(&anim);
    void* slot = nullptr;
    Reset();
    void** back = SlotAt<void** (*)(const void*, void**, const void*)>(
        raw, kD8_CreateInstance)(raw, &slot, nullptr);
    CHECK(back == &slot);
    CHECK(slot == reinterpret_cast<void*>(static_cast<uintptr_t>(0xA1A1A1A1)));
    CHECK_EQ(g_trailN, 2);
    CHECK_EQ(g_trail[0], kAnimD * 100 + 8);
    CHECK_EQ(g_trail[1], kImageD * 100 + 80);
    CHECK_EQ(g_arg, 0xA1A1A1A1);
}

UNIT_TEST(ui_leaf_c2_image_elements_slot_order) {
    FactoryScope scope;
    BareElementProbe          base{};
    ImageElementProbe         image{};
    AnimatedImageElementProbe anim{};
    void* im = static_cast<void*>(&image);
    void* an = static_cast<void*>(&anim);

    Reset(); SlotAt<const char* (*)(const void*)>(im, kE1_TypeName)(im);
    CHECK_EQ(g_who, (int)kImageE); CHECK_EQ(g_slot, 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(im, kE2_Type)(im);
    CHECK_EQ(g_who, (int)kImageE); CHECK_EQ(g_slot, 2);
    Reset(); SlotAt<void (*)(void*)>(im, kE4_ComputeSize)(im);
    CHECK_EQ(g_who, (int)kImageE); CHECK_EQ(g_slot, 4);
    Reset(); SlotAt<void (*)(void*, int, int, int, int)>(im, kE5_ComputeLayout)(im, 3, 0, 0, 4);
    CHECK_EQ(g_who, (int)kImageE); CHECK_EQ(g_slot, 5); CHECK_EQ(g_arg, 3004);
    Reset(); SlotAt<void (*)(const void*, void*, uint8_t)>(im, kE6_Draw)(im, nullptr, 9);
    CHECK_EQ(g_who, (int)kImageE); CHECK_EQ(g_slot, 6); CHECK_EQ(g_arg, 9);

    const void* const* vb = VTableOf(static_cast<void*>(&base));
    const void* const* vi = VTableOf(im);
    const void* const* va = VTableOf(an);
    for (int i : {3, 7, 8, 9, 10, 11}) CHECK(vi[i] == vb[i]);
    // AnimatedImageElement: ONLY Update (7) is its own among 3..11.
    for (int i = 3; i <= 11; ++i) {
        if (i == kE7_Update / 8) CHECK(va[i] != vi[i]);
        else                     CHECK(va[i] == vi[i]);
    }

    Reset(); SlotAt<const char* (*)(const void*)>(an, kE1_TypeName)(an);
    CHECK_EQ(g_who, (int)kAnimE); CHECK_EQ(g_slot, 1);
    Reset(); SlotAt<uint32_t (*)(const void*)>(an, kE2_Type)(an);
    CHECK_EQ(g_who, (int)kAnimE); CHECK_EQ(g_slot, 2);

    void* handle = reinterpret_cast<void*>(static_cast<uintptr_t>(0x9000));
    UI::Element::UpdateState state;
    state.mouseX = 2;
    Reset();
    SlotAt<void (*)(void*, const void*, void*, void**, const void*)>(an, kE7_Update)(
        an, nullptr, nullptr, &handle, &state);
    CHECK_EQ(g_who, (int)kAnimE); CHECK_EQ(g_slot, 7);
    CHECK_EQ(g_arg, 0x9000 + 2);

    CHECK(static_cast<void*>(static_cast<UI::Element*>(&anim)) == an);
}

// ===========================================================================
// ENDGAME B.3.9 UI-A (B39_RULES D1/D2, rule B9-16) — the CONCRETE classes.
// ===========================================================================
namespace {
template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);
    if (words[1] != 0) return ~uintptr_t(0);
    return words[0] - 1;
}

template <typename T, typename Make>
void B39ExpectBothArms(size_t elemSize, Make make) {
    void* p = Object::detail::InternalMemAlloc(elemSize, 8);
    CHECK(p != nullptr);
    T* one = make(p);
    g_b39DtorCount = 0;
    CHECK(one->DeletingDtor(1) == one);
    CHECK(g_b39DtorCount >= 1);

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

UNIT_TEST(b39_uia_image_family_slot0_by_pmf) {
    CHECK_EQ(SlotOffset(&UI::ImageDescription::DeletingDtor),         (uintptr_t)0x00);   // 0x1800361f0
    CHECK_EQ(SlotOffset(&UI::ImageDescription::Serialize),            (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&UI::ImageDescription::Construct),            (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&UI::AnimatedImageDescription::DeletingDtor), (uintptr_t)0x00);   // 0x1800369e0
    CHECK_EQ(SlotOffset(&UI::AnimatedImageDescription::Construct),    (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&UI::ImageElement::DeletingDtor),             (uintptr_t)0x00);   // 0x180036600
    CHECK_EQ(SlotOffset(&UI::ImageElement::Draw),                     (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&UI::AnimatedImageElement::DeletingDtor),     (uintptr_t)0x00);   // 0x180036db0
    CHECK_EQ(SlotOffset(&UI::AnimatedImageElement::Update),           (uintptr_t)0x38);
}

// The real ctors' stores (each header cites the transcribed instructions);
// untouched bytes stay 0xCD.
UNIT_TEST(b39_uia_image_family_ctors_make_retails_stores) {
    FactoryScope scope;
    {
        alignas(8) uint8_t buf[0x80];
        std::memset(buf, 0xCD, sizeof buf);
        UI::ImageDescription* d = ::new (buf) UI::ImageDescription();
        CHECK_EQ(d->_id, -1);                         // ElementDescription() ran
        CHECK(d->_spriteSheet == nullptr);
        CHECK(d->_spriteName == nullptr);
        CHECK_EQ(d->_width, -1);
        CHECK_EQ(d->_height, -1);
        ImageDescriptionProbe probe;
        CHECK(VTableOf(d) != VTableOf(&probe));
        g_b39DtorCount = 0;
        CHECK(d->DeletingDtor(0) == d);
        CHECK_EQ(g_b39DtorCount, 1);
    }
    {
        alignas(8) uint8_t buf[0x98];
        std::memset(buf, 0xCD, sizeof buf);
        UI::AnimatedImageDescription* d = ::new (buf) UI::AnimatedImageDescription();
        CHECK_EQ(d->_width, -1);                      // ImageDescription() ran
        CHECK(d->_frameNames._data == nullptr);
        CHECK_EQ(d->_frameNames._size, 0);
        CHECK_EQ(d->_framesPerSecond, 0x1e);
        CHECK_EQ(d->_pad94, 0xCDCDCDCDu);             // NOT written
        g_b39DtorCount = 0;
        CHECK(d->DeletingDtor(0) == d);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~AnimatedImageDescription, ~ImageDescription
    }
    {
        alignas(8) uint8_t buf[0xe0];
        std::memset(buf, 0xCD, sizeof buf);
        UI::ImageElement* e = ::new (buf) UI::ImageElement();
        CHECK_EQ(e->_alignment, 3);                   // Element() ran
        CHECK(e->_sprite.spriteSheet == nullptr);
        CHECK_EQ(e->_sprite.spriteIndex, -1);
        CHECK_EQ((uint32_t)e->_sprite.pad0C, 0xCDCDCDCDu);   // NOT written
        CHECK(e->_rotation == 0.0f);
        CHECK_EQ(e->_width, -1);
        CHECK_EQ(e->_height, -1);
        CHECK_EQ(e->_padDC, 0xCDCDCDCDu);             // NOT written
        ImageElementProbe probe{};
        CHECK(VTableOf(e) != VTableOf(&probe));
        g_b39DtorCount = 0;
        CHECK(e->DeletingDtor(0) == e);
        CHECK_EQ(g_b39DtorCount, 1);
    }
    {
        alignas(8) uint8_t buf[0x100];
        std::memset(buf, 0xCD, sizeof buf);
        UI::AnimatedImageElement* e = ::new (buf) UI::AnimatedImageElement();
        CHECK_EQ(e->_width, -1);                      // ImageElement() ran
        CHECK(e->_frames._data == nullptr);
        CHECK_EQ(e->_frames._size, 0);
        CHECK_EQ(e->_framesPerSecond, 0x1e);
        CHECK_EQ(e->_padF4, 0xCDCDCDCDu);             // NOT written
        CHECK_EQ(e->_accumTicks, 0);
        g_b39DtorCount = 0;
        CHECK(e->DeletingDtor(0) == e);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~AnimatedImageElement, ~ImageElement
    }
}

UNIT_TEST(b39_uia_image_family_deletingdtor_both_arms) {
    FactoryScope scope;
    B39ExpectBothArms<UI::ImageDescription>(0x80, [](void* p) { return ::new (p) UI::ImageDescription(); });
    B39ExpectBothArms<UI::ImageElement>(0xe0, [](void* p) { return ::new (p) UI::ImageElement(); });
}

UNIT_TEST(b39_uia_group_family_slot0_by_pmf) {
    CHECK_EQ(SlotOffset(&UI::GroupDescription::DeletingDtor), (uintptr_t)0x00);   // 0x180046af0
    CHECK_EQ(SlotOffset(&UI::GroupDescription::Construct),    (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&UI::PageDescription::DeletingDtor),  (uintptr_t)0x00);   // 0x1800471a0
    CHECK_EQ(SlotOffset(&UI::PageDescription::Serialize),     (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&UI::RadioDescription::DeletingDtor), (uintptr_t)0x00);   // 0x180047930
    CHECK_EQ(SlotOffset(&UI::RadioDescription::Construct),    (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&UI::GroupElement::DeletingDtor),     (uintptr_t)0x00);   // 0x180046e10
    CHECK_EQ(SlotOffset(&UI::GroupElement::Draw),             (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&UI::PageElement::DeletingDtor),      (uintptr_t)0x00);   // 0x180047540
    CHECK_EQ(SlotOffset(&UI::PageElement::Type),              (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&UI::RadioElement::DeletingDtor),     (uintptr_t)0x00);   // 0x180047d50
    CHECK_EQ(SlotOffset(&UI::RadioElement::Update),           (uintptr_t)0x38);
}

UNIT_TEST(b39_uia_group_family_ctors_make_retails_stores) {
    FactoryScope scope;
    {
        alignas(8) uint8_t buf[0x78];
        std::memset(buf, 0xCD, sizeof buf);
        UI::GroupDescription* d = ::new (buf) UI::GroupDescription();
        CHECK_EQ(d->_id, -1);                         // ElementDescription() ran
        CHECK(d->_children._data == nullptr);
        CHECK_EQ(d->_children._size, 0);
        CHECK_EQ(d->_children._capacity, 0);
        GroupDescriptionProbe probe;
        CHECK(VTableOf(d) != VTableOf(&probe));
        g_b39DtorCount = 0;
        CHECK(d->DeletingDtor(0) == d);
        CHECK_EQ(g_b39DtorCount, 1);
    }
    {
        alignas(8) uint8_t buf[0x80];
        std::memset(buf, 0xCD, sizeof buf);
        UI::PageDescription* d = ::new (buf) UI::PageDescription();
        CHECK(d->_children._data == nullptr);         // GroupDescription() ran
        CHECK_EQ(d->_activePage, 0);
        CHECK_EQ(d->_pad7c, 0xCDCDCDCDu);             // NOT written
        g_b39DtorCount = 0;
        CHECK(d->DeletingDtor(0) == d);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~PageDescription, ~GroupDescription
    }
    {
        alignas(8) uint8_t buf[0x80];
        std::memset(buf, 0xCD, sizeof buf);
        UI::RadioDescription* d = ::new (buf) UI::RadioDescription();
        CHECK_EQ(d->_activeRadio, -1);
        CHECK_EQ(d->_pad7c, 0xCDCDCDCDu);             // NOT written
        g_b39DtorCount = 0;
        CHECK(d->DeletingDtor(0) == d);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~RadioDescription, ~GroupDescription
    }
    {
        alignas(8) uint8_t buf[0xc0];
        std::memset(buf, 0xCD, sizeof buf);
        UI::GroupElement* e = ::new (buf) UI::GroupElement();
        CHECK_EQ(e->_alignment, 3);                   // Element() ran
        GroupElementProbe probe{};
        CHECK(VTableOf(e) != VTableOf(&probe));
        g_b39DtorCount = 0;
        CHECK(e->DeletingDtor(0) == e);
        CHECK_EQ(g_b39DtorCount, 1);
    }
    {
        alignas(8) uint8_t buf[0xc8];
        std::memset(buf, 0xCD, sizeof buf);
        UI::PageElement* e = ::new (buf) UI::PageElement();
        CHECK_EQ(e->_alignment, 3);
        CHECK_EQ(e->_activePage, 0);
        CHECK_EQ(e->_padC4, 0xCDCDCDCDu);             // NOT written
        g_b39DtorCount = 0;
        CHECK(e->DeletingDtor(0) == e);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~PageElement, ~GroupElement
    }
    {
        alignas(8) uint8_t buf[0xd8];
        std::memset(buf, 0xCD, sizeof buf);
        UI::RadioElement* e = ::new (buf) UI::RadioElement();
        CHECK_EQ(e->_alignment, 3);
        CHECK(e->_checkBoxes._data == nullptr);
        CHECK_EQ(e->_checkBoxes._size, 0);
        CHECK_EQ(e->_activeIndex, -1);
        CHECK_EQ(e->_padD4, 0xCDCDCDCDu);             // NOT written
        g_b39DtorCount = 0;
        CHECK(e->DeletingDtor(0) == e);
        CHECK_EQ(g_b39DtorCount, 2);                  // ~RadioElement, ~GroupElement
    }
}

UNIT_TEST(b39_uia_group_family_deletingdtor_both_arms) {
    FactoryScope scope;
    B39ExpectBothArms<UI::GroupDescription>(0x78, [](void* p) { return ::new (p) UI::GroupDescription(); });
    B39ExpectBothArms<UI::GroupElement>(0xc0, [](void* p) { return ::new (p) UI::GroupElement(); });
}
