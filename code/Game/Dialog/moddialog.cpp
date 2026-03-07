// code/Game/Dialog/moddialog.cpp
// Clean reimplementation of ModDialog vtable methods.
//
// ModDialog (0xD8 bytes), vtable at 0x14015c870.
// Extends DialogInstance (0xB8 base).
//
// Layout:
//   +0xB8: dynamic array ptr (mod entry cache, stride 0x58)
//   +0xC0: array count
//   +0xC8: table element (InternalPtr)
//   +0xD0: zeroed
//
// Reimplemented:
//   Slot8 (0x14006ED40) — OnCommand: handles all mod list button clicks
//
// Not reimplemented (too complex / heavy engine deps):
//   Slot4 (0x14006F150) — Setup: 428 lines, ImageBuffer/MaterialInstance/SpriteSheet pipeline
//
// Not hookable (Runtime thunks):
//   Slot7, Slot9 — 6-byte JMP [rip+xx] to Runtime, cannot be hooked

#include "ui/Dialogs.h"
#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// ModDialog::Slot8 — OnCommand handler (0x14006ED40)
// ============================================================
//
// Dispatches button clicks by element ID ranges:
//   100         — Cancel: refresh mod list, close dialog
//   0x65 (101)  — OK/Apply: save changes, reload if needed, close
//   1000-1999   — Toggle mod enabled/disabled (index = id - 1000)
//   2000-2999   — Move mod up (swap index, index-1)
//   3000-3999   — Move mod down (swap index, index+1)
//   4000-4999   — Move mod to top of list
//   5000-5999   — Show mod details dialog
//
// Always calls base DialogInstance::OnElementEvent at the end.

void ModDialog_Slot8(ModDialog* self, void* param2, int msg, int id,
                     uint32_t p5, uint32_t p6, uint32_t p7, uint64_t p8) {
    uintptr_t s = (uintptr_t)self;

    if (msg == 0x12) {  // Button click event
        void* modSingleton = *(void**)g_ppModificationSingleton;

        if (id == 100) {
            // Cancel — refresh list and close
            fn_Modification_RefreshList(modSingleton);

            // Build InternalBasePtr for CloseDialog
            void* dialogPtr = nullptr;
            fn_InternalBasePtr_Ctor(&dialogPtr, (void*)self);

            void* ui = *(void**)(s + 0x10);
            fn_UI_CloseDialog(ui, &dialogPtr);
            goto done;
        }

        if (id == 0x65) {
            // OK/Apply — delegate to complex apply handler
            fn_ModDialog_Apply((void*)self);
            goto done;
        }

        if (id >= 1000 && id < 2000) {
            // Toggle mod checkbox
            fn_ModDialog_ToggleMod((void*)self, id - 1000);
            goto done;
        }

        if (id >= 2000 && id < 3000) {
            // Move up: swap(id-2000, id-2001)
            fn_Modification_Swap(modSingleton, id - 2000, id - 2001);
            fn_ModDialog_RefreshDisplay((void*)self);
            goto done;
        }

        if (id >= 3000 && id < 4000) {
            // Move down: swap(id-3000, id-2999)
            fn_Modification_Swap(modSingleton, id - 3000, id - 2999);
            fn_ModDialog_RefreshDisplay((void*)self);
            goto done;
        }

        if (id >= 4000 && id < 5000) {
            // Move to top
            fn_ModDialog_MoveToTop((void*)self, id - 4000);
            goto done;
        }

        if (id >= 5000 && id < 6000) {
            // Show details
            fn_ModDialog_ShowDetails((void*)self, id - 5000);
        }
    }

done:
    // Always call base handler
    // Need to pass element pointer from param2
    void* elementPtr = *(void**)param2;
    fn_DialogInstance_OnElementEvent((void*)self, &elementPtr,
                                     msg, id, p5, p6, p7, p8);
}
