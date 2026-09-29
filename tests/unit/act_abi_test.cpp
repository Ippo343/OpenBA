// ============================================================================
// tests/unit/act_abi_test.cpp — ENDGAME B.3.8 block ACT (B38_RULES.md B8-14)
//
// The ABI claims of the Action family, measured on the SHIPPED HEADERS
// (code/Game/Action/*.h):
//   PART A — layout: sizeof / alignof, every field offset the bodies read raw.
//   PART B — slot order, read off pointers to VIRTUAL members (Itanium PMF =
//            1 + vtable byte offset; adjustment 0 = single inheritance):
//            retail table 0x14015faa0, 10 slots, and each leaf's overrides at
//            retail's index (B38_SLOTS.md §6).
//   PART C — class-shape rules: polymorphic, a virtual destructor (B3X W1: retail slot 0 = D1 + D0), Action
//            abstract (slot 4 `_purecall`), every leaf concrete.
//
// NOT PROVEN HERE: `X::Vtable() == *(void**)&obj` needs a constructed object,
// i.e. the member bodies, which stand on the address registry and do not link
// into the host test (the pilot's exe_class_abi_test.cpp PART C proves the
// DEFINE_EXE_VTABLE mechanism itself on this toolchain).
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Action/Action.h"
#include "Action/AnimateNaturalResource.h"
#include "Action/CompleteRoad.h"
#include "Action/CompleteTask.h"
#include "Action/ConsumeRawMaterial.h"
#include "Action/CreateNaturalResource.h"
#include "Action/CreateRawMaterial.h"
#include "Action/DropOffRawMaterial.h"
#include "Action/HuntAnimal.h"
#include "Action/MoveTo.h"
#include "Action/PerformWork.h"
#include "Action/PickupRawMaterial.h"
#include "Action/PickupStoredRawMaterial.h"
#include "Action/PlayAnimation.h"
#include "Action/RemoveAnimal.h"
#include "Action/RemoveNaturalResource.h"
#include "Action/SendComponentMessage.h"
#include "Action/ShowTool.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace {

// ===========================================================================
// Action (the base, retail 0x14015faa0)
// ===========================================================================
UNIT_TEST(act_action_layout) {
    CHECK_EQ(sizeof(Action), (size_t)0x20);
    CHECK_EQ(alignof(Action), (size_t)8);
    CHECK_EQ(offsetof(Action, _next), (size_t)0x10);
    CHECK_EQ(offsetof(Action, _status), (size_t)0x18);
    CHECK_EQ(sizeof(Action::_next), (size_t)8);
}

UNIT_TEST(act_action_shape) {
    CHECK(std::is_polymorphic<Action>::value);
    CHECK(std::is_abstract<Action>::value);                 // [4] _purecall 0x14003addc
    CHECK(std::has_virtual_destructor<Action>::value);
    CHECK((std::is_base_of<Object::Internal, Action>::value));
    CHECK(std::is_trivially_destructible<Object::InternalBasePtr>::value);
}

UNIT_TEST(act_action_slot_order) {
    CHECK_DTOR_SLOT(Action);   // 0x140021c90
    CHECK_SLOT(Action, TypeName,     0x08/8);   // 0x140021c80
    CHECK_SLOT(Action, Type,         0x10/8);   // 0x140021c70
    CHECK_SLOT(Action, Persist,      0x18/8);   // 0x1400c4b40
    CHECK_SLOT(Action, AbstractImpl, 0x20/8);   // _purecall
    CHECK_SLOT(Action, Execute,      0x28/8);   // 0x1400c4a90
    CHECK_SLOT(Action, Enter,        0x30/8);   // 0x1400c4b90
    CHECK_SLOT(Action, Exit,         0x38/8);   // 0x1400c4ba0
    CHECK_SLOT(Action, CheckEntity,  0x40/8);   // 0x1400c4a30
    CHECK_SLOT(Action, GetTarget,    0x48/8);   // 0x1400c4a40 (last)
}

// ===========================================================================
// The leaves: every one is concrete, final-shape, 8-aligned, retail-sized, and
// its overrides sit on retail's slot index (B38_SLOTS.md §6).
// ===========================================================================
template <typename T>
void CheckLeafShape(size_t size) {
    CHECK_EQ(sizeof(T), size);
    CHECK_EQ(alignof(T), (size_t)8);
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<Action, T>::value));
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,     0x08/8);
    CHECK_SLOT(T, Type,         0x10/8);
    CHECK_SLOT(T, Persist,      0x18/8);
    CHECK_SLOT(T, AbstractImpl, 0x20/8);
}

