// ============================================================================
// tests/unit/dlg_abi_test.cpp — ENDGAME B.3.8 block DLG (B38_RULES D1-D5, B8-14)
//
// The ABI claims block DLG makes for the exe `UI::DialogInstance` subclasses,
// measured on the SHIPPED class headers under code/Game/Dialog/:
//
// PART A — LAYOUT. sizeof (= retail's create MemAlloc, B38_RULES section 1.3)
//   and alignof of every class, and the offsets of the fields past +0xb8.
// PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0: single inheritance). The retail
//   table is DialogInstance's 10 slots ([0] dtor, [1] TypeName, [2] Type,
//   [3] Persist, [4] OnOpened, [5] OnClosed, [6] OnUpdateLayout, [7] OnUpdate,
//   [8] OnElementEvent, [9] OnSerializeSettings); MessageDialog adds 10-13.
// PART C — SHAPE. polymorphic, NOT abstract (every class here is concrete in
//   retail), a virtual destructor (B3X W1; was "none", ENDGAME decision 2), the proved parent.
//
// NOT PROVEN HERE: the member bodies (they stand on the address registry and
// the UI runtime and cannot link into a host test) and `Vtable()` against a
// constructed object (the pilot's exe_class_abi_test.cpp PART C proves the
// DEFINE_EXE_VTABLE mechanism on this toolchain). The real run and the resave
// gate verify those.
// ============================================================================
#include "unit.h"
#include "slot_check.h"

#include "Dialog/MessageDialog.h"
#include "Dialog/OptionInputBindDialog.h"
#include "Dialog/EventNotifyDialog.h"
#include "Dialog/EventPopupDialog.h"
#include "Dialog/LoaderDialog.h"
#include "Dialog/AutoSaveDialog.h"
#include "Dialog/MainMenuDialog.h"
#include "Dialog/MapParamsDialog.h"
#include "Dialog/AchievementDialog.h"
#include "Dialog/GoalDialog.h"
#include "Dialog/ScenarioDialog.h"
#include "Dialog/FollowDialog.h"
#include "Dialog/NewGameDialog.h"
#include "Dialog/OptionsDialog.h"
#include "Dialog/SaveLoadDialog.h"
#include "Dialog/ModDialog.h"
#include "Dialog/ModDetailsDialog.h"
#include "Dialog/ModUploadDialog.h"
#include "Dialog/ToolbarDialog.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

template <typename T>
void CheckShape() {
    CHECK(std::is_polymorphic<T>::value);
    CHECK(!std::is_abstract<T>::value);
    CHECK(std::has_virtual_destructor<T>::value);
    CHECK((std::is_base_of<UI::DialogInstance, T>::value));
    CHECK_EQ(alignof(T), (size_t)8);
}

// The DialogInstance slots every dialog carries, read through class T (an
// override or the inherited base body answers at the same offset).
template <typename T>
void CheckDialogInstanceSlots() {
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
}

}  // namespace

// ===========================================================================
// MessageDialog (0x14015c480, 14 slots, 0xc0) -> OptionInputBindDialog
// (0x14015ce40, 14 slots, 0xc8)
// ===========================================================================
UNIT_TEST(dlg_message_dialog_layout) {
    CHECK_EQ(sizeof(MessageDialog),         (size_t)0xc0);
    CHECK_EQ(sizeof(OptionInputBindDialog), (size_t)0xc8);
    CHECK_EQ(offsetof(MessageDialog, dataB8),             (size_t)0xb8);
    CHECK_EQ(offsetof(OptionInputBindDialog, keyIndex),   (size_t)0xc0);
    CHECK_EQ(offsetof(OptionInputBindDialog, inputType),  (size_t)0xc4);
}

