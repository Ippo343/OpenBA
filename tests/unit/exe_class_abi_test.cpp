// ============================================================================
// tests/unit/exe_class_abi_test.cpp — ENDGAME B.3.8 pilot (B38_RULES.md D1-D6)
//
// The ABI claims the step-8 pilot makes, measured on the SHIPPED HEADERS:
//
// PART A — LAYOUT. sizeof of `Component`, `ControllerComponent`, the embedded
//   controller, `WellComponent`, `Description`, `WellDescription`; the
//   controller member at WellComponent+0x28 and its back-pointer at +0x40
//   (retail ctor 0x14005d060, slot 17 0x140004080 `LEA RAX,[RCX+0x28]`).
// PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0 for single inheritance) — the
//   header under test, not a copy. Component 16 slots, ControllerComponent 18,
//   the 9-slot controller interface, Description 9 + the leaf's slot 9.
//   `CHECK_SLOT` (slot_check.h) also FAILS on a this-adjusting PMF: there is no multiple
//   inheritance in this family (D6), so every adjustment must be 0.
// PART C — D5, `DEFINE_EXE_VTABLE`: the address `Class::Vtable()` hands to
//   `RegisterExeAddr` is exactly the vptr a constructed object carries, on
//   this toolchain, for a base and a derived class.
// PART D — D6, the table-driven controller member: a raw retail-shaped call
//   through the member's COMPILER vtable (`[vptr+0x18](node, sm, idx)`, what
//   StateMachine::Tick 0x1400a9660 does) reaches the per-state handler with
//   RCX = the owner stored at node+0x18 and RDX = sm, exactly as the retail
//   dispatcher's `MOV RCX,[RCX+0x18]; JMP handler`.
//
// NOT PROVEN HERE: the bodies of WellComponent / WellDescription (they stand
// on the address registry and cannot link into a host test) — the real run
// and the resave gate verify those.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Component/WellComponent.h"
#include "Component/WellDescription.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

using Ctl = ControllerComponent::Controller;

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(exe_component_sizes_match_retail) {
    CHECK_EQ(sizeof(Component),           (size_t)0x28);   // 0x10 Internal + 0x18
    CHECK_EQ(sizeof(ControllerComponent), (size_t)0x28);   // adds no bytes (D6)
    CHECK_EQ(sizeof(Ctl),                 (size_t)0x20);   // vptr, +8, +0x10, owner
    CHECK_EQ(sizeof(WellComponent),       (size_t)0x48);   // create 0x14000fc80 MemAlloc(0x48)
    CHECK_EQ(sizeof(Description),         (size_t)0x20);   // adds no bytes to External
    CHECK_EQ(sizeof(WellDescription),     (size_t)0x20);   // create 0x14000fce0 MemAlloc(0x20)
    CHECK_EQ(offsetof(Component, _entity),      (size_t)0x10);
    CHECK_EQ(offsetof(Component, _description), (size_t)0x18);
    CHECK_EQ(offsetof(Component, _flags),       (size_t)0x20);
    CHECK_EQ(offsetof(WellComponent, _controller), (size_t)0x28);
    CHECK_EQ(offsetof(WellComponent, _controller) + offsetof(Ctl, _owner), (size_t)0x40);
}

UNIT_TEST(exe_component_class_shape_rules) {
    // B3X W1: polymorphic with a virtual destructor (retail slot 0 = D1 + D0); the Controller interface (slot 0 FindState) has none.
    CHECK(std::is_polymorphic<Component>::value);
    CHECK(std::has_virtual_destructor<Component>::value);
    CHECK(std::has_virtual_destructor<WellComponent>::value);
    CHECK(std::has_virtual_destructor<WellDescription>::value);
    CHECK(!std::has_virtual_destructor<Ctl>::value);
    CHECK(std::is_abstract<ControllerComponent>::value);   // 16/17 pure (0x140162c00/08)
    CHECK(std::is_abstract<Ctl>::value);                   // 9 x _purecall (0x14015f070)
    CHECK(std::is_abstract<Description>::value);           // 3, 5-8 pure
    CHECK(!std::is_abstract<WellComponent>::value);
    CHECK(!std::is_abstract<WellDescription>::value);
    // D6: single inheritance, the controller is a MEMBER.
    CHECK((std::is_base_of<Object::Internal, WellComponent>::value));
    CHECK(!(std::is_base_of<Ctl, WellComponent>::value));
}

