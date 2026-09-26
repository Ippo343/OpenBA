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
// OptionsDialog
// ============================================================

// ENDGAME B.3.8 DLG: OptionsDialog is a real class (Dialog/OptionsDialog.h);
// its slots [4] OnOpened, [5] OnClosed and [8] OnElementEvent are members.
// The non-virtual helpers below stay free functions.

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

