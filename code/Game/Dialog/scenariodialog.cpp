// engine/src/ui/scenariodialog.cpp
// Clean reimplementation of ScenarioDialog methods.
//
// ScenarioDialog (0xE8 bytes): Scenario selection screen.
// Layout: +0xC8 = selected index (int32), +0xD8 = scenario array,
//         +0xE0 = scenario count

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// ScenarioDialog::Slot6 (vtable +0x30) — Update entry visibility
// ============================================================
// Iterates scenario entries at +0xD8 (stride 0x38), shows entry matching
// selected index at +0xC8, hides all others. Enables/disables confirm
// button at +0xD0 based on whether a valid selection exists.
//
// Original: 0x140077BE0.

void ScenarioDialog_Slot6(ScenarioDialog* self) {
    uintptr_t s = (uintptr_t)self;

    int count = *(int*)(s + 0xE0);
    int selected = *(int*)(s + 0xC8);
    uintptr_t entryArray = *(uintptr_t*)(s + 0xD8);

    // Show selected entry, hide all others
    for (int i = 0; i < count; i++) {
        void* elem = *(void**)(entryArray + 8 + (uintptr_t)i * 0x38);
        bool isNotSelected = (i != selected);
        fn_Element_SetFlagsOnOff(elem, isNotSelected ? 1 : 0, isNotSelected ? 0 : 1);
    }

    // Enable/disable confirm button based on valid selection
    void* confirmBtn = (void*)*(uint64_t*)(s + 0xD0);
    void** vtable = *(void***)confirmBtn;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable[8])(confirmBtn, selected >= 0);
}

// ============================================================
// ScenarioDialog::Slot8 (vtable +0x40) — OnCommand
// ============================================================
// Slot8 (0x140077F80): OnCommand — selection change and button dispatch.
// param3=2 (selection change): updates selected index from button IDs 1000+
// param3=0x12 (button click): id=100 → close, id=101 → load selected scenario
void ScenarioDialog_Slot8(ScenarioDialog* self, uint64_t param2,
                          int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 2) {
        // Selection change — button IDs are 1000 + index
        int maxIndex = *(int*)(s + 0xE0);
        if (param4 > 999 && param4 < maxIndex + 1000) {
            *(int*)(s + 0xC8) = param4 - 1000;
        }
    } else if (param3 == 0x12) {
        if (param4 == 100) {
            // Close button
            uint8_t basePtr[16];
            fn_InternalBasePtr_Ctor(basePtr, self);
            void* ui = *(void**)(s + 0x10);
            fn_UI_CloseDialog(ui, basePtr);
        } else if (param4 == 0x65) {
            // Confirm/load button
            fn_ScenarioDialog_LoadSelected(self);
        }
    }
}
