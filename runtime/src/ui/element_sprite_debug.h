#pragma once
// Debug logging wrappers for UI element sprite operations.
// Implementations in element_sprite_debug.cpp.
namespace UI { class ImageElement; class LabelElement; }

// Wraps fn_ImageElement_SetSprite — logs old/new sprite, dirty flags
void Debug_ImageElement_SetSprite(UI::ImageElement* self, void* spriteData);

// Wraps fn_LabelElement_SetSprite — logs old/new sprite, dirty flags
void Debug_LabelElement_SetSprite(UI::LabelElement* self, void* spriteData);
