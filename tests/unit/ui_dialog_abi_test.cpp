// ============================================================================
// tests/unit/ui_dialog_abi_test.cpp — ENDGAME B.3 step 6, block B
//   the six dialog-family `UI` classes as real C++ classes
//
//   UI::DialogInstance         : Object::Internal,   abstract, 0xb8, 10 slots
//   UI::DialogController       : Object::Internal,   abstract, 0x30, 11 slots
//   UI::DialogControllerConfig : Object::External,   abstract, 0x20,  8 slots
//   UI::Dialog                 : Object::External,   abstract, 0x88,  9 slots
//   UI::ComboDialog            : UI::DialogInstance, abstract, 0xd0, 10 slots
//   UI::ToolTipDialog          : UI::DialogInstance, abstract, 0xc0, 10 slots
//
// Built to the contract of graphics_material_abi_test.cpp (block G, step 5):
//
// PART A — LAYOUT: size, alignment, the field offsets, the POD cross-asserts,
//   and that each class's own fields begin at exactly sizeof(base).
// PART B — SLOT ORDER: probe subclasses whose GCC vtable is read back by BYTE
//   OFFSET and raw-called, each slot landing on the member it must name.
//   *** RULE B6-1.2 is the point of the DialogController half: OnOpened is +0x20
//   on DialogInstance and +0x28 on DialogController (OnCreated is +0x20 there),
//   and the test checks BOTH numberings side by side. ***
// PART C — THE BY-ADDRESS InternalPtr ARGUMENT: slot 8 (DialogInstance) and
//   slot 9 (DialogController) receive the ADDRESS of an InternalPtr<Element>,
//   the MSVC by-value-class ABI, and the member sees exactly that address.
// PART D — THE QUALIFIED CALL (B34_BRIEFS A.1): a door stamped into a minted
//   table delegates with `->UI::DialogInstance::OnUpdateLayout()` and is entered
//   exactly once.
//
// NOT PROVEN HERE: the ported bodies (runtime/src/ui/dialog*.cpp stand on the
// port registry and cannot link into a host test). The classes' out-of-line
// virtuals are defined AS MARKERS at the bottom of this file, which is also
// what makes GCC emit their vtables in this translation unit.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/Dialog.h"
#include "runtime/types/ui/DialogInstance.h"
#include "runtime/types/ui/DialogController.h"
#include "runtime/types/ui/DialogControllerConfig.h"
#include "runtime/types/ui/ComboDialog.h"
#include "runtime/types/ui/ToolTipDialog.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <type_traits>

// internal_class.cpp's unit-only seams (-DBANISHED_UNIT_TEST): every
// Object::Internal constructor registers with InternalFactory::Get(), so a
// test that builds an Internal-derived probe installs a factory it owns and
// restores the previous one (object_internal_test.cpp's FactoryScope).
namespace Object {
namespace detail {
extern InternalFactory* g_unitInternalFactory;
}  // namespace detail
}  // namespace Object

namespace {

struct FactoryScope {
    ::Object::InternalFactory  factory;
    ::Object::InternalFactory* saved;
    FactoryScope() : factory(), saved(::Object::detail::g_unitInternalFactory) {
        ::Object::detail::g_unitInternalFactory = &factory;
    }
    ~FactoryScope() { ::Object::detail::g_unitInternalFactory = saved; }
};

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
const void* g_lastArg  = nullptr;

void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; g_lastArg = nullptr; }

