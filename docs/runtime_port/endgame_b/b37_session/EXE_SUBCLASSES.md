# B.3 step 7 — exe-phase subclasses of Runtime `Engine::` / `Achievement::` / `Picking::` / `Audio::` / `Input::` / `Save::` classes

Read-only enumeration, 2026-09-26, main @ 96b03474 (block 0, agent 0X). Every claim carries
file:line. Model: `b36_session/EXE_SUBCLASSES.md`. Scope: `code/Game/**`, `standalone/**`,
`engine/**`; `runtime/` only for the retail slot lists. No source was edited, nothing was built
or run. Retail slot contents come from `standalone/rt_data_mint_tables.inc` (the `kVSlots*`
arrays, which are transcribed from the shipped DLL) joined to `standalone/rt_export_addrs.inc`.

Conventions (same as step 6)
- "slot N" = vtable index N (byte offset 8N).
- "own" = the file installs its own body; "base" = the slot holds, or is filled with, the Runtime
  base virtual; "stub" = an own standalone body that REIMPLEMENTS or no-ops a slot that retail
  inherits from the Runtime base (retail holds an import thunk there); "null" = literal `nullptr`
  that nothing fills.
- "retail" columns are what the exe's table holds (from each file's banner, which cites the exe
  address per slot); "standalone" is what our table holds.
- A QUALIFIED site is a call, inside a slot body, that reaches a VIRTUAL member of the Runtime
  base through any door. After conversion it must be spelled `->Engine::DialogLoopT::Load()`
  etc. (B36_RULES B6-3), or it re-dispatches into the subclass's own slot. Base-dtor door calls
  in slot 0 are counted too, as step 6 did.

---------------------------------------------------------------------------------------------------
## 0. The retail base slot lists these subclasses sit on

