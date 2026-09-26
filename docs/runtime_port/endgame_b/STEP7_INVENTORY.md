# ENDGAME B.3 step 7 — `Engine`/`Save`/`Achievement`/`Audio`/`Input`/`Pathfinding`/`Picking`/`Application` inventory (read-only, 2026-09-26)

> **A MAP, NOT EVIDENCE.** Modelled on `UI_INVENTORY.md` (step 6). Nothing below was verified by
> a question-pass coordinator or read byte-wise the way `B35_RULES.md`/`B36_RULES.md` did for their
> namespaces. Header comments quoted below (mostly from `dx11/src/devicedata.cpp` and the
> `standalone/*_stubs.cpp` vtable banners) ARE treated as evidence only where they cite a
> disassembled instruction inline, exactly as those files do — a header banner with no cited
> instruction is NOT evidence, same caveat `UI_INVENTORY.md` §1.1 raised for `runtime/src/ui`. No
> tree file was edited to produce this. All greps are scoped to `code/ engine/ standalone/ runtime/
> dx11/ tests/`, excluding `standalone/build*/`, `tests/unit/build/`, `.claude/worktrees/`.

Scope: eight namespaces `Engine`, `Save`, `Achievement`, `Audio`, `Input`, `Pathfinding`,
`Picking`, plus `Application` (its four dx11 import rows only — the class itself lives in
`Runtime-x64.dll`, `runtime/types/application/Application.h`, NOT polymorphic).

---

## 1. Class list

### 1.1 Headers — `runtime/types/<ns>/*.h` (one namespace spelling each, ALL `Rt<Namespace>`, unlike
UI's four-way mess — VERIFIED, `grep -n "^namespace" runtime/types/<ns>/*.h` for all seven dirs
that have a header; `Pathfinding` has NO header directory at all)

| dir | header(s) | lines | static_assert | namespace | classes |
|---|---|---|---|---|---|
| `engine` | `Engine.h`, `MainLoop.h`, `UpdateLoop.h` | 753 | 68 | `namespace RtEngine {` (all 3) | `Engine`, `MainLoop`, `VideoResourceEntry`, `MainLoopCallback`, `UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT` |
| `save` | `SaveLoad.h` | 351 | 41 | `namespace RtSave {` | `ModInfo`, `SaveInfo`, `SavedState`, `SaveLoad` |
| `achievement` | `AchievementManager.h`, `Goal.h` | 430 | 29 | `namespace RtAchievement {` (both) | `GoalListView`(a lane-local view, not the real `GoalList`), `GoalInstanceView`, `Achievement`, `AchievementInstance`, `AchievementManager`, `Goal`, `GoalInstance`, `GoalList` |
| `audio` | `Audio.h` | 200 | 31 | `namespace RtAudio {` | `Channel`, `EmitterInstance`, `EffectSlot`, `Scene`, `Configuration` — **`SoundBuffer`/`SoundEffect`/`Stream` have NO header**, defined directly in `runtime/src/audio/vtbatch7_soundbuffer_soundeffect.cpp` / `vtbatch7b_stream_run_win.cpp` |
| `input` | `Input.h` | 286 | 26 | `namespace RtInput {` | `KeyMap`, `Binding`, `BindingNode`, `Device`(`State`), `Configuration` |
| `pathfinding` | *(none)* | 0 | 0 | — | `PointList` defined in-`.cpp` only, `runtime/src/pathfinding/pointlist.cpp` (own banner documents the 0x50 layout in prose instead) |
| `picking` | `Scene.h` | 349 | 37 | `namespace RtPicking {` | `Triangle`, `UV16`, `SubMesh`, `PickingMesh`, `Entry`, `Cell`, `Link`, `Scene`, `AddDesc` — **includes `../volume/Volume.h` for `Box`**, the one header in this whole set that touches the Volume/Rectangle name-collision hazard UI flagged |
| `application` | `Application.h` | 158 | 9 | `namespace RtApplication {` | `Application` only — **NOT polymorphic**: ctor stores a zeroed qword at +0x00, not a `??_7` vtable pointer, and the DLL's 147 `??_7` exports include none for it (VERIFIED, header's own quoted disassembly) |

Two same-spelled `Scene` classes exist in this batch alone (`RtAudio::Scene`,
`RtPicking::Scene`), and a THIRD elsewhere in the tree (`RtVideo`/`Graphics`'s scene per
`VIDEO_GRAPHICS_INVENTORY.md`) — a step-7 sweep anchored on the bare name `Scene` will collide
three ways; always qualify by namespace.

### 1.2 `.cpp` files — `runtime/src/<ns>/*` (VERIFIED `ls`/`wc -l`/`grep -c` per file)

| dir | `.cpp` files | lines | export | addr |
|---|---|---|---|---|
| `engine` | 14 | 7,217 | 72 | 57 |
| `save` | 9 | 6,107 | 34 | 29 |
| `achievement` | 11 | 4,007 | 63 | 60 |
| `audio` | 15 | 6,144 | 53 | 46 |
| `input` | 9 | 3,381 | 34 | 33 |
| `pathfinding` | 2 | 384 | 9 | 13 |
| `picking` | 6 | 2,833 | 22 | 42 |
| `application` | 8 | 2,109 | 29 | 32 |
| **TOTAL** | **74** | **32,182** | **316** | **312** |

Registrations (628 total) are a small fraction of UI's 1,663 or even Video+Graphics's ~1,012 —
this whole 8-namespace batch is closer in size to IO (~593) than to any single step-5/6 namespace.

