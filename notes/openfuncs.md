# Open Funcs — Standalone Reimpl Scope Snapshot

*What's left in the standalone (`reimpl-x64.exe`) build: exe functions still resolving to
NULL, factories still stubbed, and `.rdata` constants/type-ids still unmapped.*

**Generated:** 2026-07-18. This is a **living snapshot**, not a spec. Numbers come from
(a) a real autostart run's `standalone_debug.log`/`standalone_error.log` and (b) a static
sweep of `standalone/*.cpp` + `standalone/factory_registrations.csv`. Re-derive with the
commands in the appendix after any registration wave.

## How to read this

In standalone mode, `ResolveAddr(Module::Exe, addr)` looks up an address registry
(`standalone/*.cpp` `Register*` waves). **Unregistered addresses resolve to 0** and log
`[STANDALONE] NOT IMPLEMENTED: exe function at 0x…`. Most callers null-guard, so a NULL is
usually a *deliberate safe no-op*, not a crash — but each one is a gap between us and a
faithful port. The four buckets below are the four ways something is still "open":

1. **Deferred / no-op code functions** — address known, body sometimes written, deliberately
   left unregistered (closure not satisfied, recursion risk, or unexercised path).
2. **Unmapped / gated constants & type-id globals** — `.rdata` values (`DAT_1401cxxxx`,
   float consts) that read 0 / `0xFFFFFFFF` unless a live-feature env gate is on.
3. **Null factories** — `.rsc`-driven `Object::Factory` types registered as `NullFactory`
   (log-and-return-null); no real factory yet.
4. **Loop-stub slots** — SplashDLT/MenuDLT/GameUpdateLoop/LoaderLoop vtable slots still
   print-only, literal no-op, or forwarding to a Runtime-DLL thunk.

---

## 0. Runtime-verified scope (today's autostart run)

**342 unique unregistered exe addresses were actually *hit*** during one menu→New Game→in-game
run. This is the concrete "exercised gap" — everything else in the factory table below is
dormant until its `.rsc` type is instantiated.

| Bucket | Count | Range | Meaning |
|--------|------:|-------|---------|
| Code functions | 138 | `0x140001xxx–0x14014Fxxx` | Real reimpl TODOs (§1) |
| Dispatch slots | 98 | `0x14015xxxx` | `.rdata` IAT slots for Runtime fn ptrs / Math ops (§1.6) |
| Type-id globals | 73 | `0x1401cxxxx` | Mostly gated-off live components (§2) |
| Float/rdata consts | 18 | `0x140172xxx` | Sim tuning constants (§2) |
| Vtable/typeinfo | 15 | `0x14015Exxx–0x14016xxxx` | Class vtables / type info |

The run reached in-game with **zero `[NullFactory]` hits** — the factory cascade (§3) is
fully covered for the current autostart path; the 296 null factories are latent.

---

## 1. Deferred / no-op code functions (the real TODOs)