UNIT_TEST(dlg_message_dialog_slots) {
    CheckShape<MessageDialog>();
    CheckShape<OptionInputBindDialog>();
    CHECK((std::is_base_of<MessageDialog, OptionInputBindDialog>::value));
    CheckDialogInstanceSlots<MessageDialog>();
    CheckDialogInstanceSlots<OptionInputBindDialog>();
    CHECK_SLOT(MessageDialog, OnOk,         0x50/8);
    CHECK_SLOT(MessageDialog, OnCancel,     0x58/8);
    CHECK_SLOT(MessageDialog, OnYes,        0x60/8);
    CHECK_SLOT(MessageDialog, OnNo,         0x68/8);
    CHECK_SLOT(OptionInputBindDialog, OnOk, 0x50/8);
    CHECK_SLOT(OptionInputBindDialog, OnNo, 0x68/8);
}

// ===========================================================================
// EventNotifyDialog (0x14015bca0, 10 slots, 0xf0), EventPopupDialog
// (0x14015bd80, 10 slots, 0xc0)
// ===========================================================================
UNIT_TEST(dlg_event_dialogs) {
    CHECK_EQ(sizeof(EventNotifyDialog), (size_t)0xf0);
    CHECK_EQ(sizeof(EventPopupDialog),  (size_t)0xc0);
    CHECK_EQ(offsetof(EventNotifyDialog, dataC4), (size_t)0xc4);
    CHECK_EQ(offsetof(EventNotifyDialog, dataE8), (size_t)0xe8);
    CHECK_EQ(offsetof(EventPopupDialog, eventIndex), (size_t)0xb8);
    CheckShape<EventNotifyDialog>();
    CheckShape<EventPopupDialog>();
    CheckDialogInstanceSlots<EventNotifyDialog>();
    CheckDialogInstanceSlots<EventPopupDialog>();
}

// ===========================================================================
// LoaderDialog (0x14015f300, 10 slots, 0xc8), AutoSaveDialog (0x14015b120,
// 10 slots, 0xd0)
// ===========================================================================
UNIT_TEST(dlg_loader_autosave_dialogs) {
    CHECK_EQ(sizeof(LoaderDialog),   (size_t)0xc8);
    CHECK_EQ(sizeof(AutoSaveDialog), (size_t)0xd0);
    CHECK_EQ(offsetof(LoaderDialog, loadingImage), (size_t)0xb8);
    CHECK_EQ(offsetof(LoaderDialog, rotation),     (size_t)0xc0);
    CHECK_EQ(offsetof(AutoSaveDialog, dataC0),     (size_t)0xc0);
    CHECK_EQ(offsetof(AutoSaveDialog, angle),      (size_t)0xc8);
    CheckShape<LoaderDialog>();
    CheckShape<AutoSaveDialog>();
    CheckDialogInstanceSlots<LoaderDialog>();
    CheckDialogInstanceSlots<AutoSaveDialog>();
}

