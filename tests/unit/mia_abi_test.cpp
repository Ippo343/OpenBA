// ============================================================================
// tests/unit/mia_abi_test.cpp — ENDGAME B.3.8 block MI-A (B38_RULES.md B8-14)
//
// The ABI claims block MI-A makes for its town `ControllerComponent` leaves,
// measured on the SHIPPED HEADERS (code/Game/Component/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create function's
//   MemAlloc (B38_RULES §1.3), the controller member at retail's per-class
//   offset (B38_SLOTS.md §2: slot 17 `LEA RAX,[RCX+off]`), its back-pointer at
//   off+0x18, and the own fields the ported bodies read raw.
// PART B — SHAPE. Every class is concrete, `final`, single inheritance from
//   `ControllerComponent` (so from Component and Object::Internal), polymorphic,
//   NO virtual destructor.
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): every override lands on the retail
//   index of B38_SLOTS.md §6; 16/17 are GetDefaultState/GetController.
// PART D — the member's table: a raw retail-shaped dispatch through
//   `TableController<Tag>` of one of this block's tags reaches the handler with
//   RCX = the owner (every retail member slot body: `MOV RCX,[RCX+0x18]; JMP`).
//
// NOT PROVEN HERE: the bodies and the constructors (they stand on the address
// registry and cannot link into a host test), and `Vtable() == vptr`
// (tests/unit/exe_class_abi_test.cpp PART C proves DEFINE_EXE_VTABLE on this
// toolchain). No slot of these classes is a retslot or takes a hidden pointer.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Component/AbsorbResourceComponent.h"
#include "Component/AttendanceComponent.h"
#include "Component/BoatComponent.h"
#include "Component/BuildComponent.h"
#include "Component/CemeteryComponent.h"
#include "Component/ClearAreaComponent.h"
#include "Component/ClearResourceComponent.h"
#include "Component/DestroyComponent.h"
#include "Component/HappinessComponent.h"
#include "Component/HealerComponent.h"
#include "Component/ImmigrationComponent.h"
#include "Component/NomadComponent.h"
#include "Component/ResidenceComponent.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {


// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(mia_component_sizes_match_retail) {
    CHECK_EQ(sizeof(AbsorbResourceComponent),(size_t)0x48);   // create 0x14002ea20
    CHECK_EQ(sizeof(AttendanceComponent),    (size_t)0x58);   // create 0x140002670
    CHECK_EQ(sizeof(BoatComponent),          (size_t)0x98);   // create 0x1400036e0
    CHECK_EQ(sizeof(BuildComponent),         (size_t)0x78);   // create 0x14002f2b0
    CHECK_EQ(sizeof(CemeteryComponent),      (size_t)0x58);   // create 0x140004040
    CHECK_EQ(sizeof(ClearAreaComponent),     (size_t)0x50);   // create 0x140030ea0
    CHECK_EQ(sizeof(ClearResourceComponent), (size_t)0x48);   // create 0x1400315a0
    CHECK_EQ(sizeof(DestroyComponent),       (size_t)0x68);   // create 0x140033bc0
    CHECK_EQ(sizeof(HappinessComponent),     (size_t)0x50);   // create 0x140005ba0
    CHECK_EQ(sizeof(HealerComponent),        (size_t)0x48);   // create 0x140006490
    CHECK_EQ(sizeof(ImmigrationComponent),   (size_t)0x68);   // create 0x140007a20
    CHECK_EQ(sizeof(NomadComponent),         (size_t)0x78);   // create 0x140009020
    CHECK_EQ(sizeof(ResidenceComponent),     (size_t)0x60);   // create 0x140037440
}

template <typename T>
void CheckMember(size_t off) {
    CHECK_EQ(offsetof(T, _controller), off);
    CHECK_EQ(offsetof(T, _controller) + offsetof(ControllerComponent::Controller, _owner), off + 0x18);
    CHECK_EQ(sizeof(typename T::StateController), (size_t)0x20);
}

