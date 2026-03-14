#pragma once
// Debug logging wrappers for UI::SpriteSheet functions.
// Implementations in spritesheet_debug.cpp.

// Wraps GetSpriteIndexFromName — logs name, result, highlights failures
int SpriteSheet_Debug_GetSpriteIndexFromName(void* sheet, void* name);

// Wraps GetSpriteCount — logs sheet info
int SpriteSheet_Debug_GetSpriteCount(void* sheet);

// Dumps all sprite names in a sheet (up to 200). Debug builds only.
#ifdef _DEBUG
void SpriteSheet_Debug_DumpAllSprites(void* sheet);
#endif

// Validates function pointer resolution
void SpriteSheet_Debug_Init();
