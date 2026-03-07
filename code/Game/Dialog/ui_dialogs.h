#pragma once
// Function declarations for deeply reimplemented UI dialog methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.
// No dependency on hooks.h or resolve.h — portable to standalone exe.

#include "ui/Dialogs.h"
#include <cstdint>

// ============================================================
// MessageDialog
// ============================================================

// Slot4 (vtable +0x20): Finds buttonCancel/buttonOk/buttonYes/buttonNo
// elements by name and assigns element IDs 0-3.
void MessageDialog_Slot4(MessageDialog* self);

// ============================================================
// EventPopupDialog
// ============================================================

// Slot4 (vtable +0x20): Finds imageIcon/textEvent/buttonOk elements,
// sets event icon sprite and text parameter, assigns button ID 200.
void EventPopupDialog_Slot4(EventPopupDialog* self);

// ============================================================
// LoaderDialog
// ============================================================

// Slot6 (vtable +0x30): Applies current rotation angle to loading spinner image.
void LoaderDialog_Slot6(LoaderDialog* self);

// Slot7 (vtable +0x38): Accumulates rotation angle from elapsed time (180 deg/sec).
void LoaderDialog_Slot7(LoaderDialog* self, void* time);

// ============================================================
// AutoSaveDialog
// ============================================================

// Slot6 (vtable +0x30): Toggles save/load indicator visibility based on
// SaveLoad::GetOperation, applies rotation to active indicator.
void AutoSaveDialog_Slot6(AutoSaveDialog* self);

// Slot7 (vtable +0x38): Accumulates rotation angle (90 deg/sec), wraps at 360.
void AutoSaveDialog_Slot7(AutoSaveDialog* self, void* time);

// ============================================================
// TutorialHighlightDialog
// ============================================================

// Startup (vtable +0x20): Finds "glowGroup", "glow", "arrow" elements
// and stores them at +0xC0, +0xC8, +0xD0.
void TutorialHighlightDialog_Startup(TutorialHighlightDialog* self);

// ============================================================
// MapParamsDialog
// ============================================================

// Startup (vtable +0x20): Finds all UI elements (editName, numberSeed,
// text labels, buttons), sets IDs, populates values from game state.
void MapParamsDialog_Startup(MapParamsDialog* self);

// Update (vtable +0x30): Enables/disables OK button based on name field state.
void MapParamsDialog_Update(MapParamsDialog* self);

// ============================================================
// MainMenuDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds all menu buttons, assigns element IDs,
// pauses audio if game active, hides conditional buttons, creates BuildId dialog.
void MainMenuDialog_Slot4(MainMenuDialog* self);

// Slot5 (vtable +0x28): Shutdown — unpauses audio scenes (channels 2, 3, 4).
void MainMenuDialog_Slot5(MainMenuDialog* self);

// Slot8 (vtable +0x40): OnCommand — dispatches menu button clicks.
// Opens sub-dialogs (New Game, Load, Save, Options, etc.) or quits.
void MainMenuDialog_Slot8(MainMenuDialog* self, void* param2, int param3, int param4);

// ============================================================
// ScenarioDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds buttons, queries scenario list,
// populates table with cloned template rows for each matching scenario.
void ScenarioDialog_Slot4(ScenarioDialog* self);

// Slot6 (vtable +0x30): Updates entry visibility based on selected index.
void ScenarioDialog_Slot6(ScenarioDialog* self);

// Slot8 (vtable +0x40): OnCommand — selection change and button dispatch.
void ScenarioDialog_Slot8(ScenarioDialog* self, uint64_t param2,
                          int param3, int param4);

// ============================================================
// ModDetailsDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds 12 UI elements, gets mod data from
// Modification singleton, populates name/author/path/version/description,
// builds conflict list and resource list markup strings.
void ModDetailsDialog_Slot4(ModDetailsDialog* self);

// Slot8 (vtable +0x40): OnCommand — close(0), upload(1), delete(2) dispatch.
void ModDetailsDialog_Slot8(ModDetailsDialog* self, uint64_t param2,
                            int param3, int param4);

// ============================================================
// ModDialog
// ============================================================

// Slot8 (vtable +0x40): OnCommand — dispatches mod list button clicks.
// Handles cancel(100), apply(101), toggle(1000+), up(2000+), down(3000+),
// top(4000+), details(5000+). Always calls base OnElementEvent.
void ModDialog_Slot8(ModDialog* self, void* param2, int msg, int id,
                     uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8);

// ============================================================
// OptionsDialog
// ============================================================

// Slot4 (vtable +0x20): Startup — finds all UI elements (sliders, combos,
// checkboxes), assigns IDs, copies current video/audio/input settings,
// populates combo boxes with available renderers/adapters/resolutions.
void OptionsDialog_Startup(OptionsDialog* self);

// Slot8 (vtable +0x40): OnEvent — handles option changes.
// msg 0x12: button clicks (OK=10, Cancel=11, Apply=12)
// msg 0x14: combo selection changes (renderer/adapter/resolution/refresh/antialias/filter)
// msg 0x17: slider value changes (UI/Music/Ambient/Effect volumes)
void OptionsDialog_OnEvent(OptionsDialog* self, void* element, int eventType, int eventData);

// ============================================================
// SaveLoadDialog
// ============================================================

