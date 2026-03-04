// engine/src/ui/mainmenudialog.cpp
// Clean reimplementation of MainMenuDialog methods.
//
// MainMenuDialog (0xC8 bytes): Main menu screen.
// Layout: +0xB8 = flag (non-zero when game is loaded/active)

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot5 (0x1400B3C60): Shutdown — unpause audio scene channels 2, 3, 4.
// Called when the menu closes (returning to gameplay).
void MainMenuDialog_Slot5(MainMenuDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Only unpause if a game is active (flag at +0xB8 != 0)
    if (*(uint64_t*)(s + 0xB8) == 0)
        return;

    void* world = *g_pGameWorldSingleton;
    void* audioScene = *(void**)((uintptr_t)world + 0x78);

    fn_Audio_Scene_Pause(audioScene, 4, false);
    fn_Audio_Scene_Pause(audioScene, 3, false);
    fn_Audio_Scene_Pause(audioScene, 2, false);
}
