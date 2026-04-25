#pragma once
// code/Game/Game/game_main.h
// Reimpl of the original exe's GameMain() entry function.
//
// Original: 0x140041ef0 in Application-x64.exe.
// Drives Engine ctor/Startup, MainLoop ctor/Run/dtor, and the
// boot UpdateLoop chain (ShiningRock splash -> Loading dialog ->
// MenuScene loader). Called from standalone/main.cpp WinMain.

#include <windows.h>

namespace Game {

// Returns 0 on clean exit, non-zero on error.
int RunGameMain(HINSTANCE hInstance, const char* cmdLine);

} // namespace Game
