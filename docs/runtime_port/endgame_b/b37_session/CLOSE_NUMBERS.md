# B.3.7 CLOSE NUMBERS — the step's measurements, for the STATUS block

Measured by `b37-close` on its own branch at the end of the closing pass: the tree that carries
every class block (B1–B6), the fragment (F), every caller batch (K1–K5) and this pass's commits
1–4 and 6. "Start of step" is `951fd29b` (the launch of B.3 step 7, block 0's commit), measured the
same way in a throwaway `git worktree` at that commit, then removed. The scripts are in
`C:/Users/USPC/AppData/Local/Temp/claude/b37-close/` (`numbers37.py`, `initwalk.py` + `gsweep.py` +
`sites.py` + `localbind.py` — 0X's census scripts, parameterised by root — `hexall.py`,
`classes37.py`, `doors37.py`). Units are the ones `b36_session/CLOSE_NUMBERS.md` used; every number
says what it counts.

---

## 1. `reinterpret_cast` — LINE counts per directory (the B.4 metric)

`grep -c` semantics (LINES containing the token, summed) over `*.cpp *.h *.inc`, `build*/` and
`obj/` excluded. A conversion that bridges a `void*` or POD receiver onto a real class spells it
`reinterpret_cast`, so this RISES during B.3 and falls only in B.4.

| directory | start of step (`951fd29b`) | now | delta |
|---|---:|---:|---:|
| `code` | 25,282 | **25,543** | +261 |
| `runtime` | 6,885 | **6,852** | −33 |
| `standalone` | 2,907 | **2,940** | +33 |
| `engine` | 609 | **516** | −93 |
| `dx11` | 168 | **173** | +5 |
| `tests` | 374 | **425** | +51 |
| **TOTAL** | **36,225** | **36,449** | **+224** |

`code` rose with the caller batches' bridges (268, §2). `engine` FELL 93: commit 1 deleted 74
`engine_funcs` door globals and one data global, each with a `reinterpret_cast<fn_X_t>(…)` resolver
line (plus their exe-slot fallbacks); that is most of the 93. `runtime` fell because the class blocks replaced raw-offset
casts with member access in their own bodies. `tests` rose with the seven new ABI test files.

## 2. `// B.3.7 bridge` — LINE counts per directory

| directory | lines |
|---|---:|
| `code` | 268 |
| `runtime` | 141 |
| `standalone` | 17 |
| `engine` | 0 |
| `dx11` | 5 |
| `tests` | 2 |
| **TOTAL** | **433** |

Start of step: 0 (the marker did not exist). This pass added 6 in `runtime` and `code`: the three
Save door argument casts B2 left unmarked (`saveload.cpp` `SetMinOperationTime`/`Update`,
`saveload_loadmeta.cpp` `LoadMetaData`), the `Audio::Configuration` dtor call in `Engine::Shutdown`,
the `PickSetXfMember` trampoline and the `GetAchievementCount` local in `dialog_exe_helpers.cpp`.
(`// B.3.6 bridge`: 2,083 at start, 2,082 now — net −1 (runtime −2, standalone +1) where step-7 edits rewrote step-6 bridge lines.)

## 3. The caller surface

### 3a. `engine_funcs` globals bound to the eight namespaces

Found BY INITIALISER (`initwalk.py`: positional statement walk over `engine_funcs.cpp`;
`ResolveExport` literals joined across wraps, back-references demangled; `ResolveAddr(Module::Exe,
slot)` through the PE import table; `ResolveAddr(Module::Runtime, a)` through
`rt_export_addrs.inc`; the two-statement `void** d = ResolveAddr(..); if (d) fn_X = *d;` form).

| | start of step | now |
|---|---:|---:|
| globals bound to the eight namespaces | **92** | **17** |
| — door (function) pointers | 82 (Engine 15, Save 12, Achievement 16, Audio 16, Input 20, Application 3) | **8** (Engine 4, Save 2, Achievement 1, Audio 1) |
| — data globals (singleton storage) | 10 | **9** |

The 8 doors left are ctor/dtor doors, each still called: `fn_DialogLoaderLoopT_Ctor` (7 calls),
`fn_LoaderLoop_Ctor` (7), `fn_UpdateLoop_Ctor` (4), `fn_SaveLoad_Dtor` (4), `fn_Goal_BaseCtor` (3),
`fn_AudioScene_Ctor` (2), `fn_SavedState_Ctor` (2), `fn_DialogLoopT_Ctor` (1). The data global that
went is `g_pEngineSingleton`, folded into `g_pMainLoopSingleton` (same symbol). **Zero bound globals
at zero references.**

### 3b. Door CALL sites through those globals

| unit | start of step | now |
|---|---:|---:|
| code-only CALLs tree-wide (`gsweep.py`, comments/strings stripped, `engine_funcs.h` inline bodies counted) | **233** | **30** |
| — of which in `code/Game` | 215 | **28** |
| raw `\bNAME(` occurrences in `code/Game` over the initialiser-derived list (the inventory's grep) | 215 | 28 |

Start's top doors: `fn_KeyMap_GetValue` 23, `fn_KeyMap_Pressed` 15, `fn_SaveLoad_GetOperation` 11,
`fn_Audio_Scene_Pause` 9, `fn_SaveLoad_GetSaveInfo` 9. All 30 calls left are the ctor/dtor doors
of 3a (the other two are `engine_funcs.h`'s `MenuDialogLoopT_Ctor`).

### 3c. Resolution sites naming the eight namespaces, by binding and directory

Unit: sites (`sites.py`: code-only, literals joined across wraps, each classified by its innermost
enclosing call; hex literals joined to `rt_export_addrs.inc` / the PE import table).

| binding | start (`951fd29b`) | now |
|---|---|---|
| `ResolveExport("…")` | **177**: engine 83, code/Game 49, runtime/src 27, standalone 18 | **49**: code/Game 18, engine 12, standalone 19 |
| `RT_EXPORT_ONCE(T, "…")` | **57** (runtime/src) | **13** (runtime/src) |
| `RT_DATA_SLOT(T, "…")` (static-storage reads) | 52 (runtime/src) | 53 (runtime/src) |
| other by-name atomic helpers (`AtomicExport`, `AtomicByName`) | 12 | 17 |
| exe import slot through `ResolveAddr(Exe)`/`RSlot`/`ResolveSlot`/`resolveIAT` | **47**: code/Game 28, engine 19 | **6**: engine 6 |
| Runtime hex through `ResolveAddr(Runtime)`/`RtAddr<>` | **21**: code/Game 11, runtime/src 7, engine 2, standalone 1 | **6**: code/Game 3, engine 2, standalone 1 |
| registration rows (`RegisterRuntimeExport`/`Addr`, `RegisterDispatchSlot`, `RT_DELDTOR`, promoted rows) | 287 literal + 265 hex + 17 exe-slot | 288 + 266 + 17 |

What the survivors are:
- `ResolveExport` in `code/Game` (18): ctor/dtor doors of the exe's by-value / heap holders
  (`Audio::Scene`, `Picking::Scene` ×3, `SaveLoad`, `SavedState`, `Engine` ×2, `UpdateLoop`,
  `AchievementInstance`, `Goal` ×2, `GoalInstance` ×2), two `?_typeInfo` reads, one
  `AchievementManager::_singleton` read, and `goal_exe_helpers.cpp`'s by-name install of
  `Goal::GetInstanceType` into the exe Goal tables (§12). `engine` (12): the 8 kept ctor/dtor doors
  of 3a and 4 singleton-storage fallbacks. `standalone` (19): `RegisterDispatchSlot(slot,
  ResolveExport(…))` rows and storage reads — never callers.
- `RT_EXPORT_ONCE` (13): four `??_7` vtable reads in `runtime/src/achievement`, and the ctor/dtor
  doors `Engine::Startup`/`Shutdown` and `Ctor_AudioDevice` call by name (`Application`,
  `Audio`/`Input` `Configuration` and `Device` ctors; the `Input::Configuration`, `Audio::Stream`
  and `Application` dtors — undefined or polymorphic, B37_RULES 7).
- exe import slots (6): the singleton-storage exrefs in `engine_funcs.cpp` (carry-over 5).
- Runtime hex (6): `game_main.cpp` `MainLoop` ctor/dtor, `scenario.cpp` `Achievement` ctor (the
  Scenario base, B7-13), `engine_funcs.cpp`'s `Audio::Configuration::_singleton` and
  `LoaderLoop::_loader` storage, `address_registry.cpp`'s `Application::_singleton` registration.

**The whole-tree hex-literal sweep** (`hexall.py`, comments NOT stripped; B36_RULES 14.1): live
`resolve` lines onto the eight namespaces **66 → 12** (code/Game 37 → 3, engine 21 → 8, runtime/src
7 → 0, standalone 1 → 1); header address constants 69 → 39 (the 27 `MainLoopAddr` of
`engine/types/MainLoop.h` gone; `RtEngine::MainLoopAddr` 27 and `EngineAddr` 11 stay as
documentation, `modal_driver.cpp`'s storage constant 1). Block 0's "66 by-address resolve LINES"
is reproduced exactly at the start commit.

### 3d. Hand-inlined vtable calls onto these classes

| unit | start of step | now |
|---|---:|---:|
| proved `(*(void***)p)[N]` calls onto a step-7 class (CALLER_SURFACE 4) | **2** (`UpdateLoop` slot 7 `SaveRestore`: `moddialog.cpp:733`, `mod_dialog_vtables.cpp:440`) | **0** — both are `Engine::UpdateLoop::SaveRestore` virtual calls |

The own-slot-0 calls inside Goal/Scenario exe slot bodies (`EXE_SUBCLASSES.md` §3D) are exe-phase
(step 8) and are not counted here.

## 4. `tools/runtime_census.py --namespace <NS>`

Verbatim rows (`namespace total called ported detour called-covered ceiling`):

```
start (951fd29b):
Engine              108       40       34        0   85.0%   92.5%
Save                 37       29       29        0  100.0%
Achievement         118       50       43        0   86.0%   98.0%
Audio               137       40       33        0   82.5%   95.0%
Input                63       29       27        0   93.1%   93.1%  AT CEILING
Pathfinding          45        2        0        0    0.0%
Picking              49       11        9        0   81.8%
Application          26       13       12        0   92.3%   92.3%  AT CEILING
now:
Engine              108       19       16        0   84.2%   84.2%  AT CEILING
Save                 37        7        7        0  100.0%
Achievement         118       18       11        0   61.1%   94.4%
Audio               137       13        6        0   46.2%   84.6%
Input                63        5        3        0   60.0%   60.0%  AT CEILING
Pathfinding          45        2        0        0    0.0%
Picking              49        9        7        0   77.8%
Application          26        3        2        0   66.7%   66.7%  AT CEILING
```

`called` counts the functions the tree RESOLVES (by name or address). A member call resolves
nothing, so converting callers REMOVES rows from `called`: 138 rows left the board across the
eight namespaces (214 → 76) (Input 29 → 5, Save 29 → 7, Achievement 50 → 18 …) because nothing resolves them
any more, not because anything was unported. The census measures the door surface; it is not a
coverage measure for a namespace whose callers became C++.

## 5. Unit tests

| | start of step | now |
|---|---:|---:|
| `UNIT_TEST(` registrations (the runner's own count agrees now: `516 tests, 0 failures`) | **462** | **516** |
| test files in `tests/unit` with a registration | 38 | 45 |

## 6. The class list — from the headers, cross-checked against retail

`classes37.py` parses every `runtime/types/{engine,save,achievement,audio,input,pathfinding,picking,
application}/*.h`; a class counts when it opens directly inside one of the eight namespaces.
Parent and `sizeof` are the header's (`class X : public Y`, `static_assert(sizeof(X) == …)`); slot
count is the retail vtable length from `B37_SLOTS.md` §1 (sizes corrected by §1a; the four ROOT
parents per B37_RULES §2.1). **14 of 14 polymorphic retail classes agree with `B37_SLOTS.md` on
parent AND sizeof.** At the start commit the same parse finds **0** classes in the eight
namespaces: every header held `RtNs::` POD mirrors.

| class | parent | slots | sizeof |
|---|---|---:|---:|
| `Achievement::Achievement` | Object::External | 9 | 0x58 |
| `Achievement::AchievementInstance` | Object::Internal | 4 | 0x28 |
| `Achievement::Goal` | Object::External | 10 | 0x30 |
| `Achievement::GoalInstance` | Object::Internal | 7 | 0x20 |
| `Audio::SoundBuffer` | Object::CompiledExternal | 9 | 0x80 |
| `Audio::SoundEffect` | Object::External | 9 | 0x40 |
| `Audio::Stream` | System::Thread | 2 | 0x88 |
| `Engine::UpdateLoop` | (root) | 12 | 0x28 |
| `Engine::LoaderLoop` | UpdateLoop | 12 | 0x48 |
| `Engine::DialogLoopT` | UpdateLoop | 13 | 0x168 |
| `Engine::DialogLoaderLoopT` | LoaderLoop | 13 | 0x188 |
| `Engine::Loader` | System::Thread | 2 | 0x38 |
| `Pathfinding::IPathInfo` | (root) | 2 | 0x8 |
| `Pathfinding::PointList` | Object::External | 9 | 0x50 |
| `Picking::PickingMesh` | Object::External | 9 | 0x48 |

(`Engine::Loader`'s §1 heading prints no size, so the automatic cross-check covers the other 14;
Loader's 0x38 is §1a's and equals the header's.) Non-polymorphic, 17: `Achievement::AchievementManager` 0x48, `GoalList` 0x10,
`Application::Application` 0x30, `Audio::Configuration` 0x10, `Audio::Device` 0x1298, `Audio::Scene`
0x38, `Engine::Engine` 0x6f8, `Engine::MainLoop` 0xa0, `Input::Binding` 0x18, `Input::BindingNode`
0x38, `Input::Configuration` 0x10, `Input::Device` 0x1200, `Input::KeyMap` 0x18,
`Pathfinding::Scene` 0x90, `Picking::Scene` 0x60, `Save::SaveLoad` 0x90, `Save::SavedState` 0x10 —
each equal to B37_RULES §2.3's retail size (as corrected by §10) wherever §2.3 lists the class. Nineteen nested value types (`Audio::Device::
AudioChannel`/`PlatformChannel`, `Audio::Scene::Channel`/`EmitterInstance`/`AttenutedEffect`,
`Save::SaveLoad::SaveInfo`/`ModInfo`, `Picking::Scene::Cell`/`Entry`/`Link`/`AddDesc`,
`Picking::PickingMesh::SubMesh`/`Triangle`/`UV16`, `Pathfinding::PointList::Point`,
`Input::Device::State`, `Audio::SoundEffect::Sample`/`Variant`, `Audio::Stream::Block`).

## 7. Declared but NOT defined (the unported-member ceiling)

Class scope by brace depth; a declaration with an inline body, `= 0`, `= default` or `= delete` is
not counted; "defined" = a `[NS::]Class::member(…) {` definition under `runtime/src` (namespace from
the spelling or the enclosing `namespace NS {`). **477 member declarations across 30 classes; 228
have no definition.** By family:

| family | undefined | why |
|---|---:|---|
| `operator new` / `delete` / `operator=` | 52 | allocator doors / unexercised assignment, deferred on purpose |
| factory statics, `TypeId`, `_TypeName`/`_ParentTypeName` | 34 | registered factory tables (B37_RULES 7) or literals recorded, not used (B7-2) |
| constructors incl. copy ctors | 29 | no real ctor on a polymorphic class; the singleton ctors are `singleton_ctors.cpp` doors (B37_RULES 2.4) |
| destructors | 8 | `~Engine`, `~Application`, `Input::~Configuration`, `~KeyMap`, `Audio::~Device` … — doors (B37_RULES 7) |
| **other members — the honest ceiling** | **105** | exported, declared, not ported (27 carry an explicit `// not ported` marker) |

The 105 by class: `Pathfinding::Scene` 15 (dead in retail, B7-9: declarations only), `Engine::MainLoop`
12 (the retslot `Time` getters and the post-callback API: no caller), `Audio::Device` 11,
`Input::Device` 10, `Input::Configuration` 9, `Picking::Scene` 9, `Engine::Engine` 6,
`Achievement::AchievementManager` 5, `Audio::Scene` 5, `Audio::Stream` 5, `Achievement::GoalList` 4,
`Audio::SoundBuffer` 3, `Input::KeyMap` 3, `Audio::Configuration` 2, `Audio::SoundEffect` 2,
`Engine::LoaderLoop` 2, `Achievement::GoalInstance` 1, `Picking::PickingMesh` 1.

CAVEATS: the matcher compares member NAMES, so an overload defined under one signature counts its
sibling as defined; a body generated by a macro counts as undefined. **228 / 105 are ceilings.**

## 8. Doors kept by design

Distinct mangled names registered by `RegisterRuntimeExport` under `runtime/src` whose OWNER scope
(back-references demangled) is one of the eight namespaces (`doors37.py`):

| family | start | now |
|---|---:|---:|
| member doors — now each a one-line QUALIFIED delegation to a C++ member | 237 | 238 |
| dtors `??1` | 24 | 24 |
| ctors `??0` incl. copy ctors | 22 | 22 |
| `operator=` `??4` | 1 | 1 |
| **total** | **284** | **285** |

Per namespace now: Engine 50, Save 29, Achievement 63, Audio 56, Input 34, Pathfinding 9, Picking
18, Application 26. Factory statics are registered through the `rt_factories_*.inc` tables, not
`RegisterRuntimeExport`. Retired only at the END of B.3: **16** mint vtable rows (15 named `??_7` +
the unnamed 0x180157c60 `PlatformChannel` row) and **8** `?_typeInfo` rows in
`standalone/rt_data_mint_tables.inc`; **12** `RT_DELDTOR` rows naming a step-7 class; **9**
`CUTOVER_REQUIRED` singleton-storage manifest rows (`AchievementManager`, `Application`,
`Audio::Configuration`/`Device`, `LoaderLoop::_loader`, `Engine`, `MainLoop`,
`Input::Configuration`/`Device`) plus `?_singleton@SaveLoad@Save@@`, read through `RT_DATA_SLOT` and
with NO manifest row; 36 `RT_DATA_SLOT` reads of that storage under `runtime/src`.

## 9. Gate, at the close

Per commit (1, 2, 3, 4, 6): `standalone-debug` 0, `standalone-perf` 0, `unit` 0 (516 / 0 failures),
`dispatch_slot_map --check` 0 (109 registrations, 0 mismatches), `cutover_manifest --gate` OK (59
rows; no COSMETIC warning at any commit, and `--inc` regenerates the file byte-identically, so
commit 5 had nothing to do), `rt_mint_gen --check` UP TO DATE (152 vtables, 104 BaseTypeInfo, 48
storage), no new warning in any changed `.cpp` under either flag set (base compiled in its own
worktree). No game run: the coordinator runs the step close.
