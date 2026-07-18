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

namespace AutoStart {

namespace {

// --- Layout constants (verified elsewhere in the standalone code) ---
constexpr uintptr_t kMenuDLT_DialogInstance = 0x38;   // MenuDLT m_dialogInstance
constexpr uintptr_t kNewGame_ButtonOk       = 0xF0;   // NewGameDialog::buttonOk
constexpr int       kVtableSlot_OnCommand   = 8;      // slot8 (+0x40): OnCommand/OnEvent

// Dialog command/event protocol (matches the UI dispatcher).
constexpr int kMsg_ButtonClick = 0x12;
constexpr int kButtonId_New    = 0;   // MainMenuDialog "buttonNew"
constexpr int kEventId_Ok      = 2;   // NewGameDialog "buttonOk" (Start)

// State of the driver.
enum class Phase {
    Disabled,
    WaitMenuSettle,   // menu up; counting down before firing New Game
    WaitNewGame,      // New Game opened; waiting for its Slot4 (combos/OK)
    Done,
};

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

    const char* env = std::getenv("BANISHED_AUTOSTART");
    if (!env || env[0] == '\0' ||
        std::strcmp(env, "0") == 0 ||
        std::strcmp(env, "false") == 0) {
        s_enabled = false;
        s_phase   = Phase::Disabled;
        printf("[AutoStart] disabled (BANISHED_AUTOSTART unset)\n");
        return;
    }

    // A numeric value overrides the menu settle-frame count.
    int parsed = std::atoi(env);
    if (parsed > 1) s_settleFrames = parsed;

    s_enabled = true;
    s_phase   = Phase::WaitMenuSettle;
    printf("[AutoStart] ENABLED (BANISHED_AUTOSTART=\"%s\") — will auto-open "
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

        printf("[AutoStart] menu settled (mainMenu=%p) — injecting New Game "
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
            printf("[AutoStart] NewGameDialog ready (self=%p) — injecting "
                   "Start click (buttonOk)\n", newGame);
            InjectClick(newGame, kMsg_ButtonClick, kEventId_Ok);
            s_phase = Phase::Done;
            printf("[AutoStart] sequence fired — new-game transition queued; "
                   "driver done\n");
            return;
        }

        if (s_newGameFrame > kNewGameTimeoutFrames) {
            printf("[AutoStart] WARN: NewGameDialog setup did not complete in "
                   "%d frames (self=%p) — aborting auto-Start to avoid a crash "
                   "on half-built combos\n",
                   kNewGameTimeoutFrames, newGame);
            s_phase = Phase::Done;
        }
        break;
    }

    default:
        break;
    }
}

}  // namespace AutoStart

