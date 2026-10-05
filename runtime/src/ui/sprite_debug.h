#pragma once
// Debug logging wrappers for UI::Sprite functions.
// Implementations in sprite_debug.cpp.

#include <cstdint>

namespace UI { class SpriteSheet; class Sprite; }
namespace System { class String; }

// Wraps UI::Sprite(sheet, name) — logs sheet, name, resolved index. `retval` is the
// raw storage the UI::Sprite is constructed into.
void* Debug_Sprite_Ctor(void* retval, UI::SpriteSheet* sheet, const System::String* name);

// Wraps UI::Sprite() — logs creation of invalid sprite
void* Debug_Sprite_DefaultCtor(void* retval);

// Wraps UI::Sprite::IsValid — logs validity check result
bool Debug_Sprite_IsValid(const UI::Sprite* sprite);
