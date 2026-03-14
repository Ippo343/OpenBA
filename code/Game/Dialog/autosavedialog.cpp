#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// AutoSaveDialog::Slot6 (0x14005DFC0) — Draw
// Checks SaveLoad operation type. If operation==4 (load), shows load indicator
// and hides save indicator; otherwise shows save and hides load.
// Applies rotation to the visible indicator.
void AutoSaveDialog_Slot6(AutoSaveDialog* self) {
    uintptr_t s = (uintptr_t)self;

    if (!g_pSaveLoadSingleton || !*g_pSaveLoadSingleton)
        return;

    void* saveLoad = *g_pSaveLoadSingleton;
    int operation = fn_SaveLoad_GetOperation(saveLoad);

    // operation==4 means "load"; otherwise it's a save
    uint32_t hideFlags = (uint32_t)(operation != 4);
    uint32_t showFlags = (uint32_t)(operation == 4);

    void* elem1 = *(void**)(s + 0xB8);
    if (elem1) {
        fn_Element_SetFlagsOnOff(elem1, hideFlags, showFlags);
    }

    void* elem2 = *(void**)(s + 0xC0);
    if (elem2) {
        fn_Element_SetFlagsOnOff(elem2, hideFlags, showFlags);
    }

    // Apply rotation to first indicator element
    float angle = *(float*)(s + 0xC8);
    if (elem1) {
        fn_ImageElement_SetRotation(elem1, angle);
    }
}

// AutoSaveDialog::Slot7 (0x14005E090) — Update
// Accumulates rotation angle from elapsed time at 90 degrees/sec.
// Wraps angle to stay within [0, 360).
void AutoSaveDialog_Slot7(AutoSaveDialog* self, void* time) {
    uintptr_t s = (uintptr_t)self;
    float elapsed = fn_Time_AsSeconds(time);
    float angle = *(float*)(s + 0xC8);

    angle += elapsed * 90.0f;

    if (angle >= 360.0f) {
        do {
            angle -= 360.0f;
        } while (angle >= 360.0f);
    }

    *(float*)(s + 0xC8) = angle;
}