// ===========================================================================
// PART B — SLOT ORDER
// ===========================================================================
UNIT_TEST(exe_component_slot_order) {
    CHECK_DTOR_SLOT(Component);
    CHECK_SLOT(Component, TypeName,    0x08/8);
    CHECK_SLOT(Component, Type,        0x10/8);
    CHECK_SLOT(Component, Persist,     0x18/8);
    CHECK_SLOT(Component, Startup,     0x20/8);
    CHECK_SLOT(Component, Slot5,       0x28/8);
    CHECK_SLOT(Component, OnAdded,     0x30/8);
    CHECK_SLOT(Component, OnRemove,    0x38/8);
    CHECK_SLOT(Component, Tick,        0x40/8);
    CHECK_SLOT(Component, Update,      0x48/8);
    CHECK_SLOT(Component, PostUpdate,  0x50/8);
    CHECK_SLOT(Component, Slot11,      0x58/8);
    CHECK_SLOT(Component, OnEvent,     0x60/8);
    CHECK_SLOT(Component, CommandMask, 0x68/8);
    CHECK_SLOT(Component, Command,     0x70/8);
    CHECK_SLOT(Component, OnMessage,   0x78/8);

    CHECK_SLOT(ControllerComponent, Persist,         0x18/8);
    CHECK_SLOT(ControllerComponent, GetDefaultState, 0x80/8);
    CHECK_SLOT(ControllerComponent, GetController,   0x88/8);

    // The leaf overrides land on retail's indices.
    CHECK_DTOR_SLOT(WellComponent);   // 0x14000fe50
    CHECK_SLOT(WellComponent, Persist,         0x18/8);   // 0x14005cfa0
    CHECK_SLOT(WellComponent, OnRemove,        0x38/8);   // 0x14005cf60
    CHECK_SLOT(WellComponent, GetDefaultState, 0x80/8);   // 0x1400026c0
    CHECK_SLOT(WellComponent, GetController,   0x88/8);   // 0x140004080
}

UNIT_TEST(exe_controller_interface_slot_order) {
    CHECK_SLOT(Ctl, FindState,      0x00/8);
    CHECK_SLOT(Ctl, FindTransition, 0x08/8);
    CHECK_SLOT(Ctl, PostEvent,      0x10/8);
    CHECK_SLOT(Ctl, Enter,          0x18/8);
    CHECK_SLOT(Ctl, Update,         0x20/8);
    CHECK_SLOT(Ctl, OnTransition,   0x28/8);
    CHECK_SLOT(Ctl, Tick,           0x30/8);
    CHECK_SLOT(Ctl, TickAlt,        0x38/8);
    CHECK_SLOT(Ctl, HasStates,      0x40/8);   // 9th and last slot
}

UNIT_TEST(exe_description_slot_order) {
    CHECK_DTOR_SLOT(Description);
    CHECK_SLOT(Description, TypeName,         0x08/8);
    CHECK_SLOT(Description, Type,             0x10/8);
    CHECK_SLOT(Description, GetFileVersion,   0x18/8);
    CHECK_SLOT(Description, Compile,          0x20/8);
    CHECK_SLOT(Description, Serialize,        0x28/8);
    CHECK_SLOT(Description, Reconstruct,      0x30/8);
    CHECK_SLOT(Description, Destruct,         0x38/8);
    CHECK_SLOT(Description, GetComponentType, 0x40/8);
    CHECK_SLOT(WellDescription, Construct,    0x48/8);   // leaf slot 9, 0x14000fd40
}

// ===========================================================================
// PART C — D5: DEFINE_EXE_VTABLE answers the vptr a constructed object carries
// ===========================================================================
}  // namespace

// Global-namespace wrappers so the Itanium names are simple to spell. The
// linker is the checker: a wrong spelling is an undefined reference.
struct B38VtProbe {
    virtual int A();
    virtual int B();
    static const void* Vtable();
};
struct B38VtProbeLeaf : B38VtProbe {
    int B() override;
    static const void* Vtable();
};
int B38VtProbe::A() { return 1; }
int B38VtProbe::B() { return 2; }
int B38VtProbeLeaf::B() { return 3; }
DEFINE_EXE_VTABLE(B38VtProbe, "_ZTV10B38VtProbe")
DEFINE_EXE_VTABLE(B38VtProbeLeaf, "_ZTV14B38VtProbeLeaf")

