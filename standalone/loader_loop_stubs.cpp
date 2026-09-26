// standalone/loader_loop_stubs.cpp
// Standalone-only stub vtables for the exe's two LoaderLoop subclasses
// (vtables @ 0x140158870 inner and @ 0x1401588d0 outer, used in this nesting
// in the original GameMain).
//
// LoaderLoop has 12 virtuals (UpdateLoop's full set, no CreateDialog).
//
// Inner vtable @ 0x140158870 (PTR_FUN_140158870) — wraps the splash chain:
//   +0x00  Destroy(self, flags)            — exe 0x140001150 (LoaderLoop::~ + free)
//   +0x08  Startup                         — thunk 0x14003A8E2 -> Runtime LoaderLoop::Startup
//   +0x10  Shutdown                        — thunk 0x14003A8DC
//   +0x18  Update(self, time)              — thunk 0x14003A8D6 -> Runtime LoaderLoop::Update
//   +0x20  Draw(self, alpha)               — thunk 0x14003A8D0 -> Runtime LoaderLoop::Draw
//   +0x28  DrawFailed                      — thunk 0x14003A90C
//   +0x30  Load(self)                      — exe 0x140001120 (PlacementTool ctor + reset loadBit)
//   +0x38  SaveRestore                     — thunk 0x14003A906
//   +0x40  OnVideoError                    — thunk 0x14003A8C4
//   +0x48  OnVideoRestart                  — thunk 0x14003A900
//   +0x50  GetResourceFilename             — exe 0x140001110 (returns "Game/Global")
//   +0x58  AllowsModChanges                — thunk 0x14003A8F4
//
// Outer vtable @ 0x1401588d0 (g_GameLoaderLoop_Vtable) — wraps the inner LoaderLoop:
//   +0x00  Destroy(self, flags)            — exe 0x140001190 (identical to inner Destroy)
//   +0x08-0x58                             — all thunks to Runtime LoaderLoop methods
//
// ENDGAME B.3.8 block LOOP: the two hand-built tables are gone. This file now
// holds the MEMBER bodies of the two real classes, `InnerLoaderLoop`
// (code/Game/Game/InnerLoaderLoop.h) and `GameLoaderLoop` (GameLoaderLoop.h),
// whose compiler vtables the two retail addresses serve (B38_RULES D5).


#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "stall_debug.h"
#include "UpdateLoop.h"
#include "LoaderLoop.h"
#include "options.h"
#include "system/Thread.h"   // ENDGAME B.3.4b System callers: System::Thread
#include "engine/UpdateLoop.h"   // -Iruntime/types: ENDGAME B.3.7 Engine loop classes
#include "engine/MainLoop.h"     // -Iruntime/types: ENDGAME B.3.7 Engine::MainLoop
#include "Game/InnerLoaderLoop.h"   // ENDGAME B.3.8 LOOP: the two LoaderLoop classes
#include "Game/GameLoaderLoop.h"
#include "Object/exe_type.h"        // DEFINE_EXE_VTABLE
#include <cstdio>
#include <cstdint>
#include <cstdlib>

