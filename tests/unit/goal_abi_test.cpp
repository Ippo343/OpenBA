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
#include "slot_check.h"

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
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,        0x08/8);
    CHECK_SLOT(T, Type,            0x10/8);
    CHECK_SLOT(T, GetFileVersion,  0x18/8);
    CHECK_SLOT(T, Compile,         0x20/8);
    CHECK_SLOT(T, Serialize,       0x28/8);
    CHECK_SLOT(T, Reconstruct,     0x30/8);
    CHECK_SLOT(T, Destruct,        0x38/8);
    CHECK_SLOT(T, CheckGoalState,  0x40/8);
    CHECK_SLOT(T, GetInstanceType, 0x48/8);
    CHECK_SLOT(T, Construct,       0x50/8);
}

// Achievement::GoalInstance leaves: 7 slots.
template <typename T>
void CheckGoalInstanceSlots() {
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,       0x08/8);
    CHECK_SLOT(T, Type,           0x10/8);
    CHECK_SLOT(T, Persist,        0x18/8);
    CHECK_SLOT(T, Startup,        0x20/8);
    CHECK_SLOT(T, Update,         0x28/8);
    CHECK_SLOT(T, CheckGoalState, 0x30/8);
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
    CHECK_DTOR_SLOT(Scenario);
    CHECK_SLOT(Scenario, TypeName,       0x08/8);
    CHECK_SLOT(Scenario, Type,           0x10/8);
    CHECK_SLOT(Scenario, GetFileVersion, 0x18/8);
    CHECK_SLOT(Scenario, Compile,        0x20/8);
    CHECK_SLOT(Scenario, Serialize,      0x28/8);
    CHECK_SLOT(Scenario, Reconstruct,    0x30/8);
    CHECK_SLOT(Scenario, Destruct,       0x38/8);
    CHECK_SLOT(Scenario, Construct,      0x40/8);
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
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,       0x08/8);
    CHECK_SLOT(T, Type,           0x10/8);
    CHECK_SLOT(T, GetFileVersion, 0x18/8);
    CHECK_SLOT(T, Compile,        0x20/8);
    CHECK_SLOT(T, Serialize,      0x28/8);
    CHECK_SLOT(T, Reconstruct,    0x30/8);
    CHECK_SLOT(T, Destruct,       0x38/8);
}
template <typename T>
void CheckTutorialGoalLeaf() {
    CheckExternalSlots<T>();
    CHECK_SLOT(T, IsComplete, 0x40/8);
    CHECK_SLOT(T, Construct,  0x48/8);
    CheckShape<T, TutorialGoal>(false);
}
}  // namespace

