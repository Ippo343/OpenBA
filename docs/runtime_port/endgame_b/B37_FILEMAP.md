# B37 file ownership map — `Engine`/`Save`/`Achievement`/`Audio`/`Input`/`Pathfinding`/`Picking`/`Application` (ENDGAME B.3 step 7)

Block 0's FILE-MAP deliverable. Read-only: no source file was edited, nothing was built or run,
nothing was committed. Model: `B36_FILEMAP.md` (same sections, same counting conventions, same
demangling rule: class = the scope(s) between the member name and the outermost scope, namespace
= the outermost scope). It refines `STEP7_INVENTORY.md` §1, §3 and §5, which were **re-measured,
not copied** — §8 lists what the inventory got wrong.

Scope: every `.cpp`/`.h`/`.inc` under `runtime/ standalone/ engine/ code/ dx11/ tests/`
(**1,741 candidate files**; `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`,
`docs/` excluded). Comments are stripped before any match (docs, comments and strings in
comments are not callers). Scripts live in the session scratchpad
`scratchpad/b37/0F/` (not checked in): `scan.py` (the scan), `report2.py` (per-file bodies vs
callers), `defs.py` (definition file per registered body), `ef.py`+`ef2.py` (the `engine_funcs`
population by initialiser), `efuse.py` (binding references per file), `incl.py` (include
resolution), `nsrefs.py` (type-name blast radius), `printfchk.py`, `blocks.py` (block
assignment + disjointness assert), `mktables.py` (the §1 tables). Anything not backed by one of
them or by a quoted grep is marked **INFERRED**.

## 0. The one measurement rule that changes every count: MSVC back-references

`rt_export_addrs.inc` spells a class that shares its namespace's name with a **back-reference
digit**, not a repeated name: `?Get@Application@1@SAPEAV11@XZ` is
`Application::Application::Get`, `??0Application@0@QEAA@PEAX@Z` is its ctor,
`?GetApplication@Engine@1@…` is `Engine::Engine::GetApplication`, `??_7Achievement@0@6B@` is
`Achievement::Achievement`'s vftable. A literal `@Application@@` grep finds **zero** of the 27
Application rows, and the inventory's `toks[2]` split (`name.split('@')[2]`) also drops every
special-name row (`??0`/`??1`/`??_7`…, whose class sits in `toks[0]`). `scan.py`'s demangler
resolves both. Result, **VERIFIED** (`rows.py`, which runs `scan.py`'s demangler over all 4,926 rows):

| namespace | rows (demangled) | inventory (`toks[2]`) | literal `@NS@@` substring | classes (rows) |
|---|---:|---:|---:|---|
| Engine | **116** | 58 | 86 | MainLoop 32, Engine 30, UpdateLoop 16, LoaderLoop 14, DialogLoopT 10, DialogLoaderLoopT 9, Loader 5 |
| Save | **38** | 30 | 39 | SaveLoad 28, SavedState 10 |
| Achievement | **127** | 70 | 102 | Achievement 28, AchievementManager 24, GoalInstance 23, AchievementInstance 22, Goal 18, GoalList 12 |
| Audio | **144** | 113 | 148 | SoundBuffer 33, Scene 33, SoundEffect 29, Device 23, Configuration 16, Stream 10 |
| Input | **67** | 57 | 69 | Device 31, KeyMap 20, Configuration 16 |
| Pathfinding | **48** | 32 | 49 | PointList 25, **Scene 19**, IPathInfo 4 |
| Picking | **51** | 36 | 52 | PickingMesh 29, Scene 22 |
| Application | **27** | 0 | 0 | Application 27 |
| **total** | **618** | 396 | — | |

The literal-substring column over-counts by parameter types (`?Draw@PickingDebug@Debug@@…
AEBVScene@Picking@@…` is a `Debug` member that takes a `Picking::Scene`); the `toks[2]` column
under-counts. Every address join below uses the demangled set.

**Classes the inventory's class list does not have:** `Pathfinding::Scene` (19 exports, 0 bodies
anywhere, no struct anywhere) and, as exported classes, `Engine::Engine` (30) and
`Application::Application` (27). **Classes with a registered body but no struct anywhere in the
tree:** `Audio::Device` (8 bodies), `Audio::Stream` (4), `Input::Device` (16 — `Input.h` pins
only `State` and the `kKeyboardOffset`/`kMouseOffset`/`kMouseClippingOffset` constants),
`Pathfinding::IPathInfo` (0 bodies, 2-slot mint vtable).

**Counting units.** "Bodies" = distinct registered function symbols (a body's export call and its
address call are one body), attributed to the FILE THAT DEFINES the function (`defs.py`), which
differs from the registering file only for Application (§2) and one `Input::Configuration` body.
Bodies are split three ways: a step-7 class; "(internal)" = an unexported helper registered by
address from a step-7 directory; and FOREIGN = a body of a namespace outside step 7 that sits in
a step-7 file (listed by name in §3). "Caller / data sites" are everything that REFERS to a
step-7 export without defining it: `once` (`RT_EXPORT_ONCE`/`RtResolveCached`/`RT_VTABLE_ONCE`),
`dslot` (`RT_DATA_SLOT`), `rexp` (`ResolveExport`), `rtaddr` (`RtAddr<>`/`ResolveAddr(Module::Runtime, 0x18…)`),
`exeslot` (a `0x14015…`/`0x14016…` literal that `dispatch_slot_map.py` maps to a step-7 import),
`hex` (any other `0x18…` literal joined BY ADDRESS — table rows, address-constant namespaces),
`str` (a mangled step-7 string outside those calls — named constants, table rows), `deldtor`
(`RT_DELDTOR` row), `ef{…}` (references to the 90 step-7 `engine_funcs` globals, §3.4).

---

## 1. Per-file table

**Every file below carries at least one step-7 body, caller site, data row or binding reference,
or is a zero-hit file inside a block's directory** (listed so each block's set is complete).
212 files: 116 block-owned (§7) + 96 that no block owns. Six more files depend on a step-7 type
without any site of their own (`UserInterface.h`, `ui_userinterface_abi_test.cpp`,
`dialogloaderloop.cpp`/`.h`, `finishtutorialgoal_funcs.h`, `finishtutorialinstance_funcs.h`); they are in §3.6.
"Reg X/A" = raw `RegisterRuntimeExport`/`RegisterRuntimeAddr` calls onto step-7 namespaces in the
file (comments stripped; FOREIGN registrations excluded). "Bodies defined here" is by definition
file (`defs.py`).

#### B1 Engine+Application

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/engine/Engine.h` | 225 | — | — | Eng[hex11] | B1 |
| `runtime/types/engine/MainLoop.h` | 290 | — | — | Eng[hex27] | B1 |
| `runtime/types/engine/UpdateLoop.h` | 238 | — | — | Eng[hex1] | B1 |
| `runtime/types/application/Application.h` | 158 | — | — | — | B1 |
| `runtime/src/engine/closure_batch2b.cpp` | 97 | Eng 0/1 | Engine (internal) 1 | — | B1 |
| `runtime/src/engine/engine.cpp` | 681 | Eng 3/3 | Engine::MainLoop 3 | Eng[dslot1 once3 str2] | B1 |
| `runtime/src/engine/engine_lifecycle.cpp` | 1015 | Eng 4/7 Ach 1/1 | Engine::Engine 4, Engine (internal) 3, Achievement::AchievementManager 1 | Eng[dslot1 once1] Ach[dslot1 once2] App[once4] Aud[once7] Inp[once7] | B1 |
| `runtime/src/engine/engine_lifecycle_win.cpp` | 39 | — | — | — | B1 |
| `runtime/src/engine/engine_platform.cpp` | 396 | Eng 1/1 | Engine::Engine 1 | Aud[dslot1 once1] Inp[dslot1 once1] Eng[dslot1] | B1 |
| `runtime/src/engine/engine_platform_win.cpp` | 132 | — | — | — | B1 |
| `runtime/src/engine/loops.cpp` | 1654 | Eng 34/34 | Engine::UpdateLoop 9, Engine::LoaderLoop 8, Engine::DialogLoopT 7, Engine::DialogLoaderLoopT 7, Engine::Loader 2, Engine::MainLoop 1 | Eng[dslot1 rexp10 str5] App[rexp2] | B1 |
| `runtime/src/engine/mainloop_frame.cpp` | 543 | Eng 1/1 | Engine::MainLoop 1 | Inp[once1] Aud[once1] Eng[once1] | B1 |
| `runtime/src/engine/mainloop_internal.h` | 43 | — | — | — | B1 |
| `runtime/src/engine/mainloop_run.cpp` | 381 | Eng 2/2 | Engine::MainLoop 2 | Aud[rtaddr1] App[rtaddr1] Inp[once1] Eng[once1] | B1 |
| `runtime/src/engine/mainloop_video.cpp` | 842 | Eng 2/4 | Engine (internal) 2, Engine::MainLoop 2 | Sav[dslot1 once1] Eng[dslot1 rtaddr1] | B1 |
| `runtime/src/engine/rt_engine_port.cpp` | 144 | — | — | — | B1 |
| `runtime/src/engine/singleton_ctors.cpp` | 928 | App 1/0 Inp 2/0 Aud 2/0 | Application::Application 1, Audio::Configuration 1, Audio::Device 1, Input::Device 1, Input::Configuration 1 | App[dslot1] Inp[dslot2 hex2] Aud[dslot2 hex1 once1] | B1 |
| `runtime/src/engine/singleton_ctors_win.cpp` | 63 | — | — | — | B1 |
| `runtime/src/engine/updateloop_loadresources.cpp` | 302 | Eng 1/1 | Engine::UpdateLoop 1 | — | B1 |
| `runtime/src/application/application.cpp` | 62 | — | Application::Application 2 | — | B1 |
| `runtime/src/application/application_accessors.cpp` | 145 | — | Application::Application 6 | App[dslot1] | B1 |
| `runtime/src/application/application_lifecycle.cpp` | 398 | — | Application::Application 3, Application (internal) 3 | — | B1 |
| `runtime/src/application/application_thunks.cpp` | 84 | — | Application::Application 3 | — | B1 |
| `runtime/src/application/application_win.cpp` | 416 | — | Application::Application 6 | — | B1 |
| `runtime/src/application/message_callbacks.cpp` | 289 | App 2/3 | Application::Application 2, Application (internal) 1 | — | B1 |
| `runtime/src/application/platform_startup_win.cpp` | 497 | App 2/3 Ach 1/1 Eng 1/1 | Application::Application 3, Achievement::AchievementManager 1, Application (internal) 1, Engine::Engine 1 | App[dslot1] | B1 |
| `runtime/src/application/rt_application_port.cpp` | 218 | App 21/24 | — | — | B1 |
| `engine/types/UpdateLoop.h` | 66 | — | — | — | B1 |
| `engine/types/LoaderLoop.h` | 49 | — | — | — | B1 |
| `engine/types/DialogLoaderLoopT.h` | 57 | — | — | — | B1 |
| `engine/types/MainLoop.h` | 148 | — | — | Eng[hex27] | B1 |
| `engine/types/GameUpdateLoop.h` | 166 | — | — | — | B1 |
| `standalone/loader_loop_stubs.cpp` | 353 | — | — | ef{Eng22} | B1 |
| `standalone/splash_loop_stubs.cpp` | 624 | — | — | ef{Eng13} | B1 |
| `standalone/menudialogloopt_stubs.cpp` | 744 | — | — | Sav[rexp1] ef{Eng14 Sav2} | B1 |
| `standalone/gameupdateloop_stubs.cpp` | 169 | — | — | — | B1 |
| `runtime/src/video/context_ctor.cpp` | 504 | Eng 1/1 | Engine::MainLoop 1 | — | B1 |

#### B2 Save

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/save/SaveLoad.h` | 351 | — | — | — | B2 |
| `runtime/src/save/closure_batch2b.cpp` | 152 | Sav 0/2 | Save (internal) 2 | — | B2 |
| `runtime/src/save/rt_save_port.cpp` | 51 | — | — | — | B2 |
| `runtime/src/save/savedstate_setupload.cpp` | 242 | Sav 1/1 | Save::SavedState 1 | Sav[str1] | B2 |
| `runtime/src/save/saveload.cpp` | 1130 | Sav 19/11 | Save::SaveLoad 15, Save::SavedState 4, Save (internal) 3 | Sav[once5] | B2 |
| `runtime/src/save/saveload_draw.cpp` | 901 | Sav 1/1 | Save::SaveLoad 1 | — | B2 |
| `runtime/src/save/saveload_internal.h` | 169 | — | — | Sav[dslot1] | B2 |
| `runtime/src/save/saveload_loadmeta.cpp` | 460 | Sav 1/1 | Save::SaveLoad 1 | Sav[str1] | B2 |
| `runtime/src/save/saveload_meta.cpp` | 704 | Sav 1/1 | Save::SaveLoad 1 | Sav[once1] | B2 |
| `runtime/src/save/saveload_saveinfo.cpp` | 351 | Sav 0/2 | Save (internal) 2 | — | B2 |
| `runtime/src/save/saveload_update.cpp` | 2116 | Sav 6/6 | Save::SaveLoad 6 | Sav[once8] Eng[str1] | B2 |

