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
#include "Game/inputbindings.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

#ifdef STANDALONE_EXE
// Defined in standalone/address_registry.cpp. Publishes the live KeyMap*
// into the fake exref chain at slot 0x140151738 so that triple-deref reads
// (g_pKeyMapObjectExref) reach the real KeyMap inside Input::Configuration.
extern "C" void SetStandaloneKeyMap(void* keyMap);

// Defined in standalone/menudialogloopt_stubs.cpp. Loads
// Dialog/SpriteSheet.rsc + constructs the "White" / "SaveLoadEmpty" Sprites
// into KeyMap+0xa0/+0xa8/+0xb8 (mirror of PlacementTool_Ctor's last segment).
// Idempotent. Called early in RunGameMain so SplashDLT::Stub_Startup's fade
// trigger finds a populated KeyMap+0xa8 and the overlay actually renders.
extern "C" void StandaloneEnsureSpriteSheetLoaded();
#endif

#ifdef STANDALONE_EXE
// Defined in code/Game/Game/filesystem_setup.cpp. Relativizes the engine
// FileSystem's resource directories so resource paths resolve relative to the
// working dir (install dir) instead of long absolute "C:\GOG Games\..." strings.
// Must run AFTER Engine::Startup (FileSystem allocated + SetupDirectories done)
// and BEFORE the first resource load. See that file's header for rationale.
namespace Game { extern "C" void FileSystem_MakePathsRelative(); }
namespace Game { extern "C" void FileSystem_PinDataDir(); }

// Defined in standalone/main.cpp. Walks _firstTypeInfo for each registered
// type, invokes its factory, validates vtable + ctor-set field defaults.
extern "C" void RunStartupSelfTests();

#ifdef STANDALONE_EXE
// Defined in standalone/reg_component_nature.cpp — fills the component type-ID
// .rdata globals once Object::Factory::Startup has assigned the real CRC ids.
extern "C" void FillComponentTypeIdGlobals_nature();
// Phase-1 self-contained-sim live bring-ups — fill each component's type-id global.
extern "C" void FillComponentTypeIdGlobals_fruit();
extern "C" void FillComponentTypeIdGlobals_spawn();
extern "C" void FillComponentTypeIdGlobals_tombstone();
// Phase 2 Keystone B3 — RawMaterialDescription type-id global (0x1401c7cb8) for ca520.
extern "C" void FillTypeIdGlobals_actionchild();
// Phase 2 Keystone B tail — Work-tail type-id globals (trade=private CitizenAIComponent;
// fire=InteractComponent c7900; serialize=Pasture/Animal ids).
extern "C" void FillTypeIdGlobals_worktail_trade();
extern "C" void FillTypeIdGlobals_worktail_fire();
extern "C" void FillTypeIdGlobals_worktail_serialize();
#endif

// Standalone-only chunked driver for MenuDLT::LoadScene. Init runs
// MenuDialogLoopT_LoadSceneA1 (~10ms — small allocs + one .rsc load) inline
// and captures the chunked PhaseB iteration state. We invoke this during
// boot so the heavy iteration can be ticked across the Loading splash's
// Update frames (driven from splash_loop_stubs.cpp Stub_Update). Boot is
// non-rendering, so the synchronous A1 cost is invisible.
#include "chunked_scene_loader.h"
#endif

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