namespace {

// The engine's Engine::Loader::Load(loop) spawns a background Loader thread
// (ThreadFunc -> Loader::Run -> UpdateLoop::LoadResources) that loads the loop's
// "<GetResourceFilename>Resources.rsc" and recursively resolves its ENTIRE
// dependency tree (FindOrLoad -> External::Load -> ExternalList::Serialize ->
// zlib). For the inner loop that's "Game/GlobalResources.rsc", and in the
// standalone's incomplete resource/FileSystem environment that recursive load
// NEVER terminates — the thread pegs a full CPU core forever (the root cause of
// the "menu 100% CPU"; verified non-terminating, ~1.5 cores for 600+ CPU-sec).
//
// BUT we can't simply skip it: boot DEPENDS on the Game/Global resources it
// loads (skipping it crashes boot 6/6). So this stays ENABLED by default; the
// real fix is to make the Game/Global load terminate (the standalone's resource
// path-resolution/caching, tied to the disabled Modification::SetupFileSystem,
// makes FindOrLoad never dedup -> endless re-load). Until then, the env var is
// an opt-OUT purely for investigation (expect a boot crash with it set):
//   BANISHED_SKIP_ENGINE_LOADER=1  -> skip Engine::Loader::Load
bool SkipEngineLoader() {
    return Opt().skipEngineLoader;
}

// After fn_Loader_Load spawns the engine Loader thread (Loader::Run ->
// LoadResources -> recursive External::Load = thousands of Allocate calls), the
// stub Update normally transitions IMMEDIATELY, so that thread loads the
// achievement/scenario tree CONCURRENTLY with the main thread's menu-scene build.
// HISTORICAL (true when written, NOT true now): the raw member Allocate/Free
// were then reachable without the allocator CS, so the two threads corrupted
// the free-list -> intermittent crash inside Free (Runtime 0x1800c6c00), see
// [[project_menu_heap_crash]]. Today every door into the free list is locked:
// runtime/src/system/allocator.cpp's RegisterAllocatorExports/Addresses hand
// out the composed stubs of standalone/alloc_cutover_arm.cpp for ?Allocate@/
// ?Free@ (by name and by address), and MemAlloc/MemFree lock themselves. That
// is retail's arrangement too: its Allocate call sites are an inlined
// Enter(_singleton+0x640) / Allocate / Leave (checked: operator new
// 0x18000107d/94/9f, String::SetLength 0x1800c827f/9d/a8, MemAlloc 0x1800c7a50).
// The wait below stays for loader EXCLUSIVITY (LOADER_READ_SPIN.md), not heap
// safety.
//
// Fix: block here until the loader finishes (Thread::WaitForFinish waits on the
// loader thread's completion event). The main thread is parked (not allocating)
// while the loader runs alone -> no concurrent allocation -> no free-list race.
// If the loader crashes mid-run, the standalone CrashHandler ExitProcess'es, so
// this can't deadlock. Env opt-OUT: BANISHED_SYNC_ENGINE_LOADER=0.
bool SyncEngineLoader() {
    return Opt().syncEngineLoader;
}

void WaitForEngineLoader() {
    if (!SyncEngineLoader() || !g_pLoader) return;
    printf("[EngineLoader] waiting for loader thread to finish (sync)...\n");
    reinterpret_cast<System::Thread*>(g_pLoader)->WaitForFinish();   // B.3.4 bridge
    printf("[EngineLoader] loader thread finished\n");
}

// Slot 2 Shutdown is INHERITED (ENDGAME B.3.8 LOOP): Engine::LoaderLoop::Shutdown
// (Runtime 0x1800dc280) is retail's unconditional join of the loader thread
// (WaitForSingleObject on _loader's hFinished) ahead of teardown -- the body the
// old `LoaderLoopShutdownWait` stubs re-implemented.

} // namespace

// ============================================================
// InnerLoaderLoop (vtable @ 0x140158870) — code/Game/Game/InnerLoaderLoop.h
// ============================================================
// ENDGAME B.3.8 block LOOP: the old hand-built `g_innerLoaderLoopVtable[12]`'s
// stub bodies, moved into the real class's members unchanged.

Engine::UpdateLoop* InnerLoaderLoop::DeletingDtor(unsigned flags) {
    printf("[InnerLL::Destroy] self=%p flags=%u\n", (void*)this, flags);
    // Retail 0x140001150: CALL ??1LoaderLoop@Engine@@ (import 0x140152ba8),
    // then MemFree on &1. The compiler's chain runs ~LoaderLoop -> ~UpdateLoop
    // (the _loadBit hand-back + ExternalFactory::Clean + the list cleanup).
    this->~InnerLoaderLoop();
    if (flags & 1) {
        fn_MemFreeStatic(this);
    }
    return this;
}

void InnerLoaderLoop::Startup() {
    StallDebug::Phase phase("InnerLL::Startup");
    printf("[InnerLL::Startup] self=%p next=%p loader=%p\n",
           (void*)this, (void*)_nextLoop, g_pLoader);
    if (SkipEngineLoader()) {
        printf("[InnerLL::Startup] BANISHED_SKIP_ENGINE_LOADER set — skipping "
               "Engine::Loader::Load (background loader pegs a core, but boot "
               "depends on Game/Global; expect a crash).\n");
    } else if (g_pLoader) {
        // ENDGAME B.3.7: `Engine::Loader::Load` is a (non-virtual) member of
        // the real class; the `fn_Loader_Load` name door is no longer needed.
        reinterpret_cast<Engine::Loader*>(g_pLoader)->Load(_nextLoop);        // B.3.7 bridge
        printf("[InnerLL::Startup] Loader::Load returned\n");
        WaitForEngineLoader();
    } else {
        printf("[InnerLL::Startup] WARN: skipping Loader::Load (g_pLoader null)\n");
    }
}

