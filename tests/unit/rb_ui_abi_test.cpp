// ============================================================================
// tests/unit/rb_ui_abi_test.cpp — ENDGAME B.3.8 block RB, the UI Runtime bases
//
// B38_RULES D1 / RULE B8-1: a Runtime base that an exe class derives from gets
// a REAL constructor whose body is ONE body with its ctor DOOR (`InitFields`,
// reached by the door through `X::DoorBody::Init` after the door stamped the
// MINTED table), and a destructor sharing `ReleaseFields` with its dtor door.
// What this file pins, on the shipped headers:
//
// PART A — STAMP ORDER. A test subclass's construction runs the shared body
//   while the object carries the BASE's compiler table (the compiler stamped
//   Base -> Internal/External -> X, and not yet the subclass), and ends on the
//   subclass's table; destruction runs `ReleaseFields` with the base's table
//   restamped. That is retail's order: `CALL ??0Internal`, `MOV [RBX],??_7X`,
//   body; and on the way out `MOV [RBX],??_7X`, body, `~Internal`.
// PART B — THE DOOR REACHES THE SAME BODY. A raw buffer stamped with a stand-in
//   "minted" table and handed to `DoorBody::Init` / `::Release` runs the very
//   same member with that table in place, with the same arguments, and leaves
//   the same bytes as the constructor.
// PART C — RETAIL BODIES THAT ARE INLINE, checked byte for byte: the
//   DialogInstance default ctor (0x180114ca0), DialogController's
//   (0x1800cd320, three zero stores, +0x28 untouched), DialogControllerConfig's
//   (External's three zero stores only), and DialogInstance's concrete slot 0
//   (the exe's 0x140020cb0: ~DialogInstance, MemFree on flags & 1).
//
// NOT PROVEN HERE: the real `InitFields` / `ReleaseFields` bodies. They live in
// runtime/src/ui/{dialog_create.cpp, dialog.cpp, element.cpp}, which stand on
// the port registry and cannot link into a host test, so they are defined
// below AS RECORDING MARKERS. The bodies were MOVED from the doors unchanged;
// the real run and the resave gate verify them.
// ============================================================================
#include "unit.h"

#include "runtime/types/ui/DialogInstance.h"
#include "runtime/types/ui/DialogController.h"
#include "runtime/types/ui/DialogControllerConfig.h"
#include "runtime/types/ui/Element.h"
#include "runtime/types/ui/ElementDescription.h"
#include "runtime/types/ui/SpriteSheet.h"
#include "runtime/types/ui/SpriteCapture.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <new>
#include <type_traits>

namespace Object {
namespace detail {
extern InternalFactory* g_unitInternalFactory;
}  // namespace detail
}  // namespace Object

// The compiler vtables of the three bases with a shared body, by their Itanium
// names (the linker checks the spelling). The object's vptr is group[2].
extern const void* const g_rbDiVtGroup[] __asm__("_ZTVN2UI14DialogInstanceE");
extern const void* const g_rbElVtGroup[] __asm__("_ZTVN2UI7ElementE");
extern const void* const g_rbEdVtGroup[] __asm__("_ZTVN2UI18ElementDescriptionE");
extern const void* const g_rbSsVtGroup[] __asm__("_ZTVN2UI11SpriteSheetE");
extern const void* const g_rbScVtGroup[] __asm__("_ZTVN2UI13SpriteCaptureE");

namespace {

const void* DiVt() { return &g_rbDiVtGroup[2]; }
const void* ElVt() { return &g_rbElVtGroup[2]; }
const void* EdVt() { return &g_rbEdVtGroup[2]; }
const void* VptrOf(const void* obj) { return *static_cast<const void* const*>(obj); }

struct FactoryScope {
    ::Object::InternalFactory  factory;
    ::Object::InternalFactory* saved;
    FactoryScope() : factory(), saved(::Object::detail::g_unitInternalFactory) {
        ::Object::detail::g_unitInternalFactory = &factory;
    }
    ~FactoryScope() { ::Object::detail::g_unitInternalFactory = saved; }
};

// What the markers saw.
struct Seen {
    int         initCalls = 0, releaseCalls = 0;
    const void* initSelf = nullptr;
    const void* initVptr = nullptr;
    const void* releaseVptr = nullptr;
    const void* args[4] = {};
    int32_t     flags = 0;
};
Seen g_di, g_el, g_ed;

// A stand-in for a MINTED table: any address that is not a compiler table.
const void* const kFakeMinted[4] = {};

}  // namespace

