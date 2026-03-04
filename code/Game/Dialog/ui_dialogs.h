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

// Slot5 (vtable +0x28): Shutdown — unpauses audio scenes (channels 2, 3, 4).
void MainMenuDialog_Slot5(MainMenuDialog* self);

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

// Slot8 (vtable +0x40): OnCommand — close(0), upload(1), delete(2) dispatch.
void ModDetailsDialog_Slot8(ModDetailsDialog* self, uint64_t param2,
                            int param3, int param4);

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
// AchievementDialog
// ============================================================

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
