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
// B.4 DLG-c: OnOk, OnCancel, OnApply, PopulateInputBindList and OnInputBindClick
// are private members too.
