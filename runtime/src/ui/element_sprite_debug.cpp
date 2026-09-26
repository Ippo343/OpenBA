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
#include "../../../engine/funcs/engine_funcs.h"

// ============================================================
// Sprite data interpretation helpers
// ============================================================

#ifdef _DEBUG
static void PrintSprite(const char* label, void* sheet, int32_t index, uint32_t flags) {
    printf("  %s: sheet=%p index=%d (0x%08X) flags=0x%08X\n",
           label, sheet, index, (uint32_t)index, flags);
}

static void PrintSpriteFromPtr(const char* label, const uint8_t* spriteData) {
    void*    sheet = *(void**)spriteData;
    int32_t  index = *(int32_t*)(spriteData + 8);
    uint32_t flags = *(uint32_t*)(spriteData + 12);
    PrintSprite(label, sheet, index, flags);
}
#endif

// ============================================================
// ImageElement debug wrappers
// ============================================================

// Debug wrapper: log before/after ImageElement::SetSprite
// Call this instead of fn_ImageElement_SetSprite to get logging.
// spriteData points to a 0x10-byte Sprite struct passed by value (on stack).
void Debug_ImageElement_SetSprite(void* self, void* spriteData) {
#ifdef _DEBUG
    ImageElement* elem = reinterpret_cast<ImageElement*>(self);
    const uint8_t* newSprite = reinterpret_cast<const uint8_t*>(spriteData);

    printf("[ImageElement::SetSprite] this=%p\n", self);
    PrintSprite("  OLD", elem->spriteSheet, elem->spriteIndex, elem->spriteFlags);
    PrintSpriteFromPtr("  NEW", newSprite);

    // Check dirty flag state before call
    uint32_t flagsBefore = *(uint32_t*)((uint8_t*)self + 0x34);
    printf("  flags@+0x34 before: 0x%08X\n", flagsBefore);
#endif

    // Call the real function
    if (fn_ImageElement_SetSprite) {
        fn_ImageElement_SetSprite(self, spriteData);
    }

#ifdef _DEBUG
    uint32_t flagsAfter = *(uint32_t*)((uint8_t*)self + 0x34);
    printf("  flags@+0x34 after:  0x%08X", flagsAfter);
    if (flagsAfter != flagsBefore) {
        printf(" (CHANGED: dirty flag 0x8000000 %s)",
               (flagsAfter & 0x8000000) ? "SET" : "CLEARED");
    }
    printf("\n");
#endif
}

// Debug wrapper: log ImageElement::GetSprite result
// retval points to a 0x10-byte buffer that receives the Sprite.
// Uses hidden return pointer ABI: RCX=this, RDX=retval.
void Debug_ImageElement_GetSprite(void* self, void* retval) {
#ifdef _DEBUG
    printf("[ImageElement::GetSprite] this=%p\n", self);
#endif

    if (fn_ImageElement_GetSprite) {
        fn_ImageElement_GetSprite(self, retval);
    }

#ifdef _DEBUG
    PrintSpriteFromPtr("  RESULT", reinterpret_cast<const uint8_t*>(retval));
#endif
}

// ============================================================
// LabelElement debug wrappers
// ============================================================

// Debug wrapper: log before/after LabelElement::SetSprite
// spriteData points to a 0x10-byte Sprite struct passed by value.
void Debug_LabelElement_SetSprite(void* self, void* spriteData) {
#ifdef _DEBUG
    RtUI::LabelElement* elem = reinterpret_cast<RtUI::LabelElement*>(self);
    const uint8_t* newSprite = reinterpret_cast<const uint8_t*>(spriteData);

    printf("[LabelElement::SetSprite] this=%p\n", self);
    PrintSprite("  OLD", elem->spriteSheet, elem->spriteIndex, elem->spriteFlags);
    PrintSpriteFromPtr("  NEW", newSprite);

    uint32_t flagsBefore = *(uint32_t*)((uint8_t*)self + 0x34);
    printf("  flags@+0x34 before: 0x%08X\n", flagsBefore);
#endif

    if (fn_LabelElement_SetSprite) {
        fn_LabelElement_SetSprite(self, spriteData);
    }

#ifdef _DEBUG
    uint32_t flagsAfter = *(uint32_t*)((uint8_t*)self + 0x34);
    printf("  flags@+0x34 after:  0x%08X", flagsAfter);
    if (flagsAfter != flagsBefore) {
        printf(" (CHANGED: dirty flag 0x8000000 %s)",
               (flagsAfter & 0x8000000) ? "SET" : "CLEARED");
    }
    printf("\n");
#endif
}

// Debug wrapper: log LabelElement::GetSprite result
// Uses fn_LabelElement_GetSprite when available, falls back to direct struct read.
void Debug_LabelElement_GetSprite(void* self, void* retval) {
#ifdef _DEBUG
    printf("[LabelElement::GetSprite] this=%p\n", self);
#endif

    if (fn_LabelElement_GetSprite) {
        fn_LabelElement_GetSprite(self, retval);
    } else {
        // Fallback: read directly from struct at +0x100/+0x108
        RtUI::LabelElement* elem = reinterpret_cast<RtUI::LabelElement*>(self);
        uint64_t* out = reinterpret_cast<uint64_t*>(retval);
        out[0] = reinterpret_cast<uint64_t>(elem->spriteSheet);
        uint64_t second = 0;
        second |= (uint32_t)elem->spriteIndex;
        second |= ((uint64_t)elem->spriteFlags) << 32;
        out[1] = second;
    }

#ifdef _DEBUG
    PrintSpriteFromPtr("  RESULT", reinterpret_cast<const uint8_t*>(retval));
#endif
}