Grouped by module. "STALE" = the file's own note says the defer has since been resolved by a
later wave (kept here only so the note isn't mistaken for open work). Addresses quoted only
where they literally appear in the source.

### Action
| Addr | Symbol | File | Why deferred |
|------|--------|------|--------------|
| `0x14009bd40` | FindPlacementCell | reg_action_rawmat.cpp:18 | 260-insn spiral, unexercised citizen-action path; own pass |
| `0x1400c7570`, `0x1400c7620` | DropOffRawMaterial | reg_action_complete.cpp:21 | closure pulls unregistered `c78c0` spawn / MoveTo ctor `c9170` / task-append `c54e0` |
| `0x14009c640` | ActHunt_CompleteRoadTile | reg_action_hunt.cpp:30 | STALE — registered by reg_pf_roots_road as `Pf_FinalizeRoadTile_9c640`; duplicate body retired 2026-07-19 |
| `0x1400bbea0` | RemoveAnimal_Action_Ctor | reg_action_hunt.cpp:42 | 609-insn builder; child actions via unregistered CopyCtors |

### CitizenAI / Component
| Addr | Symbol | File | Why deferred |
|------|--------|------|--------------|
| `0x14012ab00` + 14 `CitizenAI_State_*` | candidate handlers | reg_citizenai_candidatelist.cpp:12 | STALE — all 14 handlers registered (reg_citizenai_handlers/2/3, gather, growup, leisure, housing_idle); 0x14012ab00 is Storage_GetResourceCount2 (Gather2 helper, reg_citizenai_gather.cpp:24), not a 15th handler. Audited 2026-07-19 |
| `0x140025a10`,`0x140025b30`,`0x1400d62a0`,`0x1400d43b0`,`0x140095580` | PathBST/PathPairs/ReassignWorkers/AssignCitizens/NotificationEvent | reg_citizenbehavior_tick.cpp:27 | fire as dispatcher-guarded no-ops |
| `0x14010fde0`,`0x1401117a0`,`0x140111540` | needs-msg 1/4/5 helpers | reg_citizen_needsupdate.cpp:16 | dispatcher resolves NULL, skips with `[EatMsg]` trace |
| `0x140106420` | AnimalComponent::Update | reg_component_nature.cpp:58 | still a self-resolving passthrough — registering = infinite recursion. Needs genuine 452-insn reimpl |
| `0x1400302a0`,`0x140112040`,`0x1401168a0`,`0x140117080` | citizen lifecycle Level-2 | reg_component_gap_A.cpp:15 | call unregistered sub-callees (AssignWorkplace `1163d0` etc.); ~10 known Level-2 defers |
| `0x14010dca0` | BuildCandidateStateList | reg_component_gap_B.cpp:19 | STALE — registered by reg_citizenai_candidatelist.cpp:32 with all helpers (GetHappiness 115de0 / HasSufficientResources[2] / SortByPriority). Audited faithful 2026-07-19 |
| `0x14010f1f0` | ValidateAndEnterState | reg_component_gap_B.cpp:19 | STALE — registered by reg_citizenai_candidatelist.cpp:33; all 14 handlers live. Audited faithful 2026-07-19 |
| `0x140047200` | CemeteryComponent::Draw | reg_component_gap_B.cpp:19 | self-resolving passthrough → recursion |
| `0x140102b60` | NomadComp_RebuildController | reg_component_gap_B.cpp:51 | Factory::CreateObject + controller-vtable Startup; crashes on `g_ToolUI_ChildDialog_Vtable 0x14015f5a0` |

### Player tools (largest cluster — mostly unexercised on autostart)
| Addr | Symbol | File | Why deferred |
|------|--------|------|--------------|
| `0x1400fcc90` | Entity_impl_cc90 (deselect) | reg_player_edit.cpp:32 / edit2.cpp:7 / render.cpp:44 | deep unreg closure; also per-frame EditTool deactivate slot → must stay no-op |
| `0x140066800` | EditTool_helper_6800 | reg_player_edit.cpp:32 | STALE — registered by reg_player_trade (r2c; `2e30`/`c690` were typos for `102e30`/`2c690`) |
| `0x1401005a0` | camera mouse ray | reg_player_edit.cpp:32 | STALE — registered by reg_player_ray (r2; exports resolved locally) |
| `0x1400aa530` | QuarryComponent RaycastTerrain | reg_player_path.cpp:13 | pointer-aliased float march; high-risk, not exercised |
| `0x1400dd710` | PathTool SetSelectedEntity | reg_player_path.cpp | ~520-line AVL builder w/ ~12 unreg sub-callees |
| `0x1401035e0` | RemovalTool::CalcPath | reg_player_path.cpp | ~770-line fn, ~15 unreg sub-fns (see removaltool.cpp) |
| `0x140055430` | HighlightAdd | reg_player_place.cpp:43 | → unregistered `b0d80`+`ace40` |
| `0x14009ce00` | RoadTool sub_ce00 | reg_player_place.cpp:43 | → `a6e30` (unwired System::Thread / half-populated road records) |
| `0x14015f5a0` | g_ToolUI_ChildDialog_Vtable | reg_player_toolui.cpp:27 | registering crashes menu at startup (BuildId child). **Intentionally counted as unregistered forever** |

### Work
| Addr | Symbol | File | Why deferred |
|------|--------|------|--------------|
| `0x14009c480` | ZoneRefresh | reg_work_accessor.cpp:23 | calls unreg flood-fill `9c170`. STALE (landed in reg_pf_roots_road) |
| `0x1400af370` | road-mesh rebuild | reg_pf_roots_road.cpp:7 | ports call via ResolveAddr → safe no-op until own pass |
| — | MarketWork Slot4 + caller-bug BST, action ctors, copy-ctor closures | reg_work_leaf.cpp:7, reg_work_impl2.cpp:4 | half-null/drifted port = silent corruption; safe-no-op skip preferred |

### Misc / scaffolding
- `address_registry.cpp:1335` — `g_NoiseUI_ImageBufferVtable_Standalone` slots [1]–[8] are
  safe-no-op `ReturnNull` stubs (only [0] dtor + [9] Create real).
- `address_registry.cpp:1678` — `RegisterGameVtables` still has `// TODO: define static
  vtable arrays for each game class`.
- `address_registry.cpp:659` — EditTool select teardown/refresh `fcc90`/`fc9e0` deferred.

### ✔ Cross-file inconsistencies — RECONCILED 2026-07-19
Audited and resolved (comments aligned across waves; no double-registrations existed —
`RegisterExeAddr` is silent last-wins, but each address had exactly one registration site):
- `0x14009c640` — **registered** by reg_pf_roots_road (`Pf_FinalizeRoadTile_9c640`); the
  stale defer note in reg_action_hunt updated and the unregistered duplicate body
  `ActHunt_CompleteRoadTile_c640` retired from action_base_hunt.cpp.
- `0x1400fcc90` — **deliberately unregistered, permanently** (per-frame EditTool
  deactivate slot would wipe the selection outline; reg_player_render.cpp is canonical).
  Stale closure-based defer notes in reg_player_edit/edit2 rewritten; reg_edittool_select
  wrongly claimed the render batch registers it — corrected.
- `0x140100380` — **registered** by reg_player_screenrect (full closure ec70/098210/fe960;
  Math via ResolveExport). Defer notes in reg_player_edit/edit2 marked resolved.
- `0x1400ff9e0` — **registered** by reg_player_toolui (self-sufficient static child-dialog
  vtable; exe vtable 0x14015f5a0 deliberately unregistered forever). Stale notes fixed in
  reg_player_drag, reg_player_toolui's own header, and address_registry.cpp:639.

Bonus staleness fixed in the same pass: reg_player_edit's defers `0x140066800` (registered
by reg_player_trade) and `0x1401005a0` (registered by reg_player_ray); reg_player_drag's
defers `0x1400ffc60` (reg_player_createbar) and `0x140101270` (reg_player_ray); both
Level-1 caller-bug literals (`0x14010a530`, `0x1400ff5d0`) were already fixed in
edittool.cpp/dragplacementtool.cpp — notes updated to say so.

