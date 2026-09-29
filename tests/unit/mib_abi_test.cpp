// ============================================================================
// tests/unit/mib_abi_test.cpp — ENDGAME B.3.8 block MI-B (B38_RULES.md B8-14)
//
// The ABI claims block MI-B makes for its production/trade classes, measured on
// the SHIPPED HEADERS (code/Game/Component/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create function's
//   MemAlloc (B38_RULES §1.3), the controller member at retail's offset
//   (B38_SLOTS.md §2), its back-pointer at off+0x18, the own fields the ported
//   bodies read raw.
// PART B — SHAPE. ResourceProduceComponent is abstract (slot 18 pure); every leaf
//   is concrete, `final`, single inheritance, polymorphic, NO virtual destructor.
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): every override lands on the retail
//   index of B38_SLOTS.md §6 — RP's 18-21 included (22 slots).
// PART D — the shared RP member: a raw retail-shaped dispatch through
//   `TableController<RPCtrlTag>` reaches the handler with RCX = the owner.
//
// NOT PROVEN HERE: the bodies and the constructors (they stand on the address
// registry and cannot link into a host test), and `Vtable() == vptr`
// (tests/unit/exe_class_abi_test.cpp PART C proves DEFINE_EXE_VTABLE on this
// toolchain). Slot 18 (CreateWork) takes its InternalBasePtr slot as an explicit
// pointer (RCX = this, RDX = out), not as a hidden return: no retslot here.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Component/ResourceProduceComponent.h"
#include "Component/ConsumeProduceComponent.h"
#include "Component/FishingComponent.h"
#include "Component/ForesterComponent.h"
#include "Component/HerbalistComponent.h"
#include "Component/HuntComponent.h"
#include "Component/MarketComponent.h"
#include "Component/CropFieldComponent.h"
#include "Component/OrchardComponent.h"
#include "Component/PastureComponent.h"
#include "Component/StorageLocationComponent.h"
#include "Component/TradeComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Component/MarketLocationComponent.h"
#include "Component/MultiObjectComponent.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {


// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(mib_rp_sizes_match_retail) {
    CHECK_EQ(sizeof(ResourceProduceComponent), (size_t)0x50);   // ctor 0x14012b3d0 (abstract)
    CHECK_EQ(sizeof(ConsumeProduceComponent),  (size_t)0x70);   // create 0x140032300
    CHECK_EQ(sizeof(FishingComponent),         (size_t)0x58);   // create 0x140034350
    CHECK_EQ(sizeof(ForesterComponent),        (size_t)0x58);   // create 0x140034870
    CHECK_EQ(sizeof(HerbalistComponent),       (size_t)0x50);   // create 0x140006b40
    CHECK_EQ(sizeof(HuntComponent),            (size_t)0x58);   // create 0x140034f30
    CHECK_EQ(sizeof(MarketComponent),          (size_t)0x50);   // create 0x140008220
}

