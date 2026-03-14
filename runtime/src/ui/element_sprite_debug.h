#pragma once
// Debug logging wrappers for UI element sprite operations.
// Implementations in element_sprite_debug.cpp.

// Wraps fn_ImageElement_SetSprite — logs old/new sprite, dirty flags
void Debug_ImageElement_SetSprite(void* self, void* spriteData);

// Wraps fn_ImageElement_GetSprite — logs returned sprite
void Debug_ImageElement_GetSprite(void* self, void* retval);

// Wraps fn_LabelElement_SetSprite — logs old/new sprite, dirty flags
void Debug_LabelElement_SetSprite(void* self, void* spriteData);

// Wraps fn_LabelElement_GetSprite — logs returned sprite
void Debug_LabelElement_GetSprite(void* self, void* retval);