### ✔ CitizenAI Level-2 audit — DONE 2026-07-19 (openfuncs step 2)
The §1 CitizenAI rows above were stale: BuildCandidateStateList, ValidateAndEnterState, all
14 state handlers, and the candidate-list helpers were already registered and A/B-green
(un-gated 2026-06-20). Four parallel decomp-vs-port audits verified every function faithful
offset-by-offset. Findings:
- **Real bug fixed**: `CitizenAI_State_LongTermPlan` port used a 16-byte stride in the
  candidate-array grow/append/dispatch vs the correct 8-byte stride in alloc/search/retail —
  dedup break + 8-byte heap OOB write at the 4th candidate (citizenai_growup.cpp, fixed).
- **Benign divergence documented**: Leisure's zone-refresh call passes different args than
  retail; proven dead (callee ignores/dead-stores them).
- **Ghidra renames applied** (with plate-comment evidence): `AdultTransition`→`CitizenAI_State_Eat`
  (hunger +0x44 vs desc +0xcc), `ElderTransition`→`CitizenAI_State_Drink` (thirst +0x48 vs
  desc +0xec), `GetHappiness` 115de0→`CitizenComponent_GetCarryCount` (Storage QueryCount,
  gates state 0xd haul planning).
- Stale defer comments rewritten in reg_citizenai_candidatelist.cpp and reg_component_gap_B.cpp.
The remaining open item from priority #2 is the `c70e8`/`c7158`/`c42c0` global fill (§2).

*Note: several other agent-flagged defers are STALE (already resolved by a later wave):
`0x140129760`/`0x140088ef0` (rawmat), `0x140122340` (herd migration), `0x140092260` (spatial),
`0x1400a8ba0` (region path), `0x140111840` (TryFindPartner), workplace Attach/Detach
`0x14012f7e0`/`0x14012fa20`. Verify before acting.*

