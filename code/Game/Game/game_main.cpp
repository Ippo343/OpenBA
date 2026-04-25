// code/Game/Game/game_main.cpp
// Clean reimpl of the original exe's GameMain (0x140041ef0).
//
// Original sequence (decompiled from Application-x64.exe):
//   1. Engine::Engine ctor on stack (1712 bytes)
//   2. Engine::Engine::Startup(cmdLine, 0x200, 0, false) -> bool
//   3. Allocate MainLoop (0xa0), Engine::MainLoop::MainLoop(ml, engine)
//   4. Allocate GameWorld (0xe0), FUN_140001050 ctor
//   5. Allocate game settings (0x34), Game_Register
//   6. FUN_140042320 -> MenuScene factory
//   7. DialogLoaderLoopT for "Dialog\Loading.rsc" (next=MenuScene)
//   8. DialogLoaderLoopT for "Dialog\ShiningRock.rsc" (next=Loading)
//   9. LoaderLoop wrapping Splash
//  10. LoaderLoop wrapping above (the game's loader)
//  11. Engine::MainLoop::Run(ml, gameLoader)
//  12. Cleanup: ExternalFactory::SetLoadBit/-1 + Clean, free settings/world,
//      MainLoop::~MainLoop + free, Engine::Engine::Shutdown, Engine::~Engine
//
// Initial stub only invokes Engine ctor/dtor so we can probe the standalone
// startup path one slot at a time. Stages will be enabled as dispatch slots
// fill in.

#include "game_main.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

// ============================================================
// Resolved Runtime DLL functions
// ============================================================
//
// Ghidra-confirmed addresses from Runtime-x64.dll:
//   0x1800dbf00 = Engine::Engine::Engine(this, void*)        ctor (174 B)
//   0x1800dbec0 = Engine::Engine::~Engine(this)              dtor
//   0x1800dd6b0 = Engine::MainLoop::MainLoop(this, Engine*)  ctor
//   0x1800dd640 = Engine::MainLoop::~MainLoop(this)          dtor
//   0x1800dd3c0 = Engine::MainLoop::Run(this, UpdateLoop*)
//
// Engine::Engine::Startup / Shutdown have not yet been pinned to a Ghidra
// address. We resolve those by mangled name.

typedef void     (*fn_Engine_Ctor_t)(void* self, void* hInstance);
typedef void     (*fn_Engine_Dtor_t)(void* self);
// Engine::Engine::Startup(this, const char* cmdLine, uint, uint, bool) -> uint (0 = fail)
typedef unsigned (*fn_Engine_Startup_t)(void* self, const char* cmdLine, unsigned p1, unsigned p2, bool p3);
typedef void     (*fn_Engine_Shutdown_t)(void* self);

// Engine::MainLoop::MainLoop(this, Engine*)
typedef void* (*fn_MainLoop_Ctor_t)(void* self, void* engine);
// Engine::MainLoop::~MainLoop(this)
typedef void  (*fn_MainLoop_Dtor_t)(void* self);

static fn_Engine_Ctor_t      s_fn_Engine_Ctor      = nullptr;
static fn_Engine_Dtor_t      s_fn_Engine_Dtor      = nullptr;
static fn_Engine_Startup_t   s_fn_Engine_Startup   = nullptr;
static fn_Engine_Shutdown_t  s_fn_Engine_Shutdown  = nullptr;
static fn_MainLoop_Ctor_t    s_fn_MainLoop_Ctor    = nullptr;
static fn_MainLoop_Dtor_t    s_fn_MainLoop_Dtor    = nullptr;

static void EnsureResolved() {
    if (s_fn_Engine_Ctor) return;

    // Mangled names verified against Application-x64.exe symbol table at
    // 0x1401B618E (Startup) and 0x1401B6170 (Shutdown).
    s_fn_Engine_Ctor = reinterpret_cast<fn_Engine_Ctor_t>(
        ResolveExport("??0Engine@0@QEAA@PEAX@Z"));
    if (!s_fn_Engine_Ctor) {
        s_fn_Engine_Ctor = reinterpret_cast<fn_Engine_Ctor_t>(
            ResolveAddr(Module::Runtime, 0x1800dbf00));
    }
    s_fn_Engine_Dtor = reinterpret_cast<fn_Engine_Dtor_t>(
        ResolveExport("??1Engine@0@QEAA@XZ"));
    if (!s_fn_Engine_Dtor) {
        s_fn_Engine_Dtor = reinterpret_cast<fn_Engine_Dtor_t>(
            ResolveAddr(Module::Runtime, 0x1800dbec0));
    }
    s_fn_Engine_Startup = reinterpret_cast<fn_Engine_Startup_t>(
        ResolveExport("?Startup@Engine@1@QEAAIPEBDII_N@Z"));
    s_fn_Engine_Shutdown = reinterpret_cast<fn_Engine_Shutdown_t>(
        ResolveExport("?Shutdown@Engine@1@QEAAXXZ"));

    // Engine::MainLoop ctor / dtor — Ghidra-confirmed addresses.
    s_fn_MainLoop_Ctor = reinterpret_cast<fn_MainLoop_Ctor_t>(
        ResolveAddr(Module::Runtime, 0x1800dd6b0));
    s_fn_MainLoop_Dtor = reinterpret_cast<fn_MainLoop_Dtor_t>(
        ResolveAddr(Module::Runtime, 0x1800dd640));

    // Note: MemAlloc/MemFree come from engine_funcs.h (fn_MemAlloc takes the
    // MemoryAllocator singleton as first arg; fn_MemFreeStatic is the static
    // one-arg form).
}

