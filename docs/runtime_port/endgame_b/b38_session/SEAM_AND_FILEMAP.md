# B.3 step 8 — block 0D: the RegisterExeAddr seam, threading, file map, order and retirement

Block-0 deliverable for `NEXT_SESSION.md` §2b items **5, 6, 7 and 8**, over `code/Game/` +
`standalone/` + `engine/`. Read-only: no source, header, `.inc`, Makefile or tool was edited, nothing
was built or run, nothing committed. main @ 1d7866d4 (working tree: `rt_export_addrs.inc` modified,
`b38_session/` untracked). No Ghidra read was needed: the retail slot lists used here come straight
from `Application-x64.exe`'s `.rdata` bytes (`pe_slots.py`, `pefile`), so §1's "retail slot entry"
test is binary truth, not a Ghidra decompile. Scripts are in `C:/Users/USPC/AppData/Local/Temp/claude/b38/0D/`
(not checked in); §5 names each one. Comments are stripped before every match (a comment is not a
caller); `standalone/build*/`, `tests/unit/build/`, `.claude/` are never scanned.

**Class inventory caveat.** This file needs "which block owns class X" and counts classes by the
`REGISTER_TYPE*` / `REGISTER_ABSTRACT_FACTORY` literals (parent = the literal's second argument) plus
the `RegisterExeAddr`'d `.rdata` tables. The authoritative class list (hierarchy from the exe, secondary
vptrs, slot counts) is 0A/0B/0C's; where they disagree with a count below, theirs wins and only the
block assignment rule of §3.1 needs re-running.

---

## 1. The `RegisterExeAddr` seam (item 5)

### 1.1 The population

`RegisterExeAddr(` calls, comments stripped, in `code/` + `standalone/` excluding the declaration and
definition in `resolve.h` / `resolve_standalone.cpp`: **1,391** (`scan_a.py`; a raw grep including
comments and the two declarations finds 1,427; `NEXT_SESSION.md`'s 1,401 is a raw grep of a different
scope). 1,128 live in `standalone/`, 263 in `code/Game`. The first argument was resolved to an address
for 1,380 (hex literal, a file-local / header `constexpr`, or a `FooAddr::Vtable` namespace constant);
the 11 unresolved are the loop helpers that register a runtime table of `{addr, value}` pairs
(`address_registry.cpp:1073`, `reg_goal.cpp:44`, `reg_player_place.cpp:40`, `reg_unreg_lane1..4`,
`reg_element_typeids.cpp:148`, `reg_food_typeid.cpp:112`, `reg_rdata_float_consts.cpp:72`,
`serialize_arrays.cpp:180`) — all data (dispatch-slot cells, type-id cells, float constants), none a
function or a vtable.

Classified by the SECTION of the exe address (`Application-x64.exe`: `.text` 0x140001000-0x140150185,
`.rdata` 0x140151000-0x1401c0da9 with the import table at 0x140151000-0x140152c20, `.data` 0x1401c1000-):

| section | rows | what they are |
|---|---:|---|
| `.text` | **1,037** | a function body (1,037 distinct addresses, no address registered twice) |
| `.rdata` outside the IAT | **241** | **199 vtables** (the address starts a run of `.text` pointers in the retail `.rdata`, or the target is a table) + 42 other `.rdata` data (float/int constants, the `reg_rdata_float_consts.cpp` family) |
| `.data` | **100** | 88 are per-class **type-id cells** `g_tid_<Class>` (the exe's `DWORD_1401c....` that a class's `GetTypeId` reads); 12 singleton/storage cells and globals |
| IAT | 2 | two dispatch-slot cells (`reg_mods.cpp:98` `g_slot_ModificationSingleton`, one more) |
| unresolved | 11 | the table-driven data loops above |

### 1.2 Function registrations: slot body or not, and by-address reach

Definitions (`classify_a.py`):
- **slot body** = the registered `.text` address is an entry of a RETAIL vtable (it appears as a qword in
  a run of ≥2 `.text` pointers in the exe's `.rdata`; runs are split at every registered vtable start —
  `pe_slots.py`: 345 runs, 3,068 distinct `.text` targets), OR the registered function's name sits in a
  tree table (an array initialiser, a `vt[N] = …` slot store, a `*Slot*/*Vtable*(…)` helper call, or a
  `COMPONENT_*/CONTROLLER_*` factory-macro argument).
- **reached by address elsewhere** = the address (hex, or a UNIQUE named constant holding it) appears
  anywhere outside its own registration, outside a table region and outside a string literal:
  `ResolveAddr(Module::Exe, A)`, a lambda `R(A)` / `RSlot(A)`, an `engine_funcs` initialiser, a `static`
  cache.

| class | rows | meaning |
|---|---:|---|
| **(a)** slot body, reached ONLY through a table | **75** | dies with the table when a compiler vtable replaces it. 74 are retail slot entries; 1 (`PathTool_ClearHighlight_d250`, `pathtool.cpp:468` table) is a tree-table body at an address that is not a retail slot entry — read it before converting. 25 of the 75 are installed in a tree table BY NAME; the other 50 are reached through a per-file `*_VSlot`/`*_Thunk` wrapper that calls them by name (e.g. `diseasecomponent.cpp:975` calls `DiseaseComponent_Update`), so their `RegisterExeAddr` row is already a dead door today (nothing resolves the address). |
| **(b)** slot body, ALSO reached by address | **14** | stays a door delegating to the member (the step-6/7 shape) |
| **(c)** not a slot body | **948** | out of scope for the table question. 646 of them ARE reached by address (2,066 sites; `engine_funcs.cpp` 216, `citizencomponent.cpp` 46, `pasturework.cpp` 33, `foresterwork.cpp` 32, `marketwork.cpp` 30 …) — these are the non-virtual members / helpers that stay doors until their callers become member calls (caller batches); 302 are not referenced by address anywhere (their registration serves no caller in the tree). |

Of the 88 slot-body rows that are retail slot entries, only **21** are entries of a retail table the tree
ALSO hand-builds and registers; the other 67 belong to component tables that the tree builds with the
factory macros / `*_LiveVtable` arrays and never registers at the exe address (a component's vtable is
reached through `Object::Factory`, not by address).

**(a) per registering file — 75** (address, body, retail `table[slot]` (`*` = retail table NOT
registered in the tree), owning block of §3):

- `code/Game/Component/particlecomponent.cpp` (1): 0x140053090 ParticleComponent_Event 15a290[12]* {CIT}
- `code/Game/Game/gameupdateloop_serialize.cpp` (1): 0x1400959c0 GameUpdateLoop_SaveRestore 15e2f0[7] {LOOP}
- `code/Game/Player/pathtool_setselected.cpp` (1): 0x1400dd250 PathTool_ClearHighlight_d250 — no retail slot; `pathtool.cpp:468` {TOOL}
- `standalone/address_registry.cpp` (1): 0x1400aed40 Reimpl_GeometryChunkUpdate 15f110[1] {LOOP}
- `standalone/reg_ambientenv_ui_serialize.cpp` (2): 0x1400437e0 AmbientEnvironmentComponent_Serialize 158b70[3]* {CIT} · 0x14012dd00 UIComponent_Serialize 1649f0[3]* {VIS}
- `standalone/reg_animal_serialize.cpp` (6): HerdComponent_Serialize 163680[3]* · AnimalComponent_Serialize 162110[3]* · LivestockComponent_Serialize 1639b0[3]* · PopulationComponent_Serialize_Real 15a310[13] · WildAnimalComponent_Serialize 164af0[3]* {CIT} · PastureComponent_Shutdown 163f60[3]* {MI-B}
- `standalone/reg_animal_update.cpp` (3): AnimalComponent_Update 162110[9]* · _Startup [4]* · _OnAdd [6]* {CIT}
- `standalone/reg_autosave.cpp` (1): 0x14005e110 AutoSaveDialog_Slot4 15b120[4]* {DLG}
- `standalone/reg_cemetery_healer_serialize.cpp` (2): CemeteryComponent_Shutdown 1590a0[3]* · HealerComponent_Shutdown 159730[3]* {MI-A}
- `standalone/reg_citizen_employ_cascade.cpp` (1): CitizenComponent_OnAdd 162580[6]* {CIT}
- `standalone/reg_citizen_needsupdate.cpp` (1): CitizenComponent_NeedsUpdate 162580[15]* {CIT}
- `standalone/reg_citizen_onremove.cpp` (2): CitizenComponent_OnRemove 162580[7]* {CIT} · BuildComponent_OnCreate 162260[7]* {MI-A}
- `standalone/reg_citizenai_executor.cpp` (1): CharacterComponent_MoveTick_1094c0 162370[8]* {CIT}
- `standalone/reg_component_cropdisease.cpp` (5): DiseaseComponent_{Update[9], Startup[4], OnAdded[6], OnRemoved[7], Serialize[3]} 159290* {CIT}
- `standalone/reg_component_economy.cpp` (14): AttendanceComponent_{CommandMask[13], Command[14], SerializeSlot[3], Unregister[7]} 158ce0* {MI-A} · ForesterComponent_{CommandMask[13], Command[14], CreateWork[18]} 163460* · MarketComponent_CreateWork 159c40[18]* · ConsumeProduceComponent_CreateWork 162a80[18]* · TradeComponent_{Draw[6], OnCreate[7], Slot13, Slot14} 15acd0* {MI-B} · InteractComponent_Draw 1638b0[6]* {VIS}
- `standalone/reg_component_nature.cpp` (2): ZoneComponent_{Draw[6], OnCreate[7]} 164e30* {VIS}
- `standalone/reg_fire.cpp` (1): FireComponent_WaterApplied_090 159380[15] {CIT}
- `standalone/reg_fruit_happiness_serialize.cpp` (2): FruitComponent_Serialize 159400[13] {CIT} · HappinessComponent_Shutdown 1595d0[3]* {MI-A}
- `standalone/reg_growth_spawn_serialize.cpp` (2): GrowthComponent_Serialize2 163590[3]* · SpawnComponent_Serialize 15a670[3]* {CIT}
- `standalone/reg_herd_live.cpp` (4): Herd_OnRemove_Thunk 163680[7]* · Pop_{Startup[14], OnAdd[16], OnRemove[17]}_Thunk 15a310 {CIT}
- `standalone/reg_pasture_live.cpp` (5): Pasture_{ReleaseAllAnimals[7], CanCommand[13], Command[14]} 163f60* {MI-B} · LivestockComponent_{OnAdd[6], OnRemove[7]} 1639b0* {CIT}
- `standalone/reg_player_bridge.cpp` (2): BridgeTool_CalcPath_Impl 161b00[13] · BridgeTool_Draw_Impl 161b00[8] {TOOL}
- `standalone/reg_player_edit.cpp` (1): FencedTool_CalcPath_Port 1605d0[13] {TOOL}
- `standalone/reg_quarry_live.cpp` (2): QuarryComponent_Slot12 / _Slot15 15a490* {VIS}
- `standalone/reg_terraform_live.cpp` (4): TerraformComponent_{Shutdown[3], OnRemove[7], Slot12} 15a880* {VIS} · MultiObjectComponent_Slot12 163cf0[12]* {MI-B}
- `standalone/reg_tornado.cpp` (3): TornadoComponent_{Shutdown[3], Serialize[4], OnRemove[8]} 15aa80 {CIT}
- `standalone/reg_town_services.cpp` (3): CemeteryComponent_{BuildPlotGrid[6], OnRemove[7]} 1590a0* · HealerComponent_OnRemove 159730[7]* {MI-A}
- `standalone/reg_trade_serialize.cpp` (1): TradeComponent_Serialize 15acd0[3]* {MI-B}
- `standalone/reg_work_tail_trade.cpp` (1): TradeWork_Slot4_Impl 161a10[4] {WRK}