---

## 2. Unmapped / gated constants & type-id globals

These `.rdata` globals read 0 / `0xFFFFFFFF` unless their file registers + fills them. Two are
**registration-gated** (publish nothing when the env gate is OFF); the rest register
unconditionally but their *runtime effect* is gated at call sites.

### Registration-gated (publishes NOTHING when env OFF)
| Addr | Type-id | Gate | Consumer | File |
|------|---------|------|----------|------|
| `0x1401c7468` | CreatePlacedDescription | `BANISHED_LIVE_DECAL` | StartTown 1fc0 pipeline | reg_pipeline_typeids.cpp:42 |
| `0x1401c7a50` | ModelComponent | `BANISHED_LIVE_DECAL` | shared `FindComponent(ModelComponent)`; decal-bug reader | reg_pipeline_typeids.cpp:43 |
| `0x1401c7a88` | ModelDescription | `BANISHED_LIVE_DECAL` | StartTown 1fc0 downstream finders | reg_pipeline_typeids.cpp:44 |
| `0x1401c4918` | SpawnDescription | `BANISHED_LIVE_SPAWN` | SpawnComponent Update/Startup; TileFree `120c30` | reg_component_spawn.cpp:29 |
| `0x1401c77e8` | GrowthDescription | `BANISHED_LIVE_SPAWN` | SpawnComponent spawn-interval template | reg_component_spawn.cpp:30 |

### ~~Intentionally never mapped~~ → registration-gated as of 2026-07-19 (openfuncs step 2)
| Addr | Retail identity (corrected) | Gate | File |
|------|----------------------------|------|------|
| `0x1401c70e8` | CitizenAIComponent BaseTypeInfo+0 (`CitizenAIComponent_RegisterType` 0x14014e735) | `BANISHED_LIVE_CITIZENAI_TIDS` or `_AIID` | reg_citizenai_typeids.cpp |
| `0x1401c7158` | **CitizenComponent** BaseTypeInfo+0 (`CitizenComponent_RegisterType` 0x14014e7b5) — NOT CitizenAIComponent as older notes claimed | `BANISHED_LIVE_CITIZENAI_TIDS` or `_CITID` | reg_citizenai_typeids.cpp |
| `0x1401c42c0` | HappinessComponent BaseTypeInfo+0 (`HappinessComponent_RegisterType` 0x14014ace5) | `BANISHED_LIVE_CITIZENAI_TIDS` or `_HAPPYID` | reg_citizenai_typeids.cpp |

(The gate is deliberately NOT the pre-existing `BANISHED_LIVE_CITIZENAI` — `make sa-run` sets
that one, and the fills must stay opt-in on the standard test path.)

These were never "msg-ids": all three are `Object::BaseTypeInfo` base slots (0x70 bytes
each), and the SendComponentMessage action ctor (0x1400cb1d0) copies exactly one dword —
the arg is a component-TYPE selector for message delivery. With the gate OFF (default)
nothing is registered and every reader keeps its null-guarded skip. With it ON, the fill
runs post-Factory-Startup from game_main.cpp (`FillTypeIdGlobals_citizenai`). The 2026-07-19
reader sweep found every Action/Work-side `c7158` consumer is Serialize/Validate-only
(autostart-inert, save/load-correct); the SendComponentMessage ctor sites construct actions
that carry the real target-type id — receive-side maturity is what the A/B gate probes.
Bonus fix in the same pass: reg_work_tail_trade's private `c8a0` copy filled the WRONG id
("CitizenAIComponent" → corrected to "CitizenComponent"); Ghidra renames
`Entity_FindComponent_CitizenAI`→`Entity_FindComponent_Citizen`, labels
`g_BaseTypeInfo_{CitizenAIComponent,CitizenComponent,HappinessComponent}`.

### Registered unconditionally, but runtime-behavior gated at call sites
- `0x1401c7c80` RawMaterialComponent → ClearArea::Execute Phase 3, gated `BANISHED_LIVE_CLEARCTRL`
  (reg_economy_typeids.cpp:135)
- `0x1401c74a0`/`0x1401c75f0`/`0x1401c6860` (CreateRemovalDescription/DestroyComponent/BuildRoad)
  → player removal path only (RemovalTool::CalcPath `0x1401035e0`), reg_economy_typeids.cpp:145
