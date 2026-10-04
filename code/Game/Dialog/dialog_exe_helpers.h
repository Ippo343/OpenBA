#pragma once
// Declarations for Dialog-module exe-helper reimplementations.
// See code/Game/Dialog/dialog_exe_helpers.cpp.
#include <cstdint>

// FUN_14005dec0 — B4 FORWARD to AutoSaveDialog::Autosave (timer event 0x13).
void  Dialog_AutoSaveHandler(void* self, void* p2, int p3, int p4);

// Standalone stand-in for GameContainer_Init (exe 0x140099db0): fill the Game
// container slots the dialogs read (+0x00 achievements, +0x08 scenarios,
// +0x10 climate, +0x18 start conditions, +0x20 terrain). Idempotent.
void  GameContainer_EnsureLists();
