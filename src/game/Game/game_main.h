#pragma once
// src/game/Game/game_main.h
// Reimpl of the original exe's GameMain() entry function.
//
// Original: 0x140041ef0 in Application-x64.exe.
// Drives Engine ctor/Startup, MainLoop ctor/Run/dtor, and the
// boot UpdateLoop chain (ShiningRock splash -> Loading dialog ->
// MenuScene loader). Called from src/app/main.cpp: WinMain on Windows, main
// on POSIX (Phase D.4 LINUX-1).
//
// No OS header: the platform handle is the Engine ctor's own `void*`
// (??0Engine@0@QEAA@PEAX@Z) -- the HINSTANCE WinMain received on Windows,
// null on POSIX (Engine::Engine stores it for Application::PlatformStartup,
// which SDL owns since Phase D.4 SDL W3).

namespace Game {

// Returns 0 on clean exit, non-zero on error.
int RunGameMain(void* platformHandle, const char* cmdLine);

} // namespace Game
