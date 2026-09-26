// ============================================================================
// tests/unit/goal_abi_test.cpp — ENDGAME B.3.8 block GOAL (B38_RULES D1-D5, D8, B8-14)
//
// The ABI claims block GOAL makes for its exe classes, measured on the SHIPPED
// class headers under code/Game/Goal/, code/Game/Tutorial/ and code/Game/Game/:
//
// PART A — LAYOUT. sizeof (= retail's create MemAlloc, B38_RULES section 1.3),
//   alignof, and the field offsets.
// PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0: single inheritance), against the
//   retail tables of B38_SLOTS.md section 6.
// PART C — SHAPE. polymorphic, abstract exactly where retail is abstract, no
//   virtual destructor (ENDGAME decision 2), the proved parent.
//
// NOT PROVEN HERE: the member bodies (they stand on the address registry and
// the Runtime and cannot link into a host test) and `Vtable()` against a
// constructed object (the pilot's exe_class_abi_test.cpp PART C proves the
// DEFINE_EXE_VTABLE mechanism on this toolchain; rb_achievement_abi_test.cpp
// pins the Goal::GetInstanceType retslot shape the two overrides take). The
// real run and the resave gate verify those.
// ============================================================================
#include "unit.h"

#include "Goal/BuildingGoal.h"
#include "Goal/PopulationGoal.h"
#include "Goal/StatGoal.h"
#include "Goal/TimeGoal.h"
#include "Goal/TimeGoalInstance.h"
#include "Tutorial/FinishTutorialGoal.h"
#include "Tutorial/FinishTutorialInstance.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting
    return words[0] - 1;
}

template <typename T, typename Base>
void CheckShape(bool abstract) {
    CHECK(std::is_polymorphic<T>::value);
    CHECK_EQ(std::is_abstract<T>::value, abstract);
    CHECK(!std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<Base, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
}

// Achievement::Goal leaves: 11 slots (B38_SLOTS section 6, B38_RULES D8).
template <typename T>
void CheckGoalSlots() {
    CHECK_EQ(SlotOffset(&T::DeletingDtor),    (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&T::TypeName),        (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&T::Type),            (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&T::GetFileVersion),  (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&T::Compile),         (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&T::Serialize),       (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&T::Reconstruct),     (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&T::Destruct),        (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&T::CheckGoalState),  (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&T::GetInstanceType), (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&T::Construct),       (uintptr_t)0x50);
}

// Achievement::GoalInstance leaves: 7 slots.
template <typename T>
void CheckGoalInstanceSlots() {
    CHECK_EQ(SlotOffset(&T::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&T::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&T::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&T::Persist),        (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&T::Startup),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&T::Update),         (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&T::CheckGoalState), (uintptr_t)0x30);
}

}  // namespace