// ============================================================
// Partial reimpl of Engine::Engine::Shutdown (Runtime 0x1800db4f0).
//
// Mirrors steps 1–13 of the stock Shutdown body — write back configuration,
// platform-shutdown each subsystem, FreeLibrary the renderer plugins, save
// settings to the registry, close .pkg files. We deliberately stop short of
// the dtor+free chain (Achievement/Input/Audio/Video/Application dtors,
// MemoryAllocator::Shutdown, CoUninitialize) because those crash silently
// in standalone — most likely tripping over leaked dialog/UI objects that
// MenuDLT's destroy stub doesn't release.
//
// The OS reclaims memory and uninitialises COM on process exit, so skipping
// the dtor chain only leaks until ExitProcess. Once standalone has a real
// MenuDLT::Destroy that walks DialogInstance/UserInterface/Audio::Scene,
// the leaks should shrink to nothing and we can call the stock function.
// ============================================================
namespace {

template<typename Fn>
static Fn ResolveFn(const char* mangled) {
    return reinterpret_cast<Fn>(ResolveExport(mangled));
}

static void ShutdownEngineStaged(void* engineP) {
    using std::uintptr_t;
    uintptr_t base = reinterpret_cast<uintptr_t>(engineP);
    auto load_ptr = [&](uintptr_t off) -> void* {
        return *reinterpret_cast<void**>(base + off);
    };

    printf("[Shutdown] partial Engine::Shutdown (steps 1-13)\n");
    fflush(stdout);

    // 1. Write the "skip-reset" bool into Video::Configuration's first byte.
    if (void* vc = load_ptr(0x6b8)) {
        *reinterpret_cast<bool*>(vc) =
            *reinterpret_cast<uint8_t*>(base + 0x6f0) == 0;
    }

    // 2. AchievementManager::RemoveAll
    if (void* am = load_ptr(0x6e8)) {
        if (auto fn = ResolveFn<void(*)(void*)>(
                "?RemoveAll@AchievementManager@Achievement@@QEAAXXZ"))
            fn(am);
    }

    // 3. Input::Device::PlatformShutdown
    if (void* d = load_ptr(0x6e0)) {
        if (auto fn = ResolveFn<void(*)(void*)>(
                "?PlatformShutdown@Device@Input@@AEAAXXZ"))
            fn(d);
    }

    // 4. Input::Configuration::Save (skip the FUN_180053440 inner-buffer free)
    if (void* cfg = load_ptr(0x6d8)) {
        if (auto save = ResolveFn<void(*)(void*)>(
                "?Save@Configuration@Input@@QEAAXXZ"))
            save(cfg);
    }

    // 5. Audio sub-object teardown — vtable[+0x12] / [+0x2] of two children
    // at audio_device+0xa90 / +0xa88, then null those slots.
    if (void* audioDev = load_ptr(0x6d0)) {
        uintptr_t ad = reinterpret_cast<uintptr_t>(audioDev);
        if (void* sub1 = *reinterpret_cast<void**>(ad + 0xa90)) {
            void** vt = *reinterpret_cast<void***>(sub1);
            reinterpret_cast<void(*)(void*)>(vt[0x90/8])(sub1);
        }
        *reinterpret_cast<void**>(ad + 0xa90) = nullptr;
        if (void* sub2 = *reinterpret_cast<void**>(ad + 0xa88)) {
            void** vt = *reinterpret_cast<void***>(sub2);
            reinterpret_cast<void(*)(void*)>(vt[0x10/8])(sub2);
        }
        *reinterpret_cast<void**>(ad + 0xa88) = nullptr;
    }

    // 6. Audio::Configuration::Save
    if (void* cfg = load_ptr(0x6c8)) {
        if (auto save = ResolveFn<void(*)(void*)>(
                "?Save@Configuration@Audio@@QEAAXXZ"))
            save(cfg);
    }

    // 7. Video::Device::PlatformShutdown + FreeLibrary on the three plugin
    // HMODULE slots at video_device+0xb0/+0xc8/+0xe0.
    if (void* vdev = load_ptr(0x6c0)) {
        if (auto ps = ResolveFn<void(*)(void*)>(
                "?PlatformShutdown@Device@Video@@AEAAXXZ"))
            ps(vdev);
        uintptr_t pv = reinterpret_cast<uintptr_t>(vdev) + 0xb0;
        for (int i = 0; i < 3; ++i, pv += 24) {
            HMODULE m = *reinterpret_cast<HMODULE*>(pv);
            if (m) FreeLibrary(m);
            *reinterpret_cast<uint64_t*>(pv) = 0;
            *reinterpret_cast<uint64_t*>(pv + 8) = 0;
            *reinterpret_cast<uint64_t*>(pv + 16) = 0;
        }
    }

    // 8. Video::Configuration::Save
    if (void* cfg = load_ptr(0x6b8)) {
        if (auto save = ResolveFn<void(*)(void*)>(
                "?Save@Configuration@Video@@QEAAXXZ"))
            save(cfg);
    }

    // 9. Application::PlatformShutdown — DestroyWindow + UnregisterClass.
    if (void* app = load_ptr(0x690)) {
        if (auto fn = ResolveFn<void(*)(void*)>(
                "?PlatformShutdown@Application@1@AEAAXXZ"))
            fn(app);
    }

    // 10. FreeLibrary on the Modification DLL slot (Runtime 0x1801fc6d0).
    if (HMODULE* slot = reinterpret_cast<HMODULE*>(
            ResolveAddr(Module::Runtime, 0x1801fc6d0))) {
        if (*slot) FreeLibrary(*slot);
        *slot = nullptr;
    }

    // 11. Modification::SaveList
    if (void* mod = load_ptr(0x680)) {
        if (auto fn = ResolveFn<void(*)(void*)>(
                "?SaveList@Modification@IO@@QEAAXXZ"))
            fn(mod);
    }

    // 12. Persistence::WriteSettings (gated on engine[0x6f1] == 0)
    if (void* per = load_ptr(0x678)) {
        if (*reinterpret_cast<uint8_t*>(base + 0x6f1) == 0) {
            if (auto fn = ResolveFn<void(*)(void*)>(
                    "?WriteSettings@Persistence@IO@@QEAAXXZ"))
                fn(per);
        }
    }

    // 13. FileSystem::ClosePackages
    if (void* fs = load_ptr(0x670)) {
        if (auto fn = ResolveFn<void(*)(void*)>(
                "?ClosePackages@FileSystem@IO@@QEAAXXZ"))
            fn(fs);
    }

    // (14+ deliberately omitted — see header comment.)
    printf("[Shutdown] done\n"); fflush(stdout);
}

} // anonymous namespace

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
    // Engine::Startup somehow disconnects stdout/stderr from the log files
    // that WinMain freopen()'d (cause TBD — Engine::Startup itself doesn't call
    // AllocConsole/FreeConsole/SetStdHandle/freopen per Ghidra). Re-redirect
    // them so subsequent printf reaches the log.
    freopen("standalone_debug.log", "a", stdout);
    freopen("standalone_error.log", "a", stderr);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("[GameMain] Engine::Startup returned %u\n", ok);

    // On Engine::Startup failure, dump System::Error::_errorString
    // (Runtime 0x1801fc720, 8KB) — Engine::Startup's failure paths each call
    // System::Error::SetText with a unique tag (e.g., "VideoDevice: ..."),
    // so the string identifies exactly which step bombed.
    if (ok == 0) {
        const char* err = reinterpret_cast<const char*>(
            ResolveAddr(Module::Runtime, 0x1801fc720));
        if (err && err[0]) {
            printf("[GameMain] System::Error string after Startup failure:\n");
            for (int i = 0; i < 1024 && err[i]; i++) {
                if (i == 0) printf("    | ");
                fputc(err[i], stdout);
                if (err[i] == '\n' && err[i+1]) printf("    | ");
            }
            fputc('\n', stdout);
        } else {
            printf("[GameMain] System::Error string is empty after Startup failure\n");
        }
    }

    // Probe: dump Factory tree state after Engine::Startup.
    // Engine::Engine::Startup -> Object::Factory::Startup walks _firstTypeInfo
    // and assigns CRC32-derived typeIds (overwriting the 0xFFFFFFFF ctor default).
    // If the walk succeeded, every entry now has a non-FFFFFFFF typeId.
    {
        uintptr_t headAddr = ResolveAddr(Module::Runtime, 0x1801fc6e0);
        struct Probe { uint32_t typeId; uint32_t pad; const char* name; const char* parent;
                       const char* abstract; void* f; void* ip; Probe* next; };
        Probe* head = headAddr ? *reinterpret_cast<Probe**>(headAddr) : nullptr;
        int n = 0, withId = 0;
        for (Probe* p = head; p && n < 1000; p = p->next, n++) {
            if (p->typeId != 0xFFFFFFFF) withId++;
        }
        printf("[GameMain] Post-Startup _firstTypeInfo: %d entries, %d have non-FF typeId\n",
               n, withId);
        // Dump typeId for first 10 entries — sanity-check CRC32 algorithm.
        int s = 0;
        for (Probe* p = head; p && s < 10; p = p->next, s++) {
            printf("[GameMain]   typeId=0x%08X name=%s\n", p->typeId,
                   p->name ? p->name : "?");
        }
    }

