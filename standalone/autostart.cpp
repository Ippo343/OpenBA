// standalone/autostart.cpp
// See autostart.h for the rationale. Env-var-gated state machine that drives
// the menu -> New Game -> Start sequence by injecting synthetic dialog
// slot-8 events, reusing the already-reimplemented click handlers.


#include "autostart.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Captured by NewGameDialog_Ctor_Standalone (code/Game/Dialog/newgamedialog.cpp).
extern "C" {
    void* g_StandaloneNewGameDialog = nullptr;
}
// Captured by MainMenuDialog_OpenLoad (code/Game/Dialog/mainmenudialog.cpp).
void* g_StandaloneSaveLoadDialog = nullptr;
// Captured by MainMenuDialog_OpenTutorial (same file). ScenarioDialog and
// TutorialDialog are ONE class -- the Tutorial button is the only way to reach it.
void* g_StandaloneScenarioDialog = nullptr;

#include "engine_funcs.h"

// code/Game/Dialog/mod_dialog_vtables.cpp -- targets for BANISHED_AUTOMENU_CLICKS
extern "C" void* ModDialog_LastOpened();
extern "C" void* ModDetailsDialog_LastOpened();

namespace AutoStart {

namespace {

// --- Layout constants (verified elsewhere in the standalone code) ---
constexpr uintptr_t kMenuDLT_DialogInstance = 0x38;   // MenuDLT m_dialogInstance
constexpr uintptr_t kNewGame_ButtonOk       = 0xF0;   // NewGameDialog::buttonOk
constexpr int       kVtableSlot_OnCommand   = 8;      // slot8 (+0x40): OnCommand/OnEvent

// Dialog command/event protocol (matches the UI dispatcher).
constexpr int kMsg_ButtonClick = 0x12;
constexpr int kButtonId_New    = 0;   // MainMenuDialog "buttonNew"
constexpr int kButtonId_Load   = 2;   // MainMenuDialog "buttonLoad" (kButton_Load)
constexpr int kButtonId_Quit   = 6;   // MainMenuDialog "buttonQuit"
constexpr int kEventId_Ok      = 2;   // NewGameDialog "buttonOk" (Start)
constexpr int kSLD_LoadClick   = 2;   // SaveLoadDialog OnEvent 0x12/2 = buttonLoad
constexpr int kButtonId_Tutorial = 12;    // MainMenuDialog "buttonTutorial" (kButton_Tutorial)
constexpr int kSD_LoadClick      = 0x65;  // ScenarioDialog Slot8 0x12/101 = LoadSelected
constexpr uintptr_t kSD_SelectedIndex = 0xC8;  // ScenarioDialog selectedIndex (i32, ctor seeds -1)
constexpr uintptr_t kSD_EntryCount    = 0xE0;  // ScenarioDialog entry count (i32)

// State of the driver.
enum class Phase {
    Disabled,
    WaitMenuSettle,   // menu up; counting down before firing New Game
    WaitNewGame,      // New Game opened; waiting for its Slot4 (combos/OK)
    WaitQuit,         // menu up; counting down before firing Quit
    WaitLoadMenu,     // menu up; counting down before firing Load (AUTOLOAD)
    WaitLoadDialog,   // SaveLoadDialog open; waiting for the file list, then click Load
    WaitTutorialMenu, // menu up; counting down before firing Tutorial (AUTOTUTORIAL)
    WaitTutorialDialog, // ScenarioDialog open; waiting for its entry list, then click Load
    WaitMenuButton,   // menu up; counting down before firing an arbitrary button id
    MenuClicks,       // AUTOMENU_BUTTON fired; feeding BANISHED_AUTOMENU_CLICKS tokens
    Done,
};

// BANISHED_AUTOMENU_CLICKS=<tok>[,<tok>...]: follow-up clicks delivered one per
// settle interval after the AUTOMENU_BUTTON click. Token forms:
//   <id>   click element id <id> on the most recently opened ModDialog
//   d<id>  click element id <id> on the most recently opened ModDetailsDialog
//   m<id>  click element id <id> on the MainMenuDialog (e.g. m7 re-opens Mods)
// Only the mod dialogs are addressable for now; add a *_LastOpened() per
// dialog type to extend it.
char  s_menuClicks[256] = {0};
char* s_menuClickCursor = nullptr;

// AUTOLOAD: name of the save to load (defaults to the autosave, "Auto").
char s_loadName[64] = "Auto";

// AUTOTUTORIAL: which tutorial entry to load (index into the ScenarioDialog list).
int s_tutorialIndex = 0;

// AUTOMENU_BUTTON: an arbitrary MainMenuDialog element id to click at the front end,
// then leave the result on screen. The in-game half of every menu button already has a
// driver (BANISHED_PAUSEMENU_BUTTON, standalone/pausemenu_harness.cpp); this is the
// front-end half, so a menu-only dialog (Achievements id 8, Goals id 10, ...) can be
// brought up and screenshotted with nobody at the mouse.
int s_menuButtonId = -1;

bool   s_initialized   = false;
bool   s_enabled       = false;
Phase  s_phase         = Phase::Disabled;
int    s_settleFrames  = 60;     // frames to let the menu render before firing
int    s_phaseFrame    = 0;      // frames spent in the current phase
int    s_newGameFrame  = 0;      // frames since New Game opened (Slot4 grace)

// BANISHED_AUTOSTART_GAMES=<n>: start n games in one process, not one. After the
// Start click the driver rearms instead of finishing, so a quit-to-main-menu (see
// standalone/pausemenu_harness.cpp, BANISHED_PAUSEMENU_BUTTON=5) is followed by a
// second New Game. That sequence is what catches teardown bugs a single-game run
// cannot see -- anything the world teardown frees while a process-lifetime cache
// still points at it faults on game 2, not game 1.
int    s_gamesLeft     = 1;      // New Games still to start
void*  s_lastMenu      = nullptr;// the MainMenuDialog we last clicked New Game on


// Safety timeout: if the NewGameDialog never finishes setup, give up rather
// than firing Start against half-built combos (which would crash).
constexpr int kNewGameTimeoutFrames = 600;   // ~10s at 60fps

// Invoke a dialog's vtable slot-8 handler as a synthetic button click.
// MainMenuDialog::OnCommand and NewGameDialog::OnEvent share the same shape:
//   (self, element=null, msg, id)
void InjectClick(void* dialog, int msg, int id) {
    void** vt = *reinterpret_cast<void***>(dialog);
    using Handler = void (*)(void*, void*, int, int);
    reinterpret_cast<Handler>(vt[kVtableSlot_OnCommand])(dialog, nullptr, msg, id);
}

}  // namespace

void Init() {
    if (s_initialized) return;
    s_initialized = true;

    // BANISHED_AUTOQUIT drives menu -> Quit instead of menu -> New Game. It
    // exercises the only retail exit path (MainMenuDialog buttonQuit ->
    // Application::Quit -> MainLoop::Run returns -> Engine::Shutdown), which is
    // what runs Video::Configuration::Save + Persistence::WriteSettings. Use it
    // to verify settings actually round-trip to the registry.
    if (const char* q = std::getenv("BANISHED_AUTOQUIT")) {
        if (q[0] != '\0' && std::strcmp(q, "0") != 0 &&
            std::strcmp(q, "false") != 0) {
            int parsedQ = std::atoi(q);
            if (parsedQ > 1) s_settleFrames = parsedQ;
            s_enabled = true;
            s_phase   = Phase::WaitQuit;
            fprintf(stderr, "[AutoStart] AUTOQUIT ENABLED (BANISHED_AUTOQUIT=\"%s\") — will "
                   "click Quit after %d menu frames\n", q, s_settleFrames);
            return;
        }
    }

    // BANISHED_AUTOLOAD drives menu -> Load -> select save -> Load click, the
    // full retail load-from-save path (SaveLoadDialog list scan, DoLoad,
    // SetOperation(2), loader-loop chain, GameUpdateLoop::Load restore branch).
    // Value: a save NAME to load ("Auto", "Save0", ...); "1" defaults to the
    // autosave "Auto". (Save/load wave 2026-08-27.)
    if (const char* l = std::getenv("BANISHED_AUTOLOAD")) {
        if (l[0] != '\0' && std::strcmp(l, "0") != 0 &&
            std::strcmp(l, "false") != 0) {
            if (std::strcmp(l, "1") != 0) {
                std::snprintf(s_loadName, sizeof(s_loadName), "%s", l);
            }
            s_enabled = true;
            s_phase   = Phase::WaitLoadMenu;
            fprintf(stderr, "[AutoStart] AUTOLOAD ENABLED — will click Load after %d "
                   "menu frames, then load save \"%s\"\n", s_settleFrames, s_loadName);
            return;
        }
    }

    // BANISHED_AUTOTUTORIAL drives menu -> Tutorial -> select entry -> Load click.
    // That is the ONLY path to a retail-authored save: the tutorial .sav files live
    // inside WinData/*.pkg, and ScenarioDialog::LoadSelected swaps persistence+0x90
    // to the data directory so SaveLoad reads them. Value: the entry index to load
    // ("1" or empty means index 0).
    if (const char* t = std::getenv("BANISHED_AUTOTUTORIAL")) {
        if (t[0] != '\0' && std::strcmp(t, "0") != 0 &&
            std::strcmp(t, "false") != 0) {
            if (std::strcmp(t, "1") != 0) s_tutorialIndex = std::atoi(t);
            s_enabled = true;
            s_phase   = Phase::WaitTutorialMenu;
            // This block RETURNS before BANISHED_AUTOSTART_GAMES is parsed below, so
            // read it here and default to ZERO on the tutorial path. Defaulting to
            // s_gamesLeft's initial 1 would silently make every existing AUTOTUTORIAL
            // run start a New Game after a quit-to-menu, which is not what those runs
            // have always meant.
            s_gamesLeft = 0;
            if (const char* g = std::getenv("BANISHED_AUTOSTART_GAMES")) {
                const int n = std::atoi(g);
                if (n > 0) {
                    s_gamesLeft = n;
                    fprintf(stderr, "[AutoStart] AUTOTUTORIAL + %d New Game(s) after a "
                           "quit-to-menu (BANISHED_AUTOSTART_GAMES)\n", n);
                }
            }
            fprintf(stderr, "[AutoStart] AUTOTUTORIAL ENABLED — will click Tutorial after "
                   "%d menu frames, then load entry #%d\n",
                   s_settleFrames, s_tutorialIndex);
            return;
        }
    }

    // BANISHED_AUTOMENU_BUTTON=<id> clicks one front-end menu button and stops there,
    // leaving whatever it opened up for a screenshot (tools/capture_window.ps1).
    if (const char* m = std::getenv("BANISHED_AUTOMENU_BUTTON")) {
        if (m[0] != '\0' && std::strcmp(m, "0") != 0) {
            s_menuButtonId = std::atoi(m);
            s_enabled = true;
            s_phase   = Phase::WaitMenuButton;
            if (const char* c = std::getenv("BANISHED_AUTOMENU_CLICKS")) {
                std::strncpy(s_menuClicks, c, sizeof(s_menuClicks) - 1);
                s_menuClickCursor = s_menuClicks[0] ? s_menuClicks : nullptr;
                if (s_menuClickCursor)
                    fprintf(stderr, "[AutoStart] AUTOMENU_CLICKS queued: %s\n", s_menuClicks);
            }
            fprintf(stderr, "[AutoStart] AUTOMENU_BUTTON ENABLED - will click menu id=%d "
                   "after %d menu frames and stop there\n", s_menuButtonId, s_settleFrames);
            return;
        }
    }
    const char* env = std::getenv("BANISHED_AUTOSTART");
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 ||
        std::strcmp(env, "false") == 0) {
        s_enabled = false;
        s_phase   = Phase::Disabled;
        fprintf(stderr, "[AutoStart] disabled (BANISHED_AUTOSTART unset)\n");
        return;
    }

