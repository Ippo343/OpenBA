# LOADRESEARCH -- a static map of the three load routes (2026-10-14)

Read-only research for the loading-speed phase (`NEXT_SESSION.md` §2.5: "the first step is a timed census of the
load route ... before any design"). Nothing was built or run for this sheet. The only numbers in it are copied from
existing gate logs of the **debug** exe (`docs/runtime_port/phase_d/runs/EYEFIX/{newgame,resave1}/`, large map,
faithful profile, warm file cache). The perf exe strips every `printf` (`src/app/log_strip.h`), so its logs carry no
`[BootTime]` / `[Stall]` lines and no perf-exe number exists yet. **Every ms figure below is debug and only says where
to look.**

## 0. Threads that take part

| thread | created | runs during a load |
|---|---|---|
| main | WinMain | `Engine::Startup`, every loop's `Startup` / `Update` / `Draw`, the splash and loading screens, the port's chunked menu-scene build, `GameUpdateLoop::Startup` and the first frames |
| loader | `Engine::LoaderLoop::_loader` (class static, `src/engine/engine/loops.cpp:1022`, 256 KB stack) | `Loader::Run` `loops.cpp:964`: `UpdateLoop::LoadResources` then the loop's slot-6 `Load` (`GameUpdateLoop::Load` = the whole world build / save restore) |
| persistence worker | `IO::Persistence::Startup` | the save-list scan and the save-file read (`RtIo_PersistenceWorker_EnqueueLoad`, `src/engine/save/saveload_update.cpp:384/793`) |
| path worker(s) | `Pathfinder_Seed` (`src/game/Game/pathfinder_seed.cpp`) | idle until the game runs (`[PfSeed] ... ASYNC worker-thread`) |
| render / cull helpers | `ext.render_pipe`, `ext.gather_threads` | in game only, off by default; not on any load route |

The two resource entry points (`ExternalFactory::FindOrLoad`, `External::Load`) are not thread-safe and are serialized
by one recursive lock, `ResLoadGuard` (`src/app/res_load_lock.h`, taken in `src/engine/io/external_path.cpp:333`).
Package reads go through ONE Win32 handle per `.pkg` with `SetFilePointer` + `ReadFile` per read
(`src/engine/io/filesystem_openfile.cpp:289-327`). Any parallel resource loading has to live below or beside that.

## 1. The routes, stage by stage

### 1A. Process start -> main menu (debug, `EYEFIX/newgame`, t from `StallDebug::Initialize`)

| # | stage | where | thread | reads / computes | t (ms) |
|---|---|---|---|---|---|
| 1 | WinMain, settings probe, harness guard, `BootState::Start`, RuntimeInit | `src/app/main.cpp:988`, `:829`, `:1033` | main | `settings.cfg` | 0 -> 14 |
| 2 | `Engine::Startup` part 1: allocator, Configuration, Timer, **FileSystem::Startup** (`LoadPackages` = `ReadPackfile` of every `*.pkg` directory, `filesystem_startup.cpp:108`, `filesystem_packages.cpp:622`), Persistence, **Modification::RefreshList** (mod scan), Factory::Startup, InternalFactory / ExternalFactory ctors, **Application::PlatformStartup** (SDL window), Video::Configuration, **EnumerateVideoModes**, Audio / Input configuration | `src/engine/engine/engine_lifecycle.cpp:439-631` | main | `data0.pkg` 132 MB / `data1.pkg` 40 MB / 2 patch pkgs (directories only), `WinData`, `<Documents>/Banished/Mods` | 14 -> 674 (**660**, unsplit) |
| 3 | `Video::Device::PlatformStartup`: DXGI enum, ShowWindow, D3D11CreateDevice (93 ms), swap chain | `engine_lifecycle.cpp:639`, `src/render/dx11/devicedata.cpp:1146-1295` | main | -- | 674 -> 769 |
| 4 | Audio / Input device startup, AchievementManager | `engine_lifecycle.cpp:649-700` | main | -- | 769 -> 815 |
| 5 | MainLoop + Game ctors, MenuDLT and splash ctors | `src/game/Game/game_main.cpp:417-600` | main | -- | 815 -> 829 |
| 6 | pre-load `ShiningRock.rsc` + `Loading.rsc` (port-only, to avoid a blank-window flash) | `game_main.cpp:601-611` | main | 2 dialog resources | 829 -> 921 (91) |
| 7 | `ChunkedSceneLoader::Arm(menuLoop)` (port-only), `ml->Run(OuterLL)` | `game_main.cpp:635`, `:684` | main | -- | 921 |
| 8 | **OuterLL (GameLoaderLoop) Startup -> Loader::Load(InnerLL) + `WaitForEngineLoader`**: the loader runs `Game/GlobalResources.rsc` (LoadResources) and `InnerLoaderLoop::Load` = `GameContainer_Init` (`src/game/Game/loader_loop_stubs.cpp:161/187`); **the main thread blocks** (STANDALONE DIVERGENCE `syncEngineLoader`, default on, `loader_loop_stubs.cpp:102`, `:240`) | `loader_loop_stubs.cpp:240`, `loops.cpp:386`, `:964` | loader; main parked | GlobalResources and its whole dependency tree | 922 -> 1771 (**848**) |
| 9 | InnerLL Startup -> Loader::Load(ShiningRockDLT) (+ wait); ShiningRock splash Startup kicks the loader for LoadingDLT (its `.rsc` already loaded) | `loader_loop_stubs.cpp:138`, `src/game/Game/splash_loop_stubs.cpp:166` | main / loader | -- | 1786 -> 1818 |
| 10 | window revealed on the first presented splash frame | `src/app/boot_window.cpp:69` | main | -- | **1924** |
| 11 | ShiningRock splash: **2.0 s minimum hold** (retail `DAT_140172174`) + fade-out; behind it, once the loader is idle and the reveal is half done: `MenuDialogLoopT::LoadSceneA1` (261 ms, `Game/MenuScene/SceneN.rsc` FindOrLoad, `menudialogloopt.cpp:429`) then PhaseB 15,490 static meshes in 31 ticks of 500 (`chunked_scene_loader.cpp`, `splash_loop_stubs.cpp:303-345`), then A3 particles | `splash_loop_stubs.cpp:279-480` | main (port-only: retail loads the menu scene on the loader thread inside `MenuDialogLoopT::Load`) | menu scene, static meshes | 1818 -> 4608 (menu scene done at 2850; the rest is the 2.0 s floor + 0.27 s fade) |
| 12 | Loading splash: Startup kicks the loader for MenuDLT (`MenuDialogLoopT::Load` `menudialogloopt_stubs.cpp:496`: menu dialog, `Audio/Music/Menu.rsc`, `Game/MenuResources.rsc` via LoadResources); **0.5 s minimum hold** + fade | `splash_loop_stubs.cpp`, `loops.cpp:541` | main / loader | menu resources | 4608 -> 5518 |
| 13 | MenuDLT Startup (dialog, cursor) -> first menu frame | `menudialogloopt_stubs.cpp:123`, `:315` | main | -- | 5518 -> **5524 = menu interactive** |

Two package mounts happen on this route: `FileSystem::Startup` (row 2) and the first `Loader::Load` of a loop whose
`AllowsModChanges()` is true (the menu): `Loader::Load` sets `_dirty` after `RefreshList` and
`Modification::SetupFileSystem` (`src/engine/io/modification_setupfilesystem.cpp:307`) then walks every loaded External
twice (`GetCompiledPath` + a path-id tree) around `ClosePackages` / `LoadPackages`.

### 1B. Menu "Start" -> first interactive game frame (new game, large map)

| # | stage | where | thread | reads / computes | t (ms) |
|---|---|---|---|---|---|
| 1 | NewGameDialog OK -> `StartNewGame` builds GameUpdateLoop + GameLoadDialogLoopT (delay **1.0 s**) + GameLoaderLoop, queued on the menu loop; menu fades out | `src/game/Dialog/newgamedialog.cpp:650-720` | main | -- | ~6700 |
| 2 | transition: MenuDLT Shutdown + delete (`~UpdateLoop`: `ExternalFactory::Clean`, `[ResSet] Clean visited=12682`), OuterLL Startup -> Loader::Load(GameLoadDLT) + sync wait (loading dialog .rsc) | `src/engine/engine/mainloop_run.cpp:210-250`, `loader_loop_stubs.cpp:240` | main / loader | Loading dialog | 6774 -> 6781 |
| 3 | GameLoadDLT Startup: dialog, reveal fade, **kicks the loader for GameUpdateLoop** | `splash_loop_stubs.cpp:166`, `loops.cpp:541` | main | -- | 6810 |
| 4 | **Loader: `LoadResources` -> `Game/GameResources.rsc`** (`src/engine/engine/updateloop_loadresources.cpp:170`, `GameUpdateLoop::GetResourceFilename` `gameupdateloop.cpp:2341`) plus every enabled mod's `Resources.rsc` | `loops.cpp:1001` | loader | ~2,274 of the route's 5,694 `[ExtLoad]` calls (menu-only route = 3,420) | unmeasured |
| 5 | **Loader: `GameUpdateLoop::Load`** (`src/game/Game/gameupdateloop_load.cpp:689`): Ext lock, Random, Renderer, UI, scheduler, calendar, `Environment.rsc` + weather bind, Graphics / Audio Scene, AnimationManager, SunLight, SaveLoad, sky mesh (`:1000`), event string tables, achievement reset | `gameupdateloop_load.cpp:689-1107` | loader | a handful of FindOrLoads | unmeasured |
| 6 | **worldgen** `WorldGeometry::BuildTerrain` (`src/game/Game/worldgen.cpp:697`) -> `BuildFromHeightfield` (`:517`): f390 pipeline (`:217`: base heightmap e5d0; coastline / ranges / paths; rivers / stamp; **droplet erosion** cc080 `worldgen_f390_erosion.cpp:202`; carve; smooth x2) on a 1024x1024 grid; int16 quantize; `wg_BlendTexture_cf440` (splat); `UploadHeightImage`; `BuildTerrainMesh`; `BuildWaterMesh` (48k verts / 251k idx) | `gameupdateloop_load.cpp:1123` | loader | seed-driven compute + 3 texture / buffer creations | unmeasured |
| 7 | town coarse index, minimap arrays (512^2), **`MapTileGrid_Build`** (`maptilegrid.cpp:71`): classify 262,144 cells through `GetTerrainPos`, `FillZones` (`:81`), `Pathfinder_Seed` (`:82`, dim^2 A* nodes), `Region_Seed` (`:83`, region graph + pair cache), shore water-access loop | `gameupdateloop_load.cpp:1150-1176` | loader | compute | unmeasured |
| 8 | `CharManager_Seed`, start camera, **`StartTown_SpawnEntities`** (`starttown.cpp:5022`): 8 structures, 21 citizens, **41,642 natural resources**, 1,661 environment entities, simple pool, 8 livestock (`[startTown::spawn]` lines) | `gameupdateloop_load.cpp:1188-1211` | loader | template clones, scene AddObject, cell marking | unmeasured |
| 9 | path selection, scenario instance, camera, global scene objects, audio unpause, `s_worldBuiltLoop` store | `gameupdateloop_load.cpp:1318-1456` | loader | -- | -- |
| 10 | loading screen holds while the loader is busy (`EngineLoader::IsBusy`), the 1.0 s floor runs and `GameUpdateLoop_IsWorldBuilt`; the loading tips cycle every 2 s off `_loadProgress` (worldgen stages 1..5, `worldgen.cpp:214`); then fade-out (~0.27 s) | `splash_loop_stubs.cpp:279-480` | main (Sleep 14 ms per Update, `:351`) | -- | 6810 -> **28490** (loading screen 20.4 s logical, 21.7 s wall, debug) |
| 11 | transition: GameLoadDLT Shutdown (joins the loader), delete (`[ResSet] Clean visited=18376 evicted=4`), **`GameUpdateLoop::Startup`** (`gameupdateloop.cpp:1532`: UI refs, cursor, ProfessionList startup, HUD dialogs: Toolbar, sprites ...) | `mainloop_run.cpp:222-250` | main | HUD dialog resources (cached) | unmeasured |
| 12 | first `Update` (first sim step) and **first `Draw`**: `Minimap::PaintTerrain` once (`gameupdateloop.cpp:1337`, `minimap_paint.cpp:98`: off-screen 512^2 RT, top-down render), first use of every shader / material by the driver | `gameupdateloop.cpp:525`, `:1257` | main | -- | unmeasured |

### 1C. Menu "Load" -> first interactive game frame (save)

| # | stage | where | thread | reads / computes |
|---|---|---|---|---|
| 1 | Load dialog: save list + per-save info jobs (`persistence_savelist_job.cpp`, `persistence_saveinfo_job.cpp`) | SaveLoad dialog | main + persistence worker | `Save/*.sav` headers |
| 2 | pick + Load: `Save::SaveLoad::UpdateLoad` (`src/engine/save/saveload_update.cpp:649`): GetSaveGameInfo, **every mod disabled and re-enabled from the save's list, `mods->_dirty = 1` unconditionally**, then the file read into a `SavedState` on the worker (`:785-793`), spinner on the dialog | `saveloaddialog.cpp:603` | main + persistence worker | the whole `.sav` (Gate.sav: 9.3 MB compressed image, 23.4 MB raw) |
| 3 | `loadFlag`: GameUpdateLoop(savedState) + GameLoadDLT (delay 1.0 s) + GameLoaderLoop | `saveloaddialog.cpp:648-700` | main | -- |
| 4 | OuterLL `Loader::Load`: because `_dirty` is set, **`SetupFileSystem` remounts every package** (third mount of the process) | `loops.cpp:386-430`, `modification_setupfilesystem.cpp:307` | main (inside the sync wait) | all `.pkg` directories again |
| 5 | Loader: `LoadResources` (Game/GameResources) then `GameUpdateLoop::Load` up to step 30 as 1B.5, then the LOAD-FROM-SAVE arm: `CharManager_Seed`, **`SaveRestore(false, state)`** (decompress block 1 with the package LZ codec, Serialize every class; `Map::Serialize`'s tail runs `FillZones` + `Pathfinder_Seed` + `Region_Seed`, `src/game/Game/gamegrid_serialize.cpp:298-307`), the gate-only roundtrip check (`gameupdateloop_serialize.cpp:854`, returns at once unless `Opt().saveTest`) | `gameupdateloop_load.cpp:1287-1300` | loader | the decompressed save |
| 6-7 | as 1B.10-12 (loading screen 4.08 s logical on the debug resave route, `EYEFIX/resave1`, which includes the roundtrip re-save) | | | |

## 2. Serial vs overlapped; who drives the screen

- **Everything heavy is on ONE thread at a time.** Boot: rows 2-6 serial on main; row 8 the loader works while main
  is parked in `WaitForEngineLoader` (so the window is not even shown until 1.9 s); the menu scene is built on main
  only after the loader is idle (`splash_loop_stubs.cpp:315`: "Waiting for the loader here ... RESTORES retail's
  exclusivity"). New game / save: the loader does all of LoadResources + the world build / restore; main only draws
  the loading screen (Update sleeps 14 ms, `splash_loop_stubs.cpp:351`; the engine's `LoaderLoop::Update` does the
  same through `SleepFourteen`, `loops.cpp:730`). Nothing in a load runs on more than one core.
- **Overlap that exists:** the ShiningRock 2.0 s floor hides the menu scene build (done at 2.85 s, floor ends 4.6 s);
  the 1.0 s game-load floor is far below the world build; the save read happens behind the Load dialog's spinner.
- **Screen / progress:** `GameLoadDialogLoopT` (`src/game/Game/GameLoadDialogLoopT.h`, bodies in
  `splash_loop_stubs.cpp`) is every splash and loading screen: Update gates the transition on loader idle + floor +
  world built + fade-out complete + chunked loader done (`:400-475`); Draw publishes `_elapsedTicks` to the shader
  time constants (the spinner) and the UI (`:490`). "Progress" is only `_loadProgress` 1..5 from worldgen
  (`worldgen.cpp:214`), which gates the 2-second tip cycle; there is no bar. The floors are retail constants
  (2.0 s / 0.5 s at `game_main.cpp:580-588`, 1.0 s at `newgamedialog.cpp:706` / `saveloaddialog.cpp:676`); each
  splash adds ~0.27 s of fade (logical elapsed 2.267 s vs 2.0, 0.767 vs 0.5).
- MainLoop is fixed-step (`mainloop_frame.cpp:324-444`, accumulator clamp at `:341`): the floors are counted in
  steps, so they are logical seconds, ~wall seconds while frames are short.

## 3. Timing hooks that exist, and where `[LoadTime]` stamps go

**Existing:** `StallDebug` (`src/diag/stall_debug.{h,cpp}`): `Mark` prints `[BootTime] <what> +delta (t=abs)`,
`Phase` prints `[Stall] BEGIN/END ... duration`, `Tick` prints gaps over 16 ms; clock = `Platform::PerfCounter`
since `StallDebug::Initialize` (`main.cpp:1033`). Marks exist in `main.cpp:830-902`, `game_main.cpp:323-612`,
`devicedata.cpp:1146-1295`, `boot_window.cpp:69`; Phases in `loader_loop_stubs.cpp:139/241`,
`splash_loop_stubs.cpp:167-344`, `menudialogloopt_stubs.cpp:124-184`, `chunked_scene_loader.cpp:55/133`.
**All of it is `printf`, so the perf / release exe prints none of it.** Others: `[BootState] RUNNING (Engine::Startup
returned at Ns)` (1 s resolution, stderr, `boot_state_watchdog.cpp:379`); `[ExtLoad] calls=` and its counters at
exit only (`src/engine/object/external_load.cpp:415`); `[ResSet] Clean` per loop delete; `[ChunkedSceneLoader] DONE
... initToDoneMs`; `SplashDLT::Update ... elapsed= delay=` at each splash transition; `ph_fillzones_ms` and the
`ph_*` columns are stress-harness only (`src/diag/stress_harness.cpp:1068-1079`); the sampler
(`src/diag/win/sampler.cpp`) samples **only the main thread**, so the loader thread, where new game and save load
spend their time, is invisible to it.

**Proposed `[LoadTime]` stamps** (one helper beside `StallDebug`, writing `fprintf(stderr)` so the perf exe keeps
it, same clock as `[BootTime]`, each line `[LoadTime] <stage> t=<ms> d=<ms since previous> tid=<main|loader>
ext=<ExtLoad calls so far>`; always on, no flag, no Opt field; the line count is ~40 per game):

| # | boundary | file:line |
|---|---|---|
| 1 | process t0 | `src/app/main.cpp:1033` (`StallDebug::Initialize`) |
| 2 | `Engine::Startup` enter / after `fileSystem->Startup` / after `RefreshList` / after `application->PlatformStartup` / after `EnumerateVideoModes` / after `_videoDevice->PlatformStartup` / return | `src/engine/engine/engine_lifecycle.cpp:439, 493, 515, 553, 599, 639, 700` (splits the unexplained 660 ms) |
| 3 | `MainLoop::Run` entry | `src/game/Game/game_main.cpp:684` |
| 4 | every loop transition: after `next->Startup()`, naming the new loop (its `GetResourceFilename()` or the dialog file) and the Startup duration | `src/engine/engine/mainloop_run.cpp:250` (one stamp covers OuterLL, InnerLL, both splashes, Menu, GameLoadDLT, GameUpdateLoop) |
| 5 | `Loader::Load` remount: around `SetupFileSystem` | `src/engine/engine/loops.cpp:417-419` |
| 6 | loader thread: `Run` start, after `LoadResources`, after `Load` | `loops.cpp:1001, 1006, 1012` |
| 7 | `GameUpdateLoop::Load` sub-stages: after step 30 (the ctor block), `BuildTerrain` done, `MapTileGrid_Build` done, `StartTown_SpawnEntities` done, `SaveRestore` done, EXIT | `src/game/Game/gameupdateloop_load.cpp:1107, 1123, 1176, 1211, 1293, 1456` |
| 8 | worldgen stages for free: `wg_PublishLoadProgress(stage)` | `src/game/Game/worldgen.cpp:214` (+ `:544, 547, 548, 555` for splat / upload / mesh / water) |
| 9 | `MapTileGrid_Build` inner: classify done, FillZones, PfSeed, RegionSeed | `src/game/Game/maptilegrid.cpp:76, 81, 82, 83` |
| 10 | menu scene: `ChunkedSceneLoader` done | `src/game/Game/chunked_scene_loader.cpp:133` (already measured; make it stderr) |
| 11 | save read done (the worker's completion seen by `UpdateLoad`) and `loadFlag` handled | `src/engine/save/saveload_update.cpp:793`, `src/game/Dialog/saveloaddialog.cpp:648` |
| 12 | `GameUpdateLoop::Startup` enter / return | `src/game/Game/gameupdateloop.cpp:1532` |
| 13 | first `GameUpdateLoop::Draw` incl. `PaintTerrain`, and the next 3 frame times | `gameupdateloop.cpp:1257, 1337` |
| 14 | first `MenuDialogLoopT::Update` (menu interactive) | `src/game/Game/menudialogloopt_stubs.cpp:315` |

Rows 2, 4, 5 and 6 are in faithful engine files: a stamp there is one call with no effect on state (the
`devicedata.cpp` Marks already set that precedent); the coordinator decides whether DIRECTION §2's "no new
instruments" admits it or whether rows 2/4/6 move to the game-side Startup / Load overrides instead. A loader-thread
sample (the existing sampler pointed at `_loader`'s handle under the existing `BANISHED_SAMPLE`) would answer §5
questions 3-4 directly; same ruling needed.

## 4. Ranked hypotheses and the speedup classes

Classes: (a) cache / redundant-work removal, byte-identical; (b) parallel independent work; (c) I/O; (d) changes sim
state = class B/C, avoid. Constraints for (a)-(c): the faithful bodies untouched, the speedup an ext arm at a seam
(DESIGN §3), resave golden `971aaec2cf95` and `worldcmp.sh` unchanged, and `[ExtLoad] calls=5694` unchanged (it
counts `External::Load` calls, so a cache must sit BELOW `External::Load`, at the stream / package level, or the
gate invariant moves).

1. **New-game world build on the loader thread is the big one** (debug loading screen 19-22 s across three gates vs
   4 s for a save of a grown town). Inside it, by size of work: `StartTown_SpawnEntities` (43k entities from
   templates); worldgen f390 on a 1024^2 grid (erosion droplets, two smooth passes, carve); `MapTileGrid_Build`
   (262k `GetTerrainPos` classifications, FillZones, the region build); splat texture and the terrain / water meshes.
   - (b) **byte-identical candidates**: the classify pass (each cell a pure function of the finished heightmap),
     the int16 quantize, `wg_BlendTexture_cf440` and the smooth / carve passes IF each reads only a source buffer and
     writes its own cell (to verify per body), `BuildTerrainMesh` per chunk. A worker pool on a 4-core part; the
     result must be bit-identical (no float reassociation across threads), proved by `worldcmp.sh`.
   - (d) **not parallel**: droplet erosion (sequential `System::Random` and in-place mutation of the shared height
     field: order IS the result), the entity spawn (RNG order, entity ids, list order, scene insertion order),
     FillZones' flood fill. Approximate or reordered versions change the world: class B at best; avoid.
   - (a) look for repeated work in the spawn: per-entity template resolution and `FindOrLoad` of the same template
     (cached already by the factory, but each lookup expands paths, `external_path.cpp:333-470`, possibly with
     `GetFileAttributesW` probes); per-entity cell marking. Only a profile of the loader thread can rank these.
2. **Resource loading, 5,694 `External::Load` calls per new game** (boot GlobalResources alone 848 ms debug, with
   the main thread parked). Per resource: path expansion (string building, packfile-index AVL lookups, a
   `GetFileAttributesW` probe when the packfile probe misses, `external_path.cpp:430-470`), `OpenFile`, then
   `CompressedStream` reads in 32 KB blocks, each a `SetFilePointer` + `ReadFile` on the shared pkg handle plus the
   bespoke LZ decoder (`compressed_stream_read.cpp:249`, `src/engine/system/compression.cpp:242`, which also tests a
   branch-counter pointer per op), then Serialize and, for textures / shaders, D3D11 creation (BC formats uploaded
   as-is, `imagebufferdata.cpp:556`; shaders are precompiled bytecode, `shaderprograms.cpp:143/209`: no compile).
   - (c) map the four packages read-only once and serve block reads from the mapping (removes two syscalls per
     32 KB block and the seek state on the shared handle); a cold-start read-ahead of the two big packages on a
     worker at process start (pure OS cache warming, no state).
   - (b) a **block prefetch / decompress cache below `CompressedStream`**: once `GameResources.rsc`'s list is known,
     workers decompress the blocks of the files it names into a cache the loader consumes; `External::Load` order,
     counts and object creation stay on the loader thread, so `calls=5694` holds. Biggest structural win if
     decompression is a large share; worthless if Serialize dominates. Measure first.
   - (b) **boot**: GlobalResources (row 8) and the menu loads could start their I/O + decompress earlier, beside
     `Engine::Startup`'s device creation (the packages are mounted at `engine_lifecycle.cpp:493`, before the device),
     as the same prefetch cache.
3. **Package mounts: three per save load, two per boot** (`FileSystem::Startup`; the menu loop's dirty remount;
   `UpdateLoad`'s unconditional `_dirty = 1` remount). Each is `ReadPackfile` on every package (directory decode,
   index tree rebuild) plus two walks of every loaded External with `GetCompiledPath`. (a) an ext arm "skip the
   remount when the enabled mod list and order equal the mounted one" is byte-identical in outcome (the same packages,
   the same index) -- but the remount also re-registers externals against the clean bit (`loops.cpp:417-419`
   `_loadEnabled`), so its side effects need reading before it is claimed redundant (memory rule: register functions
   carry side effects). Size unknown until row 2/5 stamps exist.
4. **Splash floors are fixed time**: ShiningRock 2.0 s + Loading 0.5 s + ~0.27 s fade each; game load 1.0 s floor.
   Boot-to-menu was 5.5 s debug with ~3.6 s of it inside the two splashes; once rows 2 and 8 shrink, the floors are
   most of boot. Shortening them is presentation, not sim (class A), but it removes retail's logo hold: the user's
   call, flagged, not proposed.
5. **`Engine::Startup` before the device, 660 ms debug, unsplit.** Suspects: `LoadPackages`, `RefreshList` (mods
   directory scan), `EnumerateVideoModes` (DXGI mode lists can take 100+ ms), SDL window creation, allocator arena
   setup. (b) device creation and package mounting are independent and could overlap on two threads, but the order
   of `Engine::Startup` is retail's and the window / device must stay on main; only stamps can say if it is worth it.
6. **First frames after a load**: `GameUpdateLoop::Startup` (HUD dialogs), `PaintTerrain` on the first Draw, and
   the GPU driver compiling every pipeline on first use (the NVIDIA compiler's exceptions are what made gdb hitch,
   `reference_gdb_launch_hitches_nvidia_compiler.md`). (b) class A: warm the shaders / input layouts on the loader
   thread with a draw-less pass, or run PaintTerrain's render while the loading screen is up. Measure first-frame
   time before anything.
7. **Save restore**: decompression of the 23 MB block 1 (same LZ codec), Serialize of every entity, then the map
   tail (FillZones, PfSeed, RegionSeed). (b) decompress block 1 on a worker into memory while the loader is still in
   `LoadResources` (the SavedState is complete before the loader starts) -- byte-identical. Serialize itself is
   order-bound: (d).
8. **Debug-only costs** (not what players feel): thousands of `printf` / `fprintf(stderr)` + `fflush` lines during
   GUL::Load and the splash Draw traces. The perf exe drops `printf` but keeps every `fprintf(stderr)` + `fflush`;
   their volume in GUL::Load on the perf exe should be counted once (the newgame stderr log is 51 KB, so likely small).

Retail-faithfulness notes: the sync loader wait (`syncEngineLoader`), the main-thread chunked menu scene, the splash
`.rsc` preloads and `GameUpdateLoop_IsWorldBuilt` are the port's, not retail's; they serialize work that retail
overlapped a little (retail draws black frames with the loader running instead of a parked main thread, which costs
the same wall time). The device-lost / loader race is retail's and stays unguarded
(`reference_device_lost_loader_race_is_retail.md`): any worker that creates D3D objects during a load widens that
window and must drain on `OnVideoError`.

## 5. Questions only measurement answers

1. The perf-exe wall time of each route (boot -> menu, Start -> first game frame, Load -> first game frame), cold
   file cache (after a reboot) and warm; machine load CLEAN; mods parked.
2. The split of `Engine::Startup`'s 660 ms (debug) on the perf exe.
3. The split of the new-game loader time: LoadResources vs the GUL::Load ctor block vs worldgen (per f390 stage)
   vs `MapTileGrid_Build` (classify / FillZones / PfSeed / RegionSeed) vs `StartTown_SpawnEntities`.
4. Inside resource loading: I/O syscalls vs LZ decompress vs Serialize vs D3D creation; bytes decompressed per route;
   how many `GetFileAttributesW` probes miss (`g_pc.attrProbeEntered`).
5. The cost of one package mount (`ReadPackfile` x4 + the two External walks) and how many happen per route.
6. Save load: worker read time, block-1 decompress, Serialize, the map tail.
7. `GameUpdateLoop::Startup` and the first 3 frames (PaintTerrain, driver shader compile) on the perf exe.
8. Whether the main thread's loading-screen frames contend with the loader for the allocator lock or the resource
   lock (wait time on either).
9. How much of each route scales with map size (small / medium / large) and with town size for saves.
10. Whether Windows Defender's on-access scan shows up in the package reads or the `GetFileAttributesW` probes on a
    cold run.