// ===========================================================================
// The shared bodies, AS MARKERS (see the banner). Each records the vptr it
// runs under and writes a recognisable pattern into fields the real body also
// writes, so PART B can compare what the two entry points leave behind.
// ===========================================================================
namespace UI {
void DialogInstance::InitFields(UserInterface* ui, const Dialog* dialog,
                                const Object::InternalPtr<DialogInstance>* parent,
                                Object::Base* dataSource, void* userData, int32_t userFlags) {
    ++g_di.initCalls;
    g_di.initSelf = this;
    g_di.initVptr = VptrOf(this);
    g_di.args[0] = ui; g_di.args[1] = dialog; g_di.args[2] = parent->Get();
    g_di.args[3] = dataSource;
    _ui = ui;
    _dialog = dialog;
    _parent = *parent;
    _dataSource = dataSource;
    _userData = userData;
    _userFlags = userFlags;
}
void DialogInstance::ReleaseFields() {
    ++g_di.releaseCalls;
    g_di.releaseVptr = VptrOf(this);
}
void Element::InitFields() {
    ++g_el.initCalls;
    g_el.initSelf = this;
    g_el.initVptr = VptrOf(this);
    _alignment = 3;
    _color = 0xFFFFFFFFu;
}
void Element::ReleaseFields() {
    ++g_el.releaseCalls;
    g_el.releaseVptr = VptrOf(this);
}
void ElementDescription::InitFields() {
    ++g_ed.initCalls;
    g_ed.initSelf = this;
    g_ed.initVptr = VptrOf(this);
    _id = -1;
    _elementType = 3;
}
void ElementDescription::ReleaseFields() {
    ++g_ed.releaseCalls;
    g_ed.releaseVptr = VptrOf(this);
}
}  // namespace UI

