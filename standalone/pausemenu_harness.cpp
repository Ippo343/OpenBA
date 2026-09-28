// standalone/pausemenu_harness.cpp
// Unattended driver for the IN-GAME pause menu, and specifically for its Quit
// Game button.
//
// The front-end menu's Quit is already reachable unattended (BANISHED_AUTOQUIT
// injects the click on MainMenuDialog before a game exists). The in-game one is
// not: it needs a running world, the toolbar's menu button (ToolbarDialog action
// type 3 -> EntityUI_OpenPauseMenu), and then a click on the Pause.rsc dialog
// that appears. This harness drives that whole chain from GameUpdateLoop::Update
// so a run can prove it end to end with no human at the mouse.
//
//   BANISHED_PAUSEMENU=<frame>     open the in-game pause menu on this frame
//                                  (unset / 0 = disabled). Clamped to >= 600 so
//                                  the toolbar and the world have settled.
//   BANISHED_PAUSEMENU_QUIT=<n>    n frames after the menu opens, click a button
//                                  on it. Default 120. 0 = open only, and then
//                                  report every 120 frames whether it is still up.
//   BANISHED_PAUSEMENU_BUTTON=<id> which button to click. Default 6 = Quit Game.
//   BANISHED_PAUSEMENU_CLICKS=<tok>[,<tok>...]  follow-up clicks, one every 60
//                                  frames after the button click, delivered to the
//                                  most recently opened ModDialog (<id>) or
//                                  ModDetailsDialog (d<id>) -- the in-game half of
//                                  autostart.cpp's BANISHED_AUTOMENU_CLICKS. The
//                                  harness disarms itself after the last token so
//                                  a world reload (mods Ok) does not re-run it.
//                                  Extra tokens: w = wait one interval, p = dump the
//                                  IO::Modification/FileSystem state ([ModProbe:clicks]).
//                                  1 = Resume, 3 = Save, 5 = Quit to Main Menu.
//
// The dialog it drives is a MainMenuDialog (the same class as the front end),
// built from Dialog/Pause.rsc with +0xB8 = gameWorld. We dispatch vtable slot 8
// (OnCommand) with msg=0x12 and the button's element id -- the same two values
// UI::UserInterface sends on a real click, which is how autostart.cpp already
// drives the front-end menu.
#include "resolve.h"
#include "engine_funcs.h"
#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)

// code/Game/Dialog/mod_dialog_vtables.cpp -- BANISHED_PAUSEMENU_CLICKS targets
extern "C" void* ModDialog_LastOpened();
extern "C" void* ModDetailsDialog_LastOpened();

extern "C" void* EntityUI_OpenPauseMenu(void* entityUI);  // code/Game/Dialog/toolbardialog.cpp

namespace {

int   s_openFrame  = -1;   // -1 = env not read yet, 0 = disabled
int   s_quitDelay  = 120;
int   s_buttonId   = 6;    // kButton_Quit
bool  s_opened     = false;
bool  s_clicked    = false;
int   s_openedAt   = 0;
int   s_clickedAt  = 0;
void* s_forWorld   = nullptr;  // the GameUpdateLoop this arming belongs to
int   s_worldSeenAt = 0;       // frame we first saw that GameUpdateLoop
void* s_dialog     = nullptr;  // the pause menu the opener handed back
void* s_vtable     = nullptr;  // its vtable at open time (liveness check before we click)
char  s_clicks[256] = {0};     // BANISHED_PAUSEMENU_CLICKS, consumed in place
char* s_clickCursor = nullptr;
int   s_lastTokenAt = 0;
bool  s_feeding     = false;   // token feed in progress: survives a world reload

void Init() {
    if (s_openFrame >= 0) return;
    s_openFrame = Opt().pausemenu;
    s_quitDelay = Opt().pausemenuQuit;
    s_buttonId  = Opt().pausemenuButton;
    if (const char* c = Opt().pausemenuClicks) {
        std::strncpy(s_clicks, c, sizeof(s_clicks) - 1);
        s_clickCursor = s_clicks[0] ? s_clicks : nullptr;
    }
    if (s_openFrame > 0) {
        std::fprintf(stderr, "[PauseMenu] ENABLED - open at frame %d, click id=%d %d frames later\n",
                     s_openFrame, s_buttonId, s_quitDelay);
    }
}

// UI::UserInterface::PausesGame -- true while a dialog that stops the simulation
// is up. This is the honest "is the pause menu really on screen" check: the raw
// +0x28 dialog array does NOT hold the menu for its whole life (it read 0 entries
// a hundred frames after the menu opened), but the engine's own answer cannot be
// fooled -- GameUpdateLoop::Update reads exactly this to decide whether to tick
// the world.

// Where `dlg` sits in UserInterface's dialog array (data +0x28, count +0x30) --
// the array ToolbarUtility_HarnessDriveController walks. Diagnostic only; the
// harness addresses the dialog by the pointer the opener returned.
int DialogListIndex(void* ui, void* dlg, int* outCount) {
    *outCount = 0;
    if (!ui) return -1;
    uintptr_t uiAddr = reinterpret_cast<uintptr_t>(ui);
    uintptr_t* data  = *reinterpret_cast<uintptr_t**>(uiAddr + 0x28);
    int count        = *reinterpret_cast<int*>(uiAddr + 0x30);
    *outCount = count;
    for (int i = 0; data && i < count && i < 64; ++i) {
        uintptr_t ip = data[i];
        if (reinterpret_cast<void*>(ip) == dlg) return i;
    }
    return -1;
}

int Paused(void* ui) { return (int)reinterpret_cast<UI::UserInterface*>(ui)->PausesGame(); }  // B.3.6 bridge

}  // namespace

