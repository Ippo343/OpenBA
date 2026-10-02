// code/Game/Game/chunked_scene_loader.cpp — see chunked_scene_loader.h for design.


#include "chunked_scene_loader.h"
#include "stall_debug.h"
#include "Game/menudialogloopt_funcs.h"
#include "Game/MenuDialogLoopT.h"
#include <cstdio>
#include <cstdint>

namespace ChunkedSceneLoader {

namespace {

MenuDialogLoopT* g_menuDltSelf = nullptr;
MenuDialogLoopT* g_armedSelf = nullptr;
MenuDialogLoopT_PhaseBState g_state{};
bool   g_busy = false;
int    g_tickCount = 0;
double g_initStartMs = 0.0;
double g_chunkSumMs = 0.0;
double g_chunkMaxMs = 0.0;

}  // namespace

void Init(MenuDialogLoopT* menuDltSelf) {
    if (!menuDltSelf) {
        printf("[ChunkedSceneLoader] Init: null menuDltSelf — skipping\n");
        std::fflush(stdout);
        return;
    }
    if (g_busy && g_menuDltSelf == menuDltSelf) {
        // Idempotent — same loop already initialized.
        return;
    }
    if (g_busy) {
        printf("[ChunkedSceneLoader] Init: WARN: prior load still busy "
                    "(self=%p, new=%p) — overwriting\n",
                    (void*)g_menuDltSelf, (void*)menuDltSelf);
    }

    g_menuDltSelf  = menuDltSelf;
    g_busy         = false;
    g_tickCount    = 0;
    g_chunkSumMs   = 0.0;
    g_chunkMaxMs   = 0.0;
    g_initStartMs  = StallDebug::NowMs();

    printf("[ChunkedSceneLoader] Init: self=%p — running LoadSceneA1 (small "
                "allocs + MenuScene.rsc load)\n", (void*)menuDltSelf);
    std::fflush(stdout);

    bool a1Ok;
    {
        StallDebug::Phase a1Phase("ChunkedSceneLoader::Init LoadSceneA1");
        a1Ok = MenuDialogLoopT_LoadSceneA1(menuDltSelf);
    }
    if (!a1Ok) {
        printf("[ChunkedSceneLoader] Init: LoadSceneA1 returned false — no "
                    "scene background; skipping PhaseB\n");
        std::fflush(stdout);
        g_busy = false;
        return;
    }

    // Re-read menuScene/scene from MenuDLT struct fields (set by LoadSceneA1).
    void* menuScene = menuDltSelf->_menuScene;   // +0xb90
    void* scene = menuDltSelf->_scene;           // +0xba0

    MenuDialogLoopT_PhaseB_InitState(&g_state, menuDltSelf, menuScene, scene);
    if (g_state.totalCount <= 0) {
        printf("[ChunkedSceneLoader] Init: PhaseB has no work "
                    "(totalCount=%d) — running A3 immediately\n",
                    g_state.totalCount);
        std::fflush(stdout);
        MenuDialogLoopT_LoadSceneA3(menuDltSelf);
        g_busy = false;
        return;
    }

    g_busy = true;
    printf("[ChunkedSceneLoader] Init: ready — PhaseB has %d elements to "
                "process (chunked across Update ticks)\n",
                g_state.totalCount);
    std::fflush(stdout);
}

void Arm(MenuDialogLoopT* menuDltSelf) {
    g_armedSelf = menuDltSelf;
    printf("[ChunkedSceneLoader] Arm: self=%p — LoadSceneA1 deferred until the "
                "splash is on screen\n", (void*)menuDltSelf);
    std::fflush(stdout);
}

bool IsArmed() {
    return g_armedSelf != nullptr;
}

void RunArmedInit() {
    if (!g_armedSelf) return;
    MenuDialogLoopT* self = g_armedSelf;
    g_armedSelf = nullptr;   // clear FIRST so a failing Init can't re-enter
    Init(self);
}

int Tick(int maxElements) {
    if (!g_busy) return 0;

    double startMs = StallDebug::NowMs();
    int processed = MenuDialogLoopT_PhaseB_TickChunk(&g_state, maxElements);
    double endMs = StallDebug::NowMs();
    double durMs = endMs - startMs;
    g_chunkSumMs += durMs;
    if (durMs > g_chunkMaxMs) g_chunkMaxMs = durMs;
    ++g_tickCount;

    bool done = (g_state.index >= g_state.totalCount);
    // Log first + last tick always; every 5th tick in between.
    if (g_tickCount == 1 || done || (g_tickCount % 5) == 0) {
        printf("[ChunkedSceneLoader] Tick #%d: processed=%d cumProgress=%d/%d "
                    "(%.1f%%) chunkDur=%.2fms\n",
                    g_tickCount, processed, g_state.index, g_state.totalCount,
                    g_state.totalCount > 0
                        ? 100.0 * (double)g_state.index / (double)g_state.totalCount
                        : 0.0,
                    durMs);
        std::fflush(stdout);
    }

    if (done) {
        // PhaseC particles — small, runs in one shot (~5ms).
        {
            StallDebug::Phase a3Phase("ChunkedSceneLoader::Tick LoadSceneA3");
            MenuDialogLoopT_LoadSceneA3(g_menuDltSelf);
        }
        double totalMs = endMs - g_initStartMs;
        printf("[ChunkedSceneLoader] DONE: ticks=%d sumChunkMs=%.2f "
                    "maxChunkMs=%.2f initToDoneMs=%.2f (PhaseB %d elements)\n",
                    g_tickCount, g_chunkSumMs, g_chunkMaxMs, totalMs,
                    g_state.totalCount);
        std::fflush(stdout);
        g_busy = false;
    }

    return processed;
}

bool IsBusy() {
    return g_busy;
}

}  // namespace ChunkedSceneLoader