UNIT_TEST(mia_controller_member_at_retail_offset) {
    CheckMember<AbsorbResourceComponent>(0x28);  // slot 17 0x140004080 (D9 #8: the tree had none)
    CheckMember<AttendanceComponent>(0x38);      // slot 17 0x1400026b0
    CheckMember<BoatComponent>(0x78);            // slot 17 0x140003720
    CheckMember<BuildComponent>(0x28);           // slot 17 0x140004080
    CheckMember<CemeteryComponent>(0x28);
    CheckMember<ClearAreaComponent>(0x28);
    CheckMember<ClearResourceComponent>(0x28);
    CheckMember<DestroyComponent>(0x28);
    CheckMember<HappinessComponent>(0x30);       // slot 17 0x140005be0
    CheckMember<HealerComponent>(0x28);
    CheckMember<ImmigrationComponent>(0x28);
    CheckMember<NomadComponent>(0x58);           // slot 17 0x140009060
    CheckMember<ResidenceComponent>(0x28);
}

UNIT_TEST(mia_component_member_offsets) {
    CHECK_EQ(offsetof(AttendanceComponent, field28),       (size_t)0x28);   // waiting queue
    CHECK_EQ(offsetof(AttendanceComponent, field30),       (size_t)0x30);
    CHECK_EQ(offsetof(BoatComponent, data28),              (size_t)0x28);
    CHECK_EQ(offsetof(BoatComponent, data60),              (size_t)0x60);   // route array
    CHECK_EQ(offsetof(BoatComponent, data70),              (size_t)0x70);
    CHECK_EQ(offsetof(BuildComponent, buildState),         (size_t)0x48);
    CHECK_EQ(offsetof(BuildComponent, buildProgress),      (size_t)0x4c);
    CHECK_EQ(offsetof(BuildComponent, buildFlag),          (size_t)0x50);
    CHECK_EQ(offsetof(BuildComponent, field58),            (size_t)0x58);
    CHECK_EQ(offsetof(BuildComponent, field70),            (size_t)0x70);
    CHECK_EQ(offsetof(CemeteryComponent, plots),           (size_t)0x48);
    CHECK_EQ(offsetof(CemeteryComponent, plotCapacity),    (size_t)0x54);
    CHECK_EQ(offsetof(ClearAreaComponent, clearState),     (size_t)0x48);
    CHECK_EQ(offsetof(DestroyComponent, destroyFlags),     (size_t)0x50);
    CHECK_EQ(offsetof(DestroyComponent, field54),          (size_t)0x54);
    CHECK_EQ(offsetof(DestroyComponent, field60),          (size_t)0x60);
    CHECK_EQ(offsetof(HappinessComponent, happinessFlag),  (size_t)0x28);
    CHECK_EQ(offsetof(ImmigrationComponent, field58),      (size_t)0x58);
    CHECK_EQ(offsetof(NomadComponent, field50),            (size_t)0x50);
    CHECK_EQ(offsetof(ResidenceComponent, heatRemaining),  (size_t)0x48);
    CHECK_EQ(offsetof(ResidenceComponent, residentSlots),  (size_t)0x50);
    CHECK_EQ(offsetof(ResidenceComponent, residentSlotCount), (size_t)0x58);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
template <typename T>
void CheckLeafShape() {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<ControllerComponent, T>::value));
    CHECK((std::is_base_of<Component, T>::value));
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK(!std::has_virtual_destructor<typename T::StateController>::value);
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, Persist,         3);
    CHECK_SLOT(T, GetDefaultState, 16);
    CHECK_SLOT(T, GetController,   17);
}

UNIT_TEST(mia_component_class_shape_rules) {
    CheckLeafShape<AbsorbResourceComponent>();
    CheckLeafShape<AttendanceComponent>();
    CheckLeafShape<BoatComponent>();
    CheckLeafShape<BuildComponent>();
    CheckLeafShape<CemeteryComponent>();
    CheckLeafShape<ClearAreaComponent>();
    CheckLeafShape<ClearResourceComponent>();
    CheckLeafShape<DestroyComponent>();
    CheckLeafShape<HappinessComponent>();
    CheckLeafShape<HealerComponent>();
    CheckLeafShape<ImmigrationComponent>();
    CheckLeafShape<NomadComponent>();
    CheckLeafShape<ResidenceComponent>();
}

