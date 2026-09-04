#pragma once
// Function declarations for deeply reimplemented UI dialog methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.

#include "ui/Dialogs.h"
#include <cstdint>

// ============================================================
// MessageDialog
// ============================================================

// Slot4 (vtable +0x20): Finds buttonCancel/buttonOk/buttonYes/buttonNo
// elements by name and assigns element IDs 0-3.
void MessageDialog_Slot4(MessageDialog* self);

// Slot5 (vtable +0x28): Shutdown — 3-byte no-op, returns 0.
int MessageDialog_Slot5(MessageDialog* self);

// Slot8 (vtable +0x40): OnCommand — dispatches button clicks through vtable.
// msg 0x12: id 0=Cancel(+0x58), 1=Ok(+0x50), 2=Yes(+0x60), 3=No(+0x68).
void MessageDialog_Slot8(MessageDialog* self, void* param2, int param3, int param4);

// Slot10 (vtable +0x50): ButtonOk — sets result=1, closes dialog.
void MessageDialog_Slot10(MessageDialog* self);

// Slot11 (vtable +0x58): ButtonCancel — sets result=0, closes dialog.
void MessageDialog_Slot11(MessageDialog* self);

// Slot12 (vtable +0x60): ButtonYes — sets result=2, closes dialog.
void MessageDialog_Slot12(MessageDialog* self);

// Slot13 (vtable +0x68): ButtonNo — sets result=3, closes dialog.
void MessageDialog_Slot13(MessageDialog* self);

// ============================================================
// EventPopupDialog
// ============================================================

// Slot4 (vtable +0x20): Finds imageIcon/textEvent/buttonOk elements,
// sets event icon sprite and text parameter, assigns button ID 200.
void EventPopupDialog_Slot4(EventPopupDialog* self);

// Slot8 (vtable +0x40): OnCommand — close dialog on msg=0x12, id=200.
void EventPopupDialog_Slot8(EventPopupDialog* self, void* param2, int param3, int param4);

// ============================================================
// LoaderDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds "loadingImage" element, stores at +0xB8.
void LoaderDialog_Slot4(LoaderDialog* self);

// Slot6 (vtable +0x30): Applies current rotation angle to loading spinner image.
void LoaderDialog_Slot6(LoaderDialog* self);

// Slot7 (vtable +0x38): Accumulates rotation angle from elapsed time (180 deg/sec).
void LoaderDialog_Slot7(LoaderDialog* self, void* time);

// ============================================================
// AutoSaveDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — stops timer, picks autosave interval by
// game speed, starts new timer. JMP thunk to 0x14005E120.
void AutoSaveDialog_Slot4(AutoSaveDialog* self);

// Slot6 (vtable +0x30): Toggles save/load indicator visibility based on
// SaveLoad::GetOperation, applies rotation to active indicator.
void AutoSaveDialog_Slot6(AutoSaveDialog* self);

// Slot7 (vtable +0x38): Accumulates rotation angle (90 deg/sec), wraps at 360.
void AutoSaveDialog_Slot7(AutoSaveDialog* self, void* time);

// Slot8 (vtable +0x40): OnCommand — if msg==0x13 (timer), triggers autosave.
void AutoSaveDialog_Slot8(AutoSaveDialog* self, void* param2, int param3, int param4);

// ============================================================
// TutorialGoalDialog
// ============================================================

// Slot3 (vtable +0x18, exe 0x1400df080): Returns 1 (type constant).
int TutorialGoalDialog_Slot3(void* self);

// Slot7 (vtable +0x38, exe 0x140001640): Delegator — calls vtable[0] with flags=0.
void TutorialGoalDialog_Slot7(void* self);

// ============================================================
// TutorialHighlightDialog
// ============================================================

// Startup (vtable +0x20): Finds "glowGroup", "glow", "arrow" elements
// and stores them at +0xC0, +0xC8, +0xD0.
void TutorialHighlightDialog_Startup(TutorialHighlightDialog* self);