// Startup (vtable +0x20): Finds 12 named elements (labelLoad/Save, buttonSave/Load/Cancel/Delete,
// mainPage, textLoading/Saving/PleaseWait, loadingImage, tableSaveList). Sets button IDs,
// initializes save sprite, starts list operation.
void SaveLoadDialog_Startup(SaveLoadDialog* self);

// Update (vtable +0x30): State machine for save/load UI. When idle, shows file list
// with selection highlighting and enables/disables buttons. During operations, shows
// loading page with rotating spinner and status text.
void SaveLoadDialog_Update(SaveLoadDialog* self);

// Load (vtable +0x38): Timer/state machine. Accumulates rotation, processes deferred
// flags when operation completes (refresh file list, close on save/load, re-list on delete).
void SaveLoadDialog_Load(SaveLoadDialog* self, void* time);

// ============================================================
// FollowDialog
// ============================================================

// Slot4 (vtable +0x20): Finds buttonClose (ID 100), textFollow, textAction.
// Sets followed entity name text and activates follow camera flag.
void FollowDialog_Slot4(FollowDialog* self);

// Slot7 (vtable +0x38): Update — reads character position, updates camera target.
// Checks Escape key to close dialog.
void FollowDialog_Slot7(FollowDialog* self);

// ============================================================
// NewGameDialog
// ============================================================

// Slot4 (vtable +0x20): Startup — finds UI elements, populates combo boxes
// with map/climate/terrain generators, generates random name and seed,
// restores saved combo selections from game settings.
void NewGameDialog_Slot4(NewGameDialog* self);

// Slot6 (vtable +0x30): Update — enables/disables OK button based on
// whether the name field has content.
void NewGameDialog_Slot6(NewGameDialog* self);

// ============================================================
// AchievementDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds buttonOk, tableAchievement, textComplete.
// Clones template rows, populates each with achievement icon, star, title, description.
// Builds entry array at +0xC0 (stride 0x20) for Slot6 to update colors.
void AchievementDialog_Slot4(AchievementDialog* self);

// ============================================================
// GoalDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds buttonOk, tableGoal, textScenario.
// Creates rows from template for each goal, populates text, hides indicators.
// Sets scenario title on textScenario or hides it.
void GoalDialog_Slot4(GoalDialog* self);

// Slot6 (vtable +0x30): Updates goal entry visibility (met/failed indicators).
void GoalDialog_Slot6(GoalDialog* self);

// ============================================================
// EventNotifyDialog
// ============================================================

// Slot4 (vtable +0x20): Startup — finds ribbonNotify, notifyContent,
// builds notification entry array from children. Sets button IDs.
void EventNotifyDialog_Slot4(EventNotifyDialog* self);

// Slot7 (vtable +0x38): Event processing — decrements notification timers,
// scans event log for new events, creates notification entries with icons/colors,
// plays sound effect on new events.
void EventNotifyDialog_Slot7(EventNotifyDialog* self, void* param2);

// ============================================================
// AchievementDialog
// ============================================================

// Slot6 (vtable +0x30): Updates achievement display — checks awarded status,
// sets entry colors and check visibility, formats percentage text.
void AchievementDialog_Slot6(AchievementDialog* self);

// Slot14 (vtable +0x70): Finds ProgressElement by type, stores at +0x30,
// reads attendance capacity from sub-component, sets progress max.
void AchievementDialog_Slot14(AchievementDialog* self);

// Slot17 (vtable +0x88): Reads attendance count, updates progress bar value and text.
void AchievementDialog_Slot17(AchievementDialog* self);

// ============================================================
// ToolbarDialog
// ============================================================

// Slot6 (vtable +0x30): Update — syncs toolbar button states with game state.
// Iterates items, updates checked/enabled based on speed, pause, group visibility.
void ToolbarDialog_Update(void* self, uint64_t param2, uint64_t param3, uint64_t param4);

// Slot8 (vtable +0x40): OnEvent — handles toolbar button clicks.
// Dispatches to game actions: open sub-toolbar, toggle groups, change speed, etc.
void ToolbarDialog_OnEvent(void* self, void* element, int eventType, int eventData,
                           uint64_t p5, uint64_t p6, int p7);

// ============================================================
// OptionInputBindDialog
// ============================================================

// Startup (vtable +0x20): Finds buttonCancel/Ok/Yes/No, sets element IDs 0-3.
void OptionInputBindDialog_Startup(OptionInputBindDialog* self);

// Load (vtable +0x38): Polls keyboard (0-0x68) and mouse (4-0xB) for released
// keys/buttons. On detection, stores key index at +0xC0 and input type at +0xC4,
// then calls OnOk to close the dialog.
void OptionInputBindDialog_Load(OptionInputBindDialog* self);

// OnOk (vtable +0x50): Sets result value 1, closes dialog.
void OptionInputBindDialog_OnOk(OptionInputBindDialog* self);

// OnCancel (vtable +0x58): Sets result value 0, closes dialog.
void OptionInputBindDialog_OnCancel(OptionInputBindDialog* self);

// OnButton2 (vtable +0x60): Sets result value 2, closes dialog.
void OptionInputBindDialog_OnButton2(OptionInputBindDialog* self);

// OnButton3 (vtable +0x68): Sets result value 3, closes dialog.
void OptionInputBindDialog_OnButton3(OptionInputBindDialog* self);