UNIT_TEST(mib_rp_member_and_fields) {
    CHECK_EQ(offsetof(ResourceProduceComponent, _controller), (size_t)0x28);
    CHECK_EQ(offsetof(ResourceProduceComponent, _controller) +
             offsetof(ControllerComponent::Controller, _owner), (size_t)0x40);
    CHECK_EQ(sizeof(ResourceProduceComponent::StateController), (size_t)0x20);
    CHECK_EQ(offsetof(ResourceProduceComponent, produceFlag), (size_t)0x48);
    CHECK_EQ(offsetof(ConsumeProduceComponent, consumeResourceId), (size_t)0x50);
    CHECK_EQ(offsetof(ConsumeProduceComponent, consumeRates),      (size_t)0x58);
    CHECK_EQ(offsetof(ConsumeProduceComponent, consumeCount),      (size_t)0x60);
    CHECK_EQ(offsetof(ConsumeProduceComponent, consumeState),      (size_t)0x68);
    CHECK_EQ(offsetof(FishingComponent, fishingFlag),   (size_t)0x50);
    CHECK_EQ(offsetof(FishingComponent, fishingRate),   (size_t)0x54);
    CHECK_EQ(offsetof(ForesterComponent, plantTrees), (size_t)0x50);
    CHECK_EQ(offsetof(HuntComponent, huntProgress),     (size_t)0x50);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
UNIT_TEST(mib_rp_base_shape) {
    CHECK(std::is_polymorphic<ResourceProduceComponent>::value);
    CHECK(std::is_abstract<ResourceProduceComponent>::value);           // slot 18 _purecall
    CHECK(std::has_virtual_destructor<ResourceProduceComponent>::value);
    CHECK((std::is_base_of<ControllerComponent, ResourceProduceComponent>::value));
    CHECK_EQ(alignof(ResourceProduceComponent), (size_t)8);
    CHECK_DTOR_SLOT(ResourceProduceComponent);
    CHECK_SLOT(ResourceProduceComponent, Persist,          3);
    CHECK_SLOT(ResourceProduceComponent, CommandMask,      13);   // 0x14012b520
    CHECK_SLOT(ResourceProduceComponent, Command,          14);   // 0x14012b5a0
    CHECK_SLOT(ResourceProduceComponent, GetDefaultState,  16);   // 0x1400026c0
    CHECK_SLOT(ResourceProduceComponent, GetController,    17);   // 0x140004080
    CHECK_SLOT(ResourceProduceComponent, CreateWork,       18);   // _purecall
    CHECK_SLOT(ResourceProduceComponent, OnEnterProducing, 19);   // 0x140006b10
    CHECK_SLOT(ResourceProduceComponent, OnExitProducing,  20);   // 0x140006b10
    CHECK_SLOT(ResourceProduceComponent, ProduceTick,      21);   // 0x140006b10
}

template <typename T>
void CheckRpLeafShape() {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<ResourceProduceComponent, T>::value));
    CHECK((std::is_base_of<ControllerComponent, T>::value));
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, Persist,    3);
    CHECK_SLOT(T, CreateWork, 18);
    // the controller is the base's member at +0x28 for all six (B38_SLOTS §2 "-")
    CHECK_EQ(offsetof(T, _controller), (size_t)0x28);
}

UNIT_TEST(mib_rp_leaf_shape_rules) {
    CheckRpLeafShape<ConsumeProduceComponent>();
    CheckRpLeafShape<FishingComponent>();
    CheckRpLeafShape<ForesterComponent>();
    CheckRpLeafShape<HerbalistComponent>();
    CheckRpLeafShape<HuntComponent>();
    CheckRpLeafShape<MarketComponent>();
}

// ===========================================================================
// PART C — SLOT ORDER (the leaves' own slots, B38_SLOTS.md §6)
// ===========================================================================
UNIT_TEST(mib_rp_own_slots_land_on_retail_indices) {
    CHECK_SLOT(ConsumeProduceComponent, Startup,   4);    // 0x140118ef0
    CHECK_SLOT(FishingComponent, OnEnterProducing, 19);   // 0x14011ee80
    CHECK_SLOT(FishingComponent, OnExitProducing,  20);   // 0x14011ee50
    CHECK_SLOT(FishingComponent, ProduceTick,      21);   // 0x14011ec10
    CHECK_SLOT(ForesterComponent, CommandMask,     13);   // 0x14011f920
    CHECK_SLOT(ForesterComponent, Command,         14);   // 0x14011f890
    CHECK_SLOT(HuntComponent, Update,              9);    // 0x140122740
}

// ===========================================================================
// The ControllerComponent leaves (18 slots, member at retail's per-class offset)
// ===========================================================================
UNIT_TEST(mib_controller_sizes_match_retail) {
    CHECK_EQ(sizeof(CropFieldComponent),       (size_t)0x68);   // create 0x140032ed0
    CHECK_EQ(sizeof(OrchardComponent),         (size_t)0x68);   // create 0x140009870
    CHECK_EQ(sizeof(PastureComponent),         (size_t)0x68);   // create 0x140036730
    CHECK_EQ(sizeof(StorageLocationComponent), (size_t)0x48);   // create 0x140038df0
    CHECK_EQ(sizeof(TradeComponent),           (size_t)0xa8);   // create 0x14000db60
    CHECK_EQ(sizeof(WorkPlaceComponent),       (size_t)0x68);   // create 0x14003a030
}

