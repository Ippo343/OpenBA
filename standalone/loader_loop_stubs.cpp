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
// First-pass: every slot is a print-only stub. Update returns false on the first
// tick so MainLoop::Run exits cleanly. Real reimpls will land slot-by-slot.


#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "stall_debug.h"
#include "UpdateLoop.h"
#include "LoaderLoop.h"
#include "options.h"
#include "system/Thread.h"   // ENDGAME B.3.4b System callers: System::Thread
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
// MemoryAllocator::Allocate AND the internal Free both mutate the boundary-tag
// free-list without a shared lock (only MemFree wraps Free with the CS), so the
// two threads corrupt the free-list -> intermittent crash inside Free
// (Runtime 0x1800c6c00). See [[project_menu_heap_crash]].
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

// ============================================================
// Inner LoaderLoop stubs (vtable @ 0x140158870)
// ============================================================

void Inner_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[InnerLL::Destroy] self=%p flags=%u\n", (void*)self, flags);
    // Skip the LoaderLoop::~LoaderLoop chain (UpdateLoop dtor + ExternalList free).
    // Acceptable leak — process exits immediately after Run returns.
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Inner_Startup(UpdateLoop* self) {
    StallDebug::Phase phase("InnerLL::Startup");
    LoaderLoop* ll = reinterpret_cast<LoaderLoop*>(self);
    printf("[InnerLL::Startup] self=%p next=%p loader=%p fn=%p\n",
           (void*)self, (void*)ll->m_nextLoop, g_pLoader, (void*)fn_Loader_Load);
    if (SkipEngineLoader()) {
        printf("[InnerLL::Startup] BANISHED_SKIP_ENGINE_LOADER set — skipping "
               "Engine::Loader::Load (background loader pegs a core, but boot "
               "depends on Game/Global; expect a crash).\n");
    } else if (fn_Loader_Load && g_pLoader) {
        fn_Loader_Load(g_pLoader, ll->m_nextLoop);
        printf("[InnerLL::Startup] Loader::Load returned\n");
        WaitForEngineLoader();
    } else {
        printf("[InnerLL::Startup] WARN: skipping Loader::Load (fn or g_pLoader null)\n");
    }
}

void Inner_Shutdown(UpdateLoop* self) {
    printf("[InnerLL::Shutdown] self=%p\n", (void*)self);
}

bool Inner_Update(UpdateLoop* self, void* time) {
    StallDebug::Tick("InnerLL::Update");
    // Inner_Load is a noop in standalone (PlacementTool init is done lazily
    // from elsewhere), so there is no async work to wait for. Transition
    // immediately to next (ShiningRockSplash). The splash itself kicks the
    // single Runtime Engine::Loader thread for its own next-loop load.
    LoaderLoop* ll = reinterpret_cast<LoaderLoop*>(self);
    UpdateLoop* next = ll->m_nextLoop;
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    printf("[InnerLL::Update] self=%p time=%p next=%p mainLoop=%p -> transition\n",
           (void*)self, time, (void*)next, mainLoop);
    if (next && fn_MainLoop_SetNextLoop && mainLoop) {
        fn_MainLoop_SetNextLoop(mainLoop, next);
    }
    self->m_loadBit = -1;
    return false;
}

void Inner_Draw(UpdateLoop* self, float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[InnerLL::Draw] self=%p alpha=%f\n", (void*)self, alpha);
    }
}

void Inner_DrawFailed(UpdateLoop* self) {
    printf("[InnerLL::DrawFailed] self=%p\n", (void*)self);
}

void Inner_Load(UpdateLoop* self) {
    printf("[InnerLL::Load] self=%p (skipping PlacementTool init)\n", (void*)self);
    // Real impl @ 0x140001120: PlacementTool_Ctor(KeyMap, self->m_resourceList);
    // self->m_loadBit = -1;
    // We skip both — PlacementTool isn't reimplemented standalone yet.
    self->m_loadBit = -1;
}

void Inner_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[InnerLL::SaveRestore] self=%p save=%d state=%p\n",
           (void*)self, save, state);
}

void Inner_OnVideoError(UpdateLoop* self) {
    printf("[InnerLL::OnVideoError] self=%p\n", (void*)self);
}

void Inner_OnVideoRestart(UpdateLoop* self) {
    printf("[InnerLL::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Inner_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original (FUN_140001110) returns LEA-loaded "Game/Global".
    return "Game/Global";
}

bool Inner_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    // Original thunks to Runtime LoaderLoop::AllowsModChanges → true.
    return true;
}

