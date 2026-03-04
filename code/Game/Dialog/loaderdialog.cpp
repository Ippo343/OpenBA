#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

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
