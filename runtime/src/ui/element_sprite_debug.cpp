// element_sprite_debug.cpp — Debug logging wrappers for UI element sprite operations.
//
// Logs SetSprite/GetSprite calls for ImageElement and LabelElement to understand
// sprite data flow at runtime. All printf output is guarded by _DEBUG.
//
// These are standalone debug functions, not hooks. They can be called from
// reimpl wrappers to instrument sprite operations without modifying the
// original function flow.

#include <cstdint>
#include <cstdio>

#include "element_sprite_debug.h"
#include "../../types/ui/ImageElement.h"
#include "../../types/ui/LabelElement.h"
#include <new>

// ============================================================
// Sprite data interpretation helpers
// ============================================================

#ifdef _DEBUG
static void PrintSprite(const char* label, void* sheet, int32_t index, uint32_t flags) {
    printf("  %s: sheet=%p index=%d (0x%08X) flags=0x%08X\n",
           label, sheet, index, (uint32_t)index, flags);
}

static void PrintSpriteValue(const char* label, const UI::Sprite& sprite) {
    PrintSprite(label, sprite.spriteSheet, sprite.spriteIndex,
                static_cast<uint32_t>(sprite.pad0C));
}
#endif

// ============================================================
// ImageElement debug wrappers
// ============================================================

// Debug wrapper: log before/after ImageElement::SetSprite
// Call this instead of fn_ImageElement_SetSprite to get logging.
// spriteData points to a 0x10-byte Sprite struct passed by value (on stack).
// The void* receivers stay: code/Game declares these two with local externs.
void Debug_ImageElement_SetSprite(void* self, void* spriteData) {
    UI::ImageElement* elem = reinterpret_cast<UI::ImageElement*>(self);            // B.3.6 bridge  // LEFT: SIG the code/Game extern passes void* self / spriteData (debug harness; wave X)
    const UI::Sprite* sprite = reinterpret_cast<const UI::Sprite*>(spriteData);    // LEFT: code/Game's void* extern
#ifdef _DEBUG
    printf("[ImageElement::SetSprite] this=%p\n", self);
    PrintSpriteValue("  OLD", elem->_sprite);
    PrintSpriteValue("  NEW", *sprite);

    // Check dirty flag state before call
    uint32_t flagsBefore = elem->_flags;                                           // +0x34
    printf("  flags@+0x34 before: 0x%08X\n", flagsBefore);
#endif

    // Call the real member (ENDGAME B.3.6 closing pass: was the engine_funcs door)
    elem->SetSprite(sprite);

#ifdef _DEBUG
    uint32_t flagsAfter = elem->_flags;
    printf("  flags@+0x34 after:  0x%08X", flagsAfter);
    if (flagsAfter != flagsBefore) {
        printf(" (CHANGED: dirty flag 0x8000000 %s)",
               (flagsAfter & 0x8000000) ? "SET" : "CLEARED");
    }
    printf("\n");
#endif
}

// ============================================================
// LabelElement debug wrappers
// ============================================================

// Debug wrapper: log before/after LabelElement::SetSprite
// spriteData points to a 0x10-byte Sprite struct passed by value.
void Debug_LabelElement_SetSprite(void* self, void* spriteData) {
    UI::LabelElement* elem = reinterpret_cast<UI::LabelElement*>(self);            // B.3.6 bridge  // LEFT: SIG the code/Game extern passes void* self / spriteData (debug harness; wave X)
    const UI::Sprite* sprite = reinterpret_cast<const UI::Sprite*>(spriteData);    // LEFT: code/Game's void* extern
#ifdef _DEBUG
    printf("[LabelElement::SetSprite] this=%p\n", self);
    PrintSpriteValue("  OLD", elem->_sprite);
    PrintSpriteValue("  NEW", *sprite);

    uint32_t flagsBefore = elem->_flags;                                           // +0x34
    printf("  flags@+0x34 before: 0x%08X\n", flagsBefore);
#endif

    elem->SetSprite(sprite);

#ifdef _DEBUG
    uint32_t flagsAfter = elem->_flags;
    printf("  flags@+0x34 after:  0x%08X", flagsAfter);
    if (flagsAfter != flagsBefore) {
        printf(" (CHANGED: dirty flag 0x8000000 %s)",
               (flagsAfter & 0x8000000) ? "SET" : "CLEARED");
    }
    printf("\n");
#endif
}