**Files a step-7 planner meets first, largest by lines**: `saveload_update.cpp` (2,116),
`loops.cpp` (1,654, Engine — holds `Loader`+`LoaderLoop`+`DialogLoopT`+`DialogLoaderLoopT` bodies
together), `audio.cpp` (1,219), `saveload.cpp` (1,130), `singleton_ctors.cpp` (928, Engine),
`achievement_manager.cpp` (1,007), `scene_grid.cpp` (1,205, Picking), `scene.cpp` (1,062, Picking).

**Owner-retained zero/near-zero registration files** (same convention as `rt_video_port.cpp`):
`rt_engine_port.cpp`, `rt_save_port.cpp`, `rt_achievement_port.cpp`,
`rt_factories_achievement/audio/pathfinding/picking.cpp`, `rt_audio_port.cpp`,
`rt_input_port.cpp`, `rt_picking_port.cpp`.

**A cross-namespace file hazard, NOT a step-6-style same-name coincidence**:
`runtime/src/engine/singleton_ctors.cpp` (928 lines) stamps ctors for `Audio::Configuration`,
`Audio::Device`, `Input::Configuration`, `Input::Device`, `Video::Configuration`, `Video::Device`,
`Object::{ExternalFactory,Factory,InternalFactory}`, `IO::{FileSystem,Modification,Persistence}`
and `System::MemoryAllocator` (VERIFIED, `grep -oE '@[A-Za-z0-9_]+@[A-Za-z0-9_]+@@'`) — filed under
`engine/` but not an Engine-only file at all. It must be owner-retained across whichever blocks
touch Audio::Device, Input::Device or Video::Device, not split by namespace.
`closure_batch2b.cpp` appears in THREE of these dirs (`engine`, `save`, `picking`) but VERIFIED via
`md5sum` to be three DIFFERENT files (a wave/batch naming convention, not a shared file) — no
disjointness issue there.

### 1.3 Polymorphism / vtables (VERIFIED against `standalone/rt_data_mint_tables.inc`)

| namespace | `??_7*@NS@@6B@` mint rows | slot counts | `?_typeInfo@` rows | `RT_DELDTOR` rows |
|---|---|---|---|---|
| `Engine` | 5: `UpdateLoop`(12), `LoaderLoop`(12), `DialogLoopT`(13), `DialogLoaderLoopT`(13), `Loader`(2) | 12–13 (`Loader` only 2) | **0** | 4 (`UpdateLoop`,`LoaderLoop`,`DialogLoaderLoopT`,`DialogLoopT` — NOT `Loader`) |
| `Save` | 0 | — | 0 | 0 |
| `Achievement` | 3: `AchievementInstance`(4), `Goal`(10), `GoalInstance`(7) | 4/10/7 | 3 (all) | 4 (adds `Achievement`, `GoalList` — 2 classes with NO mint vtable but a registered dtor) |
| `Audio` | 3: `SoundBuffer`(9), `SoundEffect`(9), `Stream`(9) | 9 each | 2 (`SoundBuffer`,`SoundEffect` — NOT `Stream`) | 2 (`SoundBuffer`,`SoundEffect` — NOT `Stream`) |
| `Input` | 0 | — | 0 | 0 |
| `Pathfinding` | 2: `IPathInfo`(2), `PointList`(9) | 2/9 | 1 (`PointList` only) | 1 (`PointList`) |
| `Picking` | 1: `PickingMesh`(9) | 9 | 1 | 1 |
| `Application` | 0 (not polymorphic, §1.1) | — | 0 | 0 |

**`Engine::MainLoop` has NO `??_7` mint row at all** despite carrying its own `?_singleton@
MainLoop@Engine@@1PEAV12@EA` cutover-manifest row and 25 DLL exports (§1.1) — its polymorphism
status is UNKNOWN, not confirmed either way this pass (no Ghidra vtable-xref read was done); flag
for the step-7 question pass rather than assuming it is or is not polymorphic.

**Engine's 0/6 typeInfo coverage is the mirror image of UI's 68/68 and Achievement/Audio/
Pathfinding/Picking's ~100%** — worth a direct read before assuming any Engine class is an
`Object::External`/`CompiledExternal` subclass the way `B35_RULES.md`/`B36_RULES.md` established
for their namespaces.

**Two Achievement classes have a registered deleting dtor but NO mint vtable**: `Achievement`
(the class, distinct from `AchievementInstance`) and `GoalList`. `Achievement::Achievement`'s dtor
is `RT_DELDTOR(0x180001680, 0x18009e180, 0x058, "Achievement::Achievement")`
(`runtime/src/object/deleting_dtor.cpp:288`) with no corresponding `??_7Achievement@Achievement@@
6B@` anywhere in the mint table — a class destroyed by address, not through a vtable slot 0. Same
shape as `Pathfinding::PointList`/`Picking::PickingMesh` having a header-free `.cpp`-only
definition — this whole batch of namespaces runs looser on the "one header per class" convention
than UI did.

---

## 2. Counts

**Registrations** (VERIFIED §1.2): 316 export + 312 addr = **628** across 74 `.cpp` files, 32,182
lines.

**Header lines**: 2,527 across 6 header dirs (`Pathfinding` has none) — a fifth of UI's 4,231, in
line with the namespaces' much smaller `runtime_census.py` totals (below).

