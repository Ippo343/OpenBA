#pragma once
// Debug logging wrappers for UI::Sprite functions.
// Implementations in sprite_debug.cpp.

#include <cstdint>

// Wraps fn_Sprite_Ctor — logs sheet, name, resolved index
void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName);

// Wraps fn_Sprite_Default_Ctor — logs creation of invalid sprite
void* Debug_Sprite_DefaultCtor(void* retval);

// Wraps fn_Sprite_Ctor_ByIndex — logs sheet + direct index
void* Debug_Sprite_CtorByIndex(void* retval, void* spriteSheet, int index);

// Wraps fn_Sprite_IsValid — logs validity check result
bool Debug_Sprite_IsValid(void* sprite);
