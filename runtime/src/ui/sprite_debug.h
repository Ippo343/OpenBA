#pragma once
// Debug logging wrappers for UI::Sprite functions.
// Implementations in sprite_debug.cpp.

#include <cstdint>

// Wraps UI::Sprite(sheet, name) — logs sheet, name, resolved index
void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName);

// Wraps UI::Sprite() — logs creation of invalid sprite
void* Debug_Sprite_DefaultCtor(void* retval);

// Wraps UI::Sprite(sheet, index) — logs sheet + direct index
void* Debug_Sprite_CtorByIndex(void* retval, void* spriteSheet, int index);

// Wraps UI::Sprite::IsValid — logs validity check result
bool Debug_Sprite_IsValid(void* sprite);
