#pragma once
// Function declarations for deeply reimplemented UI dialog methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.

#include "ui/Dialogs.h"
#include <cstdint>

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

