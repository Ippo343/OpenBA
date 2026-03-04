#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// MessageDialog::Slot4 (0x14006D140) — Setup
// Finds buttonCancel, buttonOk, buttonYes, buttonNo elements by name.
// Sets element IDs: Cancel=0, Ok=1, Yes=2, No=3.
void MessageDialog_Slot4(MessageDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find and ID each button element
    struct { const char* name; int id; } buttons[] = {
        { "buttonCancel", 0 },
        { "buttonOk",     1 },
        { "buttonYes",    2 },
        { "buttonNo",     3 },
    };

    for (auto& btn : buttons) {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, btn.name);
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (elem) {
            fn_Element_SetElementId(elem, btn.id);
        }
    }
}
