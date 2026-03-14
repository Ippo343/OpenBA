// sprite_debug.cpp — Debug logging wrappers for UI::Sprite functions.
//
// These are NOT hooks or trampolines. They are call-site wrappers that log
// parameters and results, then delegate to the real engine function via the
// existing function pointers in engine_funcs.h.
//
// Usage: In debug builds, call Debug_Sprite_* instead of fn_Sprite_* to get
// printf logging of sprite construction and validation calls.

#include "sprite_debug.h"
#include "../../../engine/funcs/engine_funcs.h"
#include "../../types/ui/Sprite.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Debug wrapper for UI::Sprite::Sprite(ExternalPtr<SpriteSheet>, const String&)
// Original: 0x18011b9c0
// Resolves sprite name to index via SpriteSheet::GetSpriteIndexFromName
// ============================================================
void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName) {
#ifdef _DEBUG
    printf("[Sprite::Ctor] sheet=%p name=%p retval=%p\n",
           (void*)(uintptr_t)spriteSheet, spriteName, retval);
#endif
    void* result = fn_Sprite_Ctor(retval, spriteSheet, spriteName);
#ifdef _DEBUG
    Sprite* s = (Sprite*)retval;
    printf("[Sprite::Ctor] done -> sheet=%p index=%d valid=%s\n",
           s->spriteSheet, s->spriteIndex, s->IsValid() ? "true" : "false");
#endif
    return result;
}

// ============================================================
// Debug wrapper for UI::Sprite::Sprite() — default constructor
// Original: 0x18011ba10
// Sets sheet=null, index=-1
// ============================================================
void* Debug_Sprite_DefaultCtor(void* retval) {
#ifdef _DEBUG
    printf("[Sprite::DefaultCtor] retval=%p\n", retval);
#endif
    void* result = fn_Sprite_Default_Ctor(retval);
#ifdef _DEBUG
    Sprite* s = (Sprite*)retval;
    printf("[Sprite::DefaultCtor] done -> sheet=%p index=%d\n",
           s->spriteSheet, s->spriteIndex);
#endif
    return result;
}

// ============================================================
// Debug wrapper for UI::Sprite::Sprite(ExternalPtr<SpriteSheet>, int)
// Original: 0x18011ba30
// Direct construction with known sprite index (no name lookup)
// ============================================================
void* Debug_Sprite_CtorByIndex(void* retval, void* spriteSheet, int index) {
#ifdef _DEBUG
    printf("[Sprite::CtorByIndex] sheet=%p index=%d retval=%p\n",
           spriteSheet, index, retval);
#endif
    void* result = fn_Sprite_Ctor_ByIndex(retval, spriteSheet, index);
#ifdef _DEBUG
    Sprite* s = (Sprite*)retval;
    printf("[Sprite::CtorByIndex] done -> sheet=%p index=%d valid=%s\n",
           s->spriteSheet, s->spriteIndex, s->IsValid() ? "true" : "false");
#endif
    return result;
}

// ============================================================
// Debug wrapper for UI::Sprite::IsValid()
// Original: 0x180030c40
// Returns true if index >= 0 AND sheet != null
// ============================================================
bool Debug_Sprite_IsValid(void* sprite) {
#ifdef _DEBUG
    Sprite* s = (Sprite*)sprite;
    bool result = fn_Sprite_IsValid(sprite);
    printf("[Sprite::IsValid] sprite=%p sheet=%p index=%d -> %s\n",
           sprite, s->spriteSheet, s->spriteIndex, result ? "true" : "false");
    return result;
#else
    return fn_Sprite_IsValid(sprite);
#endif
}
