// standalone/saveload_harness.cpp
// Save/load end-to-end harness. Env BANISHED_SAVE_TEST (unset/0 = off; a value
// > 1 overrides the trigger frame, default 900 ≈ 15s in-game) drives the REAL
// retail autosave path at the trigger frame: Dialog_AutoSaveHandler — the ported
// FUN_14005dec0 — which does SetMinOperationTime(1.0s) + SetSaveSprite +
// SetSaveName("Autosave",2) + SetOperation(4,-4). The engine's
// Save::SaveLoad::UpdateSave then calls the current loop's vtable +0x38
// (SaveRestore) to fill the SavedState and async-writes <SaveDir>/Auto.sav.
//
// After triggering, polls SaveLoad::GetOperation each frame and logs
// transitions; when the operation returns to 0 it stats the save directory so
// the log shows exactly what got written (file, size, age). No pass/fail
// judgement here — the .sav content is verified outside the process.

#include "options.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "save/SaveLoad.h"   // -Iruntime/types: ENDGAME B.3.7 callers (K5)

// code/Game/Dialog/dialog_exe_helpers.cpp — ported retail autosave trigger.
void Dialog_AutoSaveHandler(void* self, void* p2, int p3, int p4);

namespace {

int TriggerFrame() {
    static int s = -2;
    if (s == -2) {
        const char* e = Opt().saveTest;
        if (!e || !e[0] || (e[0] == '0' && !e[1]) ||
            _stricmp(e, "false") == 0 ||
            _stricmp(e, "roundtrip") == 0) {   // the in-process gate, not a frame trigger
            s = -1;                       // off
        } else {
            int v = std::atoi(e);
            s = (v > 1) ? v : 900;        // "1"/non-numeric = default frame
        }
    }
    return s;
}

// GetSaveDirectory returns a System::String; avoid the ABI dance and just stat
// the known per-user save dir the engine uses (Persistence::GetSaveDirectory →
// Documents/Banished).
void DumpSaveDir(const char* tag) {
    char path[MAX_PATH];
    const char* prof = Opt().userProfile;
    if (!prof) return;
    snprintf(path, sizeof(path), "%s\\Documents\\Banished\\Save\\*", prof);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(path, &fd);
    fprintf(stderr, "[SaveTest] %s — save dir listing:\n", tag);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[SaveTest]   (dir empty or missing)\n");
        fflush(stderr);
        return;
    }
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        ULARGE_INTEGER sz;
        sz.LowPart  = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;
        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        ULARGE_INTEGER wt, nt;
        wt.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        wt.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        nt.LowPart = now.dwLowDateTime;
        nt.HighPart = now.dwHighDateTime;
        long long ageSec = (long long)((nt.QuadPart - wt.QuadPart) / 10000000ULL);
        fprintf(stderr, "[SaveTest]   %-24s %9llu bytes  written %llds ago\n",
                fd.cFileName, (unsigned long long)sz.QuadPart, ageSec);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    fflush(stderr);
}

} // namespace

extern "C" void SaveLoadHarness_Tick(void* gul, int frameNum) {
    const int trigger = TriggerFrame();
    if (trigger < 0) return;

    static int  s_state = 0;      // 0=armed, 1=waiting for op!=0, 2=op running, 3=done
    static int  s_lastOp = -999;
    static int  s_triggeredFrame = 0;

    // Save::SaveLoad::_singleton, the class static (written by the SaveLoad ctor
    // in GameUpdateLoop::Load step 25).
    static bool s_armed = false;
    if (!s_armed) {
        s_armed = true;
        fprintf(stderr, "[SaveTest] armed (trigger frame %d)\n", trigger);
        fflush(stderr);
    }
    void* saveLoad = ::Save::SaveLoad::_singleton;
    if (!saveLoad) return;

    if (s_state == 0) {
        if (frameNum < trigger) return;
        DumpSaveDir("pre-save");

        // Dialog_AutoSaveHandler only reads self+0x10 (UserInterface*).
        // GameUpdateLoop keeps it at +0x98.
        struct { uint8_t pad[0x10]; void* ui; } fake = {};
        fake.ui = *(void**)((uintptr_t)gul + 0x98);
        fprintf(stderr, "[SaveTest] frame %d: invoking retail autosave handler "
                        "(SetOperation(4,-4))\n", frameNum);
        fflush(stderr);
        Dialog_AutoSaveHandler(&fake, nullptr, 0x13, 0);

        int op = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
        fprintf(stderr, "[SaveTest] handler returned; GetOperation()=%d "
                        "(4=save expected; 0=trigger was refused)\n", op);
        fflush(stderr);
        s_triggeredFrame = frameNum;
        s_lastOp = op;
        s_state = (op != 0) ? 2 : 1;
        return;
    }

    int op = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
    if (op != s_lastOp) {
        fprintf(stderr, "[SaveTest] frame %d: operation %d -> %d\n",
                frameNum, s_lastOp, op);
        fflush(stderr);
        s_lastOp = op;
    }

    if (s_state == 1) {
        // Trigger was refused (paused / blocked / op busy). Retry every 120 frames.
        if ((frameNum - s_triggeredFrame) % 120 == 0) {
            s_state = 0;
        }
        return;
    }

    if (s_state == 2) {
        if (op == 0) {
            fprintf(stderr, "[SaveTest] frame %d: SAVE OPERATION COMPLETE "
                            "(%d frames elapsed)\n",
                    frameNum, frameNum - s_triggeredFrame);
            DumpSaveDir("post-save");
            // BANISHED_SAVE_TEST_REPEAT=<frames>: keep saving on an interval.
            // A save only crashes once the world has produced the offending
            // state (e.g. a stale Work in the goals tree), so one early save
            // proves nothing — re-arm and keep probing as the town ages.
            static int s_repeat = -2;
            if (s_repeat == -2) {
                s_repeat = Opt().saveTestRepeat;
            }
            if (s_repeat > 0) {
                s_triggeredFrame = frameNum + s_repeat;
                s_state = 4;
            } else {
                s_state = 3;
            }
        } else if ((frameNum - s_triggeredFrame) % 300 == 0) {
            fprintf(stderr, "[SaveTest] frame %d: still saving (op=%d)\n",
                    frameNum, op);
            fflush(stderr);
        }
        return;
    }

    if (s_state == 4) {
        if (frameNum >= s_triggeredFrame) {
            fprintf(stderr, "[SaveTest] frame %d: re-arming (repeat save)\n", frameNum);
            fflush(stderr);
            s_state = 0;
        }
        return;
    }
}
