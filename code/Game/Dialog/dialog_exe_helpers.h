#pragma once
// Declarations for Dialog-module exe-helper reimplementations.
// See code/Game/Dialog/dialog_exe_helpers.cpp.
#include <cstdint>

// FUN_14000d6c0 — FindComponent (type 0x1401c7008). void* fn(entity, retIBP).
void* Dialog_FindComponent_d6c0(void* entity, void* retIBP);

// FUN_14005dec0 — AutoSaveDialog autosave handler (timer event 0x13).
void  Dialog_AutoSaveHandler(void* self, void* p2, int p3, int p4);

// Standalone stand-in for GameContainer_Init (exe 0x140099db0): fill the Game
// container slots the dialogs read (+0x00 achievements, +0x08 scenarios,
// +0x10 climate, +0x18 start conditions, +0x20 terrain). Idempotent.
void  GameContainer_EnsureLists();