extern "C" void PauseMenuHarness_Tick(void* entityUI, int frameNum) {
    Init();
    if (s_openFrame <= 0) return;

    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) return;
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0x98);

    // Rearm for each new game. A quit-to-menu followed by another New Game builds a
    // fresh GameUpdateLoop, and BANISHED_AUTOSTART_GAMES=n drives n of them, so the
    // one-shot state has to be per-WORLD rather than per-process. Frames are counted
    // from when this world first appeared: GameUpdateLoop's s_frameNum is a process
    // counter that never resets, so an absolute "open at frame 800" would fire
    // instantly on game two.
    if (s_feeding) {
        if (s_clickCursor && frameNum >= s_lastTokenAt + 60) {
            s_lastTokenAt = frameNum;
            char* tok = s_clickCursor;
            char* comma = std::strchr(tok, ',');
            if (comma) { *comma = '\0'; s_clickCursor = comma + 1; }
            else       { s_clickCursor = nullptr; }
            if (tok[0] == 'w') {
                std::fprintf(stderr, "[PauseMenu] CLICKS: wait (frame %d)\n", frameNum);
            } else if (tok[0] == 'd') {
                void* dlg = ModDetailsDialog_LastOpened();
                int id = std::atoi(tok + 1);
                std::fprintf(stderr, "[PauseMenu] CLICKS: ModDetailsDialog=%p id=%d\n", dlg, id);
                if (dlg) {
                    // UI::DialogInstance::OnElementEvent (retail slot 8, +0x40).
                    // The trailing ints / userData were never set by the old raw
                    // 4-argument call; the by-name call passes 0 / nullptr. The
                    // four trailing arguments are the harness's choice: retail
                    // has no such caller.
                    static_cast<UI::DialogInstance*>(dlg)->OnElementEvent(   // B.3.x bridge
                        nullptr, 0x12, id, 0, 0, 0, nullptr);
                }
            } else {
                void* dlg = ModDialog_LastOpened();
                int id = std::atoi(tok);
                std::fprintf(stderr, "[PauseMenu] CLICKS: ModDialog=%p id=%d\n", dlg, id);
                if (dlg) {
                    // The four trailing arguments (0 / 0 / 0 / nullptr, as the
                    // old raw call already passed) are the harness's choice:
                    // retail has no such caller.
                    void* nullElem = nullptr;
                    static_cast<UI::DialogInstance*>(dlg)->OnElementEvent(   // B.3.x bridge
                        reinterpret_cast<Object::InternalPtr<UI::Element>*>(&nullElem),
                        0x12, id, 0, 0, 0, nullptr);
                }
            }
            if (!s_clickCursor) {
                std::fprintf(stderr, "[PauseMenu] CLICKS: sequence done - harness disarmed\n");
                s_feeding   = false;
                s_openFrame = 0;   // never rearm, even if the click reloaded the world
            }
        }
        return;
    }

    if (gameWorld != s_forWorld) {
        s_forWorld    = gameWorld;
        s_worldSeenAt = frameNum;
        s_opened      = false;
        s_clicked     = false;
        s_dialog      = nullptr;
        s_vtable      = nullptr;
        s_clickedAt   = 0;
        std::fprintf(stderr, "[PauseMenu] new world %p at frame %d - rearmed\n",
                     gameWorld, frameNum);
    }

    if (s_clicked) {
        // Aftermath, once. For a non-Quit button this is the real proof the menu
        // is live rather than merely present: Resume (id 1) must take PausesGame
        // back to 0, i.e. the simulation starts ticking again.
        if (s_clickedAt && frameNum == s_clickedAt + 120) {
            std::fprintf(stderr, "[PauseMenu] frame %d - 120 frames after the id=%d click: "
                                 "pausesGame=%d\n", frameNum, s_buttonId, Paused(ui));
        }
        // Hand the follow-up clicks to the feeder at the top of this function,
        // which keeps running across the world reload an in-game mods Ok triggers.
        if (s_clickCursor && s_clickedAt && !s_feeding) {
            s_feeding     = true;
            s_lastTokenAt = s_clickedAt;
        }
        return;
    }

    if (!s_opened) {
        if (frameNum - s_worldSeenAt < s_openFrame) return;
        // Retail passes gameWorld+0x120 (the EntityUI). GameUpdateLoop hands us the
        // same pointer as m_entityUI; fall back to the world field if it is null.
        void* eui = entityUI ? entityUI
                             : *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0x120);
        std::fprintf(stderr, "[PauseMenu] frame %d - opening (entityUI=%p ui=%p pausesGame=%d)\n",
                     frameNum, eui, ui, Paused(ui));
        s_dialog   = EntityUI_OpenPauseMenu(eui);
        s_opened   = true;
        s_openedAt = frameNum;
        if (!s_dialog) {
            std::fprintf(stderr, "[PauseMenu] OPEN FAILED - no dialog was built "
                                 "(Pause.rsc load or MemAlloc failed)\n");
            s_clicked = true;
            return;
        }
        s_vtable = *reinterpret_cast<void**>(s_dialog);
        int cnt = 0;
        int idx = DialogListIndex(ui, s_dialog, &cnt);
        std::fprintf(stderr, "[PauseMenu] pause dialog=%p vtable=%p (UI array index=%d of %d) "
                             "pausesGame=%d\n",
                     s_dialog, s_vtable, idx, cnt, Paused(ui));
        return;
    }

    if (s_quitDelay <= 0) {
        // Open-only mode: report periodically so a run left on screen shows whether
        // the menu holds and keeps the simulation paused.
        if ((frameNum - s_openedAt) % 120 == 0) {
            int c = 0;
            int i = DialogListIndex(ui, s_dialog, &c);
            std::fprintf(stderr, "[PauseMenu] frame %d - still up? UI array index=%d of %d "
                                 "vtableIntact=%d pausesGame=%d\n",
                         frameNum, i, c,
                         (int)(*reinterpret_cast<void**>(s_dialog) == s_vtable), Paused(ui));
        }
        return;
    }
    if (frameNum < s_openedAt + s_quitDelay) return;

    int cnt = 0;
    int idx = DialogListIndex(ui, s_dialog, &cnt);
    void* nowVtable = *reinterpret_cast<void**>(s_dialog);
    std::fprintf(stderr, "[PauseMenu] frame %d - dialog=%p vtable=%p (was %p) "
                         "UI array index=%d of %d pausesGame=%d\n",
                 frameNum, s_dialog, nowVtable, s_vtable, idx, cnt, Paused(ui));
    s_clicked = true;
    if (nowVtable != s_vtable) {
        std::fprintf(stderr, "[PauseMenu] click aborted - the dialog's vtable changed, "
                             "so the object was destroyed under us\n");
        return;
    }
    // The vptr is intact (whole-vptr compare above), so the object is still the
    // MainMenuDialog the opener built: UI::DialogInstance::OnElementEvent, by
    // name (retail slot 8, +0x40). The trailing ints / userData were never set
    // by the old raw 4-argument call; the by-name call passes 0 / nullptr. The
    // four trailing arguments are the harness's choice: retail has no such caller.
    std::fprintf(stderr, "[PauseMenu] dispatching OnCommand(msg=0x12, id=%d)\n", s_buttonId);
    static_cast<UI::DialogInstance*>(s_dialog)->OnElementEvent(   // B.3.x bridge
        nullptr, 0x12, s_buttonId, 0, 0, 0, nullptr);
    s_clickedAt = frameNum;
    std::fprintf(stderr, "[PauseMenu] OnCommand returned\n");
}
