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
#include "UpdateLoop.h"
#include "Game/game.h"
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
// Engine::MainLoop::Run(this, UpdateLoop*)
typedef void  (*fn_MainLoop_Run_t)(void* self, UpdateLoop* loop);

static fn_Engine_Ctor_t      s_fn_Engine_Ctor      = nullptr;
static fn_Engine_Dtor_t      s_fn_Engine_Dtor      = nullptr;
static fn_Engine_Startup_t   s_fn_Engine_Startup   = nullptr;
static fn_Engine_Shutdown_t  s_fn_Engine_Shutdown  = nullptr;
static fn_MainLoop_Ctor_t    s_fn_MainLoop_Ctor    = nullptr;
static fn_MainLoop_Dtor_t    s_fn_MainLoop_Dtor    = nullptr;
static fn_MainLoop_Run_t     s_fn_MainLoop_Run     = nullptr;

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

    // Engine::MainLoop ctor / dtor / Run — Ghidra-confirmed addresses.
    s_fn_MainLoop_Ctor = reinterpret_cast<fn_MainLoop_Ctor_t>(
        ResolveAddr(Module::Runtime, 0x1800dd6b0));
    s_fn_MainLoop_Dtor = reinterpret_cast<fn_MainLoop_Dtor_t>(
        ResolveAddr(Module::Runtime, 0x1800dd640));
    s_fn_MainLoop_Run = reinterpret_cast<fn_MainLoop_Run_t>(
        ResolveAddr(Module::Runtime, 0x1800dd3c0));

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

                // Stage 5a: allocate the Game/KeyMap container (0xe0 bytes, 4-byte
                // aligned) and run its ctor. Original GameMain follows this with
                // Game_Register on a settings struct; we skip Game_Register for now
                // and just zero-init settings to the same hardcoded defaults.
                void* gameObj = fn_MemAlloc(memAllocSingleton, 0xe0, 4);
                printf("[GameMain] Game alloc      = %p\n", gameObj);
                if (gameObj) {
                    Game_Ctor(gameObj);
                    printf("[GameMain] Game_Ctor done; KeyMap singleton -> %p\n",
                           g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
                }

                // Settings struct (0x34 bytes). Defaults from original GameMain.
                int32_t* settings = reinterpret_cast<int32_t*>(
                    fn_MemAlloc(memAllocSingleton, 0x34, 4));
                printf("[GameMain] Settings alloc  = %p\n", (void*)settings);
                if (settings) {
                    settings[0] = 100;          // UIScale
                    settings[1] = 1;            // AutosaveInterval
                    settings[2] = 100;          // StatusIconOpacity
                    settings[3] = 1;            // StatusIconEnable + bools at +0x15..0x17
                    settings[4] = 100;          // StatusIconScale
                    settings[5] = 0x1010001;    // packed bools (EdgeScroll, ShowCelcius, ShowWeather, AutoSlow)
                    settings[6] = 1;            // AutoPause
                    *reinterpret_cast<int64_t*>(&settings[7])  = 0;  // LastOption + LastTerrain
                    settings[9] = 0;            // LastMapSize
                    settings[10] = 1;           // LastClimate
                    *reinterpret_cast<int64_t*>(&settings[11]) = 1;  // LastStart + LastDisaster
                    if (g_pGameSettingsPtr) {
                        *g_pGameSettingsPtr = settings;
                    }
                }

                // Stage 5b: build a MenuDialogLoopT (exe-internal subclass of
                // Engine::DialogLoopT, drives Dialog\MainMenu.rsc).
                //   1. MemAlloc(0xc00, 8)
                //   2. Construct String "Dialog\\MainMenu.rsc"
                //   3. MenuDialogLoopT_Ctor (DialogLoopT base + vtable patch +
                //      Random + Audio::Scene + zero-init extra fields)
                //   4. Destroy String
                //
                // Stage 5c: wrap MenuDLT in the Loading.rsc DialogLoaderLoopT
                // (vtable @ 0x140158800). Mirrors the inner branch of the
                // original GameMain (FUN_140041ef0):
                //   - MemAlloc(0xb60, 4)
                //   - Time(0.5f), String "Dialog\\Loading.rsc"
                //   - Engine::DialogLoaderLoopT::DialogLoaderLoopT(self, &str, menuLoop, &time)
                //   - patch vtable to g_GameLoadDialogLoopT_Vtable
                //   - init colors at +0x188/+0x18c (0xff000000), zero +0x190,
                //     System::Random at +0x194, zero +0xb58 (random text timer),
                //     zero m_finished at +0x40
                //
                // Both Update stubs return false so Run exits on the first tick.
                if (s_fn_MainLoop_Run && g_MenuDialogLoopT_Vtable && fn_DialogLoopT_Ctor &&
                    fn_AudioScene_Ctor && fn_Random_Default_Ctor && fn_String_Ctor_CStr &&
                    fn_String_Dtor && fn_DialogLoaderLoopT_Ctor && fn_Time_Ctor_Float &&
                    g_GameLoadDialogLoopT_Vtable) {

                    UpdateLoop* menuLoop = reinterpret_cast<UpdateLoop*>(
                        fn_MemAlloc(memAllocSingleton, 0xc00, 8));
                    printf("[GameMain] MenuDLT alloc = %p (vtable=%p)\n",
                           (void*)menuLoop, (void*)g_MenuDialogLoopT_Vtable);

                    if (menuLoop) {
                        // Construct the dialog filename string.
                        uint8_t menuStr[8] = {};
                        fn_String_Ctor_CStr(menuStr, "Dialog\\MainMenu.rsc");

                        MenuDialogLoopT_Ctor(menuLoop, menuStr);
                        printf("[GameMain] MenuDLT ctor done, vtable=%p\n",
                               (void*)menuLoop->vtable);

                        fn_String_Dtor(menuStr);

                        // Helper: build a splash DialogLoaderLoopT (0xb60 bytes,
                        // vtable @ 0x140158800) wrapping `next`. Mirrors the
                        // identical splash setup blocks in FUN_140041ef0.
                        auto buildSplashDLT = [&](const char* filename, float delaySec,
                                                  UpdateLoop* next, const char* tag)
                            -> UpdateLoop* {
                            UpdateLoop* dlt = reinterpret_cast<UpdateLoop*>(
                                fn_MemAlloc(memAllocSingleton, 0xb60, 4));
                            printf("[GameMain] %s alloc = %p (vtable=%p)\n",
                                   tag, (void*)dlt, (void*)g_GameLoadDialogLoopT_Vtable);
                            if (!dlt) return nullptr;

                            uint64_t delayTime = 0;
                            fn_Time_Ctor_Float(&delayTime, delaySec);

                            uint8_t fileStr[8] = {};
                            fn_String_Ctor_CStr(fileStr, filename);

                            fn_DialogLoaderLoopT_Ctor(dlt, fileStr, next, &delayTime);

                            uintptr_t lp = reinterpret_cast<uintptr_t>(dlt);
                            *(void***)lp = g_GameLoadDialogLoopT_Vtable;
                            *reinterpret_cast<uint32_t*>(lp + 0x188) = 0xff000000;  // color1
                            *reinterpret_cast<uint32_t*>(lp + 0x18c) = 0xff000000;  // color2
                            *reinterpret_cast<uint8_t*>(lp + 0x190) = 0;            // fade-done
                            fn_Random_Default_Ctor(reinterpret_cast<void*>(lp + 0x194));
                            *reinterpret_cast<uint64_t*>(lp + 0xb58) = 0;           // text timer
                            *reinterpret_cast<uint8_t*>(lp + 0x40) = 0;             // m_finished

                            fn_String_Dtor(fileStr);
                            printf("[GameMain] %s ctor done, vtable=%p, next=%p, "
                                   "delay=%.2fs\n",
                                   tag, (void*)dlt->vtable, (void*)next, delaySec);
                            return dlt;
                        };

                        // Stage 5c: Loading.rsc splash wrapping menuLoop.
                        UpdateLoop* loadingLoop = buildSplashDLT(
                            "Dialog\\Loading.rsc", 0.5f, menuLoop, "LoadingDLT");

                        // Stage 5d: ShiningRock.rsc splash wrapping LoadingDLT.
                        // Original GameMain uses DAT_140172174 = 2.0f for delay.
                        UpdateLoop* shiningRockLoop = loadingLoop
                            ? buildSplashDLT("Dialog\\ShiningRock.rsc", 2.0f,
                                             loadingLoop, "ShiningRockDLT")
                            : nullptr;

                        UpdateLoop* topLoop = shiningRockLoop ? shiningRockLoop
                                            : loadingLoop ? loadingLoop
                                            : menuLoop;
                        if (topLoop) {
                            printf("[GameMain] Calling MainLoop::Run with %s top "
                                   "loop = %p...\n",
                                   shiningRockLoop ? "ShiningRockDLT"
                                   : loadingLoop ? "LoadingDLT" : "MenuDLT (fallback)",
                                   (void*)topLoop);
                            s_fn_MainLoop_Run(ml, topLoop);
                            printf("[GameMain] MainLoop::Run returned "
                                   "(top loop destroyed by Run; inner loops "
                                   "leak — Update stub returns false on first "
                                   "tick before transitioning)\n");
                        }
                    }
                } else {
                    printf("[GameMain] WARN: missing deps for splash chain — "
                           "Run=%p, MenuVT=%p, SplashVT=%p, DLLT_Ctor=%p, "
                           "DLT_Ctor=%p, AudioScene=%p, Random=%p, Time=%p\n",
                           (void*)s_fn_MainLoop_Run, (void*)g_MenuDialogLoopT_Vtable,
                           (void*)g_GameLoadDialogLoopT_Vtable,
                           (void*)fn_DialogLoaderLoopT_Ctor,
                           (void*)fn_DialogLoopT_Ctor, (void*)fn_AudioScene_Ctor,
                           (void*)fn_Random_Default_Ctor, (void*)fn_Time_Ctor_Float);
                }

                if (settings) {
                    if (g_pGameSettingsPtr) *g_pGameSettingsPtr = nullptr;
                    fn_MemFreeStatic(settings);
                    printf("[GameMain] Settings freed.\n");
                }
                if (gameObj) {
                    if (g_pKeyMapSingleton) *g_pKeyMapSingleton = nullptr;
                    fn_MemFreeStatic(gameObj);
                    printf("[GameMain] Game freed.\n");
                }

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