// ===========================================================================
// PART C — SLOT ORDER (the leaf's own slots, B38_SLOTS.md §6)
// ===========================================================================
UNIT_TEST(mia_component_own_slots_land_on_retail_indices) {
    CHECK_SLOT(AttendanceComponent, OnRemove,    7);    // 0x1400443d0
    CHECK_SLOT(AttendanceComponent, Update,      9);    // 0x140044370
    CHECK_SLOT(AttendanceComponent, CommandMask, 13);   // 0x140043f10
    CHECK_SLOT(AttendanceComponent, Command,     14);   // 0x140043e80
    CHECK_SLOT(BuildComponent, Startup,          4);    // 0x140108d60
    CHECK_SLOT(BuildComponent, OnAdded,          6);    // 0x140108d40
    CHECK_SLOT(BuildComponent, OnRemove,         7);    // 0x140108cb0
    CHECK_SLOT(BuildComponent, Slot11,           11);   // 0x140108d70
    CHECK_SLOT(BuildComponent, OnMessage,        15);   // 0x140108710
    CHECK_SLOT(CemeteryComponent, Startup,       4);    // 0x1400475b0
    CHECK_SLOT(CemeteryComponent, OnAdded,       6);    // 0x140047200
    CHECK_SLOT(CemeteryComponent, OnRemove,      7);    // 0x1400471c0
    CHECK_SLOT(DestroyComponent, Startup,        4);    // 0x14011db00
    CHECK_SLOT(DestroyComponent, OnMessage,      15);   // 0x14011daf0
    CHECK_SLOT(HappinessComponent, OnRemove,     7);    // 0x14004aa20
    CHECK_SLOT(HappinessComponent, OnMessage,    15);   // 0x14004a780
    CHECK_SLOT(HealerComponent, OnRemove,        7);    // 0x14004ae70
    CHECK_SLOT(NomadComponent, OnRemove,         7);    // 0x14004fd20
    CHECK_SLOT(ResidenceComponent, Startup,      4);    // 0x14012a120
    CHECK_SLOT(ResidenceComponent, OnRemove,     7);    // 0x14012a0e0
}

// ===========================================================================
// PART D — raw dispatch through one of this block's member types
// ===========================================================================
void* g_miaSeenData = nullptr;
void* g_miaSeenSm   = nullptr;
char MiaEnterHandler(void* data, void* sm) { g_miaSeenData = data; g_miaSeenSm = sm; return 5; }

UNIT_TEST(mia_controller_member_raw_dispatch) {
    using Ctl = ClearResourceComponent::StateController;
    using Dispatch = ::ControllerLive::Dispatcher<ClearResourceCtrlTag>;
    auto& t = Dispatch::table;
    const ::ControllerLive::StateTable saved = t;   // empty in the host test (no static init linked)
    t = {};
    ::ControllerLive::TableRegisterState(t, 2, reinterpret_cast<void*>(&MiaEnterHandler),
                                         nullptr, nullptr, nullptr);
    ::ControllerLive::TableAddTransition(t, 3, nullptr);

    alignas(8) unsigned char owner[0x48] = {};
    Ctl node(reinterpret_cast<Component*>(owner));
    void** vt = *reinterpret_cast<void***>(&node);
    CHECK_EQ(reinterpret_cast<int (*)(void*, int)>(vt[0])(&node, 2), 0);          // FindState
    int desired = 3;
    CHECK_EQ(reinterpret_cast<int (*)(void*, int, int*)>(vt[1])(&node, 0, &desired), 0);
    desired = 7;                                                                  // not a transition of state 2
    CHECK_EQ(reinterpret_cast<int (*)(void*, int, int*)>(vt[1])(&node, 0, &desired), -1);
    CHECK_EQ(desired, -1);                                                        // retail miss arm
    int smToken = 0;
    CHECK_EQ((int)reinterpret_cast<char (*)(void*, void*, int)>(vt[3])(&node, &smToken, 0), 5);
    CHECK(g_miaSeenData == static_cast<void*>(owner));                             // RCX = [node+0x18]
    CHECK(g_miaSeenSm == static_cast<void*>(&smToken));
    CHECK_EQ((int)reinterpret_cast<char (*)(void*)>(vt[8])(&node), 1);            // HasStates
    t = saved;
}

}  // namespace