**Minted vtable rows**: 14 (`Engine` 5, `Achievement` 3, `Audio` 3, `Pathfinding` 2, `Picking` 1,
`Save`/`Input`/`Application` 0) — far short of UI's 68 or even Video/Graphics's 39 combined; this
whole 8-namespace batch is much less polymorphism-heavy than any prior B.3 step.

**`tools/runtime_census.py`** (VERIFIED, ran read-only, `git status` confirmed no write) — copied
verbatim:

| namespace | total (DLL rows) | called | ported | called-coverage | ceiling |
|---|---|---|---|---|---|
| Achievement | 118 | 50 | 43 | 86.0% | 98.0% |
| Audio | 137 | 40 | 33 | 82.5% | 95.0% |
| Engine | 108 | 40 | 34 | 85.0% | 92.5% |
| Input | 63 | 29 | 27 | 93.1% | 93.1% AT CEILING |
| Save | 37 | 29 | 29 | 100.0% | — |
| Application | 26 | 13 | 12 | 92.3% | 92.3% AT CEILING |
| Picking | 49 | 11 | 9 | 81.8% | — |
| Pathfinding | 45 | 2 | 0 | 0.0% | — |
| **TOTAL (these 8)** | **583** | **214** | **187** | — | — |

Gap table rows for the same 8 (VERIFIED, same script, `BODY/CTOR/DTOR/OPNEWDEL/OPERATOR/VTABLE/
DATA/UNKNOWN` columns): `Achievement` 7 (4 VTABLE, 3 DATA), `Audio` 7 (3 VTABLE, 4 DATA), `Engine`
6 (3 VTABLE, 3 DATA), `Input` 2 (0/2), `Pathfinding` 2 (1/1), `Picking` 2 (1/1), `Application` 1
(0/1). `Save` has ZERO gap (100% called-coverage, not in the gap table at all) — **the only fully
closed namespace in the whole census**, a strong "start here" signal for step 7 the same way IO
was cited that way for step 5/6.

`Pathfinding`'s 0.0% called-coverage on 45 DLL rows, 0 ported, is the single lowest-activity
namespace this project has measured in any step's inventory — worth an explicit question in block
0 about whether the 2 "called" rows are even reachable from a live route, or whether Pathfinding is
mostly dead/mod-tool-only code (`PointList` reads as a level-editor artifact from its own banner
comment, §1.2 above).

**Cutover manifest** (VERIFIED, `python tools/cutover_manifest.py --table`, read-only): **9** rows
across these namespaces, ALL storage/singleton rows, ALL tagged REQUIRED-at-cutover (verdict
inverts the same way every non-UI-Dialog row in the manifest does):

```
Achievement 1  ?_singleton@AchievementManager@Achievement@@0PEAV12@EA
Application 1  ?_singleton@Application@1@0PEAV11@EA
Audio       2  ?_singleton@Configuration@Audio@@0PEAV12@EA, ?_singleton@Device@Audio@@0PEAV12@EA
Engine      3  ?_loader@LoaderLoop@Engine@@1VLoader@2@A, ?_singleton@Engine@1@0PEAV11@EA,
               ?_singleton@MainLoop@Engine@@1PEAV12@EA
Input       2  ?_singleton@Configuration@Input@@0PEAV12@EA, ?_singleton@Device@Input@@0PEAV12@EA
```

No `Save`, `Pathfinding` or `Picking` rows — those three have no REQUIRED storage gap the tool
flagged.

---

## 3. Caller surfaces

Measured by BINDING, per `B36_RULES.md` §14 / `B35_RULES.md` §7. Only 5 of the 8 namespaces have
ANY `engine_funcs.h` doors at all — `Pathfinding`, `Picking` and `Application` have ZERO, a
structural difference from every namespace measured in steps 5/6.

### 3.1 `engine_funcs.h` global doors bound to an `@NS@@` export

VERIFIED by joining each `fn_* = reinterpret_cast<..._t>(ResolveExport("..."))` pair (the fn_
name sits on the line immediately before the `ResolveExport` call in every site checked — NOT
window-searched loosely, which double-counts in this file's dense two-line idiom):

| namespace | doors | `code/Game` call sites (summed) |
|---|---|---|
| `Engine` | 10 | 5 |
| `Save` | 10 | 43 |
| `Achievement` | 9 | 12 |
| `Audio` | 15 | 39 |
| `Input` | 19 | 67 |
| `Pathfinding` | 0 | 0 |
| `Picking` | 0 | 0 |
| `Application` | 0 | 0 |
| **TOTAL** | **63** | **166** |

Top individual doors by call count: `fn_KeyMap_GetValue` 23, `fn_KeyMap_Pressed` 15,
`fn_SaveLoad_GetOperation` 9, `fn_Audio_Scene_Pause` 9, `fn_SaveLoad_SetOperation` 8,
`fn_SaveLoad_GetSaveInfo` 8, `fn_Input_Device_GetValue` 6, `fn_Audio_Scene_PlayEffect` 6. This
whole batch's 166 calls is an order of magnitude below UI's 1,434 and even below Video/Graphics's
61 combined for the ENGINE_FUNCS binding specifically — Input and Save carry almost all of it.

### 3.2 `ResolveExport("...@NS@@...")` by mangled name, tree-wide by directory

VERIFIED, `grep -rn` (not double-counting the `engine_funcs.h` doors' own resolution mechanism,
same convention as `UI_INVENTORY.md` §3.2):

| namespace | `engine/funcs` | `runtime/src` | `standalone` | `code/Game` | total |
|---|---|---|---|---|---|
| Engine | 15 | 10 | 2 | 1 (`Game`) | 28 |
| Save | 13 | 0 | 5 | 5 (`Game`) | 23 |
| Achievement | 12 | 7 | 3 | 12 (`Game`) | 34 |
| Audio | 16 | 1 | 0 | 6 (`Game`) | 23 |
| Input | 21 | 0 | 1 | 0 | 22 |
| Pathfinding | 0 | 0 | 0 | 0 | 0 |
| Picking | 0 | 0 | 5 | 3 (`Game`) | 8 |
| Application | 0 | 0 | 0 | 0 | 0 |

`Picking`'s 8 hits (5 `standalone`, 3 `code/Game/Game`) with ZERO `engine_funcs.h` doors confirms
it is called almost entirely through a different binding (§3.4 below), not this one.

### 3.3 `RT_EXPORT_ONCE`/`RT_DATA_SLOT` (runtime-internal registration idioms, not a `code/Game`
caller surface — VERIFIED all instances live inside `runtime/src`)