UNIT_TEST(act_leaf_animatenaturalresource) {
    CheckLeafShape<AnimateNaturalResource>(0x30);   // create 0x140021dc0, table 0x14015fb10
    CHECK_SLOT(AnimateNaturalResource, Execute,     0x28/8);   // [5] 0x1400c55b0
    CHECK_SLOT(AnimateNaturalResource, CheckEntity, 0x40/8);   // [8] 0x1400c55a0
    CHECK_SLOT(AnimateNaturalResource, GetTarget,   0x48/8);   // [9] 0x1400c5560
    CHECK_EQ(offsetof(AnimateNaturalResource, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(AnimateNaturalResource, animIndex), (size_t)0x28);
    CHECK_EQ(offsetof(AnimateNaturalResource, waitForAnim), (size_t)0x2c);
}

UNIT_TEST(act_leaf_completeroad) {
    CheckLeafShape<CompleteRoad>(0x30);   // create 0x140021ec0, table 0x14015fb70
    CHECK_EQ(offsetof(CompleteRoad, roadTileIndex), (size_t)0x20);
    CHECK_EQ(offsetof(CompleteRoad, roadType), (size_t)0x24);
    CHECK_EQ(offsetof(CompleteRoad, roadDirection), (size_t)0x28);
}

UNIT_TEST(act_leaf_completetask) {
    CheckLeafShape<CompleteTask>(0x38);   // create 0x140021fc0, table 0x14015fbd0
    CHECK_SLOT(CompleteTask, CheckEntity, 0x40/8);   // [8] 0x1400c5ac0
    CHECK_SLOT(CompleteTask, GetTarget,   0x48/8);   // [9] 0x1400c5a50
    CHECK_EQ(offsetof(CompleteTask, workEntity), (size_t)0x20);
    CHECK_EQ(offsetof(CompleteTask, targetEntity), (size_t)0x28);
    CHECK_EQ(offsetof(CompleteTask, taskIndex), (size_t)0x30);
    CHECK_EQ(offsetof(CompleteTask, taskParam), (size_t)0x34);
}

UNIT_TEST(act_leaf_consumerawmaterial) {
    CheckLeafShape<ConsumeRawMaterial>(0x40);   // create 0x1400220c0, table 0x14015fc20
    CHECK_SLOT(ConsumeRawMaterial, CheckEntity, 0x40/8);   // [8] 0x1400c5e30
    CHECK_SLOT(ConsumeRawMaterial, GetTarget,   0x48/8);   // [9] 0x1400c5f10
    CHECK_EQ(offsetof(ConsumeRawMaterial, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(ConsumeRawMaterial, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(ConsumeRawMaterial, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(ConsumeRawMaterial, _field38), (size_t)0x38);
}

UNIT_TEST(act_leaf_createnaturalresource) {
    CheckLeafShape<CreateNaturalResource>(0x38);   // create 0x1400221d0, table 0x14015fca0
    CHECK_SLOT(CreateNaturalResource, GetTarget, 0x48/8);   // [9] 0x1400c6610
    CHECK_EQ(offsetof(CreateNaturalResource, descEntity), (size_t)0x20);
    CHECK_EQ(offsetof(CreateNaturalResource, parentEntity), (size_t)0x28);
    CHECK_EQ(offsetof(CreateNaturalResource, cellIndex), (size_t)0x30);
}

UNIT_TEST(act_leaf_createrawmaterial) {
    CheckLeafShape<CreateRawMaterial>(0x58);   // create 0x1400222d0, table 0x14015fd10
    CHECK_SLOT(CreateRawMaterial, GetTarget, 0x48/8);   // [9] 0x1400c6aa0
    CHECK_EQ(offsetof(CreateRawMaterial, descEntity), (size_t)0x20);
    CHECK_EQ(offsetof(CreateRawMaterial, sourceBuilding), (size_t)0x28);
    CHECK_EQ(offsetof(CreateRawMaterial, secondaryEntity), (size_t)0x30);
    CHECK_EQ(offsetof(CreateRawMaterial, tertiaryEntity), (size_t)0x38);
    CHECK_EQ(offsetof(CreateRawMaterial, totalQuantity), (size_t)0x40);
    CHECK_EQ(offsetof(CreateRawMaterial, materialCellIdx), (size_t)0x44);
    CHECK_EQ(offsetof(CreateRawMaterial, placementFlags), (size_t)0x48);
    CHECK_EQ(offsetof(CreateRawMaterial, yieldMultiplier), (size_t)0x4c);
    CHECK_EQ(offsetof(CreateRawMaterial, naturalResFlag), (size_t)0x50);
    CHECK_EQ(offsetof(CreateRawMaterial, placeNearFlag), (size_t)0x51);
}

UNIT_TEST(act_leaf_dropoffrawmaterial) {
    CheckLeafShape<DropOffRawMaterial>(0x40);   // create 0x140022600, table 0x14015fd80
    CHECK_SLOT(DropOffRawMaterial, CheckEntity, 0x40/8);   // [8] 0x1400c72f0
    CHECK_SLOT(DropOffRawMaterial, GetTarget,   0x48/8);   // [9] 0x1400c7270
    CHECK_EQ(offsetof(DropOffRawMaterial, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(DropOffRawMaterial, _field24), (size_t)0x24);
    CHECK_EQ(offsetof(DropOffRawMaterial, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(DropOffRawMaterial, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(DropOffRawMaterial, _field38), (size_t)0x38);
}

UNIT_TEST(act_leaf_huntanimal) {
    CheckLeafShape<HuntAnimal>(0x40);   // create 0x140022700, table 0x14015fde0
    CHECK_SLOT(HuntAnimal, Execute,   0x28/8);   // [5] 0x1400c8240
    CHECK_SLOT(HuntAnimal, Enter,     0x30/8);   // [6] 0x1400c7de0
    CHECK_SLOT(HuntAnimal, GetTarget, 0x48/8);   // [9] 0x1400c7d60
    CHECK_EQ(offsetof(HuntAnimal, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(HuntAnimal, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(HuntAnimal, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(HuntAnimal, _field38), (size_t)0x38);
}

UNIT_TEST(act_leaf_moveto) {
    CheckLeafShape<MoveTo>(0x480);   // create 0x1400228c0, table 0x14015fe40
    CHECK_SLOT(MoveTo, Execute,     0x28/8);   // [5] 0x1400c8570
    CHECK_SLOT(MoveTo, Enter,       0x30/8);   // [6] 0x1400c8970
    CHECK_SLOT(MoveTo, Exit,        0x38/8);   // [7] 0x1400c8520
    CHECK_SLOT(MoveTo, CheckEntity, 0x40/8);   // [8] 0x1400c84d0
    CHECK_SLOT(MoveTo, GetTarget,   0x48/8);   // [9] 0x1400c8480
    CHECK_EQ(offsetof(MoveTo, targetId), (size_t)0x20);
    CHECK_EQ(offsetof(MoveTo, _field24), (size_t)0x24);
    CHECK_EQ(offsetof(MoveTo, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(MoveTo, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(MoveTo, _field38), (size_t)0x38);
    CHECK_EQ(offsetof(MoveTo, _field40), (size_t)0x40);
    CHECK_EQ(offsetof(MoveTo, _field48), (size_t)0x48);
    CHECK_EQ(offsetof(MoveTo, _field50), (size_t)0x50);
    CHECK_EQ(offsetof(MoveTo, _field58), (size_t)0x58);
    CHECK_EQ(offsetof(MoveTo, _field60), (size_t)0x60);
    CHECK_EQ(offsetof(MoveTo, _field64), (size_t)0x64);
    CHECK_EQ(offsetof(MoveTo, _pathData), (size_t)0x68);
    CHECK_EQ(offsetof(MoveTo, _field474), (size_t)0x474);
    CHECK_EQ(offsetof(MoveTo, speedMultiplier), (size_t)0x478);
}

UNIT_TEST(act_leaf_performwork) {
    CheckLeafShape<PerformWork>(0x48);   // create 0x140022b90, table 0x14015fea0
    CHECK_SLOT(PerformWork, Execute,     0x28/8);   // [5] 0x1400c93c0
    CHECK_SLOT(PerformWork, Exit,        0x38/8);   // [7] 0x1400c9370
    CHECK_SLOT(PerformWork, CheckEntity, 0x40/8);   // [8] 0x1400c9320
    CHECK_SLOT(PerformWork, GetTarget,   0x48/8);   // [9] 0x1400c92d0
    CHECK_EQ(offsetof(PerformWork, workTarget), (size_t)0x20);
    CHECK_EQ(offsetof(PerformWork, facingYaw), (size_t)0x28);
    CHECK_EQ(offsetof(PerformWork, animationId), (size_t)0x2c);
    CHECK_EQ(offsetof(PerformWork, requiredTime), (size_t)0x30);
    CHECK_EQ(offsetof(PerformWork, elapsedTime), (size_t)0x38);
    CHECK_EQ(offsetof(PerformWork, useTool), (size_t)0x40);
    CHECK_EQ(offsetof(PerformWork, field44), (size_t)0x44);
}

UNIT_TEST(act_leaf_pickuprawmaterial) {
    CheckLeafShape<PickupRawMaterial>(0x30);   // create 0x140022c60, table 0x14015ff10
    CHECK_SLOT(PickupRawMaterial, GetTarget, 0x48/8);   // [9] 0x1400c9860
    CHECK_EQ(offsetof(PickupRawMaterial, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(PickupRawMaterial, _field28), (size_t)0x28);
}

UNIT_TEST(act_leaf_pickupstoredrawmaterial) {
    CheckLeafShape<PickupStoredRawMaterial>(0x48);   // create 0x140022d60, table 0x14015ff80
    CHECK_SLOT(PickupStoredRawMaterial, CheckEntity, 0x40/8);   // [8] 0x1400c9c70
    CHECK_SLOT(PickupStoredRawMaterial, GetTarget,   0x48/8);   // [9] 0x1400c9bf0
    CHECK_EQ(offsetof(PickupStoredRawMaterial, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(PickupStoredRawMaterial, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(PickupStoredRawMaterial, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(PickupStoredRawMaterial, _field38), (size_t)0x38);
    CHECK_EQ(offsetof(PickupStoredRawMaterial, _field40), (size_t)0x40);
}

UNIT_TEST(act_leaf_playanimation) {
    CheckLeafShape<PlayAnimation>(0x40);   // create 0x1400011f0, table 0x140158950
    CHECK_SLOT(PlayAnimation, Execute,     0x28/8);   // [5] 0x140042400
    CHECK_SLOT(PlayAnimation, CheckEntity, 0x40/8);   // [8] 0x1400423f0
    CHECK_SLOT(PlayAnimation, GetTarget,   0x48/8);   // [9] 0x1400423b0
    CHECK_EQ(offsetof(PlayAnimation, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(PlayAnimation, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(PlayAnimation, _field38), (size_t)0x38);
}

UNIT_TEST(act_leaf_removeanimal) {
    CheckLeafShape<RemoveAnimal>(0x28);   // create 0x140022e70, table 0x14015ffe0
    CHECK_SLOT(RemoveAnimal, CheckEntity, 0x40/8);   // [8] 0x1400ca760
    CHECK_SLOT(RemoveAnimal, GetTarget,   0x48/8);   // [9] 0x1400ca720
    CHECK_EQ(offsetof(RemoveAnimal, _field20), (size_t)0x20);
}

UNIT_TEST(act_leaf_removenaturalresource) {
    CheckLeafShape<RemoveNaturalResource>(0x38);   // create 0x140022f70, table 0x140160050
    CHECK_SLOT(RemoveNaturalResource, CheckEntity, 0x40/8);   // [8] 0x1400caaa0
    CHECK_SLOT(RemoveNaturalResource, GetTarget,   0x48/8);   // [9] 0x1400caa20
    CHECK_EQ(offsetof(RemoveNaturalResource, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(RemoveNaturalResource, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(RemoveNaturalResource, _field30), (size_t)0x30);
}

UNIT_TEST(act_leaf_sendcomponentmessage) {
    CheckLeafShape<SendComponentMessage>(0x40);   // create 0x140023070, table 0x1401600c0
    CHECK_SLOT(SendComponentMessage, GetTarget, 0x48/8);   // [9] 0x1400cae80
    CHECK_EQ(offsetof(SendComponentMessage, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(SendComponentMessage, _field28), (size_t)0x28);
    CHECK_EQ(offsetof(SendComponentMessage, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(SendComponentMessage, _field34), (size_t)0x34);
    CHECK_EQ(offsetof(SendComponentMessage, _field38), (size_t)0x38);
}

UNIT_TEST(act_leaf_showtool) {
    CheckLeafShape<ShowTool>(0x30);   // create 0x1400013b0, table 0x1401589b0
    CHECK_SLOT(ShowTool, Exit,        0x38/8);   // [7] 0x140042700
    CHECK_SLOT(ShowTool, CheckEntity, 0x40/8);   // [8] 0x1400426d0
    CHECK_SLOT(ShowTool, GetTarget,   0x48/8);   // [9] 0x140042680
    CHECK_EQ(offsetof(ShowTool, _field20), (size_t)0x20);
    CHECK_EQ(offsetof(ShowTool, _field28), (size_t)0x28);
}

// ENDGAME B.3.9 EXA: the citizen-AI builders construct PerformWork (the
// parameterised ctor 0x1400c9720) and PlayAnimation (default ctor + retail's
// inlined leaf stores) with `::new (MemAlloc(size, 8))` instead of the
// `??0Internal` door plus two stamps. The expressions are checked unevaluated.
template <typename T, typename... A>
using ExaNew = decltype(::new (std::declval<void*>()) T(std::declval<A>()...));
template <typename, typename T, typename... A> struct ExaCanNew : std::false_type {};
template <typename T, typename... A>
struct ExaCanNew<std::void_t<ExaNew<T, A...>>, T, A...> : std::true_type {};

UNIT_TEST(act_citizenai_builders_construct_in_place) {
    CHECK((ExaCanNew<void, PerformWork, void*, float, uint32_t, int64_t, uint8_t, int>::value));
    CHECK((ExaCanNew<void, PlayAnimation>::value));
}

}  // namespace
