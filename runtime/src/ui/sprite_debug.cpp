// sprite_debug.cpp — Debug logging wrappers for UI::Sprite functions.
//
// These are NOT hooks or trampolines. They are call-site wrappers that log
// parameters and results, then delegate to the real engine function via the
// existing function pointers in engine_funcs.h.
// ENDGAME B.3.6 closing pass: UI::Sprite is a real value type (RULE B6-10), so
// the wrappers construct / test it as C++ instead of calling the engine_funcs
// ctor doors, which are retired.
//
// Usage: In debug builds, call Debug_Sprite_* instead of fn_Sprite_* to get
// printf logging of sprite construction and validation calls.

#include "sprite_debug.h"
#include "../../types/object/External.h"
#include "../../types/ui/Sprite.h"
#include <cstdio>
#include <cstdint>
#include <new>

// ============================================================
// Debug wrapper for UI::Sprite::Sprite(ExternalPtr<SpriteSheet>, const String&)
// Original: 0x18011b9c0
// Resolves sprite name to index via SpriteSheet::GetSpriteIndexFromName
// ============================================================
void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName) {
    // The code/Game callers pass the ExternalPtr<SpriteSheet> as the raw word
    // and the System::String const& untyped (their local extern declarations).
    UI::SpriteSheet* const sheet = reinterpret_cast<UI::SpriteSheet*>(spriteSheet);  // B.3.6 bridge  // LEFT: SIG the code/Game extern passes void* (debug harness; wave X)
    const System::String* const name = static_cast<const System::String*>(spriteName);   // LEFT: H5 a System::String held as raw char* storage (class not converted this pass)
#ifdef _DEBUG
    const char* nameStr = "(null)";
    if (name) {
        const char* dataPtr = name->_data;
        if (dataPtr) nameStr = dataPtr;
    }
    printf("[Sprite::Ctor] sheet=%p name=\"%s\" retval=%p\n",
           static_cast<void*>(sheet), nameStr, retval);
#endif
    UI::Sprite* const s = new (retval) UI::Sprite(
        Object::ExternalPtr<UI::SpriteSheet>(sheet), *name);
#ifdef _DEBUG
    printf("[Sprite::Ctor] done -> sheet=%p index=%d valid=%s\n",
           s->spriteSheet, s->spriteIndex, s->IsValid() ? "true" : "false");
#endif
    return s;
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
    UI::Sprite* const s = new (retval) UI::Sprite();
#ifdef _DEBUG
    printf("[Sprite::DefaultCtor] done -> sheet=%p index=%d\n",
           s->spriteSheet, s->spriteIndex);
#endif
    return s;
}

// ============================================================
// Debug wrapper for UI::Sprite::Sprite(ExternalPtr<SpriteSheet>, int)
// Original: 0x18011ba30
// Direct construction with known sprite index (no name lookup)
// ============================================================

// ============================================================
// Debug wrapper for UI::Sprite::IsValid()
// Original: 0x180030c40
// Returns true if index >= 0 AND sheet != null
// ============================================================
bool Debug_Sprite_IsValid(void* sprite) {
    const UI::Sprite* const s = reinterpret_cast<const UI::Sprite*>(sprite);  // B.3.6 bridge  // LEFT: SIG the code/Game extern passes void* (debug harness; wave X)
#ifdef _DEBUG
    bool result = s->IsValid();
    printf("[Sprite::IsValid] sprite=%p sheet=%p index=%d -> %s\n",
           sprite, s->spriteSheet, s->spriteIndex, result ? "true" : "false");
    return result;
#else
    return s->IsValid();
#endif
}