| namespace | `RT_EXPORT_ONCE` | `RT_DATA_SLOT` |
|---|---|---|
| Engine | 3 | 1 |
| Save | 6 | 2 |
| Achievement | 1 | 0 |
| Audio | 10 | 13 |
| Input | 10 | 8 |
| Pathfinding | 0 | 2 |
| Picking | 0 | 2 |
| Application | 0 | 0 |

### 3.4 Exe dispatch slots (spelling 10) and their `ResolveAddr(Module::Exe, 0x...)` hits

VERIFIED via `python tools/dispatch_slot_map.py` (833-slot PE import table, read-only) intersected
against the 3,085 `ResolveAddr(Module::Exe, ...)` call sites tree-wide:

| namespace | dispatch slots | `ResolveAddr(Exe)` hits | hit files |
|---|---|---|---|
| Engine | 27 | 2 | `engine/funcs/engine_funcs.cpp` ×2 |
| Save | 14 | 5 | `engine/funcs/engine_funcs.cpp` ×5 |
| Achievement | 27 | 11 | `code/Game/Goal/{timegoalinstance,statgoal,timegoal}.cpp`, `code/Game/Game/gameupdateloop.cpp` ×3, `engine/funcs/engine_funcs.cpp` ×2, `standalone/address_registry.cpp` |
| Audio | 18 | 3 | `code/Game/Tutorial/finishtutorialinstance.cpp`, `engine/funcs/engine_funcs.cpp` ×2 |
| Input | 21 | 5 | `engine/funcs/engine_funcs.cpp` ×4, `code/Game/Player/pathtool.cpp` |
| Pathfinding | 1 | 0 | — |
| Picking | 8 | **8 (100%)** | `code/Game/Player/{bridgetool_calcpath,fencedtool_calcpath,pathtool}.cpp`, `code/Game/Game/mapquery.cpp`, `code/Game/Component/{quarrycomponent_deform,terraformcomponent,pickingcomponent}.cpp` |
| Application | 0 | 0 | — |

**`Picking` is called EXCLUSIVELY through this binding** — every one of its 8 dispatch slots that
is called at all is called this way, concentrated in `code/Game/Player` (pathfinding/building
tools) and `code/Game/Component` (terrain/quarry deformation). A step-7 caller batch for Picking
must grep `ResolveAddr(Module::Exe,` with hex literals in those two directories; a name-anchored
sweep would find nothing (§3.2 already showed only 3 `code/Game` name-based hits).

### 3.5 By-address Runtime slot (spelling 3, the one `B36_RULES.md` §14.1 says was MISSED in step 6)

VERIFIED: built the address set of every `@NS@@` export from `standalone/rt_export_addrs.inc`
(4,926 rows total; per-namespace counts: Engine 58, Save 30, Achievement 70, Audio 113, Input 57,
Pathfinding 32, Picking 36, Application 0), then intersected against all 61 tree-wide
`RtAddr<...>(0x18...)` / `ResolveAddr(Module::Runtime, 0x18...)` call sites:

| namespace | hits | files |
|---|---|---|
| Engine | 3 | `code/Game/Game/game_main.cpp`, `engine/funcs/engine_funcs.cpp`, `standalone/rt_data_mint.cpp` (last two both hit `0x1801fe7b8`) |
| Audio | 1 | `engine/funcs/engine_funcs.cpp` (`0x1801fe758`) |
| Picking | 4 | `code/Game/Component/{citizenai_executor,floorcomponent,pickingcomponent}.cpp`, `floorcomponent.cpp` again at a second address |
| Save/Achievement/Input/Pathfinding/Application | 0 | — |

Small in absolute terms (8 total) but real, and per `B36_RULES.md` this spelling is invisible to
every other sweep — a step-7 planner touching `Picking`/`Engine`/`Audio` callers must grep for
`RtAddr<` and `ResolveAddr(Module::Runtime,` with hex literals, not just mangled-name strings.

### 3.6 Hand-inlined `(*(void***)p)[N]` vtable-slot calls

VERIFIED, scoped to the directories that plausibly consume these namespaces
(`code/Game/{Goal,Player,Component,Game,Tutorial}`): only **2 occurrences**, both in
`code/Game/Component` — a tiny hazard surface compared to UI's 417 in `code/Game/Dialog` alone.
**NOT VERIFIED** which vtable either targets, and this sweep does not cover every directory that
might touch these namespaces (`code/Game/Dialog`, `code/Game/Entity`, `code/Game/Work` were not
re-checked here) — treat as a lower bound, not a ceiling.

