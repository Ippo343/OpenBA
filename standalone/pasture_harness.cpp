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
#include "engine_funcs.h"
#include <cstdio>
#include <cstdlib>

// BANISHED_PASTURE_TEND=<frame interval> — herder-labor stand-in. Every N
// frames, sends message 0 to every typed pasture (Pasture_MsgReceive, retail
// slot 15), exactly what a herdsman's completed work cycle does: each message
// tags one random pastured animal breed-eligible (flag bit 23). Value "1"
// picks the default interval (300 frames ≈ 5s). Stand-in until the organic
// chain lands (PastureComponent_Slot14 cmd 2 posts PastureWork -> citizen
// herder claims it -> SendComponentMessage(0)).
static void PastureTendHarness_Tick(int frameNum) {
    static int s_interval = -2;               // -2 = env unread, -1 = disabled
    if (s_interval == -2) {
        const char* e = std::getenv("BANISHED_PASTURE_TEND");
        if (!e || !e[0] || e[0] == '0') s_interval = -1;
        else {
            int v = std::atoi(e);
            s_interval = (v > 1) ? v : 300;
            fprintf(stderr, "[PastureTend] armed: msg-0 every %d frames\n", s_interval);
        }
    }
    if (s_interval < 0 || frameNum == 0 || (frameNum % s_interval) != 0) return;

    typedef void (*fn_MsgReceive_t)(void* pasture, int msg);
    uintptr_t addr = ResolveAddr(Module::Exe, 0x140127720);
    if (!addr || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gs = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!gs) return;
    int n = *reinterpret_cast<int*>(gs + 0x98);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(gs + 0x90);
    if (n <= 0 || !list) return;
    for (int i = 0; i < n; ++i) {
        void* pasture = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!pasture) continue;
        if (*reinterpret_cast<void**>((uintptr_t)pasture + 0x28) == nullptr) continue;
        reinterpret_cast<fn_MsgReceive_t>(addr)(pasture, 0);
    }
}

// BANISHED_PASTURE_HERDERS=<n> — herder STAFFING stand-in (the ProfessionUI
// "+1 herdsman" click, the one player action left in the chain). One-shot at
// frame 1000 (after AUTOTYPE ran Slot14 cmd 2, which turned the workplace on
// and posted PastureWork): for every typed pasture whose workplace is
// accepting, drive the real seat cascade (CitizenEmploy_SeatWorkers =
// FindAvailableBuilding + cbf0->ca20->c5f0->c4b0, exactly what ProfessionUI's
// dd10 does per worker) for n workers on the pasture's natural profession key
// (WPC desc+0x20 — the key the workplace registered under). From the seat on,
// the chain is ORGANIC: the herder's CitizenAI claims the posted PastureWork,
// works it, and each completed cycle sends the slot-15 msg-0 breed tag.
// Default n=1; =0 disables. Candidates must be laborers (StartTown seeds the
// laborer profession on founding adults ungated).
extern "C" int CitizenEmploy_SeatWorkers(int64_t townMgr, uint64_t professionKey, int count);

// Manual play types the pasture from the dialog whenever the player gets to it — minutes
// after frame 1000 — so a one-shot fires against an untyped pasture and silently does
// nothing. Re-check every kHerderRetry frames from 1000 on, and remember which pastures
// have already been seated so a typed pasture gets its herder exactly once.
static const int kHerderRetry = 120;
static void* s_herderSeated[16];
static int   s_herderSeatedCount = 0;
static bool HerderAlreadySeated(void* pasture) {
    for (int i = 0; i < s_herderSeatedCount; ++i)
        if (s_herderSeated[i] == pasture) return true;
    return false;
}

static void PastureHerderHarness_Tick(int frameNum) {
    static int s_count = -2;                  // -2 = env unread
    if (s_count == -2) {
        const char* e = std::getenv("BANISHED_PASTURE_HERDERS");
        s_count = (!e || !e[0]) ? 1 : std::atoi(e);
        if (s_count > 0)
            fprintf(stderr, "[PastureHerders] armed: seat %d herder(s) for each typed "
                    "pasture, re-checked every %d frames from frame 1000\n",
                    s_count, kHerderRetry);
    }
    if (s_count <= 0 || frameNum < 1000 || ((frameNum - 1000) % kHerderRetry) != 0) return;

    typedef void* (*fn_FindWPC_t)(int64_t entity, void* outIBP);
    uintptr_t f2e10 = ResolveAddr(Module::Exe, 0x140002e10);
    if (!f2e10 || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
    uintptr_t gs = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
    if (!gs) return;
    int n = *reinterpret_cast<int*>(gs + 0x98);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(gs + 0x90);
    for (int i = 0; i < n && list; ++i) {
        void* pasture = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!pasture) continue;
        if (*reinterpret_cast<void**>((uintptr_t)pasture + 0x28) == nullptr) continue;
        if (HerderAlreadySeated(pasture)) continue;
        uint64_t entRef = *reinterpret_cast<uint64_t*>((uintptr_t)pasture + 0x10);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entRef);
        if (!entRef) continue;
        uint64_t wpcIBP = 0;
        reinterpret_cast<fn_FindWPC_t>(f2e10)(static_cast<int64_t>(entRef), &wpcIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIBP);
        if (!wpcIBP) continue;
        if (*reinterpret_cast<char*>(wpcIBP + 0x50) == 0) continue;   // workplace off
        uint64_t desc = *reinterpret_cast<uint64_t*>(wpcIBP + 0x18);
        uint64_t profKey = desc ? *reinterpret_cast<uint64_t*>(desc + 0x20) : 0;
        if (!profKey) continue;
        int seated = CitizenEmploy_SeatWorkers(static_cast<int64_t>(gs), profKey, s_count);
        if (s_herderSeatedCount < (int)(sizeof(s_herderSeated) / sizeof(s_herderSeated[0])))
            s_herderSeated[s_herderSeatedCount++] = pasture;
        fprintf(stderr, "[PastureHerders] frame %d: pasture=%p profKey=0x%llx "
                "seated %d/%d herder(s)%s\n", frameNum, pasture,
                (unsigned long long)profKey, seated, s_count,
                seated == 0 ? "  <-- NO IDLE LABORER FOUND (all citizens already "
                              "have a profession? BUILDER_AUTOPOOL takes them)" : "");
        fflush(stderr);
    }
}

static const int kFillRetry = 300;

extern "C" void PastureFillHarness_Tick(int frameNum) {
    PastureTendHarness_Tick(frameNum);
    PastureHerderHarness_Tick(frameNum);
    static int s_fillFrame = -2;              // -2 = env unread, -1 = disabled
    if (s_fillFrame == -2) {
        const char* e = std::getenv("BANISHED_PASTURE_FILL");
        if (!e || !e[0] || e[0] == '0') s_fillFrame = -1;
        else {
            int v = std::atoi(e);
            s_fillFrame = (v > 1) ? v : 900;
            fprintf(stderr, "[PastureFill] armed: fill pastures from frame %d, re-run "
                    "every %d frames (retail body only touches TYPED pastures and stops "
                    "at capacity, so this is self-limiting)\n", s_fillFrame, kFillRetry);
        }
    }
    // Same manual-play problem as the herder stand-in: a one-shot at frame 900 runs before
    // the player has picked a livestock type, and the retail fill body skips untyped
    // pastures — so nothing ever arrives. Repeat instead; it is bounded by pasture capacity.
    if (s_fillFrame < 0 || frameNum < s_fillFrame ||
        ((frameNum - s_fillFrame) % kFillRetry) != 0) return;

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
