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

#include "engine_funcs.h"

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

// State of the driver.
enum class Phase {
    Disabled,
    WaitMenuSettle,   // menu up; counting down before firing New Game
    WaitNewGame,      // New Game opened; waiting for its Slot4 (combos/OK)
    WaitQuit,         // menu up; counting down before firing Quit
    WaitLoadMenu,     // menu up; counting down before firing Load (AUTOLOAD)
    WaitLoadDialog,   // SaveLoadDialog open; waiting for the file list, then click Load
    Done,
};

// AUTOLOAD: name of the save to load (defaults to the autosave, "Auto").
char s_loadName[64] = "Auto";

bool   s_initialized   = false;
bool   s_enabled       = false;
Phase  s_phase         = Phase::Disabled;
int    s_settleFrames  = 60;     // frames to let the menu render before firing
int    s_phaseFrame    = 0;      // frames spent in the current phase
int    s_newGameFrame  = 0;      // frames since New Game opened (Slot4 grace)

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
        if (++s_phaseFrame < s_settleFrames) return;

        fprintf(stderr, "[AutoStart] menu settled (mainMenu=%p) — injecting New Game "
               "click\n", mainMenu);
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