### 3.7 Unit tests touching these namespaces

VERIFIED, `grep -rl "NS::" tests/unit --include=*.cpp --include=*.h`: `Engine` 2 files, `Save` 2
files, `Audio` 1 file, `Input` 1 file, `Achievement`/`Pathfinding`/`Picking`/`Application` 0 —
same bare-struct name-lookup caveat UI §5.13 raised applies here too (`Pathfinding::PointList`'s
no-namespace-in-source shape, §1.1, would not show up in a `Pathfinding::` grep even if a test used
it).

---

## 4. Special shapes

### 4.1 Exe-side subclasses — Engine has the richest set in this batch, ALREADY well-documented

Unlike UI's 22 INFERRED-count `DialogController` subclasses, these five are VERIFIED by name and
each vtable's own banner comment (which cites disassembled addresses per slot, not just prose):

- **`GameUpdateLoop`** — exe subclass of `Engine::UpdateLoop`, 0x160 bytes, vtable at
  `0x14015e2f0`, 22 slots, built in `code/Game/Game/gameupdateloop.cpp` (comment: "Clean
  reimplementation of GameUpdateLoop vtable methods from Application-x64.exe").
- **`MenuDialogLoopT` ("MenuDLT")** — exe subclass of `Engine::DialogLoopT`, 13-slot vtable at
  `0x14015ed00`, built in `standalone/menudialogloopt_stubs.cpp` (own banner: "the exe-internal
  subclass of Engine::DialogLoopT that drives the main menu").
- **The splash `DialogLoaderLoopT` subclass** — one class, vtable at `0x140158800`, used for
  BOTH `Dialog\Loading.rsc` and `Dialog\ShiningRock.rsc`, built in `standalone/splash_loop_stubs.cpp`.
- **Two `LoaderLoop` subclass vtables** ("inner" `0x140158870`, "outer" `0x1401588d0`), nested in
  the original `GameMain`, built in `standalone/loader_loop_stubs.cpp`.

All five are `standalone/`-owned stub vtables (not `code/Game`-owned classes with real C++
inheritance) except `GameUpdateLoop`, which has its own directory of files
(`code/Game/Game/gameupdateloop*.cpp`, 4 files). **A step-7 base-class block for `UpdateLoop`/
`DialogLoopT`/`DialogLoaderLoopT`/`LoaderLoop` inherits this exact QUALIFIED-CALL hazard
`B36_RULES.md` §2d already names for UI's dialog controllers**: a slot body in one of these five
hand-built subclasses calling its own base's virtual must qualify
(`->Engine::UpdateLoop::Update()`), or an unqualified call redispatches into itself.

### 4.2 Achievement's exe-side Goal family — 7 concrete subclasses, all with real headers already

`engine/types/AchievementGoals.h` (165 lines) declares `BuildingGoal`, `PopulationGoal`,
`StatGoal`, `TimeGoal`, `FinishTutorialGoal`, `FinishTutorialInstance`, `TimeGoalInstance` — exe
subclasses of `Achievement::Goal`/`GoalInstance`, each with its own `.cpp` in `code/Game/Goal/`
(`buildinggoal.cpp`, `populationgoal.cpp`, `statgoal.cpp`, `timegoal.cpp`,
`timegoalinstance.cpp`, plus shared `goal_exe_helpers.cpp`/`statvaluedispatch.cpp`). All six
class-bearing files DO mention "vtable" (VERIFIED grep) but zero hand-inlined `(*(void***)p)[N]`
calls were found there (§3.6) — this family looks like the cleanest, most already-prepared exe
subclass set in the whole batch. `PickingComponent.h` (39 lines, `engine/types/`) is the only other
mirror-POD file touching these namespaces and is a single small struct, not a class hierarchy.

### 4.3 The four dx11 `Application` import rows (NEXT_SESSION.md item 11, VERIFIED read of
`dx11/src/devicedata.cpp:229-240`)

```
void  (*AppRegisterMessageCallback)(void*, unsigned, void*);   // 0x180013290
void  (*AppUnregisterMessageCallback)(void*, unsigned);        // 0x180013298
HINSTANCE (*AppGetInstance)(void*);                            // 0x1800132a8
void** appSingleton;                                           // 0x180013310  (?_singleton@Application@1@)
```
The file's own comment says why these three function pointers plus the singleton slot were left
as raw dx11 imports rather than converted in step 5: *"`Application::Application` is not a real
C++ class yet: the layout lives in `runtime/types/application/Application.h` as the POD
`RtApplication::Application`, which is a step-6 conversion, not a Video/Graphics one. There is no
member to call."* Step 7 making `Application` a real class is the trigger to convert these four.
Also note the SAME comment block corrects three historical `decomp/videodx11.md` errors that
conflated `Application::_singleton` (0x180013310) with two DIFFERENT `Video::Configuration`/
`Video::Device` singletons (0x180013480/0x1800134b8) — a step-7 Application pass should re-read
that correction before touching any of the three.

### 4.4 Threading — a SECOND thread beyond the already-documented loader thread

`docs/runtime_port/endgame_b/NEXT_SESSION.md` item 10 already covers the `Engine::Loader` thread
(`?Run@Loader@Engine@@UEAAXXZ`, slot 0 of `??_7Loader@Engine@@6B@`) and its device-lost race — nothing
new to add there. **Newly noted this pass**: `runtime/src/audio/audio_stream_win.cpp` documents a
SEPARATE worker thread — its own banner: *"the base ctor `_beginthreadex`s a real worker"* backing
`Audio::Stream`'s base `System::Thread` — NOT investigated further this pass (no read of what data
that thread touches concurrently with the main/loader threads), but it is a second live
`CreateThread`/`_beginthreadex` site in this whole batch and was not on any prior step's radar.
`runtime/src/engine/singleton_ctors_win.cpp` and several comments (`engine_lifecycle.cpp`,
`saveload_saveinfo.cpp`, `audio_device.cpp`) show `EnterCriticalSection`/`LeaveCriticalSection`
pairs guarding the allocator and per-object locks — all already-documented allocator/lock idioms,
not new hazards.

