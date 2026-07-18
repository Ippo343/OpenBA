// standalone/autostart.h
// Env-var-gated auto-driver that walks the standalone exe straight to a new
// game, so rendering/gameplay changes can be tested without manually clicking
// New Game -> Start each launch.
//
// Enabled by setting the BANISHED_AUTOSTART environment variable (any value
// except "0"/"false"). `make sa-run` sets it automatically; a direct launch
// of reimpl-x64.exe leaves it unset and stops at the menu as normal.
//
// Optional tuning via the same variable's value:
//   BANISHED_AUTOSTART=1        -> enabled, default settle (60 frames)
//   BANISHED_AUTOSTART=120      -> enabled, wait 120 menu frames before firing
//
// Mechanism: both the "New Game" and "Start" actions are already reimplemented
// as dialog vtable slot-8 handlers (MainMenuDialog::OnCommand /
// NewGameDialog::OnEvent). The driver injects the same synthetic button-click
// events the UI would dispatch — no mouse/input simulation needed — from the
// per-frame MenuDLT::Update tick.

#pragma once


namespace AutoStart {

// Read BANISHED_AUTOSTART once and latch the enabled flag + settle count.
// Safe to call multiple times (idempotent).
void Init();

// True if BANISHED_AUTOSTART requested auto-new-game.
bool Enabled();

// Per-frame driver. Call from MenuDLT::Update with the MenuDialogLoopT loop
// pointer (the menu loop whose +0x38 holds the live MainMenuDialog). No-op
// once the sequence has fired or if disabled.
void TickMenu(void* menuDLT);

}  // namespace AutoStart

// Set by the standalone NewGameDialog ctor (newgamedialog.cpp) so the driver
// can locate the dialog it just opened and fire its Start handler once setup
// (Slot4) has populated the combos + OK button. Null until OpenNewGame runs.
extern "C" void* g_StandaloneNewGameDialog;