// Update (vtable +0x30): Positions glow+arrow overlay each frame.
//   If dataE8 != 0: positions arrow based on world-space rect stored in dataE8.
//   If dataE8 == 0: positions glow+arrow based on the target UI element in dataE0.
// Unhookable via trampoline (rel branch at byte 8); installed via vtable patch.
void TutorialHighlightDialog_Draw(TutorialHighlightDialog* self);

// Load (vtable +0x38): Accumulates delta-time ticks into dataB8 (System::Time).
// param2 is a pointer to the tick count to add. Original is only 11 bytes —
// unhookable via trampoline; installed via vtable patch.
void TutorialHighlightDialog_Load(TutorialHighlightDialog* self, void* param2);

// ============================================================
// MapParamsDialog
// ============================================================

// Slot5 (vtable +0x28): Shutdown — 3-byte no-op, returns 0.
int MapParamsDialog_Slot5(MapParamsDialog* self);

// Startup (vtable +0x20): Finds all UI elements (editName, numberSeed,
// text labels, buttons), sets IDs, populates values from game state.
void MapParamsDialog_Startup(MapParamsDialog* self);

// Update (vtable +0x30): Enables/disables OK button based on name field state.
void MapParamsDialog_Update(MapParamsDialog* self);

// OnEvent (vtable +0x40, exe 0x14006b5f0): Handles button clicks.
// eventType 0x12 (button): 0=newName(randomize), 1=ok(save+close), 2=cancel(close)
void MapParamsDialog_OnEvent(MapParamsDialog* self, void* element, int eventType, int eventData);

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

// Slot7 (vtable +0x38, exe 0x140078000): RET 0 no-op (3 bytes).
int ScenarioDialog_Slot7(ScenarioDialog* self);

// Slot8 (vtable +0x40): OnCommand — selection change and button dispatch.
void ScenarioDialog_Slot8(ScenarioDialog* self, uint64_t param2,
                          int param3, int param4);

// Ctor (0x1400786b0): shared with TutorialDialog (difficulty byte differentiates).
void* ScenarioDialog_Ctor(void* self, void* ui, void* extRes, void* parentIBP,
                          void* parentMenuIBP, uint8_t difficulty);

// Returns the standalone-built vtable_ScenarioDialog (0x14015d2d0) array.
void* ScenarioDialog_GetStandaloneVtable();

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

// Slot4 (vtable +0x20): Setup — populates mod list table with cloned template
// rows, creates per-mod ImageBuffer/MaterialInstance/SpriteSheet thumbnail
// objects, attaches browser controller from "Dialog/Mod.rsc:browser".
void ModDialog_Slot4(ModDialog* self);

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

// OnOk: Applies all settings, saves to disk, closes dialog.
void OptionsDialog_OnOk(OptionsDialog* self);

// OnCancel: Reverts all settings from disk, closes dialog.
void OptionsDialog_OnCancel(OptionsDialog* self);

// OnApply: Applies all settings, saves to disk (keeps dialog open).
void OptionsDialog_OnApply(OptionsDialog* self);

// PopulateInputBindList: Fills input binding list with key names and element IDs.
void OptionsDialog_PopulateInputBindList(OptionsDialog* self);

// OnInputBindClick: Shows input bind dialog, handles key rebinding.
void OptionsDialog_OnInputBindClick(OptionsDialog* self, void* element, int bindIndex, int slot);

// Shutdown (vtable +0x28, exe 0x140073480): RET 0 no-op (3 bytes).
int OptionsDialog_Shutdown(OptionsDialog* self);

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
// flags when operation completes (refresh file list, close on save/load, re-list on delete,
// construct game loader loop chain on load completion).
void SaveLoadDialog_Load(SaveLoadDialog* self, void* time, void** loaderLoopOut, uint64_t param4);

