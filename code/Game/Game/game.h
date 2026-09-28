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

// The exe cell 0x1401c3de8 (B3R STORAGE): the live 0xe0-byte Game container
// (scenario list +0x08, cursors +0x38, UI sounds +0x80..+0x98, sprites
// +0xa8/+0xb8, key-name StringTable +0xd0). Game_Ctor (0x140001050, the cell's
// only retail writer at 0x1400010f1) publishes it. Not a class in the tree, so a
// namespace-scope pointer; was the file-static g_storage_KeyMapSingleton of
// standalone/address_registry.cpp, read through g_pKeyMapSingleton ("container
// / manager object, NOT the KeyMap"). Defined in game.cpp.
extern void* g_gameContainer;
