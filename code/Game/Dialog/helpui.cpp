// engine/src/ui/helpui.cpp
// Clean reimplementation of HelpUI game logic.
//
// HelpUI (0x78 bytes) — help page navigation panel.
// Extends DialogController. Fields:
//   +0x48 prevButton, +0x50 nextButton (InternalBasePtr)
//   +0x64 pageIndex (int32_t), +0x70 pageCount (int32_t)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// HelpUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables/disables prev/next buttons based on current page index.
//
// Original: 0x14006A230.

void HelpUI_Slot7(HelpUI* self) {
    int pageIndex = *(int*)((uintptr_t)self + 0x64);
    int pageCount = *(int*)((uintptr_t)self + 0x70);

    // Enable prev button if not on first page
    void* prevBtn = (void*)self->data48;
    void** vtable1 = *(void***)prevBtn;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable1[8])(prevBtn, pageIndex > 0);

    // Enable next button if not on last page
    void* nextBtn = (void*)self->data50;
    void** vtable2 = *(void***)nextBtn;
    ((fn_SetEnabled_t)vtable2[8])(nextBtn, pageIndex < pageCount - 1);
}