template <typename T>
void CheckCtlLeaf(size_t memberOff) {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::is_final<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<ControllerComponent, T>::value));
    CHECK(!(std::is_base_of<ResourceProduceComponent, T>::value));
    CHECK((std::is_base_of<Object::Internal, T>::value));
    CHECK(!std::has_virtual_destructor<typename T::StateController>::value);
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK_EQ(offsetof(T, _controller), memberOff);
    CHECK_EQ(offsetof(T, _controller) + offsetof(ControllerComponent::Controller, _owner),
             memberOff + 0x18);
    CHECK_EQ(sizeof(typename T::StateController), (size_t)0x20);
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, Persist,         3);
    CHECK_SLOT(T, GetDefaultState, 16);
    CHECK_SLOT(T, GetController,   17);
}

UNIT_TEST(mib_controller_leaf_shape_and_member_offset) {
    CheckCtlLeaf<CropFieldComponent>(0x28);        // slot 17 0x140004080
    CheckCtlLeaf<OrchardComponent>(0x28);          // slot 17 0x140004080
    CheckCtlLeaf<PastureComponent>(0x48);          // slot 17 0x140036770
    CheckCtlLeaf<StorageLocationComponent>(0x28);  // slot 17 0x140004080
    CheckCtlLeaf<TradeComponent>(0x88);            // slot 17 0x14000dba0
    CheckCtlLeaf<WorkPlaceComponent>(0x28);        // slot 17 0x140004080
    CheckCtlLeaf<MarketLocationComponent>(0x28);   // slot 17 0x140004080 (D9 #8: the tree had none)
    CHECK_EQ(sizeof(MarketLocationComponent), (size_t)0x48);   // create 0x1400086c0
    CHECK_SLOT(MarketLocationComponent, OnRemove, 7);   // 0x14004ef60
    CheckCtlLeaf<MultiObjectComponent>(0x28);      // slot 17 0x140004080 (D9 #8: the tree had none)
    CHECK_EQ(sizeof(MultiObjectComponent), (size_t)0x60);      // create 0x140035ef0
    CHECK_EQ(offsetof(MultiObjectComponent, children),    (size_t)0x48);
    CHECK_EQ(offsetof(MultiObjectComponent, multiCount), (size_t)0x58);
    CHECK_SLOT(MultiObjectComponent, OnRemove, 7);      // 0x1401257c0
    CHECK_SLOT(MultiObjectComponent, OnEvent,  12);     // 0x140125490
}

UNIT_TEST(mib_controller_member_fields) {
    CHECK_EQ(offsetof(CropFieldComponent, workEnabled), (size_t)0x48);
    CHECK_EQ(offsetof(CropFieldComponent, slots),      (size_t)0x58);
    CHECK_EQ(offsetof(OrchardComponent, lastHarvestYear),(size_t)0x4c);
    CHECK_EQ(offsetof(OrchardComponent, slots),        (size_t)0x50);
    CHECK_EQ(offsetof(PastureComponent, livestockType),(size_t)0x28);
    CHECK_EQ(offsetof(PastureComponent, production),   (size_t)0x44);
    CHECK_EQ(offsetof(TradeComponent, tradeId),        (size_t)0x2c);
    CHECK_EQ(offsetof(TradeComponent, tradeFlag),      (size_t)0x71);
    CHECK_EQ(offsetof(TradeComponent, field80),        (size_t)0x80);
    CHECK_EQ(offsetof(WorkPlaceComponent, profession), (size_t)0x48);
    CHECK_EQ(offsetof(WorkPlaceComponent, isAccepting),(size_t)0x50);
    CHECK_EQ(offsetof(WorkPlaceComponent, workerCount),(size_t)0x60);
}

