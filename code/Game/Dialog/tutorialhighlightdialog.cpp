#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// TutorialHighlightDialog::Startup (0x1400E0020)
// Finds "glowGroup", "glow", "arrow" elements by name and stores them
// at +0xC0, +0xC8, +0xD0 respectively.
void TutorialHighlightDialog_Startup(TutorialHighlightDialog* self) {
    uintptr_t s = (uintptr_t)self;

    struct { const char* name; uintptr_t offset; } elements[] = {
        { "glowGroup", 0xC0 },
        { "glow",      0xC8 },
        { "arrow",     0xD0 },
    };

    for (auto& e : elements) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, e.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        fn_String_Dtor(nameStr);

        // Store the found element pointer at the target offset
        void* elem = *(void**)retBuf;
        *(void**)(s + e.offset) = elem;
    }
}
