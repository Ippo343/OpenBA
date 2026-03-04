// engine/src/ui/moddetailsdialog.cpp
// Clean reimplementation of ModDetailsDialog methods.
//
// ModDetailsDialog (0xC8 bytes): Shows mod details with action buttons.
// Layout: +0x10 = UserInterface ptr

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot8 (0x14006D6A0): OnCommand — button dispatch.
// param3=0x12: id=0 → close, id=1 → upload/update mod, id=2 → delete mod
void ModDetailsDialog_Slot8(ModDetailsDialog* self, uint64_t param2,
                            int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 != 0x12)
        return;

    if (param4 == 0) {
        // Close dialog
        uint8_t basePtr[16];
        fn_InternalBasePtr_Ctor(basePtr, self);
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, basePtr);
    } else if (param4 == 1) {
        // Upload/update mod
        fn_ModDetailsDialog_OpenUpdateDialog(self);
    } else if (param4 == 2) {
        // Delete mod
        fn_ModDetailsDialog_DeleteMod(self);
    }
}