namespace {

// ---- test subclasses (stand-ins for the step-8 exe classes) ---------------
struct RbDialog final : UI::DialogInstance {
    RbDialog() = default;
    RbDialog(UI::UserInterface* ui, const UI::Dialog* d,
             Object::InternalPtr<UI::DialogInstance> parent, Object::Base* ds,
             void* ud, int32_t uf)
        : UI::DialogInstance(ui, d, parent, ds, ud, uf) {}
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct RbPanel final : UI::DialogController {
    Object::Base* DeletingDtor(unsigned) override { return this; }
};
struct RbConfig final : UI::DialogControllerConfig {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};
struct RbElement final : UI::Element {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    void ComputeSize() override {}
};
struct RbElementDesc final : UI::ElementDescription {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
    Object::InternalPtr<UI::Element>* CreateInstance(
            Object::InternalPtr<UI::Element>* ret, const UI::Dialog*) const override {
        return ret;
    }
};

template <class T>
T* BuildIn(unsigned char* buf, unsigned char fill) {
    std::memset(buf, fill, sizeof(T));
    // DEFAULT-initialisation (no `()`): value-initialisation would zero the
    // bytes first for a probe whose own ctor is defaulted, hiding the holes.
    return ::new (static_cast<void*>(buf)) T;
}
template <class T>
uint64_t Q(const T* obj, size_t off) {
    uint64_t v;
    std::memcpy(&v, reinterpret_cast<const unsigned char*>(obj) + off, sizeof v);
    return v;
}
template <class T>
uint32_t D(const T* obj, size_t off) {
    uint32_t v;
    std::memcpy(&v, reinterpret_cast<const unsigned char*>(obj) + off, sizeof v);
    return v;
}
template <class T>
uint8_t By(const T* obj, size_t off) { return reinterpret_cast<const unsigned char*>(obj)[off]; }

// ===========================================================================
// PART A — stamp order through a test subclass
// ===========================================================================
UNIT_TEST(rb_dialoginstance_ctor_runs_shared_body_under_base_table) {
    FactoryScope scope;
    g_di = Seen{};
    int dialogToken = 0, dsToken = 0, userToken = 0;
    alignas(8) unsigned char parentObj[0x10] = {};
    Object::InternalPtr<UI::DialogInstance> parent(
        reinterpret_cast<UI::DialogInstance*>(parentObj));
    alignas(8) unsigned char buf[sizeof(RbDialog)];
    std::memset(buf, 0xCD, sizeof buf);
    RbDialog* d = ::new (static_cast<void*>(buf)) RbDialog(
        reinterpret_cast<UI::UserInterface*>(&dialogToken + 1),
        reinterpret_cast<const UI::Dialog*>(&dialogToken), parent,
        reinterpret_cast<Object::Base*>(&dsToken), &userToken, 0x1234);
    CHECK_EQ(g_di.initCalls, 1);
    CHECK(g_di.initSelf == static_cast<void*>(buf));
    CHECK(g_di.initVptr == DiVt());          // base stamped, subclass not yet
    CHECK(VptrOf(d) != DiVt());              // ... and the subclass last
    CHECK(g_di.args[1] == &dialogToken);
    CHECK(g_di.args[2] == static_cast<void*>(parentObj));   // by value through the ctor
    CHECK(g_di.args[3] == &dsToken);
    CHECK_EQ(d->_userFlags, 0x1234);
    d->~RbDialog();
    CHECK_EQ(g_di.releaseCalls, 1);
    CHECK(g_di.releaseVptr == DiVt());       // ~DialogInstance restamped first
}

UNIT_TEST(rb_element_ctor_dtor_run_shared_bodies_under_base_table) {
    FactoryScope scope;
    g_el = Seen{};
    alignas(8) unsigned char buf[sizeof(RbElement)];
    RbElement* e = BuildIn<RbElement>(buf, 0xCD);
    CHECK_EQ(g_el.initCalls, 1);
    CHECK(g_el.initVptr == ElVt());
    CHECK(VptrOf(e) != ElVt());
    CHECK_EQ(e->_alignment, 3);
    e->~RbElement();
    CHECK_EQ(g_el.releaseCalls, 1);
    CHECK(g_el.releaseVptr == ElVt());
}

UNIT_TEST(rb_elementdescription_ctor_dtor_run_shared_bodies_under_base_table) {
    g_ed = Seen{};
    alignas(8) unsigned char buf[sizeof(RbElementDesc)];
    RbElementDesc* d = BuildIn<RbElementDesc>(buf, 0xCD);
    CHECK_EQ(g_ed.initCalls, 1);
    CHECK(g_ed.initVptr == EdVt());
    CHECK(VptrOf(d) != EdVt());
    // External() ran first (retail inlines its three zero stores).
    CHECK_EQ(Q(d, 0x08), (uint64_t)0);
    CHECK_EQ(Q(d, 0x10), (uint64_t)0);
    CHECK_EQ(Q(d, 0x18), (uint64_t)0);
    d->~RbElementDesc();
    CHECK_EQ(g_ed.releaseCalls, 1);
    CHECK(g_ed.releaseVptr == EdVt());
}

// ===========================================================================
// PART B — the door reaches the same body with the MINTED table in place
// ===========================================================================
UNIT_TEST(rb_dialoginstance_door_and_ctor_share_one_body) {
    FactoryScope scope;
    int dialogToken = 0, dsToken = 0, userToken = 0;
    Object::InternalPtr<UI::DialogInstance> parent;

    alignas(8) unsigned char viaCtor[sizeof(RbDialog)];
    std::memset(viaCtor, 0, sizeof viaCtor);
    g_di = Seen{};
    RbDialog* d = ::new (static_cast<void*>(viaCtor)) RbDialog(
        nullptr, reinterpret_cast<const UI::Dialog*>(&dialogToken), parent,
        reinterpret_cast<Object::Base*>(&dsToken), &userToken, 7);

    alignas(8) unsigned char viaDoor[sizeof(RbDialog)];
    std::memcpy(viaDoor, viaCtor, sizeof viaDoor);          // same Internal half
    std::memset(viaDoor + 0x10, 0, sizeof viaDoor - 0x10);  // fields unwritten
    *reinterpret_cast<const void**>(viaDoor) = kFakeMinted; // the door's stamp
    g_di = Seen{};
    UI::DialogInstance::DoorBody::Init(
        reinterpret_cast<UI::DialogInstance*>(viaDoor), nullptr,
        reinterpret_cast<const UI::Dialog*>(&dialogToken), &parent,
        reinterpret_cast<Object::Base*>(&dsToken), &userToken, 7);
    CHECK_EQ(g_di.initCalls, 1);
    CHECK(g_di.initVptr == static_cast<const void*>(kFakeMinted));
    // Same body, same arguments -> the same bytes after the vptr.
    CHECK(std::memcmp(viaCtor + 0x10, viaDoor + 0x10, sizeof(UI::DialogInstance) - 0x10) == 0);

    g_di = Seen{};
    UI::DialogInstance::DoorBody::Release(reinterpret_cast<UI::DialogInstance*>(viaDoor));
    CHECK_EQ(g_di.releaseCalls, 1);
    CHECK(g_di.releaseVptr == static_cast<const void*>(kFakeMinted));
    d->~RbDialog();
}

UNIT_TEST(rb_element_and_description_doors_reach_the_same_body) {
    alignas(8) unsigned char raw[sizeof(UI::Element)] = {};
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    g_el = Seen{};
    UI::Element::DoorBody::Init(reinterpret_cast<UI::Element*>(raw));
    CHECK_EQ(g_el.initCalls, 1);
    CHECK(g_el.initVptr == static_cast<const void*>(kFakeMinted));
    UI::Element::DoorBody::Release(reinterpret_cast<UI::Element*>(raw));
    CHECK_EQ(g_el.releaseCalls, 1);
    CHECK(g_el.releaseVptr == static_cast<const void*>(kFakeMinted));

    alignas(8) unsigned char rawD[sizeof(UI::ElementDescription)] = {};
    *reinterpret_cast<const void**>(rawD) = kFakeMinted;
    g_ed = Seen{};
    UI::ElementDescription::DoorBody::Init(reinterpret_cast<UI::ElementDescription*>(rawD));
    CHECK_EQ(g_ed.initCalls, 1);
    CHECK(g_ed.initVptr == static_cast<const void*>(kFakeMinted));
    CHECK_EQ(D(reinterpret_cast<UI::ElementDescription*>(rawD), 0x3c), 0xFFFFFFFFu);
    UI::ElementDescription::DoorBody::Release(reinterpret_cast<UI::ElementDescription*>(rawD));
    CHECK_EQ(g_ed.releaseCalls, 1);
}

// ===========================================================================
// PART C — the inline retail bodies, byte for byte
// ===========================================================================
UNIT_TEST(rb_dialoginstance_default_ctor_is_retail_0x180114ca0) {
    FactoryScope scope;
    alignas(8) unsigned char buf[sizeof(RbDialog)];
    RbDialog* d = BuildIn<RbDialog>(buf, 0xCD);
    const uint64_t kUnwritten8 = 0xCDCDCDCDCDCDCDCDull;
    CHECK_EQ(Q(d, 0x10), (uint64_t)0);          // _ui
    CHECK_EQ(Q(d, 0x18), (uint64_t)0);          // _dialog
    CHECK_EQ(Q(d, 0x20), (uint64_t)0);          // _flags + _result
    CHECK_EQ(Q(d, 0x28), (uint64_t)0);          // _parent
    CHECK_EQ(Q(d, 0x30), kUnwritten8);          // _dataSource: NOT written by 0x180114ca0
    CHECK_EQ(Q(d, 0x38), (uint64_t)0);          // _children head
    CHECK_EQ(Q(d, 0x40), (uint64_t)0);          //           tail
    CHECK_EQ(D(d, 0x48), 0u);                   //           count (DWORD)
    CHECK_EQ(D(d, 0x4c), 0xCDCDCDCDu);          // hole
    CHECK_EQ(Q(d, 0x50), (uint64_t)0);          // _root
    CHECK_EQ(Q(d, 0x58), (uint64_t)0);          // _spriteSheet
    CHECK_EQ(D(d, 0x60), 0xFFFFFFFFu);          // _spriteIndex
    CHECK_EQ(D(d, 0x64), 0xCDCDCDCDu);          // hole
    for (size_t off = 0x68; off < 0x90; off += 8)
        CHECK_EQ(Q(d, off), (uint64_t)0);       // _timers, _controllers, upper-left
    CHECK_EQ(Q(d, 0x90), ~(uint64_t)0);         // both hover ints, one QWORD -1
    CHECK_EQ((int)By(d, 0x98), 0);              // _hovering (BYTE)
    CHECK_EQ((int)By(d, 0x99), 0xCD);           // hole
    CHECK_EQ(Q(d, 0xa0), (uint64_t)0);          // _hoverTime
    CHECK_EQ(Q(d, 0xa8), (uint64_t)0);          // _userData
    CHECK_EQ(Q(d, 0xb0), kUnwritten8);          // _userFlags + hole: NOT written
    d->~RbDialog();
}

UNIT_TEST(rb_dialoginstance_slot0_is_the_exe_scalar_deleting_dtor) {
    FactoryScope scope;
    g_di = Seen{};
    // Retail 0x140020cb0: ~DialogInstance, then MemFree when flags & 1.
    void* mem = Object::detail::InternalMemAlloc(sizeof(UI::DialogInstance), 8);
    UI::DialogInstance* d = ::new (mem) UI::DialogInstance();
    CHECK(VptrOf(d) == DiVt());
    using Slot0 = Object::Base* (*)(void*, unsigned);
    Slot0 slot0 = reinterpret_cast<Slot0>(const_cast<void*>(
        static_cast<const void* const*>(DiVt())[0]));
    Object::Base* r = slot0(d, 1u);
    CHECK(static_cast<void*>(r) == mem);
    CHECK_EQ(g_di.releaseCalls, 1);
    CHECK(g_di.releaseVptr == DiVt());
}

UNIT_TEST(rb_dialogcontroller_ctor_is_retail_0x1800cd320) {
    FactoryScope scope;
    alignas(8) unsigned char buf[sizeof(RbPanel)];
    RbPanel* c = BuildIn<RbPanel>(buf, 0xCD);
    CHECK_EQ(Q(c, 0x10), (uint64_t)0);                        // _config
    CHECK_EQ(Q(c, 0x18), (uint64_t)0);                        // _dialog
    CHECK_EQ(Q(c, 0x20), (uint64_t)0);                        // _container
    CHECK_EQ(Q(c, 0x28), 0xCDCDCDCDCDCDCDCDull);              // _dataSource: NOT written
    c->~RbPanel();
}

UNIT_TEST(rb_dialogcontrollerconfig_ctor_is_retail_0x18003b4a0) {
    alignas(8) unsigned char buf[sizeof(RbConfig)];
    RbConfig* c = BuildIn<RbConfig>(buf, 0xCD);
    CHECK_EQ(Q(c, 0x08), (uint64_t)0);
    CHECK_EQ(Q(c, 0x10), (uint64_t)0);
    CHECK_EQ(Q(c, 0x18), (uint64_t)0);
    c->~RbConfig();
}

UNIT_TEST(rb_ui_bases_shape) {
    CHECK_EQ(sizeof(UI::DialogInstance), (size_t)0xb8);
    CHECK_EQ(sizeof(UI::DialogController), (size_t)0x30);
    CHECK_EQ(sizeof(UI::DialogControllerConfig), (size_t)0x20);
    CHECK_EQ(sizeof(UI::Element), (size_t)0xc0);
    CHECK_EQ(sizeof(UI::ElementDescription), (size_t)0x68);
    CHECK(!std::is_abstract<UI::DialogInstance>::value);   // the exe instantiates it
    CHECK(std::is_abstract<UI::DialogController>::value);
    CHECK(std::is_abstract<UI::DialogControllerConfig>::value);
    CHECK(std::is_abstract<UI::Element>::value);
    CHECK(std::is_abstract<UI::ElementDescription>::value);
    CHECK(!std::has_virtual_destructor<UI::DialogInstance>::value);
    CHECK(!std::has_virtual_destructor<UI::DialogController>::value);
    CHECK(!std::has_virtual_destructor<UI::DialogControllerConfig>::value);
    CHECK(!std::has_virtual_destructor<UI::Element>::value);
    CHECK(!std::has_virtual_destructor<UI::ElementDescription>::value);
}

// ===========================================================================
// PART D — the two UI resources the exe instantiates itself (B38_SLOTS
// section 4: SpriteSheet 0x14015c3b0, SpriteCapture 0x14015d550). Their
// InitFields are inline (the REAL retail bodies, 0x18011d600 / 0x1800d9140);
// their virtuals and the out-of-line dtors are ui_resources_abi_test.cpp's
// markers. Slot 0 is the exe scalar deleting dtor (0x1400121e0 / 0x140013f80).
// ===========================================================================
UNIT_TEST(rb_spritesheet_ctor_is_retail_0x18011d600_and_concrete) {
    alignas(8) unsigned char buf[sizeof(UI::SpriteSheet)];
    std::memset(buf, 0xCD, sizeof buf);
    UI::SpriteSheet* s = ::new (static_cast<void*>(buf)) UI::SpriteSheet();
    CHECK(VptrOf(s) == &g_rbSsVtGroup[2]);
    for (size_t off = 0x08; off < 0x50; off += 8) CHECK_EQ(Q(s, off), (uint64_t)0);
    CHECK_EQ((int)By(s, 0x50), 0);
    CHECK_EQ((int)By(s, 0x51), 0xCD);            // never written
    alignas(8) unsigned char raw[sizeof(UI::SpriteSheet)];
    std::memset(raw, 0xCD, sizeof raw);
    *reinterpret_cast<const void**>(raw) = kFakeMinted;
    UI::SpriteSheet::DoorBody::Init(reinterpret_cast<UI::SpriteSheet*>(raw));
    CHECK(std::memcmp(buf + 0x20, raw + 0x20, sizeof raw - 0x20) == 0);
    s->DeletingDtor(0);                           // destruct only (stack buffer)

    void* mem = Object::detail::InternalMemAlloc(sizeof(UI::SpriteSheet), 8);
    UI::SpriteSheet* h = ::new (mem) UI::SpriteSheet();
    using Slot0 = Object::Base* (*)(void*, unsigned);
    CHECK(static_cast<void*>(reinterpret_cast<Slot0>(const_cast<void*>(g_rbSsVtGroup[2]))(h, 1u)) == mem);
}

UNIT_TEST(rb_spritecapture_ctor_is_retail_0x1800d9140_and_concrete) {
    alignas(8) unsigned char buf[sizeof(UI::SpriteCapture)];
    std::memset(buf, 0xCD, sizeof buf);
    UI::SpriteCapture* c = ::new (static_cast<void*>(buf)) UI::SpriteCapture();
    CHECK(VptrOf(c) == &g_rbScVtGroup[2]);
    for (size_t off = 0x08; off < 0x58; off += 8) CHECK_EQ(Q(c, off), (uint64_t)0);
    // ENDGAME B.3.9 UI-D: the DoorBody bridge is retired with the ctor door;
    // the real ctor is the only body.
    c->DeletingDtor(0);

    void* mem = Object::detail::InternalMemAlloc(sizeof(UI::SpriteCapture), 8);
    UI::SpriteCapture* h = ::new (mem) UI::SpriteCapture();
    using Slot0 = Object::Base* (*)(void*, unsigned);
    CHECK(static_cast<void*>(reinterpret_cast<Slot0>(const_cast<void*>(g_rbScVtGroup[2]))(h, 1u)) == mem);
}

}  // namespace
