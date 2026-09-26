# B.3 step 7 — caller surface of `Engine` / `Save` / `Achievement` / `Audio` / `Input` / `Pathfinding` / `Picking` / `Application`, BY BINDING

Read-only measurement, 2026-09-26, main @ 96b03474 (block 0, agent 0X). No source edited, no
build, no run. Units follow `b36_session/CLOSE_NUMBERS.md` §3: sites by binding, the
`engine_funcs` population derived from INITIALISERS, comments and string contents stripped before
counting calls (mangled literals are read with strings intact, comments stripped).

## 0. Method (scripts in `…/scratchpad/b37/0X/`, not in the repo)

- **Namespace = the OWNER scope of the mangled name**, demangled with back-references
  (`common.py demangle_scope`): `?GetTitle@Achievement@1@…` is `Achievement::Achievement`,
  `?SetSaveSprite@SaveLoad@Save@@…@UI@@` is Save (a `@UI@@` in a parameter does not count). A
  plain `@NS@@` regex misses every `@1@`/`@0@` back-reference — that is why the inventory saw no
  `Application` exports and no `??_7Achievement@0@6B@`.
- `initwalk.py` — positional statement walk over `engine/funcs/engine_funcs.cpp` (comments
  stripped): each `X = RHS` binds X to `ResolveExport("…" "…")` (adjacent literals joined),
  `ResolveAddr(Module::Exe, slot)` → `python tools/dispatch_slot_map.py` dump,
  `ResolveAddr(Module::Runtime, a)` → `standalone/rt_export_addrs.inc`, or an identifier bound
  earlier (the two-statement `void** d = ResolveAddr(..); if (d) fn_X = *d;` form). Globals =
  `extern` names in `engine_funcs.h` plus file-scope definitions.
- `gsweep.py` — every code-only occurrence of each bound global in `code/ engine/ standalone/
  runtime/src/ dx11/ tests/` (build dirs excluded). CALL = `NAME(`; the `extern` line,
  file-scope definition and the initialiser's own LHS are excluded; `engine_funcs.h`'s inline
  bodies ARE counted (B36_RULES 14.2).
- `sites.py` — every mangled literal and every `0x1[48]xxxxxxx` hex literal in the same dirs,
  joined to `rt_export_addrs.inc` (4,926 rows) and the PE import table (833 Runtime slots), then
  classified by the innermost enclosing call (`ResolveExport`, `ResolveAddr`, `RtAddr`,
  `resolveIAT`, `RSlot`, `ResolveSlot`, `ResolveFn`, `RT_EXPORT_ONCE`, `RT_DATA_SLOT`,
  `AtomicExport`, `RegisterRuntime*`, `RegisterDispatchSlot`, tables…). The generated tables
  (`rt_export_addrs.inc`, `rt_data_mint_tables.inc`, `cutover_manifest.inc`) are excluded.
- `localbind.py` — spellings 2/4/5: in every file holding a caller-kind site, a POSITIONAL binder
  (nearest preceding assignment wins; `= nullptr` does not kill; an alias of an `engine_funcs`
  global inherits its binding; any other RHS kills) and then the CALLs of each bound local.
- `final.py` / `mkmd.py` — the tables below.

Spellings used in the tables: **(a)** `engine_funcs` global door CALLs, and data-global
reads; **(b)** `ResolveExport("…")` outside `engine_funcs.cpp`; **(b2)** other by-name helpers
(`ResolveFn`, `AtomicExport`, `AtomicByName`, `RtDataExport`, `CallVideoCallback`); **(c)**
`RT_EXPORT_ONCE`; **(d)** `RT_DATA_SLOT`; **(e)** exe import slot through `ResolveAddr(Module::
Exe,…)`/`resolveIAT`/`RSlot`/`ResolveSlot` outside `engine_funcs.cpp`; **(e2)**
`RegisterDispatchSlot`-family rows; **(f)** hand-inlined vtable calls with a proved receiver;
**(g)** Runtime hex literal through `ResolveAddr(Module::Runtime,…)`/`RtAddr<>` outside
`engine_funcs.cpp`; **(L)** CALLs through the file-local pointers that (b)/(b2)/(e)/(g) bind —
(b)/(e)/(g) are RESOLUTION sites, (L) is where they are called, so do not add them.

---------------------------------------------------------------------------------------------------
## 1. Totals per namespace, per spelling (all directories)

| namespace | (a) door CALLs | (a) data reads | (b) | (b2) | (c) | (d) | (e) | (e2) | (f) | (g) | (L) CALLs | **CALL sites = a + L + c** | registrations / tables (not callers) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Engine | 37 | 60 | 17 | 5 | 6 | 5 | 0 | 2 | 2 | 4 | 15 | **58** | 143 |
| Save | 53 | 25 | 10 | 0 | 15 | 2 | 0 | 3 | 0 | 0 | 22 | **90** | 53 |
| Achievement | 31 | 22 | 36 | 1 | 5 | 10 | 8 | 5 | 0 | 4 | 35 | **71** | 172 |
| Audio | 41 | 10 | 12 | 2 | 13 | 14 | 6 | 0 | 0 | 2 | 32 | **86** | 116 |
| Input | 69 | 62 | 1 | 10 | 12 | 9 | 3 | 1 | 0 | 3 | 23 | **104** | 64 |
| Pathfinding | 0 | 0 | 0 | 0 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | **0** | 29 |
| Picking | 0 | 0 | 12 | 0 | 0 | 2 | 11 | 5 | 0 | 4 | 22 | **22** | 56 |
| Application | 2 | 13 | 6 | 2 | 6 | 8 | 0 | 1 | 0 | 2 | 12 | **20** | 55 (incl. 4 dx11 import rows) |
| **total** | **233** | **192** | 94 | 20 | 57 | 52 | 28 | 17 | 2 | 19 | 161 | **451** | 688 |

`engine_funcs.cpp`'s own resolution sites are not in the table (they are the (a) bindings):
`ResolveExport` 83, exe slot 19, Runtime hex 3. (f) = the two proved `UpdateLoop::SaveRestore`
slot-7 calls (§4); the four Goal/Scenario own-slot-0 calls are in `EXE_SUBCLASSES.md` §3D and are
not caller-surface. `tests/`: zero hits in every spelling.

### 1.1 Per directory (CALL sites a + L + c; the cut for caller batches)

| namespace | directories, by CALL sites |
|---|---|
| Engine | standalone 12 · code/Game/Game 15 (8 a + 7 L) · code/Game/Dialog 10 · runtime/src/engine 14 (8 L + 6 c) · code/Game/Tutorial 5 · code/Game/Goal 1 · engine 1 (`engine_funcs.h:2603`) |
| Save | code/Game/Dialog 40 · runtime/src/save 28 (14 L + 14 c) · code/Game/Game 8 · code/Game/Tutorial 7 · standalone 5 · runtime/src/engine 2 |
| Achievement | code/Game/Game 20 · code/Game/Dialog 18 · code/Game/Goal 17 · code/Game/Tutorial 6 · runtime/src/achievement 4 · runtime/src/engine 4 · runtime/src/object 2 |
| Audio | code/Game/Component 23 · runtime/src/engine 21 · code/Game/Game 15 · code/Game/Dialog 11 · runtime/src/audio 10 · code/Game/Player 3 · code/Game/Tutorial 1 · runtime/src/ui 1 · engine 1 |
| Input | code/Game/Dialog 32 · code/Game/Game 24 · runtime/src/engine 20 · code/Game/Player 17 · runtime/src/input 5 · code/Game/Tutorial 4 · runtime/src/ui 2 |
| Picking | code/Game/Component 10 · code/Game/Player 7 · code/Game/Game 5 |
| Application | runtime/src/engine 10 · runtime/src/input 4 · code/Game/Game 2 · code/Game/Dialog 2 · code/Game/Action 1 · code/Game/Player 1 · (dx11 4 import rows) |

**Batch cut.** Engine: the loop callers are the five subclass files (block 1 owns them,
`EXE_SUBCLASSES.md`) plus the seven loop-CONSTRUCTION sites in `code/Game/{Dialog,Game,Tutorial}`,
which are ctor doors that STAY (B36_RULES 7) — so Engine's caller batch is small: `MainLoop`
members (`SetNextLoop` ×5, `OnVideoError/Restart` in `optionsdialog.cpp`, `game_main.cpp`'s
`MainLoop`/`Engine` by-address doors). Save: one batch, `code/Game/Dialog`
(`saveloaddialog.cpp` dominates) + `code/Game/Game/gameupdateloop*` + `code/Game/Tutorial`.
Achievement: `code/Game/Game` + `code/Game/Dialog` in one batch; `code/Game/Goal` + the two
`code/Game/Tutorial/finishtutorial*.cpp` files go with block 3 (they are the exe subclasses).
Audio: `code/Game/Component` (citizen emitters) and `code/Game/{Game,Dialog,Player,Tutorial}`.
Input: `code/Game/Dialog` + `code/Game/Tutorial`, then `code/Game/{Game,Player}`. Picking:
`code/Game/{Player,Component,Game}` in one batch — hex literals and names both. Application:
`dx11/` (the four rows) and the `code/Game` YieldToThreads/IsPaused/Quit sites. The
`runtime/src/engine` callers of Audio/Input/Application (21 + 20 + 10) belong to block 1's files
(`singleton_ctors.cpp`, `mainloop_run.cpp`, `engine.cpp`), not to a caller batch.

---------------------------------------------------------------------------------------------------
## 2. (a) The `engine_funcs` population, by initialiser — 82 door pointers + 10 data globals

| namespace | door pointers | data globals | door CALLs | inventory said |
|---|---:|---:|---:|---|
| Engine | 15 | 3 (`g_pEngineSingleton`, `g_pMainLoopSingleton` — BOTH bound to `?_singleton@MainLoop@Engine@@`; `g_pLoader` = `?_loader@LoaderLoop@Engine@@`) | 37 | 10 |
| Save | 12 | 1 | 53 | 10 |
| Achievement | 16 | 1 | 31 | 9 |
| Audio | 16 | 1 | 41 | 15 |
| Input | 20 | 3 (`g_pKeyMapObjectExref` is bound to `?_singleton@Configuration@Input@@`, not a KeyMap) | 69 | 19 |
| Application | 3 | 1 | 2 | 0 |
| Pathfinding / Picking | 0 | 0 | 0 | 0 |
| **total** | **82** | **10** | **233** (215 in `code/Game`) | 63 doors / 166 calls |

The inventory's 63 was name-anchored (`@NS@@`), so it lost every back-referenced export
(`Achievement@1@`, `Application@1@`) and the exe-slot-only bindings (`fn_IsRunning`,
`fn_Goal_BaseSerialize`, `fn_KeyMap_Released`). Five doors have **zero CALLs** — only a debug
`printf` reference in `engine_funcs.cpp:2426-2443`: `fn_UpdateLoop_LoadResources`,
`fn_AudioDevice_FreeChannel`, `fn_InputDevice_Update`, `fn_App_PlatformUpdate`,
`fn_App_SleepThread` (dead with their globals at the close).

Top doors by CALLs: `fn_KeyMap_GetValue` 23, `fn_KeyMap_Pressed` 15, `fn_SaveLoad_GetOperation`
11, `fn_Audio_Scene_Pause` 9, `fn_SaveLoad_GetSaveInfo` 9, `fn_SaveLoad_SetOperation` 8,
`fn_DialogLoaderLoopT_Ctor` 7, `fn_LoaderLoop_Ctor` 7, `fn_Input_Device_GetValue` 6,
`fn_Audio_Scene_PlayEffect` 6. Top data globals by code-only references (engine_funcs.cpp guards included): `g_pKeyMapObjectExref` 46,
`g_pEngineSingleton` 27, `g_pMainLoopSingleton` 26, `g_pSaveLoadSingleton` 26,
`g_ppAchievementManager` 22.

THE DOOR DECIDES THE OVERLOAD: `fn_KeyMap_ClearInput2`/`3` are the `(HH)`/`(HHH)` overloads;
`fn_Input_Device_Pressed/Released/GetValue` take `W4InputDevice` + two ints while
`fn_KeyMap_Pressed/Released/GetValue` take two ints; `fn_Audio_Scene_PlayEffect` vs
`PlayEffect3D` differ by the trailing `Vector3`; `fn_Audio_Scene_Update` passes `Time` BY VALUE
(`VTime@System@@0`) — read the prologue (B6-11). By-value `System::String` returns (hidden
retslot): `fn_Achievement_GetTitle/GetDescription/GetSpriteName`, `fn_Goal_GetText`; by-value
`ExternalPtr`/`InternalPtr` returns: `fn_AchievementManager_GetAchievement`,
`fn_GoalList_GetGoal`; `fn_SaveLoad_GetOperation` returns an enum.

### 2.1 Every bound global (generated from `gsweep.txt`)

