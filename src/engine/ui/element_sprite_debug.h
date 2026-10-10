#pragma once
// Debug logging wrappers for UI element sprite operations.
// Implementations in element_sprite_debug.cpp.
namespace UI { class ImageElement; class LabelElement; class Sprite; }

// Wraps fn_ImageElement_SetSprite — logs old/new sprite, dirty flags
void Debug_ImageElement_SetSprite(UI::ImageElement* self, const UI::Sprite* sprite);

// Wraps fn_LabelElement_SetSprite — logs old/new sprite, dirty flags
void Debug_LabelElement_SetSprite(UI::LabelElement* self, const UI::Sprite* sprite);