#### B3 Achievement

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/achievement/AchievementManager.h` | 226 | — | — | Ach[hex1] | B3 |
| `runtime/types/achievement/Goal.h` | 204 | — | — | Ach[hex6] | B3 |
| `runtime/src/achievement/achievement_manager.cpp` | 1007 | Ach 21/21 | Achievement::AchievementManager 9, Achievement::AchievementInstance 6, Achievement::Achievement 6 | Ach[dslot2 rexp6] | B3 |
| `runtime/src/achievement/achievement_serialize.cpp` | 328 | Ach 2/2 | Achievement::AchievementManager 1, Achievement::Achievement 1 | Ach[once1] | B3 |
| `runtime/src/achievement/achievement_state.cpp` | 458 | Ach 5/5 | Achievement::AchievementManager 4, Achievement::Achievement 1 | Ach[dslot1 once1 rexp1] | B3 |
| `runtime/src/achievement/achievement_typeinfo_accessors.cpp` | 404 | Ach 11/11 | Achievement::GoalInstance 4, Achievement::Goal 3, Achievement::AchievementInstance 2, Achievement::Achievement 2 | Ach[dslot4] | B3 |
| `runtime/src/achievement/achievement_vtable.cpp` | 424 | Ach 4/4 | Achievement::Achievement 3, Achievement::AchievementInstance 1 | — | B3 |
| `runtime/src/achievement/batchc_goallist_dtor.cpp` | 92 | Ach 1/1 | Achievement::GoalList 1 | — | B3 |
| `runtime/src/achievement/goal.cpp` | 544 | Ach 13/13 | Achievement::GoalInstance 6, Achievement::Goal 4, Achievement::GoalList 3 | Ach[dslot1 rexp2] | B3 |
| `runtime/src/achievement/goal_list.cpp` | 353 | Ach 1/1 | Achievement::GoalList 1 | Ach[dslot1] | B3 |
| `runtime/src/achievement/goal_serialize.cpp` | 289 | Ach 2/2 | Achievement::Goal 1, Achievement::GoalInstance 1 | — | B3 |
| `runtime/src/achievement/rt_achievement_port.cpp` | 82 | — | — | — | B3 |
| `runtime/src/achievement/rt_factories_achievement.cpp` | 26 | — | — | — | B3 |
| `runtime/src/achievement/rt_factories_achievement.inc` | 127 | — | — | Ach[hex12 str10] | B3 |
| `engine/types/AchievementGoals.h` | 165 | — | — | — | B3 |
| `code/Game/Goal/buildinggoal.cpp` | 420 | — | — | ef{Ach6} | B3 |
| `code/Game/Goal/buildinggoal_funcs.h` | 62 | — | — | — | B3 |
| `code/Game/Goal/goal_exe_helpers.cpp` | 236 | — | — | Ach[rexp1] | B3 |
| `code/Game/Goal/goal_exe_helpers.h` | 25 | — | — | — | B3 |
| `code/Game/Goal/populationgoal.cpp` | 207 | — | — | ef{Ach6 Eng2} | B3 |
| `code/Game/Goal/populationgoal_funcs.h` | 58 | — | — | — | B3 |
| `code/Game/Goal/statgoal.cpp` | 496 | — | — | Ach[exeslot1 rexp2] | B3 |
| `code/Game/Goal/statgoal_funcs.h` | 62 | — | — | — | B3 |
| `code/Game/Goal/statvaluedispatch.cpp` | 1092 | — | — | — | B3 |
| `code/Game/Goal/statvaluedispatch.h` | 13 | — | — | — | B3 |
| `code/Game/Goal/timegoal.cpp` | 278 | — | — | Ach[exeslot1 rexp3] | B3 |
| `code/Game/Goal/timegoal_funcs.h` | 57 | — | — | — | B3 |
| `code/Game/Goal/timegoalinstance.cpp` | 296 | — | — | Ach[exeslot4 rexp1] | B3 |
| `code/Game/Goal/timegoalinstance_funcs.h` | 38 | — | — | — | B3 |

#### B4 Audio

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/audio/Audio.h` | 200 | — | — | — | B4 |
| `runtime/src/audio/audio.cpp` | 1219 | Aud 21/5 | Audio::Scene 18, Audio::Configuration 3 | Aud[dslot2 once2] | B4 |
| `runtime/src/audio/audio_device.cpp` | 466 | Aud 4/4 | Audio::Device 4 | — | B4 |
| `runtime/src/audio/audio_effectslot_serialize.cpp` | 226 | Aud 0/2 | Audio (internal) 2 | — | B4 |
| `runtime/src/audio/audio_platform_startup_win.cpp` | 177 | Aud 1/2 | Audio::Device 1, Audio (internal) 1 | — | B4 |
| `runtime/src/audio/audio_play_platform.cpp` | 491 | Aud 1/1 | Audio::Device 1 | Aud[rexp1] | B4 |
| `runtime/src/audio/audio_scene_update.cpp` | 741 | Aud 1/1 | Audio::Scene 1 | Aud[dslot2 once1 rexp5] | B4 |
| `runtime/src/audio/audio_stream_play_win.cpp` | 184 | Aud 1/1 | Audio::Stream 1 | — | B4 |
| `runtime/src/audio/audio_stream_win.cpp` | 438 | Aud 2/2 | Audio::Stream 2 | Aud[rtaddr1] | B4 |
| `runtime/src/audio/audio_voice_callback_win.cpp` | 283 | Aud 0/7 | Audio (internal) 7 | — | B4 |
| `runtime/src/audio/batchc_audio_leaves.cpp` | 235 | Aud 2/2 | Audio::Device 1, Audio::SoundBuffer 1 | Aud[dslot2] | B4 |
| `runtime/src/audio/rt_audio_port.cpp` | 101 | — | — | — | B4 |
| `runtime/src/audio/rt_factories_audio.cpp` | 24 | — | — | — | B4 |
| `runtime/src/audio/rt_factories_audio.inc` | 120 | — | — | Aud[hex8 str8] | B4 |
| `runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp` | 609 | Aud 16/17 | Audio::SoundBuffer 8, Audio::SoundEffect 8, Audio (internal) 1 | Aud[dslot4] | B4 |
| `runtime/src/audio/vtbatch7b_stream_run_win.cpp` | 550 | Aud 1/1 | Audio::Stream 1 | Aud[dslot1] | B4 |
| `runtime/src/audio/wave29_lane_avbody.cpp` | 400 | Aud 3/0 | Audio::Scene 3 | — | B4 |