    // A numeric value overrides the menu settle-frame count.
    int parsed = std::atoi(env);
    if (parsed > 1) s_settleFrames = parsed;

    if (const char* g = std::getenv("BANISHED_AUTOSTART_GAMES")) {
        int n = std::atoi(g);
        if (n > 1) {
            s_gamesLeft = n;
            fprintf(stderr, "[AutoStart] will start %d games this run "
                   "(BANISHED_AUTOSTART_GAMES)\n", n);
        }
    }

    s_enabled = true;
    s_phase   = Phase::WaitMenuSettle;
    fprintf(stderr, "[AutoStart] ENABLED (BANISHED_AUTOSTART=\"%s\") — will auto-open "
           "New Game after %d menu frames, then auto-Start\n",
           env, s_settleFrames);
}

bool Enabled() { return s_enabled; }

void TickMenu(void* menuDLT) {
    if (!s_enabled || s_phase == Phase::Disabled || s_phase == Phase::Done)
        return;
    if (!menuDLT) return;

    switch (s_phase) {
    case Phase::WaitMenuSettle: {
        // The MainMenuDialog is created + set up (Slot4) by the menu loop's
        // Startup path; it lands at MenuDLT+0x38. Wait for it, then let the
        // menu render for s_settleFrames before injecting the New Game click.
        void* mainMenu = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
        if (!mainMenu) {
            s_phaseFrame = 0;   // not ready yet — keep waiting
            return;
        }
        // On a rearm (BANISHED_AUTOSTART_GAMES > 1) the old menu keeps ticking for
        // a few frames after the Start click, before the loop transition takes
        // hold. Refuse to fire on that same dialog — wait for the fresh one a
        // quit-to-menu builds, or we would re-enter New Game on a dying menu.
        if (mainMenu == s_lastMenu) {
            s_phaseFrame = 0;
            return;
        }
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) — injecting New Game "
               "click\n", mainMenu);
        s_lastMenu = mainMenu;
        InjectClick(mainMenu, kMsg_ButtonClick, kButtonId_New);
        s_phase        = Phase::WaitNewGame;
        s_newGameFrame = 0;
        break;
    }

    case Phase::WaitNewGame: {
        ++s_newGameFrame;
        void* newGame = g_StandaloneNewGameDialog;
        // NewGameDialog::Slot4 (Startup) finds buttonOk and populates the
        // combos. buttonOk != null is our proxy that setup finished, so
        // StartGame can safely read the combo selections.
        bool setupDone = newGame &&
            *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(newGame) + kNewGame_ButtonOk) != nullptr;

        if (setupDone) {
            fprintf(stderr, "[AutoStart] NewGameDialog ready (self=%p) — injecting "
                   "Start click (buttonOk)\n", newGame);
            InjectClick(newGame, kMsg_ButtonClick, kEventId_Ok);
            if (--s_gamesLeft > 0) {
                // Rearm for the next game. WaitMenuSettle refuses to fire on the
                // menu we just clicked (s_lastMenu), so the few frames the old menu
                // still ticks before the transition cannot re-trigger New Game; it
                // waits for the fresh MainMenuDialog a quit-to-menu rebuilds.
                s_phase        = Phase::WaitMenuSettle;
                s_phaseFrame   = 0;
                s_newGameFrame = 0;
                fprintf(stderr, "[AutoStart] sequence fired — new-game transition queued; "
                       "%d more game(s) to start, rearmed\n", s_gamesLeft);
                return;
            }
            s_phase = Phase::Done;
            fprintf(stderr, "[AutoStart] sequence fired — new-game transition queued; "
                   "driver done\n");
            return;
        }

        if (s_newGameFrame > kNewGameTimeoutFrames) {
            fprintf(stderr, "[AutoStart] WARN: NewGameDialog setup did not complete in "
                   "%d frames (self=%p) — aborting auto-Start to avoid a crash "
                   "on half-built combos\n",
                   kNewGameTimeoutFrames, newGame);
            s_phase = Phase::Done;
        }
        break;
    }

    case Phase::WaitMenuButton: {
        void* mainMenu = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
        if (!mainMenu) {
            s_phaseFrame = 0;
            return;
        }
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) - injecting click id=%d\n",
                mainMenu, s_menuButtonId);
        InjectClick(mainMenu, kMsg_ButtonClick, s_menuButtonId);
        s_phase      = s_menuClickCursor ? Phase::MenuClicks : Phase::Done;
        s_phaseFrame = 0;
        break;
    }

    case Phase::MenuClicks: {
        if (++s_phaseFrame < s_settleFrames) return;
        s_phaseFrame = 0;
        if (!s_menuClickCursor || !*s_menuClickCursor) { s_phase = Phase::Done; break; }
        char* tok = s_menuClickCursor;
        char* comma = std::strchr(tok, ',');
        if (comma) { *comma = '\0'; s_menuClickCursor = comma + 1; }
        else       { s_menuClickCursor = nullptr; }

        if (tok[0] == 'm') {
            void* mainMenu = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
            int id = std::atoi(tok + 1);
            fprintf(stderr, "[AutoStart] AUTOMENU_CLICKS: MainMenuDialog=%p id=%d\n", mainMenu, id);
            if (mainMenu) InjectClick(mainMenu, kMsg_ButtonClick, id);
        } else if (tok[0] == 'd') {
            void* dlg = ModDetailsDialog_LastOpened();
            int id = std::atoi(tok + 1);
            fprintf(stderr, "[AutoStart] AUTOMENU_CLICKS: ModDetailsDialog=%p id=%d\n", dlg, id);
            if (dlg) {
                void** vt = *reinterpret_cast<void***>(dlg);
                using H = void (*)(void*, uint64_t, int, int);
                reinterpret_cast<H>(vt[kVtableSlot_OnCommand])(dlg, 0, kMsg_ButtonClick, id);
            }
        } else {
            void* dlg = ModDialog_LastOpened();
            int id = std::atoi(tok);
            fprintf(stderr, "[AutoStart] AUTOMENU_CLICKS: ModDialog=%p id=%d\n", dlg, id);
            // A fabricated click on a per-row id (1000..5999: row = id % 1000)
            // must name a row the dialog actually built. The real UI cannot
            // deliver such a click without the element, but this driver can,
            // and with no .pkm installed (WinData/_mods_off holds them) the
            // entry array count at +0xC0 is 0 and the row handler faults reading
            // its entry. Mirror WaitLoadDialog's saveCount gate: skip loudly.
            if (dlg && id >= 1000 && id < 6000) {
                const int rows = *reinterpret_cast<int*>(static_cast<uint8_t*>(dlg) + 0xC0);
                const int row  = id % 1000;
                if (row >= rows) {
                    fprintf(stderr, "[AutoStart] WARN: AUTOMENU_CLICKS token %d names row %d "
                                    "but the ModDialog has %d row(s) -- skipped (are any "
                                    ".pkm files installed in WinData?)\n", id, row, rows);
                    dlg = nullptr;
                }
            }
            if (dlg) {
                // ModDialog::OnElementEvent is the 8-arg shape and reads *param2 as
                // the element InternalPtr, so hand it a real (null) qword.
                void* nullElem = nullptr;
                void** vt = *reinterpret_cast<void***>(dlg);
                using H = void (*)(void*, void*, int, int, uint32_t, uint32_t, uint32_t, uint64_t);
                reinterpret_cast<H>(vt[kVtableSlot_OnCommand])(dlg, &nullElem, kMsg_ButtonClick,
                                                               id, 0, 0, 0, 0);
            }
        }
        if (!s_menuClickCursor) s_phase = Phase::Done;
        break;
    }

    case Phase::WaitLoadMenu: {
        void* mainMenu = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
        if (!mainMenu) {
            s_phaseFrame = 0;
            return;
        }
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) — injecting Load click\n",
                mainMenu);
        InjectClick(mainMenu, kMsg_ButtonClick, kButtonId_Load);
        s_phase        = Phase::WaitLoadDialog;
        s_newGameFrame = 0;
        break;
    }

    case Phase::WaitLoadDialog: {
        ++s_newGameFrame;
        void* dlg = g_StandaloneSaveLoadDialog;
        if (!dlg) {
            if (s_newGameFrame > kNewGameTimeoutFrames) {
                fprintf(stderr, "[AutoStart] WARN: SaveLoadDialog never appeared — "
                       "aborting AUTOLOAD\n");
                s_phase = Phase::Done;
            }
            return;
        }
        uintptr_t d = reinterpret_cast<uintptr_t>(dlg);
        // Wait for the list scan: refreshFlag (+0xBB) cleared AND saveCount
        // (+0xC4) populated by SaveLoadDialog_Load's refresh pass.
        int saveCount = *reinterpret_cast<int*>(d + 0xC4);
        if (*reinterpret_cast<uint8_t*>(d + 0xBB) != 0 || saveCount <= 0) {
            if (s_newGameFrame > kNewGameTimeoutFrames) {
                fprintf(stderr, "[AutoStart] WARN: save list never populated "
                       "(refreshFlag=%d count=%d) — aborting AUTOLOAD\n",
                       *reinterpret_cast<uint8_t*>(d + 0xBB), saveCount);
                s_phase = Phase::Done;
            }
            return;
        }

        // Pick the save whose SaveInfo name (+0x38 String) matches s_loadName;
        // fall back to index 0.
        int pick = 0;
        if (g_pSaveLoadSingleton && *g_pSaveLoadSingleton && fn_SaveLoad_GetSaveInfo) {
            for (int i = 0; i < saveCount; i++) {
                void* info = fn_SaveLoad_GetSaveInfo(*g_pSaveLoadSingleton, i);
                const char* nm = info ? *reinterpret_cast<const char* const*>(
                    reinterpret_cast<uintptr_t>(info) + 0x38) : nullptr;
                if (nm && std::strcmp(nm, s_loadName) == 0) { pick = i; break; }
            }
        }
        *reinterpret_cast<int*>(d + 0xC0) = pick;   // selectedIndex (SelectEntry sans sound)
        fprintf(stderr, "[AutoStart] save list ready (%d entries) — selecting #%d "
               "(\"%s\") and injecting Load click\n", saveCount, pick, s_loadName);
        InjectClick(dlg, kMsg_ButtonClick, kSLD_LoadClick);
        s_phase = Phase::Done;
        fprintf(stderr, "[AutoStart] AUTOLOAD sequence fired — load operation queued; "
               "driver done\n");
        break;
    }

    case Phase::WaitTutorialMenu: {
        void* mainMenu = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
        if (!mainMenu) {
            s_phaseFrame = 0;
            return;
        }
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) — injecting Tutorial "
                "click\n", mainMenu);
        InjectClick(mainMenu, kMsg_ButtonClick, kButtonId_Tutorial);
        s_phase        = Phase::WaitTutorialDialog;
        s_newGameFrame = 0;
        break;
    }

    case Phase::WaitTutorialDialog: {
        ++s_newGameFrame;
        void* dlg = g_StandaloneScenarioDialog;
        if (!dlg) {
            if (s_newGameFrame > kNewGameTimeoutFrames) {
                fprintf(stderr, "[AutoStart] WARN: ScenarioDialog never appeared — "
                       "aborting AUTOTUTORIAL\n");
                s_phase = Phase::Done;
            }
            return;
        }
        uintptr_t d = reinterpret_cast<uintptr_t>(dlg);
        // Slot4 fills the entry array (+0xD8) and its count (+0xE0) from the
        // scenario resource list. Count > 0 is our proxy that setup finished.
        int count = *reinterpret_cast<int*>(d + kSD_EntryCount);
        if (count <= 0) {
            if (s_newGameFrame > kNewGameTimeoutFrames) {
                fprintf(stderr, "[AutoStart] WARN: tutorial list never populated "
                       "(count=%d) — aborting AUTOTUTORIAL\n", count);
                s_phase = Phase::Done;
            }
            return;
        }

        int pick = s_tutorialIndex;
        if (pick < 0 || pick >= count) {
            fprintf(stderr, "[AutoStart] AUTOTUTORIAL index %d out of range (count=%d) "
                    "— clamping to 0\n", pick, count);
            pick = 0;
        }
        // The ctor seeds selectedIndex = -1; LoadSelected indexes the entry array
        // with it unguarded, so it MUST be set before the click.
        *reinterpret_cast<int*>(d + kSD_SelectedIndex) = pick;
        fprintf(stderr, "[AutoStart] tutorial list ready (%d entries) — selecting #%d "
                "and injecting Load click\n", count, pick);
        InjectClick(dlg, kMsg_ButtonClick, kSD_LoadClick);
        // BANISHED_AUTOSTART_GAMES=<n> alongside AUTOTUTORIAL rearms the driver so a
        // quit-to-main-menu (PAUSEMENU_BUTTON=5) is followed by n NEW GAMES. Added
        // 2026-09-07 for a user-reported repro that the pre-existing games-loop could
        // not express: new -> quit -> new is CLEAN, but TUTORIAL -> quit -> new builds
        // an empty world. The distinction matters because a tutorial is a loaded
        // retail save and a new game is a generated world, so they tear down through
        // different paths -- and only the tutorial's leaves the next world broken.
        if (s_gamesLeft > 0) {
            s_phase        = Phase::WaitMenuSettle;
            s_phaseFrame   = 0;
            s_newGameFrame = 0;
            s_lastMenu     = nullptr;   // the tutorial never clicked a MainMenuDialog
            fprintf(stderr, "[AutoStart] AUTOTUTORIAL sequence fired — rearmed for %d "
                   "New Game(s) after a quit-to-menu\n", s_gamesLeft);
            break;
        }
        s_phase = Phase::Done;
        fprintf(stderr, "[AutoStart] AUTOTUTORIAL sequence fired — driver done\n");
        break;
    }

    case Phase::WaitQuit: {
        void* mainMenu = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(menuDLT) + kMenuDLT_DialogInstance);
        if (!mainMenu) {
            s_phaseFrame = 0;
            return;
        }
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) — injecting Quit click\n",
               mainMenu);
        InjectClick(mainMenu, kMsg_ButtonClick, kButtonId_Quit);
        s_phase = Phase::Done;
        break;
    }

    default:
        break;
    }
}

}  // namespace AutoStart