void* g_innerLoaderLoopVtable[12] = {
    reinterpret_cast<void*>(&Inner_Destroy),               // +0x00
    reinterpret_cast<void*>(&Inner_Startup),               // +0x08
    reinterpret_cast<void*>(&Inner_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Inner_Update),                // +0x18
    reinterpret_cast<void*>(&Inner_Draw),                  // +0x20
    reinterpret_cast<void*>(&Inner_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Inner_Load),                  // +0x30
    reinterpret_cast<void*>(&Inner_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Inner_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Inner_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Inner_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Inner_AllowsModChanges),      // +0x58
};

// ============================================================
// Outer LoaderLoop stubs (vtable @ 0x1401588d0)
// ============================================================
// All slots are pure thunks in the original — same shape as inner but with
// no exe-internal Load/GetResourceFilename overrides.

void Outer_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[OuterLL::Destroy] self=%p flags=%u\n", (void*)self, flags);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Outer_Startup(UpdateLoop* self) {
    StallDebug::Phase phase("OuterLL::Startup");
    LoaderLoop* ll = reinterpret_cast<LoaderLoop*>(self);
    printf("[OuterLL::Startup] self=%p next=%p loader=%p fn=%p\n",
           (void*)self, (void*)ll->m_nextLoop, g_pLoader, (void*)fn_Loader_Load);
    if (SkipEngineLoader()) {
        printf("[OuterLL::Startup] BANISHED_SKIP_ENGINE_LOADER set — skipping "
               "Engine::Loader::Load (see InnerLL note; expect a crash).\n");
    } else if (fn_Loader_Load && g_pLoader) {
        fn_Loader_Load(g_pLoader, ll->m_nextLoop);
        printf("[OuterLL::Startup] Loader::Load returned\n");
        WaitForEngineLoader();
    } else {
        printf("[OuterLL::Startup] WARN: skipping Loader::Load (fn or g_pLoader null)\n");
    }
}

void Outer_Shutdown(UpdateLoop* self) {
    printf("[OuterLL::Shutdown] self=%p\n", (void*)self);
}

bool Outer_Update(UpdateLoop* self, void* time) {
    StallDebug::Tick("OuterLL::Update");
    // Outer_Load is a noop, transition immediately to InnerLL.
    LoaderLoop* ll = reinterpret_cast<LoaderLoop*>(self);
    UpdateLoop* next = ll->m_nextLoop;
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    printf("[OuterLL::Update] self=%p time=%p next=%p mainLoop=%p -> transition\n",
           (void*)self, time, (void*)next, mainLoop);
    if (next && fn_MainLoop_SetNextLoop && mainLoop) {
        fn_MainLoop_SetNextLoop(mainLoop, next);
    }
    self->m_loadBit = -1;
    return false;
}

void Outer_Draw(UpdateLoop* self, float alpha) {
    static int draws = 0;
    if (draws++ < 3) {
        printf("[OuterLL::Draw] self=%p alpha=%f\n", (void*)self, alpha);
    }
}

void Outer_DrawFailed(UpdateLoop* self) {
    printf("[OuterLL::DrawFailed] self=%p\n", (void*)self);
}

void Outer_Load(UpdateLoop* self) {
    printf("[OuterLL::Load] self=%p\n", (void*)self);
    self->m_loadBit = -1;
}

void Outer_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[OuterLL::SaveRestore] self=%p save=%d state=%p\n",
           (void*)self, save, state);
}

void Outer_OnVideoError(UpdateLoop* self) {
    printf("[OuterLL::OnVideoError] self=%p\n", (void*)self);
}

void Outer_OnVideoRestart(UpdateLoop* self) {
    printf("[OuterLL::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Outer_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original thunks straight to Runtime LoaderLoop::GetResourceFilename
    // (returns nullptr in the base — stub matches "no resource group").
    return nullptr;
}

bool Outer_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    return true;
}

void* g_outerLoaderLoopVtable[12] = {
    reinterpret_cast<void*>(&Outer_Destroy),               // +0x00
    reinterpret_cast<void*>(&Outer_Startup),               // +0x08
    reinterpret_cast<void*>(&Outer_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Outer_Update),                // +0x18
    reinterpret_cast<void*>(&Outer_Draw),                  // +0x20
    reinterpret_cast<void*>(&Outer_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Outer_Load),                  // +0x30
    reinterpret_cast<void*>(&Outer_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Outer_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Outer_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Outer_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Outer_AllowsModChanges),      // +0x58
};

} // namespace

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterLoaderLoopVtables() {
    RegisterExeAddr(0x140158870, reinterpret_cast<uintptr_t>(g_innerLoaderLoopVtable));
    RegisterExeAddr(0x1401588d0, reinterpret_cast<uintptr_t>(g_outerLoaderLoopVtable));
}

