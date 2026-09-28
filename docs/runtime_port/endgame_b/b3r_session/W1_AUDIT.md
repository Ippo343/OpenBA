# W1 audit — the registry teardown, wave 1 (independent, read-only)

Audited 2026-09-28. Trees: **537f8c39** (end of W0, the "before" of every W1 conversion),
**ac33b9c1** (end of W1: the 14 reader blocks and the loader-line fix `85a40178`), and **fef215fe**
(main's tip when the table was cut; `b3r-Leftovers` merged DURING the audit). Parts B-E read the W1
diff `537f8c39..ac33b9c1` (W0-P0c and W0-A0 are inside that range; they were read too). Part A's
table and part F's numbers are on fef215fe; part A was also cut on ac33b9c1 (counts in §2).
Table: `data/W1_audit.tsv`. Script: `scripts/W1_audit.py` (repo root, no arguments; reproduces the
table, part B and part F). Scratch scripts for parts C-E: `<scratchpad>/b3r/Audit/` (named per part
below; not committed — they read `git archive` copies of the three trees).

## 1. Findings

### 1.1 DEFECT (part B: a deleted row that something still reads) — none
Rows registered at 537f8c39 and not at the tip: **1,203 names, 1,433 runtime addresses, 22 exe
addresses**. Readers found for them on the tip, by binding (method §3): **0**. The only hits are
not readers: the 7 `RtPortLookupExport("…")` fills of `standalone/reg_rt_mirror16.cpp` (below) and
one mangled name inside the text of a `static_assert` string (`runtime/types/ui/DialogInstance.h:325`).

`reg_rt_mirror16.cpp` now registers **nothing**: its 7 address rows are filled from name rows that
RT-UI / RT-VG deleted, so `RtPortLookupExport` returns null and `RegisterRuntimeAddr` drops the
null. No code reads any of the 7 addresses (0x180002470 0x1800031f0 0x180035020 0x1800d4bb0
0x1800fdb20 0x18011cbe0 0x18011d050). This is why the static count is 104 addresses and the boot
line says 97. The file is W2-RT's; it is dead, not defective.

### 1.2 Wrong PORT GAP arms and untitled behaviour changes (parts D, E)
| # | where | commit (block) | what | effect |
|---|---|---|---|---|
| **E1 DEFECT** | `runtime/src/input/keymap_save.cpp:324` `RtInput_KeyMap_DeletingDtor` | `18c4c429` (RT-Core) | A `// PORT GAP 0x180006a10` null arm replaced `guardedFree(selfPtr)`. The marker says "no body is bound at this address". **False**: at 537f8c39 (and still on the tip) `context_lifecycle.cpp` registers `0x180006a10 -> Rt_GuardedObjectFree` inside `RtVideoContextLifecycleRegister()`, called unconditionally from `RtVideoPortInit()`. The resolve returned our body; the KeyMap object WAS freed. | Behaviour change, untitled: the KeyMap deleting destructor no longer frees its object. Reached from `Engine::Engine::Shutdown` (`engine_lifecycle.cpp:766`, every clean exit) and `Input::Configuration::Startup`'s re-entry arm (`input_config_startup.cpp:165`). No gate route can see a missing free at exit. Fix: `Rt_GuardedObjectFree(selfPtr)` (the body `runtime/src/save/closure_batch2b.cpp:54` already calls by name). |
| **E2** | `runtime/src/engine/closure_batch2b.cpp:78` `RtEngine_Application_ScalarDeletingDtor` | `70747bfb` (RT-Core) | Same false premise: `GuardedFree(self)` (an unguarded `fn(p)` on the same resolve) became a no-call PORT GAP arm. | None at run time: the body has no caller (its 0x18004da00 row had no reader and was deleted). The comment is wrong and the arm drops retail's free; correct it or delete the body at the close. |
| D1 | `code/Game/Game/gameupdateloop.cpp` (4 sites) | `7fa3ee32` (G-Game) | `fn_CitizenList_Dtor / fn_EntityUI_Dtor / fn_Weather_Dtor / fn_ImageCapture_Dtor(p, nullptr, nullptr, nullptr)` became one-argument direct calls. | No value reaches a body (each body declares one parameter). But R2 says an argument list that disagrees with the body is settled from retail in a titled commit; this was done silently. |
| D2 | `code/Game/Player/edittool.cpp` 4 sites | G-Player | A local bound to import slot 0x140152a98 (the RAW `IsDerivedFrom`) became `Object::IsDerivedFromNullSafe`. | None: every site tests the pointer non-null first. |
| D3 | `code/Game/Component/citizen_assignworkplace.cpp:183` | G-CompA | The 0x140002280 null arm now evaluates `AudioScene()` (the old null path skipped the whole block). | None: `AudioScene()` is a pure read of `*g_pGameWorldSingleton + 0x78`. |
| D4 | titles | G-Work ×4, G-Game `e200bbb6`, RT-UI `638c4b3a`, W0-A0 `33d73c9a` | The behaviour commits say `behaviour:` inside the title (`B3R G-Work behaviour: …`), not at its start. Only `423dd876` starts with it. | Formality; a `git log --grep '^behaviour:'` finds one of eight. |

The five `behaviour:` value changes of §5 of the rule sheet (zone refresh ×3, ConsumeRawMaterial,
market cleanup, cemetery enqueue, ToInteger / memmove, the allocator seat) are titled and are not
repeated here. Documented width / sign casts (`known_port_issues` 1, 2, 5, 8) keep the door's bits and
are not counted as changes.

### 1.3 MISSED (a reader or dead reader W1 should have converted or deleted)
Door lines (part A) MISSED: **0** on ac33b9c1 (83 lines were LEFTOVER, all on the Leftovers list) and
**0** on fef215fe. What the door sweep does not see, and was missed:
| # | owner | what |
|---|---|---|
| M1 | **G-Work** | R3 dead readers not deleted: 113 unused `XAddr` constants in 23 headers (`code/Game/Work/{AutoPickupWork,BuildRoad,BuildStructure,ClearArea,ClearStorage,ConsumeProduce,FireWork,FishingWork,ForesterWork,GatherRawMaterials,HarvestArea,HarvestOrchard,HerbalistWork,HuntWork,LocationWork,MarketWork,PastureWork,Pickup,PlantArea,PlantOrchard,TendArea,TradeWork,Work}.h`). One is live: `BuildRoadAddr::CtorParams` (a W2 row key). |
| M2 | **G-Player** | same, 12 constants in 10 headers (`code/Game/Player/{BridgeTool,DragPlacementTool,dragtool,EditTool,FencedTool,PathTool,PlacementTool,RemovalTool,RoadTool,Tool}.h`). |
| M3 | (no W1 owner; W2-EZ) | same, 138 constants in `engine/types/ui/Panels.h` (38 namespaces) and `Dialogs.h` (22). R3 counted 262 of 264 unused; 265 remain on the tip. |
| M4 | **G-CompB** | `AmbientEnvironmentComponent_InitFuncs()` (`ambientenvironmentcomponent.cpp:273`, declared in `_funcs.h:20`) has no caller; it only calls a type-id latch. A caller-less `_InitFuncs` definition (R2 "cache dies with its reader"). |
| M5 | G-CompB/C, RT-* | R3 says a body that loses its last reference is not deleted in W1 unless `static`. Deleted anyway (external linkage, no caller at 537f8c39): real transcriptions `NomadComp_RebuildController_b60` (128 lines) and `NomadComponent_OnCreate` (`nomadcomponent.cpp`), `StorageLocationComponent_OnCreate`; plus ~175 external C-door delegations in `runtime/` (e.g. `RtIo_PackageFile_GetName`, `RtUiDialogInstance_AddController6`, `RtGraphics_EdgeMesh_SetMaterials`) whose only reference was their registration. No run-time effect; provenance for B.4 is gone from the tree (history has it). |

### 1.4 LOST side effects of deleted functions (part C)
| function (file, 537f8c39) | non-registration statement | now |
|---|---|---|
| `RtObjectExternalLoad/Add/SaveTextRegister` (`external_load.cpp`, `external_add.cpp`, `external_savetext_win.cpp`) | `std::atexit(Dump*Counters)` — the `[ExtLoad]` / `[ExtAdd]` / `[ExtSaveText]` gate lines | restored in `85a40178` by `RtObjectExternalLoadInstallCounters()`, called from `RtObjectPortInit()`. The three hooks are now installed in one place in the order SaveText, Add, Load, so they PRINT in the order Load, Add, SaveText (cosmetic). |
| `RtObjectExternalFindRegister` (`external_find.cpp`) | `std::atexit(&DumpFindCounters)` | **LOST** (GATE_TABLE row 21: read by nothing) |
| `RtObjectExternalPathRegister` (`external_path_win.cpp`) | `std::atexit(&DumpCounters)` | **LOST** (same) |
| `RtEngineLifecycleRegister` (`engine_lifecycle.cpp`) | `if (!RtLaneXEnsureAllocatorPlatform()) { printf(REFUSING); return 0; }` — installs the allocator platform seam `g_plat` when absent | survives: `RtSystemAllocatorRegister()` (called earlier from `RtSystemPortInit`) installs it first. `RtLaneXEnsureAllocatorPlatform` now has NO caller (W2 hazard H1). |
| `coastline_EnsureInit`, `rg_EnsureInit` (worldgen) | `fprintf("[worldgen] coastline resolves …" / "ranges resolves …")` | LOST (diagnostic; no tool reads it) |
| `ResolveInstFuncs` (`tornado.cpp`), `ResolveFuncs` (`minimap_paint.cpp`), `TornadoComponent_InitFuncs`, `HarvestOrchard_ResolveFuncs`, `ForesterWork_ResolveFuncs`, `*_ResolveFuncs` `_DEBUG` dumps, `gameupdateloop` `CHECK_FN` | failure-only / debug prints (`[Tornado/particle] … UNRESOLVED`, `[MinimapPaint] primitives unresolved`, `MISSING:` lines, `HO_MISS`) | LOST; each fired only when a term was null, and every term was registered (§1.5) |
| `RtObject_InternalFactory_ClearPersistState` (+1 more) | `printf("[RtPort] FIRST LIVE CALL into our …")` once per process | LOST (diagnostic; no tool reads it) |
| `citizenui_sa / residenceui_sa / eventnotify_sa::EnsureGlobals` | `if (!g_pAgeMultiplier) g_pAgeMultiplier = &s_ageMul;` (1.0f), `if (!g_pNotifyTimerDuration) … = &s_notifyTimer` (30.0f) — writes to engine_funcs globals | never ran: 0x140172198 / 0x1401721cc are registered (`reg_citizenai_actionctors.cpp:72`, `reg_unreg_lane4.cpp:49`) in `AddressRegistryInit()`, which runs before `EngineFuncsInit()` binds the globals. Nothing lost. |
| `ResolveDragToolFuncs` (`dragtool.cpp`) | `if (!g_fn_PlayTickSound) g_fn_PlayTickSound = &DragNoop_Sound;` ×5 | never ran: the five addresses are registered (`reg_player_drag.cpp:41-44`, `reg_player_toolui.cpp:63`). |
| `RtLanePlatInputFormatProbe` (`input_platform_startup_win.cpp`) | the DirectInput format probe | had no caller at 537f8c39 |
| `Ctor_InternalFactory` & the 14 other `singleton_ctors.cpp` bodies | `W64` zeroing + `Publish(...)` | NOT lost: moved into `namespace rtengine`, called by name from `engine_lifecycle.cpp` |

**W2 hazards** (register / port-init functions still on the tip that do more than register — W2
must move these statements before deleting the function, or row 21 repeats):
H1 `RtSystemAllocatorRegister()` (`allocator.cpp:1195`): installs the allocator platform seam
(`RtSystemAllocatorInstallPlatform`), calls the cutover armer, prints `[RtAlloc] ARMED …` / ATTEST
(`tools/stress_run.sh` collects it); its twin installer `RtLaneXEnsureAllocatorPlatform` has no caller.
H2 `RtObjectPortInit()`: `RtObjectExternalLoadInstallCounters()` (the `[ExtLoad]` gate line).
H3 `RtMathPortInit()`: `RtLibm::Init()`. H4 `RtGraphicsPortInit()`: `RtGraphicsWave29SlotsSummary()`.
H5 `RegisterRdataFloatConsts()`: fills `g_storage[]` (the cells readers dereference until STORAGE).
H6 `RegisterTownWorkplaceRegistry()`: the `SeatShuffleOn()` test. H7 the three slot-cell wrappers
(`RegisterDispatchSlot` ×2, `RegisterLane3Slot`, `RegisterPlacePlayerSlot`) own the cell storage.
H8 `RegisterCodeFunctions` / `RegisterVtablesAndTypeIds` / `RegisterGameLeafBatch` are call lists of
other register functions. (`scratch w2hazard.py` lists all 14.)

### 1.5 Guards whose condition was not always true (part D)
Every deleted readiness guard with a resolvable binding was checked, not sampled: **3,972** guard
lines (all 14 blocks) → **3,579** bound to a key registered non-null at 537f8c39; **390** bound to a
null or ambiguous key; 3 lambda thunks (non-null). The 390, read by hand:
* 254 are `fn_IsDerivedFrom` (the null-safe trampoline, non-null whenever the name row exists; the
  inventory's binding string names two things and the script reads it as a key).
* 116 test `g_pMemAllocSingleton`, a SNAPSHOT that is null until `RunFactorySelfTest` refreshes it
  right after `Engine::Startup` (`game_main.cpp:437`). Every one of the 116 sites is game, menu, dialog,
  serializer or harness code that runs after that point; the guards were true on every route.
  (Blind spot: a code/Game body reached from inside `Engine::Startup` would have taken the null path.
  None was found; `minimap_paint.cpp`'s once-latched `s_ok` includes the snapshot and is painted
  in-game only.)
* the rest are the PORT GAP keys of §1.6 (each converted to its null arm), the dead nomad resolver
  (`nomadcomponent.cpp`, its functions were deleted uncalled), and the 0x140151ab8 DeletePtr slot,
  whose four readers had a live fallback to `fn_InternalBasePtr_DeletePtr` (bound by name, non-null):
  the member `DeletePtr` they now call is what ran.
No guard deleted on a live null path was found. Composite "ready" latches that went with their
resolvers (`tradeui_sa::DepsReady` 10 leaves, `destroyui/upgradeui_sa::Ready`, `ForesterWork`
`execReady`, `minimap_paint` 8 names + 3 globals, `w2_takescreenshot` 10 names, `tool_base_createbar`
4, `tornado.cpp` 3, dx11 `g_deps.ok` / `g_imOk`): every term was registered at 537f8c39.
The 68-row sample the brief asks for (≥4 per block, STUB / slot / CALLBACK keys first) is §5.

### 1.6 PORT GAP arms added in W1 (part E)
43 new marker lines (33 name a key; the rest are banner / cross-reference lines). Each key was checked unregistered at 537f8c39 (the premise of
R4) and the new statements compared with the old null path (`git diff 537f8c39 ac33b9c1`):
| key | file(s) | block | verdict |
|---|---|---|---|
| 0x140022480 | action_base_cnr.cpp | G-Action | same (no call) |
| 0x1401c22d0, 0x1401c23d0 | action_pf_geom.cpp, action_pf_road.cpp | G-Action | same (the fallback table was the code) |
| 0x140002280 | citizen_assignworkplace.cpp ×2 | G-CompA | same (a550 adds a pure `AudioScene()` read, D3) |
| 0x140152a70 | citizen_assignworkplace.cpp | G-CompA | same (`same = 1`) |
| 0x140002280 | firecomponent_ignite.cpp ×2 | G-CompB | same (both audio blocks skipped) |
| 0x14009f5d0 | growthcomponent.cpp | G-CompC | same: the address IS registered (`PlayerPath_CharComp_f5d0`), but this file's pointer was never assigned, so `placed = false` ran (known_port_issues 3) |
| 0x140172130, 0x140172274 | quarrycomponent_deform.cpp | G-CompD | same (0.85 / -0.25, the defaults `RF::Get` left) |
| 0x140002280 | tornadocomponent.cpp ×2 | G-CompD | same |
| 0x140002280 | debugui.cpp | G-Dialog | same (warn-once, geometry only) |
| 0x14009e8c0, 0x1400938d0, 0x140101ee0, 0x1401026a0 | gamegrid_serialize.cpp, gameupdateloop.cpp ×3 | G-Game | same (EntityUI::Startup's `if (!fn) EntityUI_OpenRegisteredDialogs_SA` arm kept; Update's `: 1`) |
| 0x1401513f8 | dragtool.cpp | G-Player | same (Box ctor skipped) |
| 0x1400fcc90 | edittool.cpp | G-Player | same |
| 0x140151ab8 | work_act_clear / path / perform ×2 / rawmat | G-Work | same (the member-DeletePtr fallback ran) |
| **0x180006a10** | **keymap_save.cpp, closure_batch2b.cpp** | **RT-Core** | **WRONG** (E1, E2): the key was registered |
| 0x1801557e8 | filesystem_createfile_win.cpp | RT-IO | same (null prefix) |
| 0x18007f8a8 | error.cpp | RT-Core | same (the fallback exit ran) |
Cross-check over ALL 68 keys named in any `PORT GAP 0x…` marker on the tip: only 0x14009f5d0
(explained) and 0x180006a10 were registered at 537f8c39.

## 2. Part A — the remaining doors, classified
`data/W1_audit.tsv`: 2,962 code lines on fef215fe (file, line, spelling, key, class, reason, code).
| class | fef215fe | ac33b9c1 | what |
|---|---:|---:|---|
| STORAGE | 1,048 | 1,048 | reads of `g_pGameWorldSingleton` (≈880), `g_pGameSettingsPtr`, `g_pKeyMapSingleton`, `g_pInputConfigSingletonExref`, `g_pGameSingleton` (townhallui ×25), `g_pNewGameSettings`, `g_pMemAllocSingleton` (the `main.cpp` self-test refresh: R7 behaviour commit 2 is still owed), and their `EngineFuncsInit` bindings |
| W2-ROW | 1,013 | 1,013 | registration lines (948 carry the reader count of their key) + 50 slot fills `RegisterDispatchSlot(slot, ResolveExport("…"))` + the 7 null mirror rows |
| W2-MACHINERY | 755 | 738 | `EngineFuncsInit` bindings whose global has no reader outside `engine/funcs` (717, 7 of them slot-cell reads), `resolve.h` / `resolve_standalone.cpp` / `rt_port.h` / `rt_resolve_cache.cpp`, the slot wrappers' own bodies |
| NOT-A-DOOR | 111 | 106 | `Ensure*` latches that fill type ids or do real work; `ArraySetSize` templates in audio / external_load; a local `const float g_fUVScale` |
| LEFTOVER | 0 | 83 | on the Leftovers list (g_fUVScale ×16, the 5 `_ResolveFuncs` stubs, road A*, EdgeMesh doors, exe_bodies.h inline doors, ArrayPush, g_pAgeMultiplier) — merged in fef215fe |
| INSTRUMENT | 23 | 23 | `redzone_alloc.cpp` ×19, `dlgctrl_route.cpp` ×3, `scene_watch.cpp` ×1 (R11) |
| TEST | 9 | 9 | `tests/unit` |
| STUB | 3 | 3 | `EngineFuncsInit`'s 0x140002280 binding (every reader is a PORT GAP arm) |
| MISSED | 0 | 0 | see §1.3 for what the door sweep cannot see |
Registry cross-check: static registered keys on the tip are 135 names / 104 addresses / 738 exe;
the boot lines say 135 / 97 / 737. The 7 are the null mirror rows; the 1 is 0x140089470, registered
only when `Opt().liveSeatShuffle`.

## 3. Part B — method
* Registered set per tree = the key argument of every `RegisterRuntimeExport` (literal, split
  literals, file `const char*` constant, `r.mangled` table loop), `RegisterRuntimeAddr` (literal,
  `r.addr` loop), `RegisterExeAddr` (literal, `BuildRoadAddr::CtorParams`, the `g_consts` loop), and
  the slot wrappers. At 537f8c39 this gives 1,338 / 1,537 exactly as the `[RtPort]` line of GATE row 4.
* For every key registered at 537f8c39 and not at the tip, the tip was swept (comments stripped,
  strings blanked for hex): the name as a whole or split string literal anywhere (also as a
  substring of a longer literal, then read); the address as a 9-digit hex literal with any suffix or
  case, including `constexpr`s, tables and macro arguments; every PE import slot the retail exe maps
  to a deleted name (439 slots); and `0x180000000 +` arithmetic. A registration's KEY argument is not
  a reader; its fill (`ResolveExport("…")`) is.
* Deleted exe rows: 22 — 21 by G-Game (the serializer / teardown / goals register functions,
  `gameupdateloop_serialize.cpp` ×8, `_teardown.cpp` ×4, `goals_serialize.cpp` ×4,
  `gamegrid_serialize.cpp` ×2, `terraingenerator.cpp` ×2, `citizenlist_serialize.cpp` ×1) and
  0x1400dd710 by G-Player — GATE rows 14-15 give the same numbers.

## 4. Part C — method
Every function defined at 537f8c39 and not at ac33b9c1 (by qualified name; 1,019), its body read
with comments stripped, then registration calls, resolves, `Ensure*`/`_ResolveFuncs` calls, counters,
latch flags, printf of counts and local prototypes removed; what is left (88 functions) was read by
hand (§1.4). A second pass kept `if (f())` conditions whose call is not a door (it found
`RtLaneXEnsureAllocatorPlatform`). A third pass listed the 102 deleted non-register functions that
had a live caller at 537f8c39; all are door delegations or diagnostics whose callers were converted.
Moves into a namespace (`rtengine::Ctor_*`) show up as deletions and were resolved by reading.
A diff of every `atexit`, `CreateThread`, `InitializeCriticalSection*`, `CreateEvent`, `TlsAlloc`,
`AddVectored*`, `SetUnhandledExceptionFilter`, `Publish(` and `Install*(` line between the two trees
found only the five `atexit` changes. Every log line the F inventory lists as read by a gate or tool
was looked for on the tip: only the ones retired on purpose (`[RtMint]`, the stub lines) are gone.

## 5. Part D — method and the sample
All 193 non-merge commits in `537f8c39..ac33b9c1`; eight have `behaviour` in the title (§1.2 D4).
* **Guards**: every removed line holding `if (X)`, `if (!X)`, `X && …`, `X ? X(…)` on a door-shaped
  variable; X's binding read from the 537f8c39 file (initialiser, slot-deref two lines up, alias of an
  `engine_funcs` global through inventory E); registration checked in the 537f8c39 key sets (a slot
  counts as non-null only if its fill's name row exists). 436 guard lines had no resolvable binding:
  394 variables, mostly latches (`s_resolved`, `s_init`); the door-shaped ones among them
  (`Flt(0x…)` floats, `s_fn_Time*`, `s_*MemAlloc`, the `IsDerived` slot locals,
  `fn_Random_RandFloatTol` — a function, always non-null) were read by hand: all registered.
* **IsDerivedFrom**: 40 hunks turned a slot-bound local into `->IsDerivedFrom` — all 40 locals were
  bound to slot 0x140152a98, whose fill is the RAW body, so the member call is faithful; 4 went the
  other way (D2). No `fn_IsDerivedFrom` (the null-safe trampoline) call became a raw member call.
* **Arguments**: hunks replacing ONE door call by ONE direct call with a different argument count
  (receiver and retslot allowed): 466 flagged by the script, read: the by-value retslot and member
  forms, the titled zone-refresh / region-path commits, and D1.
* **Bindings**: 1,410 added direct calls carry a `// 0x…` address comment; for 100 the name is not the
  body registered at that address at 537f8c39 — all read: member forms of the same C-door body, local
  wrappers around it, `.rdata` getters, and the documented double body 0x14012e5c0. 0 wrong bindings.
* **Constants**: all 62 `kRdata_*` in `exe_constants.h` were compared bit for bit with the cell
  registered at 537f8c39 (the `g_consts` loop or the `k_rdata_*` definition): 62 equal.

Sample (the brief's ≥60; `status` at 537f8c39; "NULL*" rows are explained in §1.5):
| block | guard | key | status |
|---|---|---|---|
| G-Action | `s_fn_022480` | 0x140022480 | NULL → PORT GAP arm |
| G-Action | `s_fn_Time` | slot 0x140152b00 ← `??0Time@System@@QEAA@M@Z` | REG |
| G-Action | `slotTypeSer` | slot 0x1401516b0 ← `?Serialize@Type@Object@@…` | REG |
| G-Action | `g_pMemAllocSingleton` | snapshot | NULL* (refreshed before use) |
| G-Action | `fn_IsDerivedFrom` | trampoline | NULL* (non-null) |
| G-CompA | `s_slot_Vec3Eq` | slot 0x140152a70 | NULL → PORT GAP arm |
| G-CompA | `s_fn_2280` | 0x140002280 | NULL → PORT GAP arm |
| G-CompA | `s_v3ctorScalar` | slot 0x140151320 ← `??0Vector3@Math@@QEAA@AEBVScalar@1@@Z` | REG |
| G-CompA | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-CompB | `s_findAmbient` | 0x140002280 | NULL → PORT GAP arm |
| G-CompB | `slot` (IsDerived) | slot 0x140152a98 ← `?IsDerivedFrom@Base@Object@@QEBA_NI@Z` | REG |
| G-CompB | `fn_MemFree` | lambda thunk | non-null |
| G-CompB | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-CompC | `slot` (CreateObject) | slot 0x140152778 | NULL (dead nomad resolver, deleted) |
| G-CompC | `slot` (IsA) | slot 0x140151410 ← `?IsA@Base@Object@@QEBA_NI@Z` | REG |
| G-CompC | `s_memAlloc` | slot 0x140152b60 ← `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z` | REG |
| G-CompC | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-CompD | `slot` ×3 | slots 0x140152a98, 0x140152b00, 0x1401514c0 (`??4String@System@@…`) | REG |
| G-CompD | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-Dialog | `slot` | slot 0x140152b00 | REG |
| G-Dialog | `g_pAgeMultiplier` / `g_pNotifyTimerDuration` | 0x140172198 / 0x1401721cc (.rdata) | REG |
| G-Dialog | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-Game | `fn_EntityUI_Startup`, `s_fn_RegionSystemBuild`, `fn_Weather_DrawUpdate` | 0x140101ee0, 0x14009e8c0, 0x1400938d0 | NULL → PORT GAP arms |
| G-Game | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| G-Player | `s_fn_Entity_impl_cc90` | 0x1400fcc90 | NULL → PORT GAP arm |
| G-Player | `s_fn_IsDerived` | slot 0x140152a98 | REG |
| G-Player | `fn_MemFree`, `g_pMemAllocSingleton`, `fn_IsDerivedFrom` | — | non-null / NULL* |
| G-Work | `s_wap_DeletePtr` | slot 0x140151ab8 (unfilled) | NULL → the fallback member DeletePtr ran and runs |
| G-Work | `s_war_MemAllocStatic` | slot 0x140152b60 | REG |
| G-Work | `s_wac_IsDerivedFrom`, `fn_MemFree`, `g_pMemAllocSingleton` | — | non-null / NULL* |
| RT-Core | `g_memAlloc`, `g_memFree`, `fn` ×3 | `?MemAlloc@…`, `?MemFree@…`, `?AsMatrix@Transform@Math@@…`, `?SetText@Error@System@@…`, 0x1800c7a50 | REG |
| RT-IO | `fn` ×4, `fnArrayDtor` | 0x1800c7a50, 0x1800c7ad0, 0x1800098d0, 0x180010b80, 0x18000f7d0 | REG |
| RT-UI | `fn` | 0x1800c7a50 | REG |
| RT-VG | `fn` ×5 | `?NextGroup@Random@System@@…`, `?IsDerivedFrom@…`, 0x1800c7a50, 0x18001f470, 0x1800c7ad0 | REG |
| S-Harness | `slot`, `fn_Frustum_MulTransform3`, `fn_ExternalList_GetResourceCount` | slot 0x140151460 ← `?GetTransform@Geometry@Graphics@@…`, `??DFrustum@Volume@@…`, `?GetResourceCount@ExternalList@Object@@…` | REG |
| S-Harness | `g_pMemAllocSingleton` / `fn_IsDerivedFrom` | — | NULL* |
| DX | `fn` | `?MemFree@MemoryAllocator@System@@SAXPEAX@Z` | REG |
(68 rows in the scratch `sampleD.tsv`; the table above folds the repeated snapshot / trampoline rows.)
RT-UI, RT-IO and DX removed few guards (10, 15, 1): their doors were unguarded `RtAddr<>(…)(…)`
calls, whose keys part B / the null-key sweep covered instead (every address literal read at
537f8c39 was checked against the key set: the unregistered ones are the PORT GAP keys of §1.6, the
two ToInteger / memmove addresses of `638c4b3a`, unused constants (the `XAddr` namespaces, seven
never-read mangled-name constants in `type_system.cpp`, `dialog.cpp`, `dialog_vtable.cpp`,
`userinterface_draw.cpp`), dx11's own IAT addresses (each table row resolves by name first, and every
name was registered), the uncalled DirectInput probe, and the launcher-DLL offsets of
`engine_platform_win.cpp`).

## 6. Part F — numbers for the close (code lines; 65b800d4 → fef215fe)
| measure | 65b800d4 | tip | per directory |
|---|---:|---:|---|
| `RegisterRuntimeExport(` | 1,258 | 139 | runtime 1,254→136, standalone 3→2, tests 1→1 |
| `RegisterRuntimeAddr(` (space-tolerant) | 1,453 | 109 | runtime 1,449→99, standalone 3→9, tests 1→1 |
| `RegisterExeAddr(` | 696 | 674 | code 63→41, standalone 633→633 |
| `ResolveAddr(Module::Exe` | 2,185 | 197 | code 1,967→0, runtime 1→0, standalone 22→2, engine 195→195 |
| `ResolveAddr(Module::Runtime` | 275 | 3 | code 3→0, runtime 265→0, standalone 5→2, engine 2→1 |
| `ResolveExport(` | 205 | 79 | code 83→0, runtime 36→3, standalone 49→45, engine 31→31, dx11 5→0, tests 1→0 |
| `RT_EXPORT_ONCE` | 242 | 1 | runtime (the macro in `rt_port.h`) |
| `RtAddr<` | 430 | 0 | |
| `X_ResolveFuncs` / `_InitFuncs` lines | 354 (89 defs, 74 decls, 191 calls) | 2 (1 def, 1 decl, 0 calls: M4) | code |
| `engine_funcs` externs | 227 | 227 | engine (W2-EZ) |
| `engine_funcs` use lines outside `engine/funcs` | 4,279 | 1,055 | code 4,153→975, standalone 126→80 |
| `reinterpret_cast` | 29,064 | 27,433 | code 20,887→19,600, runtime 5,028→4,766, standalone 2,190→2,170, engine 259→238, dx11 168→134, tests 532→525 |
| `// B.3.x bridge` markers (raw lines) | 6,698 | 6,863 | code 5,646→5,718, runtime 874→970, standalone 105→111, engine 11→2 |
| `PORT GAP` markers (raw lines) | 149 | 191 | code 147→185, runtime 0→4 |
| `STANDALONE DEVIATION` markers | 49 | 56 | runtime 0→6, standalone 0→1 (all W0-A0's seat and `IsDerivedFromNullSafe`) |
| `standalone/reg_*.cpp` files | 127 | 128 | (+`reg_rt_mirror16.cpp`) |
| `address_registry.cpp` lines | 1,243 | 1,243 | |

## 7. Blind spots
* **Runs.** Nothing here was built or run. A deleted row with a reader prints nothing; neither does a
  missing free (E1). Both classes were searched statically only.
* **Computed keys.** A key built at run time (`base + offset` other than `0x180000000 +`, a name
  assembled from pieces not adjacent as literals, an address read from data) is invisible to the
  literal sweeps. `0x180000000` arithmetic occurs only in `engine_platform_win.cpp` (the DLL-handle
  launcher path, never taken). Table loops were expanded only for the `{0x18…, "…"}` shape.
* **Door-shaped variables without a binding.** 436 deleted guard lines (§5) had no binding the
  script could read; the door-shaped ones were read by hand, the latches were not.
* **Registration order.** "Registered" means statically registered by a register function that is
  called. A row registered AFTER `EngineFuncsInit` binds its global would still read null; checked
  for the globals in §1.4-1.5 (AddressRegistryInit precedes EngineFuncsInit in `main.cpp`), not for
  every row. Rows behind a flag other than `liveSeatShuffle` were not searched for.
* **The snapshot.** The 116 `g_pMemAllocSingleton` guards are "true after `RunStartupSelfTests`";
  that no code/Game body runs inside `Engine::Startup` was read from the call structure, not proven.
* **Argument / width changes.** Only single-door hunks were compared by argument count; a hunk with
  two or more door calls, or a cast that narrows inside an otherwise identical call, is not flagged.
* **Moved code.** Function deletion is by qualified name; a body moved and renamed at once would
  show as deleted-plus-added and its side effects were compared only when the residue was flagged.
* **Leftovers.** `b3r-Leftovers` merged (fef215fe) while this audit ran. Its 55 files were covered by
  part A, part B, the guard pass and the PORT GAP key cross-check (no new NULL guard, no new
  registered PORT GAP key), not by the hand reading of parts C and E.