// OnEvent (vtable +0x40, exe 0x1400b67e0): Handles button clicks and list selection.
// eventType 2 (list select): eventData in (3, saveCount+4) → select entry
// eventType 0x12 (button): 0=cancel(close), 1=save, 2=load, 3=delete
void SaveLoadDialog_OnEvent(SaveLoadDialog* self, void* element, int eventType, int eventData);

// ============================================================
// FollowDialog
// ============================================================

// Slot4 (vtable +0x20): Finds buttonClose (ID 100), textFollow, textAction.
// Sets followed entity name text and activates follow camera flag.
void FollowDialog_Slot4(FollowDialog* self);

// Slot5 (vtable +0x28): Shutdown — clears follow-active flag (gameWorld->+0x120->+0x38 = 0).
void FollowDialog_Slot5(FollowDialog* self);

// Slot6 (vtable +0x30): Draw — updates profession text (parameter 1 on textFollow)
// and action text (textAction) each frame from citizen's current state.
void FollowDialog_Slot6(FollowDialog* self, void* p2, void* p3, void* p4);

// Slot7 (vtable +0x38): Update — reads character position, updates camera target.
// Checks Escape key to close dialog.
void FollowDialog_Slot7(FollowDialog* self);

// Slot8 (vtable +0x40): OnCommand — handles buttonClose click (id 100).
// Nulls back-reference on followed entity, closes dialog.
void FollowDialog_Slot8(FollowDialog* self, void* param2, int param3, int param4);

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

// Slot5 (vtable +0x28, exe 0x1400706c0): Shutdown — RET 0 no-op (3 bytes).
int NewGameDialog_Shutdown(NewGameDialog* self);

// OnEvent (vtable +0x40, exe 0x140070110): Handles button clicks and combo changes.
// eventType 0x12 (button): 0=newName, 1=newSeed, 2=ok(startGame), 3=cancel(close)
// eventType 0x14 (combo): 4=comboCondition (update description text)
void NewGameDialog_OnEvent(NewGameDialog* self, void* element, int eventType, int eventData);

// ============================================================
// TutorialSetupDialog
// ============================================================

// ScalarDtor (vtable slot 0, exe 0x140028620):
// Restores vtable, destroys string vectors and inline strings,
// calls External::~External, conditionally frees memory.
void* TutorialSetupDialog_ScalarDtor(void* self, uint32_t flags);

// Shutdown (vtable slot 5, exe 0x1400e2a60):
// Serializes/deserializes tutorial setup state: two String vectors,
// two inline Strings, one External pointer, two int fields.
// Returns 1 always.
uint64_t TutorialSetupDialog_Shutdown(void* self, void* stream);

// OnEvent (vtable slot 6, exe 0x140028600):
// Resets object by calling virtual dtor (no-free) then re-running Ctor.
void TutorialSetupDialog_OnEvent(void* self);

// Slot8 (vtable +0x40, exe 0x1400e25f0):
// Runs setup helper + validate state; iterates 2nd string vector to find open dialogs.
// For each: if entity IsA BuildComponent -> follow building; if ComponentEntity -> close.
void TutorialSetupDialog_Slot8(void* self);

// Slot9 (vtable +0x48, exe 0x1400e24c0):
// Activates tutorial setup: runs setup helper then opens tutorial dialog (start mode).
void TutorialSetupDialog_Slot9(void* self);

// Slot10 (vtable +0x50, exe 0x1400e2460):
// Shows all dialogs, finds current TutorialSetupDialog instance, closes it.
void TutorialSetupDialog_Slot10(void* self);

// Slot3 (vtable +0x18, exe 0x1400e2be0): Returns 6 (type constant).
int TutorialSetupDialog_Slot3(void* self);

// Slot7 (vtable +0x38, exe 0x140001640): Delegator — calls vtable[0] with flags=0.
void TutorialSetupDialog_Slot7(void* self);

