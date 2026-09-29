#pragma once
// Function declarations for deeply reimplemented UI dialog methods.
// These are clean game logic functions called from engine/reimpl/ wrappers.

#include "ui/Dialogs.h"
#include <cstdint>

// ============================================================
// MainMenuDialog
// ============================================================

// Slot4 (vtable +0x20): Setup — finds all menu buttons, assigns element IDs,
// pauses audio if game active, hides conditional buttons, creates BuildId dialog.
void MainMenuDialog_Slot4(MainMenuDialog* self);

// Slot5 (vtable +0x28): Shutdown — unpauses audio scenes (channels 2, 3, 4).
void MainMenuDialog_Slot5(MainMenuDialog* self);

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