UNIT_TEST(tutorialgoal_family_is_retail) {
    CheckExternalSlots<TutorialGoal>();
    CHECK_SLOT(TutorialGoal, IsComplete, 0x40/8);
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

// ===========================================================================
// TutorialSetup (abstract, 11 slots) and its leaves (12 slots)
// ===========================================================================
#include "Tutorial/TutorialSetup.h"
#include "Tutorial/TutorialSetupCamera.h"
#include "Tutorial/TutorialSetupGameSpeed.h"
#include "Tutorial/TutorialSetupRestrictPlayer.h"

namespace {
template <typename T>
void CheckTutorialSetupSlots() {
    CheckExternalSlots<T>();
    CHECK_SLOT(T, Begin,  0x40/8);
    CHECK_SLOT(T, Update, 0x48/8);
    CHECK_SLOT(T, End,    0x50/8);
}
template <typename T>
void CheckTutorialSetupLeaf() {
    CheckTutorialSetupSlots<T>();
    CHECK_SLOT(T, Construct, 0x58/8);
    CheckShape<T, TutorialSetup>(false);
}
}  // namespace

UNIT_TEST(tutorialsetup_family_is_retail) {
    CheckTutorialSetupSlots<TutorialSetup>();
    CheckShape<TutorialSetup, Object::External>(true);
    CHECK_EQ(sizeof(TutorialSetup), (size_t)0x20);
    CheckTutorialSetupLeaf<TutorialSetupCamera>();
    CheckTutorialSetupLeaf<TutorialSetupGameSpeed>();
    CheckTutorialSetupLeaf<TutorialSetupRestrictPlayer>();
    CHECK_EQ(sizeof(TutorialSetupCamera),         (size_t)0x30);
    CHECK_EQ(sizeof(TutorialSetupGameSpeed),      (size_t)0x28);
    CHECK_EQ(sizeof(TutorialSetupRestrictPlayer), (size_t)0x68);
    CHECK_EQ(offsetof(TutorialSetupRestrictPlayer, cameraRefCached), (size_t)0x60);
}

// ===========================================================================
// TutorialImage (concrete, 10 slots) and its leaves (10 slots)
// ===========================================================================
#include "Tutorial/TutorialImage.h"
#include "Tutorial/TutorialImageHotKey.h"
#include "Tutorial/TutorialImageInputKey.h"
#include "Tutorial/TutorialImageToolbarIcon.h"

namespace {
template <typename T>
void CheckTutorialImageSlots() {
    CheckExternalSlots<T>();
    CHECK_SLOT(T, Construct,     0x40/8);
    CHECK_SLOT(T, CreateElement, 0x48/8);
    CheckShape<T, TutorialImage>(false);
}
}  // namespace

UNIT_TEST(tutorialimage_family_is_retail) {
    CheckTutorialImageSlots<TutorialImage>();
    CheckTutorialImageSlots<TutorialImageHotKey>();
    CheckTutorialImageSlots<TutorialImageInputKey>();
    CheckTutorialImageSlots<TutorialImageToolbarIcon>();
    CHECK_EQ(sizeof(TutorialImage),            (size_t)0x30);
    CHECK_EQ(sizeof(TutorialImageHotKey),      (size_t)0x48);
    CHECK_EQ(sizeof(TutorialImageInputKey),    (size_t)0x38);
    CHECK_EQ(sizeof(TutorialImageToolbarIcon), (size_t)0x48);
    CHECK_EQ(offsetof(TutorialImage, imageWidth), (size_t)0x28);
    CHECK_EQ(offsetof(TutorialImageToolbarIcon, _toolbarRef), (size_t)0x40);
}

// ===========================================================================
// TutorialInstance : Object::Internal — 4 slots, 0x38
// ===========================================================================
#include "Tutorial/TutorialInstance.h"

UNIT_TEST(tutorialinstance_is_retail_4_slots_and_0x38) {
    CHECK_EQ(sizeof(TutorialInstance), (size_t)0x38);
    CHECK_EQ(offsetof(TutorialInstance, currentStep), (size_t)0x10);
    CHECK_EQ(offsetof(TutorialInstance, tutorialDef), (size_t)0x30);
    CHECK_DTOR_SLOT(TutorialInstance);
    CHECK_SLOT(TutorialInstance, TypeName, 0x08/8);
    CHECK_SLOT(TutorialInstance, Type,     0x10/8);
    CHECK_SLOT(TutorialInstance, Persist,  0x18/8);
    CheckShape<TutorialInstance, Object::Internal>(false);
}

// ===========================================================================
// Tutorial and TutorialStep : Object::External — 9 slots
// ===========================================================================
#include "Tutorial/Tutorial.h"
#include "Tutorial/TutorialStep.h"

UNIT_TEST(tutorial_and_step_are_retail_9_slots) {
    CheckExternalSlots<Tutorial>();
    CHECK_SLOT(Tutorial, Construct, 0x40/8);
    CheckShape<Tutorial, Object::External>(false);
    CHECK_EQ(sizeof(Tutorial), (size_t)0x40);
    CHECK_EQ(offsetof(Tutorial, currentStep), (size_t)0x38);
    CheckExternalSlots<TutorialStep>();
    CHECK_SLOT(TutorialStep, Construct, 0x40/8);
    CheckShape<TutorialStep, Object::External>(false);
    CHECK_EQ(sizeof(TutorialStep), (size_t)0x70);
    CHECK_EQ(offsetof(TutorialStep, goalCount), (size_t)0x68);
}

// ===========================================================================
// TutorialGoalDialog : TutorialGoal — 10 slots, 0x40
// ===========================================================================
#include "Tutorial/TutorialGoalDialog.h"

UNIT_TEST(tutorialgoaldialog_is_retail_10_slots_and_0x40) {
    CheckTutorialGoalLeaf<TutorialGoalDialog>();
    CHECK_EQ(sizeof(TutorialGoalDialog), (size_t)0x40);
    CHECK_EQ(offsetof(TutorialGoalDialog, targetStamp), (size_t)0x30);
    CHECK_EQ(offsetof(TutorialGoalDialog, targetDescription), (size_t)0x38);
}

// ===========================================================================
// TutorialSetupDialog : TutorialSetup — 12 slots, 0x78
// ===========================================================================
#include "Tutorial/TutorialSetupDialog.h"

UNIT_TEST(tutorialsetupdialog_is_retail_12_slots_and_0x78) {
    CheckTutorialSetupLeaf<TutorialSetupDialog>();
    CHECK_EQ(sizeof(TutorialSetupDialog), (size_t)0x78);
    CHECK_EQ(offsetof(TutorialSetupDialog, highlightDialog), (size_t)0x50);
    CHECK_EQ(offsetof(TutorialSetupDialog, toolbarStamp), (size_t)0x60);
    CHECK_EQ(offsetof(TutorialSetupDialog, toolbarIndex), (size_t)0x70);
}

// ===========================================================================
// TutorialHighlightDialog : UI::DialogInstance — 10 slots, 0xf0
// ===========================================================================
#include "Tutorial/TutorialHighlightDialog.h"

UNIT_TEST(tutorialhighlightdialog_is_retail_10_slots_and_0xf0) {
    using T = TutorialHighlightDialog;
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,            0x08/8);
    CHECK_SLOT(T, Type,                0x10/8);
    CHECK_SLOT(T, Persist,             0x18/8);
    CHECK_SLOT(T, OnOpened,            0x20/8);
    CHECK_SLOT(T, OnClosed,            0x28/8);
    CHECK_SLOT(T, OnUpdateLayout,      0x30/8);
    CHECK_SLOT(T, OnUpdate,            0x38/8);
    CHECK_SLOT(T, OnElementEvent,      0x40/8);
    CHECK_SLOT(T, OnSerializeSettings, 0x48/8);
    CheckShape<T, UI::DialogInstance>(false);
    CHECK_EQ(sizeof(T), (size_t)0xF0);
    CHECK_EQ(offsetof(T, elapsedTicks), (size_t)0xB8);
    CHECK_EQ(offsetof(T, rotation), (size_t)0xD8);
    CHECK_EQ(offsetof(T, targetEntity), (size_t)0xE8);
}

// ===========================================================================
// TutorialUI : UI::DialogController — 11 slots, 0x60
// ===========================================================================
#include "Tutorial/TutorialUI.h"

UNIT_TEST(tutorialui_is_retail_11_slots_and_0x60) {
    using T = TutorialUI;
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,            0x08/8);
    CHECK_SLOT(T, Type,                0x10/8);
    CHECK_SLOT(T, Persist,             0x18/8);
    CHECK_SLOT(T, OnCreated,           0x20/8);
    CHECK_SLOT(T, OnOpened,            0x28/8);
    CHECK_SLOT(T, OnClosed,            0x30/8);
    CHECK_SLOT(T, OnUpdateLayout,      0x38/8);
    CHECK_SLOT(T, OnUpdate,            0x40/8);
    CHECK_SLOT(T, OnElementEvent,      0x48/8);
    CHECK_SLOT(T, OnSerializeSettings, 0x50/8);
    CheckShape<T, UI::DialogController>(false);
    CHECK_EQ(sizeof(T), (size_t)0x60);
    CHECK_EQ(offsetof(T, nextButton), (size_t)0x50);
    CHECK_EQ(offsetof(T, quitButton), (size_t)0x58);
}