namespace Game {

// Engine instance is 0x6f8 bytes (last field at +0x6f0 + 8). Original uses
// stack alloc of size 0x6b0 — we round up generously.
struct alignas(16) EngineStorage {
    unsigned char bytes[0x700];
};

int RunGameMain(HINSTANCE hInstance, const char* cmdLine) {
    EnsureResolved();

    printf("[GameMain] Engine ctor          = %p\n", (void*)s_fn_Engine_Ctor);
    printf("[GameMain] Engine dtor          = %p\n", (void*)s_fn_Engine_Dtor);
    printf("[GameMain] Engine::Startup      = %p\n", (void*)s_fn_Engine_Startup);
    printf("[GameMain] Engine::Shutdown     = %p\n", (void*)s_fn_Engine_Shutdown);

    if (!s_fn_Engine_Ctor || !s_fn_Engine_Dtor) {
        fprintf(stderr, "[GameMain] FATAL: Engine ctor/dtor not resolved\n");
        return 1;
    }

    if (!s_fn_Engine_Startup || !s_fn_Engine_Shutdown) {
        fprintf(stderr, "[GameMain] FATAL: Engine::Startup/Shutdown not resolved\n");
        return 2;
    }

    EngineStorage engine;
    std::memset(&engine, 0, sizeof(engine));

    printf("[GameMain] Constructing Engine (storage=%p)...\n", (void*)&engine);
    s_fn_Engine_Ctor(&engine, hInstance);
    printf("[GameMain] Engine constructed.\n");

    printf("[GameMain] Engine::Startup(cmdLine=%p, 0x200, 0, false)...\n", (void*)cmdLine);
    unsigned ok = s_fn_Engine_Startup(&engine, cmdLine, 0x200, 0, false);
    printf("[GameMain] Engine::Startup returned %u\n", ok);

    if (ok != 0) {
        // Stage 3: allocate + construct + destroy a MainLoop. (No Run yet —
        // that needs an UpdateLoop chain.)
        printf("[GameMain] MainLoop ctor      = %p\n", (void*)s_fn_MainLoop_Ctor);
        printf("[GameMain] MainLoop dtor      = %p\n", (void*)s_fn_MainLoop_Dtor);
        printf("[GameMain] MemAlloc/Free      = %p / %p\n",
               (void*)fn_MemAlloc, (void*)fn_MemFreeStatic);

        if (s_fn_MainLoop_Ctor && s_fn_MainLoop_Dtor && fn_MemAlloc && fn_MemFreeStatic) {
            // The MemoryAllocator singleton was NULL when EngineFuncsInit cached
            // g_pMemAllocSingleton (Engine ctor hadn't run yet). Read it fresh
            // from the Runtime DLL .data slot now.
            void** memAllocSingletonPtr = reinterpret_cast<void**>(
                ResolveAddr(Module::Runtime, 0x1801fc718));
            void* memAllocSingleton = memAllocSingletonPtr ? *memAllocSingletonPtr : nullptr;
            printf("[GameMain] MemAllocSingleton (fresh) = %p\n", memAllocSingleton);

            // MainLoop is 0xa0 bytes, 8-byte aligned (matches original GameMain).
            void* ml = fn_MemAlloc(memAllocSingleton, 0xa0, 8);
            printf("[GameMain] MainLoop alloc = %p\n", ml);
            if (ml) {
                s_fn_MainLoop_Ctor(ml, &engine);
                printf("[GameMain] MainLoop constructed.\n");

                // TODO(stage 4): build UpdateLoop chain + MainLoop::Run.

                s_fn_MainLoop_Dtor(ml);
                fn_MemFreeStatic(ml);
                printf("[GameMain] MainLoop destroyed + freed.\n");
            }
        }

        printf("[GameMain] Engine::Shutdown...\n");
        s_fn_Engine_Shutdown(&engine);
        printf("[GameMain] Engine::Shutdown done\n");
    } else {
        fprintf(stderr, "[GameMain] Engine::Startup FAILED\n");
    }

    printf("[GameMain] Destroying Engine...\n");
    s_fn_Engine_Dtor(&engine);
    printf("[GameMain] Engine destroyed.\n");

    return ok != 0 ? 0 : 3;
}

} // namespace Game