- Storage seeder `0x14012d230`/`0x140107ff0`/`0x140091660`/`0x1400a1f00` gated
  `BANISHED_LIVE_STORAGE` (reg_town_storage_seed.cpp)

The other ~20 economy type-ids (reg_economy_typeids.cpp) + fruit/tombstone/nature type-ids
register unconditionally and are only 0 in the *absence* of their file — they are covered.

### `.rdata` float sim-constants
All float consts (`0x140172xxx`) found in `standalone/` are registered **unconditionally** (no
gate, no missing note) across reg_citizen*/reg_component_*/reg_action_move/reg_work_tail_fire.
The 18 float addresses flagged NOT-IMPLEMENTED in §0 belong to the gated-off live-component
paths (they get filled once the owning component's env gate turns on). The in-game loop's
1.0 / audio-time-scale / camera floats are wired by `RegisterGameUpdateLoopLoadAddrs()`
(body in `code/Game/Game/gameupdateloop_load.cpp`, outside `standalone/`) — no gap there.

---

## 3. Null factories (296 of 310 `.rsc` types)

`standalone/factory_registrations.csv` holds 310 exe `Object::Factory` registrations; only
**14** have a real factory (the `REIMPLEMENTED` set in `tools/gen_factory_registrations.py`:
StatGoal, TimeGoal, BuildingGoal, PopulationGoal, TimeGoalInstance, FinishTutorialGoal,
FinishTutorialInstance, MainMenuDialog, LoaderDialog, MenuScene, Weather, ClimateGenerator,
MapGenerator, TerrainGenerator). The other **296 log-and-return-null**.

**Big caveat:** a null *factory* ≠ not reimplemented. Most of these types already have a
registered vtable + reimpl (the `Register*DescriptionVtable`/`Register*Vtable` cascade in
`address_registry.cpp`) — they just lack the `.rsc`-driven instantiation factory. A factory is
only needed when the loaded `.rsc` actually constructs that type via `External::Load`. Today's
run hit **zero**, so this whole list is latent; prioritize by what a given `.rsc` instantiates.

Counts by parent group (full type lists via the appendix script):

| Parent group | # | Parent group | # |
|--------------|--:|--------------|--:|
| Description | 78 | Component | 40 |
| UI::DialogController | 35 | ControllerComponent | 23 |
| Action | 17 | Work | 16 |
| UI::DialogInstance | 16 | Object::External | 14 |
| Object::Internal | 7 | ResourceProduceComponent | 6 |
| AutoPickupWork | 6 | DragTool | 5 |
| Tool | 4 | TutorialGoal | 4 |
| TutorialSetup | 4 | UI::DialogControllerConfig | 4 |
| SelectResourceUI | 3 | TutorialImage | 3 |
| Element/ElementDescription | 2+2 | others (1 each) | 11 |

Single-type groups: Scenario, DebugUI, AnimAudioEvent, Tornado, OptionInputBindDialog,
RotationSetting, OptionsDialog.

---

## 4. Loop-stub slots still incomplete

Vtable slots that are print-only / literal no-op / forward to a Runtime thunk (not a faithful
port). REAL slots omitted. `⊘` = retail-accurate no-op (complete as-is).

### SplashDLT — DialogLoaderLoopT (vtable `0x140158800`), splash_loop_stubs.cpp
- `Stub_DrawFailed` +0x28 (thunk `0x14003A90C`) — print-only → port video-lost draw
- `Stub_Load` +0x30 (thunk `0x14003A8E8`) — **forwards** to `fn_DialogLoaderLoopT_Load`
- `Stub_SaveRestore` +0x38, `Stub_OnVideoError` +0x40, `Stub_OnVideoRestart` +0x48 — print-only
- `Stub_Destroy` +0x00 (`0x140001010`) — print + free, skips dtor unwind (accepted leak)
- `Stub_Shutdown` +0x10 (`0x140098F90`) — ⊘ retail is literal RET 0

### MenuDLT — MenuDialogLoopT (vtable `0x14015ed00`), menudialogloopt_stubs.cpp
- `Stub_Shutdown` +0x10 (`0x14009B280`) — print → port teardown
- `Stub_DrawFailed` +0x28, `Stub_SaveRestore` +0x38 — print-only
- `Stub_OnVideoError` +0x40 (`0x14009A8D0`), `Stub_OnVideoRestart` +0x48 (`0x14009A8B0`) — print
- `Stub_Load` +0x30 (`0x14009A930`) — functional but still forwards dialog-rsc to `fn_DialogLoopT_Load`
- `Stub_Destroy` +0x00 (`0x14001A2E0`) — print + free, skips dtor unwind

### GameUpdateLoop (vtable `0x14015e2f0`), gameupdateloop_stubs.cpp
- `GUL_SaveRestore` +0x38 (`0x1400959c0`, impl `0x140095a90`) — no-op; **needed before load-from-save (M5+)**
- `GUL_OnVideoError` +0x40 (`0x140095970`), `GUL_OnVideoRestart` +0x48 (`0x140095930`) — no-op (GPU buffer release/recreate)
- `GUL_DrawFailed` +0x28 (`0x140096000`) — ⊘ retail RET 0
- `GUL_GetResourceFilename` +0x50 (`0x140019d40`) — **DEVIATES**: returns null (retail = "Game/Game") to avoid non-terminating preload; needs FindOrLoad dedup fix
- ⚠ Doc mismatch: file header still calls `Load` +0x30 a "Stage-C stub" but the slot is wired
  to the real `GameUpdateLoop_Load` world-build (line 146).

### LoaderLoop inner (`0x140158870`) / outer (`0x1401588d0`), loader_loop_stubs.cpp
Almost entirely stub/thunk except the two constant getters:
- Inner/Outer `Startup` +0x08 — **forward** to Runtime `fn_Loader_Load` + WaitForFinish
- Inner/Outer `Update` +0x18 — print + standalone SetNextLoop (not a real Update gate)
- Inner `Load` +0x30 (`0x140001120`) — no-op, skips `PlacementTool_Ctor(KeyMap, resourceList)`
- Shutdown/Draw/DrawFailed/SaveRestore/OnVideoError/OnVideoRestart — all print-only
- Destroy (inner `0x140001150` / outer `0x140001190`) — print + free

---

## 5. Suggested priority order

1. **Reconcile the 4 cross-file address conflicts** (§1) — cheap, removes latent double-register / stale-defer bugs.
2. **CitizenAI Level-2 closure** — ✔ MOSTLY DONE (audited 2026-07-19): `10dca0`/`10f1f0` +
   all 14 `CitizenAI_State_*` handlers were already registered (citizenai waves) and verified
   faithful against fresh decomp; one real bug found+fixed (LongTermPlan arrA stride,
   citizenai_growup.cpp). Ghidra renames applied: AdultTransition→Eat, ElderTransition→Drink,
   GetHappiness→GetCarryCount. REMAINING: the deliberate global fill of msg-id/type-id
   `.rdata` slots `c70e8`/`c7158`/`c42c0` (see §2 warning; dual-role readers — Serialize
   type-checks want the CRC, work-action SendComponentMessage ctor sites currently rely on 0).
3. **`AnimalComponent::Update` `0x140106420`** — remove the recursion-passthrough with a genuine reimpl.
4. **`GUL_SaveRestore` `0x1400959c0`** — the one loop slot blocking load-from-save (everything else new-game-only).
5. **Player-tool closures** (RemovalTool `1035e0`, PathTool `dd710`, ToolUI `f5a0`) — large but
   only gate player interaction, not the sim; defer until the sim path is solid.
6. **Null factories** — only as each `.rsc` path demands them (enable `BANISHED_FACTORY_ENUM`
   to enumerate a specific load's needs; see §3 caveat).

---

## Appendix — regenerate this inventory

```bash
# Runtime scope (after an autostart run):
grep -h "NOT IMPLEMENTED: exe function" "/c/GOG Games/Banished"/standalone_{error,debug}.log \
  | grep -oE "0x[0-9a-f]+" | sort -u          # 342 hit addrs; bucket by range

grep -h "\[NullFactory\] Hit for type:" "/c/GOG Games/Banished"/standalone_{error,debug}.log \
  | sed 's/.*type: //' | sort -u              # factories actually instantiated

# Static factory-null list grouped by parent:
python tools/gen_factory_registrations.py     # REIMPLEMENTED set = the 14 done; CSV = all 310

# Deferred/no-op markers in the registry:
grep -rniE "DEFER|no-op|not registered|placeholder|can't be registered" standalone/
```