| ns | global | calls | other refs | binding (initialiser line: how) | class::member | mangled |
|---|---|---:|---:|---|---|---|
| Achievement | `fn_Achievement_GetTitle` | 5 | 1 | engine_funcs.cpp:1530,1534 (export,via) | `Achievement::Achievement::GetTitle` | `?GetTitle@Achievement@1@QEBA?AVString@System@@XZ` |
| Achievement | `fn_Goal_BaseCtor` | 3 | 4 | engine_funcs.cpp:2822 (export) | `Achievement::Goal::??0` | `??0Goal@Achievement@@QEAA@XZ` |
| Achievement | `fn_Goal_BaseDtor` | 3 | 4 | engine_funcs.cpp:2816 (export) | `Achievement::Goal::??1` | `??1Goal@Achievement@@UEAA@XZ` |
| Achievement | `fn_Goal_BaseSerialize` | 3 | 4 | engine_funcs.cpp:2828 (via) | `Achievement::Goal::Serialize` | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | `fn_AchievementManager_GetAchievement` | 2 | 1 | engine_funcs.cpp:1550 (export) | `Achievement::AchievementManager::GetAchievement` | `?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z` |
| Achievement | `fn_AchievementManager_GetAchievementCount` | 2 | 1 | engine_funcs.cpp:1548 (export) | `Achievement::AchievementManager::GetAchievementCount` | `?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ` |
| Achievement | `fn_Achievement_GetDescription` | 2 | 1 | engine_funcs.cpp:1536,1540 (export,via) | `Achievement::Achievement::GetDescription` | `?GetDescription@Achievement@1@QEBA?AVString@System@@XZ` |
| Achievement | `fn_Achievement_GetSpriteName` | 2 | 1 | engine_funcs.cpp:1542,1546 (export,via) | `Achievement::Achievement::GetSpriteName` | `?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ` |
| Achievement | `fn_GoalList_GetGoal` | 2 | 0 | engine_funcs.cpp:1448 (export) | `Achievement::GoalList::GetGoal` | `?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z` |
| Achievement | `fn_AchievementManager_IsAchievementAwarded` | 1 | 0 | engine_funcs.cpp:1461 (export) | `Achievement::AchievementManager::IsAchievementAwarded` | `?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z` |
| Achievement | `fn_GoalInstance_BaseDtor` | 1 | 2 | engine_funcs.cpp:2832 (export) | `Achievement::GoalInstance::??1` | `??1GoalInstance@Achievement@@UEAA@XZ` |
| Achievement | `fn_GoalInstance_HasFailed` | 1 | 0 | engine_funcs.cpp:1454 (export) | `Achievement::GoalInstance::HasFailed` | `?HasFailed@GoalInstance@Achievement@@QEBA_NXZ` |
| Achievement | `fn_GoalInstance_IsMet` | 1 | 0 | engine_funcs.cpp:1451 (export) | `Achievement::GoalInstance::IsMet` | `?IsMet@GoalInstance@Achievement@@QEBA_NXZ` |
| Achievement | `fn_GoalInstance_Persist` | 1 | 2 | engine_funcs.cpp:2835 (export) | `Achievement::GoalInstance::Persist` | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` |
| Achievement | `fn_GoalList_GetGoalCount` | 1 | 0 | engine_funcs.cpp:1526 (export) | `Achievement::GoalList::GetGoalCount` | `?GetGoalCount@GoalList@Achievement@@QEBAHXZ` |
| Achievement | `fn_Goal_GetText` | 1 | 0 | engine_funcs.cpp:1528 (export) | `Achievement::Goal::GetText` | `?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ` |
| Achievement | `g_ppAchievementManager` | 0 | 22 | engine_funcs.cpp:1458 (exeslot) | `Achievement::AchievementManager::_singleton` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Application | `fn_App_Quit` | 2 | 2 | engine_funcs.cpp:1796 (export) | `Application::Application::Quit` | `?Quit@Application@1@QEAAXXZ` |
| Application | `fn_App_PlatformUpdate` | 0 | 1 | engine_funcs.cpp:1026 (export) | `Application::Application::PlatformUpdate` | `?PlatformUpdate@Application@1@AEAA_NXZ` |
| Application | `fn_App_SleepThread` | 0 | 1 | engine_funcs.cpp:1034 (export) | `Application::Application::SleepThread` | `?SleepThread@Application@1@QEAAXH@Z` |
| Application | `g_pAppSingleton` | 0 | 16 | engine_funcs.cpp:2412 (via) | `Application::Application::_singleton` | `?_singleton@Application@1@0PEAV11@EA` |
| Audio | `fn_Audio_Scene_Pause` | 9 | 2 | engine_funcs.cpp:1400,1404 (export,via) | `Audio::Scene::Pause` | `?Pause@Scene@Audio@@QEAAXH_N@Z` |
| Audio | `fn_Audio_Scene_PlayEffect` | 6 | 5 | engine_funcs.cpp:1683 (export) | `Audio::Scene::PlayEffect` | `?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z` |
| Audio | `fn_Audio_Scene_AddEmitter` | 3 | 3 | engine_funcs.cpp:2316 (export) | `Audio::Scene::AddEmitter` | `?AddEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z` |
| Audio | `fn_Audio_Scene_FindEffect3D` | 3 | 3 | engine_funcs.cpp:2312 (export) | `Audio::Scene::FindEffect3D` | `?FindEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@@Z` |
| Audio | `fn_Audio_Scene_PlayEffect3D` | 3 | 3 | engine_funcs.cpp:2314 (export) | `Audio::Scene::PlayEffect3D` | `?PlayEffect3D@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@AEBVVector3@Math@@@Z` |
| Audio | `fn_Audio_Scene_SetEmitterVolume` | 3 | 2 | engine_funcs.cpp:2309 (export) | `Audio::Scene::SetEmitterVolume` | `?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z` |
| Audio | `fn_AudioScene_Ctor` | 2 | 3 | engine_funcs.cpp:2399 (export) | `Audio::Scene::??0` | `??0Scene@Audio@@QEAA@XZ` |
| Audio | `fn_Audio_Scene_GetEmitterCount` | 2 | 2 | engine_funcs.cpp:2320 (export) | `Audio::Scene::GetEmitterCount` | `?GetEmitterCount@Scene@Audio@@QEBAHI@Z` |
| Audio | `fn_Audio_Scene_MoveEmitter` | 2 | 1 | engine_funcs.cpp:2307 (export) | `Audio::Scene::MoveEmitter` | `?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z` |
| Audio | `fn_Audio_Scene_RemoveEmitter` | 2 | 2 | engine_funcs.cpp:2318 (export) | `Audio::Scene::RemoveEmitter` | `?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z` |
| Audio | `fn_Audio_Scene_StopEffect3D` | 2 | 2 | engine_funcs.cpp:2322 (export) | `Audio::Scene::StopEffect3D` | `?StopEffect3D@Scene@Audio@@QEAAXI@Z` |
| Audio | `fn_AudioConfig_Save` | 1 | 1 | engine_funcs.cpp:2508 (export) | `Audio::Configuration::Save` | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | `fn_AudioConfig_Startup` | 1 | 1 | engine_funcs.cpp:2510 (export) | `Audio::Configuration::Startup` | `?Startup@Configuration@Audio@@QEAA_NXZ` |
| Audio | `fn_AudioDevice_Update` | 1 | 2 | engine_funcs.cpp:1018 (export) | `Audio::Device::Update` | `?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z` |
| Audio | `fn_Audio_Scene_Update` | 1 | 1 | engine_funcs.cpp:1411 (export) | `Audio::Scene::Update` | `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` |
| Audio | `fn_AudioDevice_FreeChannel` | 0 | 1 | engine_funcs.cpp:1022 (export) | `Audio::Device::FreeChannelPlatform` | `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` |
| Audio | `g_pAudioConfigSingleton` | 0 | 10 | engine_funcs.cpp:1699,1709 (via) | `Audio::Configuration::_singleton` | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Engine | `fn_DialogLoaderLoopT_Ctor` | 7 | 5 | engine_funcs.cpp:2375 (export) | `Engine::DialogLoaderLoopT::??0` | `??0DialogLoaderLoopT@Engine@@QEAA@AEBVString@System@@PEAVUpdateLoop@1@AEBVTime@3@@Z` |
| Engine | `fn_LoaderLoop_Ctor` | 7 | 4 | engine_funcs.cpp:2373 (export) | `Engine::LoaderLoop::??0` | `??0LoaderLoop@Engine@@QEAA@PEAVUpdateLoop@1@AEBVTime@System@@@Z` |
| Engine | `fn_MainLoop_SetNextLoop` | 5 | 7 | engine_funcs.cpp:2548 (export) | `Engine::MainLoop::SetNextLoop` | `?SetNextLoop@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | `fn_DialogLoaderLoopT_Load` | 4 | 3 | engine_funcs.cpp:2397 (export) | `Engine::DialogLoaderLoopT::Load` | `?Load@DialogLoaderLoopT@Engine@@UEAAXXZ` |
| Engine | `fn_UpdateLoop_Ctor` | 4 | 3 | engine_funcs.cpp:2370 (export) | `Engine::UpdateLoop::??0` | `??0UpdateLoop@Engine@@QEAA@XZ` |
| Engine | `fn_Loader_Load` | 2 | 5 | engine_funcs.cpp:1030 (export) | `Engine::Loader::Load` | `?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | `fn_DialogLoaderLoopT_Startup` | 1 | 2 | engine_funcs.cpp:2395 (export) | `Engine::DialogLoaderLoopT::Startup` | `?Startup@DialogLoaderLoopT@Engine@@UEAAXXZ` |
| Engine | `fn_DialogLoopT_Ctor` | 1 | 2 | engine_funcs.cpp:2387 (export) | `Engine::DialogLoopT::??0` | `??0DialogLoopT@Engine@@QEAA@AEBVString@System@@@Z` |
| Engine | `fn_DialogLoopT_Load` | 1 | 1 | engine_funcs.cpp:2393 (export) | `Engine::DialogLoopT::Load` | `?Load@DialogLoopT@Engine@@UEAAXXZ` |
| Engine | `fn_DialogLoopT_Startup` | 1 | 1 | engine_funcs.cpp:2391 (export) | `Engine::DialogLoopT::Startup` | `?Startup@DialogLoopT@Engine@@UEAAXXZ` |
| Engine | `fn_DialogLoopT_Update` | 1 | 1 | engine_funcs.cpp:2389 (export) | `Engine::DialogLoopT::Update` | `?Update@DialogLoopT@Engine@@UEAA_NAEBVTime@System@@@Z` |
| Engine | `fn_IsRunning` | 1 | 2 | engine_funcs.cpp:2844 (via) | `Engine::UpdateLoop::IsRunning` | `?IsRunning@UpdateLoop@Engine@@QEBA_NXZ` |
| Engine | `fn_MainLoop_OnVideoError` | 1 | 1 | engine_funcs.cpp:2544 (export) | `Engine::MainLoop::OnVideoError` | `?OnVideoError@MainLoop@Engine@@QEAAXXZ` |
| Engine | `fn_MainLoop_OnVideoRestart` | 1 | 1 | engine_funcs.cpp:2546 (export) | `Engine::MainLoop::OnVideoRestart` | `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` |
| Engine | `fn_UpdateLoop_LoadResources` | 0 | 1 | engine_funcs.cpp:1077 (export) | `Engine::UpdateLoop::LoadResources` | `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` |
| Engine | `g_pEngineSingleton` | 0 | 27 | engine_funcs.cpp:1263 (via) | `Engine::MainLoop::_singleton` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | `g_pLoader` | 0 | 11 | engine_funcs.cpp:2404 (rtaddr) | `Engine::LoaderLoop::_loader` | `?_loader@LoaderLoop@Engine@@1VLoader@2@A` |
| Engine | `g_pMainLoopSingleton` | 0 | 26 | engine_funcs.cpp:2559 (via) | `Engine::MainLoop::_singleton` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Input | `fn_KeyMap_GetValue` | 23 | 6 | engine_funcs.cpp:1655 (export) | `Input::KeyMap::GetValue` | `?GetValue@KeyMap@Input@@QEBAFHH@Z` |
| Input | `fn_KeyMap_Pressed` | 15 | 14 | engine_funcs.cpp:1467,1471 (export,via) | `Input::KeyMap::Pressed` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | `fn_Input_Device_GetValue` | 6 | 0 | engine_funcs.cpp:2489 (export) | `Input::Device::GetValue` | `?GetValue@Device@Input@@QEBAFW4InputDevice@12@HH@Z` |
| Input | `fn_KeyMap_GetDeviceForId` | 4 | 4 | engine_funcs.cpp:1127 (export) | `Input::KeyMap::GetDeviceForId` | `?GetDeviceForId@KeyMap@Input@@QEBAHHH@Z` |
| Input | `fn_KeyMap_GetInputForId` | 4 | 4 | engine_funcs.cpp:1123 (export) | `Input::KeyMap::GetInputForId` | `?GetInputForId@KeyMap@Input@@QEBAHHH@Z` |
| Input | `fn_InputDevice_EnableMouseClipping` | 2 | 2 | engine_funcs.cpp:2533 (export) | `Input::Device::EnableMouseClipping` | `?EnableMouseClipping@Device@Input@@QEAAX_N@Z` |
| Input | `fn_Input_Device_Released` | 2 | 0 | engine_funcs.cpp:1381 (export) | `Input::Device::Released` | `?Released@Device@Input@@QEBA_NW4InputDevice@12@HH@Z` |
| Input | `fn_KeyMap_Released` | 2 | 2 | engine_funcs.cpp:1857 (via) | `Input::KeyMap::Released` | `?Released@KeyMap@Input@@QEBA_NHH@Z` |
| Input | `fn_InputConfig_Load` | 1 | 1 | engine_funcs.cpp:2518 (export) | `Input::Configuration::Load` | `?Load@Configuration@Input@@QEAAXXZ` |
| Input | `fn_InputConfig_Save` | 1 | 1 | engine_funcs.cpp:2516 (export) | `Input::Configuration::Save` | `?Save@Configuration@Input@@QEAAXXZ` |
| Input | `fn_InputDevice_ResetState` | 1 | 0 | engine_funcs.cpp:2535 (export) | `Input::Device::ResetState` | `?ResetState@Device@Input@@QEAAXXZ` |
| Input | `fn_Input_Device_Pressed` | 1 | 0 | engine_funcs.cpp:2495 (export) | `Input::Device::Pressed` | `?Pressed@Device@Input@@QEBA_NW4InputDevice@12@HH@Z` |
| Input | `fn_KeyMap_ClearAll` | 1 | 2 | engine_funcs.cpp:2595 (export) | `Input::KeyMap::ClearAll` | `?ClearAll@KeyMap@Input@@QEAAXXZ` |
| Input | `fn_KeyMap_ClearInput2` | 1 | 0 | engine_funcs.cpp:2583 (export) | `Input::KeyMap::ClearInput` | `?ClearInput@KeyMap@Input@@QEAAXHH@Z` |
| Input | `fn_KeyMap_ClearInput3` | 1 | 0 | engine_funcs.cpp:2585 (export) | `Input::KeyMap::ClearInput` | `?ClearInput@KeyMap@Input@@QEAAXHHH@Z` |
| Input | `fn_KeyMap_GetIdForDeviceInput` | 1 | 0 | engine_funcs.cpp:2587 (export) | `Input::KeyMap::GetIdForDeviceInput` | `?GetIdForDeviceInput@KeyMap@Input@@QEBAHHH@Z` |
| Input | `fn_KeyMap_Load` | 1 | 2 | engine_funcs.cpp:2597 (export) | `Input::KeyMap::Load` | `?Load@KeyMap@Input@@QEAAXXZ` |
| Input | `fn_KeyMap_RegisterInput` | 1 | 2 | engine_funcs.cpp:2593 (export) | `Input::KeyMap::RegisterInput` | `?RegisterInput@KeyMap@Input@@QEAAXHHHPEBD_N@Z` |
| Input | `fn_KeyMap_SetInput` | 1 | 0 | engine_funcs.cpp:2581 (export) | `Input::KeyMap::SetInput` | `?SetInput@KeyMap@Input@@QEAAXHHHH@Z` |
| Input | `fn_InputDevice_Update` | 0 | 2 | engine_funcs.cpp:1014 (export) | `Input::Device::Update` | `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` |
| Input | `g_pInputConfigSingleton` | 0 | 10 | engine_funcs.cpp:2525 (via) | `Input::Configuration::_singleton` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | `g_pInputDeviceSingleton` | 0 | 9 | engine_funcs.cpp:1388,1395 (via) | `Input::Device::_singleton` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | `g_pKeyMapObjectExref` | 0 | 46 | engine_funcs.cpp:1258 (exeslot) | `Input::Configuration::_singleton` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Save | `fn_SaveLoad_GetOperation` | 11 | 5 | engine_funcs.cpp:1336 (export) | `Save::SaveLoad::GetOperation` | `?GetOperation@SaveLoad@Save@@QEBA?AW4Operation@12@XZ` |
| Save | `fn_SaveLoad_GetSaveInfo` | 9 | 1 | engine_funcs.cpp:1371 (export) | `Save::SaveLoad::GetSaveInfo` | `?GetSaveInfo@SaveLoad@Save@@QEBAAEBUSaveInfo@12@H@Z` |
| Save | `fn_SaveLoad_SetOperation` | 8 | 4 | engine_funcs.cpp:1361,1365 (export,via) | `Save::SaveLoad::SetOperation` | `?SetOperation@SaveLoad@Save@@QEAAXW4Operation@12@H@Z` |
| Save | `fn_SaveLoad_Dtor` | 4 | 0 | engine_funcs.cpp:1333 (export) | `Save::SaveLoad::??1` | `??1SaveLoad@Save@@QEAA@XZ` |
| Save | `fn_SaveLoad_SetMinOperationTime` | 4 | 1 | engine_funcs.cpp:1358 (export) | `Save::SaveLoad::SetMinOperationTime` | `?SetMinOperationTime@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z` |
| Save | `fn_SaveLoad_SetSaveName` | 4 | 4 | engine_funcs.cpp:2688,2691 (exeslot,export) | `Save::SaveLoad::SetSaveName` | `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` |
| Save | `fn_SaveLoad_Update` | 4 | 5 | engine_funcs.cpp:1416,1420 (export,via) | `Save::SaveLoad::Update` | `?Update@SaveLoad@Save@@QEAAXAEBVTime@System@@@Z` |
| Save | `fn_SaveLoad_GetSaveState` | 3 | 1 | engine_funcs.cpp:1373 (export) | `Save::SaveLoad::GetSaveState` | `?GetSaveState@SaveLoad@Save@@QEAAPEAVSavedState@2@XZ` |
| Save | `fn_SaveLoad_SetSaveSprite` | 2 | 1 | engine_funcs.cpp:1355 (export) | `Save::SaveLoad::SetSaveSprite` | `?SetSaveSprite@SaveLoad@Save@@QEAAXVSprite@UI@@@Z` |
| Save | `fn_SavedState_Ctor` | 2 | 2 | engine_funcs.cpp:2013 (export) | `Save::SavedState::??0` | `??0SavedState@Save@@QEAA@XZ` |
| Save | `fn_SaveLoad_CheckMod` | 1 | 1 | engine_funcs.cpp:2674,2678 (exeslot,export) | `Save::SaveLoad::CheckMod` | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` |
| Save | `fn_SaveLoad_GetSaveCount` | 1 | 0 | engine_funcs.cpp:1368 (export) | `Save::SaveLoad::GetSaveCount` | `?GetSaveCount@SaveLoad@Save@@QEBAHXZ` |
| Save | `g_pSaveLoadSingleton` | 0 | 26 | engine_funcs.cpp:1343,1350 (export,via) | `Save::SaveLoad::_singleton` | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |

---------------------------------------------------------------------------------------------------
## 3. (b)-(g) and (L): where the non-`engine_funcs` bindings are

The full site list is appendix B (every resolution site outside `engine_funcs.cpp`) and appendix C
(every file-local binding with its CALL lines). Highlights a caller batch will meet:

- **Picking is NOT called exclusively through exe slots.** Of its 22 CALL sites, the pointers come
  from exe slots (e) at bridgetool_calcpath.cpp:226, fencedtool_calcpath.cpp:92, pathtool.cpp:109,
  :113, mapquery.cpp:154, quarrycomponent_deform.cpp:93, terraformcomponent.cpp:307,
  pickingcomponent.cpp:314, edittool.cpp:213-214 (`resolveIAT`), citizenai_executor.cpp:369
  (`ResolveSlot`); by NAME (b) at gameupdateloop.cpp:643, gameupdateloop_load.cpp:340,
  keystone_a_gamedtor.cpp:94, mapquery.cpp:158, edittool.cpp:243/:246, pickingcomponent.cpp:147;
  by Runtime HEX (g) at citizenai_executor.cpp:372, floorcomponent.cpp:249/:254,
  pickingcomponent.cpp:312. Most files bind the same member two ways (name, then slot fallback).
- **Achievement's `code/Game` callers are mostly file-local:** gameupdateloop.cpp:529-549
  (`AchievementInstance::Update/IsMet/HasFailed`, name + exe-slot fallback, called :1231, :1234,
  :1238, :2740, :2772), gameupdateloop_load.cpp:88-93 / :1280 / :1325, gameupdateloop_serialize.cpp
  :157 / :179-181, scenario.cpp:74-81 (by Runtime ADDRESS), dialog_exe_helpers.cpp:214,
  game_main.cpp:270.
- **Engine by address:** game_main.cpp:184-188 (`MainLoop` ctor/dtor/Run via
  `ResolveAddr(Module::Runtime, 0x1800dd6b0/640/3c0)`), game_main.cpp:174-180 (`Engine` ctor,
  dtor, Startup, Shutdown by name — Shutdown is resolved and never called).
- **Input through UI's atomic slots:** runtime/src/ui/{editelement.cpp:89,:93,
  element_bodies_b5.cpp:203, userinterface.cpp:303, userinterface_draw.cpp:175,:179,
  userinterface_frame.cpp:185,:189} read `Input::Device`/`Configuration::_singleton`, and
  buttonelement.cpp:284 / checkelement.cpp:148 call `KeyMap::Pressed` through `RtAddr<>` —
  step 6's by-address spelling, live.
- **(c)/(d) concentrate in block files:** `runtime/src/engine` holds 10 of Audio's and 10 of
  Input's `RT_EXPORT_ONCE`, i.e. `singleton_ctors.cpp`/`mainloop_run.cpp` calling other blocks'
  classes — they convert when those classes exist, inside block 1's files.
- Dead resolution sites (resolved, never called): game_main.cpp:180 `Engine::Shutdown`,
  gameupdateloop.cpp:653 `Application::IsPaused`. Dead named constants (never read):
  runtime/src/engine/loops.cpp:150-154, runtime/src/picking/scene.cpp:138-150,
  runtime/src/save/savedstate_setupload.cpp:117, runtime/src/save/saveload_loadmeta.cpp:250,
  and `engine/types/MainLoop.h:43-75` (`MainLoopAddr`, 26 hex constants — zero readers; the
  `MainLoopAddr::` reads in runtime/src/engine/engine.cpp:512-514 are the OTHER header,
  runtime/types/engine/MainLoop.h).

---------------------------------------------------------------------------------------------------
## 4. (f) Hand-inlined vtable calls — swept over ALL of `code/Game`, `standalone`, `engine`

Tokens swept: `(*(void***)`, `(*(uintptr_t**)`, `*reinterpret_cast<void***>(`,
`*reinterpret_cast<uintptr_t**>(` (197 lines) and the double-deref slot idiom
`*(uintptr_t*)(*(uintptr_t*)p + off)` / `*reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(`
(122 lines). Receivers read one by one where the name or offset could be one of these classes.

| site | receiver | proof | slot |
|---|---|---|---|
| code/Game/Dialog/moddialog.cpp:733 | `Engine::UpdateLoop` | `inner = *(MainLoop + 0x20)`; `g_pEngineSingleton` is bound to `?_singleton@MainLoop@Engine@@` (exe slot 0x140151440, engine_funcs.cpp:1261-1263); +0x20 is `MainLoop::_currentLoop` (runtime/types/engine/MainLoop.h:233, :275) | +0x38 = 7 `SaveRestore(bool, SavedState*)`, args `(inner, true, statePtr)` match |
| code/Game/Dialog/mod_dialog_vtables.cpp:440 | same | same (:435-441) | same |

These are ordinary virtual calls (not in a subclass slot body): after conversion
`reinterpret_cast<Engine::UpdateLoop*>(inner)->SaveRestore(true, state)`, unqualified.
**Proved NOT these namespaces** (listed so nobody re-reads them): game_main.cpp:293, :298 — the
`IXAudio2MasteringVoice` / `IXAudio2` COM objects in `Audio::Device` +0xa90/+0xa88
(runtime/src/audio/audio_platform_startup_win.cpp:42-43); this block is exe code reaching into
`Audio::Device`'s private platform fields (a field-access caller of `Audio::Device`, not a
door). quarrycomponent_deform.cpp:167/:176 (DecalComponent), workplacecomponent.cpp:301
(MeshInstance), gameupdateloop.cpp:1406, :2625-2637 (world geometry / sun / sky),
dialogloaderloop.cpp:37 (Video device), fencedtool_calcpath.cpp:341, edittool.cpp:318 (tool /
component). The Goal/Scenario own-slot-0 calls (statgoal.cpp:167, timegoal.cpp:78,
goal_exe_helpers.cpp:205, scenario.cpp:196) are in `EXE_SUBCLASSES.md` §3D. **Unproved: none**
left among the lines that could plausibly reach these classes; the remaining ~310 lines target
components, entities, UI and Graphics objects by their own names.

---------------------------------------------------------------------------------------------------
## 5. Pathfinding — reachability verdict

**Tree-side callers of any Pathfinding export or address, by every spelling: zero** outside
Pathfinding's own port (`runtime/src/pathfinding/pointlist.cpp` registrations :331-358,
`RT_DATA_SLOT` ×2 :103-105 reading its own vtable/typeInfo, `rt_factories_pathfinding.inc`) and
the object tables (`runtime/src/object/deleting_dtor.cpp:927`,
`class_operator_delete.cpp:94`, `rt_factories_core.cpp`, `rt_object_port.cpp`). The exe
imports exactly one Pathfinding symbol, `?_typeInfo@PointList@Pathfinding@@` (slot 0x140151448),
and no tree file resolves that slot.