bool InnerLoaderLoop::Update(const System::Time& time) {
    StallDebug::Tick("InnerLL::Update");
    // Inner_Load is a noop in standalone (PlacementTool init is done lazily
    // from elsewhere), so there is no async work to wait for. Transition
    // immediately to next (ShiningRockSplash). The splash itself kicks the
    // single Runtime Engine::Loader thread for its own next-loop load.
    Engine::UpdateLoop* next = _nextLoop;
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    printf("[InnerLL::Update] self=%p time=%p next=%p mainLoop=%p -> transition\n",
           (void*)this, (const void*)&time, (void*)next, mainLoop);
    if (next && mainLoop) {
        reinterpret_cast<Engine::MainLoop*>(mainLoop)->SetNextLoop(next);    // B.3.7 bridge
    }
    _loadBit = -1;
    return false;
}

void InnerLoaderLoop::Draw(float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[InnerLL::Draw] self=%p alpha=%f\n", (void*)this, alpha);
    }
}

void InnerLoaderLoop::Load() {
    printf("[InnerLL::Load] self=%p (skipping PlacementTool init)\n", (void*)this);
    // Real impl @ 0x140001120: PlacementTool_Ctor(KeyMap, _resourceList);
    // _loadBit = -1;
    // We skip the first — PlacementTool isn't reimplemented standalone yet.
    _loadBit = -1;
}

const char* InnerLoaderLoop::GetResourceFilename() {
    // Original (FUN_140001110) returns LEA-loaded "Game/Global".
    return "Game/Global";
}

// ============================================================
// GameLoaderLoop (vtable @ 0x1401588d0) — code/Game/Game/GameLoaderLoop.h
// ============================================================
// All slots are pure thunks in the original — same shape as inner but with
// no exe-internal Load/GetResourceFilename overrides.

Engine::UpdateLoop* GameLoaderLoop::DeletingDtor(unsigned flags) {
    printf("[OuterLL::Destroy] self=%p flags=%u\n", (void*)this, flags);
    // Retail 0x140001190: the same shape as the inner loop's slot 0.
    this->~GameLoaderLoop();
    if (flags & 1) {
        fn_MemFreeStatic(this);
    }
    return this;
}

void GameLoaderLoop::Startup() {
    StallDebug::Phase phase("OuterLL::Startup");
    printf("[OuterLL::Startup] self=%p next=%p loader=%p\n",
           (void*)this, (void*)_nextLoop, g_pLoader);
    if (SkipEngineLoader()) {
        printf("[OuterLL::Startup] BANISHED_SKIP_ENGINE_LOADER set — skipping "
               "Engine::Loader::Load (see InnerLL note; expect a crash).\n");
    } else if (g_pLoader) {
        // ENDGAME B.3.7: `Engine::Loader::Load` is a (non-virtual) member of
        // the real class; the `fn_Loader_Load` name door is no longer needed.
        reinterpret_cast<Engine::Loader*>(g_pLoader)->Load(_nextLoop);        // B.3.7 bridge
        printf("[OuterLL::Startup] Loader::Load returned\n");
        WaitForEngineLoader();
    } else {
        printf("[OuterLL::Startup] WARN: skipping Loader::Load (g_pLoader null)\n");
    }
}

bool GameLoaderLoop::Update(const System::Time& time) {
    StallDebug::Tick("OuterLL::Update");
    // Outer_Load is a noop, transition immediately to InnerLL.
    Engine::UpdateLoop* next = _nextLoop;
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    printf("[OuterLL::Update] self=%p time=%p next=%p mainLoop=%p -> transition\n",
           (void*)this, (const void*)&time, (void*)next, mainLoop);
    if (next && mainLoop) {
        reinterpret_cast<Engine::MainLoop*>(mainLoop)->SetNextLoop(next);    // B.3.7 bridge
    }
    _loadBit = -1;
    return false;
}

void GameLoaderLoop::Draw(float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[OuterLL::Draw] self=%p alpha=%f\n", (void*)this, alpha);
    }
}

void GameLoaderLoop::Load() {
    printf("[OuterLL::Load] self=%p\n", (void*)this);
    _loadBit = -1;
}

// B38_RULES D5: `X::Vtable()` is the COMPILER's table. The retail addresses
// keep serving it while unconverted readers stamp them by address (the
// door-then-stamp sites in code/Game/{Dialog,Tutorial} and engine_funcs.cpp's
// g_GameLoaderLoop_Vtable).
DEFINE_EXE_VTABLE(InnerLoaderLoop, "_ZTV15InnerLoaderLoop")
DEFINE_EXE_VTABLE(GameLoaderLoop, "_ZTV14GameLoaderLoop")

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterLoaderLoopVtables() {
    RegisterExeAddr(0x140158870, reinterpret_cast<uintptr_t>(InnerLoaderLoop::Vtable()));
    RegisterExeAddr(0x1401588d0, reinterpret_cast<uintptr_t>(GameLoaderLoop::Vtable()));
}