### 4.5 Hidden-retslot exports — ONE dangerous virtual case found, unlike UI's clean zero

A mangled-return (`?A[VU]`) grep across all eight namespaces' rows in `rt_export_addrs.inc` found
26 hidden-retslot exports (vs. UI's dozen). Cross-checked each against the mint vtable slot tables
(VERIFIED): **25 are non-virtual** (`QEAA`/`QEBA` calling convention), but
**`?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ` IS virtual** — slot 9 (0-based,
`kVSlots63[9]`) of `Goal`'s 10-slot vtable, confirmed in `standalone/rt_data_mint_tables.inc:2511`.
This is exactly the dangerous (class, slot) shape `B35_RULES.md` found 9 of for Video/Graphics and
UI found none of — a by-value `Object::Type` return through a live virtual slot, needing the
`T* M(T* ret, args…)` GCC-ms_abi retslot-before-`this` shape (`B34B_RULES`/`B35_RULES` class-shape
rule 1) the moment `Goal` becomes a real class.

### 4.6 By-value holdings (raw, unfiltered grep for `String `/`Color `/`Rectangle `/`Array<`/
`ExternalPtr<`/`InternalPtr<`-typed members across each namespace's header)

`achievement` 29, `picking` 17, `save` 12, `engine` 8, `audio` 7, `input` 0, `application` 0 (no
header lines to search besides the 158-line `Application.h`, which has none). Smaller in absolute
terms than UI's 92 but `Achievement`'s 29 in a 430-line header is a denser hit rate — worth the
same per-class confirmed read UI's hazard 4 called for before assuming trivial copyability.

### 4.7 No printf-wrapped `Register*` call found

VERIFIED, `grep -rn "printf(.*Register\|TRACE(.*Register"` across all eight `runtime/src/<ns>`
dirs returned nothing — the perf-build-strips-printf-arguments bug class does not have an obvious
instance here, same clean finding UI reported for its own directory.

### 4.8 Duplicate POD tree — much smaller than UI's 1,887-line hazard

Only two files: `engine/types/AchievementGoals.h` (165 lines, §4.2) and
`engine/types/PickingComponent.h` (39 lines) — 204 lines total vs. UI's 1,887. Neither was
cross-checked against `runtime/types/` for field agreement this pass (UI's hazard 1 spot-check
method was not repeated here) — flag, don't assume agreement.

### 4.9 Volume.h / Rectangle trap

`runtime/types/picking/Scene.h:31` is the ONLY header in this whole batch that includes
`../volume/Volume.h` (for `Box`, not `Rectangle` — so the specific name-collision UI hazard 12
warned about does NOT apply here, but the `<windows.h>`-ordering caution from the same hazard still
does for any `.cpp` that includes both).

---

## 5. Proposed blocks by disjoint file set (first-draft; coordinator confirms disjointness)

Modelled on `UI_INVENTORY.md` §6 / `B36_RULES.md` §2c's ordering logic — base classes serial-owned
first (here, the exe-subclass-bearing ones), then disjoint leaf work fully parallel.

| block | scope | rationale |
|---|---|---|
| **0 — question pass (serial, one owner)** | resolve `Engine::MainLoop`'s polymorphism status (§1.3); retail-bytes read of all 14 mint vtables' slot lists/`_ParentTypeName`; enumerate `GameUpdateLoop`/`MenuDLT`/splash-loop/two-`LoaderLoop` exe subclasses fully (§4.1) and confirm the QUALIFIED-CALL hazard sites in each; decide `Application`'s ownership (still not a real class — is step 7 the one that makes it one, given the 4 dx11 rows waiting on it, §4.3); confirm `Goal::GetInstanceType`'s virtual retslot shape against a live disassembly read | this batch has had NO Q1-Q6-equivalent pass, same starting point B36 opened with for UI |
| **1 — `Engine` (`MainLoop`, `UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`, `DialogLoaderLoopT`) + `Application`, serial** | `runtime/types/engine/*.h`, `runtime/types/application/Application.h`, `runtime/src/engine/*`, the five exe-subclass files (§4.1), `runtime/src/application/*` (n/a — no such dir; `runtime/src/application` DOES exist, 8 files) | highest exe-subclass blast radius in this batch (5 hand-built vtables); `Application` becoming real unblocks the 4 dx11 import rows (§4.3); `singleton_ctors.cpp` cross-namespace hazard (§1.2) must be resolved here first since it also stamps Video/Audio/Input singletons |
| **2 — `Save`** | `runtime/types/save/SaveLoad.h`, `runtime/src/save/*` (9 files) | ALREADY AT 100% called-coverage (§2) and ZERO exe-subclass/hidden-retslot/threading hazards found this pass — the cleanest, lowest-risk block in the batch; do it early to bank a win the way IO was step 5's model case |
| **3 — `Achievement`** | `runtime/types/achievement/*.h`, `runtime/src/achievement/*` (11 files), the 7-class exe Goal family (`engine/types/AchievementGoals.h`, `code/Game/Goal/*`) | the ONE virtual hidden-retslot in this whole batch (§4.5) lives here (`Goal::GetInstanceType`) — needs the retslot-before-`this` shape from day one; `Achievement`/`GoalList`'s no-vtable-but-has-dtor shape (§1.3) needs settling before the class boundary is drawn |
| **4 — `Audio`** | `runtime/types/audio/Audio.h`, `runtime/src/audio/*` (15 files, incl. the header-free `SoundBuffer`/`SoundEffect`/`Stream`) | the SECOND thread (§4.4, `audio_stream_win.cpp`'s `_beginthreadex` worker) needs its own threading read before `Stream`'s class shape is finalized |
| **5 — `Input`** | `runtime/types/input/Input.h`, `runtime/src/input/*` (9 files) | AT CEILING already (93.1%, §2), zero polymorphic classes (§1.3) — should be nearly mechanical once `Engine`'s cross-namespace `singleton_ctors.cpp` dependency (block 1) is settled |
| **6 — `Pathfinding` + `Picking`** | `runtime/src/pathfinding/*` (2 files, no header dir), `runtime/types/picking/Scene.h`, `runtime/src/picking/*` (6 files) | smallest two namespaces by every measure; `Picking`'s callers are ENTIRELY exe-dispatch-slot (§3.4) and by-address-runtime-slot (§3.5) bound — a caller batch here must grep hex literals, not names, from the start; `Pathfinding`'s near-zero census activity (§2) makes it low-risk to bundle with Picking rather than stand alone |
| **7 — Caller batches by directory, AFTER blocks 1-6** | `code/Game/{Goal,Player,Component,Game,Tutorial}` split by the binding-concentration findings in §3: `code/Game/Player`+`Component` for Picking's dispatch-slot calls, `code/Game/Game` for Engine/Save/Achievement's mixed bindings, `code/Game/Goal` for the Achievement exe subclasses, `code/Game/Tutorial` for Audio's one dispatch hit | mirrors the UI/B35 directory-partition approach; unlike UI, no single directory dominates here — the 166 `engine_funcs.h` calls and 8+8 dispatch-slot hits are each concentrated in DIFFERENT directories, so a single "biggest file" batch does not exist for this step |

Blocks 1-6 are NOT confirmed disjoint by file (block 0's job): `singleton_ctors.cpp` (Engine-filed,
cross-namespace, §1.2) is the one file certain to need coordination between blocks 1, 4 and 5
before any of them can call `Audio::Device`/`Input::Device`/`Video::Device`'s layout final.

---

## 6. Summary of what is VERIFIED vs NOT VERIFIED / INFERRED

**VERIFIED this pass**: every header's line count/static_assert count/namespace line (read, not
assumed) for all 7 header-bearing dirs; the full per-`.cpp`-file export/addr/line counts for all 74
files across 8 `runtime/src` dirs; all 14 mint vtable rows and their slot counts (parsed from
promotion-table entries); the `?_typeInfo@`/`RT_DELDTOR` row counts and names per namespace; the
`Runtime-x64.dll` export-directory class list via `tools/pe_exports.py` against the retail DLL at
`C:\GOG Games\Banished\Runtime-x64.dll` (4,926 total exports; confirmed `Application` has ZERO
named exports); `tools/runtime_census.py`'s per-namespace rows (ran read-only, `git status`
confirmed no write); `tools/cutover_manifest.py --table`'s 9 rows for these namespaces; all
`engine_funcs.h` door names, their binding mangled names, and `code/Game` call counts (fixed a
false-zero from an early BRE-escaping bug in the measurement script — verified the fix against a
known 15-call door before trusting the rest); `ResolveExport`/`RT_EXPORT_ONCE`/`RT_DATA_SLOT`
counts per directory; the 833-slot exe dispatch table via `tools/dispatch_slot_map.py` (read-only,
`pefile` against the retail exe) and its intersection with the 3,085 tree-wide
`ResolveAddr(Module::Exe,...)` sites; the 4,926-row `rt_export_addrs.inc` intersected with the 61
tree-wide `RtAddr<>`/`ResolveAddr(Module::Runtime,...)` sites (the spelling `B36_RULES.md` §14.1
flagged as missed in step 6); the one virtual hidden-retslot export cross-checked against its mint
vtable slot index; the five exe-side subclass vtable banners (`gameupdateloop.cpp`,
`menudialogloopt_stubs.cpp`, `splash_loop_stubs.cpp`, `loader_loop_stubs.cpp`) read directly, each
citing disassembled addresses per slot; the four dx11 `Application` import rows read directly from
`dx11/src/devicedata.cpp`; the `singleton_ctors.cpp` cross-namespace class list; the absence of a
printf-wrapped `Register*` call; the `md5sum` proof that `closure_batch2b.cpp` is three distinct
files, not one shared file.