**But `Pathfinding::PointList` IS on a live route, by data.** Ghidra (Application-x64.exe,
read-only): slot 0x140151448 is read only by `FUN_14000ab20` (0x14000ab85, 0x14000ace4), the
exe's `ExternalPtr<Pathfinding::PointList>::Serialize` (FindOrLoad + `IsDerivedFrom(PointList)`).
Its callers are `FUN_140053460` and `FUN_140123500`. `FUN_140123500` is
**`InteractDescription::Serialize`**, ported as `code/Game/Component/interactdescription.cpp:82`:
it loads the ExternalPtr at +0x20 through the generic `fn_ExternalPtr_Serialize2` (:91 — no
PointList type check) and then reads the PointList's fields RAW — `ext + 0x30` (named point
array) and `ext + 0x38` (count), :100-125, building the use/build/create nodes every building's
access point depends on (the file's own comment calls the loss of those nodes "Economy Brick M-D
root cause #3", :94-99). So every new game constructs `PointList`s through the Runtime factory
(`??_7PointList` stamped by `rt_factories_pathfinding.inc`) and runs the ported
`PointList::Serialize`. The file's header (:9) misnames the pointee "Description".

Verdict: `PointList` (9 slots, External subclass) is **live and must be a real class with its
field layout pinned** (+0x30 array, +0x38 count, stride 0x18 per interactdescription.cpp:106);
its one exe caller is a raw field read, not a door. Everything else in the 48 Pathfinding
exports (`Scene`, `IPathInfo`, the path solver) has **no caller on any route**:
`IPathInfo` (2 pure slots) and the solver are header + declarations only.

---------------------------------------------------------------------------------------------------
## 6. Binding defects found on the way (NOT fixed — reported)

1. **`fn_KeyMap_Released` is always null.** Its only initialiser is exe slot 0x1401528f0
   (engine_funcs.cpp:1855-1857), which nothing registers (`grep -n 0x1401528f0 standalone/`
   finds only the comment reg_unreg_lane2.cpp:129, which wrongly says the global "resolves by
   ResolveExport"). Both callers, code/Game/Player/dragtool.cpp:255 and :401, therefore never
   see a key release. Candidate carry-over; the fix is a `ResolveExport` initialiser (the door is
   `?Released@KeyMap@Input@@QEBA_NHH@Z`).
2. **code/Game/Tutorial/finishtutorialinstance.cpp:72 resolves exe slot 0x140151c08 as
   "IsDerivedFrom"; the PE import table says that slot is `?Save@Configuration@Audio@@QEAAXXZ`.**
   The slot is unregistered, so the pointer stays null and the TutorialInstance type check at
   :85-88 is silently skipped (it would otherwise call `Audio::Configuration::Save` with two
   arguments). A misbinding in a Goal-family slot body (slot 3).
3. `g_pEngineSingleton` and `g_pMainLoopSingleton` are two globals for ONE symbol
   (`MainLoop::_singleton`; engine_funcs.cpp:1261-1263 and :2552-2559). The name
   `g_pEngineSingleton` is wrong — it is not `Engine::Engine::_singleton`.
   `g_pKeyMapObjectExref` (engine_funcs.cpp:1257-1258) is `Input::Configuration::_singleton`.
4. Exe `AchievementInstance` table 0x14015e350 is resolved (gameupdateloop_load.cpp:1633) but
   never registered, so standalone keeps the Runtime table (the code says so, :1627-1632).

---------------------------------------------------------------------------------------------------
## 7. `STEP7_INVENTORY.md` sections 1.3, 3 and 4 — numbers re-measured

| inventory claim (section) | measured | command / source |
|---|---|---|
| 14 mint rows; Achievement 3 (1.3, 2) | **15**; Achievement **4** — `??_7Achievement@0@6B@` 9 slots | `sites.py` owner demangle over `rt_export_addrs.inc`; `rt_data_mint_tables.inc:1545, :3889` |
| `Achievement::Achievement` "registered dtor but NO mint vtable" (1.3) | has one (above); the regex missed the `@0@` back-reference | same |
| Audio `Stream` 9 slots (1.3) | **2** (`Run`, `OnCompleted`) | `rt_data_mint_tables.inc:3525-3530` |
| `Application` has ZERO named exports (1.1, 6) | **27** exports owned by `Application::Application` | `python -c "from common import *; …ns_of(m)=='Application'"` over `rt_export_addrs.inc` |
| per-namespace export rows Engine 58, Save 30, Ach 70, Audio 113, Input 57, Path 32, Pick 36, App 0 (3.5) | by owner: **116, 38, 127, 144, 67, 48, 51, 27** | same |
| 63 `engine_funcs` doors, 166 `code/Game` calls (3.1) | **82 doors + 10 data globals; 233 door CALLs (215 in `code/Game`)** | `initwalk.py` + `gsweep.py` |
| Engine 10 / Save 10 / Ach 9 / Audio 15 / Input 19 / App 0 doors (3.1) | 15 / 12 / 16 / 16 / 20 / **3** | same |
| `fn_SaveLoad_GetOperation` 9, `fn_SaveLoad_GetSaveInfo` 8 calls (3.1) | **11**, **9** | `gsweep.txt` |
| `ResolveExport` Engine code/Game 1 (3.2) | **5** | `sites.tsv` |
| `ResolveExport` Achievement engine 12 / runtime 7 / standalone 3 / code/Game 12 (3.2) | 15 / 9 / 5 / **22** | `sites.tsv` |
| `ResolveExport` Audio runtime/src 1 (3.2) | **6** | `sites.tsv` |
| `ResolveExport` Picking code/Game 3 (3.2) | **7** | `sites.tsv` |
| `ResolveExport` Application all 0 (3.2) | engine 3 / runtime 2 / code/Game 4 | `sites.tsv` |
| `RT_EXPORT_ONCE` Engine 3, Save 6, Ach 1, Audio 10, Input 10, App 0 (3.3) | **6, 15, 5, 13, 12, 6** | `sites.tsv` (literal-resolved) |
| `RT_DATA_SLOT` Engine 1, Ach 0, Audio 13, Input 8, App 0 (3.3) | **5, 10, 14, 9, 8** | `sites.tsv` |
| exe dispatch slots Engine 27, Save 14, Ach 27, Audio 18, Pick 8, App 0 (3.4) | **42, 18, 42, 21, 11, 4** (Input 21, Path 1 agree) | `python tools/dispatch_slot_map.py` + owner demangle |
| exe-slot hits Ach 11, Audio 3, Input 5, Pick 8, App 0 (3.4) | **13** (8 + 5 init), **8** (6 + 2), **7** (3 + 4), **11**, **1** | `sites.tsv` |
| "Picking is called EXCLUSIVELY through" exe slots (3.4) | false: 7 by-name + 4 by-hex resolution sites too (§3) | `sites.tsv` |
| by-address Runtime hits: 8 total; Input 0, Achievement 0, Application 0 (3.5) | **19**: Engine 5, Achievement 4 (scenario.cpp:75-81), Audio 3, Input 3, Picking 4, Application 2 — plus 26 dead hex constants in `engine/types/MainLoop.h` | `sites.tsv` |
| hand-inlined vtable calls: 2, in `code/Game/Component`, targets unverified (3.6) | full sweep: **2 proved** (UpdateLoop slot 7, moddialog.cpp:733, mod_dialog_vtables.cpp:440) + 4 own-slot-0 in Goal/Scenario bodies; the two Component hits are DecalComponent (not these namespaces) | §4 |
| `GameUpdateLoop` 22 slots (4.1) | **12** | standalone/gameupdateloop_stubs.cpp:149 |
| five exe-side loop subclasses; only GameUpdateLoop not a standalone stub (4.1) | five loop tables confirmed, but GameUpdateLoop's TABLE is also in `standalone/` (gameupdateloop_stubs.cpp:149); plus **Scenario : Achievement::Achievement** (scenario.cpp:277) and the exe `AchievementInstance` (0x14015e350) the inventory missed | `EXE_SUBCLASSES.md` §1 |
| Goal family "zero hand-inlined calls", all in `code/Game/Goal` (4.2) | 3 own-slot-0 calls (statgoal.cpp:167, timegoal.cpp:78, goal_exe_helpers.cpp:205); FinishTutorialGoal/Instance live in `code/Game/Tutorial/` | `EXE_SUBCLASSES.md` §1B, §3D |
| two `engine/types` mirror PODs, 204 lines (4.2, 4.8) | the loop PODs survive too: `engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT,GameUpdateLoop,MainLoop}.h`, plus `TutorialGoals.h` (an External family) | `ls engine/types` |
| `PickingComponent` a Picking mirror POD (4.2) | an exe **`Component`** subclass holding an int id (PickingComponent.h:6, :29) | `engine/types/PickingComponent.h` |
| Pathfinding: 0 called, mod-tool only (2) | `PointList` is LIVE via `InteractDescription` (§5) | Ghidra xrefs to 0x140151448 |

---------------------------------------------------------------------------------------------------
## 8. Reproduce

```
SP=…/scratchpad/b37/0X
python tools/dispatch_slot_map.py > $SP/slotmap.txt
cd $SP && python initwalk.py && python gsweep.py && python sites.py && python localbind.py && python final.py && python mkmd.py
```
Outputs: `globals.tsv` (every binding), `gsweep.txt` (every use of every global, CALL/REF),
`sites.tsv` (every literal / hex site with its enclosing call), `localbind.txt`, `final.txt`.

---------------------------------------------------------------------------------------------------
## Appendix A — CALL sites of every `engine_funcs` door (code-only)

- `fn_Achievement_GetTitle` (Achievement): code/Game/Dialog/achievementdialog.cpp:141, code/Game/Dialog/goaldialog.cpp:190, code/Game/Dialog/scenariodialog.cpp:230, code/Game/Game/eventlog_achievement.cpp:80, code/Game/Game/gameupdateloop.cpp:2753
- `fn_Goal_BaseCtor` (Achievement): code/Game/Goal/buildinggoal.cpp:171, code/Game/Goal/populationgoal.cpp:101, code/Game/Tutorial/finishtutorialgoal.cpp:105
- `fn_Goal_BaseDtor` (Achievement): code/Game/Goal/buildinggoal.cpp:71, code/Game/Goal/populationgoal.cpp:47, code/Game/Tutorial/finishtutorialgoal.cpp:48
- `fn_Goal_BaseSerialize` (Achievement): code/Game/Goal/buildinggoal.cpp:99, code/Game/Goal/populationgoal.cpp:70, code/Game/Tutorial/finishtutorialgoal.cpp:72
- `fn_AchievementManager_GetAchievement` (Achievement): code/Game/Dialog/achievementdialog.cpp:125, code/Game/Game/eventlog_achievement.cpp:74
- `fn_AchievementManager_GetAchievementCount` (Achievement): code/Game/Dialog/achievementdialog.cpp:67, code/Game/Dialog/dialog_exe_helpers.cpp:221
- `fn_Achievement_GetDescription` (Achievement): code/Game/Dialog/achievementdialog.cpp:149, code/Game/Dialog/scenariodialog.cpp:246
- `fn_Achievement_GetSpriteName` (Achievement): code/Game/Dialog/achievementdialog.cpp:130, code/Game/Dialog/scenariodialog.cpp:214
- `fn_GoalList_GetGoal` (Achievement): code/Game/Dialog/goaldialog.cpp:134, code/Game/Dialog/goaldialog.cpp:242
- `fn_AchievementManager_IsAchievementAwarded` (Achievement): code/Game/Dialog/achievementdialog.cpp:213
- `fn_GoalInstance_BaseDtor` (Achievement): code/Game/Tutorial/finishtutorialgoal.cpp:212
- `fn_GoalInstance_HasFailed` (Achievement): code/Game/Dialog/goaldialog.cpp:246
- `fn_GoalInstance_IsMet` (Achievement): code/Game/Dialog/goaldialog.cpp:245
- `fn_GoalInstance_Persist` (Achievement): code/Game/Tutorial/finishtutorialinstance.cpp:51
- `fn_GoalList_GetGoalCount` (Achievement): code/Game/Dialog/goaldialog.cpp:78
- `fn_Goal_GetText` (Achievement): code/Game/Dialog/goaldialog.cpp:142
- `fn_App_Quit` (Application): code/Game/Dialog/mainmenudialog.cpp:753, code/Game/Dialog/mainmenudialog.cpp:969
- `fn_Audio_Scene_Pause` (Audio): code/Game/Dialog/mainmenudialog.cpp:649, code/Game/Dialog/mainmenudialog.cpp:650, code/Game/Dialog/mainmenudialog.cpp:651, code/Game/Dialog/mainmenudialog.cpp:697, code/Game/Dialog/mainmenudialog.cpp:698, code/Game/Dialog/mainmenudialog.cpp:699, code/Game/Game/gameupdateloop_load.cpp:1699, code/Game/Game/gameupdateloop_load.cpp:1700, code/Game/Game/gameupdateloop_load.cpp:1701
- `fn_Audio_Scene_PlayEffect` (Audio): code/Game/Dialog/eventnotifydialog.cpp:555, code/Game/Game/menudialogloopt.cpp:1206, code/Game/Game/unreg_lane5_musicplayer.cpp:271, code/Game/Player/tool_base_drag.cpp:122, code/Game/Player/tool_base_drag.cpp:152, code/Game/Player/tool_base_drag.cpp:172
- `fn_Audio_Scene_AddEmitter` (Audio): code/Game/Component/ambientemittercomponent.cpp:241, code/Game/Component/ambientenvironmentcomponent.cpp:117, code/Game/Component/firecomponent_ignite.cpp:268
- `fn_Audio_Scene_FindEffect3D` (Audio): code/Game/Component/ambientemittercomponent.cpp:233, code/Game/Component/ambientenvironmentcomponent.cpp:107, code/Game/Component/firecomponent_ignite.cpp:260
- `fn_Audio_Scene_PlayEffect3D` (Audio): code/Game/Component/ambientemittercomponent.cpp:238, code/Game/Component/ambientenvironmentcomponent.cpp:112, code/Game/Component/firecomponent_ignite.cpp:265
- `fn_Audio_Scene_SetEmitterVolume` (Audio): code/Game/Component/ambientenvironmentcomponent.cpp:176, code/Game/Component/tornadocomponent.cpp:957, code/Game/Dialog/debugui.cpp:343
- `fn_AudioScene_Ctor` (Audio): code/Game/Game/gameupdateloop_load.cpp:1113, engine/funcs/engine_funcs.h:2610
- `fn_Audio_Scene_GetEmitterCount` (Audio): code/Game/Component/ambientenvironmentcomponent.cpp:161, code/Game/Component/firecomponent_ignite.cpp:355
- `fn_Audio_Scene_MoveEmitter` (Audio): code/Game/Component/tornadocomponent.cpp:1053, code/Game/Dialog/debugui.cpp:318
- `fn_Audio_Scene_RemoveEmitter` (Audio): code/Game/Component/ambientenvironmentcomponent.cpp:160, code/Game/Component/firecomponent_ignite.cpp:353
- `fn_Audio_Scene_StopEffect3D` (Audio): code/Game/Component/ambientenvironmentcomponent.cpp:162, code/Game/Component/firecomponent_ignite.cpp:357
- `fn_AudioConfig_Save` (Audio): code/Game/Dialog/optionsdialog.cpp:1163
- `fn_AudioConfig_Startup` (Audio): code/Game/Dialog/optionsdialog.cpp:1220
- `fn_AudioDevice_Update` (Audio): code/Game/Game/menudialogloopt.cpp:1139
- `fn_Audio_Scene_Update` (Audio): code/Game/Game/menudialogloopt.cpp:1334
- `fn_DialogLoaderLoopT_Ctor` (Engine): code/Game/Dialog/mainmenudialog.cpp:259, code/Game/Dialog/newgamedialog.cpp:877, code/Game/Dialog/saveloaddialog.cpp:811, code/Game/Game/game_main.cpp:723, code/Game/Game/worldsetup_reload.cpp:116, code/Game/Tutorial/tutorialui.cpp:110, code/Game/Tutorial/tutorial_next_scenario.cpp:115
- `fn_LoaderLoop_Ctor` (Engine): code/Game/Dialog/mainmenudialog.cpp:276, code/Game/Dialog/newgamedialog.cpp:894, code/Game/Dialog/saveloaddialog.cpp:834, code/Game/Game/game_main.cpp:815, code/Game/Game/worldsetup_reload.cpp:134, code/Game/Tutorial/tutorialui.cpp:134, code/Game/Tutorial/tutorial_next_scenario.cpp:132
- `fn_MainLoop_SetNextLoop` (Engine): code/Game/Game/gameupdateloop.cpp:1630, standalone/loader_loop_stubs.cpp:169, standalone/loader_loop_stubs.cpp:282, standalone/menudialogloopt_stubs.cpp:525, standalone/splash_loop_stubs.cpp:482
- `fn_DialogLoaderLoopT_Load` (Engine): code/Game/Game/game_main.cpp:765, code/Game/Game/game_main.cpp:769, standalone/splash_loop_stubs.cpp:173, standalone/splash_loop_stubs.cpp:549
- `fn_UpdateLoop_Ctor` (Engine): code/Game/Dialog/newgamedialog.cpp:829, code/Game/Dialog/saveloaddialog.cpp:764, code/Game/Game/worldsetup_reload.cpp:76, code/Game/Tutorial/tutorial_next_scenario.cpp:77
- `fn_Loader_Load` (Engine): standalone/loader_loop_stubs.cpp:144, standalone/loader_loop_stubs.cpp:260
- `fn_DialogLoaderLoopT_Startup` (Engine): standalone/splash_loop_stubs.cpp:199
- `fn_DialogLoopT_Ctor` (Engine): engine/funcs/engine_funcs.h:2603
- `fn_DialogLoopT_Load` (Engine): standalone/menudialogloopt_stubs.cpp:669
- `fn_DialogLoopT_Startup` (Engine): standalone/menudialogloopt_stubs.cpp:318
- `fn_DialogLoopT_Update` (Engine): standalone/menudialogloopt_stubs.cpp:551
- `fn_IsRunning` (Engine): code/Game/Goal/populationgoal.cpp:137
- `fn_MainLoop_OnVideoError` (Engine): code/Game/Dialog/optionsdialog.cpp:1064
- `fn_MainLoop_OnVideoRestart` (Engine): code/Game/Dialog/optionsdialog.cpp:1100
- `fn_KeyMap_GetValue` (Input): code/Game/Dialog/entityui.cpp:521, code/Game/Dialog/entityui.cpp:527, code/Game/Dialog/toolui.cpp:419, code/Game/Dialog/toolui.cpp:424, code/Game/Dialog/toolui.cpp:447, code/Game/Dialog/toolui.cpp:460, code/Game/Dialog/toolui.cpp:464, code/Game/Dialog/toolui.cpp:522, code/Game/Game/camera_input.cpp:137, code/Game/Game/camera_input.cpp:138, code/Game/Game/camera_input.cpp:139, code/Game/Game/camera_input.cpp:140, code/Game/Game/camera_input.cpp:149, code/Game/Game/camera_input.cpp:150, code/Game/Game/camera_input.cpp:156, code/Game/Game/camera_input.cpp:157, code/Game/Game/camera_input.cpp:158, code/Game/Game/camera_input.cpp:159, code/Game/Game/camera_input.cpp:165, code/Game/Player/dragtool.cpp:403, code/Game/Player/roadtool.cpp:246, code/Game/Player/tool_base_ray.cpp:233, code/Game/Player/tool_base_ray.cpp:234
- `fn_KeyMap_Pressed` (Input): code/Game/Dialog/entityui.cpp:557, code/Game/Dialog/followdialog.cpp:337, code/Game/Game/camera_input.cpp:168, code/Game/Game/camera_input.cpp:170, code/Game/Game/camera_input.cpp:177, code/Game/Game/camera_input.cpp:179, code/Game/Game/camera_input.cpp:348, code/Game/Game/camera_input.cpp:357, code/Game/Game/gameupdateloop.cpp:1524, code/Game/Game/gameupdateloop.cpp:1555, code/Game/Player/dragtool.cpp:264, code/Game/Player/dragtool.cpp:374, code/Game/Player/fencedtool.cpp:161, code/Game/Player/fencedtool.cpp:176, code/Game/Player/tool_activation_standalone.cpp:393
- `fn_Input_Device_GetValue` (Input): code/Game/Dialog/spritecaptureui.cpp:121, code/Game/Dialog/spritecaptureui.cpp:130, code/Game/Dialog/spritecaptureui.cpp:131, code/Game/Dialog/spritecaptureui.cpp:139, code/Game/Dialog/spritecaptureui.cpp:141, code/Game/Dialog/spritecaptureui.cpp:142
- `fn_KeyMap_GetDeviceForId` (Input): code/Game/Dialog/hotkeytooltipui.cpp:106, code/Game/Dialog/optionsdialog.cpp:1441, code/Game/Tutorial/tutorialimagehotkey.cpp:263, code/Game/Tutorial/tutorialimageinputkey.cpp:246
- `fn_KeyMap_GetInputForId` (Input): code/Game/Dialog/hotkeytooltipui.cpp:105, code/Game/Dialog/optionsdialog.cpp:1442, code/Game/Tutorial/tutorialimagehotkey.cpp:272, code/Game/Tutorial/tutorialimageinputkey.cpp:250
- `fn_InputDevice_EnableMouseClipping` (Input): code/Game/Dialog/optionsdialog.cpp:844, code/Game/Dialog/optionsdialog.cpp:1211
- `fn_Input_Device_Released` (Input): code/Game/Dialog/optioninputbinddialog.cpp:81, code/Game/Dialog/optioninputbinddialog.cpp:94
- `fn_KeyMap_Released` (Input): code/Game/Player/dragtool.cpp:255, code/Game/Player/dragtool.cpp:401
- `fn_InputConfig_Load` (Input): code/Game/Dialog/optionsdialog.cpp:1204
- `fn_InputConfig_Save` (Input): code/Game/Dialog/optionsdialog.cpp:848
- `fn_InputDevice_ResetState` (Input): code/Game/Dialog/optionsdialog.cpp:1647
- `fn_Input_Device_Pressed` (Input): code/Game/Dialog/spritecaptureui.cpp:138
- `fn_KeyMap_ClearAll` (Input): code/Game/Game/inputbindings.cpp:142
- `fn_KeyMap_ClearInput2` (Input): code/Game/Dialog/optionsdialog.cpp:1538
- `fn_KeyMap_ClearInput3` (Input): code/Game/Dialog/optionsdialog.cpp:1635
- `fn_KeyMap_GetIdForDeviceInput` (Input): code/Game/Dialog/optionsdialog.cpp:1531
- `fn_KeyMap_Load` (Input): code/Game/Game/inputbindings.cpp:151
- `fn_KeyMap_RegisterInput` (Input): code/Game/Game/inputbindings.cpp:145
- `fn_KeyMap_SetInput` (Input): code/Game/Dialog/optionsdialog.cpp:1646
- `fn_SaveLoad_GetOperation` (Save): code/Game/Dialog/autosavedialog.cpp:139, code/Game/Dialog/dialog_exe_helpers.cpp:117, code/Game/Dialog/saveloaddialog.cpp:524, code/Game/Dialog/saveloaddialog.cpp:706, code/Game/Dialog/saveloaddialog.cpp:713, code/Game/Dialog/scenariodialog.cpp:466, code/Game/Dialog/scenariodialog.cpp:473, code/Game/Tutorial/tutorial_next_scenario.cpp:304, code/Game/Tutorial/tutorial_next_scenario.cpp:310, standalone/saveload_harness.cpp:122, standalone/saveload_harness.cpp:132
- `fn_SaveLoad_GetSaveInfo` (Save): code/Game/Dialog/saveloaddialog.cpp:191, code/Game/Dialog/saveloaddialog.cpp:542, code/Game/Dialog/saveloaddialog.cpp:575, code/Game/Dialog/saveloaddialog.cpp:972, code/Game/Dialog/saveloaddialog.cpp:1025, code/Game/Dialog/saveloaddialog.cpp:1049, code/Game/Dialog/saveloaddialog.cpp:1074, code/Game/Dialog/saveloaddialog.cpp:1130, standalone/autostart.cpp:421
- `fn_SaveLoad_SetOperation` (Save): code/Game/Dialog/dialog_exe_helpers.cpp:142, code/Game/Dialog/saveloaddialog.cpp:486, code/Game/Dialog/saveloaddialog.cpp:722, code/Game/Dialog/saveloaddialog.cpp:947, code/Game/Dialog/saveloaddialog.cpp:991, code/Game/Dialog/saveloaddialog.cpp:1165, code/Game/Dialog/scenariodialog.cpp:425, code/Game/Tutorial/tutorial_next_scenario.cpp:279
- `fn_SaveLoad_Dtor` (Save): code/Game/Dialog/mainmenudialog.cpp:230, code/Game/Dialog/mainmenudialog.cpp:238, code/Game/Game/gameupdateloop.cpp:2658, code/Game/Tutorial/tutorialui.cpp:85
- `fn_SaveLoad_SetMinOperationTime` (Save): code/Game/Dialog/dialog_exe_helpers.cpp:123, code/Game/Dialog/saveloaddialog.cpp:481, code/Game/Dialog/saveloaddialog.cpp:968, code/Game/Dialog/saveloaddialog.cpp:1015
- `fn_SaveLoad_SetSaveName` (Save): code/Game/Dialog/dialog_exe_helpers.cpp:138, code/Game/Dialog/saveloaddialog.cpp:988, code/Game/Dialog/scenariodialog.cpp:426, code/Game/Tutorial/tutorial_next_scenario.cpp:280
- `fn_SaveLoad_Update` (Save): code/Game/Dialog/scenariodialog.cpp:472, code/Game/Game/gameupdateloop.cpp:1565, code/Game/Tutorial/tutorial_next_scenario.cpp:309, standalone/menudialogloopt_stubs.cpp:426
- `fn_SaveLoad_GetSaveState` (Save): code/Game/Dialog/saveloaddialog.cpp:762, code/Game/Dialog/scenariodialog.cpp:497, code/Game/Tutorial/tutorial_next_scenario.cpp:319
- `fn_SaveLoad_SetSaveSprite` (Save): code/Game/Dialog/dialog_exe_helpers.cpp:131, code/Game/Dialog/saveloaddialog.cpp:476
- `fn_SavedState_Ctor` (Save): code/Game/Dialog/moddialog.cpp:723, code/Game/Dialog/mod_dialog_vtables.cpp:431
- `fn_SaveLoad_CheckMod` (Save): code/Game/Dialog/saveloaddialog.cpp:1047
- `fn_SaveLoad_GetSaveCount` (Save): code/Game/Dialog/saveloaddialog.cpp:728

## Appendix B — resolution sites outside engine_funcs.cpp (b, b2, c, d, e, g)

| ns | site | spelling | symbol |
|---|---|---|---|
| Achievement | code/Game/Game/gameupdateloop.cpp:536 | exehex via `ResolveAddr` | `?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp:541 | exehex via `ResolveAddr` | `?IsMet@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Game/gameupdateloop.cpp:546 | exehex via `ResolveAddr` | `?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Goal/statgoal.cpp:66 | exehex via `ResolveAddr` | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoal.cpp:34 | exehex via `ResolveAddr` | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp:58 | exehex via `ResolveAddr` | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp:64 | exehex via `ResolveAddr` | `?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp:67 | exehex via `ResolveAddr` | `?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:266 | lit via `RT_DATA_SLOT` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:276 | lit via `RT_DATA_SLOT` | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_state.cpp:144 | lit via `RT_DATA_SLOT` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:210 | lit via `RT_DATA_SLOT` | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:214 | lit via `RT_DATA_SLOT` | `?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:218 | lit via `RT_DATA_SLOT` | `?_typeInfo@Goal@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:222 | lit via `RT_DATA_SLOT` | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/goal.cpp:171 | lit via `RT_DATA_SLOT` | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/goal_list.cpp:209 | lit via `RT_DATA_SLOT` | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp:288 | lit via `RT_DATA_SLOT` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/achievement/achievement_serialize.cpp:288 | lit via `RT_EXPORT_ONCE` | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/achievement/achievement_state.cpp:412 | lit via `RT_EXPORT_ONCE` | `?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp:751 | lit via `RT_EXPORT_ONCE` | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp:846 | lit via `RT_EXPORT_ONCE` | `??1AchievementManager@Achievement@@QEAA@XZ` |
| Achievement | runtime/src/object/deleting_dtor_leaves.cpp:153 | lit via `RT_EXPORT_ONCE` | `??1GoalList@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Dialog/dialog_exe_helpers.cpp:214 | lit via `ResolveExport` | `?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp:529 | lit via `ResolveExport` | `?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp:531 | lit via `ResolveExport` | `?IsMet@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Game/gameupdateloop.cpp:533 | lit via `ResolveExport` | `?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Game/gameupdateloop.cpp:647 | lit via `ResolveExport` | `?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp:649 | lit via `ResolveExport` | `?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp:1280 | lit via `ResolveExport` | `?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp:1325 | lit via `ResolveExport` | `?Reset@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp:88 | lit via `ResolveExport` | `??0AchievementInstance@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp:90 | lit via `ResolveExport` | `?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp:93 | lit via `ResolveExport` | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp:157 | lit via `ResolveExport` | `?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp:179 | lit via `ResolveExport` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp:181 | lit via `ResolveExport` | `?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | code/Game/Goal/goal_exe_helpers.cpp:223 | lit via `ResolveExport` | `?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ` |
| Achievement | code/Game/Goal/statgoal.cpp:356 | lit via `ResolveExport` | `??1Goal@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/statgoal.cpp:465 | lit via `ResolveExport` | `??0Goal@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp:130 | lit via `ResolveExport` | `??1GoalInstance@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp:166 | lit via `ResolveExport` | `??1Goal@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp:258 | lit via `ResolveExport` | `??0Goal@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Goal/timegoalinstance.cpp:224 | lit via `ResolveExport` | `??0GoalInstance@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Tutorial/finishtutorialinstance.cpp:302 | lit via `ResolveExport` | `??0GoalInstance@Achievement@@QEAA@XZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:173 | lit via `ResolveExport` | `?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:178 | lit via `ResolveExport` | `?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:190 | lit via `ResolveExport` | `?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@XZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:203 | lit via `ResolveExport` | `?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:255 | lit via `ResolveExport` | `??_7Achievement@0@6B@` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:527 | lit via `ResolveExport` | `??_7AchievementInstance@Achievement@@6B@` |
| Achievement | runtime/src/achievement/achievement_state.cpp:377 | lit via `ResolveExport` | `??_7AchievementInstance@Achievement@@6B@` |
| Achievement | runtime/src/achievement/goal.cpp:124 | lit via `ResolveExport` | `??_7Goal@Achievement@@6B@` |
| Achievement | runtime/src/achievement/goal.cpp:128 | lit via `ResolveExport` | `??_7GoalInstance@Achievement@@6B@` |
| Achievement | standalone/address_registry.cpp:1247 | lit via `ResolveExport` | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | standalone/reg_goal.cpp:73 | lit via `ResolveExport` | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` |
| Achievement | standalone/reg_goal.cpp:80 | lit via `ResolveExport` | `?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z` |
| Achievement | standalone/reg_goal.cpp:82 | lit via `ResolveExport` | `?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z` |
| Achievement | standalone/reg_unreg_lane1.cpp:212 | lit via `ResolveExport` | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | code/Game/Game/game_main.cpp:270 | lit via `ResolveFn` | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | code/Game/Game/scenario.cpp:75 | rthex via `ResolveAddr` | `??0Achievement@0@QEAA@XZ` |
| Achievement | code/Game/Game/scenario.cpp:77 | rthex via `ResolveAddr` | `??1Achievement@0@UEAA@XZ` |
| Achievement | code/Game/Game/scenario.cpp:79 | rthex via `ResolveAddr` | `?GetFileVersion@Achievement@1@UEBAHXZ` |
| Achievement | code/Game/Game/scenario.cpp:81 | rthex via `ResolveAddr` | `?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z` |
| Application | runtime/src/application/application_accessors.cpp:48 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/application/platform_startup_win.cpp:113 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/singleton_ctors.cpp:161 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_device_update.cpp:172 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_platform_shutdown_win.cpp:165 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_platform_startup_win.cpp:110 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_win.cpp:143 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/video/device_startup.cpp:159 | lit via `RT_DATA_SLOT` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/engine_lifecycle.cpp:588 | lit via `RT_EXPORT_ONCE` | `??0Application@0@QEAA@PEAX@Z` |
| Application | runtime/src/engine/engine_lifecycle.cpp:591 | lit via `RT_EXPORT_ONCE` | `?PlatformStartup@Application@1@AEAA_NXZ` |
| Application | runtime/src/engine/engine_lifecycle.cpp:810 | lit via `RT_EXPORT_ONCE` | `?PlatformShutdown@Application@1@AEAAXXZ` |
| Application | runtime/src/engine/engine_lifecycle.cpp:885 | lit via `RT_EXPORT_ONCE` | `??1Application@0@QEAA@XZ` |
| Application | runtime/src/input/input_platform_shutdown_win.cpp:216 | lit via `RT_EXPORT_ONCE` | `?UnregisterMessageCallback@Application@1@QEAAXI@Z` |
| Application | runtime/src/input/input_platform_startup_win.cpp:132 | lit via `RT_EXPORT_ONCE` | `?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z` |
| Application | code/Game/Action/keystone_a_zone.cpp:69 | lit via `ResolveExport` | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | code/Game/Game/gameupdateloop.cpp:653 | lit via `ResolveExport` | `?IsPaused@Application@1@QEAA_NXZ` |
| Application | code/Game/Game/keystone_a_gamedtor.cpp:91 | lit via `ResolveExport` | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | code/Game/Player/pathtool_setselected.cpp:87 | lit via `ResolveExport` | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | runtime/src/engine/loops.cpp:1061 | lit via `ResolveExport` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/loops.cpp:1070 | lit via `ResolveExport` | `?SleepThread@Application@1@QEAAXH@Z` |
| Application | code/Game/Game/game_main.cpp:364 | lit via `ResolveFn` | `?PlatformShutdown@Application@1@AEAAXXZ` |
| Application | runtime/src/ui/userinterface.cpp:299 | lit via `rtuielem::AtomicExport` | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/mainloop_run.cpp:284 | rthex via `ResolveAddr` | `?PlatformUpdate@Application@1@AEAA_NXZ` |
| Application | standalone/address_registry.cpp:1544 | rthex via `ResolveAddr` | `?_singleton@Application@1@0PEAV11@EA` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp:133 | exehex via `RSlot` | `?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp:134 | exehex via `RSlot` | `?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp:135 | exehex via `RSlot` | `?StopEffect3D@Scene@Audio@@QEAAXI@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp:136 | exehex via `RSlot` | `?GetEmitterCount@Scene@Audio@@QEBAHI@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp:137 | exehex via `RSlot` | `?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z` |
| Audio | code/Game/Tutorial/finishtutorialinstance.cpp:72 | exehex via `ResolveAddr` | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | runtime/src/audio/audio.cpp:1204 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio.cpp:824 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio_scene_update.cpp:288 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio_scene_update.cpp:293 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/batchc_audio_leaves.cpp:94 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/batchc_audio_leaves.cpp:98 | lit via `RT_DATA_SLOT` | `??_7SoundBuffer@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:181 | lit via `RT_DATA_SLOT` | `??_7SoundBuffer@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:183 | lit via `RT_DATA_SLOT` | `?_typeInfo@SoundBuffer@Audio@@0VBaseTypeInfo@Object@@A` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:305 | lit via `RT_DATA_SLOT` | `??_7SoundEffect@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:307 | lit via `RT_DATA_SLOT` | `?_typeInfo@SoundEffect@Audio@@0VBaseTypeInfo@Object@@A` |
| Audio | runtime/src/audio/vtbatch7b_stream_run_win.cpp:276 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/engine_platform.cpp:141 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/singleton_ctors.cpp:164 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/singleton_ctors.cpp:165 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio.cpp:550 | lit via `RT_EXPORT_ONCE` | `?FindFree3DEffect@Scene@Audio@@AEBAHV?$ExternalPtr@VSoundEffect@Audio@@@Object@@AEBVTime@System@@AEBVVector3@Math@@@Z` |
| Audio | runtime/src/audio/audio.cpp:845 | lit via `RT_EXPORT_ONCE` | `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:331 | lit via `RT_EXPORT_ONCE` | `?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:637 | lit via `RT_EXPORT_ONCE` | `??0Configuration@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:639 | lit via `RT_EXPORT_ONCE` | `?Startup@Configuration@Audio@@QEAA_NXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:681 | lit via `RT_EXPORT_ONCE` | `??0Device@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:683 | lit via `RT_EXPORT_ONCE` | `?PlatformStartup@Device@Audio@@AEAA_NXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:789 | lit via `RT_EXPORT_ONCE` | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:863 | lit via `RT_EXPORT_ONCE` | `??1Stream@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp:868 | lit via `RT_EXPORT_ONCE` | `??1Configuration@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_platform.cpp:284 | lit via `RT_EXPORT_ONCE` | `?Startup@Configuration@Audio@@QEAA_NXZ` |
| Audio | runtime/src/engine/mainloop_frame.cpp:417 | lit via `RT_EXPORT_ONCE` | `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` |
| Audio | runtime/src/engine/singleton_ctors.cpp:760 | lit via `RT_EXPORT_ONCE` | `??0Stream@Audio@@QEAA@XZ` |
| Audio | code/Game/Game/gameupdateloop.cpp:629 | lit via `ResolveExport` | `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` |
| Audio | code/Game/Game/gameupdateloop.cpp:631 | lit via `ResolveExport` | `?StopAll@Scene@Audio@@QEAAXXZ` |
| Audio | code/Game/Game/gameupdateloop.cpp:641 | lit via `ResolveExport` | `??1Scene@Audio@@QEAA@XZ` |
| Audio | code/Game/Game/gameupdateloop_serialize.cpp:159 | lit via `ResolveExport` | `?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z` |
| Audio | code/Game/Game/menudialogloopt.cpp:1241 | lit via `ResolveExport` | `?StopAll@Scene@Audio@@QEAAXXZ` |
| Audio | code/Game/Game/unreg_lane5_musicplayer.cpp:127 | lit via `ResolveExport` | `?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z` |
| Audio | runtime/src/audio/audio_play_platform.cpp:473 | lit via `ResolveExport` | `?Play@Stream@Audio@@QEAAXHH@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:391 | lit via `ResolveExport` | `?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@AEBVVector3@Math@@M@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:393 | lit via `ResolveExport` | `?StopEffect@Scene@Audio@@QEAAXI@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:395 | lit via `ResolveExport` | `?StopEffect3D@Scene@Audio@@QEAAXI@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:397 | lit via `ResolveExport` | `?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp:504 | lit via `ResolveExport` | `?PlayPlatform@Device@Audio@@AEAA_NHV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMM@Z` |
| Audio | code/Game/Game/game_main.cpp:307 | lit via `ResolveFn` | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | runtime/src/ui/userinterface_standalone.cpp:153 | lit via `rtuielem::AtomicExport` | `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` |
| Audio | runtime/src/audio/audio_stream_win.cpp:296 | rthex via `ResolveAddr` | `??_7Stream@Audio@@6B@` |
| Audio | runtime/src/engine/mainloop_run.cpp:282 | rthex via `ResolveAddr` | `?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z` |
| Engine | runtime/src/ui/userinterface_showdialog.cpp:213 | lit via `AtomicByName` | `?Frame@MainLoop@Engine@@QEAA_N_N@Z` |
| Engine | runtime/src/ui/userinterface_showdialog.cpp:214 | lit via `AtomicByName` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/engine.cpp:433 | lit via `CallVideoCallback` | `?OnVideoError@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:441 | lit via `CallVideoCallback` | `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:230 | lit via `RT_DATA_SLOT` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/engine_lifecycle.cpp:281 | lit via `RT_DATA_SLOT` | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/engine_platform.cpp:150 | lit via `RT_DATA_SLOT` | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/loops.cpp:314 | lit via `RT_DATA_SLOT` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/mainloop_video.cpp:358 | lit via `RT_DATA_SLOT` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/engine.cpp:435 | lit via `RT_EXPORT_ONCE` | `?OnVideoError@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:443 | lit via `RT_EXPORT_ONCE` | `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:556 | lit via `RT_EXPORT_ONCE` | `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` |
| Engine | runtime/src/engine/engine_lifecycle.cpp:663 | lit via `RT_EXPORT_ONCE` | `?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z` |
| Engine | runtime/src/engine/mainloop_frame.cpp:521 | lit via `RT_EXPORT_ONCE` | `?Draw@MainLoop@Engine@@QEAAXM@Z` |
| Engine | runtime/src/engine/mainloop_run.cpp:319 | lit via `RT_EXPORT_ONCE` | `?Frame@MainLoop@Engine@@QEAA_N_N@Z` |
| Engine | code/Game/Game/game_main.cpp:174 | lit via `ResolveExport` | `??0Engine@0@QEAA@PEAX@Z` |
| Engine | code/Game/Game/game_main.cpp:176 | lit via `ResolveExport` | `??1Engine@0@QEAA@XZ` |
| Engine | code/Game/Game/game_main.cpp:178 | lit via `ResolveExport` | `?Startup@Engine@1@QEAAIPEBDII_N@Z` |
| Engine | code/Game/Game/game_main.cpp:180 | lit via `ResolveExport` | `?Shutdown@Engine@1@QEAAXXZ` |
| Engine | code/Game/Game/gameupdateloop.cpp:2806 | lit via `ResolveExport` | `??1UpdateLoop@Engine@@UEAA@XZ` |
| Engine | runtime/src/engine/loops.cpp:1025 | lit via `ResolveExport` | `?_loader@LoaderLoop@Engine@@1VLoader@2@A` |
| Engine | runtime/src/engine/loops.cpp:1109 | lit via `ResolveExport` | `?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | runtime/src/engine/loops.cpp:1311 | lit via `ResolveExport` | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/loops.cpp:1335 | lit via `ResolveExport` | `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/loops.cpp:326 | lit via `ResolveExport` | `??_7UpdateLoop@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp:330 | lit via `ResolveExport` | `??_7LoaderLoop@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp:334 | lit via `ResolveExport` | `??_7DialogLoopT@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp:338 | lit via `ResolveExport` | `??_7DialogLoaderLoopT@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp:878 | lit via `ResolveExport` | `?_loader@LoaderLoop@Engine@@1VLoader@2@A` |
| Engine | runtime/src/engine/loops.cpp:883 | lit via `ResolveExport` | `?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | standalone/reg_unreg_lane1.cpp:196 | lit via `ResolveExport` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | standalone/reg_unreg_lane3.cpp:136 | lit via `ResolveExport` | `?IsRunning@UpdateLoop@Engine@@QEBA_NXZ` |
| Engine | runtime/src/save/saveload_update.cpp:1117 | lit via `RtDataExport` | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | code/Game/Game/game_main.cpp:184 | rthex via `ResolveAddr` | `??0MainLoop@Engine@@QEAA@PEAV11@@Z` |
| Engine | code/Game/Game/game_main.cpp:186 | rthex via `ResolveAddr` | `??1MainLoop@Engine@@QEAA@XZ` |
| Engine | code/Game/Game/game_main.cpp:188 | rthex via `ResolveAddr` | `?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | runtime/src/engine/mainloop_video.cpp:717 | rthex via `ResolveAddr` | `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` |
| Input | code/Game/Player/pathtool.cpp:117 | exehex via `ResolveAddr` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | code/Game/Player/edittool.cpp:212 | exehex via `resolveIAT` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | code/Game/Player/placementtool.cpp:146 | exehex via `resolveIAT` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | runtime/src/ui/editelement.cpp:89 | lit via `AtomicExport` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/editelement.cpp:93 | lit via `AtomicExport` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/engine_platform.cpp:144 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/singleton_ctors.cpp:162 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/singleton_ctors.cpp:163 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input.cpp:113 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input.cpp:604 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_device_update.cpp:175 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_platform_startup_win.cpp:107 | lit via `RT_DATA_SLOT` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_platform_startup_win.cpp:180 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/combodialog.cpp:213 | lit via `RT_DATA_SLOT` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/engine_lifecycle.cpp:648 | lit via `RT_EXPORT_ONCE` | `??0Configuration@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:650 | lit via `RT_EXPORT_ONCE` | `?Startup@Configuration@Input@@QEAA_NXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:692 | lit via `RT_EXPORT_ONCE` | `??0Device@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:694 | lit via `RT_EXPORT_ONCE` | `?PlatformStartup@Device@Input@@AEAA_NXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:755 | lit via `RT_EXPORT_ONCE` | `?PlatformShutdown@Device@Input@@AEAAXXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:758 | lit via `RT_EXPORT_ONCE` | `?Save@Configuration@Input@@QEAAXXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp:857 | lit via `RT_EXPORT_ONCE` | `??1Configuration@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_platform.cpp:239 | lit via `RT_EXPORT_ONCE` | `?Startup@Configuration@Input@@QEAA_NXZ` |
| Input | runtime/src/engine/mainloop_frame.cpp:404 | lit via `RT_EXPORT_ONCE` | `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` |
| Input | runtime/src/engine/mainloop_run.cpp:296 | lit via `RT_EXPORT_ONCE` | `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` |
| Input | runtime/src/input/input_config_startup.cpp:147 | lit via `RT_EXPORT_ONCE` | `?Load@KeyMap@Input@@QEAAXXZ` |
| Input | runtime/src/input/input_platform_startup_win.cpp:123 | lit via `RT_EXPORT_ONCE` | `?PlatformUpdate@Device@Input@@AEAAXXZ` |
| Input | standalone/mouse_inject.cpp:350 | lit via `ResolveExport` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | code/Game/Game/game_main.cpp:277 | lit via `ResolveFn` | `?PlatformShutdown@Device@Input@@AEAAXXZ` |
| Input | code/Game/Game/game_main.cpp:284 | lit via `ResolveFn` | `?Save@Configuration@Input@@QEAAXXZ` |
| Input | runtime/src/ui/element_bodies_b5.cpp:203 | lit via `rtuielem::AtomicExport` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface.cpp:303 | lit via `rtuielem::AtomicExport` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_draw.cpp:175 | lit via `rtuielem::AtomicExport` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_draw.cpp:179 | lit via `rtuielem::AtomicExport` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_frame.cpp:185 | lit via `rtuielem::AtomicExport` | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_frame.cpp:189 | lit via `rtuielem::AtomicExport` | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input.cpp:559 | rthex via `ResolveAddr` | `?Save@KeyMap@Input@@QEAAXXZ` |
| Input | runtime/src/ui/buttonelement.cpp:284 | rthex via `RtAddr` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | runtime/src/ui/checkelement.cpp:148 | rthex via `RtAddr` | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Pathfinding | runtime/src/pathfinding/pointlist.cpp:103 | lit via `RT_DATA_SLOT` | `??_7PointList@Pathfinding@@6B@` |
| Pathfinding | runtime/src/pathfinding/pointlist.cpp:105 | lit via `RT_DATA_SLOT` | `?_typeInfo@PointList@Pathfinding@@0VBaseTypeInfo@Object@@A` |
| Picking | code/Game/Component/pickingcomponent.cpp:314 | exehex via `ResolveAddr` | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` |
| Picking | code/Game/Component/quarrycomponent_deform.cpp:93 | exehex via `ResolveAddr` | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | code/Game/Component/terraformcomponent.cpp:307 | exehex via `ResolveAddr` | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | code/Game/Game/mapquery.cpp:154 | exehex via `ResolveAddr` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/bridgetool_calcpath.cpp:226 | exehex via `ResolveAddr` | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Picking | code/Game/Player/fencedtool_calcpath.cpp:92 | exehex via `ResolveAddr` | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Picking | code/Game/Player/pathtool.cpp:109 | exehex via `ResolveAddr` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/pathtool.cpp:113 | exehex via `ResolveAddr` | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | code/Game/Component/citizenai_executor.cpp:369 | exehex via `ResolveSlot` | `?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z` |
| Picking | code/Game/Player/edittool.cpp:213 | exehex via `resolveIAT` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/edittool.cpp:214 | exehex via `resolveIAT` | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | runtime/src/picking/pickingmesh.cpp:103 | lit via `RT_DATA_SLOT` | `??_7PickingMesh@Picking@@6B@` |
| Picking | runtime/src/picking/pickingmesh.cpp:105 | lit via `RT_DATA_SLOT` | `?_typeInfo@PickingMesh@Picking@@0VBaseTypeInfo@Object@@A` |
| Picking | code/Game/Component/pickingcomponent.cpp:147 | lit via `ResolveExport` | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Game/gameupdateloop.cpp:643 | lit via `ResolveExport` | `??1Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/gameupdateloop_load.cpp:340 | lit via `ResolveExport` | `??0Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/keystone_a_gamedtor.cpp:94 | lit via `ResolveExport` | `??1Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/mapquery.cpp:158 | lit via `ResolveExport` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/edittool.cpp:243 | lit via `ResolveExport` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/edittool.cpp:246 | lit via `ResolveExport` | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | standalone/address_registry.cpp:1473 | lit via `ResolveExport` | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | standalone/address_registry.cpp:1483 | lit via `ResolveExport` | `?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z` |
| Picking | standalone/address_registry.cpp:1493 | lit via `ResolveExport` | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | standalone/address_registry.cpp:1499 | lit via `ResolveExport` | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | standalone/address_registry.cpp:1507 | lit via `ResolveExport` | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Picking | code/Game/Component/citizenai_executor.cpp:372 | rthex via `ResolveAddr` | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Component/floorcomponent.cpp:249 | rthex via `ResolveAddr` | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Component/floorcomponent.cpp:254 | rthex via `ResolveAddr` | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` |
| Picking | code/Game/Component/pickingcomponent.cpp:312 | rthex via `ResolveAddr` | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` |
| Save | runtime/src/engine/mainloop_video.cpp:355 | lit via `RT_DATA_SLOT` | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | runtime/src/save/saveload_internal.h:113 | lit via `RT_DATA_SLOT` | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | runtime/src/engine/mainloop_video.cpp:725 | lit via `RT_EXPORT_ONCE` | `?OnVideoRestart@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp:847 | lit via `RT_EXPORT_ONCE` | `?UpdateList@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp:850 | lit via `RT_EXPORT_ONCE` | `?UpdateLoad@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp:853 | lit via `RT_EXPORT_ONCE` | `?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp:856 | lit via `RT_EXPORT_ONCE` | `?UpdateSave@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp:859 | lit via `RT_EXPORT_ONCE` | `?UpdateDelete@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload_meta.cpp:572 | lit via `RT_EXPORT_ONCE` | `?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | runtime/src/save/saveload_update.cpp:1045 | lit via `RT_EXPORT_ONCE` | `?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z` |
| Save | runtime/src/save/saveload_update.cpp:1138 | lit via `RT_EXPORT_ONCE` | `?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ` |
| Save | runtime/src/save/saveload_update.cpp:1161 | lit via `RT_EXPORT_ONCE` | `?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ` |
| Save | runtime/src/save/saveload_update.cpp:1768 | lit via `RT_EXPORT_ONCE` | `?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ` |
| Save | runtime/src/save/saveload_update.cpp:1885 | lit via `RT_EXPORT_ONCE` | `?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ` |
| Save | runtime/src/save/saveload_update.cpp:1924 | lit via `RT_EXPORT_ONCE` | `?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z` |
| Save | runtime/src/save/saveload_update.cpp:438 | lit via `RT_EXPORT_ONCE` | `?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ` |
| Save | runtime/src/save/saveload_update.cpp:912 | lit via `RT_EXPORT_ONCE` | `?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ` |
| Save | code/Game/Game/gameupdateloop.cpp:637 | lit via `ResolveExport` | `?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z` |
| Save | code/Game/Game/gameupdateloop_load.cpp:343 | lit via `ResolveExport` | `??0SaveLoad@Save@@QEAA@XZ` |
| Save | code/Game/Game/gameupdateloop_load.cpp:344 | lit via `ResolveExport` | `??1SavedState@Save@@QEAA@XZ` |
| Save | code/Game/Game/gameupdateloop_serialize.cpp:173 | lit via `ResolveExport` | `?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | code/Game/Game/gameupdateloop_serialize.cpp:175 | lit via `ResolveExport` | `?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | standalone/address_registry.cpp:1426 | lit via `ResolveExport` | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` |
| Save | standalone/address_registry.cpp:1428 | lit via `ResolveExport` | `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` |
| Save | standalone/menudialogloopt_stubs.cpp:261 | lit via `ResolveExport` | `??0SaveLoad@Save@@QEAA@XZ` |
| Save | standalone/reg_unreg_lane1.cpp:174 | lit via `ResolveExport` | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | standalone/saveload_harness.cpp:99 | lit via `ResolveExport` | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |

## Appendix C — file-local bindings and their CALL / data-read lines (L)

| ns | file | kind | n | lines | symbol |
|---|---|---|---:|---|---|
| Achievement | code/Game/Dialog/dialog_exe_helpers.cpp | call | 1 | 216 | `?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z` |
| Achievement | code/Game/Game/game_main.cpp | call | 1 | 271 | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | code/Game/Game/gameupdateloop.cpp | call | 1 | 1231 | `?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp | call | 1 | 1238 | `?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Game/gameupdateloop.cpp | call | 1 | 1347 | `?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp | call | 1 | 2418 | `?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z` |
| Achievement | code/Game/Game/gameupdateloop.cpp | call | 3 | 1234,2740,2772 | `?IsMet@AchievementInstance@Achievement@@QEBA_NXZ` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1284 | `?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1329 | `?Reset@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1626 | `??0AchievementInstance@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1662 | `?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z` |
| Achievement | code/Game/Game/gameupdateloop_load.cpp | data | 3 | 1653,1654,1660 | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp | call | 1 | 860 | `?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp | data | 1 | 844 | `?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | code/Game/Game/gameupdateloop_serialize.cpp | data | 2 | 859,860 | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | code/Game/Game/scenario.cpp | call | 1 | 112 | `??1Achievement@0@UEAA@XZ` |
| Achievement | code/Game/Game/scenario.cpp | call | 1 | 134 | `?GetFileVersion@Achievement@1@UEBAHXZ` |
| Achievement | code/Game/Game/scenario.cpp | call | 1 | 153 | `?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | code/Game/Game/scenario.cpp | call | 1 | 219 | `??0Achievement@0@QEAA@XZ` |
| Achievement | code/Game/Goal/goal_exe_helpers.cpp:223 | resolved, never used or a registration | 0 | ResolveExport | `?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ` |
| Achievement | code/Game/Goal/statgoal.cpp | call | 1 | 360 | `??1Goal@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/statgoal.cpp | call | 1 | 469 | `??0Goal@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Goal/statgoal.cpp | call | 1 | 88 | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoal.cpp | call | 1 | 134 | `??1GoalInstance@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp | call | 1 | 170 | `??1Goal@Achievement@@UEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp | call | 1 | 262 | `??0Goal@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Goal/timegoal.cpp | call | 1 | 52 | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp | call | 1 | 100 | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp | call | 1 | 152 | `?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp | call | 1 | 161 | `?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z` |
| Achievement | code/Game/Goal/timegoalinstance.cpp | call | 1 | 228 | `??0GoalInstance@Achievement@@QEAA@XZ` |
| Achievement | code/Game/Tutorial/finishtutorialinstance.cpp | call | 1 | 306 | `??0GoalInstance@Achievement@@QEAA@XZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp | data | 1 | 256 | `??_7Achievement@0@6B@` |
| Achievement | runtime/src/achievement/achievement_manager.cpp | data | 1 | 267 | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/achievement/achievement_manager.cpp | data | 1 | 528 | `??_7AchievementInstance@Achievement@@6B@` |
| Achievement | runtime/src/achievement/achievement_manager.cpp | data | 3 | 277,829,830 | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:173 | resolved, never used or a registration | 0 | ResolveExport | `?ReadAchievements@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:178 | resolved, never used or a registration | 0 | ResolveExport | `?SetAchievementMet@AchievementManager@Achievement@@QEAA_NH@Z` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:190 | resolved, never used or a registration | 0 | ResolveExport | `?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@XZ` |
| Achievement | runtime/src/achievement/achievement_manager.cpp:203 | resolved, never used or a registration | 0 | ResolveExport | `?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z` |
| Achievement | runtime/src/achievement/achievement_serialize.cpp | call | 1 | 289 | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/achievement/achievement_state.cpp | call | 1 | 414 | `?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z` |
| Achievement | runtime/src/achievement/achievement_state.cpp | data | 2 | 377,378 | `??_7AchievementInstance@Achievement@@6B@` |
| Achievement | runtime/src/achievement/achievement_state.cpp:144 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:210 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:214 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:218 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@Goal@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/achievement_typeinfo_accessors.cpp:222 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/goal.cpp | data | 1 | 125 | `??_7Goal@Achievement@@6B@` |
| Achievement | runtime/src/achievement/goal.cpp | data | 1 | 129 | `??_7GoalInstance@Achievement@@6B@` |
| Achievement | runtime/src/achievement/goal.cpp | data | 1 | 172 | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/achievement/goal_list.cpp | data | 1 | 211 | `?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 750 | `?RemoveAll@AchievementManager@Achievement@@QEAAXXZ` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 845 | `??1AchievementManager@Achievement@@QEAA@XZ` |
| Achievement | runtime/src/engine/engine_lifecycle.cpp:288 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Achievement | runtime/src/object/deleting_dtor_leaves.cpp | call | 1 | 153 | `??1GoalList@Achievement@@QEAA@XZ` |
| Achievement | standalone/address_registry.cpp:1247 | resolved, never used or a registration | 0 | ResolveExport | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` |
| Achievement | standalone/reg_goal.cpp:73 | resolved, never used or a registration | 0 | ResolveExport | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` |
| Achievement | standalone/reg_goal.cpp:80 | resolved, never used or a registration | 0 | ResolveExport | `?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z` |
| Achievement | standalone/reg_goal.cpp:82 | resolved, never used or a registration | 0 | ResolveExport | `?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z` |
| Achievement | standalone/reg_unreg_lane1.cpp:212 | resolved, never used or a registration | 0 | ResolveExport | `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` |
| Application | code/Game/Action/keystone_a_zone.cpp | call | 1 | 159 | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | code/Game/Game/game_main.cpp | call | 1 | 365 | `?PlatformShutdown@Application@1@AEAAXXZ` |
| Application | code/Game/Game/gameupdateloop.cpp:653 | resolved, never used or a registration | 0 | ResolveExport | `?IsPaused@Application@1@QEAA_NXZ` |
| Application | code/Game/Game/keystone_a_gamedtor.cpp | call | 1 | 382 | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | code/Game/Player/pathtool_setselected.cpp | call | 1 | 565 | `?YieldToThreads@Application@1@QEAAXXZ` |
| Application | runtime/src/application/application_accessors.cpp | data | 1 | 49 | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/application/platform_startup_win.cpp:113 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 587 | `??0Application@0@QEAA@PEAX@Z` |
| Application | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 591 | `?PlatformStartup@Application@1@AEAA_NXZ` |
| Application | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 810 | `?PlatformShutdown@Application@1@AEAAXXZ` |
| Application | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 885 | `??1Application@0@QEAA@XZ` |
| Application | runtime/src/engine/loops.cpp | call | 1 | 1071 | `?SleepThread@Application@1@QEAAXH@Z` |
| Application | runtime/src/engine/loops.cpp | data | 1 | 1062 | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/engine/mainloop_run.cpp | call | 1 | 300 | `?PlatformUpdate@Application@1@AEAA_NXZ` |
| Application | runtime/src/engine/singleton_ctors.cpp:161 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_device_update.cpp:172 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_platform_shutdown_win.cpp | call | 1 | 217 | `?UnregisterMessageCallback@Application@1@QEAAXI@Z` |
| Application | runtime/src/input/input_platform_shutdown_win.cpp:165 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_platform_startup_win.cpp | call | 1 | 133 | `?RegisterMessageCallback@Application@1@QEAAXIP6AHPEAUHWND__@@I_K_J@Z@Z` |
| Application | runtime/src/input/input_platform_startup_win.cpp:110 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/input/input_win.cpp | data | 1 | 144 | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/ui/userinterface.cpp:299 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Application@1@0PEAV11@EA` |
| Application | runtime/src/video/device_startup.cpp:159 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Application@1@0PEAV11@EA` |
| Application | standalone/address_registry.cpp:1544 | resolved, never used or a registration | 0 | ResolveAddr | `?_singleton@Application@1@0PEAV11@EA` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp | call | 1 | 268 | `?RemoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp | call | 1 | 269 | `?GetEmitterCount@Scene@Audio@@QEBAHI@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp | call | 1 | 270 | `?StopEffect3D@Scene@Audio@@QEAAXI@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp | call | 1 | 324 | `?MoveEmitter@Scene@Audio@@QEAAXIAEBVVector3@Math@@0@Z` |
| Audio | code/Game/Component/citizen_assignworkplace.cpp | call | 1 | 340 | `?SetEmitterVolume@Scene@Audio@@QEAAXIAEBVVector3@Math@@M@Z` |
| Audio | code/Game/Game/game_main.cpp | call | 1 | 308 | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | code/Game/Game/gameupdateloop.cpp | call | 1 | 1510 | `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` |
| Audio | code/Game/Game/gameupdateloop.cpp | call | 1 | 2439 | `?StopAll@Scene@Audio@@QEAAXXZ` |
| Audio | code/Game/Game/gameupdateloop.cpp | call | 1 | 2610 | `??1Scene@Audio@@QEAA@XZ` |
| Audio | code/Game/Game/gameupdateloop_serialize.cpp | call | 1 | 868 | `?Serialize@Scene@Audio@@QEAAXAEAVSerializer@IO@@@Z` |
| Audio | code/Game/Game/menudialogloopt.cpp | call | 1 | 1244 | `?StopAll@Scene@Audio@@QEAAXXZ` |
| Audio | code/Game/Game/unreg_lane5_musicplayer.cpp | call | 1 | 190 | `?IsEffectPlaying@Scene@Audio@@QEBA_NI@Z` |
| Audio | code/Game/Tutorial/finishtutorialinstance.cpp | call | 1 | 85 | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | runtime/src/audio/audio.cpp | call | 1 | 844 | `?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z` |
| Audio | runtime/src/audio/audio.cpp | data | 1 | 825 | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio.cpp:1204 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio.cpp:550 | resolved, never used or a registration | 0 | RT_EXPORT_ONCE | `?FindFree3DEffect@Scene@Audio@@AEBAHV?$ExternalPtr@VSoundEffect@Audio@@@Object@@AEBVTime@System@@AEBVVector3@Math@@@Z` |
| Audio | runtime/src/audio/audio_play_platform.cpp | call | 1 | 474 | `?Play@Stream@Audio@@QEAAXHH@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | call | 1 | 413 | `?GetAttenuatedVolume@Scene@Audio@@AEBAMAEBUAttenutedEffect@12@AEBVVector3@Math@@M@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | call | 1 | 422 | `?StopEffect@Scene@Audio@@QEAAXI@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | call | 1 | 433 | `?PlayEffect@Scene@Audio@@QEAAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@AEBVTime@System@@@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | call | 1 | 461 | `?StopEffect3D@Scene@Audio@@QEAAXI@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | call | 1 | 614 | `?PlayPlatform@Device@Audio@@AEAA_NHV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMM@Z` |
| Audio | runtime/src/audio/audio_scene_update.cpp | data | 1 | 289 | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio_scene_update.cpp | data | 1 | 294 | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/audio_scene_update.cpp:331 | resolved, never used or a registration | 0 | RT_EXPORT_ONCE | `?OnSoundEnded@Scene@Audio@@SAXPEAUAudioChannel@Device@2@PEAXI@Z` |
| Audio | runtime/src/audio/audio_stream_win.cpp | data | 2 | 296,297 | `??_7Stream@Audio@@6B@` |
| Audio | runtime/src/audio/batchc_audio_leaves.cpp | data | 3 | 95,115,116 | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/audio/batchc_audio_leaves.cpp:98 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `??_7SoundBuffer@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:181 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `??_7SoundBuffer@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:183 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@SoundBuffer@Audio@@0VBaseTypeInfo@Object@@A` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:305 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `??_7SoundEffect@Audio@@6B@` |
| Audio | runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp:307 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@SoundEffect@Audio@@0VBaseTypeInfo@Object@@A` |
| Audio | runtime/src/audio/vtbatch7b_stream_run_win.cpp | data | 1 | 277 | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 636 | `??0Configuration@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 639 | `?Startup@Configuration@Audio@@QEAA_NXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 680 | `??0Device@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 683 | `?PlatformStartup@Device@Audio@@AEAA_NXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 789 | `?Save@Configuration@Audio@@QEAAXXZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 863 | `??1Stream@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 868 | `??1Configuration@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/engine_platform.cpp | call | 1 | 284 | `?Startup@Configuration@Audio@@QEAA_NXZ` |
| Audio | runtime/src/engine/engine_platform.cpp:141 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/mainloop_frame.cpp | call | 1 | 416 | `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` |
| Audio | runtime/src/engine/mainloop_run.cpp | call | 1 | 299 | `?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z` |
| Audio | runtime/src/engine/singleton_ctors.cpp | call | 1 | 760 | `??0Stream@Audio@@QEAA@XZ` |
| Audio | runtime/src/engine/singleton_ctors.cpp:164 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Device@Audio@@0PEAV12@EA` |
| Audio | runtime/src/engine/singleton_ctors.cpp:165 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Audio@@0PEAV12@EA` |
| Audio | runtime/src/ui/userinterface_standalone.cpp | call | 1 | 152 | `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 446 | `??0Engine@0@QEAA@PEAX@Z` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 461 | `?Startup@Engine@1@QEAAIPEBDII_N@Z` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 588 | `??0MainLoop@Engine@@QEAA@PEAV11@@Z` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 845 | `?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 888 | `??1MainLoop@Engine@@QEAA@XZ` |
| Engine | code/Game/Game/game_main.cpp | call | 1 | 905 | `??1Engine@0@QEAA@XZ` |
| Engine | code/Game/Game/game_main.cpp:180 | resolved, never used or a registration | 0 | ResolveExport | `?Shutdown@Engine@1@QEAAXXZ` |
| Engine | code/Game/Game/gameupdateloop.cpp | call | 1 | 2822 | `??1UpdateLoop@Engine@@UEAA@XZ` |
| Engine | runtime/src/engine/engine.cpp | call | 1 | 556 | `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp | data | 1 | 231 | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/engine.cpp:433 | resolved, never used or a registration | 0 | CallVideoCallback | `?OnVideoError@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:435 | resolved, never used or a registration | 0 | RT_EXPORT_ONCE | `?OnVideoError@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:441 | resolved, never used or a registration | 0 | CallVideoCallback | `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine.cpp:443 | resolved, never used or a registration | 0 | RT_EXPORT_ONCE | `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 663 | `?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z` |
| Engine | runtime/src/engine/engine_lifecycle.cpp:281 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/engine_platform.cpp:150 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/loops.cpp | call | 1 | 1336 | `?LoadResources@UpdateLoop@Engine@@QEAAXXZ` |
| Engine | runtime/src/engine/loops.cpp | call | 2 | 884,1110 | `?Load@Loader@Engine@@QEAAXPEAVUpdateLoop@2@@Z` |
| Engine | runtime/src/engine/loops.cpp | data | 1 | 315 | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/engine/loops.cpp | data | 1 | 327 | `??_7UpdateLoop@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp | data | 1 | 331 | `??_7LoaderLoop@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp | data | 1 | 335 | `??_7DialogLoopT@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp | data | 1 | 339 | `??_7DialogLoaderLoopT@Engine@@6B@` |
| Engine | runtime/src/engine/loops.cpp | data | 2 | 1314,1316 | `?_singleton@Engine@1@0PEAV11@EA` |
| Engine | runtime/src/engine/loops.cpp | data | 2 | 884,1026 | `?_loader@LoaderLoop@Engine@@1VLoader@2@A` |
| Engine | runtime/src/engine/mainloop_frame.cpp | call | 1 | 521 | `?Draw@MainLoop@Engine@@QEAAXM@Z` |
| Engine | runtime/src/engine/mainloop_run.cpp | call | 1 | 319 | `?Frame@MainLoop@Engine@@QEAA_N_N@Z` |
| Engine | runtime/src/engine/mainloop_video.cpp | call | 1 | 716 | `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` |
| Engine | runtime/src/engine/mainloop_video.cpp:358 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/save/saveload_update.cpp | data | 2 | 1118,1119 | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | runtime/src/ui/userinterface_showdialog.cpp:213 | resolved, never used or a registration | 0 | AtomicByName | `?Frame@MainLoop@Engine@@QEAA_N_N@Z` |
| Engine | runtime/src/ui/userinterface_showdialog.cpp:214 | resolved, never used or a registration | 0 | AtomicByName | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | standalone/reg_unreg_lane1.cpp:196 | resolved, never used or a registration | 0 | ResolveExport | `?_singleton@MainLoop@Engine@@1PEAV12@EA` |
| Engine | standalone/reg_unreg_lane3.cpp:136 | resolved, never used or a registration | 0 | ResolveExport | `?IsRunning@UpdateLoop@Engine@@QEBA_NXZ` |
| Input | code/Game/Game/game_main.cpp | call | 1 | 278 | `?PlatformShutdown@Device@Input@@AEAAXXZ` |
| Input | code/Game/Game/game_main.cpp | call | 1 | 285 | `?Save@Configuration@Input@@QEAAXXZ` |
| Input | code/Game/Player/edittool.cpp | call | 1 | 711 | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | code/Game/Player/pathtool.cpp | call | 1 | 196 | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | code/Game/Player/placementtool.cpp | call | 4 | 371,386,400,411 | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 647 | `??0Configuration@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 650 | `?Startup@Configuration@Input@@QEAA_NXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 691 | `??0Device@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 694 | `?PlatformStartup@Device@Input@@AEAA_NXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 754 | `?PlatformShutdown@Device@Input@@AEAAXXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 758 | `?Save@Configuration@Input@@QEAAXXZ` |
| Input | runtime/src/engine/engine_lifecycle.cpp | call | 1 | 857 | `??1Configuration@Input@@QEAA@XZ` |
| Input | runtime/src/engine/engine_platform.cpp | call | 1 | 239 | `?Startup@Configuration@Input@@QEAA_NXZ` |
| Input | runtime/src/engine/engine_platform.cpp:144 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/mainloop_frame.cpp | call | 1 | 403 | `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` |
| Input | runtime/src/engine/mainloop_run.cpp | call | 1 | 296 | `?Update@Device@Input@@QEAAXAEBVTime@System@@@Z` |
| Input | runtime/src/engine/singleton_ctors.cpp:162 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/engine/singleton_ctors.cpp:163 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input.cpp | call | 1 | 560 | `?Save@KeyMap@Input@@QEAAXXZ` |
| Input | runtime/src/input/input.cpp | data | 5 | 114,187,192,193,195 | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input.cpp:604 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_config_startup.cpp | call | 1 | 147 | `?Load@KeyMap@Input@@QEAAXXZ` |
| Input | runtime/src/input/input_device_update.cpp:175 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_platform_startup_win.cpp | call | 1 | 124 | `?PlatformUpdate@Device@Input@@AEAAXXZ` |
| Input | runtime/src/input/input_platform_startup_win.cpp:107 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/input/input_platform_startup_win.cpp:180 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/buttonelement.cpp | call | 1 | 284 | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | runtime/src/ui/checkelement.cpp | call | 1 | 147 | `?Pressed@KeyMap@Input@@QEBA_NHH@Z` |
| Input | runtime/src/ui/combodialog.cpp | data | 1 | 214 | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/editelement.cpp:89 | resolved, never used or a registration | 0 | AtomicExport | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/editelement.cpp:93 | resolved, never used or a registration | 0 | AtomicExport | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/element_bodies_b5.cpp | data | 1 | 204 | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface.cpp:303 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_draw.cpp:175 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_draw.cpp:179 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_frame.cpp:185 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Device@Input@@0PEAV12@EA` |
| Input | runtime/src/ui/userinterface_frame.cpp:189 | resolved, never used or a registration | 0 | rtuielem::AtomicExport | `?_singleton@Configuration@Input@@0PEAV12@EA` |
| Input | standalone/mouse_inject.cpp | data | 3 | 348,351,352 | `?_singleton@Device@Input@@0PEAV12@EA` |
| Pathfinding | runtime/src/pathfinding/pointlist.cpp:103 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `??_7PointList@Pathfinding@@6B@` |
| Pathfinding | runtime/src/pathfinding/pointlist.cpp:105 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@PointList@Pathfinding@@0VBaseTypeInfo@Object@@A` |
| Picking | code/Game/Component/citizenai_executor.cpp | call | 1 | 427 | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Component/citizenai_executor.cpp | call | 1 | 687 | `?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z` |
| Picking | code/Game/Component/floorcomponent.cpp | call | 1 | 301 | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` |
| Picking | code/Game/Component/floorcomponent.cpp | call | 1 | 396 | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Component/pickingcomponent.cpp | call | 1 | 215 | `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@AEBVTransform@Math@@PEAXHHP6AX2HHAEBVVector3@6@33@Z@Z` |
| Picking | code/Game/Component/pickingcomponent.cpp | call | 1 | 81 | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` |
| Picking | code/Game/Component/quarrycomponent_deform.cpp | call | 2 | 230,236 | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | code/Game/Component/terraformcomponent.cpp | call | 2 | 435,441 | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | code/Game/Game/gameupdateloop.cpp | call | 1 | 2617 | `??1Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/gameupdateloop_load.cpp | call | 2 | 641,1120 | `??0Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/keystone_a_gamedtor.cpp | call | 1 | 452 | `??1Scene@Picking@@QEAA@XZ` |
| Picking | code/Game/Game/mapquery.cpp | call | 1 | 224 | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/bridgetool_calcpath.cpp | call | 1 | 232 | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Picking | code/Game/Player/edittool.cpp | call | 1 | 810 | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/edittool.cpp | call | 2 | 815,1058 | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | code/Game/Player/fencedtool_calcpath.cpp | call | 1 | 145 | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Picking | code/Game/Player/pathtool.cpp | call | 1 | 247 | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | code/Game/Player/pathtool.cpp | call | 1 | 250 | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | runtime/src/picking/pickingmesh.cpp:103 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `??_7PickingMesh@Picking@@6B@` |
| Picking | runtime/src/picking/pickingmesh.cpp:105 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_typeInfo@PickingMesh@Picking@@0VBaseTypeInfo@Object@@A` |
| Picking | standalone/address_registry.cpp:1473 | resolved, never used or a registration | 0 | ResolveExport | `?Vertices@PickingMesh@Picking@@QEBAAEBV?$Array@VVector3@Math@@@Collection@@H@Z` |
| Picking | standalone/address_registry.cpp:1483 | resolved, never used or a registration | 0 | ResolveExport | `?SetTransform@Scene@Picking@@QEAAXIAEBVTransform@Math@@@Z` |
| Picking | standalone/address_registry.cpp:1493 | resolved, never used or a registration | 0 | ResolveExport | `?Intersect@Scene@Picking@@QEBAHAEBVVector3@Math@@0AEBVScalar@4@AEAV34@2@Z` |
| Picking | standalone/address_registry.cpp:1499 | resolved, never used or a registration | 0 | ResolveExport | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` |
| Picking | standalone/address_registry.cpp:1507 | resolved, never used or a registration | 0 | ResolveExport | `?SetMeshUserData@Scene@Picking@@QEAAXIPEAXHHP6AX0HHAEBVVector3@Math@@11@Z@Z` |
| Save | code/Game/Game/gameupdateloop.cpp | call | 1 | 1878 | `?Draw@SaveLoad@Save@@QEAAXAEAVContext@Video@@V?$ExternalPtr@VMaterial@Graphics@@@Object@@@Z` |
| Save | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1175 | `??0SaveLoad@Save@@QEAA@XZ` |
| Save | code/Game/Game/gameupdateloop_load.cpp | call | 1 | 1533 | `??1SavedState@Save@@QEAA@XZ` |
| Save | code/Game/Game/gameupdateloop_serialize.cpp | call | 1 | 936 | `?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | code/Game/Game/gameupdateloop_serialize.cpp | call | 2 | 928,968 | `?Retrieve@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | runtime/src/engine/mainloop_video.cpp | call | 1 | 726 | `?OnVideoRestart@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/engine/mainloop_video.cpp:355 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | runtime/src/save/saveload.cpp | call | 1 | 846 | `?UpdateList@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp | call | 1 | 849 | `?UpdateLoad@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp | call | 1 | 852 | `?UpdateLoadFromFile@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp | call | 1 | 855 | `?UpdateSave@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload.cpp | call | 1 | 858 | `?UpdateDelete@SaveLoad@Save@@QEAAXXZ` |
| Save | runtime/src/save/saveload_internal.h:113 | resolved, never used or a registration | 0 | RT_DATA_SLOT | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | runtime/src/save/saveload_meta.cpp | call | 1 | 572 | `?Store@SavedState@Save@@QEAAXAEAVMemoryStream@IO@@@Z` |
| Save | runtime/src/save/saveload_update.cpp | call | 1 | 1050 | `?SaveMetaData@SaveLoad@Save@@QEAAXPEAXHHVString@System@@H@Z` |
| Save | runtime/src/save/saveload_update.cpp | call | 1 | 1165 | `?GenerateSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ` |
| Save | runtime/src/save/saveload_update.cpp | call | 1 | 1931 | `?LoadMetaData@SaveLoad@Save@@QEAAXAEAVString@System@@PEAPEAXAEAH22AEAV?$Array@UModInfo@SaveLoad@Save@@@Collection@@@Z` |
| Save | runtime/src/save/saveload_update.cpp | call | 2 | 1142,1769 | `?GetAutoSaveName@SaveLoad@Save@@QEBA?AVString@System@@XZ` |
| Save | runtime/src/save/saveload_update.cpp | call | 3 | 442,916,1892 | `?SetupLoad@SavedState@Save@@QEAA?AUSaveBlock@Persistence@IO@@XZ` |
| Save | standalone/address_registry.cpp:1426 | resolved, never used or a registration | 0 | ResolveExport | `?CheckMod@SaveLoad@Save@@QEBAHHH@Z` |
| Save | standalone/address_registry.cpp:1428 | resolved, never used or a registration | 0 | ResolveExport | `?SetSaveName@SaveLoad@Save@@QEAAXAEBVString@System@@H@Z` |
| Save | standalone/menudialogloopt_stubs.cpp | call | 1 | 264 | `??0SaveLoad@Save@@QEAA@XZ` |
| Save | standalone/reg_unreg_lane1.cpp:174 | resolved, never used or a registration | 0 | ResolveExport | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
| Save | standalone/saveload_harness.cpp | data | 4 | 97,102,106,107 | `?_singleton@SaveLoad@Save@@0PEAV12@EA` |