template <typename Fn>
Fn SlotAt(void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

// Retail byte offsets, B36_SLOTS.md section 1.
namespace DI {   // UI::DialogInstance and its two subclasses
constexpr int kDeletingDtor = 0x00, kTypeName = 0x08, kType = 0x10, kPersist = 0x18,
              kOnOpened = 0x20, kOnClosed = 0x28, kOnUpdateLayout = 0x30,
              kOnUpdate = 0x38, kOnElementEvent = 0x40, kOnSerializeSettings = 0x48;
}
namespace DC {   // UI::DialogController — ONE SLOT APART (RULE B6-1.2)
constexpr int kDeletingDtor = 0x00, kTypeName = 0x08, kType = 0x10, kPersist = 0x18,
              kOnCreated = 0x20, kOnOpened = 0x28, kOnClosed = 0x30,
              kOnUpdateLayout = 0x38, kOnUpdate = 0x40, kOnElementEvent = 0x48,
              kOnSerializeSettings = 0x50;
}
namespace EX {   // the Object::External family
constexpr int kDeletingDtor = 0x00, kTypeName = 0x08, kType = 0x10,
              kGetFileVersion = 0x18, kCompile = 0x20, kSerialize = 0x28,
              kReconstruct = 0x30, kDestruct = 0x38, kConstruct = 0x40;
}

// ---------------------------------------------------------------------------
// Probes. Each overrides only slot 0 (pure in every one of the six) — plus, for
// DialogControllerConfig, the four External slots retail leaves `_purecall`.
// ---------------------------------------------------------------------------
struct BareInternalProbe final : Object::Internal {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    const char* TypeName() const override { return "bare"; }
};
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    const char* TypeName() const override { return "bare"; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

struct DialogInstanceProbe final : UI::DialogInstance {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct ComboDialogProbe final : UI::ComboDialog {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct ToolTipDialogProbe final : UI::ToolTipDialog {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct DialogControllerProbe final : UI::DialogController {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct DialogProbe final : UI::Dialog {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct ConfigProbe final : UI::DialogControllerConfig {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
    int32_t GetFileVersion() const override { Note(this, 3); return 0; }
    bool Serialize(void*) override { Note(this, 5); return true; }
    Object::External* Reconstruct() override { Note(this, 6); return this; }
    void Destruct() override { Note(this, 7); }
};

using DtorFn  = void* (*)(void*, unsigned);
using NameFn  = const char* (*)(const void*);
using TypeFn  = uint32_t (*)(const void*);
using VoidFn  = void (*)(void*);
using PtrArgFn = void (*)(void*, const void*);
using EventFn = void (*)(void*, void*, int32_t, int32_t, int32_t, int32_t, int32_t, void*);

// Raw-call every DialogInstance-numbered slot; `mark` is the member each
// slot's marker records, `owner` whose marker it is (0 = DialogInstance's own,
// otherwise the subclass's).
void CheckInstanceSlots(void* raw) {
    Reset(); SlotAt<DtorFn>(raw, DI::kDeletingDtor)(raw, 0u);   CHECK_EQ(g_lastMark, 0);
    CHECK(g_lastSelf == raw);
    Reset(); SlotAt<NameFn>(raw, DI::kTypeName)(raw);           CHECK_EQ(g_lastMark % 100, 1);
    Reset(); SlotAt<TypeFn>(raw, DI::kType)(raw);               CHECK_EQ(g_lastMark % 100, 2);
    Reset(); SlotAt<PtrArgFn>(raw, DI::kPersist)(raw, nullptr); CHECK_EQ(g_lastMark % 100, 3);
    Reset(); SlotAt<VoidFn>(raw, DI::kOnOpened)(raw);           CHECK_EQ(g_lastMark % 100, 4);
    Reset(); SlotAt<VoidFn>(raw, DI::kOnClosed)(raw);           CHECK_EQ(g_lastMark % 100, 5);
    Reset(); SlotAt<VoidFn>(raw, DI::kOnUpdateLayout)(raw);     CHECK_EQ(g_lastMark % 100, 6);
    int64_t ticks = 7;
    Reset(); SlotAt<PtrArgFn>(raw, DI::kOnUpdate)(raw, &ticks); CHECK_EQ(g_lastMark % 100, 7);
    CHECK(g_lastArg == &ticks);                   // `const Time&` is an address
    void* elem = nullptr;
    Reset(); SlotAt<EventFn>(raw, DI::kOnElementEvent)(raw, &elem, 1, 2, 3, 4, 5, nullptr);
    CHECK_EQ(g_lastMark % 100, 8);
    CHECK(g_lastArg == &elem);                    // PART C: the ADDRESS arrives
    Reset(); SlotAt<PtrArgFn>(raw, DI::kOnSerializeSettings)(raw, nullptr);
    CHECK_EQ(g_lastMark % 100, 9);
}

void CheckDistinct(void* raw, int n) {
    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            CHECK(vt[i] != vt[j]);
}

}  // namespace

// ---------------------------------------------------------------------------
// PART B — slot order
// ---------------------------------------------------------------------------
UNIT_TEST(ui_dialoginstance_slot_order_is_retails) {
    FactoryScope scope;   // the probes are Object::Internal objects
    DialogInstanceProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckInstanceSlots(raw);
    CheckDistinct(raw, 10);
    // Persist is DialogInstance's OWN body (0x180114fa0), not Internal's
    // 0x1800bfda0 — so the +0x18 entries must differ.
    BareInternalProbe bare;
    CHECK(VTableOf(raw)[DI::kPersist / 8] != VTableOf(static_cast<void*>(&bare))[DI::kPersist / 8]);
    CHECK(static_cast<void*>(static_cast<Object::Internal*>(&probe)) == raw);
}

UNIT_TEST(ui_dialogcontroller_slot_order_is_one_apart) {
    FactoryScope scope;   // the probes are Object::Internal objects
    DialogControllerProbe probe;
    void* raw = static_cast<void*>(&probe);
    Reset(); SlotAt<DtorFn>(raw, DC::kDeletingDtor)(raw, 0u);   CHECK_EQ(g_lastMark, 0);
    Reset(); SlotAt<NameFn>(raw, DC::kTypeName)(raw);           CHECK_EQ(g_lastMark, 201);
    Reset(); SlotAt<TypeFn>(raw, DC::kType)(raw);               CHECK_EQ(g_lastMark, 202);
    Reset(); SlotAt<VoidFn>(raw, DC::kOnCreated)(raw);          CHECK_EQ(g_lastMark, 204);
    Reset(); SlotAt<VoidFn>(raw, DC::kOnOpened)(raw);           CHECK_EQ(g_lastMark, 205);
    Reset(); SlotAt<VoidFn>(raw, DC::kOnClosed)(raw);           CHECK_EQ(g_lastMark, 206);
    Reset(); SlotAt<VoidFn>(raw, DC::kOnUpdateLayout)(raw);     CHECK_EQ(g_lastMark, 207);
    int64_t ticks = 1;
    Reset(); SlotAt<PtrArgFn>(raw, DC::kOnUpdate)(raw, &ticks); CHECK_EQ(g_lastMark, 208);
    void* elem = nullptr;
    Reset(); SlotAt<EventFn>(raw, DC::kOnElementEvent)(raw, &elem, 1, 2, 3, 4, 5, nullptr);
    CHECK_EQ(g_lastMark, 209);
    CHECK(g_lastArg == &elem);                    // PART C
    Reset(); SlotAt<PtrArgFn>(raw, DC::kOnSerializeSettings)(raw, nullptr);
    CHECK_EQ(g_lastMark, 210);

    // Slot 3 is INHERITED from Object::Internal (0x1800bfda0).
    BareInternalProbe bare;
    CHECK(VTableOf(raw)[DC::kPersist / 8] == VTableOf(static_cast<void*>(&bare))[DC::kPersist / 8]);

    // RULE B6-1.2, side by side: the same NAME, one slot apart.
    DialogInstanceProbe inst;
    void* iraw = static_cast<void*>(&inst);
    Reset(); SlotAt<VoidFn>(iraw, 0x20)(iraw); CHECK_EQ(g_lastMark, 104);   // DI OnOpened
    Reset(); SlotAt<VoidFn>(raw,  0x20)(raw);  CHECK_EQ(g_lastMark, 204);   // DC OnCreated
    Reset(); SlotAt<VoidFn>(raw,  0x28)(raw);  CHECK_EQ(g_lastMark, 205);   // DC OnOpened
}

UNIT_TEST(ui_combodialog_slot_order_is_retails) {
    FactoryScope scope;   // the probes are Object::Internal objects
    ComboDialogProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckInstanceSlots(raw);
    // Own: 1, 2, 6, 7, 8 (marks 3xx); inherited from DialogInstance: 3, 4, 5, 9.
    Reset(); SlotAt<NameFn>(raw, DI::kTypeName)(raw);       CHECK_EQ(g_lastMark, 301);
    Reset(); SlotAt<VoidFn>(raw, DI::kOnUpdateLayout)(raw); CHECK_EQ(g_lastMark, 306);
    Reset(); SlotAt<VoidFn>(raw, DI::kOnOpened)(raw);       CHECK_EQ(g_lastMark, 104);
    DialogInstanceProbe base;
    for (int s : {DI::kPersist, DI::kOnOpened, DI::kOnClosed, DI::kOnSerializeSettings})
        CHECK(VTableOf(raw)[s / 8] == VTableOf(static_cast<void*>(&base))[s / 8]);
    for (int s : {DI::kOnUpdateLayout, DI::kOnUpdate, DI::kOnElementEvent})
        CHECK(VTableOf(raw)[s / 8] != VTableOf(static_cast<void*>(&base))[s / 8]);
}

UNIT_TEST(ui_tooltipdialog_overrides_only_slots_0_1_2) {
    FactoryScope scope;   // the probes are Object::Internal objects
    ToolTipDialogProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckInstanceSlots(raw);
    Reset(); SlotAt<NameFn>(raw, DI::kTypeName)(raw); CHECK_EQ(g_lastMark, 401);
    Reset(); SlotAt<TypeFn>(raw, DI::kType)(raw);     CHECK_EQ(g_lastMark, 402);
    DialogInstanceProbe base;
    for (int s = DI::kPersist; s <= DI::kOnSerializeSettings; s += 8)
        CHECK(VTableOf(raw)[s / 8] == VTableOf(static_cast<void*>(&base))[s / 8]);
}

UNIT_TEST(ui_dialog_slot_order_is_retails_construct_at_8) {
    DialogProbe probe;
    void* raw = static_cast<void*>(&probe);
    using VerFn = int32_t (*)(const void*);
    using SerFn = bool (*)(void*, void*);
    using PtrFn = void* (*)(void*);
    Reset(); SlotAt<DtorFn>(raw, EX::kDeletingDtor)(raw, 0u);   CHECK_EQ(g_lastMark, 0);
    Reset(); SlotAt<NameFn>(raw, EX::kTypeName)(raw);           CHECK_EQ(g_lastMark, 501);
    Reset(); SlotAt<TypeFn>(raw, EX::kType)(raw);               CHECK_EQ(g_lastMark, 502);
    Reset(); CHECK_EQ(SlotAt<VerFn>(raw, EX::kGetFileVersion)(raw), 6);
    CHECK_EQ(g_lastMark, 503);
    Reset(); SlotAt<SerFn>(raw, EX::kSerialize)(raw, nullptr);  CHECK_EQ(g_lastMark, 505);
    Reset(); SlotAt<PtrFn>(raw, EX::kReconstruct)(raw);         CHECK_EQ(g_lastMark, 506);
    Reset(); SlotAt<VoidFn>(raw, EX::kDestruct)(raw);           CHECK_EQ(g_lastMark, 507);
    Reset(); SlotAt<PtrFn>(raw, EX::kConstruct)(raw);           CHECK_EQ(g_lastMark, 508);
    CheckDistinct(raw, 9);
    // Slot 4 is External's Compile, inherited.
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[EX::kCompile / 8] == VTableOf(static_cast<void*>(&bare))[EX::kCompile / 8]);
}

UNIT_TEST(ui_dialogcontrollerconfig_slot_order_is_retails) {
    ConfigProbe probe;
    void* raw = static_cast<void*>(&probe);
    Reset(); SlotAt<NameFn>(raw, EX::kTypeName)(raw);  CHECK_EQ(g_lastMark, 601);
    Reset(); SlotAt<TypeFn>(raw, EX::kType)(raw);      CHECK_EQ(g_lastMark, 602);
    // 3/5/6/7 are PURE on the class (retail `_purecall`) — the probe's own.
    using VerFn = int32_t (*)(const void*);
    Reset(); SlotAt<VerFn>(raw, EX::kGetFileVersion)(raw); CHECK_EQ(g_lastMark, 3);
    Reset(); SlotAt<VoidFn>(raw, EX::kDestruct)(raw);      CHECK_EQ(g_lastMark, 7);
    CHECK(std::is_abstract<UI::DialogControllerConfig>::value);
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[EX::kCompile / 8] == VTableOf(static_cast<void*>(&bare))[EX::kCompile / 8]);
}

// ---------------------------------------------------------------------------
// PART D — the qualified call
// ---------------------------------------------------------------------------
namespace {
int g_doorEntries = 0;
void MintedOnUpdateLayoutDoor(void* self) {
    ++g_doorEntries;
    reinterpret_cast<UI::DialogInstance*>(self)             // B.3.6 bridge
        ->UI::DialogInstance::OnUpdateLayout();
}
}  // namespace

UNIT_TEST(ui_dialoginstance_qualified_call_does_not_reenter_the_door) {
    FactoryScope scope;   // the probes are Object::Internal objects
    DialogInstanceProbe probe;
    void* raw = static_cast<void*>(&probe);
    const void* minted[10] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 10; ++i) minted[i] = real[i];
    minted[DI::kOnUpdateLayout / 8] = reinterpret_cast<const void*>(&MintedOnUpdateLayoutDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    SlotAt<VoidFn>(raw, DI::kOnUpdateLayout)(raw);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_lastMark, 106);

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ---------------------------------------------------------------------------
// PART A — layout
// ---------------------------------------------------------------------------
UNIT_TEST(ui_dialog_family_layout) {
    FactoryScope scope;   // the probes are Object::Internal objects
    CHECK_EQ((int)sizeof(UI::DialogInstance), 0xb8);
    CHECK_EQ((int)sizeof(UI::DialogController), 0x30);
    CHECK_EQ((int)sizeof(UI::DialogControllerConfig), 0x20);
    CHECK_EQ((int)sizeof(UI::Dialog), 0x88);
    CHECK_EQ((int)sizeof(UI::ComboDialog), 0xd0);
    CHECK_EQ((int)sizeof(UI::ToolTipDialog), 0xc0);
    CHECK_EQ((int)alignof(UI::DialogInstance), 8);
    CHECK_EQ((int)alignof(UI::Dialog), 8);

    // Parents, as the `_ParentTypeName` literals prove them.
    CHECK((std::is_base_of<Object::Internal, UI::DialogInstance>::value));
    CHECK((std::is_base_of<Object::Internal, UI::DialogController>::value));
    CHECK((std::is_base_of<Object::External, UI::DialogControllerConfig>::value));
    CHECK((std::is_base_of<Object::External, UI::Dialog>::value));
    CHECK((std::is_base_of<UI::DialogInstance, UI::ComboDialog>::value));
    CHECK((std::is_base_of<UI::DialogInstance, UI::ToolTipDialog>::value));
    CHECK((!std::is_base_of<Object::External, UI::DialogInstance>::value));

    // All six ABSTRACT (RULE B6-3).
    CHECK(std::is_abstract<UI::DialogInstance>::value);
    CHECK(std::is_abstract<UI::DialogController>::value);
    CHECK(std::is_abstract<UI::DialogControllerConfig>::value);
    CHECK(std::is_abstract<UI::Dialog>::value);
    CHECK(std::is_abstract<UI::ComboDialog>::value);
    CHECK(std::is_abstract<UI::ToolTipDialog>::value);

    // Own fields start at sizeof(base).
    CHECK_EQ((int)offsetof(UI::DialogInstance, _ui), (int)sizeof(Object::Internal));
    CHECK_EQ((int)offsetof(UI::DialogController, _config), (int)sizeof(Object::Internal));
    CHECK_EQ((int)offsetof(UI::Dialog, _spriteSheet), (int)sizeof(Object::External));
    CHECK_EQ((int)offsetof(UI::ComboDialog, _list), (int)sizeof(UI::DialogInstance));
    CHECK_EQ((int)offsetof(UI::ToolTipDialog, _unknownB8), (int)sizeof(UI::DialogInstance));

    // The fields the old engine POD got wrong (POD_TREE_AUDIT 1.2).
    CHECK_EQ((int)offsetof(UI::DialogInstance, _result), 0x24);
    CHECK_EQ((int)offsetof(UI::DialogInstance, _dataSource), 0x30);
    CHECK_EQ((int)offsetof(UI::DialogInstance, _userFlags), 0xb0);

    // The engine aliases (engine/types/ui/{DialogInstance,DialogController}.h)
    // name the PODs, which must describe the same bytes as the classes.
    CHECK_EQ((int)sizeof(RtUI::DialogInstance), (int)sizeof(UI::DialogInstance));
    CHECK_EQ((int)sizeof(RtUI::DialogController), (int)sizeof(UI::DialogController));
    CHECK_EQ((int)sizeof(RtUI::ComboDialog), (int)sizeof(UI::ComboDialog));
    CHECK_EQ((int)sizeof(RtUI::Dialog), (int)sizeof(UI::Dialog));

    // A write through the class lands where the POD readers look.
    DialogInstanceProbe probe;
    probe._result = 3;
    probe._userFlags = 0x40;
    const RtUI::DialogInstance* pod = reinterpret_cast<const RtUI::DialogInstance*>(
        static_cast<UI::DialogInstance*>(&probe));
    CHECK_EQ(pod->_result, 3);
    CHECK_EQ(pod->_userFlags, 0x40);
}

// ============================================================================
// Marker definitions of the classes' out-of-line virtuals. Mark = 100*class +
// slot: DialogInstance 1xx, DialogController 2xx, ComboDialog 3xx,
// ToolTipDialog 4xx, Dialog 5xx, DialogControllerConfig 6xx.
// ============================================================================
namespace UI {

const char* DialogInstance::TypeName() const { Note(this, 101); return "DialogInstance"; }
uint32_t    DialogInstance::Type() const { Note(this, 102); return 0u; }
void DialogInstance::Persist(void*) { Note(this, 103); }
void DialogInstance::OnOpened() { Note(this, 104); }
void DialogInstance::OnClosed() { Note(this, 105); }
void DialogInstance::OnUpdateLayout() { Note(this, 106); }
void DialogInstance::OnUpdate(const System::Time& t) { Note(this, 107); g_lastArg = &t; }
void DialogInstance::OnElementEvent(Object::InternalPtr<Element>* e, int32_t, int32_t,
                                    int32_t, int32_t, int32_t, void*) {
    Note(this, 108); g_lastArg = e;
}
void DialogInstance::OnSerializeSettings(void*) { Note(this, 109); }
DialogInstance::~DialogInstance() {}

const char* DialogController::TypeName() const { Note(this, 201); return "DialogController"; }
uint32_t    DialogController::Type() const { Note(this, 202); return 0u; }
void DialogController::OnCreated() { Note(this, 204); }
void DialogController::OnOpened() { Note(this, 205); }
void DialogController::OnClosed() { Note(this, 206); }
void DialogController::OnUpdateLayout() { Note(this, 207); }
void DialogController::OnUpdate(const System::Time& t) { Note(this, 208); g_lastArg = &t; }
void DialogController::OnElementEvent(Object::InternalPtr<Element>* e, int32_t, int32_t,
                                      int32_t, int32_t, int32_t, void*) {
    Note(this, 209); g_lastArg = e;
}
void DialogController::OnSerializeSettings(void*) { Note(this, 210); }
DialogController::~DialogController() {}

const char* ComboDialog::TypeName() const { Note(this, 301); return "ComboDialog"; }
uint32_t    ComboDialog::Type() const { Note(this, 302); return 0u; }
void ComboDialog::OnUpdateLayout() { Note(this, 306); }
void ComboDialog::OnUpdate(const System::Time& t) { Note(this, 307); g_lastArg = &t; }
void ComboDialog::OnElementEvent(Object::InternalPtr<Element>* e, int32_t, int32_t,
                                 int32_t, int32_t, int32_t, void*) {
    Note(this, 308); g_lastArg = e;
}
ComboDialog::~ComboDialog() {}

const char* ToolTipDialog::TypeName() const { Note(this, 401); return "ToolTipDialog"; }
uint32_t    ToolTipDialog::Type() const { Note(this, 402); return 0u; }
ToolTipDialog::~ToolTipDialog() {}

const char* Dialog::TypeName() const { Note(this, 501); return "Dialog"; }
uint32_t    Dialog::Type() const { Note(this, 502); return 0u; }
int32_t     Dialog::GetFileVersion() const { Note(this, 503); return 6; }
bool        Dialog::Serialize(void*) { Note(this, 505); return true; }
Object::External* Dialog::Reconstruct() { Note(this, 506); return this; }
void        Dialog::Destruct() { Note(this, 507); }
Object::External* Dialog::Construct() { Note(this, 508); return this; }
Dialog::~Dialog() {}

const char* DialogControllerConfig::TypeName() const { Note(this, 601); return "DialogControllerConfig"; }
uint32_t    DialogControllerConfig::Type() const { Note(this, 602); return 0u; }
DialogControllerConfig::~DialogControllerConfig() {}

}  // namespace UI