// Slot11 (vtable +0x58, exe 0x1400285f0): Ctor wrapper — TEST+branch,
// constructs the object if non-null (sets vtables, zeros fields).
void* TutorialSetupDialog_Slot11(void* self);

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

// Slot7 (vtable +0x38): No-op handler — returns 0.
int GoalDialog_Slot7(GoalDialog* self);

// Slot8 (vtable +0x40): OnCommand — close dialog on msg=0x12, id=100.
void GoalDialog_Slot8(GoalDialog* self, void* param2, int param3, int param4);

// ============================================================
// EventNotifyDialog
// ============================================================

// Slot4 (vtable +0x20): Startup — finds ribbonNotify, notifyContent,
// builds notification entry array from children. Sets button IDs.
void EventNotifyDialog_Slot4(EventNotifyDialog* self);

// Slot5 (vtable +0x28): Shutdown — 3-byte no-op, returns 0.
int EventNotifyDialog_Slot5(EventNotifyDialog* self);

// Slot6 (vtable +0x30): Update — smoothly positions the notification ribbon
// relative to the toolbar, updates per-slot sprites/flags/colors/text.
// For active slots: sets icon sprite, toggles button/image visibility based on
// popup state, sets color on button controller and its children.
// For empty slots: clears color, hides image, shows button, clears text.
void EventNotifyDialog_Update(EventNotifyDialog* self);

// Slot7 (vtable +0x38): Event processing — decrements notification timers,
// scans event log for new events, creates notification entries with icons/colors,
// plays sound effect on new events.
void EventNotifyDialog_Slot7(EventNotifyDialog* self, void* param2);

// OnEvent (vtable +0x40, exe 0x140068680): Handles notification button clicks.
// eventType 0x12 (button): eventData in range [0x5DC, 0x5DC+activeCount) →
//   navigate to event position on map (calls FUN_1400685a0).
void EventNotifyDialog_OnEvent(EventNotifyDialog* self, void* element, int eventType, int eventData);

// ============================================================
// AchievementDialog
// ============================================================

// Slot6 (vtable +0x30): Updates achievement display — checks awarded status,
// sets entry colors and check visibility, formats percentage text.
void AchievementDialog_Slot6(AchievementDialog* self);

// Slot7 (vtable +0x38, exe 0x14005D480): RET 0 no-op (3 bytes).
int AchievementDialog_Slot7(AchievementDialog* self);

// Slot8 (vtable +0x40, exe 0x14005D440): OnCommand -- buttonOk (id 100) closes.
void AchievementDialog_Slot8(AchievementDialog* self, void* param2, int param3, int param4);

// Slot14 (vtable +0x70): Finds ProgressElement by type, stores at +0x30,
// reads attendance capacity from sub-component, sets progress max.
void AchievementDialog_Slot14(AchievementDialog* self);

// Slot17 (vtable +0x88): Reads attendance count, updates progress bar value and text.
void AchievementDialog_Slot17(AchievementDialog* self);

// ============================================================
// ToolbarDialog
// ============================================================

// Startup (vtable +0x20, exe 0x1400b97c0): RET 0 no-op (3 bytes).
int ToolbarDialog_Startup(void* self);

// Shutdown (vtable +0x28, exe 0x1400b97b0): JMP to CloseSubDialog.
void ToolbarDialog_Shutdown(void* self);

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

// Shutdown (vtable +0x28, exe 0x14006d0c0): RET 0 no-op (3 bytes).
int OptionInputBindDialog_Shutdown(OptionInputBindDialog* self);

// OnEvent (vtable +0x40, exe 0x14006d070): Dispatches button clicks to OnOk/OnCancel/etc.
// eventType 0x12 (button): 0=cancel, 1=ok, 2=button2, 3=button3
// Implemented by dispatching through vtable slots +0x58/+0x50/+0x60/+0x68.
void OptionInputBindDialog_OnEvent(OptionInputBindDialog* self, void* element, int eventType, int eventData);