UNIT_TEST(mib_controller_own_slots_land_on_retail_indices) {
    CHECK_SLOT(CropFieldComponent, Startup,        4);    // 0x14011ae60
    CHECK_SLOT(CropFieldComponent, CommandMask,    13);   // 0x14011ada0
    CHECK_SLOT(CropFieldComponent, Command,        14);   // 0x14011acc0
    CHECK_SLOT(CropFieldComponent, OnMessage,      15);   // 0x14011ab50
    CHECK_SLOT(OrchardComponent, Startup,          4);    // 0x140051dd0
    CHECK_SLOT(OrchardComponent, CommandMask,      13);   // 0x140051c90
    CHECK_SLOT(OrchardComponent, Command,          14);   // 0x140051ba0
    CHECK_SLOT(OrchardComponent, OnMessage,        15);   // 0x140051990
    CHECK_SLOT(PastureComponent, Startup,          4);    // 0x140127a50
    CHECK_SLOT(PastureComponent, OnAdded,          6);    // 0x140127a30
    CHECK_SLOT(PastureComponent, OnRemove,         7);    // 0x1401277e0
    CHECK_SLOT(PastureComponent, OnEvent,          12);   // 0x140127540
    CHECK_SLOT(PastureComponent, CommandMask,      13);   // 0x140127dd0
    CHECK_SLOT(PastureComponent, Command,          14);   // 0x140127a60
    CHECK_SLOT(PastureComponent, OnMessage,        15);   // 0x140127720
    CHECK_SLOT(StorageLocationComponent, OnRemove, 7); // 0x14012d270
    CHECK_SLOT(TradeComponent, OnAdded,            6);    // 0x14005c1d0
    CHECK_SLOT(TradeComponent, OnRemove,           7);    // 0x14005c190
    CHECK_SLOT(TradeComponent, CommandMask,        13);   // 0x14005c140
    CHECK_SLOT(TradeComponent, Command,            14);   // 0x14005c000
    CHECK_SLOT(WorkPlaceComponent, OnRemove,       7);    // 0x14012f090
}

// ===========================================================================
// PART D — raw dispatch through the shared RP member
// ===========================================================================
void* g_mibSeenData = nullptr;
void* g_mibSeenSm   = nullptr;
char MibEnterHandler(void* data, void* sm) { g_mibSeenData = data; g_mibSeenSm = sm; return 7; }

UNIT_TEST(mib_rp_controller_member_raw_dispatch) {
    using Ctl = ResourceProduceComponent::StateController;
    using Dispatch = ::ControllerLive::Dispatcher<RPCtrlTag>;
    auto& t = Dispatch::table;
    const ::ControllerLive::StateTable saved = t;
    t = {};
    ::ControllerLive::TableRegisterState(t, 13, reinterpret_cast<void*>(&MibEnterHandler),
                                         nullptr, nullptr, nullptr);
    ::ControllerLive::TableAddTransition(t, 14, nullptr);

    alignas(8) unsigned char owner[0x58] = {};
    Ctl node(reinterpret_cast<Component*>(owner));
    void** vt = *reinterpret_cast<void***>(&node);
    CHECK_EQ(reinterpret_cast<int (*)(void*, int)>(vt[0])(&node, 13), 0);          // FindState
    int desired = 14;
    CHECK_EQ(reinterpret_cast<int (*)(void*, int, int*)>(vt[1])(&node, 0, &desired), 0);
    int smToken = 0;
    CHECK_EQ((int)reinterpret_cast<char (*)(void*, void*, int)>(vt[3])(&node, &smToken, 0), 7);
    CHECK(g_mibSeenData == static_cast<void*>(owner));                             // RCX = [node+0x18]
    CHECK(g_mibSeenSm == static_cast<void*>(&smToken));
    CHECK_EQ((int)reinterpret_cast<char (*)(void*)>(vt[8])(&node), 1);            // HasStates
    t = saved;
}

}  // namespace
