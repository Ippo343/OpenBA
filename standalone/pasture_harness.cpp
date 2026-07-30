// standalone/pasture_harness.cpp
// BANISHED_PASTURE_FILL=<frame> — one-shot pasture stocking for the animal-wave
// test rig. At the chosen in-game frame (value "1" means the default frame 900,
// ~15s in: long past the pasture controller's state-2 enter, so AUTOTYPE has
// already assigned a livestock type), calls the ported debug-menu "fill
// pastures" body (retail 0x140060290: for every pasture in the world list with
// an assigned type, spawn animals up to capacity, 5 per pasture per call).
// Retail reaches the same body from DebugUI_Slot9; the retail PLAYER path is
// trade delivery, which standalone doesn't have yet — this is the stand-in.
// No-op unless the env var is set AND BANISHED_LIVE_PASTURE is live (the
// address only resolves when reg_pasture_live.cpp registered it).
#include "resolve.h"
#include <cstdio>
#include <cstdlib>

extern "C" void PastureFillHarness_Tick(int frameNum) {
    static int s_fillFrame = -2;              // -2 = env unread, -1 = disabled
    if (s_fillFrame == -2) {
        const char* e = std::getenv("BANISHED_PASTURE_FILL");
        if (!e || !e[0] || e[0] == '0') s_fillFrame = -1;
        else {
            int v = std::atoi(e);
            s_fillFrame = (v > 1) ? v : 900;
            fprintf(stderr, "[PastureFill] armed: will fill pastures at frame %d\n",
                    s_fillFrame);
        }
    }
    if (s_fillFrame < 0 || frameNum != s_fillFrame) return;

    typedef int (*fn_FillAllPastures_t)();
    uintptr_t addr = ResolveAddr(Module::Exe, 0x140060290);
    if (!addr) {
        fprintf(stderr, "[PastureFill] frame %d: 0x140060290 unresolved — is "
                "BANISHED_LIVE_PASTURE on?\n", frameNum);
        return;
    }
    int n = reinterpret_cast<fn_FillAllPastures_t>(addr)();
    fprintf(stderr, "[PastureFill] frame %d: fill pass ran (ret=%d) — see "
            "[Pasture] SPAWN/bind lines\n", frameNum, n);
    fflush(stderr);
}
