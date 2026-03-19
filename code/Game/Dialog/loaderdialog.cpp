#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// LoaderDialog::Slot4 (0x1400B2D90) — vtable +0x20, Setup
// ============================================================
// Finds "loadingImage" element and stores pointer at +0xB8.
//
// Original: 0x1400B2D90.

void LoaderDialog_Slot4(LoaderDialog* self) {
    uintptr_t s = (uintptr_t)self;

    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "loadingImage");
    uint8_t retBuf[16];
    fn_DialogInstance_FindElement(self, retBuf, nameStr);
    *(void**)(s + 0xB8) = *(void**)retBuf;
    fn_String_Dtor(nameStr);
}

// LoaderDialog::Slot6 (0x1400B2D00) — Draw
// Applies the current rotation angle to the loading spinner image element.
void LoaderDialog_Slot6(LoaderDialog* self) {
    uintptr_t s = (uintptr_t)self;
    void* imageElem = *(void**)(s + 0xB8);

    if (imageElem) {
        float angle = *(float*)(s + 0xC0);
        fn_ImageElement_SetRotation(imageElem, angle);
    }
}

// LoaderDialog::Slot7 (0x1400B2D50) — Update
// Accumulates rotation angle from elapsed time at 180 degrees/sec.
void LoaderDialog_Slot7(LoaderDialog* self, void* time) {
    uintptr_t s = (uintptr_t)self;
    float elapsed = fn_Time_AsSeconds(time);
    *(float*)(s + 0xC0) += elapsed * 180.0f;
}