// ===========================================================================
// PART A — layout
// ===========================================================================
UNIT_TEST(goal_leaf_sizes_are_retail_memalloc) {
    CHECK_EQ(sizeof(BuildingGoal),           (size_t)0x48);
    CHECK_EQ(sizeof(PopulationGoal),         (size_t)0x38);
    CHECK_EQ(sizeof(StatGoal),               (size_t)0x80);
    CHECK_EQ(sizeof(TimeGoal),               (size_t)0x38);
    CHECK_EQ(sizeof(FinishTutorialGoal),     (size_t)0x38);
    CHECK_EQ(sizeof(TimeGoalInstance),       (size_t)0x28);
    CHECK_EQ(sizeof(FinishTutorialInstance), (size_t)0x28);
    CHECK_EQ(offsetof(BuildingGoal, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(BuildingGoal, _field38), (size_t)0x38);
    CHECK_EQ(offsetof(PopulationGoal, targetPop), (size_t)0x30);
    CHECK_EQ(offsetof(StatGoal, statTypeA), (size_t)0x30);
    CHECK_EQ(offsetof(StatGoal, statSubType), (size_t)0x7c);
    CHECK_EQ(offsetof(TimeGoal, requiredYears), (size_t)0x30);
    CHECK_EQ(offsetof(TimeGoal, requireOtherGoalsMet), (size_t)0x34);
    CHECK_EQ(offsetof(FinishTutorialGoal, _field30), (size_t)0x30);
    CHECK_EQ(offsetof(TimeGoalInstance, elapsedTime), (size_t)0x20);
    CHECK_EQ(offsetof(FinishTutorialInstance, _field20), (size_t)0x20);
}

// ===========================================================================
// PART B — slot order
// ===========================================================================
UNIT_TEST(goal_leaves_have_the_retail_11_slot_order) {
    CheckGoalSlots<BuildingGoal>();
    CheckGoalSlots<PopulationGoal>();
    CheckGoalSlots<StatGoal>();
    CheckGoalSlots<TimeGoal>();
    CheckGoalSlots<FinishTutorialGoal>();
}

UNIT_TEST(goalinstance_leaves_have_the_retail_7_slot_order) {
    CheckGoalInstanceSlots<TimeGoalInstance>();
    CheckGoalInstanceSlots<FinishTutorialInstance>();
}

// ===========================================================================
// PART C — shape
// ===========================================================================
UNIT_TEST(goal_classes_shape) {
    CheckShape<BuildingGoal, Achievement::Goal>(false);
    CheckShape<PopulationGoal, Achievement::Goal>(false);
    CheckShape<StatGoal, Achievement::Goal>(false);
    CheckShape<TimeGoal, Achievement::Goal>(false);
    CheckShape<FinishTutorialGoal, Achievement::Goal>(false);
    CheckShape<TimeGoalInstance, Achievement::GoalInstance>(false);
    CheckShape<FinishTutorialInstance, Achievement::GoalInstance>(false);
}

// ===========================================================================
// Scenario : Achievement::Achievement — 9 slots, 0x90 (B38_RULES D8)
// ===========================================================================
#include "Game/Scenario.h"

UNIT_TEST(scenario_is_retail_9_slots_and_0x90) {
    CHECK_EQ(sizeof(Scenario), (size_t)0x90);
    CHECK_EQ(offsetof(Scenario, scenarioName), (size_t)0x58);
    CHECK_EQ(offsetof(Scenario, _field68), (size_t)0x68);
    CHECK_EQ(offsetof(Scenario, _field88), (size_t)0x88);
    CHECK_EQ(SlotOffset(&Scenario::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&Scenario::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&Scenario::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&Scenario::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&Scenario::Compile),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&Scenario::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&Scenario::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&Scenario::Destruct),       (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&Scenario::Construct),      (uintptr_t)0x40);
    CheckShape<Scenario, Achievement::Achievement>(false);
}

// ===========================================================================
// TutorialGoal (abstract, 9 slots) and its leaves (10 slots)
// ===========================================================================
#include "Tutorial/TutorialGoal.h"
#include "Tutorial/TutorialGoalButton.h"
#include "Tutorial/TutorialGoalCount.h"
#include "Tutorial/TutorialGoalGameSpeed.h"

namespace {
template <typename T>
void CheckExternalSlots() {
    CHECK_EQ(SlotOffset(&T::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&T::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&T::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&T::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&T::Compile),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&T::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&T::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&T::Destruct),       (uintptr_t)0x38);
}
template <typename T>
void CheckTutorialGoalLeaf() {
    CheckExternalSlots<T>();
    CHECK_EQ(SlotOffset(&T::IsComplete), (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&T::Construct),  (uintptr_t)0x48);
    CheckShape<T, TutorialGoal>(false);
}
}  // namespace

UNIT_TEST(tutorialgoal_family_is_retail) {
    CheckExternalSlots<TutorialGoal>();
    CHECK_EQ(SlotOffset(&TutorialGoal::IsComplete), (uintptr_t)0x40);
    CheckShape<TutorialGoal, Object::External>(true);
    CHECK_EQ(sizeof(TutorialGoal), (size_t)0x20);
    CheckTutorialGoalLeaf<TutorialGoalButton>();
    CheckTutorialGoalLeaf<TutorialGoalCount>();
    CheckTutorialGoalLeaf<TutorialGoalGameSpeed>();
    CHECK_EQ(sizeof(TutorialGoalButton),    (size_t)0x30);
    CHECK_EQ(sizeof(TutorialGoalCount),     (size_t)0x50);
    CHECK_EQ(sizeof(TutorialGoalGameSpeed), (size_t)0x28);
    CHECK_EQ(offsetof(TutorialGoalCount, elementName), (size_t)0x48);
}