**NOT VERIFIED / INFERRED**: `Engine::MainLoop`'s polymorphism status (no `??_7` row found, but not
proven absent by a Ghidra vtable-xref read); whether `Achievement`/`GoalList`'s no-vtable-but-dtor
shape means they are non-polymorphic value types or just missing from the mint table for another
reason; the exact base-class parent chain for any of these 8 namespaces' classes (no
`?_ParentTypeName@` literal was read this pass); the `audio_stream_win.cpp` worker thread's data
races with the main/loader threads (flagged, not investigated); whether the two `engine/types`
mirror-POD headers (`AchievementGoals.h`, `PickingComponent.h`) agree field-for-field with
`runtime/types/`; multiple inheritance / secondary vptrs anywhere in this batch (not checked at
all); whether any `code/Game` directory outside the five checked in §3.6 has hand-inlined vtable
calls against these namespaces; whether `runtime/src/<ns>/README.md` files (not read this pass) are
stale the way UI's and Video/Graphics's were found to be.

---

## How this was measured

All commands run from the repo root, read-only (no file was edited to produce this document;
`git status` was checked clean of unintended writes after the pass):

```bash
# header/src inventories
ls runtime/types/<ns>; wc -l runtime/types/<ns>/*.h; grep -n "^namespace" runtime/types/<ns>/*.h
grep -cH static_assert runtime/types/<ns>/*.h
grep -nE "^(struct|class) [A-Za-z0-9_]+" runtime/types/<ns>/*.h
for f in runtime/src/<ns>/*.cpp; do wc -l "$f"; grep -c RegisterRuntimeExport "$f"; grep -c RegisterRuntimeAddr "$f"; done
md5sum runtime/src/{engine,save,picking}/closure_batch2b.cpp
grep -oE '@[A-Za-z0-9_]+@[A-Za-z0-9_]+@@' runtime/src/engine/singleton_ctors.cpp | sort -u

# mint vtable / typeInfo / RT_DELDTOR
grep -oE '"\?\?_7[A-Za-z0-9_]+@NS@@6B@"' standalone/rt_data_mint_tables.inc
grep -oE '"\?_typeInfo@[A-Za-z0-9_]+@NS@@[^"]*"' standalone/rt_data_mint_tables.inc
grep -hoE 'RT_DELDTOR\([^)]*"NS::[A-Za-z0-9_]+"' runtime/src/object/deleting_dtor*.cpp
sed -n '2502,2516p' standalone/rt_data_mint_tables.inc   # Goal's kVSlots63, to find GetInstanceType's slot index

# retail exports (against the GOG install's retail DLL, not loaded at runtime per CLAUDE.md)
python3 -c "import sys; sys.path.insert(0,'tools'); from pe_exports import exports; \
  ex = exports(r'C:\GOG Games\Banished\Runtime-x64.dll'); \
  [... split each name by '@', toks[1]=class, toks[2]=namespace ...]"

# census / manifest
python tools/runtime_census.py            # read-only; grepped its namespace/gap tables
python tools/cutover_manifest.py --table   # read-only

# caller surfaces
python3 <script over engine/funcs/engine_funcs.cpp: regex ResolveExport("...") per line,
  nearest preceding "fn_X =" line (0..3 lines back), namespace = mangled.split('@')[2]>
grep -rn 'ResolveExport("[^"]*@NS@@' code/Game engine standalone runtime/src
grep -rc 'RT_EXPORT_ONCE([^)]*@NS@@' / 'RT_DATA_SLOT([^)]*@NS@@' runtime/src code/ standalone/ engine/
python tools/dispatch_slot_map.py                                 # dumps the 833-slot exe import table
grep -rnoE 'ResolveAddr\(Module::Exe, *0x[0-9a-fA-F]+' code/Game engine standalone
grep -rnoE 'RtAddr<[^>]*>\(0x[0-9a-fA-F]+' / 'ResolveAddr\(Module::Runtime, *0x[0-9a-fA-F]+' code/Game engine standalone
# (both dispatch-slot and by-address sets intersected against the above via a Python dict-address match)
grep -rEo '\(\*\(void\*\*\*\)[A-Za-z0-9_.>\-]+\)\[[0-9]+\]' code/Game/{Goal,Player,Component,Game,Tutorial}
grep -rl "NS::" tests/unit --include=*.cpp --include=*.h

# hidden retslots
python3 <script over standalone/rt_export_addrs.inc: regex '\?A[VU]' in the mangled name,
  namespace = toks[2], then checked '@@([QU])E[AB]A' for virtual (U) vs non-virtual (Q)>

# special shapes
grep -rn "MenuDLT\|Stub_Shutdown" code/Game engine standalone runtime docs
sed -n '1,20p' standalone/{splash_loop_stubs,menudialogloopt_stubs,loader_loop_stubs}.cpp
sed -n '1,40p;215,245p' dx11/src/devicedata.cpp
grep -n "CreateThread\|_beginthread" runtime/src/audio/*.cpp
grep -rn "printf(.*Register\|TRACE(.*Register" runtime/src/<each ns>
ls engine/types/ | grep -iE "engine|save|achievement|audio|input|pathfind|picking|application"
```

A note on tooling: an early pass of the caller-surface script under-counted (returned 0 calls for
every `fn_*` door) because a Python `subprocess` call to `grep` used `\(`/`\)` for a literal
parenthesis — correct for extended regex (`-E`) but WRONG for grep's default basic regex, where
`\(` starts a capture group instead. Caught by spot-checking `fn_KeyMap_Pressed` against a plain
`grep -rho` run in the shell (15 hits) before trusting the script's output (0 hits); fixed by using
literal `(` instead of `\(` in the BRE pattern. Recorded here so a step-7 planner reusing this
script does not repeat it.