UNIT_TEST(dlg_mainmenu_mapparams_dialogs) {
    CHECK_EQ(sizeof(MainMenuDialog), (size_t)0xc8);
    CHECK_EQ(offsetof(MainMenuDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(MainMenuDialog, dataC0), (size_t)0xc0);
    CheckShape<MainMenuDialog>();
    CheckDialogInstanceSlots<MainMenuDialog>();
    CHECK_EQ(sizeof(MapParamsDialog), (size_t)0xf8);
    CHECK_EQ(offsetof(MapParamsDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(MapParamsDialog, dataF0), (size_t)0xf0);
    CheckShape<MapParamsDialog>();
    CheckDialogInstanceSlots<MapParamsDialog>();
}

UNIT_TEST(dlg_achievement_goal_dialogs) {
    CHECK_EQ(sizeof(AchievementDialog), (size_t)0xd0);
    CHECK_EQ(offsetof(AchievementDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(AchievementDialog, dataC0), (size_t)0xc0);
    CHECK_EQ(offsetof(AchievementDialog, dataC8), (size_t)0xc8);
    CheckShape<AchievementDialog>();
    CheckDialogInstanceSlots<AchievementDialog>();
    CHECK_EQ(sizeof(GoalDialog), (size_t)0xc8);
    CHECK_EQ(offsetof(GoalDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(GoalDialog, dataC0), (size_t)0xc0);
    CheckShape<GoalDialog>();
    CheckDialogInstanceSlots<GoalDialog>();
}

UNIT_TEST(dlg_scenario_dialog) {
    CHECK_EQ(sizeof(ScenarioDialog), (size_t)0xe8);
    CHECK_EQ(offsetof(ScenarioDialog, difficulty), (size_t)0xb8);
    CHECK_EQ(offsetof(ScenarioDialog, dataC0), (size_t)0xc0);
    CHECK_EQ(offsetof(ScenarioDialog, dataC8), (size_t)0xc8);
    CHECK_EQ(offsetof(ScenarioDialog, dataD8), (size_t)0xd8);
    CHECK_EQ(offsetof(ScenarioDialog, dataE0), (size_t)0xe0);
    CheckShape<ScenarioDialog>();
    CheckDialogInstanceSlots<ScenarioDialog>();
}

UNIT_TEST(dlg_follow_dialog) {
    CHECK_EQ(sizeof(FollowDialog), (size_t)0xd8);
    CHECK_EQ(offsetof(FollowDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(FollowDialog, dataD0), (size_t)0xd0);
    CheckShape<FollowDialog>();
    CheckDialogInstanceSlots<FollowDialog>();
}

UNIT_TEST(dlg_newgame_dialog) {
    CHECK_EQ(sizeof(NewGameDialog), (size_t)0xae8);
    CHECK_EQ(offsetof(NewGameDialog, comboTerrain), (size_t)0xb8);
    CHECK_EQ(offsetof(NewGameDialog, random), (size_t)0x118);
    CHECK_EQ(offsetof(NewGameDialog, worldSetup), (size_t)0xae0);
    CheckShape<NewGameDialog>();
    CheckDialogInstanceSlots<NewGameDialog>();
}

UNIT_TEST(dlg_options_dialog) {
    CHECK_EQ(sizeof(OptionsDialog), (size_t)0x1c0);
    CHECK_EQ(offsetof(OptionsDialog, comboRenderer), (size_t)0xb8);
    CHECK_EQ(offsetof(OptionsDialog, savedRenderer), (size_t)0x190);
    CHECK_EQ(offsetof(OptionsDialog, checkMouseClip), (size_t)0x1b0);
    CHECK_EQ(offsetof(OptionsDialog, tabOptions), (size_t)0x1b8);
    CheckShape<OptionsDialog>();
    CheckDialogInstanceSlots<OptionsDialog>();
}

UNIT_TEST(dlg_saveload_dialog) {
    CHECK_EQ(sizeof(SaveLoadDialog), (size_t)0x160);
    CHECK_EQ(offsetof(SaveLoadDialog, mode), (size_t)0xb8);
    CHECK_EQ(offsetof(SaveLoadDialog, selectedIndex), (size_t)0xc0);
    CHECK_EQ(offsetof(SaveLoadDialog, rotation), (size_t)0xc8);
    CHECK_EQ(offsetof(SaveLoadDialog, sprite), (size_t)0xd0);
    CHECK_EQ(offsetof(SaveLoadDialog, engine), (size_t)0xe0);
    CHECK_EQ(offsetof(SaveLoadDialog, gameWorld), (size_t)0xe8);
    CHECK_EQ(offsetof(SaveLoadDialog, dataF0), (size_t)0xf0);
    CheckShape<SaveLoadDialog>();
    CheckDialogInstanceSlots<SaveLoadDialog>();
}

UNIT_TEST(dlg_mod_dialogs) {
    CHECK_EQ(sizeof(ModDialog), (size_t)0xd8);
    CHECK_EQ(offsetof(ModDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(ModDialog, dataD0), (size_t)0xd0);
    CheckShape<ModDialog>();
    CheckDialogInstanceSlots<ModDialog>();
    CHECK_EQ(sizeof(ModDetailsDialog), (size_t)0xc8);
    CHECK_EQ(offsetof(ModDetailsDialog, dataB8), (size_t)0xb8);
    CHECK_EQ(offsetof(ModDetailsDialog, modIndex), (size_t)0xc0);
    CheckShape<ModDetailsDialog>();
    CheckDialogInstanceSlots<ModDetailsDialog>();
}

UNIT_TEST(dlg_modupload_dialog) {
    CHECK_EQ(sizeof(ModUploadDialog), (size_t)0xb8);
    CheckShape<ModUploadDialog>();
    CheckDialogInstanceSlots<ModUploadDialog>();
}

UNIT_TEST(dlg_toolbar_dialog) {
    CHECK_EQ(sizeof(ToolbarDialog), (size_t)0x108);
    CHECK_EQ(offsetof(ToolbarDialog, radioElement), (size_t)0xb8);
    CHECK_EQ(offsetof(ToolbarDialog, buttonElements), (size_t)0xc0);
    CHECK_EQ(offsetof(ToolbarDialog, toolbarItems), (size_t)0xd0);
    CHECK_EQ(offsetof(ToolbarDialog, subDialog), (size_t)0xe8);
    CHECK_EQ(offsetof(ToolbarDialog, openSubIndex), (size_t)0xf0);
    CHECK_EQ(offsetof(ToolbarDialog, speedValues), (size_t)0xf8);
    CHECK_EQ(offsetof(ToolbarDialog, speedValueCount), (size_t)0x100);
    CheckShape<ToolbarDialog>();
    CheckDialogInstanceSlots<ToolbarDialog>();
}

// B38 D3.1: the slots retail fills with DialogInstance's body are INHERITED,
// not overridden. For an inherited member, &T::M names DialogInstance's member,
// so its pointer type is `R (UI::DialogInstance::*)(...)`.
namespace {
template <typename T> constexpr bool InheritsOnClosed =
    std::is_same<decltype(&T::OnClosed), void (UI::DialogInstance::*)()>::value;
template <typename T> constexpr bool InheritsOnUpdateLayout =
    std::is_same<decltype(&T::OnUpdateLayout), void (UI::DialogInstance::*)()>::value;
template <typename T> constexpr bool InheritsOnUpdate =
    std::is_same<decltype(&T::OnUpdate), void (UI::DialogInstance::*)(const System::Time&)>::value;
template <typename T> constexpr bool InheritsOnElementEvent =
    std::is_same<decltype(&T::OnElementEvent),
                 void (UI::DialogInstance::*)(Object::InternalPtr<UI::Element>*, int32_t, int32_t,
                                              int32_t, int32_t, int32_t, void*)>::value;
}  // namespace

UNIT_TEST(dlg_d31_inherited_slots) {
    CHECK(InheritsOnUpdateLayout<MessageDialog>);
    CHECK(InheritsOnUpdate<MessageDialog>);
    CHECK(InheritsOnClosed<EventPopupDialog>);
    CHECK(InheritsOnUpdateLayout<EventPopupDialog>);
    CHECK(InheritsOnUpdate<EventPopupDialog>);
    CHECK(InheritsOnClosed<LoaderDialog>);
    CHECK(InheritsOnElementEvent<LoaderDialog>);
    CHECK(InheritsOnClosed<AutoSaveDialog>);
    CHECK(InheritsOnClosed<AchievementDialog>);
    CHECK(InheritsOnClosed<GoalDialog>);
    CHECK(InheritsOnClosed<ScenarioDialog>);
    CHECK(InheritsOnUpdateLayout<OptionsDialog>);
    CHECK(InheritsOnUpdate<OptionsDialog>);
    CHECK(InheritsOnClosed<SaveLoadDialog>);
    CHECK(InheritsOnClosed<ModUploadDialog>);
    CHECK(InheritsOnUpdate<ToolbarDialog>);
    // and an own slot is NOT inherited
    CHECK(!InheritsOnClosed<ToolbarDialog>);
    CHECK(!InheritsOnClosed<MessageDialog>);
}
