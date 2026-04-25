#pragma once
// code/Game/Game/game.h
// Reimpl of the exe-internal "Game" KeyMap container struct.
//
// Original: FUN_140001050 in Application-x64.exe (Game ctor).
// 0xe0 bytes — zeroed memory, two embedded UI::Sprites, then publishes the
// instance pointer to g_pKeyMapSingleton (DAT_1401c3de8). Used as the global
// key-binding registry.

#include <cstdint>

extern "C" {

// Game_Ctor(self) — initializes the 0xe0-byte Game struct in place.
// Maps to FUN_140001050 in the exe (registered at 0x140001050).
// Caller is responsible for the 0xe0-byte allocation.
void* Game_Ctor(void* self);

} // extern "C"