#### B5 Input

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/input/Input.h` | 286 | — | — | — | B5 |
| `runtime/src/input/input.cpp` | 673 | Inp 17/17 | Input::KeyMap 10, Input::Device 4, Input::Configuration 2 | Inp[dslot2 rtaddr1] | B5 |
| `runtime/src/input/input_config_startup.cpp` | 214 | Inp 1/1 | Input::Configuration 2 | Inp[once1] | B5 |
| `runtime/src/input/input_device_update.cpp` | 763 | Inp 4/4 | Input::Device 4 | App[dslot1] Inp[dslot1] | B5 |
| `runtime/src/input/input_platform_shutdown_win.cpp` | 323 | Inp 1/1 | Input::Device 1 | App[dslot1 once1] | B5 |
| `runtime/src/input/input_platform_startup_win.cpp` | 468 | Inp 4/4 | Input::Device 4 | Inp[dslot2 once1] App[dslot1 once1] | B5 |
| `runtime/src/input/input_win.cpp` | 223 | Inp 2/2 | Input::Device 2 | App[dslot1] | B5 |
| `runtime/src/input/keymap_load.cpp` | 293 | Inp 1/1 | Input::KeyMap 1 | — | B5 |
| `runtime/src/input/keymap_save.cpp` | 350 | Inp 1/2 | Input::KeyMap 1, Input (internal) 1 | — | B5 |
| `runtime/src/input/rt_input_port.cpp` | 74 | — | — | — | B5 |

#### B6 Pathfinding+Picking

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `runtime/types/picking/Scene.h` | 349 | — | — | — | B6 |
| `runtime/src/pathfinding/pointlist.cpp` | 360 | Pth 9/12 | Pathfinding::PointList 9, Pathfinding (internal) 3 | Pth[dslot2] | B6 |
| `runtime/src/pathfinding/rt_factories_pathfinding.cpp` | 24 | — | — | — | B6 |
| `runtime/src/pathfinding/rt_factories_pathfinding.inc` | 62 | — | — | Pth[hex4 str4] | B6 |
| `runtime/src/picking/closure_batch2b.cpp` | 111 | Pck 0/1 | Picking (internal) 1 | — | B6 |
| `runtime/src/picking/pickingmesh.cpp` | 402 | Pck 9/11 | Picking::PickingMesh 9, Picking (internal) 2 | Pck[dslot2] | B6 |
| `runtime/src/picking/rt_factories_picking.cpp` | 24 | — | — | — | B6 |
| `runtime/src/picking/rt_factories_picking.inc` | 60 | — | — | Pck[hex4 str4] | B6 |
| `runtime/src/picking/rt_picking_port.cpp` | 29 | — | — | — | B6 |
| `runtime/src/picking/scene.cpp` | 1062 | Pck 9/9 | Picking::Scene 8, Picking::PickingMesh 1 | Pck[str9] | B6 |
| `runtime/src/picking/scene_grid.cpp` | 1205 | Pck 0/19 | Picking (internal) 19 | — | B6 |
| `runtime/src/picking/scene_grid.h` | 100 | — | — | — | B6 |

#### Files no block owns

| File | Lines | Reg X/A | Bodies defined here (class n) | Caller / data sites | Owner |
|---|---:|---|---|---|---|
| `code/Game/Action/keystone_a_zone.cpp` | 333 | — | — | App[rexp1] ef{App2} | — |
| `code/Game/Component/ambientemittercomponent.cpp` | 326 | — | — | ef{Aud6} | — |
| `code/Game/Component/ambientenvironmentcomponent.cpp` | 442 | — | — | ef{Aud14} | — |
| `code/Game/Component/citizen_assignworkplace.cpp` | 439 | — | — | Aud[exeslot5] | — |
| `code/Game/Component/citizenai_executor.cpp` | 697 | — | — | Pck[exeslot1 rtaddr1] | — |
| `code/Game/Component/firecomponent_ignite.cpp` | 630 | — | — | ef{Aud12} | — |
| `code/Game/Component/floorcomponent.cpp` | 547 | — | — | Pck[rtaddr2] | — |
| `code/Game/Component/particlecomponent.cpp` | 1040 | — | — | ef{Eng3} | — |
| `code/Game/Component/pickingcomponent.cpp` | 399 | — | — | Pck[exeslot1 rtaddr1 rexp1] | — |
| `code/Game/Component/quarrycomponent_deform.cpp` | 414 | — | — | Pck[exeslot2] | — |
| `code/Game/Component/terraformcomponent.cpp` | 618 | — | — | Pck[exeslot2] | — |
| `code/Game/Component/tornadocomponent.cpp` | 1123 | — | — | ef{Aud4} | — |
| `code/Game/Dialog/achievementdialog.cpp` | 358 | — | — | ef{Ach8} | — |
| `code/Game/Dialog/autosavedialog.cpp` | 288 | — | — | ef{Sav4} | — |
| `code/Game/Dialog/debugui.cpp` | 1759 | — | — | ef{Aud2 Eng2} | — |
| `code/Game/Dialog/dialog_exe_helpers.cpp` | 228 | — | — | Ach[rexp1] ef{Sav13 Ach5} | — |
| `code/Game/Dialog/entityui.cpp` | 933 | — | — | ef{Inp4} | — |
| `code/Game/Dialog/eventnotifydialog.cpp` | 1044 | — | — | ef{Aud1} | — |
| `code/Game/Dialog/followdialog.cpp` | 470 | — | — | ef{Inp3} | — |
| `code/Game/Dialog/goaldialog.cpp` | 282 | — | — | ef{Ach7} | — |
| `code/Game/Dialog/hotkeytooltipui.cpp` | 147 | — | — | ef{Inp7} | — |
| `code/Game/Dialog/mainmenudialog.cpp` | 1314 | — | — | ef{Sav2 Eng2 Aud6 App13} | — |
| `code/Game/Dialog/mod_dialog_vtables.cpp` | 516 | — | — | ef{Sav2 Eng2} | — |
| `code/Game/Dialog/moddialog.cpp` | 840 | — | — | ef{Sav2 Eng2} | — |
| `code/Game/Dialog/newgamedialog.cpp` | 1176 | — | — | ef{Eng3} | — |
| `code/Game/Dialog/optioninputbinddialog.cpp` | 415 | — | — | ef{Inp3} | — |
| `code/Game/Dialog/optionsdialog.cpp` | 1819 | — | — | ef{Inp24 Aud14 Eng4} | — |
| `code/Game/Dialog/saveloaddialog.cpp` | 1328 | — | — | ef{Sav33 Eng3} | — |
| `code/Game/Dialog/scenariodialog.cpp` | 639 | — | — | ef{Ach3 Sav13} | — |
| `code/Game/Dialog/spritecaptureui.cpp` | 233 | — | — | ef{Inp9} | — |
| `code/Game/Dialog/toolui.cpp` | 669 | — | — | ef{Inp8} | — |
| `code/Game/Dialog/ui_dialog_stub_factories.cpp` | 1723 | — | — | ef{Inp3} | — |
| `code/Game/Game/camera_input.cpp` | 362 | — | — | ef{Inp23} | — |
| `code/Game/Game/eventlog_achievement.cpp` | 100 | — | — | ef{Ach6} | — |
| `code/Game/Game/game_main.cpp` | 911 | — | — | Eng[rtaddr3 rexp4] Ach[str1] Inp[str2] Aud[str1] App[str1] ef{Eng10 Aud2} | — |
| `code/Game/Game/gameupdateloop.cpp` | 2833 | — | — | Ach[exeslot3 rexp5] Aud[rexp3] Sav[rexp1] Pck[rexp1] App[rexp1] Eng[rexp1] ef{Eng16 Ach9 Inp3 Sav3} | — |
| `code/Game/Game/gameupdateloop_load.cpp` | 1739 | — | — | Ach[rexp5] Pck[rexp1] Sav[rexp2] ef{Aud6 Ach6} | — |
| `code/Game/Game/gameupdateloop_serialize.cpp` | 1043 | — | — | Ach[rexp3] Aud[rexp1] Sav[rexp2] | — |
| `code/Game/Game/inputbindings.cpp` | 234 | — | — | ef{Inp16} | — |
| `code/Game/Game/keystone_a_gamedtor.cpp` | 453 | — | — | App[rexp1] Pck[rexp1] ef{App2} | — |
| `code/Game/Game/mapquery.cpp` | 253 | — | — | Pck[exeslot1 rexp1] | — |
| `code/Game/Game/menudialogloopt.cpp` | 1342 | — | — | Aud[rexp1] ef{Aud9} | — |
| `code/Game/Game/scenario.cpp` | 315 | — | — | Ach[rtaddr4] | — |
| `code/Game/Game/unreg_lane5_musicplayer.cpp` | 282 | — | — | Aud[rexp1] ef{Aud2} | — |
| `code/Game/Game/w2_takescreenshot.cpp` | 313 | — | — | ef{Eng3} | — |
| `code/Game/Game/worldsetup_reload.cpp` | 153 | — | — | ef{Eng9} | — |
| `code/Game/Player/bridgetool_calcpath.cpp` | 487 | — | — | Pck[exeslot1] | — |
| `code/Game/Player/dragtool.cpp` | 464 | — | — | ef{Inp11} | — |
| `code/Game/Player/edittool.cpp` | 1123 | — | — | Pck[exeslot2 rexp2] Inp[exeslot1] ef{Inp5} | — |
| `code/Game/Player/fencedtool.cpp` | 1066 | — | — | ef{Inp6} | — |
| `code/Game/Player/fencedtool_calcpath.cpp` | 469 | — | — | Pck[exeslot1] | — |
| `code/Game/Player/pathtool.cpp` | 523 | — | — | Pck[exeslot2] Inp[exeslot1] ef{Inp3} | — |
| `code/Game/Player/pathtool_setselected.cpp` | 652 | — | — | App[rexp1] ef{App2} | — |
| `code/Game/Player/placementtool.cpp` | 1485 | — | — | Inp[exeslot1] ef{Inp3} | — |
| `code/Game/Player/roadtool.cpp` | 638 | — | — | ef{Inp3} | — |
| `code/Game/Player/tool_activation_standalone.cpp` | 438 | — | — | ef{Inp5} | — |
| `code/Game/Player/tool_base_drag.cpp` | 273 | — | — | ef{Aud6} | — |
| `code/Game/Player/tool_base_ray.cpp` | 439 | — | — | ef{Inp6} | — |
| `code/Game/Tutorial/finishtutorialgoal.cpp` | 223 | — | — | ef{Ach8} | — |
| `code/Game/Tutorial/finishtutorialinstance.cpp` | 371 | — | — | Aud[exeslot1] Ach[rexp1] ef{Ach2} | — |
| `code/Game/Tutorial/tutorial_next_scenario.cpp` | 343 | — | — | ef{Eng6 Sav14} | — |
| `code/Game/Tutorial/tutorialimagehotkey.cpp` | 330 | — | — | ef{Inp7} | — |
| `code/Game/Tutorial/tutorialimageinputkey.cpp` | 307 | — | — | ef{Inp7} | — |
| `code/Game/Tutorial/tutorialui.cpp` | 166 | — | — | ef{Sav1 Eng2} | — |
| `dx11/src/devicedata.cpp` | 2230 | — | — | App[str4] | — |
| `engine/funcs/engine_funcs.cpp` | 3210 | — | — | Inp[exeslot4 rexp21] Aud[exeslot2 rtaddr1 rexp16] App[exeslot1 rexp3] Eng[exeslot2 rtaddr1 rexp15] Sav[exeslot5 rexp13] Ach[exeslot5 rexp15] | — |
| `engine/funcs/engine_funcs.h` | 4954 | — | — | ef{Eng1 Aud1} | — |
| `runtime/src/object/class_operator_delete.cpp` | 111 | — | — | Ach[hex3 str3] Aud[hex2 str2] Pck[hex1 str1] Pth[hex1 str1] | — |
| `runtime/src/object/deleting_dtor.cpp` | 2011 | — | — | Ach[deldtor4] Eng[deldtor4] Pth[deldtor1] Pck[deldtor1] Aud[deldtor2] | — |
| `runtime/src/object/deleting_dtor_leaves.cpp` | 210 | Ach 1/1 Aud 1/1 | Achievement::AchievementInstance 1, Audio::SoundEffect 1 | Ach[once1 str1] Aud[str1] | — |
| `runtime/src/object/external_ptr_serialize.cpp` | 796 | — | — | Ach[str3] Aud[str2] | — |
| `runtime/src/object/internal_ptr_array_serialize.cpp` | 100 | — | — | Ach[str2] | — |
| `runtime/src/ui/buttonelement.cpp` | 426 | — | — | Inp[rtaddr1] | — |
| `runtime/src/ui/checkelement.cpp` | 266 | — | — | Inp[rtaddr1] | — |
| `runtime/src/ui/combodialog.cpp` | 343 | — | — | Inp[dslot1] | — |
| `runtime/src/ui/editelement.cpp` | 615 | — | — | Inp[str2] | — |
| `runtime/src/ui/element_bodies_b5.cpp` | 1107 | — | — | Inp[str1] | — |
| `runtime/src/ui/userinterface.cpp` | 1696 | — | — | App[str1] Inp[str1] | — |
| `runtime/src/ui/userinterface_draw.cpp` | 716 | — | — | Inp[str2] | — |
| `runtime/src/ui/userinterface_frame.cpp` | 563 | — | — | Inp[str2] | — |
| `runtime/src/ui/userinterface_showdialog.cpp` | 459 | — | — | Eng[str2] | — |
| `runtime/src/ui/userinterface_standalone.cpp` | 208 | — | — | Aud[str1] | — |
| `runtime/src/video/device_startup.cpp` | 900 | — | — | App[dslot1] | — |
| `standalone/address_registry.cpp` | 2078 | — | — | Ach[exeslot1 rexp1] Sav[exeslot2 rexp2] Pck[exeslot5 rexp5] App[exeslot1 rtaddr1] Inp[exeslot1] | — |
| `standalone/autostart.cpp` | 533 | — | — | ef{Sav5} | — |
| `standalone/cutover_manifest.inc` | 130 | — | — | Ach[hex1 str1] App[hex1 str1] Aud[hex2 str2] Eng[hex3 str3] Inp[hex2 str2] | — |
| `standalone/modal_driver.cpp` | 316 | — | — | Eng[hex1] | — |
| `standalone/mouse_inject.cpp` | 892 | — | — | Inp[rexp1] | — |
| `standalone/reg_goal.cpp` | 84 | — | — | Ach[exeslot3 rexp3] | — |
| `standalone/reg_mint_promoted.cpp` | 3257 | Sav 1/1 Ach 2/2 | — | Sav[hex1 str1] Ach[hex2 str2] Eng[hex1 str1] | — |
| `standalone/reg_unreg_lane1.cpp` | 235 | — | — | Sav[exeslot1 rexp1] Eng[exeslot1 rexp1] Ach[exeslot1 rexp1] | — |
| `standalone/reg_unreg_lane3.cpp` | 275 | — | — | Eng[exeslot1 rexp1] | — |
| `standalone/rt_data_mint.cpp` | 958 | — | — | Eng[str4] | — |
| `standalone/rt_data_mint_tables.inc` | 4725 | — | — | Ach[hex36 str36] Eng[hex48 str48] Pck[hex12 str12] Pth[hex13 str13] Aud[hex27 str27] Inp[hex4 str4] App[hex1 str1] Sav[hex1 str1] | — |
| `standalone/rt_export_addrs.inc` | 4932 | — | — | Ach[hex127 str127] App[hex27 str27] Aud[hex144 str144] Eng[hex116 str116] Inp[hex67 str67] Sav[hex38 str38] Pth[hex48 str48] Pck[hex51 str51] | — |
| `standalone/saveload_harness.cpp` | 184 | — | — | Sav[rexp1] ef{Sav6} | — |


Notes on the table:
- `rt_application_port.cpp` REGISTERS 21 Application exports + 3 internals but DEFINES none;
  the bodies live in `application.cpp` (2), `application_accessors.cpp` (6),
  `application_lifecycle.cpp` (3 + the 3 `RtApp_Set*` internals), `application_thunks.cpp` (3),
  `application_win.cpp` (6) and `platform_startup_win.cpp` (1 of its 3). A block that moves a
  body must edit the defining file AND the registering file — both are B1's.
- `input.cpp` registers one `Input::Configuration` body that `input_config_startup.cpp` defines
  (both B5).
- `runtime/types/engine/{Engine,MainLoop}.h` and `engine/types/MainLoop.h` carry 11/27/27
  step-7 addresses as `EngineAddr::`/`MainLoopAddr::` constants (`hex`); `Goal.h` 6,
  `AchievementManager.h` 1, `UpdateLoop.h` 1. Header address constants are a hex-literal
  binding spelling (B36_RULES §14.1): a closing sweep must include them.
- `engine/types/MainLoop.h` and `runtime/types/engine/MainLoop.h` carry the SAME 27
  `MainLoopAddr::` constants — the duplicate-POD pair of §4.2.
- The `ui/*` `str` hits are named `const char*` constants holding an `Input::`/`Engine::`/
  `Audio::`/`Application::` mangled name (UI files calling into step 7); they are callers.

---

## 2. Per-class table (by DEFINITION file)

Bold = bodies in more than one file. FOREIGN bodies are excluded here (§3.1). The last column says
whether every defining file is inside the class's own block (§7).

| Class | Exports | Bodies | Files (bodies each) | All in own block? |
|---|---:|---:|---|---|
| **Engine::MainLoop** | 32 | 10 | engine.cpp (3), mainloop_run.cpp (2), mainloop_video.cpp (2), loops.cpp (1), mainloop_frame.cpp (1), **video/context_ctor.cpp (1)** | yes, once `context_ctor.cpp` is B1's (§3.1) |
| **Engine::UpdateLoop** | 16 | 10 | loops.cpp (9), updateloop_loadresources.cpp (1) | yes |
| Engine::LoaderLoop | 14 | 8 | loops.cpp (8) | yes |
| Engine::DialogLoopT | 10 | 7 | loops.cpp (7) | yes |
| Engine::DialogLoaderLoopT | 9 | 7 | loops.cpp (7) | yes |
| **Engine::Engine** | 30 | 6 | engine_lifecycle.cpp (4), engine_platform.cpp (1), **application/platform_startup_win.cpp (1)** | yes (both dirs are B1) |
| Engine::Loader | 5 | 2 | loops.cpp (2) | yes |
| Engine (internal) | — | 1 | mainloop_video.cpp (`RtEngine_VideoResourceDestroyRange_180034440`) | yes |
| **Application::Application** | 27 | 26 | application_win.cpp (6), application_accessors.cpp (6), application_thunks.cpp (3), application_lifecycle.cpp (3), platform_startup_win.cpp (3), message_callbacks.cpp (2), application.cpp (2), **engine/singleton_ctors.cpp (1: the ctor)** | yes |
| Application (unexported) | — | 5 | application_lifecycle.cpp (3 `RtApp_Set*`), message_callbacks.cpp (1 `RtApp_SetAllocNode`), **engine/closure_batch2b.cpp (1: `RtEngine_Application_ScalarDeletingDtor` 0x18004da00 — an Application body filed under engine/)** | yes |
| **Save::SaveLoad** | 28 | 24 | saveload.cpp (15), saveload_update.cpp (6), saveload_draw.cpp (1), saveload_loadmeta.cpp (1), saveload_meta.cpp (1) | yes |
| **Save::SavedState** | 10 | 5 | saveload.cpp (4), savedstate_setupload.cpp (1) (+ closure_batch2b.cpp's unexported `RtSave_SavedState_Delete`) | yes |
| Save (unexported) | — | 7 | saveload.cpp (3: `ArraySaveInfo_Dtor`, `SaveInfo_Dtor`, `ArrayModInfo_Dtor`), closure_batch2b.cpp (2: `RtSave_MetaData_CopyCtor`, `RtSave_SavedState_Delete`), saveload_saveinfo.cpp (2) | yes |
| Save::SaveInfo / ModInfo / MetaData | 0 | (in "unexported") | value types in `SaveLoad.h`; no export of their own | — |
| **Achievement::AchievementManager** | 24 | 16 | achievement_manager.cpp (9), achievement_state.cpp (4), achievement_serialize.cpp (1), **engine/engine_lifecycle.cpp (1: ctor)**, **application/platform_startup_win.cpp (1: `PlatformStartup`)** | **no** — 2 bodies in B1 files |
| **Achievement::Achievement** | 28 | 13 | achievement_manager.cpp (6), achievement_vtable.cpp (3), achievement_typeinfo_accessors.cpp (2), achievement_serialize.cpp (1), achievement_state.cpp (1) | yes |
| **Achievement::GoalInstance** | 23 | 11 | goal.cpp (6), achievement_typeinfo_accessors.cpp (4), goal_serialize.cpp (1) | yes |
| **Achievement::AchievementInstance** | 22 | 10 | achievement_manager.cpp (6), achievement_typeinfo_accessors.cpp (2), achievement_vtable.cpp (1), **object/deleting_dtor_leaves.cpp (1: dtor)** | **no** — dtor in the coordinator's file |
| **Achievement::Goal** | 18 | 8 | goal.cpp (4), achievement_typeinfo_accessors.cpp (3), goal_serialize.cpp (1) | yes |
| **Achievement::GoalList** | 12 | 5 | goal.cpp (3), batchc_goallist_dtor.cpp (1: the non-virtual `??1GoalList@Achievement@@QEAA@XZ`), goal_list.cpp (1) | yes |
| **Audio::Scene** | 33 | 22 | audio.cpp (18), wave29_lane_avbody.cpp (3), audio_scene_update.cpp (1) | yes |
| **Audio::SoundBuffer** | 33 | 9 | vtbatch7_soundbuffer_soundeffect.cpp (8), batchc_audio_leaves.cpp (1) | yes |
| **Audio::SoundEffect** | 29 | 9 | vtbatch7_soundbuffer_soundeffect.cpp (8), **object/deleting_dtor_leaves.cpp (1: dtor)** | **no** — dtor in the coordinator's file |
| **Audio::Device** | 23 | 8 | audio_device.cpp (4), audio_platform_startup_win.cpp (1), audio_play_platform.cpp (1), batchc_audio_leaves.cpp (1), **engine/singleton_ctors.cpp (1: ctor)** | **no** — ctor in a B1 file |
| **Audio::Configuration** | 16 | 4 | audio.cpp (3), **engine/singleton_ctors.cpp (1: ctor)** | **no** — ctor in a B1 file |
| **Audio::Stream** | 10 | 4 | audio_stream_win.cpp (2), audio_stream_play_win.cpp (1), vtbatch7b_stream_run_win.cpp (1) | yes |
| Audio (unexported) | — | 11 | audio_voice_callback_win.cpp (7: `RtAudio::VoiceCallback`), audio_effectslot_serialize.cpp (2), audio_platform_startup_win.cpp (1: `RtAudio_CreateXAudio2`), vtbatch7 (1: `SerializeBuffers`) | yes |
| Audio::Channel / EmitterInstance / EffectSlot | 0 | — | value types in `Audio.h` (`Array<Channel>`/`Array<EffectSlot>` in `Scene`) | — |
| **Input::Device** | 31 | 16 | input.cpp (4), input_device_update.cpp (4), input_platform_startup_win.cpp (4), input_win.cpp (2), input_platform_shutdown_win.cpp (1), **engine/singleton_ctors.cpp (1: ctor)** | **no** — ctor in a B1 file |
| **Input::KeyMap** | 20 | 12 | input.cpp (10), keymap_load.cpp (1), keymap_save.cpp (1) (+ keymap_save.cpp's unexported `RtInput_KeyMap_DeletingDtor`) | yes |
| **Input::Configuration** | 16 | 5 | input.cpp (2), input_config_startup.cpp (2), **engine/singleton_ctors.cpp (1: ctor)** | **no** — ctor in a B1 file |
| Input::State / Binding / BindingNode | 0 | — | value types in `Input.h` (`Binding` by value in `BindingNode`; `State[]` arrays inside the undeclared `Device`) | — |
| Pathfinding::PointList | 25 | 9 | pointlist.cpp (9) (+3 unexported `RtPathfinding_PointArray_*`) — struct defined in `pointlist.cpp`'s anonymous namespace (0x50) | yes |
| Pathfinding::IPathInfo | 4 | 0 | — (2-slot mint vtable only; no struct) | — |
| **Pathfinding::Scene** | 19 | 0 | — (no struct, no body; not in the inventory) | — |
| **Picking::PickingMesh** | 29 | 10 | pickingmesh.cpp (9), scene.cpp (1: `Vertices`) | yes |
| Picking::Scene | 22 | 8 | scene.cpp (8) (+ scene_grid.cpp's 19 and closure_batch2b.cpp's 1 unexported grid helpers, pickingmesh.cpp's 2 `Serialize*` helpers) | yes |
| Picking::SubMesh / Triangle / UV16 / Entry / Cell / Link / AddDesc | 0 | — | value types in `runtime/types/picking/Scene.h`; `Entry`/`Cell`/`Link` are manipulated only by the scene_grid.cpp helpers | — |
| Engine::VideoResourceEntry / MainLoopCallback | 0 | — | value types in `runtime/types/engine/MainLoop.h` (`Array<>` members of `MainLoop`) | — |

**7 of 30 exported classes have 8 bodies outside their own block**: four singleton ctors in
`singleton_ctors.cpp` (Audio×2, Input×2 — the fifth, Application's, is B1's own), the
AchievementManager ctor in `engine_lifecycle.cpp` and its `PlatformStartup` in
`platform_startup_win.cpp`, and two dtors in `deleting_dtor_leaves.cpp` (AchievementInstance,
SoundEffect). **Every one of the eight is a ctor, a dtor or a private platform hook** — none is an ordinary member that a class block
would convert to a real member function. Under B36_RULES §7 ("ctor/dtor doors stay") seven of
them need no move at all; only `AchievementManager::PlatformStartup` is a fragment candidate.

---

## 3. Cross-cutting files

### 3.1 Files carrying bodies of more than one block, or FOREIGN bodies

| File | Step-7 bodies (lines) | FOREIGN bodies (lines) | Recommended resolution |
|---|---|---|---|
| `runtime/src/engine/singleton_ctors.cpp` (928) | `Ctor_Application` 576 (B1), `Ctor_InputDevice` 616 + `Ctor_InputConfiguration` 651 (B5), `Ctor_AudioConfiguration` 672 + `Ctor_AudioDevice` 747 (B4); registered 917-921; `RT_DATA_SLOT` singletons for all five at 161-165 | `Ctor_MemoryAllocator` 239 / `Dtor_MemoryAllocator` 243 (System), `Ctor_Modification` 264, `Ctor_FileSystem` 381, `Ctor_Persistence` 441 (IO), `Ctor_Factory` 288, `Ctor_ExternalFactory` 312, `Ctor_InternalFactory` 352 (Object), `Ctor_VideoDevice` 495, `Ctor_VideoConfiguration` 541 (Video); registered 907-916 | **B1 owns, no split.** All five step-7 bodies are ctor doors (B36_RULES §7): B4/B5 declare the ctors and leave these doors in place. It includes `runtime/types/audio/Audio.h` and `runtime/types/input/Input.h` (§3.6), so B4/B5 must keep `RtAudio::Configuration`/`RtInput::*` spelled as they are until B1 has merged, or the coordinator patches the two includes at merge. `singleton_ctors_win.cpp` (63) holds only `RtLaneX*` allocator-platform helpers (System), no step-7 body. |
| `runtime/src/engine/engine_lifecycle.cpp` (1,015) | Engine::Engine ×4 (982-992, B1); `??0AchievementManager@Achievement@@` 999-1000 (B3, ctor) | `??0Configuration@System@@` 996-997; `??2Device@Video@@`/`??3Device@Video@@` 1002-1006 | **B1 owns.** The AchievementManager ctor is a ctor door and stays. |
| `runtime/src/application/platform_startup_win.cpp` (497) | Application ×3 (B1), `?PlatformStartup@Engine@1@` 469-471 (B1), `?PlatformStartup@AchievementManager@Achievement@@` 461-463 (B3) | `?PlatformStartup@ExternalFactory@Object@@` 465-467 | **B1 owns.** `AchievementManager::PlatformStartup` goes on the FRAGMENT list: B3 declares it (`// not ported` door-backed) and the fragment pass moves it after B1 and B3 have both merged. |
| `runtime/src/video/context_ctor.cpp` (504) | `?CreateVideoContext@MainLoop@Engine@@` 486-488 (B1) | the file's own Video bodies (step 5, closed) | **B1 owns for step 7** (no other step-7 block touches it; Video is closed). Alternative: fragment pass. |
| `runtime/src/engine/mainloop_video.cpp` (842) | MainLoop ×2 + `RtEngine_VideoResourceDestroyRange_180034440` | `RtColl_ArraySetSizeFill_18004f620` (Collection) at 830 | B1 owns; the Collection helper is a foreign by-address door, untouched. |
| `runtime/src/object/deleting_dtor_leaves.cpp` (210) | `??1AchievementInstance@Achievement@@UEAA@XZ` 187-188 (B3), `??1SoundEffect@Audio@@UEAA@XZ` 207-208 (B4); also calls `??1GoalList@Achievement@@` on `this+0x10` at 153 | — | **Coordinator's.** Dtor doors stay; not a block's file (B36 precedent). |
| `standalone/reg_mint_promoted.cpp` (3,257) | data rows only: `?_singleton@SaveLoad@Save@@` 711-714, `?_typeInfo@Achievement@1@` 1531-1534, `?_typeInfo@GoalInstance@Achievement@@` 1536-1539, `??_7UpdateLoop@Engine@@` 1820 | — | **Coordinator's** (mint machinery). |

`runtime/src/engine/loops.cpp` (1,654) is NOT cross-cutting: its 34 bodies are all Engine —
UpdateLoop 9 (`RtEngine_UpdateLoop_Ctor` 545, `_Dtor` 559, `_IsRunning` 654, `_DrawFailed`/
`_SaveRestore`/`_OnVideoError`/`_OnVideoRestart`/`_AllowsModChanges` 948-971), LoaderLoop 8
(`_Dtor` 617, `_Ctor` 668, `_Startup`/`_Shutdown`/`_OnVideoError`/`_Load`/`_Update`/`_Draw`
1107-1162), DialogLoopT 7 (`_Dtor` 628, `_Ctor` 730, `_Load` 742, `_Startup` 765, `_Update` 805,
`_Shutdown` 1256, `_Draw` 1267), DialogLoaderLoopT 7 (`_Dtor` 641, `_Ctor` 820, `_Load` 832,
`_Startup` 853, `_Shutdown` 1180, `_Update` 1194, `_Draw` 1228), Loader 2 (`_Load` 683, `_Run`
1308), MainLoop 1 (`_Draw` 1374); registered from `RtEngineLoopsRegister()` at 1482. The four
loop classes are **interleaved** through the file (ctor/dtor group 545-683, bodies 730-1374), so a
split into per-class files is a B1-internal decision, not a cross-block one. It also holds the
function-local lazy cache `static Loader* p = …ResolveExport("?_loader@LoaderLoop@Engine@@1VLoader@2@A")`
in `LoaderLoopLoader()` (1023-1027), read on the loader thread — the B6-5 shape (§4.3).

### 3.2 The three `closure_batch2b.cpp` (VERIFIED distinct: `md5sum` 7d4b6abf…, 5c1ed385…, b052d5eb…)

| File | Holds |
|---|---|
| `runtime/src/engine/closure_batch2b.cpp` (97) | ONE body, and it is **Application's**: `RtEngine_Application_ScalarDeletingDtor` (0x18004da00, unexported; `~Application` + `operator delete`), registered by address at 95; three by-address Collection/System helpers (`SetFreeSubtree` 0x18000a4f0, `SetFreeNode` 0x180009c30, `GuardedFree` 0x180006a10) with function-local `static Fn fn` caches |
| `runtime/src/save/closure_batch2b.cpp` (152) | `RtSave_MetaData_CopyCtor` (0x180032f70) and `RtSave_SavedState_Delete` (0x180032320), both unexported, registered at 149-150; its own `GuardedFree` (0x180006a10) |
| `runtime/src/picking/closure_batch2b.cpp` (111) | `RtPicking_Scene_InitGrid` (0x18007aef0, unexported), registered at 109 |

### 3.3 Port / factory files (owner-retained, zero or near-zero registrations)

`rt_engine_port.cpp` (144; prints only, calls the `Rt*Register()` entry points at 96-113 OUTSIDE
the printf arguments — §6), `rt_save_port.cpp` (51), `rt_achievement_port.cpp` (82),
`rt_factories_achievement.cpp` (26) + `.inc` (127: 10 mangled rows + 12 addresses),
`rt_audio_port.cpp` (101), `rt_factories_audio.cpp` (24) + `.inc` (120: 8+8),
`rt_input_port.cpp` (74), `rt_factories_pathfinding.cpp` (24) + `.inc` (62: 4+4),
`rt_picking_port.cpp` (29), `rt_factories_picking.cpp` (24) + `.inc` (60: 4+4),
`rt_application_port.cpp` (218; the Application registrar, §1 note). Each sits in its
namespace's directory and goes with that block.

### 3.4 `engine/funcs/engine_funcs.{h,cpp}` — the population BY INITIALISER

`ef.py` walks every `NAME = …;` statement in `engine_funcs.cpp` (comments stripped) and binds the
global to the first step-7 initialiser it finds — a mangled `ResolveExport` literal (demangled
with back-references), a `0x14015…` exe slot mapped by `dispatch_slot_map.py`, or a `0x18…`
runtime address. `ef2.py` adds the globals bound inside `{ … }` blocks by exe/runtime slot alone
(the first `g_`/`fn_` assigned after each `ResolveAddr` line, read by hand at 1262-2843).
**90 distinct globals**:

| namespace | globals | by name | exe/runtime-slot only | name + exe fallback |
|---|---:|---:|---|---|
| Engine | 17 | 14 | `g_pEngineSingleton` (slot 0x140151440 = `?_singleton@MainLoop@Engine@@` — **misnamed: it is MainLoop's singleton**, 17 reads in `gameupdateloop.cpp`), `fn_IsRunning` (slot 0x140152850, `UpdateLoop::IsRunning`), `g_pLoader` (runtime 0x1801fe7b8 = `LoaderLoop::_loader`) | — |
| Save | 13 | 11 | `fn_SaveLoad_CheckMod` (0x140152738), `fn_SaveLoad_SetSaveName` (0x140151650) | `fn_SaveLoad_SetOperation`, `fn_SaveLoad_Update`, `g_pSaveLoadSingleton` |
| Achievement | 17 | 15 | `g_ppAchievementManager` (0x1401514f0), `fn_Goal_BaseSerialize` (0x140152840) | `fn_Achievement_GetTitle`/`GetDescription`/`GetSpriteName` |
| Audio | 17 | 16 | `g_pAudioConfigSingleton` (exe 0x140151b78, then runtime 0x1801fe758) | `fn_Audio_Scene_Pause` |
| Input | 22 | 19 | `g_pKeyMapObjectExref` (0x140151738), `g_pInputDeviceSingleton` (0x140151b80), `fn_KeyMap_Released` (0x1401528f0) | `fn_KeyMap_Pressed` |
| Application | 4 | 3 | `g_pAppSingleton` (0x140152368) | — |
| Pathfinding / Picking | 0 | 0 | — | — |

**`engine_funcs.h` inline bodies calling the population: two**, both in `MenuDialogLoopT_Ctor`
(`engine_funcs.h:2601-2614`): `fn_DialogLoopT_Ctor(self, dialogFilename)` at 2603 (B1) and
`fn_AudioScene_Ctor(self + 0xb30)` at 2610 (B4) — that function constructs an `Audio::Scene` BY
VALUE at `MenuDialogLoopT+0xb30` (§4.3). The closing sweep must include these (B36_RULES §14.2).

**Binding references outside `engine_funcs.cpp` (`efuse.py`)**: 548 references to the 90 globals
in 61 files — Input 159, Engine 119, Save 100, Audio 85, Achievement 66, Application 19. These are
references (calls, readiness guards, `(void*)` prints), not calls alone; the inventory's "166
calls" counted calls to 63 doors. Per-file `ef{}` counts are in §1.

### 3.5 Other caller surfaces (spellings 3/10/11; VERIFIED sites, comments stripped)

| namespace | `rtaddr` (`RtAddr<>`, `ResolveAddr(Runtime)`) | `exeslot` | `rexp` | `once` | `dslot` |
|---|---|---|---|---:|---:|
| Engine | 5: `game_main.cpp:184,186,188` (0x1800dd6b0/dd640/dd3c0), `engine_funcs.cpp:2404`, `mainloop_video.cpp:717` | 4 (engine_funcs ×2, reg_unreg_lane1, reg_unreg_lane3) | 32 in 6 files | 6 | 5 |
| Save | 0 | 8 (engine_funcs ×5, address_registry ×2, reg_unreg_lane1) | 23 in 8 | 15 | 2 |
| Achievement | **4: `scenario.cpp:75-81`** (0x18009e240/e180/e170/e070) | 19 in 8 files | 51 in 16 | 5 | 10 |
| Audio | 3: `engine_funcs.cpp:1708`, `audio_stream_win.cpp:296`, `mainloop_run.cpp:282` | 8 (**`citizen_assignworkplace.cpp:133-137` ×5**, `finishtutorialinstance.cpp`, engine_funcs ×2) | 28 in 7 | 13 | 14 |
| Input | 3: **`ui/buttonelement.cpp:284`, `ui/checkelement.cpp:148`** (`RtAddr` onto `KeyMap` 0x1800a7a80), `input.cpp:559` | 8 (edittool, pathtool, placementtool, engine_funcs ×4, address_registry) | 22 in 2 | 12 | 9 |
| Pathfinding | 0 | 0 | 0 | 0 | 2 |
| Picking | 4: `citizenai_executor.cpp:372`, `floorcomponent.cpp:249,254`, `pickingcomponent.cpp:312` | 18 in 10 files (`code/Game/{Component,Player,Game}` + address_registry ×5) | 12 in 7 | 0 | 2 |
| Application | 2: `mainloop_run.cpp:284`, `address_registry.cpp:1544` | 2 (engine_funcs, address_registry) | 9 in 6 | 6 | 8 |

### 3.6 Type-only dependents (a block's rename breaks them; they register nothing)

Resolved with the Makefile's `SA_INCLUDES` order (`incl.py`: own dir, then `. engine/types
engine/funcs code/Game runtime/types runtime/funcs dx11/types dx11/funcs standalone`, +
`tests/unit` for tests — a bare `"MainLoop.h"` from `runtime/src/engine/` resolves to
`engine/types/MainLoop.h`, not `runtime/types/engine/MainLoop.h`). `blocks.py`'s 12 cross-block
include edges:

| Includer | Owner | Includes (block) | Resolution |
|---|---|---|---|
| `runtime/src/engine/singleton_ctors.cpp` | B1 | `runtime/types/audio/Audio.h` (B4), `runtime/types/input/Input.h` (B5) | §3.1 |
| `runtime/src/input/input_platform_shutdown_win.cpp` | B5 | `runtime/types/application/Application.h` (B1) — 1 `RtApplication::` use | sequence: B1 keeps the `RtApplication::Application` spelling (POD or alias) until B5 rebases on B1's merge |
| `tests/unit/ui_userinterface_abi_test.cpp` | coordinator | `runtime/types/engine/UpdateLoop.h` (B1) — `RtEngine::DialogLoopT`/`DialogLoaderLoopT` ×6 at 98-103 | coordinator edits at B1's merge |
| `standalone/mouse_inject.cpp` | caller round | `runtime/types/input/Input.h` (B5) — `RtInput::State`, `RtInput::kMouseOffset` ×5 at 355-357, 824 | untouched if `State` stays an `RtInput::` POD (it is embedded in `Device`, §4.3) |
| `code/Game/Game/game_main.cpp` | caller round | `engine/types/UpdateLoop.h` (B1) | B1 keeps the mirror header compiling |
| `code/Game/Game/gameupdateloop.cpp`, `gameupdateloop_load.cpp` | caller round | `engine/types/GameUpdateLoop.h` (B1) | same |
| `code/Game/Game/dialogloaderloop.cpp` / `.h` | caller round | `engine/types/MainLoop.h`, `engine/types/DialogLoaderLoopT.h` (B1) | same |
| `code/Game/Tutorial/finishtutorialgoal_funcs.h`, `finishtutorialinstance_funcs.h` | caller round | `engine/types/AchievementGoals.h` (B3) | B3 keeps the header compiling |

Plus two non-include type dependents found by `nsrefs.py`: `runtime/types/ui/UserInterface.h`
(UI, step 6 closed) declares `friend struct RtEngine::DialogLoopT;` / `friend struct
RtEngine::DialogLoaderLoopT;` at 387-388 (B1) and the ONLY existing declaration of a real step-7
namespace in the tree, `namespace Audio { class Scene; class SoundEffect; }` at 272, used by
`SetAudioScene(Audio::Scene*)` 355 and `SetSound(int, ExternalPtr<Audio::SoundEffect>)` 362 (B4).
**Coordinator's**: one-line edits at B1's and B4's merges (the class-key B4 chooses must match
`class` there, or the forward declaration changes).

---

## 4. Type headers

### 4.1 `runtime/types/<ns>/*.h` (includers resolved by `incl.py`; no header includes another block's header)

| Header | Lines | `static_assert` | Namespaces | Structs | Direct includers (transitive-only) |
|---|---:|---:|---|---|---|
| `engine/Engine.h` | 225 | 20 | `RtEngine`, `EngineAddr` | `Engine` | engine.cpp, engine_lifecycle.cpp, engine_platform.cpp |
| `engine/MainLoop.h` | 290 | 24 | `RtEngine`, `MainLoopAddr` | `VideoResourceEntry`, `MainLoopCallback`, `MainLoop` | engine.cpp, loops.cpp, mainloop_frame.cpp, mainloop_run.cpp, mainloop_video.cpp (all via `"engine/MainLoop.h"`; the grep-by-basename count of 7 in the inventory included `splash_loop_stubs.cpp` and `dialogloaderloop.cpp`, which resolve to `engine/types/MainLoop.h`) |
| `engine/UpdateLoop.h` | 238 | 24 | `RtEngine` | `UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT` | loops.cpp, updateloop_loadresources.cpp, **tests/unit/ui_userinterface_abi_test.cpp** |
| `save/SaveLoad.h` | 351 | 41 | `RtSave` | `ModInfo`, `SaveInfo`, `SavedState`, `SaveLoad` | saveload.cpp, saveload_loadmeta.cpp, saveload_saveinfo.cpp, saveload_internal.h (→ rt_save_port.cpp, savedstate_setupload.cpp, saveload_draw.cpp, saveload_meta.cpp, saveload_update.cpp) |
| `achievement/AchievementManager.h` | 226 | 19 | `RtAchievement`, `goalflags` | `GoalListView`, `GoalInstanceView`, `Achievement`, `AchievementInstance`, `AchievementManager` | achievement_manager.cpp, achievement_serialize.cpp, achievement_state.cpp, achievement_vtable.cpp |
| `achievement/Goal.h` | 204 | 10 | `RtAchievement`, `GoalVtbl`, `GoalInstanceVtbl`, `GoalStateBits` | `Goal`, `GoalInstance`, `GoalList` | achievement_typeinfo_accessors.cpp, batchc_goallist_dtor.cpp, goal.cpp, goal_list.cpp, goal_serialize.cpp |
| `audio/Audio.h` | 200 | 31 | `RtAudio` | `Channel`, `EmitterInstance`, `EffectSlot`, `Scene`, `Configuration` | audio.cpp, audio_effectslot_serialize.cpp, audio_scene_update.cpp, wave29_lane_avbody.cpp, **engine/singleton_ctors.cpp** |
| `input/Input.h` | 286 | 26 | `RtInput` | `KeyMap`, `Binding`, `BindingNode`, `State`, `Configuration` | 8 input/*.cpp, **engine/singleton_ctors.cpp**, **standalone/mouse_inject.cpp** |
| `picking/Scene.h` | 349 | 37 | `RtPicking` | `Triangle`, `UV16`, `SubMesh`, `PickingMesh`, `Entry`, `Cell`, `Link`, `Scene`, `AddDesc` | closure_batch2b.cpp, pickingmesh.cpp, scene.cpp, scene_grid.h (→ rt_picking_port.cpp, scene_grid.cpp); includes `../../../engine/types/common.h`, `../math/Math.h`, `../volume/Volume.h`, `../collection/Collection.h` |
| `application/Application.h` | 158 | 9 | `RtApplication` | `Application` | 8 application/*.cpp, **input/input_platform_shutdown_win.cpp** |

`Pathfinding` has no header. `AchievementManager.h`'s `GoalListView`/`GoalInstanceView` are
lane-local views of `Goal.h`'s `GoalList`/`GoalInstance` — two definitions of one layout in the
same block (B3 reconciles). `SaveLoad.h` includes `../ui/Sprite.h` (`UI::Sprite` by value).

### 4.2 Duplicate POD trees (one layout, two definitions)

| Layout | Definition A | Definition B | Who uses B |
|---|---|---|---|
| `Engine::UpdateLoop` (0x28) | `RtEngine::UpdateLoop` (`runtime/types/engine/UpdateLoop.h:82`) | global `struct UpdateLoop` + `UpdateLoopVtable` (`engine/types/UpdateLoop.h:52`, 66 lines, 8 asserts) | 7 direct, 12 transitive (the four standalone loop stubs, game_main.cpp, gameupdateloop*.cpp, dialogloaderloop.*) |
| `Engine::LoaderLoop` (0x48) | `RtEngine::LoaderLoop` (`UpdateLoop.h:176`) | global `struct LoaderLoop` (`engine/types/LoaderLoop.h:16`) — base fields FLATTENED, not embedded; its `m_finished` at +0x40 is the misnaming `runtime/types/engine/UpdateLoop.h`'s banner corrects | loader_loop_stubs.cpp, DialogLoaderLoopT.h |
| `Engine::DialogLoaderLoopT` (0x188) | `RtEngine::DialogLoaderLoopT` (`UpdateLoop.h:225`, `UI::UserInterface _ui` at +0x60) | global `struct DialogLoaderLoopT` (`engine/types/DialogLoaderLoopT.h:20`, `uint8_t m_userInterface[0x128]`) | splash_loop_stubs.cpp, dialogloaderloop.h |
| `Engine::MainLoop` | `RtEngine::MainLoop` (`runtime/types/engine/MainLoop.h:223`) | global `struct MainLoop` (`engine/types/MainLoop.h:77`), same 27 `MainLoopAddr::` constants | splash_loop_stubs.cpp, dialogloaderloop.cpp |
| `Picking::PickingMesh` (0x48) | `RtPicking::PickingMesh` (`Scene.h:158`, bare `SubMesh* _nodes; int32 _nodeCount`) | anonymous-namespace `struct PickingMesh` (`pickingmesh.cpp:87`, full `Array` header incl. capacity at +0x44) | pickingmesh.cpp only |
| `Achievement::GoalList` | `RtAchievement::GoalList` (`Goal.h:175`) | `RtAchievement::GoalListView` (`AchievementManager.h:52`) | achievement_*.cpp |

`engine/types/GameUpdateLoop.h` (exe `GameUpdateLoop`, 166 lines, 26 asserts) and
`engine/types/AchievementGoals.h` (7 exe Goal subclasses, `_goalBase[0x28]` opaque) mirror EXE
classes, not runtime ones; they embed no `RtEngine`/`RtAchievement` POD. **`engine/types/
PickingComponent.h` is not a Picking mirror at all** — it is the exe `PickingComponent : Component`
(0x38) holding an `int32_t pickingId` handle; it belongs to the `code/Game/Component` caller batch,
not to B6.

### 4.3 PODs embedded BY VALUE (the B6-4 check)

| Embedded | In | Offset | Polymorphic? | Consequence |
|---|---|---|---|---|
| `RtEngine::UpdateLoop` `_base` | `LoaderLoop`, `DialogLoopT` | +0x00 | yes (12-slot vftable, exe-derived) | inheritance spelled as a member; becomes a base class. The exe mirrors (§4.2) flatten it — they are the holders B6-4 is about, so the POD must survive as `RtEngine::UpdateLoop` for them |
| `RtEngine::LoaderLoop` `_base` | `DialogLoaderLoopT` | +0x00 | yes (12 slots; two exe `LoaderLoop` subclasses) | same |
| `RtSystem::Thread` `_thread` | `RtEngine::Loader` | +0x00 | yes, ABSTRACT (`_purecall` slot 0) | already a POD per B.3.4b; stays |
| `Engine::Loader` (static `?_loader@LoaderLoop@Engine@@1VLoader@2@A`, 0x1801fe7b8) | `LoaderLoop` (static member) | — | yes (2-slot vftable) | held by value → concrete; built by `standalone/rt_data_mint.cpp:434-461` via the Thread ctor door + vftable stamp; `g_pLoader` and `loops.cpp:1023` read it |
| `UI::UserInterface` `_ui` | `DialogLoopT` / `DialogLoaderLoopT` | +0x40 / +0x60 (0x128) | no (concrete since B.3.6; friends at `UserInterface.h:387-388`) | friend spelling changes with B1 |
| `Audio::Scene` | exe `MenuDialogLoopT` | +0xb30 | no | `engine_funcs.h:2610` constructs it in place; B4 needs a callable ctor door |
| `RtInput::State[]` (`analog[4][0x12]`, `keyboard[0x69]` at 0x6C0, `mouse[0xD]` at 0x1098) | `Input::Device` (no struct) | — | no | `State` stays `RtInput::`; `mouse_inject.cpp` reads it |
| `RtInput::Binding` `element` | `BindingNode` | +0x00 | no | — |
| `RtSave::SavedState` `_embeddedState` | `SaveLoad` | +0x28 | no | — |
| `UI::Sprite` `thumbnail` / `_saveSprite` | `SaveInfo` / `SaveLoad` | +0x00 / +0x60 | no | — |
| `GoalListView` (= `GoalList`) `goalList` | `AchievementInstance` | +0x10 | no (non-virtual `QEAA` dtor, called on `this+0x10` at `deleting_dtor_leaves.cpp:153`) | value type |
| `RtObject::External` / `RtObject::Internal` `base` | `Achievement`, `Goal` / `AchievementInstance`, `GoalInstance` | +0x00 | yes | inheritance spelled as a member |

---

## 5. Unit tests (`tests/unit`, 48 entries)

By `Ns::` / `Rt<Ns>::` (comments included, then read): 5 files match, **one** is a real code
dependency — `ui_userinterface_abi_test.cpp` includes `runtime/types/engine/UpdateLoop.h` and
asserts `offsetof(RtEngine::DialogLoopT, _ui) == 0x40`, `offsetof(RtEngine::DialogLoaderLoopT,
_ui) == 0x60`, the two `sizeof`s (0x168/0x188) and `decltype(_ui) == UI::UserInterface` (lines
98-103). The other four mention a step-7 name only in comments: `set_test.cpp:73`
(`Input::KeyMap`), `system_abi_test.cpp:16,143` (`Audio::Stream`, `RtEngine::Loader`),
`ui_spritecontext_abi_test.cpp:9` and `ui_value_types_abi_test.cpp:7` (`Save::SaveLoad`). By bare
struct name (`SaveLoad`, `KeyMap`, `PointList`, `MainLoop`, `UpdateLoop`, … 21 names): no further
hit outside comments. No test includes any other step-7 header. Tests are the coordinator's.

---

## 6. Printf-wrapped `Register*` calls

**Zero**, VERIFIED by `printfchk.py` over all 81 `.cpp/.h/.inc` files in the eight directories:
`(printf|fprintf|sprintf|snprintf|TRACE|LOG\w*|DiagPrint\w*)\s*\(` followed, before the
statement's `;`, by any `\w*(Register|PortInit|Init)\w*\s*\(` (`re.S`, so it spans lines). The
widened callee pattern matters: the entry points here are named `RtEngineLoopsRegister()`,
`RtEnginePlatformRegister()`, …, which a `\bRegister` pattern misses. The 46 plain
`printf`/`fprintf` calls that exist were listed; the nine summary prints in `rt_engine_port.cpp`
(114-142) take counters (`loops`, `core`, `plat`, …) assigned from the `Register()` calls on
the lines before (96, 112-113), not the calls themselves.

---

## 7. Proposed blocks by disjoint file set

NEXT_SESSION §2c's six blocks are **confirmed**, with four amendments: B1 also owns
`runtime/src/video/context_ctor.cpp` and the four `standalone/*_stubs.cpp` exe-loop vtable files
(not `code/Game/Game/gameupdateloop.cpp`, which is a multi-namespace caller); B3 owns
`code/Game/Goal/*` but not `code/Game/Tutorial/finishtutorial*`; `engine/types/PickingComponent.h`
is not B6's. `blocks.py` asserts disjointness:

```
DISJOINTNESS: PASS (116 files in 6 blocks, 0 duplicates)
  B1 Engine+Application: 37 files
  B2 Save: 11 files
  B3 Achievement: 29 files
  B4 Audio: 17 files
  B5 Input: 10 files
  B6 Pathfinding+Picking: 12 files
```

| Block | Classes | Owned files |
|---|---|---|
| **B1 Engine + Application** (serial; first) | `MainLoop`, `UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`, `Engine`, `Application` | `runtime/types/engine/{Engine,MainLoop,UpdateLoop}.h`, `runtime/types/application/Application.h`; `runtime/src/engine/{closure_batch2b, engine, engine_lifecycle, engine_lifecycle_win, engine_platform, engine_platform_win, loops, mainloop_frame, mainloop_run, mainloop_video, rt_engine_port, singleton_ctors, singleton_ctors_win, updateloop_loadresources}.cpp` + `mainloop_internal.h`; `runtime/src/application/{application, application_accessors, application_lifecycle, application_thunks, application_win, message_callbacks, platform_startup_win, rt_application_port}.cpp`; `engine/types/{UpdateLoop,LoaderLoop,DialogLoaderLoopT,MainLoop,GameUpdateLoop}.h`; `standalone/{loader_loop_stubs, splash_loop_stubs, menudialogloopt_stubs, gameupdateloop_stubs}.cpp`; `runtime/src/video/context_ctor.cpp` |
| **B2 Save** (parallel with B1) | `SaveLoad`, `SavedState`, `SaveInfo`, `ModInfo` | `runtime/types/save/SaveLoad.h`; `runtime/src/save/{closure_batch2b, rt_save_port, savedstate_setupload, saveload, saveload_draw, saveload_loadmeta, saveload_meta, saveload_saveinfo, saveload_update}.cpp` + `saveload_internal.h` |
| **B3 Achievement** | `AchievementManager`, `Achievement`, `AchievementInstance`, `Goal`, `GoalInstance`, `GoalList` (+ the 7 exe Goal subclasses' header) | `runtime/types/achievement/{AchievementManager,Goal}.h`; `runtime/src/achievement/{achievement_manager, achievement_serialize, achievement_state, achievement_typeinfo_accessors, achievement_vtable, batchc_goallist_dtor, goal, goal_list, goal_serialize, rt_achievement_port, rt_factories_achievement}.cpp` + `rt_factories_achievement.inc`; `engine/types/AchievementGoals.h`; `code/Game/Goal/*` (14 files) |
| **B4 Audio** (after B1) | `Scene`, `Channel`, `EmitterInstance`, `EffectSlot`, `Configuration`, `Device`, `SoundBuffer`, `SoundEffect`, `Stream` | `runtime/types/audio/Audio.h`; `runtime/src/audio/{audio, audio_device, audio_effectslot_serialize, audio_platform_startup_win, audio_play_platform, audio_scene_update, audio_stream_play_win, audio_stream_win, audio_voice_callback_win, batchc_audio_leaves, rt_audio_port, rt_factories_audio, vtbatch7_soundbuffer_soundeffect, vtbatch7b_stream_run_win, wave29_lane_avbody}.cpp` + `rt_factories_audio.inc` |
| **B5 Input** (after B1) | `KeyMap`, `Binding`, `BindingNode`, `Device`, `State`, `Configuration` | `runtime/types/input/Input.h`; `runtime/src/input/{input, input_config_startup, input_device_update, input_platform_shutdown_win, input_platform_startup_win, input_win, keymap_load, keymap_save, rt_input_port}.cpp` |
| **B6 Pathfinding + Picking** | `PointList`, `IPathInfo`, `Pathfinding::Scene` (declarations only — no body exists); `Picking::Scene`, `PickingMesh`, `SubMesh`, `Entry`, `Cell`, `Link` | `runtime/types/picking/Scene.h`; `runtime/src/pathfinding/{pointlist, rt_factories_pathfinding}.cpp` + `.inc`; `runtime/src/picking/{closure_batch2b, pickingmesh, rt_factories_picking, rt_picking_port, scene, scene_grid}.cpp` + `scene_grid.h` + `rt_factories_picking.inc` |

### 7.1 Files serving more than one block (the coordinator resolves)

`blocks.py` output, verbatim:

```
BODY STRADDLERS (registered bodies of a block other than the file's owner):
   runtime/src/application/platform_startup_win.cpp  owner=B1 Engine+Application  bodies-of=['B1 Engine+Application', 'B3 Achievement']
   runtime/src/engine/engine_lifecycle.cpp  owner=B1 Engine+Application  bodies-of=['B1 Engine+Application', 'B3 Achievement']
   runtime/src/engine/singleton_ctors.cpp  owner=B1 Engine+Application  bodies-of=['B1 Engine+Application', 'B4 Audio', 'B5 Input']
   runtime/src/object/deleting_dtor_leaves.cpp  owner=NONE  bodies-of=['B3 Achievement', 'B4 Audio']
   standalone/reg_mint_promoted.cpp  owner=NONE  bodies-of=['B2 Save', 'B3 Achievement']
```

plus the 12 include edges of §3.6. Resolutions:

| File | Blocks | Resolution |
|---|---|---|
| `singleton_ctors.cpp` | B1+B4+B5 | **owner-retained (B1)**; the four B4/B5 bodies are ctor doors and stay. B4/B5 keep `RtAudio::`/`RtInput::` POD spellings until they rebase on B1 |
| `engine_lifecycle.cpp` | B1+B3 | **owner-retained (B1)**; `AchievementManager` ctor door stays |
| `platform_startup_win.cpp` | B1+B3 | **owner-retained (B1)** + **fragment**: `AchievementManager::PlatformStartup` after B1 and B3 merge |
| `deleting_dtor_leaves.cpp` | B3+B4 | **coordinator's**; dtor doors stay |
| `reg_mint_promoted.cpp` | B2+B3 (+B1 vftable row) | **coordinator's**; data rows |
| `input_platform_shutdown_win.cpp` (include) | B5 needs B1's `Application.h` | **sequence**: B5 after B1 (already the plan) |
| `runtime/types/ui/UserInterface.h` (friends + `Audio` forward decl) | B1+B4 | **coordinator's**, at each merge |
| `tests/unit/ui_userinterface_abi_test.cpp` | B1 | **coordinator's** |
| `engine/funcs/engine_funcs.h` (`MenuDialogLoopT_Ctor`) | B1+B4 | **coordinator / closing sweep** |

No file needs a line split before the blocks start. This is a cleaner start than step 6 (three
real fragment files); here there is ONE fragment body.

### 7.2 Coordinator's / not a block's

- Registry, mint and generated tables: `standalone/{address_registry.cpp, reg_mint_promoted.cpp,
  reg_goal.cpp, reg_unreg_lane1.cpp, reg_unreg_lane3.cpp, rt_data_mint.cpp,
  rt_data_mint_tables.inc, rt_export_addrs.inc (GENERATED), cutover_manifest.inc (GENERATED)}`.
- Deleting-dtor / operator-delete / serializer tables: `runtime/src/object/{deleting_dtor.cpp
  (RT_DELDTOR rows at 288-291 Achievement ×4, 306-312 Engine ×4, 927 PointList, 928 PickingMesh,
  976-977 SoundBuffer/SoundEffect), deleting_dtor_leaves.cpp, class_operator_delete.cpp (7
  `??3` rows at 70-95), external_ptr_serialize.cpp (5 `_typeInfo` rows), internal_ptr_array_serialize.cpp (2)}`.
- `engine/funcs/engine_funcs.{h,cpp}`; `tests/unit/*`; `runtime/types/ui/UserInterface.h`.
- Caller rounds (not blocks): the 96 unowned files of §1 — `code/Game/{Game, Dialog, Component,
  Player, Tutorial, Action}` (64 files), `runtime/src/ui/*` (10), `runtime/src/video/device_startup.cpp`,
  `dx11/src/devicedata.cpp` (the four Application import rows at 281-286), `standalone/{autostart,
  modal_driver, mouse_inject, saveload_harness}.cpp`.

---

## 8. What `STEP7_INVENTORY.md` got wrong

1. **§3.5 / §2a per-namespace export counts** (Engine 58, Save 30, Achievement 70, Audio 113,
   Input 57, Pathfinding 32, Picking 36, Application 0): the `toks[2]` split drops back-referenced
   and special-name rows. Demangled: 116 / 38 / 127 / 144 / 67 / 48 / 51 / 27 (§0). Every address
   join built on the old set under-reported.
2. **"`Application` has ZERO named exports"** (§1.1, §6, NEXT_SESSION §2a item 4): it has 27, all
   spelled `@Application@1@` / `Application@0@`; `dx11/src/devicedata.cpp:281-286` binds four of them.
3. **§1.3 Achievement polymorphism**: 4 mint vftables, not 3 — `??_7Achievement@0@6B@`
   (0x180154740, **9 slots**, `rt_data_mint_tables.inc:3889`) — and 4 `_typeInfo` rows, not 3
   (`?_typeInfo@Achievement@1@…` at 4251). "`Achievement` has a registered dtor but NO mint
   vtable" is false; it is polymorphic.
4. **"`GoalList` has an `RT_DELDTOR` row"** (§1.3): it does not. The four Achievement rows
   (`deleting_dtor.cpp:288-291`) are `Achievement`, `AchievementInstance`, `Goal`, `GoalInstance`.
   `GoalList`'s dtor is the non-virtual `??1GoalList@Achievement@@QEAA@XZ` (0x18009f420),
   registered in `batchc_goallist_dtor.cpp:89-90`; it is a value type held at
   `AchievementInstance+0x10`.
5. **Missing class `Pathfinding::Scene`** (19 exports, no struct, no body). The class list names
   only `PointList`/`IPathInfo` for Pathfinding.
6. **§3.5 by-address sites**: Engine's `rt_data_mint.cpp` "hit" on 0x1801fe7b8 is a COMMENT (line
   592); `game_main.cpp` has 3 sites, not 1. Missed: Achievement 4 (`scenario.cpp:75-81`), Input 2
   in UI (`buttonelement.cpp:284`, `checkelement.cpp:148`) + `input.cpp:559`, Audio 2
   (`audio_stream_win.cpp:296`, `mainloop_run.cpp:282`), Application 2 (`mainloop_run.cpp:284`,
   `address_registry.cpp:1544`), Engine `mainloop_video.cpp:717`.
7. **§3.4 exe-slot sites**: Audio misses `citizen_assignworkplace.cpp:133-137` (five `Audio::Scene`
   slots 0x140152a00-a20 held in file-local `s_slot_*` globals).
8. **§3.1 `engine_funcs` doors**: 90 globals by initialiser, not 63 (Engine 17/10, Save 13/10,
   Achievement 17/9, Audio 17/15, Input 22/19, Application 4/0). Exe-slot-only bindings and
   `@1@`-spelled names are the difference. `g_pEngineSingleton` is MainLoop's singleton.
9. **§3.7 unit tests**: of the matched files only `ui_userinterface_abi_test.cpp` has code; the
   rest match in comments.
10. **§4.8 / NEXT_SESSION §2b item 2 "mirror PODs `AchievementGoals.h`, `PickingComponent.h`"**:
    `PickingComponent.h` mirrors an exe component, not a Picking type; `AchievementGoals.h` holds
    exe subclasses with an opaque base. The real duplicate-POD trees are the four
    `engine/types/*Loop*.h` + `MainLoop.h` mirrors and `pickingmesh.cpp`'s private `PickingMesh`
    (§4.2).
11. **§1.1 "`SoundBuffer`/`SoundEffect`/`Stream` have NO header, defined in the vtbatch files"**:
    `SoundBuffer` (0x80) and `SoundEffect` (0x40) are anonymous-namespace structs in
    `vtbatch7_soundbuffer_soundeffect.cpp:158/283`; `Stream` is defined NOWHERE (nor are
    `Audio::Device` and `Input::Device`).
12. **§1.2 raw per-dir counts** count comment lines (`saveload.cpp` 20 exports vs 19 real; the
    `:40` hit is a comment). Minor.
13. **§3.4 inventory attributes the five exe loop vtables to `gameupdateloop.cpp`**: the
    GameUpdateLoop vtable is built in `standalone/gameupdateloop_stubs.cpp`;
    `gameupdateloop.cpp` holds five slot bodies and uses Engine only through `g_pEngineSingleton`
    and `fn_MainLoop_SetNextLoop`.

Confirmed as stated: `singleton_ctors.cpp`'s cross-namespace stamp list; the three distinct
`closure_batch2b.cpp`; Engine 0 `_typeInfo` rows and 5 vftables; Audio 3 vftables / 2 typeInfo /
2 `RT_DELDTOR`; Pathfinding 2 / 1 / 1; Picking 1 / 1 / 1; zero printf-wrapped registrations.

---

## 9. How this was measured

All from the repo root at HEAD 96b03474 (working tree: `standalone/rt_export_addrs.inc` shows as
modified, line endings only — `git diff --numstat` prints nothing). Read-only.

```bash
SP=…/scratchpad/b37/0F
python tools/dispatch_slot_map.py > $SP/slots.txt           # 899 lines, the exe import table
python $SP/scan.py        # 1,741 files; demangled rows per ns; 128 files with >=1 site -> scan.json
python $SP/rows.py        # per-class export rows; the inventory's toks[2] method for comparison
python $SP/report2.py     # per file: distinct bodies per ns/class vs caller kinds -> report2.json
python $SP/defs.py        # definition file for every registered fn symbol -> defs.json
python $SP/ef.py; python $SP/ef2.py   # engine_funcs population by initialiser (90) -> ef_names.txt
python $SP/efuse.py       # 548 references to the 90 globals in 61 files
python $SP/incl.py <headers…>          # include resolution in SA_INCLUDES order
python $SP/nsrefs.py      # Rt<Ns>:: / <Ns>:: tokens outside comments and strings
python $SP/printfchk.py   # 81 files, 0 wrapped, 46 plain prints listed
python $SP/blocks.py      # assignment, disjointness assert, straddlers -> blocks.json
python $SP/mktables.py    # the §1 tables
# spot checks quoted inline
grep -nE '"\?\?_7[A-Za-z]+@(Achievement|Engine|Application|[0-9])@' standalone/rt_data_mint_tables.inc
grep -n 'GoalList' runtime/src/object/deleting_dtor*.cpp
grep -nE 'Register|GoalList@' runtime/src/achievement/batchc_goallist_dtor.cpp
grep -n '1801fe7b8' standalone/rt_data_mint.cpp code/Game/Game/game_main.cpp
grep -rnE '^\s*namespace\s+(Engine|Save|Achievement|Audio|Input|Pathfinding|Picking|Application)\b' runtime standalone engine code dx11 tests
grep -rnE '^\s*(struct|class)\s+(<the 49 class names>)\b[^;]*$' runtime standalone engine code dx11 tests
md5sum runtime/src/{engine,save,picking}/closure_batch2b.cpp
grep -nE '\-I' Makefile                                      # SA_INCLUDES order, line 67-69
for f in runtime/src/<8 dirs>/*.{cpp,h,inc}; do wc -l; grep -c RegisterRuntimeExport(; grep -c RegisterRuntimeAddr(; done
```

Scanner rules worth carrying forward: (1) demangle with back-references before any namespace
test; (2) join a `0x18…` literal BY ADDRESS, and fall back to the function-name convention and
then the directory only for unexported addresses — the directory fallback misfiles FOREIGN
helpers (`RtSystem_`, `RtVideo_`, `RtObject_`, `RtColl_`), which §3.1 lists by hand; (3) a
registering file is not always the defining file (Application); (4) resolve `#include "X.h"` in
`SA_INCLUDES` order — two `MainLoop.h`/`UpdateLoop.h` exist and a basename grep conflates them;
(5) a heredoc-written Python script lost a backslash in a regex and silently returned 12 globals
instead of 90 — scripts are written as files (NEXT_SESSION §2d "Process").

**NOT VERIFIED here** (left to the block-0 question pass): slot lists, `sizeof` and parent
literals from retail bytes; whether `Engine::MainLoop`/`Engine::Engine` are polymorphic (no
`??_7` export for either — a Ghidra xref read decides); whether any live route reaches
`Pathfinding::Scene`; the thread-safety of the `static Fn fn` caches in the three
`closure_batch2b.cpp` and `loops.cpp:1023` (INFERRED loader-thread reachable from the `LoaderLoop`
call graph, not traced).
