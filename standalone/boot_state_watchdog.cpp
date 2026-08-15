// standalone/boot_state_watchdog.cpp
// Machine-readable boot state, so an automated run can tell "blocked" from "slow".
//
// WHY THIS EXISTS
// The launcher dialog (Engine::PlatformConfigure -> DialogBoxParamW) is no longer
// patched out; retail shows it whenever the previous run did not finish
// Engine::Startup, which during development means every time we kill the process.
// HarnessGuardUnattendedBoot clears the arming bit for unattended runs, but it is a
// guard, not a guarantee -- it only fires when BANISHED_AUTOSTART/_AUTOQUIT is set, it
// cannot help an attended run, and any OTHER modal Win32 dialog (an assert, a driver
// prompt) blocks boot exactly the same way.
//
// When that happens the process is ALIVE and PUMPING MESSAGES: it holds a normal memory
// footprint and the WndMsg trace keeps ticking, while standalone_error.log simply stops
// mid-boot. That is indistinguishable from a slow load or a hang if all you have is a
// log tail, and an agent polling `grep -q HuntScan` will sit there until its timeout
// burns. This session lost a run to exactly that.
//
// WHAT IT DOES
// A watchdog thread polls once a second and writes ONE line to standalone_boot_state.txt
// (in the working directory, next to the logs):
//
//   STATE=<state> pid=<n> uptimeSec=<n> detail=<text>
//
//   BOOTING         started, Engine::Startup has not returned yet
//   RUNNING         Engine::Startup returned; normal operation
//   BLOCKED_DIALOG  a visible modal Win32 dialog is open -- boot cannot proceed
//                   without a human click. detail= carries its window title.
//
// The file is rewritten every second, so a stale file from a dead process is detectable
// via its pid (see tools/boot_state.sh, which reports EXITED/STALE for that case).
// The state is also logged: once when it changes, then every 15s while BLOCKED_DIALOG
// persists, so a log tail shows it too rather than going silent.
//
// DETECTION: Win32 dialogs created by DialogBoxParamW have window class "#32770".
// We enumerate top-level windows, keep the visible ones owned by THIS process, and
// report the first "#32770". That covers the launcher and any assert/system dialog
// without hooking or patching anything -- this file only observes.

#include "boot_state_watchdog.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace BootState {

namespace {

volatile LONG g_startupComplete = 0;
volatile LONG g_stop            = 0;
HANDLE        g_thread          = nullptr;
DWORD         g_startTick       = 0;

struct DialogScan {
    DWORD  pid;
    bool   found;
    wchar_t title[128];
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lp) {
    auto* scan = reinterpret_cast<DialogScan*>(lp);

    if (!IsWindowVisible(hwnd)) return TRUE;

    DWORD wndPid = 0;
    GetWindowThreadProcessId(hwnd, &wndPid);
    if (wndPid != scan->pid) return TRUE;

    wchar_t cls[64] = {0};
    if (!GetClassNameW(hwnd, cls, 63)) return TRUE;
    if (wcscmp(cls, L"#32770") != 0) return TRUE;   // not a Win32 dialog

    scan->found = true;
    GetWindowTextW(hwnd, scan->title, 127);
    return FALSE;   // first match is enough
}

// Returns true and fills utf8Title when a modal dialog owned by us is on screen.
bool ModalDialogOpen(char* utf8Title, int cap) {
    DialogScan scan{};
    scan.pid = GetCurrentProcessId();
    EnumWindows(&EnumProc, reinterpret_cast<LPARAM>(&scan));
    if (!scan.found) return false;

    if (cap > 0) {
        utf8Title[0] = '\0';
        WideCharToMultiByte(CP_UTF8, 0, scan.title, -1, utf8Title, cap, nullptr, nullptr);
        utf8Title[cap - 1] = '\0';
    }
    return true;
}

void WriteStateFile(const char* state, unsigned uptimeSec, const char* detail) {
    // Rewritten in full every tick; a reader either sees the previous complete line or
    // this one. Kept to a single line so a script can parse it with one grep.
    FILE* f = std::fopen("standalone_boot_state.txt", "w");
    if (!f) return;
    std::fprintf(f, "STATE=%s pid=%lu uptimeSec=%u detail=%s\n",
                 state, (unsigned long)GetCurrentProcessId(), uptimeSec,
                 (detail && detail[0]) ? detail : "-");
    std::fclose(f);
}

DWORD WINAPI WatchdogThread(LPVOID) {
    const char* lastState = "";
    unsigned    blockedTicks = 0;

    while (InterlockedCompareExchange(&g_stop, 0, 0) == 0) {
        const unsigned uptimeSec = (GetTickCount() - g_startTick) / 1000;

        char title[128] = {0};
        const bool blocked = ModalDialogOpen(title, sizeof(title));

        const char* state = blocked
            ? "BLOCKED_DIALOG"
            : (InterlockedCompareExchange(&g_startupComplete, 0, 0) ? "RUNNING" : "BOOTING");

        WriteStateFile(state, uptimeSec, blocked ? title : "-");

        // Log on transition, then keep reminding while blocked -- a silent log is the
        // very symptom this exists to explain, so it must not go quiet again.
        const bool changed = (std::strcmp(state, lastState) != 0);
        if (blocked) {
            if (changed || (blockedTicks % 15) == 0) {
                std::fprintf(stderr,
                    "[BootState] BLOCKED_DIALOG after %us -- a modal Win32 dialog "
                    "(class #32770, title=\"%s\") is open and boot cannot proceed "
                    "without a click. This is usually the launcher dialog, armed "
                    "because the previous run did not exit cleanly.\n",
                    uptimeSec, title);
                std::fflush(stderr);
            }
            ++blockedTicks;
        } else {
            if (changed && std::strcmp(state, "RUNNING") == 0) {
                std::fprintf(stderr, "[BootState] RUNNING (Engine::Startup returned at %us)\n",
                             uptimeSec);
                std::fflush(stderr);
            }
            if (changed && blockedTicks) {
                std::fprintf(stderr, "[BootState] dialog cleared after %u s blocked\n",
                             blockedTicks);
                std::fflush(stderr);
            }
            blockedTicks = 0;
        }
        lastState = state;

        Sleep(1000);
    }
    return 0;
}

}  // namespace

void Start() {
    if (g_thread) return;
    g_startTick = GetTickCount();
    // Publish BOOTING immediately: a run that dies before the first tick should still
    // leave a file saying it got this far, rather than none at all.
    WriteStateFile("BOOTING", 0, "-");
    g_thread = CreateThread(nullptr, 0, &WatchdogThread, nullptr, 0, nullptr);
    if (!g_thread) {
        std::fprintf(stderr, "[BootState] WARNING: watchdog thread failed to start "
                             "(GetLastError=%lu) -- boot state will not be published\n",
                     GetLastError());
        std::fflush(stderr);
    }
}

void MarkStartupComplete() {
    InterlockedExchange(&g_startupComplete, 1);
}

void Stop() {
    InterlockedExchange(&g_stop, 1);
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    WriteStateFile("EXITED", (GetTickCount() - g_startTick) / 1000, "clean-shutdown");
}

}  // namespace BootState
