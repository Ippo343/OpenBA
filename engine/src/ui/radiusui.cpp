// engine/src/ui/radiusui.cpp
// Clean reimplementation of RadiusUI game logic.
//
// RadiusUI (0x38 bytes) — displays building radius overlay.
// Extends DialogController. data30 (int32_t +0x30) = radius overlay index (-1 = none).
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// RadiusUI::Slot6 (vtable +0x30) — Remove radius overlay
// ============================================================
// When the panel is hidden/closed, removes the active radius overlay
// from the RadiusDisplayManager and resets the index to -1.
//
// Original: 0x140077600, 28 bytes.

void RadiusUI_Slot6(RadiusUI* self) {
    if (self->data30 >= 0) {
        // Game/World singleton +0x110 = RadiusDisplayManager
        void* gameWorld = *g_pGameWorldSingleton;
        void* radiusMgr = *(void**)((uintptr_t)gameWorld + 0x110);
        fn_RadiusDisplay_Remove(radiusMgr, self->data30);
    }
    self->data30 = -1;
}