namespace {

UNIT_TEST(exe_vtable_handle_is_the_objects_vptr) {
    B38VtProbe base;
    B38VtProbeLeaf leaf;
    const void* vBase = *reinterpret_cast<const void* const*>(&base);
    const void* vLeaf = *reinterpret_cast<const void* const*>(&leaf);
    CHECK(vBase == B38VtProbe::Vtable());
    CHECK(vLeaf == B38VtProbeLeaf::Vtable());
    CHECK(vBase != vLeaf);
    // A slot read through the handle is the override the object dispatches to.
    using Fn = int (*)(void*);
    auto slot = [](const void* vt, int i) {
        return reinterpret_cast<Fn>(reinterpret_cast<void* const*>(vt)[i]);
    };
    CHECK_EQ(slot(B38VtProbeLeaf::Vtable(), 0)(&leaf), 1);
    CHECK_EQ(slot(B38VtProbeLeaf::Vtable(), 1)(&leaf), 3);
    CHECK_EQ(slot(B38VtProbe::Vtable(), 1)(&base), 2);
    // The registry row a converted class keeps (D5) stores exactly this value;
    // an unconverted by-address reader that stamps it builds an object whose
    // dispatch is the compiler table's.
    uintptr_t served = reinterpret_cast<uintptr_t>(B38VtProbeLeaf::Vtable());
    B38VtProbeLeaf stamped;
    *reinterpret_cast<const void**>(&stamped) = reinterpret_cast<const void*>(served);
    CHECK_EQ(stamped.B(), 3);
}

// ===========================================================================
// PART D — D6: raw retail-shaped dispatch through the controller member
// ===========================================================================
struct B38TestCtrlTag {};
using TestCtl = ::ControllerLive::TableController<B38TestCtrlTag>;
using TestDispatch = ::ControllerLive::Dispatcher<B38TestCtrlTag>;

void* g_seenData = nullptr;
void* g_seenSm   = nullptr;
char  EnterHandler(void* data, void* sm) { g_seenData = data; g_seenSm = sm; return 7; }

UNIT_TEST(exe_controller_member_raw_dispatch) {
    auto& t = TestDispatch::table;
    t = {};
    ::ControllerLive::TableRegisterState(t, 2, reinterpret_cast<void*>(&EnterHandler),
                                         nullptr, nullptr, nullptr);
    ::ControllerLive::TableAddTransition(t, 3, nullptr);

    alignas(8) unsigned char owner[0x48] = {};
    TestCtl node(reinterpret_cast<Component*>(owner));
    unsigned char* raw = reinterpret_cast<unsigned char*>(&node);
    CHECK(*reinterpret_cast<void**>(raw + 0x18) == static_cast<void*>(owner));   // member+0x18
    CHECK(*reinterpret_cast<void**>(raw + 0x08) == nullptr);
    CHECK(*reinterpret_cast<void**>(raw + 0x10) == nullptr);

    void** vt = *reinterpret_cast<void***>(&node);
    // slot 0 (+0x00) FindState(node, id)
    CHECK_EQ(reinterpret_cast<int (*)(void*, int)>(vt[0])(&node, 2), 0);
    CHECK_EQ(reinterpret_cast<int (*)(void*, int)>(vt[0])(&node, 9), -1);
    // slot 1 (+0x08) FindTransition(node, stateIndex, int* desired)
    int desired = 3;
    CHECK_EQ(reinterpret_cast<int (*)(void*, int, int*)>(vt[1])(&node, 0, &desired), 0);
    CHECK_EQ(desired, 3);                                   // a hit leaves *desired alone
    desired = 42;                                           // a miss writes -1 (retail 0x140010103)
    CHECK_EQ(reinterpret_cast<int (*)(void*, int, int*)>(vt[1])(&node, 0, &desired), -1);
    CHECK_EQ(desired, -1);
    // slot 3 (+0x18) Enter(node, sm, idx) -> handler(node+0x18, sm)
    int smToken = 0;
    char r = reinterpret_cast<char (*)(void*, void*, int)>(vt[3])(&node, &smToken, 0);
    CHECK_EQ((int)r, 7);
    CHECK(g_seenData == static_cast<void*>(owner));
    CHECK(g_seenSm == static_cast<void*>(&smToken));
    // slot 8 (+0x40): retail `count != 0` (Well 0x140010260), B38_RULES D9 #9.
    CHECK_EQ((int)reinterpret_cast<char (*)(void*)>(vt[8])(&node), 1);
    t = {};
}

}  // namespace