#ifdef STANDALONE_EXE
    // Now that Object::Factory::Startup has assigned the real CRC type ids (the
    // 0xFFFFFFFF ctor sentinel is overwritten only here, NOT at
    // InvokeAllRegistrations), populate the component type-ID .rdata globals that
    // the shared FindComponent accessors compare against. Filling earlier stored
    // the sentinel. See reg_component_nature.cpp / standalone/main.cpp.
    FillComponentTypeIdGlobals_nature();
    FillComponentTypeIdGlobals_fruit();
    FillComponentTypeIdGlobals_spawn();
    FillComponentTypeIdGlobals_tombstone();
    FillTypeIdGlobals_actionchild();   // RawMaterialDescription tid 0x1401c7cb8 (ca520 + latent fix)
    FillTypeIdGlobals_worktail_trade();    // Phase 2 B tail — TradeWork c8a0 private CitizenAIComponent id
    FillTypeIdGlobals_worktail_fire();     // Phase 2 B tail — FireWork InteractComponent id (0x1401c7900)
    FillTypeIdGlobals_worktail_serialize();// Phase 2 B tail — PastureWork Pasture/Animal ids
#endif

    if (ok != 0) {
#ifdef STANDALONE_EXE
        // Relativize the engine FileSystem resource directories. This was a
        // CANDIDATE crash mitigation (session 2e) that has been RULED OUT and is
        // ACTIVELY HARMFUL: with it on, the menu either crashes or stalls (never
        // draws) — A/B (session 5): FS-rel ON => crash/stall in every run; FS-rel
        // OFF => menu draws. The real menu-crash fix is the FindOrLoad serialize
        // lock (InstallFindOrLoadWatch). So this is now DEFAULT OFF; opt IN with
        // BANISHED_FS_RELATIVE=1 only for path-shortening experiments.
        {
            const char* e = getenv("BANISHED_FS_RELATIVE");
            if (e && *e && e[0] != '0') {
                printf("[GameMain] BANISHED_FS_RELATIVE set — relativizing FileSystem "
                       "paths (experimental; known to break the menu)\n");
                Game::FileSystem_MakePathsRelative();
            }
            // Session 4 menu-heap-crash fix-test (env BANISHED_PIN_DATADIR):
            // pin the dir Strings' refcounts so the data-dir String can't erode
            // to 0 and get freed-then-AddRef'd. No-op unless the env is set.
            Game::FileSystem_PinDataDir();
        }
#endif

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

#ifdef STANDALONE_EXE
            // Validate per-class factory registrations end-to-end now that
            // Engine::Engine::Engine has populated the MemoryAllocator
            // singleton. See standalone/main.cpp for the implementation.
            RunStartupSelfTests();
#endif

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

#ifdef STANDALONE_EXE
                // Wire the live Input::KeyMap into our fake exref chain so
                // triple-deref reads of g_pKeyMapObjectExref reach the real
                // KeyMap. Engine::Startup constructed Input::Configuration at
                // engine[0x6d8]; its first field is the KeyMap pointer
                // (Input::Configuration::GetKeyMap is `return *(KeyMap**)this`).
                void* inputCfg = *reinterpret_cast<void**>(
                    reinterpret_cast<uintptr_t>(&engine) + 0x6d8);
                void* keyMap = inputCfg ? *reinterpret_cast<void**>(inputCfg)
                                        : nullptr;
                printf("[GameMain] InputConfig=%p  -> KeyMap=%p\n",
                       inputCfg, keyMap);
                SetStandaloneKeyMap(keyMap);

                // Populate KeyMap with default bindings (mirror of the original
                // PlacementTool_Ctor's call to FUN_1400992f0). Must happen
                // before any dialog opens — OptionsDialog buttons poll
                // KeyMap::Pressed every frame and will fault on an empty map.
                Game::RegisterDefaultInputBindings();

                // Load the input localization StringTables that
                // PlacementTool_Ctor (FUN_140099ff0) normally loads. These map
                // input ID -> "Move View Forward" etc., used by OptionsDialog's
                // input bind list.
                Game::LoadInputNameTables();

                // Load Dialog/SpriteSheet.rsc + construct the "White" /
                // "SaveLoadEmpty" Sprites into KeyMap+0xa8/+0xb8 (mirror of
                // PlacementTool_Ctor's last segment). Required BEFORE MainLoop::
                // Run because SplashDLT::Stub_Startup triggers fullscreen-fade
                // reveals that read KeyMap+0xa8 — without the sprite, the fade
                // STATE machine still runs but no overlay actually renders, so
                // splash transitions hard-cut as visible flashes.
                ::StandaloneEnsureSpriteSheetLoaded();
#endif

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

                        // Pre-load each splash's own .rsc on the main thread BEFORE
                        // MainLoop::Run starts. Boot is non-rendering (no swap chain
                        // Present yet from gameplay code), so blocking ~56ms here is
                        // invisible — whereas blocking those same ms inside
                        // SplashDLT::Stub_Startup happens after Engine::Startup has
                        // already presented at least one frame, so the user sees a
                        // brief flash of the DX11 initial backbuffer color before
                        // the splash appears. Loading the .rsc up front populates
                        // m_dialogExternal so Stub_Startup's fallback synchronous
                        // load is skipped.
                        if (fn_DialogLoaderLoopT_Load) {
                            if (shiningRockLoop) {
                                printf("[GameMain] pre-loading ShiningRock.rsc\n");
                                fn_DialogLoaderLoopT_Load(shiningRockLoop);
                            }
                            if (loadingLoop) {
                                printf("[GameMain] pre-loading Loading.rsc\n");
                                fn_DialogLoaderLoopT_Load(loadingLoop);
                            }
                        }

                        // Pre-run MenuDLT's LoadSceneA1 here in boot. Boot is
                        // non-rendering (no swap chain Present yet from
                        // gameplay code), so the ~10ms A1 cost is invisible.
                        // After this, MenuDLT struct fields (+0xb90 menuScene,
                        // +0xb98 renderer, etc.) are populated; the Loading
                        // splash's Stub_Update will Tick(500) the chunked
                        // PhaseB iteration each frame so the splash keeps
                        // animating while the heavy 12K-15K static-mesh loop
                        // runs in chunks. (Driving Init from the splash
                        // crashed inside VideoDX11 — see splash_loop_stubs.cpp.)
#ifdef STANDALONE_EXE
                        if (menuLoop) {
                            printf("[GameMain] pre-running ChunkedSceneLoader::Init for MenuDLT\n");
                            ChunkedSceneLoader::Init(menuLoop);
                        }
#endif

                        // Stage 5e: wrap the splash chain in two LoaderLoop layers.
                        // Inner LoaderLoop wraps shiningRockLoop, outer LoaderLoop
                        // wraps inner. Both use fn_LoaderLoop_Ctor with delay 0.0f
                        // (matches FUN_140041ef0). Vtables come from our stubs in
                        // standalone/loader_loop_stubs.cpp.
                        auto buildLoaderLoop = [&](UpdateLoop* next, uintptr_t vtableAddr,
                                                   const char* tag) -> UpdateLoop* {
                            if (!next || !fn_LoaderLoop_Ctor) return nullptr;
                            UpdateLoop* ll = reinterpret_cast<UpdateLoop*>(
                                fn_MemAlloc(memAllocSingleton, 0x48, 4));
                            printf("[GameMain] %s alloc = %p\n", tag, (void*)ll);
                            if (!ll) return nullptr;

                            uint64_t zeroDelay = 0;
                            fn_Time_Ctor_Float(&zeroDelay, 0.0f);
                            fn_LoaderLoop_Ctor(ll, next, &zeroDelay);

                            void** vt = reinterpret_cast<void**>(
                                ResolveAddr(Module::Exe, vtableAddr));
                            *(void***)ll = vt;
                            printf("[GameMain] %s ctor done, vtable=%p, next=%p\n",
                                   tag, (void*)ll->vtable, (void*)next);
                            return ll;
                        };

                        UpdateLoop* innerLoader = shiningRockLoop
                            ? buildLoaderLoop(shiningRockLoop, 0x140158870, "InnerLL")
                            : nullptr;
                        UpdateLoop* outerLoader = innerLoader
                            ? buildLoaderLoop(innerLoader, 0x1401588d0, "OuterLL")
                            : nullptr;

                        UpdateLoop* topLoop = outerLoader ? outerLoader
                                            : innerLoader ? innerLoader
                                            : shiningRockLoop ? shiningRockLoop
                                            : loadingLoop ? loadingLoop
                                            : menuLoop;
                        const char* topTag = outerLoader ? "OuterLL"
                                           : innerLoader ? "InnerLL"
                                           : shiningRockLoop ? "ShiningRockDLT"
                                           : loadingLoop ? "LoadingDLT"
                                           : "MenuDLT (fallback)";
                        if (topLoop) {
                            printf("[GameMain] Calling MainLoop::Run with %s top "
                                   "loop = %p...\n", topTag, (void*)topLoop);
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

        // BISECT: stepped reimpl of Engine::Engine::Shutdown (Runtime 0x1800db4f0).
        // The stock function silently terminates the process somewhere inside —
        // not even our SetUnhandledExceptionFilter sees a fault. We mirror its
        // body in stages with fflush between each so the last logged step is
        // the one that killed us.
        ShutdownEngineStaged(&engine);
    } else {
        fprintf(stderr, "[GameMain] Engine::Startup FAILED\n");
    }

    printf("[GameMain] Destroying Engine...\n"); fflush(stdout);
    s_fn_Engine_Dtor(&engine);
    printf("[GameMain] Engine destroyed.\n"); fflush(stdout);

    return ok != 0 ? 0 : 3;
}

} // namespace Game
