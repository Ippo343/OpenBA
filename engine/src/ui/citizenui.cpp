// engine/src/ui/citizenui.cpp
// Clean reimplementation of CitizenUI panel methods.
//
// CitizenUI (0xE8 bytes): Citizen info panel.
// Layout: +0xE0 = sub-dialog InternalBasePtr (follow-dialog reference)

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot6: Close sub-dialog if open, then clear the reference.
void CitizenUI_Slot6(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;

    uint64_t subDialog = *(uint64_t*)(s + 0xE0);
    if (subDialog != 0) {
        // Get the UserInterface from DialogInstance (+0x18) -> +0x10
        void* dialogInst = (void*)*(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);
        fn_UI_CloseDialog(userInterface, (void*)(s + 0xE0));
    }

    // Reset +0xE0 to null InternalBasePtr
    uint64_t nullPtr = 0;
    fn_InternalBasePtr_Ctor(&nullPtr, nullptr);
    *(uint64_t*)(s + 0xE0) = nullPtr;
}