| Runtime base | mint row | slots | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Engine::UpdateLoop` 0x180155ba0 | rt_data_mint_tables.inc:3735 | 12 | dtor | *pure* Startup | *pure* Shutdown | *pure* Update | *pure* Draw | DrawFailed | *pure* Load | SaveRestore | OnVideoError | OnVideoRestart | GetResourceFilename | AllowsModChanges | — |
| `Engine::LoaderLoop` 0x180157940 | :2834 | 12 | dtor | Startup | Shutdown | Update | Draw | (UL) DrawFailed | Load | (UL) SaveRestore | OnVideoError | (UL) OnVideoRestart | (UL) GetResourceFilename | (UL) AllowsModChanges | — |
| `Engine::DialogLoopT` 0x1801579a0 | :2220 | 13 | dtor | Startup | Shutdown | Update | Draw | (UL) | Load | (UL) | (UL) OnVideoError | (UL) | (UL) | (UL) | *pure* CreateDialog |
| `Engine::DialogLoaderLoopT` 0x1801578d0 | :2201 | 13 | dtor | Startup | Shutdown | Update | Draw | (UL) | Load | (UL) | (LL) OnVideoError | (UL) | (UL) | (UL) | *pure* CreateDialog |
| `Achievement::Achievement` 0x180154740 | :1545 | 9 | dtor | TypeName | Type | GetFileVersion | (External) Compile | Serialize | Reconstruct | Destruct | Construct | — | | | |
| `Achievement::Goal` 0x1801546b0 | :2501 | 10 | dtor | TypeName | Type | GetFileVersion | (External) Compile | Serialize | *pure* | *pure* | *pure* | GetInstanceType (by-value `Object::Type`) | | | |
| `Achievement::GoalInstance` 0x180154678 | :2517 | 7 | dtor | TypeName | Type | Persist | Startup | Update | CheckGoalState | | | | | | |
| `Achievement::AchievementInstance` 0x180154700 | :1560 | 4 | dtor | TypeName | Type | Persist | | | | | | | | | |

(UL)/(LL) = the slot holds `UpdateLoop`'s / `LoaderLoop`'s body, not the class's own.
Every slot-0 dtor is UEAA (virtual) in the export directory.

---------------------------------------------------------------------------------------------------
## 1. Complete list of hand-built-vtable exe subclasses in these namespaces

Found by: the five loop tables named in the brief; `RegisterExeAddr` of every table whose file
calls a ctor door of one of these namespaces (`grep` for `??0…@(Engine|Achievement|Audio|Picking|
Save|Input)` literals and the `fn_*_Ctor`/`s_fn_*Ctor` doors, `sites.tsv`/`localbind.txt` in the
scratchpad); the three `engine/types` headers; `scenario.cpp` and `gameupdateloop_load.cpp:1633`
turned up that way. **Twelve exe classes, fourteen tables** (the brief named five loops and seven
goals; `Scenario` and the exe `AchievementInstance` are new).

### 1A. `Engine::` loop subclasses — 4 classes, 5 tables

| Class | Table var | File:line | Slots | Retail vtable | Alloc | Base ctor (door, site) | Dtor calls base? |
|---|---|---|---|---|---|---|---|
| GameUpdateLoop (base `UpdateLoop`) | `g_GameUpdateLoop_Vtable_Standalone` | standalone/gameupdateloop_stubs.cpp:149, reg :165-167 | **12** (the inventory's "22" is wrong; banner :9, `engine/types/GameUpdateLoop.h:4`) | 0x14015e2f0 | 0x160 (newgamedialog.cpp:824, saveloaddialog.cpp:759, worldsetup_reload.cpp:73) | `fn_UpdateLoop_Ctor` newgamedialog.cpp:829, saveloaddialog.cpp:764, worldsetup_reload.cpp:76, tutorial_next_scenario.cpp:77; vptr stamped the next line in each | **yes** — slot 0 `GameUpdateLoop_Destroy` calls a file-local `??1UpdateLoop@Engine@@UEAA@XZ` door, gameupdateloop.cpp:2822 (bound :2805-2806) |
| MenuDialogLoopT "MenuDLT" (base `DialogLoopT`) | `g_menuDialogLoopVtable` | standalone/menudialogloopt_stubs.cpp:722, reg :741-743 | 13 | 0x14015ed00 | 0xC00 (game_main.cpp:692, mainmenudialog.cpp:246, tutorialui.cpp:96 caller) | inline `MenuDialogLoopT_Ctor` engine/funcs/engine_funcs.h:2601-2615 → `fn_DialogLoopT_Ctor` :2603, stamp :2606; also constructs `Audio::Scene` **by value** at +0xb30 (:2610) and `System::Random` at +0x168 | **no** — `Stub_Destroy` :237-245 frees only; skips `~DialogLoopT`, `~Audio::Scene`, `~UserInterface` (comment :239-241 calls it a leak) |
| splash GameLoadDialogLoopT (base `DialogLoaderLoopT`) | `g_splashDialogLoopVtable` | standalone/splash_loop_stubs.cpp:602, reg :621-623 | 13 | 0x140158800 | 0xB60 (game_main.cpp:713, newgamedialog.cpp:870, saveloaddialog.cpp:799, mainmenudialog.cpp:254, worldsetup_reload.cpp:107, tutorialui.cpp:102, tutorial_next_scenario.cpp:106) | `fn_DialogLoaderLoopT_Ctor` game_main.cpp:723, newgamedialog.cpp:877, saveloaddialog.cpp:811, mainmenudialog.cpp:259, worldsetup_reload.cpp:116, tutorialui.cpp:110, tutorial_next_scenario.cpp:115; stamp next line; `System::Random` at +0x194 | **no** — `Stub_Destroy` :130-138 frees only (comment :132-134) |
| LoaderLoop "inner" (base `LoaderLoop`) | `g_innerLoaderLoopVtable` | standalone/loader_loop_stubs.cpp:223, reg :350 | 12 | 0x140158870 | 0x48 (game_main.cpp:809) | `fn_LoaderLoop_Ctor` game_main.cpp:815, stamp :817-819 (by address) | **no** — `Inner_Destroy` :125-132 (comment :127-128) |
| LoaderLoop "outer" / GameLoaderLoop (base `LoaderLoop`) | `g_outerLoaderLoopVtable` | standalone/loader_loop_stubs.cpp:331, reg :351 | 12 | 0x1401588d0 | 0x48 (game_main.cpp:809, newgamedialog.cpp:889, saveloaddialog.cpp:829, mainmenudialog.cpp:272, worldsetup_reload.cpp:130, tutorialui.cpp:129, tutorial_next_scenario.cpp:128) | `fn_LoaderLoop_Ctor` game_main.cpp:815, newgamedialog.cpp:894, saveloaddialog.cpp:834, mainmenudialog.cpp:276, worldsetup_reload.cpp:134, tutorialui.cpp:134, tutorial_next_scenario.cpp:132 | **no** — `Outer_Destroy` :244-249 |

No other loop-shaped table exists: `RegisterExeAddr` calls in `standalone/` and `code/` for
`0x14015…`/`0x140158…` addresses that a loop ctor door stamps are exactly these five
(`grep -rn "0x140158870\|0x1401588d0\|0x140158800\|0x14015ed00\|0x14015e2f0"`, list in §5).
`Engine::Loader` has no exe subclass (the exe does not import `Engine::Loader` at all —
splash_loop_stubs.cpp:189-191). `Engine::MainLoop` and `Engine::Engine` are not polymorphic
(runtime/types/engine/MainLoop.h:43-51, Engine.h:48-57); the exe constructs a bare `Engine` **by
value** on the stack (`EngineStorage engine;` game_main.cpp:442, ctor :446, dtor :905) and a heap
`MainLoop` (0xA0, game_main.cpp:584, ctor :588, dtor :888).

#### Per-slot: retail vs standalone (loops)

| slot | GameUpdateLoop retail / standalone | MenuDLT retail / standalone | splash retail / standalone | inner LL retail / standalone | outer LL retail / standalone |
|---|---|---|---|---|---|
| 0 dtor | own / own (calls base dtor) | own / own (skips base) | own / own (skips base) | own / own (skips base) | own / own (skips base) |
| 1 Startup | own / own | own 0x14009b930 / own | own 0x140098fa0 / own | **base** / stub (re-does `LoaderLoop::Startup` inline: `fn_Loader_Load` :144) | **base** / stub (:260) |
| 2 Shutdown | own / own | own 0x14009b280 / own (truncated, §3C) | own `RET` / own | **base** / stub (re-does the base `WaitForSingleObject` :114-119) | **base** / stub |
| 3 Update | own / own | own 0x14009b6e0 / own | own 0x140098da0 / own (does not chain base, §3C) | **base** / stub | **base** / stub |
| 4 Draw | own / own | own 0x14009b440 / own | **base** / stub → `DialogLoaderLoopT_Draw_Impl` (code/Game/Game/dialogloaderloop.cpp:25, a port of the base body) | **base** / stub (print) | **base** / stub |
| 5 DrawFailed | own `RET` / own no-op | **base** / stub (print) | **base** / stub | **base** / stub | **base** / stub |
| 6 Load | own / own | own 0x14009a930 / own | **base** / stub that CALLS the base door (:549) | own 0x140001120 / own (skips PlacementTool) | **base** / stub |
| 7 SaveRestore | own / own | **base** / stub | **base** / stub | **base** / stub | **base** / stub |
| 8 OnVideoError | own / no-op | own / stub | **base** / stub | **base** / stub | **base** / stub |
| 9 OnVideoRestart | own / no-op | own / stub | **base** / stub | **base** / stub | **base** / stub |
| 10 GetResourceFilename | own "Game/Game" / own | own "Game/Menu" / own | **base** / stub returning "Game/Loader" (:570-575) | own "Game/Global" / own | **base** / stub returning null (:317-322) |
| 11 AllowsModChanges | **base** / stub `false` (:141-144) | own `true` / own | **base** / stub `false` | **base** / stub `false` | **base** / stub `false` |
| 12 CreateDialog | — | own 0x14009ba30 / own | own 0x140099070 / own | — | — |

No loop table holds a raw Runtime pointer today: every "base" slot is a standalone stub that
reimplements the base body. After conversion those slots should become INHERITED (the stub
deleted) — 35 stub slots across the five tables (GUL 1, MenuDLT 2, splash 8, inner 8, outer 11
minus the dtor = 30, plus the five skipped base dtors; count by the table above). That is a
behaviour decision for block 1, not a qualification.

### 1B. `Achievement::Goal` / `GoalInstance` subclasses — 7 classes, 7 tables

Retail table addresses and sizes from `engine/types/AchievementGoals.h`; tables from the files.

| Class | Table var | File:line | Slots | Retail vtable | Alloc | Base ctor | Dtor calls base? |
|---|---|---|---|---|---|---|---|
| BuildingGoal : Goal | `g_BuildingGoal_Vtable` | code/Game/Goal/buildinggoal.cpp:306, reg :320-323 | 11 | 0x140160300 (AchievementGoals.h:18) | 0x48 :327 | `fn_Goal_BaseCtor` :171, inside **slot 6** `BuildingGoal_Slot6` :162, which the factory :329/:334 also calls | yes `fn_Goal_BaseDtor` :71 |
| PopulationGoal : Goal | `g_PopulationGoal_Vtable` | code/Game/Goal/populationgoal.cpp:175, reg :189-192 | 11 | 0x140160370 (:35) | 0x38 :196 | `fn_Goal_BaseCtor` :101 in slot 6 :92 | yes `fn_Goal_BaseDtor` :47 |
| StatGoal : Goal | `g_StatGoal_Vtable` | code/Game/Goal/statgoal.cpp:413, reg :431-434 | 11 | 0x1401604c0 (:60) | sizeof(StatGoal)=0x80 statgoal.cpp:438 | file-local `s_fn_GoalCtor` :469 (bound :464, `??0Goal@Achievement@@QEAA@XZ`) in `StatGoal_Ctor_Impl` :458 | yes file-local `s_fn_GoalDtor` :360 (bound :355) |
| TimeGoal : Goal | `g_TimeGoal_Vtable` | code/Game/Goal/timegoal.cpp:210, reg :225-229 | **12** | 0x140160540 (:107) | 0x38 :233 | `s_fn_GoalCtor` :262 (bound :257) | yes `s_fn_GoalDtor` :170 (bound :165) |
| FinishTutorialGoal : Goal | `g_FinishTutorialGoal_Vtable` | code/Game/Tutorial/finishtutorialgoal.cpp:155, reg :170-172 | **12** | 0x140160420 (:125) | 0x38 :176 | `fn_Goal_BaseCtor` :105 in slot 6 :96 | yes `fn_Goal_BaseDtor` :48 |
| TimeGoalInstance : GoalInstance | `g_TimeGoalInstance_Vtable` | code/Game/Goal/timegoalinstance.cpp:269, reg :279-281 | 7 | 0x140160598 (:156) | 0x28 :285 | `s_fn_GoalInstanceCtor` :228 (bound :223) | yes — slot 0 is `TimeGoal_Slot11` (timegoal.cpp:104), which calls `s_fn_GoalInstanceDtor` :134 (bound :129) |
| FinishTutorialInstance : GoalInstance | `g_FinishTutorialInstance_Vtable` | code/Game/Tutorial/finishtutorialinstance.cpp:342, reg :352-354 | 7 | 0x140160478 (:141) | 0x28 :358 | `s_fn_GoalInstanceCtor` :306 (bound :301) | yes — slot 0 is `FinishTutorialGoal_Slot11` (finishtutorialgoal.cpp:198), `fn_GoalInstance_BaseDtor` :212 |

Per slot (Goal tables): 0 own dtor; **1 null in all five** (`TypeName` — nothing fills it:
`Goal_FillInheritedVtableSlots` fills only 4/7/9, goal_exe_helpers.cpp:226-228); 2 own `Type`;
3 own `GetFileVersion` (the "returns 1/2" trampolines); 4 base `External::Compile` installed BY
NAME (goal_exe_helpers.cpp:218-219, :226); 5 own Serialize (chains base, §3); 6 own (retail
pure: construct-in-place); 7 `Goal_Slot7_CallOwnDtor` (goal_exe_helpers.cpp:203-208, exe
0x140001640 — retail pure on `Goal`); 8 own (retail pure); 9 `GetInstanceType` (§1D); 10 own
CreateInstance; TimeGoal/FinishTutorialGoal add 11 (shared instance dtor).
**`FinishTutorialGoal` never calls `Goal_FillInheritedVtableSlots`** (finishtutorialgoal.cpp:170-172
registers the raw array), so its slots **1, 4 and 7 stay null** — a Compile or slot-7 dispatch on
it would jump to 0. Instance tables: 0 shared dtor, **1 null**, 2 own `Type`, 3 own Persist
(chains base), 4 own Startup, 5 own Update (TimeGoalInstance chains base), 6 own CheckGoalState.

### 1C. `Achievement::Achievement` and `AchievementInstance` subclasses — 2 classes, 2 tables (NOT in the inventory)

| Class | Table var | File:line | Slots | Retail vtable | Alloc | Base ctor | Dtor calls base? |
|---|---|---|---|---|---|---|---|
| **Scenario : Achievement::Achievement** (0x90; banner scenario.cpp:1-11) | `g_Scenario_Vtable` | code/Game/Game/scenario.cpp:277, reg :293-295 | **13** | 0x14015f020 (:6) | 0x90 :299 | file-local `s_fn_Achievement_Ctor` **by address** `ResolveAddr(Module::Runtime, 0x18009e240)` :74-75, called :219 inside slot 8 `Scenario_Init` | yes `s_fn_Achievement_Dtor` (0x18009e180, :76-77) at :112 |
| exe AchievementInstance (retail 0x14015e350, 4 slots: own dtor 0x140019d90 + 3 base thunks) | **none** — standalone keeps the Runtime table | code/Game/Game/gameupdateloop_load.cpp:1626-1635 | — | 0x14015e350, **not registered** (`grep 0x14015e350` finds only :1633) | 0x28 :1624 | file-local `??0AchievementInstance` door :1626 (bound :88) | n/a — the object keeps `??_7AchievementInstance@Achievement@@6B@`, so the Runtime dtor runs (comment :1627-1632) |

Scenario per slot (base = Achievement's 9): 0 own dtor; **1 null**; 2 own Type; 3 own
GetFileVersion (chains base, §3); **4 null** (retail base `External::Compile`); 5 own Serialize
(chains base); 6 own "Cleanup" (retail slot 6 = `Reconstruct`); **7 null** (retail base
`Destruct`); 8 own Init (retail slot 8 = `Construct`); **9-12 null** (exe-added slots).
The inventory's claim that `Achievement::Achievement` "has a registered deleting dtor but NO mint
vtable" is wrong: `??_7Achievement@0@6B@` is minted (rt_data_mint_tables.inc:1545, :3889) — the
inventory's regex `@Achievement@@6B@` cannot match the back-referenced spelling `@0@`.

### 1D. `Goal::GetInstanceType` (slot 9, returns `Object::Type` by value)

| Table | Slot 9 holds | Signature |
|---|---|---|
| BuildingGoal, PopulationGoal, StatGoal | the Runtime export `?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ`, installed raw by name (goal_exe_helpers.cpp:222-223, :228) | retail's own (MSVC: `this` RCX, retslot RDX) |
| TimeGoal | own `TimeGoal_Slot9_Standalone` timegoal.cpp:199 | `void* (void* self, uint32_t* retval)` — writes the id into `*retval`, returns `retval` |
| FinishTutorialGoal | own `FinishTutorialGoal_Slot9_Standalone` finishtutorialgoal.cpp:140 | same shape |

Both overrides already use the MSVC by-value-return shape (`this`, then the hidden return
pointer, return the pointer). That is exactly what a member declared
`virtual Object::Type* GetInstanceType(Object::Type* ret) const` compiles to under GCC ms_abi
(B5-12 / B6-2), so neither override needs a signature change; the three tables that install the
export raw become inherited slots. No tree code CALLS slot 9.

### 1E. Other exe structs holding one of these namespaces' objects

| Holder | What | How | File:line |
|---|---|---|---|
| MenuDLT (+0xb30) | `Audio::Scene` **by value** | `fn_AudioScene_Ctor(self+0xb30)` | engine/funcs/engine_funcs.h:2610; never destroyed (menudialogloopt_stubs.cpp:237-245) |
| exe `Game` object (+0x08) | `Picking::Scene` **by value** | `s_fn_Picking_Scene_Ctor(p + 8)` in `Reimpl_FUN_14009ee70` | code/Game/Game/gameupdateloop_load.cpp:639-641; dtor via `s_fn_SceneDtor` keystone_a_gamedtor.cpp:452 |
| GameUpdateLoop (+0x70, +0x78, +0x140) | `Picking::Scene`, `Audio::Scene`, `Save::SaveLoad` by pointer (heap 0x60 / 0x38 / 0x90) | ctor doors | gameupdateloop_load.cpp:1111-1122, :1172-1176; dtors gameupdateloop.cpp:2610, :2617 |
| MenuDLT (+0xb70) | `Save::SaveLoad` by pointer (heap 0x90) | file-local `??0SaveLoad` | menudialogloopt_stubs.cpp:258-267 |
| GameUpdateLoop (+0x150) | `AchievementInstance` via `InternalBasePtr` | §1C | gameupdateloop_load.cpp:1622-1639 |
| ModDialog reload paths | `Save::SavedState` heap 0x10 | `fn_SavedState_Ctor` | moddialog.cpp:721-723, mod_dialog_vtables.cpp:430-431 |
| stack `EngineStorage` | `Engine::Engine` by value | §1A note | game_main.cpp:442, :446, :905 |
| `Audio::Device` fields +0xa88/+0xa90 | `IXAudio2*` / mastering voice — read and COM-released **from exe code** | raw offsets | game_main.cpp:288-300 (proved by runtime/src/audio/audio_platform_startup_win.cpp:42-43) |

`PickingComponent` (`engine/types/PickingComponent.h:22-33`, `code/Game/Component/
pickingcomponent.cpp`) is an exe **`Component`** subclass (header :6 "Parent: Component
(direct)"), not a Picking subclass: it holds only an `int32_t pickingId` (+0x28) into
`Picking::Scene`, reached by door (`s_fn_AddMesh` pickingcomponent.cpp:147/:215, `s_fn_RemoveMesh`
:312/:81). No exe struct subclasses or embeds an `Input::`, `Save::SaveInfo`/`ModInfo` or
`Pathfinding::` object: `saveloaddialog.cpp` holds only `void*` returned by
`fn_SaveLoad_GetSaveInfo` (:191, :542, :575, :972, :1025, :1049, :1074, :1130).
`engine/types/TutorialGoals.h` (the `TutorialGoal*` family) derives from `Object::External`
(:6), not from `Achievement::Goal` — out of this step.

---------------------------------------------------------------------------------------------------
## 2. Summary (the B36_RULES 3.1 shape)

| Runtime base | exe subclasses | hand-built tables | how the base is constructed | base dtor run by slot 0? |
|---|---|---|---|---|
| `Engine::UpdateLoop` | 1 (GameUpdateLoop) | 1 (12 slots) | `fn_UpdateLoop_Ctor` door, vptr overwritten (4 sites) | yes (file-local door) |
| `Engine::DialogLoopT` | 1 (MenuDLT) | 1 (13) | `fn_DialogLoopT_Ctor` inside the inline `MenuDialogLoopT_Ctor` | **no** |
| `Engine::DialogLoaderLoopT` | 1 (splash) | 1 (13) | `fn_DialogLoaderLoopT_Ctor` (7 sites) | **no** |
| `Engine::LoaderLoop` | 2 (inner, outer) | 2 (12 each) | `fn_LoaderLoop_Ctor` (7 sites) | **no** |
| `Achievement::Goal` | 5 | 5 (11/11/11/12/12) | `fn_Goal_BaseCtor` or file-local `??0Goal` | yes |
| `Achievement::GoalInstance` | 2 | 2 (7) | file-local `??0GoalInstance` | yes |
| `Achievement::Achievement` | 1 (Scenario) | 1 (13) | Runtime ctor by ADDRESS 0x18009e240 | yes |
| `Achievement::AchievementInstance` | 1 in retail | **0** (0x14015e350 unregistered; Runtime table kept) | file-local `??0AchievementInstance` | Runtime's |
| `Audio::*`, `Picking::*`, `Input::*`, `Save::*`, `Pathfinding::*` | 0 | 0 | by-value holders only (§1E) | — |

Twelve exe classes (eleven with a standalone table), fourteen tables counting both LoaderLoops.

---------------------------------------------------------------------------------------------------
## 3. Slot bodies that call a base member through a door

### 3A. QUALIFIED-call sites (base member is VIRTUAL) — 25

| # | File:line | In slot | Base member (virtual slot) | Door |
|---|---|---|---|---|
| 1 | standalone/splash_loop_stubs.cpp:173 | 1 Startup | `DialogLoaderLoopT::Load` (6) | `fn_DialogLoaderLoopT_Load` |
| 2 | standalone/splash_loop_stubs.cpp:199 | 1 Startup | `DialogLoaderLoopT::Startup` (1) | `fn_DialogLoaderLoopT_Startup` |
| 3 | standalone/splash_loop_stubs.cpp:549 | 6 Load | `DialogLoaderLoopT::Load` (6) — unqualified it recurses forever | `fn_DialogLoaderLoopT_Load` |
| 4 | standalone/menudialogloopt_stubs.cpp:318 | 1 Startup | `DialogLoopT::Startup` (1) | `fn_DialogLoopT_Startup` |
| 5 | standalone/menudialogloopt_stubs.cpp:551 | 3 Update | `DialogLoopT::Update` (3) | `fn_DialogLoopT_Update` |
| 6 | standalone/menudialogloopt_stubs.cpp:669 | 6 Load | `DialogLoopT::Load` (6) | `fn_DialogLoopT_Load` |
| 7 | code/Game/Game/gameupdateloop.cpp:2822 | 0 dtor | `~UpdateLoop` | file-local `fn_UpdateLoop_Dtor` (:2795, bound :2805) |
| 8-12 | buildinggoal.cpp:71, populationgoal.cpp:47, statgoal.cpp:360, timegoal.cpp:170, finishtutorialgoal.cpp:48 | 0 dtor | `~Goal` | `fn_Goal_BaseDtor` ×3, file-local `s_fn_GoalDtor` ×2 |
| 13-14 | timegoal.cpp:134 (in `TimeGoal_Slot11` = TimeGoalInstance slot 0), finishtutorialgoal.cpp:212 (in `FinishTutorialGoal_Slot11` = FinishTutorialInstance slot 0) | 0 dtor | `~GoalInstance` | `s_fn_GoalInstanceDtor`, `fn_GoalInstance_BaseDtor` |
| 15-19 | buildinggoal.cpp:99, populationgoal.cpp:70, statgoal.cpp:88, timegoal.cpp:52, finishtutorialgoal.cpp:72 | 5 Serialize | `Goal::Serialize` (5) | `fn_Goal_BaseSerialize` ×3 (exe slot 0x140152840, engine_funcs.cpp:2826-2830), file-local `s_fn_GoalSerialize` ×2 (same slot, statgoal.cpp:66-67, timegoal.cpp:34-35) |
| 20-21 | timegoalinstance.cpp:100, finishtutorialinstance.cpp:51 | 3 Persist | `GoalInstance::Persist` (3) | `s_fn_GoalInstancePersist` (slot 0x140152868 :58-59), `fn_GoalInstance_Persist` |
| 22 | timegoalinstance.cpp:161 | 5 Update | `GoalInstance::Update` (5) | `s_fn_GoalInstanceUpdate` (slot 0x140152880 :67-68) |
| 23 | code/Game/Game/scenario.cpp:112 | 0 dtor | `~Achievement` | `s_fn_Achievement_Dtor` (by address 0x18009e180) |
| 24 | scenario.cpp:134 | 3 GetFileVersion | `Achievement::GetFileVersion` (3) | `s_fn_Achievement_GetFileVersion` (0x18009e170) |
| 25 | scenario.cpp:153 | 5 Serialize | `Achievement::Serialize` (5) | `s_fn_Achievement_Serialize` (0x18009e070) |

By class: loops 7 (6 virtual + 1 dtor), Goal family 15 (7 dtor + 8 virtual), Scenario 3.
All eight Goal-family and Scenario virtual doors are exe dispatch slots or by-address Runtime
reads, not `ResolveExport` names — a name-anchored sweep finds 5 of the 25.

### 3B. Retail base calls the standalone stubs DROP (owed; each becomes a QUALIFIED call if restored)

| File:line | Slot | What retail does that the stub does not |
|---|---|---|
| standalone/menudialogloopt_stubs.cpp:378-388 | 2 Shutdown | **carry-over 11.** Retail 0x14009b280 tears down menu scene / renderer / SaveLoad, then calls `DialogLoopT::Shutdown` (virtual slot 2), then `Audio::Scene::StopAll(self+0xb30)`. The stub runs only `MenuDialogLoopT_StopMusic` (:387 → code/Game/Game/menudialogloopt.cpp:1237-1244). |
| menudialogloopt_stubs.cpp:237-245 | 0 dtor | `~DialogLoopT`, `~Audio::Scene`, `~UserInterface` skipped |
| splash_loop_stubs.cpp:130-138 | 0 dtor | `~DialogLoaderLoopT` skipped |
| splash_loop_stubs.cpp:254-496 | 3 Update | retail 0x140098da0 chains `DialogLoaderLoopT::Update`; the stub re-does it inline (`UI::UserInterface::Update` :295) |
| loader_loop_stubs.cpp:125-132, :244-249 | 0 dtor | `~LoaderLoop` skipped (comment :127-128) |

### 3C. Non-virtual base / sibling members reached from slot bodies (no qualification)

`Engine::MainLoop::SetNextLoop` (`fn_MainLoop_SetNextLoop`): gameupdateloop.cpp:1630,
loader_loop_stubs.cpp:169, :282, menudialogloopt_stubs.cpp:525, splash_loop_stubs.cpp:482.
`Engine::Loader::Load` (`fn_Loader_Load`): loader_loop_stubs.cpp:144, :260.
`System::Thread::WaitForFinish` on `LoaderLoop::_loader`: loader_loop_stubs.cpp:93, :118.
`Engine::UpdateLoop::IsRunning` (`fn_IsRunning`, exe slot 0x140152850): populationgoal.cpp:137
(slot 8). `Achievement::GoalList::AreGoalsMetExcept` (slot 0x140152878): timegoalinstance.cpp:152.
`Save::SaveLoad` ctor/Update: menudialogloopt_stubs.cpp:264, :426. Base **ctor** doors called
from inside a slot body (placement re-construct; stays a ctor door, B36_RULES 7):
buildinggoal.cpp:171, populationgoal.cpp:101, finishtutorialgoal.cpp:105 (slot 6),
scenario.cpp:219 (slot 8); statgoal.cpp:173 / timegoal.cpp:84 reach it through `*_Ctor_Impl`.

### 3D. Hand-inlined `(*(void***)p)[N]` in these bodies — 4, all proved `self`, all slot 0

statgoal.cpp:165-170 (slot 6), timegoal.cpp:76-81 (slot 6), goal_exe_helpers.cpp:205-207
(slot 7), scenario.cpp:195-197 (slot 6, `Scenario_Cleanup`): each calls the object's OWN slot 0 with `flags = 0`
("destroy, do not free") and then re-constructs. These dispatch through the exe table, so they
are correct as virtual calls; they carry the step-6 `thumb_vtables.cpp` hazard only if slot 0
ever becomes a GCC-generated table (GCC's slot 0 is the complete dtor with no flags argument).

---------------------------------------------------------------------------------------------------
## 4. Exe code that COPIES Runtime slots into a table

**None copies from a minted table** (no B36_RULES 3.3 shape in these namespaces). One site
installs Runtime virtuals BY NAME into exe tables: goal_exe_helpers.cpp:210-235
(`Goal_FillInheritedVtableSlots`) writes `?Compile@External@Object@@…` into slot 4 and
`?GetInstanceType@Goal@Achievement@@…` into slot 9 of the Building/Population/Stat/Time tables
(called from buildinggoal.cpp:321, populationgoal.cpp:190, statgoal.cpp:432, timegoal.cpp:227).
It survives conversion as long as both names resolve. The loop tables inherit nothing from the
Runtime — every base slot is a standalone stub (§1A).

---------------------------------------------------------------------------------------------------
## 5. How this was measured

```
grep -rnE "0x140158870|0x1401588d0|0x140158800|0x14015ed00|0x14015e2f0|g_[A-Za-z]*Loop[A-Za-z]*_Vtable" code engine standalone
grep -rn "MenuDialogLoopT_Ctor(" code engine standalone
grep -nE "g_vt|Vtable|\[[0-9]+\] *=|RegisterExeAddr|fn_Goal_|fn_GoalInstance_|s_fn_Goal" code/Game/Goal/*.cpp code/Game/Tutorial/finishtutorial*.cpp
grep -rn "0x14015e350" code engine standalone
sed -n 1545,1558p;2201,2232p;2501,2528p;2834,2848p;3735,3750p standalone/rt_data_mint_tables.inc
python tools/dispatch_slot_map.py 0x140152840 0x140152868 0x140152878 0x140152880 0x140152850 0x140151c08
# enclosing-function map for every base-door call: scratchpad/b37/0X (inline python, §3A lines)
```
Scratchpad (not in the repo): `…/scratchpad/b37/0X/{common,initwalk,gsweep,sites,localbind,final}.py`.