**(b) per registering file — 14** (← the by-address sites that keep the door alive):

- `code/Game/Component/particlecomponent.cpp:1038` 0x140052b30 ParticleComponent_UpdateTick 15a290[8]* ← `particlecomponent.cpp:806` {CIT}
- `standalone/reg_code_lane2.cpp:70` 0x14005f3d0 ConsumeProduceUI_Slot4 15b2e0[4]* ← `engine_funcs.cpp:1306` {PNL}
- `standalone/reg_code_lane6.cpp` (3): :69 0x1400fc6b0 DragTool_OnKey 161b00[6], 161b90[6], 161c10[6] ← `engine_funcs.cpp:1078` {TOOL} · :86 0x14011b560 DecalComponent_Draw 162fd0[6]* ← `decalcomponent.cpp:385` {VIS} · :96 0x14012bbd0 StatusIconComponent_OnRemove_Standalone 164620[8]* ← `statusiconcomponent.cpp:80` {VIS}
- `standalone/reg_component_economy.cpp` (3): :97 0x14012b4a0 ResourceProduceComponent_BaseShutdown 1643f0[3]* ← `consumeproducecomponent.cpp:53`, `fishingcomponent.cpp:35`, `forestercomponent.cpp:34`, `herbalistcomponent.cpp:39` +2 {MI-B} · :109 0x140108d40 BuildComponent_OnAddedThunk 162260[6]* ← `buildcomponent.cpp:71` {MI-A} · :190 0x1401189b0 **Component_SerializeReimpl** 1628d0[13] ← 52 sites (`absorbresourcecomponent.cpp:136`, `ambientemittercomponent.cpp:283`, … every component's serialize) — the Component base `Serialize`; it belongs to **K1**, not the file that registers it.
- `standalone/reg_herd_live.cpp:80` 0x140121f40 Herd_OnAdd_Thunk 163680[6]* ← `starttown.cpp:3681`, `death_harness.cpp:549` {CIT}
- `standalone/reg_keystone_a_removal.cpp:41` 0x1401035e0 RemovalCalc_Slot13 161de0[13] ← `removaltool.cpp:543` {TOOL}
- `standalone/reg_pasture_live.cpp:79` 0x140127720 Pasture_MsgReceive 163f60[15]* ← `pasture_harness.cpp:40` {MI-B}
- `standalone/reg_player_drag.cpp:54` 0x1400fc640 DragTool_CancelDrag_Impl 161b00[9], 161b90[9], 161c10[9] ← `engine_funcs.cpp:1080` {TOOL}
- `standalone/reg_work_base.cpp:49` 0x1400f7400 **WorkBase_Serialize_f7400** 161a80[3] ← 22 sites (`autopickupwork.cpp:66`, `buildroad.cpp:60`, `cleararea.cpp:107`, `clearstorage.cpp:668` …) — the Work base `Serialize` {WRK}
- `standalone/reg_work_tail_serialize.cpp:55` 0x1400f2370 PastureWork_Slot4_Impl 1617a0[4] ← `pasturework.cpp:251` {WRK}

Rule for the sheet: the two base `Serialize` rows (`Component_SerializeReimpl`, `WorkBase_Serialize_f7400`)
and the two `DragTool` rows are the (b) rows with real fan-out; each stays a door that delegates to the
base member (`Component::Serialize`, `Work::Serialize`, `DragTool::OnKey`/`CancelDrag`) until its callers
are qualified base calls (`X::Base::Serialize(stream)` — the step-6/7 qualified-call shape).

### 1.3 Vtable registrations: who reads the table back

**199** registered `.rdata` vtables (`vt_readers.py` → `vt_readers.json`, summarised by `vtsum.py`).
**183** of them are read back BY ADDRESS somewhere other than the registration — **552 sites** — and
**60** are read back from a file other than the one that builds the table. Every by-address read is a
`ResolveAddr(Module::Exe, <vt>)` (hex, a `kXxxVtable` constant, or `XxxAddr::Vtable`), classified by
what the statement does with the pointer (following one cache variable within the file):

| use of the resolved pointer | sites |
|---|---:|
| stamped into an object (`*(void**)self = …`, `self->vtable = …`) directly | 124 |
| stamped via a cache variable (`s_vt_X = ResolveAddr(…)` → `*(void**)p = s_vt_X`) | 173 |
| passed to a helper that stamps (`Action_InitInPlace(mem, size, vt)`, `uintptr_t vt = …; Init(vt)`) | 89 direct + 143 via a variable |
| slot read through the pointer | 3 |
| cached and not used / other | 20 |
| **compared** (`*(void**)obj == vt`) | **0 by address** |

Identity comparisons exist only by NAME, and there are four: `code/Game/Game/tornado.cpp:636`
(`vt == s_tornadoVtable`, the re-entry guard of the Tornado slot copy), `standalone/main.cpp:781`
(the factory self-test compares against `ResolveAddr(Module::Exe, expectedVtableExeAddr)`),
`standalone/pausemenu_harness.cpp:227,240` (liveness check on an open dialog). The exe itself
type-checks with `GetTypeId`/`IsDerivedFrom`, not by vtable identity.

By-NAME uses of the tree arrays outside their definition and registration: 1,427 — 761 slot fills
(`g_X_Vtable[N] = …`, the init-time fills), 375 stamps, 169 helper arguments, 57 registrations, 50
getters (`X_GetVtable()` returning the array, e.g. `reg_mods.cpp:76-77`, `reg_tutorial_wave.cpp:84-89`,
`reg_nullsweep.cpp:21`), 10 other.

**Cross-block by-address readers — 87 sites, the order constraints of §4** (owner of the table → the
block whose file reads it):

| table owner → reader | sites | examples |
|---|---:|---|
| `ComponentEntity` table (registered in `address_registry.cpp`; class → K1) → ACT / WRK / CIT / K1 | 12 / 5 / 2 / 2 | `action_child_ctors.cpp:60`, `action_pf_ctors.cpp:86`, `work_act_path.cpp:154`, `citizenai_handlers.cpp:80`, `componententity.cpp:45-46` |
| GOAL's `TutorialGoalDialog` tables (built in `reg_tutorial_wave.cpp`) → GOAL | 24 | `tutorialgoaldialog.cpp:44-45`, `tutorialgoalbutton.cpp:52,121,204` |
| WRK → MI-B | 11 | `resourceproduce_creatework.cpp:113,162,184,237,333`, `cropfieldcomponent_states.cpp:587` |
| WRK → MI-A | 4 | `clearareacomponent_states.cpp:82,342`, `destroycomponent_states.cpp:157`, `attendancecomponent_states.cpp:91` |
| ACT → WRK | 9 | `work_act_hunt.cpp:127`, `work_act_path.cpp:155`, `work_act_perform.cpp:163`, `work_base_actionctor.cpp:162-164` |
| ACT → CIT | 3 | `citizenai_actionctors.cpp:145-146`, `citizenai_handlers.cpp:81` |
| WRK → CIT / VIS | 1 / 1 | `firecomponent_ignite.cpp:296`, `rawmaterialcomponent.cpp:128` |
| DLG → `engine_funcs.cpp` | 4 | `engine_funcs.cpp:1330` (MainMenuDialog), `:1563` (AchievementDialog), `:1565` (GoalDialog), `:1742` (MessageDialog) |
| LOOP → `engine_funcs.cpp` | 4 | `engine_funcs.cpp:2082` (GameUpdateLoop), `:2090` (splash), `:2091` (outer LoaderLoop), `:2101` (MenuDialogLoopT) |
| standalone tables → `engine_funcs.cpp` | 3 | `:1943` NoiseUI ImageBuffer, `:1710` ModDetailsDialog, `:2464` TutorialInstance |
| GOAL → `standalone/main.cpp` | 1 | `main.cpp:953` (StatGoal self-test) |
| `g_PurecallVtable` → VIS | 1 | `statemachinecomponent.cpp:91` |

**The slot-copy sites — FIVE files, six tables, not four.** Each fills a tree table at run time by
copying the slots of a table some Runtime ctor just stamped, then overrides a few slots:
1. `code/Game/Player/tool_base_toolui.cpp:142-146` — copies `ResolveAddr(Module::Runtime, 0x180156d70)`
   into `g_ToolUI_ChildDialog_Vtable_Standalone` (eager, at registration).
2. `code/Game/Component/frustumoutlineelement.cpp:229-237` — copies the `UI::Element` table the
   `fn_Element_Ctor` just installed (24 slots), overrides 4/5/6; **lazy, guarded by a plain
   `bool g_FrustumVtableReady`**.
3. `code/Game/Dialog/thumb_vtables.cpp:69-77` — `Arm()`: two mirrors (`g_mat`, `g_sheet`) filled from the
   first constructed object, **lazy, `m.ready` plain bool**.
4. `code/Game/Dialog/dialog_controller_vtable.cpp:152-187` — fills the inherited `UI::DialogController`
   slots of every registered controller table from the DLL bodies (eager, at registration).
5. **`code/Game/Game/tornado.cpp:595-600` and `:633-654`** (NOT in `NEXT_SESSION.md`'s list) —
   `Tornado : Graphics::Particle` and its `ParticleInstance` copy the live base table on first
   construction (`s_instVtableReady`, `s_vtableReady`, plain bools) and patch slots 1-3, 5-9 / slot 1.

Every one of these must be gone (its class converted to a real subclass of the real Runtime base) before
the mint/`RT_DELDTOR` row of the base it copies can retire (§4.3).

---

## 2. Threading (item 6)

### 2.1 What runs on the Engine::Loader thread

The loader thread's entry points are the `+0x30` Load slot of the five loop tables:
`GameUpdateLoop_Load` (`standalone/gameupdateloop_stubs.cpp:156` → `gameupdateloop_load.cpp:902`),
`Inner_Load` / `Outer_Load` (`loader_loop_stubs.cpp:229,341` tables), and the `Stub_Load` of
`menudialogloopt_stubs.cpp:782` and `splash_loop_stubs.cpp:612` (`menudialogloopt_stubs.cpp:715` and
`gameupdateloop_load.cpp:888-921` document that they run on the one `Engine::Loader` thread).

A static call graph from those roots (`callgraph.py`: direct calls by name with same-file preference,
calls through `ResolveAddr(Exe, A)` / `R(A)` / cached `s_fn_*` / `engine_funcs` globals to the body
registered at A; **vtable dispatch NOT followed**) reaches **827 functions in 140 files, 125 of them in
`code/Game`**: Action 20, Component 45, Dialog 3, Entity 2, Game 37, Object 1, Player 7, Tutorial 1,
Work 9. The ones that construct or run exe-class members:

- **Components** — `ComponentEntity_CtorFromDescription`, `_StartupComponents`, `_OnAddedPass`
  (`componententity.cpp`) are reached, so EVERY component type spawned by the world build (start town,
  worldgen resources, animals, and every entity of a loaded save) is **constructed, and runs slot 4
  (`Startup`) and slot 6 (`OnAdded`), on the loader thread**; `EntityList_Serialize` is reached, so on the
  load route every component's slot-3 `Serialize` runs there too. Directly reached component bodies
  include `CitizenComponent_OnAdd`, `DecalComponent_OnCreate`, `PickingComponent_OnCreate`,
  `SpawnComponent_InitialSpawn`, `ResidenceComponent_ResidenceInit`, `WorkPlaceComponent_OnCreateInit`,
  `AmbientEnvironmentComponent_InitRegion`, `Component_SerializeReimpl`.
- **Actions and Works** — copy/ctor bodies reached from the start-town citizen setup and the save load:
  `Pf_MoveToCtor_9170`, `Pf_PerformWorkCtor7_9720`, `ActChild_*CopyCtor_*`, `CitizenAI_PerformWorkCtor{4,5,6,8,10,11}`,
  `CitizenAI_PlayAnimationCtor4_ba6b0`, `WorkAct_GatherRawMatCtor_bf790`, the `work_base_*` helpers,
  `Goals_WorkTree_Serialize_d5f0`.
- **Descriptions** (all 78) — loaded through `Object::ExternalList::Find` / the loop's resource list,
  which is the factory's `CreateNewObject` + `Serialize` on the loader thread (B36_RULES §5 established
  this for the Runtime `External`s; the exe descriptions ride the same path). INFERRED (factory dispatch
  is not followed by the call graph).
- **The world externals** — `ClimateGenerator`, `MapGenerator`, `TerrainGenerator`, `Profession(List)`,
  `Disease` list, `MusicList`, `NameDatabase`, `Weather`, the `SkyMesh` (`gameupdateloop_load.cpp` steps
  1-4, 15-16, 28, 31); `MenuScene` and the menu's music on the menu loader (`MenuDialogLoopT_LoadMusic`).
- **Goals / tutorial / scenario** — `Goals_Serialize_f9240`, the goal trees, `AchievementInstance`
  (step 32), `Scenario` (`scenario.cpp:224` documents the menu loader constructing it).
- **UI** — step 9 constructs `UserInterface` and step 13 `EntityUI` on the loader thread; the
  `ControlUIConfig` / `ResourceLimitUIConfig` `Serialize` bodies are reached (config externals). The
  dialogs and panels themselves are shown later from `Startup`/`Update` on the main thread — NOT
  measured beyond that (no dialog ctor is in the reached set).
- **Tools** — only the tool-base helpers reached from start-town (`tool_base_highlight*`, `_path`,
  `_ray`, `_tile`, `_trade`, `keystone_a_removalcalcpath`); tool objects are created on the main thread.

Main thread: every `Update`/`Draw`/`OnMessage` tick, all dialogs and panels, tools, and the Actions and
Works created during play. The same classes (components, actions, works) are therefore constructed on
BOTH threads over a session — the loader during the build, the main thread afterwards — so any lazy
cache in a member is a cross-thread first-use race, not a single-thread idiom.

### 2.2 The size of B6-5 for step 8

`statics.py` + `distinct.py` over `code/Game/**` and the four loop-stub files. Unit = one distinct cache
variable (a function-local `static` and the `if (!x) x = …` that fills it count once).

| pattern | variables | files | shape (example) |
|---|---:|---:|---|
| per-class `GetTypeId` cache: `static uint32_t cached = 0; if (!cached) cached = GetTypeIdByName("X")` | **370** | 287 | `timegoal.cpp:144`, `attendancedescription.cpp:46`; the same body is in all 8 factory macros (`component_factory_base.h:79-82` …) and both `controller_live_base.h` macros (`:63-66`, `:118-121`) |
| other function-local lazy statics (`static T* s = nullptr; if (!s) s = …`) | 249 | 135 | `createremovaldescription.cpp:42` (`s_vtable`), `founding_spawn.cpp:158`, `tool_base_toolui.cpp:253` (`s_pTid`) |
| function-local once-flags (`static bool x = false; … x = true`) | 169 | 91 | most are print-once diagnostics (`harvestorchard_ctor.cpp:55`); some gate one-time work (`dialog_exe_helpers.cpp:208`) |
| function-local "magic statics" initialised from a resolve (`static T f = ResolveAddr(…)`) — thread-safe under GCC's default guard, but still B6-5 | 19 | 10 | `destroycomponent_states.cpp:587`, `sendcomponentmessage.cpp:237`, `citizenbehavior.cpp:259`, `menudialogloopt_stubs.cpp` 4 |
| file-scope `EnsureInit` once-functions (`if (s_init) return; … s_init = true`) filling `s_fn_*` / `s_vt_*` / `s_pTid_*` | 293 | 239 | `action_base_cnr.cpp:120-145`, `gameupdateloop_serialize.cpp:177-192`, `immigrationcomponent.cpp:62` |
| file-scope lazy fills of a resolve (`if (!s_x) s_x = ResolveAddr(…)`) | 77 | 43 | `dragtool.cpp:33`, `work_leaf_hunt.cpp:222`, `tutorialgoaldialog.cpp:32` |
| file-scope lazy `GetTypeIdByName` | 38 | 17 | `citizenai_findhelpers.cpp` 6, `citizenai_leisure.cpp` 5 |
| file-scope lazy other | 86 | 49 | `toolbar_vtable_standalone.cpp` 6 |
| **total** | **1,301** | **474** | |

Per directory: Component 526, Game 201, Dialog 161, Player 121, Work 110, Action 73, Tutorial 57,
Goal 22, Entity 18, loop stubs 8, Object 4. The 111 loader-reached files hold 384 of them.

Plus the table fills, which are the same pattern one level up and are ALSO B6-5: `FillTypedVtable`
(`component_factory_base.cpp:80-87`, sentinel `vt[2]`, the stub loop writes `vt[2]` before the real
`vt[2]` and `vt[3]` land, so a second thread that sees the sentinel can still read a stub in slot 3),
`EnsureVtable` (`:67-73`, plain `bool g_vtableInit`), every `*_FinishLiveVtable`/`*_InstallLive` re-write
on each construction (18 live macro invocations: 13 `CONTROLLER_LIVE_FACTORY_STATES`, 4
`COMPONENT_INERT_FACTORY_SER`, 1 `COMPONENT_LIVE_FACTORY_ONADDED` — `macros.py`; the other component
files hand-expand the same shape), and the three lazy slot-copy sites of §1.3 (frustum, thumb, tornado).
A compiler vtable removes all of the table fills outright; the 370 type-id caches become the class
static the reflected-class recipe gives (`X::GetTypeId()` reading the `_typeInfo` record); the `EnsureInit`
blocks are what B6-5 says to move to global scope at registration time.
The only atomics in the population: `gameupdateloop_load.cpp:895` (`s_worldBuiltLoop`) and `:922`
(`s_loadingLoop`).

---

## 3. The file map (item 7)

### 3.1 The block rule (deterministic by path; `blocks.py` / `blocks_lib.py`)

- `code/Game/Component/`: `*description.*` → **K1**; `component*`, `controller_live_base.*`,
  `controller_state_table.h`, `coll_bridge.h`, `ui_components.h`, `keystone_a_*` → **K1**; otherwise by
  file-name prefix (longest match first): MI-B {cropfield, marketlocation, multiobject, orchard, pasture,
  storagelocation, trade, workplace, resourceproduce, consumeproduce, fishing, forester, herbalist, hunt,
  market} · MI-A {absorbresource, attendance, boat, build, cemetery, cleararea, clearresource, destroy,
  happiness, healer, immigration, nomad, residence, well} · CIT {character, citizen, charmanager, animal,
  herd, livestock, wildanimal, population, growth, naturalresource, fruit, spawn, disease, tombstone,
  fire, tornado, particle, ambient, animalwave, entity_finders} · VIS {autopickup, decal, floor,
  highlight, interact, map, mine, model, picking, quarry, rawmaterial, statemachine, statusicon,
  storagedisplay, storage, terraform, tracker, ui, work, zone, frustumoutline, capturebox, animaudioevent}.
- `code/Game/Entity/`: `componententity*` → K1, `citizenbehavior*` → CIT, `entitylist*` → LOOP.
- `code/Game/Action/` → ACT · `Work/` → WRK · `Player/` → TOOL · `Goal/`, `Tutorial/`, `Game/scenario*` → GOAL.
- `code/Game/Dialog/`: the DialogInstance files (`*dialog*`, `menu*`, `mod*`, `dialog_*`, `thumb_vtables`,
  `mod_dialog_vtables`) → DLG; everything else (the `*ui*`/`*uiconfig*` controllers, `townhall*`,
  `tradeui*`, `toolbar_*` helpers, `ui_dialog_stub_factories.cpp`) → PNL.
- `code/Game/Game/` (all other files) and `standalone/{gameupdateloop,loader_loop,menudialogloopt,splash_loop}_stubs.cpp` → LOOP.
- `code/Game/Object/` → coordinator.
- A single-block `standalone/reg_*.cpp` goes to that block; a multi-block one is coordinator-owned.

The rule is a function of the path, so the sets are disjoint by construction; every one of the 724
`code/Game` files (`.cpp` + `.h`) lands in exactly one set (0 unassigned).

### 3.2 The proposed blocks (12 + coordinator)

| block | wave | exe base(s) it owns / derives from | reflected classes | files (.cpp) | lines | registered exe vtables |
|---|---|---|---:|---:|---:|---:|
| **K1** Component + Description bases, all Descriptions, ComponentEntity | 1 | owns `Component`, `ControllerComponent` (the secondary base), `Description`, `ComponentDescription`, `ComponentEntity`; derives `Object::Internal`/`External` | 83 (78 descriptions) | 93 (87) | 14,538 | 82 |
| **ACT** Actions | 1 | owns `Action`; `Object::Internal` | 17 (+`Action`) | 58 (40) | 19,626 | 17 |
| **WRK** Works | 1 | owns `Work`, `AutoPickupWork` | 21 (+2 bases) | 84 (55) | 32,944 | 23 |
| **TOOL** Tools | 1 | owns `Tool`, `DragTool`, `ToolSetting` | 9 (+3 bases) | 44 (34) | 17,512 | 9 |
| **DLG** Dialogs | 1 | `UI::DialogInstance` (+ `MessageDialog` → `OptionInputBindDialog`) | 11 (19 of `Dialogs.h`'s 22 PODs) | 26 (24) | 14,547 | 9 |
| **PNL** Panels (controllers + configs) | 1 | `UI::DialogController`, `UI::DialogControllerConfig`; owns `SelectResourceUI` | 38 (36 of `Panels.h`'s 38 PODs) | 58 (55) | 28,622 | 4 |
| **GOAL** Goals, Tutorial, Scenario | 1 | `Achievement::{Goal, GoalInstance, Achievement}`, `Object::External` (tutorial externals), `UI::DialogInstance` (the 3 tutorial dialogs) | 25 | 60 (34) | 12,497 | 20 (+6 in `reg_tutorial_wave.cpp`) |
| **LOOP** Engine loops + world externals + the world-build caller files | 1 | `Engine::{UpdateLoop, LoaderLoop, DialogLoopT, DialogLoaderLoopT}` (retires the `RtEngine::` PODs and the five stub tables); `Object::External`; `Graphics::Particle` (Tornado) | 12 + 6 loop tables | 97 (79) | 39,749 | 23 |
| **CIT** Citizens + nature/animals + fire/particle/ambient (non-MI components) | 2 | `Component` | 19 | 76 (56) | 31,942 | 1 (+ standalone) |
| **VIS** Presentation + misc components, the 2 `UI::Element`s and `AnimAudioEvent` | 2 | `Component`, `UI::Element`/`ElementDescription`, `Graphics::AnimationEvent` | 23 | 45 (26) | 10,538 | 1 |
| **MI-A** ControllerComponent types (town) | 3 | `Component` + `ControllerComponent` | 14 | 42 (28) | 12,937 | 0 (+ standalone) |
| **MI-B** ControllerComponent types (production/trade) + `ResourceProduceComponent` | 3 | `ControllerComponent` → owns `ResourceProduceComponent` | 14 (+1 intermediate) | 40 (26) | 12,783 | 0 (+ standalone) |

Line counts are raw (comments included). LOOP is large in lines but small in classes: only ~30 of its
97 files carry a class (`gameupdateloop*.cpp`, `menudialogloopt*`, `dialogloaderloop*`, the four
stub files, `climategenerator`/`mapgenerator`/`terraingenerator`/`menuscene`/`musiclist`/`namedatabase`/
`profession*`/`toolbar`/`weather*`/`disease`/`tornado`/`entitylist`); the other ~65 (`starttown.cpp`,
`worldgen*`, `serialize_*`, `*_harness.cpp`, `*_seed.cpp`) are caller files owned by LOOP only so no one
else edits them. If a 13th block is wanted, split LOOP into LOOP (loops) and GEXT (the world externals).
The 28 `ControllerComponent` classes (22 with the literal parent `ControllerComponent` + 6 with
`ResourceProduceComponent`) are what `ENDGAME.md` calls "the 56 multiple-inheritance types" if the 56
counts TABLES (primary + secondary vptr per class) — INFERRED; 0A/0B's stamp-chain read is authoritative.

### 3.3 Coordinator-owned files, their fan-out, and which blocks need lines in them

| file | why shared | fan-out (`fmap.py`, `-I` order resolution) | blocks that need lines |
|---|---|---|---|
| `engine/types/ui/Dialogs.h` (22 PODs) | one header, 3 blocks | 5 direct includers, 27 `.cpp` transitively (Dialog 22, Tutorial 3, standalone 2) | DLG 19 classes, GOAL 3 (`TutorialGoalDialog`, `TutorialHighlightDialog`, `TutorialSetupDialog`; `::TutorialSetupDialog` is ALSO defined at `engine/types/TutorialSetups.h:90` — the live ODR violation), `Dialogs.h:23` vs `Panels.h:22` both claim 0x14015b070 |
| `engine/types/ui/Panels.h` (38 PODs) | one header, 3 blocks | 3 direct, 44 `.cpp` (Dialog 40, Tutorial 2, standalone 2) | PNL 36, DLG 1 (`ModBrowseUI`), GOAL 1 (`TutorialUI`) |
| `engine/types/Component.h` | base of every component POD | 46 direct (the component headers), 84 `.cpp` (Component 73, standalone 8, Player 2, Game 1) | K1 edits; CIT/VIS/MI-A/MI-B only read |
| `engine/types/ControllerComponent.h` | secondary base | 17 direct, 27 `.cpp` (Component 24, standalone 3) | K1 edits; MI-A/MI-B read |
| `engine/types/Tool.h` | `Tool` + `ToolSetting` | 11 direct, 12 `.cpp` (all TOOL + 1 standalone) | TOOL only → TOOL may own it |
| `engine/types/UpdateLoop.h`, `LoaderLoop.h`, `DialogLoaderLoopT.h`, `GameUpdateLoop.h`, `MainLoop.h`, `EntityList.h` | loop PODs | 8 / 4 / 3 / 3 / 3 / 4 `.cpp` (LOOP + standalone + `tests/`) | LOOP only → LOOP may own them (tests/unit readers listed in `hdr.json`) |
| `engine/types/BuildActions.h` (8), `AchievementGoals.h` (7), `TutorialGoals.h`/`Images.h`/`Setups.h` | multi-class but single-block | 9 / 7 / 4 / 4 / 4 `.cpp` | WRK / GOAL only → block-owned |
| `engine/types/common.h` | everything | 14 direct, 757 `.cpp` | none expected |
| `engine/funcs/engine_funcs.{h,cpp}` | the exe-address globals | 216 by-address reaches of registered bodies | by the block of the body: LOOP 47, PNL 38, WRK 30, ACT 21, DLG 19, CIT 18, TOOL 11, GOAL 10, VIS 7, MI-B 6, K1 5, MI-A 4 (`.text`); vtable reads DLG 5, LOOP 5 (`.rdata`); 59 `.data` reads (54 type-id cells, 5 storage cells) |
| `standalone/address_registry.cpp` | the registry | — | LOOP 46, DLG 4, K1 4 (incl. the `ComponentEntity` table), CIT 2, VIS 1, PNL 1 + 4 data rows; its `RegisterVtablesAndTypeIds()` list calls every `Register*Vtable()` |
| the 28 multi-block `standalone/reg_*.cpp` (table below) | rows of several blocks | — | per-row block given below; the coordinator applies each block's row deletions/edits, or a block edits ONLY its own rows and the coordinator merges (two blocks editing different rows of one file conflict trivially) |
| the 26 data-only registration files (`reg_*typeid*.cpp`, `reg_economy_typeids.cpp`, `reg_rdata_float_consts.cpp`, `reg_element_typeids.cpp`, …) | the 88 `g_tid_*` type-id cells + constants | — | retire at the END of B.3 (§4.3), not in a block |
| `code/Game/Object/factory_registrations_bulk.cpp` | 13 abstract base rows | — | K1 (`Component`, `Description`, `ControllerComponent`), MI-B (`ResourceProduceComponent`), ACT (`Action`), WRK (`Work`, `AutoPickupWork`), TOOL (`Tool`, `DragTool`, `ToolSetting`), GOAL (`TutorialGoal`, `TutorialSetup`), PNL (`SelectResourceUI`) |
| `code/Game/Object/factory_registration.{h,cpp}`, `internal_factory_audit.*` | `REGISTER_TYPE`, `GetTypeIdByName` | every reflected class | none — the recipe's class-static registration must keep the NAME literal |
| `code/Game/Component/component_factory_base.{h,cpp}`, `controller_live_base.{h,cpp}` | the factory macros | 18 live macro uses + every hand-expanded factory | K1 in wave 1; afterwards coordinator (a macro/helper is deleted when its last user converts — the closing pass) |
| `code/Game/Dialog/dialog_controller_vtable.{h,cpp}`, `thumb_vtables.cpp`, `tool_base_toolui.cpp`, `frustumoutlineelement.cpp`, `tornado.cpp` | the slot-copy sites | — | PNL, DLG, TOOL, VIS, LOOP respectively own the file; the file is deleted/rewritten only when its class converts |

B6-4.4 applies to `Dialogs.h` / `Panels.h` (and would to any other shared POD header): a leaf block
authors a per-class header for its class; the shared POD header becomes aliases in the closing pass.

---

## 4. Order of conversion and the END-of-B.3 retirement (items 3 and 8)

### 4.1 Dependency sketch

```
Runtime (real):  Object::Internal ─┬─ Component(K1) ─┬─ 39 plain components (CIT 19, VIS 20)
                                   │                 └─ ControllerComponent(K1, secondary) ─┬─ 22 MI types (MI-A 14, MI-B 8)
                                   │                                                        └─ ResourceProduceComponent(MI-B) ── 6 (MI-B)
                                   ├─ ComponentEntity(K1)          ├─ Action(ACT) ── 17
                                   ├─ Work(WRK) ─┬─ 16             ├─ Tool(TOOL) ─┬─ 4
                                   │             └─ AutoPickupWork ── 6            └─ DragTool ── 5      ToolSetting ── RotationSetting
                                   └─ TutorialInstance(GOAL)
                 Object::External ─┬─ Description(K1) ── 78 descriptions;  ComponentDescription(K1)
                                   ├─ 12 world externals (LOOP)
                                   └─ TutorialGoal/TutorialSetup/TutorialImage/TutorialStep/Tutorial (GOAL)
                 UI::DialogInstance ── DLG (11 reflected + the plain ones) + 3 tutorial dialogs (GOAL);  MessageDialog → OptionInputBindDialog
                 UI::DialogController ── PNL (30 + DebugUI + SelectResourceUI → 3) + TutorialUI (GOAL);  UI::DialogControllerConfig ── PNL 4
                 UI::Element / ElementDescription ── VIS 2 + 2;   Graphics::AnimationEvent ── VIS 1;  Graphics::Particle ── Tornado (LOOP)
                 Engine loops ── LOOP (GameUpdateLoop, MenuDialogLoopT, splash, 2 LoaderLoops, GameLoad/Loader dialog loops)
                 Achievement::{Goal, GoalInstance, Achievement} ── GOAL (5 + 2 + Scenario)
```

Waves: **1** = K1, ACT, WRK, TOOL, DLG, PNL, GOAL, LOOP in parallel (none needs another block's base).
**2** = CIT, VIS (after K1 merges `Component`). **3** = MI-A, MI-B (after K1's `ControllerComponent` and
the secondary-vptr unit-test pattern; MI-B's first commit is `ResourceProduceComponent`).

Order constraints from the by-address readers (§1.3) — a table cannot be deleted while another block
stamps it by address: ACT before WRK's `work_act_*` / `work_base_actionctor.cpp` stop reading ACT tables
(9) and before CIT's `citizenai_*` (3); WRK before MI-A (4) / MI-B (11) / CIT (1) / VIS (1); K1's
`ComponentEntity` table after ACT (12), WRK (5), CIT (2); DLG and LOOP tables after `engine_funcs.cpp`'s
8 reads. Within a wave the rule is simply: a block converts its class and KEEPS the exe-address
registration of the table as long as a reader outside the block exists (the table then has one live
filler: the compiler vtable address, registered at the exe address) — the registration is deleted by
whoever converts the last reader.

### 4.2 Per-class retirement order (what each block does, and what waits)

For a class X converted in a block:
1. X is a real C++ class; its ctor stamps the compiler vtable. The hand-built table becomes dead.
2. The (a) slot bodies of X (§1.2) lose their `RegisterExeAddr` rows with the table; the (b) rows stay
   as doors delegating to the member.
3. Every by-address reader of X's exe vtable (§1.3) converts: a stamp becomes `new (p) X(...)` / the
   real ctor; a `vt` passed to `Action_InitInPlace`-style helpers becomes a typed placement ctor.
4. Only then `RegisterExeAddr(<X vtable>)` is deleted.
5. X's `g_tid_X` type-id cell (one of the 88 `.data` rows) is kept until no `ResolveAddr(Module::Exe,
   0x1401c....)` reader of it remains (`engine_funcs.cpp` holds 54 such reads) — END of B.3.
6. X's `REGISTER_TYPE` row becomes the class-static registration with the SAME name literal (saves
   depend on the CRC of the name).

### 4.3 Mint / `_typeInfo` / `RT_DELDTOR` rows that name a base the exe subclasses

All in `standalone/rt_data_mint_tables.inc` (vtable row line, `_typeInfo` row line) and
`runtime/src/object/deleting_dtor.cpp` (`RT_DELDTOR` line). None of the three tables carries an EXE
class name (the mint generator is DLL-only); these 20 are the Runtime bases step 8's classes sit on:

| base | mint vt | `_typeInfo` | `RT_DELDTOR` | subclassed by (block) | retires when |
|---|---:|---:|---:|---|---|
| `Object::Base` | 3896 | 4275 | 295 | everything | last |
| `Object::Internal` | 3960 | 4459 | 294 | K1, ACT, WRK, TOOL, GOAL | all of those converted |
| `Object::External` | 3939 | 4395 | 296 | K1 (descriptions), LOOP, GOAL | idem |
| `Object::CompiledExternal` | 3913 | 4331 | 297 | (none measured in `code/Game`) | with Base |
| `UI::DialogInstance` | 3927 | 4359 | 309 | DLG, GOAL | DLG + GOAL converted and `tool_base_toolui.cpp:142-146` gone (it copies 0x180156d70) |
| `UI::DialogController` | 3925 | 4351 | 308 | PNL | PNL converted and `dialog_controller_vtable.cpp` gone |
| `UI::DialogControllerConfig` | 3926 | 4355 | 896 | PNL (4) | PNL converted |
| `UI::Element` | 3936 | 4383 | 307 | VIS (`CaptureBoxElement`, `FrustumOutlineElement`) | VIS converted and `frustumoutlineelement.cpp:229-237` gone |
| `UI::ElementDescription` | 3937 | 4387 | 818 | VIS (2) | VIS converted |
| `UI::Dialog` | 3924 | 4347 | 349 | read by `engine_funcs.{h,cpp}` `_typeInfo` (not subclassed) | its readers become `UI::Dialog::_typeInfo` |
| `UI::SpriteSheet`, `Graphics::Material` | 4011 / 3970 | 4615 / 4487 | 293 / 822 | `thumb_vtables.cpp` mirrors (DLG) | `thumb_vtables.cpp` gone |
| `Engine::UpdateLoop` | 4026 | — | 306 | LOOP | LOOP converted (the five stub tables and the `RtEngine::` PODs gone) |
| `Engine::LoaderLoop` | 3969 | — | 310 | LOOP (2 tables) | idem |
| `Engine::DialogLoopT` | 3929 | — | 312 | LOOP | idem |
| `Engine::DialogLoaderLoopT` | 3928 | — | 311 | LOOP | idem |
| `Achievement::Achievement` | 3889 | 4251 | 288 | GOAL (`Scenario`) | GOAL converted |
| `Achievement::AchievementInstance` | 3890 | 4255 | 289 | the never-registered exe table 0x14015e350; `gameupdateloop_serialize.cpp` reads its `_typeInfo` | LOOP + GOAL converted |
| `Achievement::Goal` | 3947 | 4419 | 290 | GOAL (5) | GOAL converted |
| `Achievement::GoalInstance` | 3948 | 4423 | 291 | GOAL (2) | idem |
| `Graphics::Particle` / `ParticleInstance` | 3981 / 3982 | 4523 / — | 302 / 303 | `Tornado` + its instance (LOOP, `tornado.cpp` slot copies) | `tornado.cpp:595-654` gone |
| `Graphics::AnimationEvent` | 3894 | 4267 | 299 | VIS (`AnimAudioEvent`) | VIS converted |

Exe-side readers of these rows by mangled name (comments stripped): `standalone/reg_mint_promoted.cpp`
names all of them; `reg_unreg_lane1.cpp` (`External` `_typeInfo`), `reg_unreg_lane3.cpp` (`Particle`),
`reg_element_typeids.cpp` + `engine_funcs.h` (`Element`), `engine_funcs.{h,cpp}` (`Dialog`),
`gameupdateloop_serialize.cpp:175` (`AchievementInstance`), `dialog_controller_vtable.h` +
`standalone/dlgctrl_route.cpp` (`DialogController`), `code/Game/Game/tornado.cpp` (`Particle`),
`cutover_manifest.cpp` (`Dialog`). The general condition, per base B: **B's mint vtable row retires when
no exe class stamps or copies B's minted table (every subclass converted, every slot-copy site of §1.3
gone) and no `ResolveExport("??_7B…")` / `RT_VTABLE_ONCE` remains in `code/Game` + `standalone/`; B's
`_typeInfo` row when no exe reader resolves `?_typeInfo@B…` by name; B's `RT_DELDTOR` row when every
subclass has a compiler deleting dtor (no exe deleting-dtor dispatch reaches the row)**. Order inside
the END step: slot-copy sites → subclasses → exe vtable registrations → `g_tid_*` cells → mint vt rows →
`RT_DELDTOR` rows → `_typeInfo` rows → `Object::Base` last.

---

## 5. How this was measured (scripts in `C:/Users/USPC/AppData/Local/Temp/claude/b38/0D/`)

- `calls.py` — file walk (`code standalone engine runtime dx11 tests`, `build*` and `.claude` pruned),
  comment stripper (keeps strings and line numbers), balanced-argument splitter.
- `scan_a.py` — every `RegisterExeAddr(` call, address resolution (hex / constant / `Ns::Name`), the
  tree's pointer arrays (`arrays.json`, 975). `dump_rea.py` — first raw census.
- `pe_slots.py` — retail `.rdata` runs of `.text` pointers from `Application-x64.exe` via `pefile`
  (`slotmap.json`, `runs.json`).
- `classify_a.py` — (a)/(b)/(c) with slot-body test (retail + tree tables incl. factory macros) and
  by-address reach (hex + unique constants, strings/registrations/table regions excluded) → `seam_final.json`.
- `vt_readers.py` + `shape.py` + `vtsum.py` — by-address and by-name readers of every registered vtable.
- `callgraph.py` — loader-thread reach (`loader_reach.json`). `statics.py` + `distinct.py` — the B6-5
  census (`statics.json`, `distinct.json`). `macros.py` — live factory-macro invocations.
- `classes.py` (`regtypes.json`), `perfile.py`, `fmap.py` (`hdr.json`: engine/types struct counts and
  include fan-out with the Makefile's `-I` order), `blocks.py` / `blocks_lib.py` (block rule, disjointness,
  per-reg-file block counts → `regfiles.json`), `ef.py` (engine_funcs / address_registry rows by block),
  `appendix.py` (§6).
- Slot-copy sites: grep `\[\s*(i|k|…)\s*\]\s*=\s*…\w+\[(i|k|…)\]` over `code/Game` + `standalone`,
  read by hand. Compare sites: grep of `==`/`!=` against `vt`/`vtable`/`Vtable` names.
- Mint rows: `grep -n '"??_7<B>@@6B@"' standalone/rt_data_mint_tables.inc`, `'"?_typeInfo@<B>'`,
  `'RT_DELDTOR(.*"<NS>::<B>"' runtime/src/object/deleting_dtor.cpp`.

**Not measured:** vtable-dispatch edges in the call graph (the component/description factory paths are
INFERRED from the reached `CtorFromDescription` / `ExternalList` / `EntityList_Serialize` callers);
which dialogs are constructed on the loader thread (none reached directly); the secondary-vptr offsets
(0A/0B); by-address reads through computed addresses (`base + off` arithmetic) — none were seen, but a
table-driven `{addr, fn}` loop would evade the hex search (the 11 unresolved registrations are the known
ones and are all data).

---

## 6. Appendix — the per-block file sets

### K1 — 93 files (87 .cpp), 14538 lines; reflected classes (83): AbsorbResourceDescription, AmbientEmitterDescription, AmbientEnvironmentDescription, AnimalDescription, AttendanceDescription, AutoPickupDescription, BoatDescription, BuildDescription, CaptureBoxDescription, CemeteryDescription, CharacterDescription, CitizenAIDescription, CitizenDescription, ClearAreaDescription, ClearResourceDescription, ComponentDescription, ComponentEntity, ConsumeProduceDescription, CreateBridgeDescription, CreateDragDescription, CreateFencedDescription, CreatePathDescription, CreatePlacedDescription, CreateRemovalDescription, CreateRoadDescription, CropFieldDescription, DecalDescription, DestroyDescription, DiseaseDescription, EnvironmentDescription, FireDescription, FishingDescription, FloorDescription, ForesterDescription, FruitDescription, FrustumOutlineDescription, GrowthDescription, HappinessDescription, HealerDescription, HerbalistDescription, HerdDescription, HighlightDescription, HuntDescription, ImmigrationDescription, InteractDescription, LivestockDescription, MapDescription, MarketDescription, MarketLocationDescription, MineDescription, ModelDescription, MultiObjectDescription, NaturalResourceDescription, NomadDescription, OrchardDescription, ParticleDescription, PastureDescription, PickingDescription, PopulationDescription, QuarryDescription, RadiusDescription, RawMaterialDescription, ResidenceDescription, SelectDescription, SpawnDescription, StateMachineDescription, StatusIconDescription, StorageDescription, StorageDisplayDescription, StorageLocationDescription, TerraformDescription, TombstoneDescription, ToolbarDescription, TornadoDescription, TrackerDescription, TradeDescription, UIDescription, ValueDescription, WellDescription, WildAnimalDescription, WorkDescription, WorkPlaceDescription, ZoneDescription
- `code/Game/Component/` (91): absorbresourcedescription.cpp ambientemitterdescription.cpp ambientenvironmentdescription.cpp animaldescription.cpp attendancedescription.cpp autopickupdescription.cpp boatdescription.cpp builddescription.cpp captureboxdescription.cpp cemeterydescription.cpp characterdescription.cpp citizenaidescription.cpp citizendescription.cpp clearareadescription.cpp clearresourcedescription.cpp coll_bridge.h component_factory_base.cpp component_factory_base.h componentdescription.cpp consumeproducedescription.cpp controller_live_base.cpp controller_live_base.h controller_state_table.h createbridgedescription.cpp createdragdescription.cpp createfenceddescription.cpp createpathdescription.cpp createplaceddescription.cpp createremovaldescription.cpp createroaddescription.cpp cropfielddescription.cpp decaldescription.cpp destroydescription.cpp diseasedescription.cpp environmentdescription.cpp firedescription.cpp fishingdescription.cpp floordescription.cpp foresterdescription.cpp fruitdescription.cpp frustumoutlinedescription.cpp growthdescription.cpp happinessdescription.cpp healerdescription.cpp herbalistdescription.cpp herddescription.cpp highlightdescription.cpp huntdescription.cpp immigrationdescription.cpp interactdescription.cpp keystone_a_map.cpp keystone_a_restree.cpp livestockdescription.cpp mapdescription.cpp marketdescription.cpp marketlocationdescription.cpp minedescription.cpp modeldescription.cpp multiobjectdescription.cpp naturalresourcedescription.cpp nomaddescription.cpp orcharddescription.cpp particledescription.cpp pasturedescription.cpp pickingdescription.cpp populationdescription.cpp quarrydescription.cpp radiusdescription.cpp rawmaterialdescription.cpp residencedescription.cpp selectdescription.cpp spawndescription.cpp statemachinedescription.cpp statusicondescription.cpp storagedescription.cpp storagedisplaydescription.cpp storagelocationdescription.cpp terraformdescription.cpp tombstonedescription.cpp toolbardescription.cpp tornadodescription.cpp trackerdescription.cpp tradedescription.cpp ui_components.h uidescription.cpp valuedescription.cpp welldescription.cpp wildanimaldescription.cpp workdescription.cpp workplacedescription.cpp zonedescription.cpp
- `code/Game/Entity/` (2): componententity.cpp componententity_funcs.h
- single-block `standalone/` registration files (2): reg_keystone_a_map.cpp reg_keystone_a_restree.cpp

### ACT — 58 files (40 .cpp), 19626 lines; reflected classes (17): AnimateNaturalResource, CompleteRoad, CompleteTask, ConsumeRawMaterial, CreateNaturalResource, CreateRawMaterial, DropOffRawMaterial, HuntAnimal, MoveTo, PerformWork, PickupRawMaterial, PickupStoredRawMaterial, PlayAnimation, RemoveAnimal, RemoveNaturalResource, SendComponentMessage, ShowTool
- `code/Game/Action/` (58): action_base_cnr.cpp action_base_complete.cpp action_base_hunt.cpp action_base_move.cpp action_base_movedeps.cpp action_base_rawmat.cpp action_base_standalone.cpp action_base_standalone.h action_child_ctors.cpp action_child_ctors2.cpp action_pf_astar.cpp action_pf_avl.cpp action_pf_ctors.cpp action_pf_geom.cpp action_pf_pools.cpp action_pf_region.cpp action_pf_road.cpp action_pf_roots_drop.cpp action_pf_roots_misc.cpp action_pf_roots_road.cpp action_pf_roots_route.cpp animatenaturalresource.cpp animatenaturalresource_funcs.h completeroad.cpp completeroad_funcs.h completetask.cpp completetask_funcs.h consumerawmaterial.cpp consumerawmaterial_funcs.h createnaturalresource.cpp createnaturalresource_funcs.h createrawmaterial.cpp createrawmaterial_funcs.h dropoffrawmaterial.cpp dropoffrawmaterial_funcs.h huntanimal.cpp huntanimal_funcs.h keystone_a_zone.cpp moveto.cpp moveto_funcs.h performwork.cpp performwork_funcs.h pickuprawmaterial.cpp pickuprawmaterial_funcs.h pickupstoredrawmaterial.cpp pickupstoredrawmaterial_funcs.h playanimation.cpp playanimation_funcs.h region_graph_build.cpp region_paircache.cpp removeanimal.cpp removeanimal_funcs.h removenaturalresource.cpp removenaturalresource_funcs.h sendcomponentmessage.cpp sendcomponentmessage_funcs.h showtool.cpp showtool_funcs.h
- single-block `standalone/` registration files (21): reg_action_child_ctors.cpp reg_action_child_ctors2.cpp reg_action_cnr.cpp reg_action_complete.cpp reg_action_hunt.cpp reg_action_move.cpp reg_action_rawmat.cpp reg_keystone_a_zone.cpp reg_pf_astar.cpp reg_pf_avl.cpp reg_pf_ctors.cpp reg_pf_geom.cpp reg_pf_pools.cpp reg_pf_region.cpp reg_pf_road.cpp reg_pf_roots_drop.cpp reg_pf_roots_misc.cpp reg_pf_roots_road.cpp reg_pf_roots_route.cpp reg_region_graph.cpp reg_region_paircache.cpp

### WRK — 84 files (55 .cpp), 32944 lines; reflected classes (21): BuildRoad, BuildStructure, ClearArea, ClearStorage, ConsumeProduce, FireWork, FishingWork, ForesterWork, GatherRawMaterials, HarvestArea, HarvestOrchard, HerbalistWork, HuntWork, LocationWork, MarketWork, PastureWork, Pickup, PlantArea, PlantOrchard, TendArea, TradeWork
- `code/Game/Work/` (84): autopickupwork.cpp autopickupwork_funcs.h build_actions.cpp buildroad.cpp buildroad_funcs.h buildstructure.cpp buildstructure_funcs.h cleararea.cpp cleararea_funcs.h clearstorage.cpp clearstorage_funcs.h consumeproduce.cpp consumeproduce_funcs.h firework.cpp firework_funcs.h firework_search.cpp fishingwork.cpp fishingwork_funcs.h foresterwork.cpp foresterwork_funcs.h gatherrawmaterials.cpp gatherrawmaterials_ctor.cpp gatherrawmaterials_funcs.h harvestarea.cpp harvestarea_funcs.h harvestorchard.cpp harvestorchard_ctor.cpp harvestorchard_funcs.h herbalistwork.cpp herbalistwork_funcs.h huntwork.cpp huntwork_funcs.h locationwork.cpp locationwork_funcs.h marketwork.cpp marketwork_bst_a9f0.cpp marketwork_bst_a9f0.h marketwork_bst_aaa0.cpp marketwork_bst_aaa0.h marketwork_bst_afb0.cpp marketwork_bst_afb0.h marketwork_bst_b290.cpp marketwork_bst_b290.h marketwork_bst_b680.cpp marketwork_bst_b680.h marketwork_bst_ba70.cpp marketwork_bst_ba70.h marketwork_funcs.h pasturework.cpp pasturework_funcs.h pickup.cpp pickup_funcs.h pickup_gate.cpp plantarea.cpp plantarea_funcs.h plantorchard.cpp plantorchard_ctor.cpp plantorchard_funcs.h tendarea.cpp tendarea_ctor.cpp tendarea_funcs.h tradework.cpp tradework_funcs.h tradework_serialize.cpp work_act_clear.cpp work_act_consume.cpp work_act_hunt.cpp work_act_path.cpp work_act_perform.cpp work_act_rawmat.cpp work_base_accessor.cpp work_base_actionctor.cpp work_base_bst.cpp work_base_cellbst.cpp work_base_grid.cpp work_base_impl2.cpp work_base_reimpl.cpp work_base_standalone.cpp work_base_standalone.h work_base_walkpos.cpp work_leaf_clear.cpp work_leaf_gather.cpp work_leaf_hunt.cpp work_leaf_market.cpp
- single-block `standalone/` registration files (16): reg_build_actions.cpp reg_gatherrawmaterials_ctor.cpp reg_marketwork_bst.cpp reg_work_accessor.cpp reg_work_act.cpp reg_work_actionctor.cpp reg_work_base.cpp reg_work_bst.cpp reg_work_cellbst.cpp reg_work_grid.cpp reg_work_impl2.cpp reg_work_leaf.cpp reg_work_tail_fire.cpp reg_work_tail_serialize.cpp reg_work_tail_trade.cpp reg_work_walkpos.cpp

### TOOL — 44 files (34 .cpp), 17512 lines; reflected classes (9): BridgeTool, DragPlacementTool, EditTool, FencedTool, PathTool, PlacementTool, RemovalTool, RoadTool, RotationSetting
- `code/Game/Player/` (44): bridgetool.cpp bridgetool_calcpath.cpp bridgetool_draw.cpp bridgetool_funcs.h bridgetool_validate.cpp dragplacementtool.cpp dragplacementtool_funcs.h dragtool.cpp dragtool.h edittool.cpp edittool_funcs.h fencedtool.cpp fencedtool_calcpath.cpp fencedtool_funcs.h keystone_a_removalcalcpath.cpp pathtool.cpp pathtool_funcs.h pathtool_setselected.cpp placementtool.cpp placementtool_funcs.h removaltool.cpp removaltool_funcs.h roadtool.cpp roadtool_calcpath.cpp roadtool_funcs.h rotationsetting.cpp tool_activation_standalone.cpp tool_base_createbar.cpp tool_base_drag.cpp tool_base_edit.cpp tool_base_edit2.cpp tool_base_highlight.cpp tool_base_highlight_tess.cpp tool_base_path.cpp tool_base_place.cpp tool_base_ray.cpp tool_base_render.cpp tool_base_screenrect.cpp tool_base_standalone.cpp tool_base_standalone.h tool_base_tile.cpp tool_base_tilerender.cpp tool_base_toolui.cpp tool_base_trade.cpp
- single-block `standalone/` registration files (15): reg_keystone_a_removal.cpp reg_player_bridge.cpp reg_player_createbar.cpp reg_player_drag.cpp reg_player_edit.cpp reg_player_highlight.cpp reg_player_path.cpp reg_player_ray.cpp reg_player_render.cpp reg_player_roadcalcpath.cpp reg_player_screenrect.cpp reg_player_tile.cpp reg_player_tilerender.cpp reg_player_toolui.cpp reg_player_trade.cpp

### DLG — 26 files (24 .cpp), 14547 lines; reflected classes (11): AutoSaveDialog, EventNotifyDialog, EventPopupDialog, LoaderDialog, MainMenuDialog, MessageDialog, ModDetailsDialog, ModDialog, NewGameDialog, OptionInputBindDialog, OptionsDialog
- `code/Game/Dialog/` (26): achievementdialog.cpp autosavedialog.cpp dialog_controller_vtable.cpp dialog_controller_vtable.h dialog_exe_helpers.cpp dialog_exe_helpers.h eventnotifydialog.cpp eventpopupdialog.cpp followdialog.cpp goaldialog.cpp loaderdialog.cpp mainmenudialog.cpp mapparamsdialog.cpp menu_dialog_standalone_vtables.cpp messagedialog.cpp mod_dialog_vtables.cpp modbrowseui.cpp moddetailsdialog.cpp moddialog.cpp moduploaddialog.cpp newgamedialog.cpp optioninputbinddialog.cpp optionsdialog.cpp saveloaddialog.cpp scenariodialog.cpp toolbardialog.cpp
- single-block `standalone/` registration files (3): reg_autosave.cpp reg_dialog.cpp reg_nullsweep.cpp

### PNL — 58 files (55 .cpp), 28622 lines; reflected classes (38): AttendanceUI, BuildRequirementUI, BuildUI, CemeteryUI, CitizenUI, ConsumeProduceUI, ControlUI, ControlUIConfig, CropUI, DebugUI, DestroyUI, EntityUI, EventLogUI, HelpUI, HotKeyToolTipUI, ImmigrationUI, MapUI, OrchardUI, PageToggleUI, PastureUI, ProductionUI, ProfessionUI, RadiusUI, ResidenceUI, ResourceLimitUI, ResourceLimitUIConfig, StatusBarUI, StatusIconUI, StatusIconUIConfig, StorageCapacityUI, StorageUI, TitleDescToolTipUI, ToolUI, TownHallUI, TradeUI, UpgradeUI, UpgradeUIConfig, WorkPlaceUI
- `code/Game/Dialog/` (58): attendanceui.cpp buildrequirementui.cpp buildui.cpp cemeteryui.cpp citizen_icon_helpers.h citizenui.cpp consumeproduceui.cpp controlui.cpp controluiconfig.cpp cropui.cpp cropui_promote.cpp debugui.cpp destroyui.cpp edittool_select.cpp entityui.cpp eventlogui.cpp helpui.cpp hotkeytooltipui.cpp immigrationui.cpp mapui.cpp noiseui.cpp orchardui.cpp pagetoggleui.cpp pastureui.cpp productionui.cpp professionui.cpp radiusui.cpp radiusui_promote.cpp residenceui.cpp resourcelimitui.cpp resourcelimituiconfig.cpp selectresourceui.cpp spritecaptureui.cpp statusbarui.cpp statusiconui.cpp statusiconuiconfig.cpp storagecapacityui.cpp storageui.cpp thumb_vtables.cpp titledesctooltipui.cpp toolbar_utility_helpers.cpp toolbar_vtable_standalone.cpp toolui.cpp townhall_helpers.cpp townhallui.cpp tradeui.cpp tradeui_buyentries.cpp tradeui_checkout.cpp tradeui_leaves.cpp tradeui_price.cpp tradeui_sort.cpp ui_dialog_stub_factories.cpp ui_dialogs.h ui_panels.h unreg_lane2_mapparamsdialog_ctor.cpp upgradeui.cpp upgradeuiconfig.cpp workplaceui.cpp
- single-block `standalone/` registration files (6): reg_debugmenu.cpp reg_edittool_select.cpp reg_toolbar_utility.cpp reg_tradeui_leaves.cpp reg_unreg_lane2.cpp reg_unreg_lane3.cpp

### GOAL — 60 files (34 .cpp), 12497 lines; reflected classes (25): BuildingGoal, FinishTutorialGoal, FinishTutorialInstance, PopulationGoal, Scenario, StatGoal, TimeGoal, TimeGoalInstance, Tutorial, TutorialGoalButton, TutorialGoalCount, TutorialGoalDialog, TutorialGoalGameSpeed, TutorialHighlightDialog, TutorialImage, TutorialImageHotKey, TutorialImageInputKey, TutorialImageToolbarIcon, TutorialInstance, TutorialSetupCamera, TutorialSetupDialog, TutorialSetupGameSpeed, TutorialSetupRestrictPlayer, TutorialStep, TutorialUI
- `code/Game/Game/` (2): scenario.cpp scenario_funcs.h
- `code/Game/Goal/` (14): buildinggoal.cpp buildinggoal_funcs.h goal_exe_helpers.cpp goal_exe_helpers.h populationgoal.cpp populationgoal_funcs.h statgoal.cpp statgoal_funcs.h statvaluedispatch.cpp statvaluedispatch.h timegoal.cpp timegoal_funcs.h timegoalinstance.cpp timegoalinstance_funcs.h
- `code/Game/Tutorial/` (44): find_ui_element_by_path.cpp finishtutorialgoal.cpp finishtutorialgoal_funcs.h finishtutorialinstance.cpp finishtutorialinstance_funcs.h tutorial.cpp tutorial_base.cpp tutorial_dialog_vtables.cpp tutorial_funcs.h tutorial_next_scenario.cpp tutorial_wave.cpp tutorialgoal.cpp tutorialgoal_funcs.h tutorialgoalbutton.cpp tutorialgoalbutton_funcs.h tutorialgoalcount.cpp tutorialgoalcount_funcs.h tutorialgoaldialog.cpp tutorialgoaldialog_funcs.h tutorialgoalgamespeed.cpp tutorialgoalgamespeed_funcs.h tutorialhighlightdialog.cpp tutorialimage.cpp tutorialimage_funcs.h tutorialimagehotkey.cpp tutorialimagehotkey_funcs.h tutorialimageinputkey.cpp tutorialimageinputkey_funcs.h tutorialimagetoolbaricon.cpp tutorialimagetoolbaricon_funcs.h tutorialinstance.cpp tutorialinstance_funcs.h tutorialsetup.cpp tutorialsetup_funcs.h tutorialsetupcamera.cpp tutorialsetupcamera_funcs.h tutorialsetupdialog.cpp tutorialsetupgamespeed.cpp tutorialsetupgamespeed_funcs.h tutorialsetuprestrictplayer.cpp tutorialsetuprestrictplayer_funcs.h tutorialstep.cpp tutorialstep_funcs.h tutorialui.cpp
- single-block `standalone/` registration files (2): reg_tutorial_base.cpp reg_tutorial_wave.cpp

### LOOP — 97 files (79 .cpp), 39749 lines; reflected classes (12): ClimateGenerator, Disease, MapGenerator, MenuScene, MusicList, NameDatabase, Profession, ProfessionList, TerrainGenerator, Toolbar, Tornado, Weather
- `code/Game/Entity/` (2): entitylist.cpp entitylist_funcs.h
- `code/Game/Game/` (91): builder_pool_driver.cpp camera_input.cpp charmanager_seed.cpp citizen_names.cpp citizenlist_serialize.cpp climategenerator.cpp climategenerator_funcs.h construction_force_harness.cpp dialogloaderloop.cpp dialogloaderloop.h disease.cpp eventlog.cpp eventlog_achievement.cpp force_work.cpp force_work.h forester_force_harness.cpp founding_spawn.cpp game.cpp game.h game_leaf_helpers.cpp game_main.cpp game_main.h game_settings_persist.cpp gamegrid_serialize.cpp gameupdateloop.cpp gameupdateloop_load.cpp gameupdateloop_serialize.cpp gameupdateloop_teardown.cpp geometry_draw.cpp geometry_serialize.cpp goals_serialize.cpp inputbindings.cpp inputbindings.h keystone_a_gamedtor.cpp mapgenerator.cpp mapgenerator_funcs.h mapquery.cpp maptilegrid.cpp menudialogloopt.cpp menudialogloopt_funcs.h menuscene.cpp menuscene_funcs.h minimap_paint.cpp minimap_upload.cpp musiclist.cpp namedatabase.cpp pathfinder_seed.cpp profession.cpp profession_funcs.h professionlist.cpp professionlist_funcs.h region_seed.cpp serialize_arrays.cpp serialize_externals.cpp serialize_externals.h serialize_externals_array.cpp serialize_externals_typed.cpp serialize_heightmap_mesh.cpp serialize_image_upload.cpp serialize_resize_helpers.cpp serialize_struct60b.cpp serialize_struct60b_serialize.cpp starttown.cpp statemachine.cpp statemachine_funcs.h terraingenerator.cpp terraingenerator_funcs.h toolbar.cpp toolbar_funcs.h tornado.cpp town_storage_seed.cpp town_workplace_registry.cpp uifade.cpp uifade.h unreg_lane1_mapgenerator_dtorbody.cpp unreg_lane5_musicplayer.cpp w2_takescreenshot.cpp water_contour_mesh.cpp weather.cpp weather_funcs.h weather_particle_draw.cpp weather_particles.cpp weather_sim.cpp weather_sun.cpp worldgen.cpp worldgen_f390_coastline.cpp worldgen_f390_erosion.cpp worldgen_f390_ranges.cpp worldgen_f390_rivers.cpp worldgen_f390_shims.cpp worldsetup_reload.cpp
- `standalone/` (4): gameupdateloop_stubs.cpp loader_loop_stubs.cpp menudialogloopt_stubs.cpp splash_loop_stubs.cpp
- single-block `standalone/` registration files (12): reg_closeout_lane_c.cpp reg_code_lane1.cpp reg_code_lane5.cpp reg_entity.cpp reg_game_leaf.cpp reg_gamesettings.cpp reg_keystone_a_gamedtor.cpp reg_namedb.cpp reg_town_storage_seed.cpp reg_town_workplace_registry.cpp reg_town_workplace_registry_reader.cpp reg_w2_screenshot.cpp

### CIT — 76 files (56 .cpp), 31942 lines; reflected classes (19): AmbientEmitterComponent, AmbientEnvironmentComponent, AnimalComponent, CharacterComponent, CitizenAIComponent, CitizenComponent, DiseaseComponent, FireComponent, FruitComponent, GrowthComponent, HerdComponent, LivestockComponent, NaturalResourceComponent, ParticleComponent, PopulationComponent, SpawnComponent, TombstoneComponent, TornadoComponent, WildAnimalComponent
- `code/Game/Component/` (74): ambient_externalptr_serialize.cpp ambientemittercomponent.cpp ambientemittercomponent_funcs.h ambientenvironmentcomponent.cpp ambientenvironmentcomponent_funcs.h ambientenvironmentcomponent_serialize.cpp animalcomponent.cpp animalcomponent_funcs.h animalwave_serialize.cpp character_trig.cpp charactercomponent.cpp charactercomponent_funcs.h charactercomponent_serialize.cpp charmanager.cpp citizen_assign_citizens.cpp citizen_assignworkplace.cpp citizen_employ_cascade.cpp citizen_lifeevent.cpp citizen_lifeevent_handler.cpp citizen_needsupdate.cpp citizen_onremove.cpp citizen_reassign_workers.cpp citizen_updatehealth.cpp citizenai_actionctors.cpp citizenai_candidatelist.cpp citizenai_executor.cpp citizenai_findhelpers.cpp citizenai_findtarget.cpp citizenai_gather.cpp citizenai_growup.cpp citizenai_handlers.cpp citizenai_handlers2.cpp citizenai_handlers3.cpp citizenai_housing_idle.cpp citizenai_leisure.cpp citizenai_work_scheduler.cpp citizenaicomponent.cpp citizenaicomponent_funcs.h citizenaicomponent_serialize.cpp citizencomponent.cpp citizencomponent_funcs.h citizencomponent_lifecycle.cpp citizencomponent_serialize.cpp diseasecomponent.cpp diseasecomponent_funcs.h entity_finders_disaster.cpp firecomponent.cpp firecomponent_funcs.h firecomponent_ignite.cpp firecomponent_smstate.cpp fruitcomponent.cpp fruitcomponent_funcs.h fruitcomponent_serialize.cpp growthcomponent.cpp growthcomponent_funcs.h growthcomponent_serialize.cpp herdcomponent.cpp herdcomponent_funcs.h livestockcomponent.cpp livestockcomponent_funcs.h naturalresourcecomponent.cpp naturalresourcecomponent_funcs.h particlecomponent.cpp particlecomponent_funcs.h populationcomponent.cpp populationcomponent_funcs.h spawncomponent.cpp spawncomponent_funcs.h tombstonecomponent.cpp tombstonecomponent_funcs.h tornadocomponent.cpp tornadocomponent_funcs.h wildanimalcomponent.cpp wildanimalcomponent_funcs.h
- `code/Game/Entity/` (2): citizenbehavior.cpp citizenbehavior_funcs.h
- single-block `standalone/` registration files (25): reg_animal_update.cpp reg_charmanager.cpp reg_citizen_25.cpp reg_citizen_assignworkplace.cpp reg_citizen_employ_cascade.cpp reg_citizen_lifeevent_handler.cpp reg_citizen_needsupdate.cpp reg_citizen_updatehealth.cpp reg_citizenai_actionctors.cpp reg_citizenai_candidatelist.cpp reg_citizenai_executor.cpp reg_citizenai_findhelpers.cpp reg_citizenai_findtarget.cpp reg_citizenai_gather.cpp reg_citizenai_growup.cpp reg_citizenai_handlers.cpp reg_citizenai_handlers2.cpp reg_citizenai_handlers3.cpp reg_citizenai_housing_idle.cpp reg_citizenai_leisure.cpp reg_citizenbehavior_tick.cpp reg_code_lane4.cpp reg_fire.cpp reg_growth_spawn_serialize.cpp reg_tornado.cpp

### VIS — 45 files (26 .cpp), 10538 lines; reflected classes (23): AnimAudioEvent, AutoPickupComponent, CaptureBoxElement, DecalComponent, FloorComponent, FrustumOutlineElement, HighlightComponent, InteractComponent, MapComponent, MineComponent, ModelComponent, PickingComponent, QuarryComponent, RawMaterialComponent, StateMachineComponent, StatusIconComponent, StorageComponent, StorageDisplayComponent, TerraformComponent, TrackerComponent, UIComponent, WorkComponent, ZoneComponent
- `code/Game/Component/` (45): animaudioevent.cpp autopickupcomponent.cpp autopickupcomponent_funcs.h captureboxelement.cpp decalcomponent.cpp decalcomponent_funcs.h floorcomponent.cpp floorcomponent_funcs.h frustumoutlineelement.cpp highlightcomponent.cpp highlightcomponent_funcs.h interactcomponent.cpp interactcomponent_funcs.h mapcomponent.cpp mapcomponent_funcs.h minecomponent.cpp minecomponent_funcs.h modelcomponent.cpp modelcomponent_funcs.h pickingcomponent.cpp pickingcomponent_funcs.h quarrycomponent.cpp quarrycomponent_deform.cpp quarrycomponent_funcs.h rawmaterialcomponent.cpp rawmaterialcomponent_funcs.h statemachinecomponent.cpp statemachinecomponent_funcs.h statusicon_geometry.cpp statusiconcomponent.cpp statusiconcomponent_funcs.h storagecomponent.cpp storagecomponent_funcs.h storagedisplaycomponent.cpp storagedisplaycomponent_funcs.h terraformcomponent.cpp terraformcomponent_funcs.h trackercomponent.cpp trackercomponent_funcs.h uicomponent.cpp uicomponent_serialize.cpp workcomponent.cpp workcomponent_funcs.h zonecomponent.cpp zonecomponent_funcs.h
- single-block `standalone/` registration files (2): reg_component_gap_C.cpp reg_quarry_live.cpp

### MI-A — 42 files (28 .cpp), 12937 lines; reflected classes (14): AbsorbResourceComponent, AttendanceComponent, BoatComponent, BuildComponent, CemeteryComponent, ClearAreaComponent, ClearResourceComponent, DestroyComponent, HappinessComponent, HealerComponent, ImmigrationComponent, NomadComponent, ResidenceComponent, WellComponent
- `code/Game/Component/` (42): absorbresourcecomponent.cpp absorbresourcecomponent_funcs.h attendancecomponent.cpp attendancecomponent_funcs.h attendancecomponent_states.cpp boatcomponent.cpp boatcomponent_funcs.h boatcomponent_route.cpp boatcomponent_states.cpp buildcomponent.cpp buildcomponent_funcs.h buildcomponent_state1.cpp buildcomponent_state8.cpp buildcomponent_sub8630.cpp cemeterycomponent.cpp cemeterycomponent_funcs.h clearareacomponent.cpp clearareacomponent_funcs.h clearareacomponent_states.cpp clearresourcecomponent.cpp clearresourcecomponent_funcs.h clearresourcecomponent_states.cpp destroycomponent.cpp destroycomponent_funcs.h destroycomponent_state6.cpp destroycomponent_states.cpp destroycomponent_workerrelease.cpp happinesscomponent.cpp happinesscomponent_funcs.h healercomponent.cpp healercomponent_funcs.h immigrationcomponent.cpp immigrationcomponent_funcs.h immigrationcomponent_states.cpp nomadcomponent.cpp nomadcomponent_funcs.h nomadcomponent_states.cpp residence_heattick.cpp residencecomponent.cpp residencecomponent_funcs.h wellcomponent.cpp wellcomponent_funcs.h
- single-block `standalone/` registration files (6): reg_boat_ctrl.cpp reg_buildcomponent_sub8630.cpp reg_cemetery_healer_serialize.cpp reg_destroy_sm.cpp reg_immigration.cpp reg_town_services.cpp

### MI-B — 40 files (26 .cpp), 12783 lines; reflected classes (14): ConsumeProduceComponent, CropFieldComponent, FishingComponent, ForesterComponent, HerbalistComponent, HuntComponent, MarketComponent, MarketLocationComponent, MultiObjectComponent, OrchardComponent, PastureComponent, StorageLocationComponent, TradeComponent, WorkPlaceComponent
- `code/Game/Component/` (40): consumeproduce_recipe_dump.cpp consumeproducecomponent.cpp consumeproducecomponent_funcs.h cropfieldcomponent.cpp cropfieldcomponent_funcs.h cropfieldcomponent_states.cpp fishingcomponent.cpp fishingcomponent_funcs.h fishingcomponent_produce.cpp forestercomponent.cpp forestercomponent_funcs.h herbalistcomponent.cpp herbalistcomponent_funcs.h huntcomponent.cpp huntcomponent_funcs.h marketcomponent.cpp marketcomponent_funcs.h marketlocationcomponent.cpp marketlocationcomponent_funcs.h multiobjectcomponent.cpp multiobjectcomponent_funcs.h orchardcomponent.cpp orchardcomponent_funcs.h orchardcomponent_states.cpp pasturecomponent.cpp pasturecomponent_funcs.h pasturecomponent_spawn.cpp pasturecomponent_states.cpp resourceproduce_creatework.cpp resourceproducecomponent_states.cpp storagelocationcomponent.cpp storagelocationcomponent_funcs.h tradecomponent.cpp tradecomponent_dock.cpp tradecomponent_funcs.h tradecomponent_livestock.cpp tradecomponent_serialize.cpp tradecomponent_states.cpp workplacecomponent.cpp workplacecomponent_funcs.h
- single-block `standalone/` registration files (3): reg_food_typeid.cpp reg_orchard_growth.cpp reg_trade_serialize.cpp

### COORD — 5 files (3 .cpp), 761 lines; reflected classes (14): Action, AutoPickupWork, Component, ControllerComponent, Description, DragTool, ResourceProduceComponent, SelectResourceUI, StandaloneFactoryTest, Tool, ToolSetting, TutorialGoal, TutorialSetup, Work
- `code/Game/Object/` (5): factory_registration.cpp factory_registration.h factory_registrations_bulk.cpp internal_factory_audit.cpp internal_factory_audit.h

### Registration files that register ONLY exe data (type-id cells, float constants, loop tables defined in the same file) — 26
(The four `*_stubs.cpp` here register their own loop tables and are LOOP-owned — see LOOP above; the other 22 are coordinator-owned and retire at the END of B.3.)

gameupdateloop_stubs.cpp loader_loop_stubs.cpp menudialogloopt_stubs.cpp reg_citizenai_typeids.cpp reg_cleararea_typeids.cpp reg_component_animal.cpp reg_component_fruit.cpp reg_component_spawn.cpp reg_component_tombstone.cpp reg_controlui.cpp reg_economy_typeids.cpp reg_element_typeids.cpp reg_growth_typeid.cpp reg_herd_live.cpp reg_market_typeid.cpp reg_orchard_typeid.cpp reg_pipeline_typeids.cpp reg_rdata_float_consts.cpp reg_tracker_typeid.cpp reg_tradeui_typeids.cpp reg_tutorial_typeid.cpp reg_unreg_lane4.cpp reg_w2_lane1.cpp reg_w2_lane2.cpp reg_w2_lane3.cpp splash_loop_stubs.cpp

### Registration files spanning more than one block — 28 (coordinator-owned; the count is registrations per block)

| file | rows per block |
|---|---|
| `standalone/address_registry.cpp` | LOOP 46, DLG 4, K1 4, local/data 4, CIT 2, VIS 1, PNL 1 |
| `standalone/reg_component_economy.cpp` | MI-A 16, MI-B 15, VIS 10, CIT 1 |
| `standalone/reg_component_nature.cpp` | CIT 9, MI-B 9, local/data 7, VIS 6, MI-A 1 |
| `standalone/reg_component_people.cpp` | CIT 20, MI-A 2, VIS 1 |
| `standalone/reg_component_cropdisease.cpp` | CIT 16, LOOP 3, local/data 2 |
| `standalone/reg_trade_ctrl.cpp` | MI-B 14, MI-A 5 |
| `standalone/reg_component_gap_A.cpp` | CIT 10, local/data 4, MI-A 3 |
| `standalone/reg_pasture_live.cpp` | MI-B 14, CIT 2, local/data 1 |
| `standalone/reg_townhallui.cpp` | PNL 9, CIT 6 |
| `standalone/reg_component_gap_B.cpp` | MI-A 9, CIT 3, VIS 1, DLG 1 |
| `standalone/reg_component_gap_D.cpp` | MI-B 7, CIT 3, MI-A 1 |
| `standalone/reg_animal_serialize.cpp` | CIT 5, local/data 3, MI-B 1 |
| `standalone/reg_mods.cpp` | DLG 6, LOOP 1, local/data 1 |
| `standalone/reg_citizen_25_tick.cpp` | ACT 2, CIT 2, LOOP 2 |
| `standalone/reg_terraform_live.cpp` | VIS 4, local/data 1, MI-B 1 |
| `standalone/reg_code_lane2.cpp` | PNL 3, DLG 2 |
| `standalone/reg_code_lane6.cpp` | VIS 2, TOOL 1, DLG 1, local/data 1 |
| `standalone/reg_component_visuals.cpp` | VIS 2, K1 1, CIT 1, MI-A 1 |
| `standalone/reg_goal.cpp` | GOAL 3, PNL 1 |
| `standalone/reg_tradeui_checkout.cpp` | PNL 3, MI-B 1 |
| `standalone/reg_closeout_lane_d.cpp` | LOOP 2, VIS 1 |
| `standalone/reg_destroy_workers.cpp` | MI-A 2, MI-B 1 |
| `standalone/reg_eventlog_citizen.cpp` | LOOP 2, CIT 1 |
| `standalone/reg_ambientenv_ui_serialize.cpp` | CIT 1, VIS 1 |
| `standalone/reg_citizen_onremove.cpp` | CIT 1, MI-A 1 |
| `standalone/reg_fruit_happiness_serialize.cpp` | CIT 1, MI-A 1 |
| `standalone/reg_player_place.cpp` | PNL 1, TOOL 1 |
| `standalone/reg_unreg_lane1.cpp` | PNL 1, LOOP 1 |
