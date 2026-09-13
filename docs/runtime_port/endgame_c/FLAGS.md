# BANISHED_* environment flag inventory (code/, runtime/, dx11/)

Read-only inventory for ENDGAME.md section C. Scope: every `BANISHED_*` token read in
`code/`, `runtime/`, `dx11/` (standalone/ is owned by another agent and excluded here).
Generated 2026-09-13 by grepping `code runtime dx11` for `BANISHED_[A-Z0-9_]*`, then
checking `kDefaultOnFlags` in `code/Game/Component/component_factory_base.cpp`, the raw
read style at each call site, and setters in Makefile/tools//perf//.vscode/launch.json/
CLAUDE.md/docs/runtime_port/{STATUS,DIRECTION,ENDGAME,PLAN}.md.

No compile-time `#if`/`#ifdef BANISHED_*` macros were found anywhere in this scope
(the `BANISHED_PERF_EXPERIMENTAL`-style example in the brief does not exist here) --
every flag below is a runtime `getenv`/`LiveEnabled` read.

**Five raw grep hits were prose, not real flags**, and are dropped from the inventory
entirely (not counted below): `BANISHED_LIVE_*`, `BANISHED_ORCHARD_*`, `BANISHED_FORCE_*`,
`BANISHED_BUILD_DEMO_*`, `BANISHED_DX11_*` all appear verbatim in comments as a wildcard
reference to the family (e.g. `component_factory_base.cpp:174`, `starttown.cpp:1340`,
`dx11_seam.h:112`) -- the trailing `*` is not part of an identifier so the grep regex
captured the bare prefix as if it were its own env var. No such literal-named variable
is ever passed to `getenv`/`LiveEnabled`.

## Tier counts

| Tier | Count | Meaning |
|---|---|---|
| GRADUATE | 96 | In `kDefaultOnFlags`, or a raw-getenv kill-switch whose default is ON -- gate deleted, body kept unconditionally |
| OPT-IN | 18 | `LiveEnabled`-gated, NOT in `kDefaultOnFlags`, dormant unless set or `BANISHED_LIVE_COMPONENTS` |
| KEEP-T3 | 67 | Has a setter in Makefile/tools//perf//.vscode/launch.json/CLAUDE.md/docs -- documented harness/world-setup/perf/log-control route |
| DELETE | 282 | No setter found anywhere in the documented locations -- default: delete |
| **Total real flags** | **463** | (468 raw grep hits minus 5 prose wildcard artifacts) |

A caveat on the DELETE count: several `BANISHED_BUILD_DEMO_<Building>` and
`BANISHED_FORCE_<Disaster>` flags not individually found in a tool/doc setter (e.g.
`BUILD_DEMO_CEMETERY/CHAPEL/FISHINGDOCK/FORESTERLODGE/GATHERERHUT/HOUSE/HUNTERLODGE/
MARKET/PASTURE/SCHOOLHOUSE/TAVERN/TRADINGPOST`, `FORCE_BIRTH/CONSTRUCT/DEVICE_LOST/
FORESTERWORK/HARVEST/LOD0/SNOW/WORK`) sit in the exact same family as siblings that ARE
KEEP-T3 (`BUILD_DEMO_QUARRY/HOSPITAL/WELL/CROPFIELD/ORCHARD/INDEX`, `FORCE_INFEST/SICK/
SPEED/TORNADO`). ENDGAME.md §C.4 says this whole family "stays"; this pass tiers strictly
on found setters per the brief, so these are listed DELETE below but a follow-up batch
should treat the family as one decision, not delete siblings inconsistently.

## OPT-IN (detailed) -- 18 flags

`LiveEnabled("BANISHED_...")`-gated, not in `kDefaultOnFlags`, dormant unless the flag or
the master `BANISHED_LIVE_COMPONENTS` is set. This is the section a graduation decision
actually needs to be made about.

### `BANISHED_AUDIO_AUDIT`
- Readers: `code/Game/Game/gameupdateloop.cpp`
- Diagnostic, not a feature: one-shot scan of the 2D sound-effect table for a null entry (regression hunt for a load-restore bug). Off = never runs (must not run by default per its own comment -- the scan pattern only makes sense once per session).

### `BANISHED_AUDIO_PROBE`
- Readers: `code/Game/Game/gameupdateloop.cpp`
- Diagnostic: loads ambient sound resources directly to isolate a load path. Off = skipped.

### `BANISHED_CLEAR_FOOTPRINT`
- Readers: `code/Game/Component/clearareacomponent.cpp`
- Feature: de-inert ClearAreaComponent so it clears its building's footprint automatically (Track A step 2), no harness needed. Off = footprint clearing stays on the older harness-driven path.

### `BANISHED_LIVE_ANIMALS`
- Readers: `code/Game/Component/wildanimalcomponent.cpp`
- One of four gates (with LIVE_HERD/LIVE_POPULATION/LIVE_ANIMAL_UPDATE, any-of OR'd) for WildAnimalComponent's live vtable slot 3 (Serialize). Off (all four unset) = animal serialize slot stays uninstalled.

### `BANISHED_LIVE_AUTOSAVE`
- Readers: `code/Game/Game/gameupdateloop.cpp`
- Feature: creates and adds the AutoSave dialog in the game update loop. Off = no autosave dialog is created.

### `BANISHED_LIVE_BUILD_ONREMOVE`
- Readers: `code/Game/Component/buildcomponent.cpp`
- Feature: BuildComponent's OnRemove hook, needed only for a construction site torn down before completion (state8 already handles the normal completion removal). Off = a cancelled/demolished foundation leaves a hole (component doesn't clean up).

### `BANISHED_LIVE_CEMETERY`
- Readers: `code/Game/Component/cemeterycomponent.cpp`
- Feature: CemeteryComponent live vtable wiring. Off = component stays inert (see reference_vtable_slot3_serialize_never_installed for the shared serialize-slot caveat across several LIVE_* component gates).

### `BANISHED_LIVE_CITIZEN_FAMILY`
- Readers: `code/Game/Component/citizencomponent.cpp`, `code/Game/Game/starttown.cpp`
- Feature: the marriage/partner-linking block on CitizenComponent (sets partner pointers both ways). Off = marriage block stays a dead no-op (declared, null-checked, never resolved).

### `BANISHED_LIVE_CITIZEN_POSTLOAD_TOOL`
- Readers: `code/Game/Component/citizencomponent_lifecycle.cpp`
- Known deviation, explicitly opt-in: a post-load tool-reset behavior on CitizenComponent lifecycle. Off = retail-faithful (no post-load reset).

### `BANISHED_LIVE_CROPDISEASE`
- Readers: `code/Game/Component/diseasecomponent.cpp`, `code/Game/Component/diseasecomponent_funcs.h`
- Feature: crop disease infestation/spread/kill timers on DiseaseComponent. Off = crops never get infested by this path.

### `BANISHED_LIVE_FOUNDING_SPAWN`
- Readers: `code/Game/Game/founding_spawn.cpp`, `code/Game/Game/starttown.cpp`
- Feature: the founding-family spawn path in founding_spawn.cpp, mutually exclusive with the older starttown.cpp family-loop path. Off = starttown.cpp's older path runs instead.

### `BANISHED_LIVE_HAPPINESS`
- Readers: `code/Game/Component/happinesscomponent.cpp`
- Feature: HappinessComponent live wiring (its Shutdown/Serialize slot 3 is unconditional already, same as GrowthComponent/ResidenceComponent). Off = the rest of the component (non-serialize slots) stays inert.

### `BANISHED_LIVE_HEALER`
- Readers: `code/Game/Component/healercomponent.cpp`
- Feature: HealerComponent live vtable wiring. Off = component stays inert (same slot-3-serialize caveat as LIVE_CEMETERY).

### `BANISHED_LIVE_MARKETLOC`
- Readers: `code/Game/Component/marketlocationcomponent.cpp`
- Feature: MarketLocationComponent live wiring, slot 7. Off = component stays inert. Comment flags a slot-index vs slot-semantics correction was needed here.

### `BANISHED_LIVE_PRODUCTION`
- Readers: `code/Game/Component/citizenaicomponent.cpp`, `code/Game/Game/production_harness.cpp`, `code/Game/Game/starttown.cpp`
- Harness/diagnostic env gate (raw getenv, default OFF -- inverted from the LIVE_* norm) for production_harness.cpp's forced production path. Off (default) = harness path does not run.

### `BANISHED_LIVE_TOMBSTONE`
- Readers: `code/Game/Component/tombstonecomponent.cpp`
- Feature: TombstoneComponent live wiring, slots 4 (Startup, random-lifetime timer) and 9 (Update, per-tick countdown); slot 3 (Shutdown) already unconditional. Off = spawn timer/countdown never installed.

### `BANISHED_LIVE_WORKPLACE`
- Readers: `code/Game/Game/forester_force_harness.cpp`, `code/Game/Game/production_harness.cpp`, `code/Game/Game/starttown.cpp`
- Harness/diagnostic env gate (raw getenv, default OFF) for starttown.cpp's/production_harness.cpp's/forester_force_harness.cpp's forced-workplace path. Off (default) = harness path does not run.

### `BANISHED_PROBE_WORKPLACE_KEY`
- Readers: `code/Game/Component/workplacedescription.cpp`, `code/Game/Game/starttown.cpp`
- Diagnostic: WAVE 1 probe pinning a null-Profession-key failure branch during workplace template load; also gates a LiveEnabled check in starttown.cpp (line 994) in addition to the #if defined(_DEBUG) raw-getenv probe in workplacedescription.cpp. Off = no probe output.

## KEEP-T3 (with setter) -- 67 flags

| Flag | Setter(s) | Readers (n files) |
|---|---|---|
| `BANISHED_ACH_IGNORE_MODS` | `perf/results/20260905_223921_cp_probe/standalone_error.log`, `perf/results/20260905_224249_forester_trace/standalone_error.log`, `perf/results/20260906_092932_prod_rate/standalone_error.log` | 3 |
| `BANISHED_AUTOLOAD` | `CLAUDE.md`, `docs/runtime_port/ENDGAME.md`, `docs/runtime_port/PLAN.md`, `tools/resave_gate/resave_gate.sh` | 5 |
| `BANISHED_AUTOMENU_BUTTON` | `CLAUDE.md`, `docs/runtime_port/ENDGAME.md`, `docs/runtime_port/STATUS.md` | 4 |
| `BANISHED_AUTOMENU_CLICKS` | `CLAUDE.md`, `docs/runtime_port/STATUS.md` | 2 |
| `BANISHED_AUTOQUIT` | `CLAUDE.md`, `docs/runtime_port/ENDGAME.md`, `docs/runtime_port/PLAN.md`, `docs/runtime_port/STATUS.md` | 4 |
| `BANISHED_AUTOSTART` | `.vscode/launch.json`, `CLAUDE.md`, `Makefile`, `docs/runtime_port/STATUS.md` ... | 4 |
| `BANISHED_AUTOTUTORIAL` | `CLAUDE.md` | 2 |
| `BANISHED_BUILD_DEMO_CROPFIELD` | `perf/results/20260905_132233_ach_abstain/standalone_error.log` | 1 |
| `BANISHED_BUILD_DEMO_HOSPITAL` | `tools/disaster_report.sh` | 1 |
| `BANISHED_BUILD_DEMO_INDEX` | `perf/README.md` | 1 |
| `BANISHED_BUILD_DEMO_ORCHARD` | `perf/results/20260905_132233_ach_abstain/standalone_error.log` | 1 |
| `BANISHED_BUILD_DEMO_QUARRY` | `CLAUDE.md` | 1 |
| `BANISHED_BUILD_DEMO_WELL` | `tools/disaster_report.sh` | 1 |
| `BANISHED_CAMERA_PIN` | `CLAUDE.md`, `docs/runtime_port/STATUS.md`, `perf/README.md`, `tools/trace_oracle/trace_oracle.py` | 4 |
| `BANISHED_CAPTURE_INGAME` | `tools/dx11_state_diff/capture.sh`, `tools/trace_oracle/capture_arm.sh` | 1 |
| `BANISHED_CHECKPTR_STATIC_NOP` | `Makefile`, `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_CLIMATE` | `CLAUDE.md`, `perf/README.md`, `perf/results/20260903_221813_baseline-final/standalone_error.log`, `perf/results/20260903_222754_mx_s5/standalone_error.log` ... | 1 |
| `BANISHED_CROPDISEASE_TRACE` | `tools/disaster_report.sh` | 1 |
| `BANISHED_CROPFIELD_FORCE_START` | `perf/README.md`, `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log` ... | 3 |
| `BANISHED_DIFFICULTY` | `perf/README.md` | 1 |
| `BANISHED_FACTORY_ENUM` | `tools/gen_factory_registrations.py` | 1 |
| `BANISHED_FAST_TRIG` | `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_FOOD_WORKERS` | `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log`, `perf/results/20260903_215100_smoke4/standalone_error.log` ... | 1 |
| `BANISHED_FORCE_INFEST` | `tools/disaster_report.sh` | 1 |
| `BANISHED_FORCE_SICK` | `docs/runtime_port/PLAN.md`, `tools/disaster_report.sh` | 1 |
| `BANISHED_FORCE_SPEED` | `perf/README.md` | 1 |
| `BANISHED_FORCE_TORNADO` | `tools/disaster_report.sh` | 1 |
| `BANISHED_LIVE_COMPONENTS` | `docs/runtime_port/ENDGAME.md` | 13 |
| `BANISHED_LOD_CENSUS` | `docs/runtime_port/PLAN.md` | 2 |
| `BANISHED_MAP_SIZE` | `CLAUDE.md`, `perf/README.md`, `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log` ... | 1 |
| `BANISHED_MAP_TRACE` | `.vscode/launch.json` | 7 |
| `BANISHED_MENU_SCENE` | `docs/runtime_port/ENDGAME.md`, `tools/resave_gate/resave_gate.sh` | 1 |
| `BANISHED_MODAL_ANSWER` | `CLAUDE.md`, `docs/runtime_port/STATUS.md` | 4 |
| `BANISHED_MOVETICK_TRACE` | `perf/README.md` | 2 |
| `BANISHED_ORCHARD_CUT` | `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log`, `perf/results/20260903_215100_smoke4/standalone_error.log` ... | 1 |
| `BANISHED_ORCHARD_DIAG` | `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log`, `perf/results/20260903_215100_smoke4/standalone_error.log` ... | 4 |
| `BANISHED_ORCHARD_FORCE_START` | `perf/README.md`, `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log` ... | 2 |
| `BANISHED_ORCHARD_WORKERS` | `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log`, `perf/results/20260903_214930_smoke3/standalone_error.log`, `perf/results/20260903_215100_smoke4/standalone_error.log` ... | 1 |
| `BANISHED_PANEL_HOLD` | `tools/capture_window.ps1` | 1 |
| `BANISHED_PAUSEMENU` | `CLAUDE.md`, `docs/runtime_port/STATUS.md`, `perf/README.md`, `perf/mintsel/run_mintsel.sh` ... | 2 |
| `BANISHED_PAUSEMENU_BUTTON` | `CLAUDE.md`, `perf/README.md` | 1 |
| `BANISHED_PAUSEMENU_QUIT` | `CLAUDE.md`, `docs/runtime_port/STATUS.md`, `perf/mintsel/run_mintsel.sh`, `tools/resave_gate/resave_gate.sh` | 1 |
| `BANISHED_PERF_ANIM_CULL` | `perf/README.md` | 1 |
| `BANISHED_PERF_TREE_SNAPSHOT` | `perf/README.md` | 1 |
| `BANISHED_PLAGUE_TRACE` | `tools/disaster_report.sh` | 2 |
| `BANISHED_REDZONE` | `.vscode/launch.json`, `CLAUDE.md`, `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 8 |
| `BANISHED_REIMPL_SERIALIZER` | `docs/runtime_port/PLAN.md` | 2 |
| `BANISHED_RT_LIBM` | `docs/runtime_port/PLAN.md`, `perf/results/20260907_171618_libm_dll/standalone_error.log` | 1 |
| `BANISHED_RT_MINT` | `perf/mintsel/census_after_note.txt`, `perf/mintsel/census_before_note.txt`, `perf/mintsel/control/shadow_report.txt`, `perf/mintsel/promoted/shadow_report.txt` ... | 1 |
| `BANISHED_RT_STRICT` | `docs/runtime_port/DIRECTION.md`, `docs/runtime_port/PLAN.md`, `docs/runtime_port/STATUS.md`, `perf/mintsel/runtime_census_at_MINTSEL.py` ... | 1 |
| `BANISHED_SAMPLE` | `CLAUDE.md`, `perf/OPTIMISATION_LOG.md`, `perf/README.md`, `tools/sample_report.py` | 1 |
| `BANISHED_SAVE_TEST` | `tools/resave_gate/resave_gate.sh` | 3 |
| `BANISHED_SCREENSHOT_AT` | `docs/runtime_port/PLAN.md` | 1 |
| `BANISHED_SEED` | `CLAUDE.md`, `docs/runtime_port/STATUS.md`, `perf/README.md`, `perf/results/20260903_214226_smoke/standalone_error.log` ... | 5 |
| `BANISHED_SELECT_DIAG` | `docs/runtime_port/STATUS.md` | 1 |
| `BANISHED_SERIALIZE_TRACE` | `tools/slot3_resolver_audit.py` | 1 |
| `BANISHED_SIMPROF` | `perf/README.md` | 2 |
| `BANISHED_STRESS` | `CLAUDE.md`, `Makefile`, `perf/OPTIMISATION_LOG.md`, `perf/README.md` ... | 11 |
| `BANISHED_STRESS_ALLOW_POCKETS` | `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_STRESS_COMPPROF` | `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_STRESS_KINDMAP` | `perf/README.md` | 1 |
| `BANISHED_STRESS_SPEED` | `perf/README.md`, `tools/stress_run.sh` | 1 |
| `BANISHED_TOOL_HARNESS` | `.vscode/launch.json`, `docs/runtime_port/PLAN.md`, `perf/results/20260903_214226_smoke/standalone_error.log`, `perf/results/20260903_214615_smoke2/standalone_error.log` ... | 6 |
| `BANISHED_TORNADO_TRACE` | `tools/disaster_report.sh` | 1 |
| `BANISHED_TREE_GUARDS` | `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_TREE_PREFETCH` | `perf/OPTIMISATION_LOG.md`, `perf/README.md` | 1 |
| `BANISHED_UNREG_CENSUS` | `tools/unreg_full.py` | 1 |

## Full table (all 463 flags, sorted tier then name)

| Flag | Readers (files) | Read style | Tier | Setter | Note |
|---|---|---|---|---|---|
| `BANISHED_AMBIENT_AUDIO` | 6 files / 12 sites | LiveEnabled+getenv | GRADUATE |  | to the real AmbientEmitterComponent::Draw (0x140042ba0). This is what seeds a building's ambient audio: resize the emitter array from the description, |
| `BANISHED_BUILDER_NATURAL` | 5 files / 8 sites | LiveEnabled | GRADUATE |  | State8Enter hardcoded-entity direct seat. Off = the existing direct-seat/force-harness path. Default-ON via kDefaultOnFlags (graduated 2026-07-22); en |
| `BANISHED_BUILD_LIVE_HAUL` | 3 files / 4 sites | LiveEnabled | GRADUATE |  | (the town->site haul). Exe: CitizenAI_impl_ec60(*pWorkCompIBP, 0x2001, 1, &workIBP). Default-ON via kDefaultOnFlags (graduated 2026-07-22); env var is |
| `BANISHED_GAME_PRELOAD` | 5 files / 5 sites | other/ref | GRADUATE |  | (promoted per its own "fold into SA_LIVE_ENV once verified" note). |
| `BANISHED_LIVE_ANIMAL_UPDATE` | 3 files / 5 sites | LiveEnabled | GRADUATE |  | ===== Standalone factory ===== |
| `BANISHED_LIVE_ATTENDANCE` | 2 files / 2 sites | other/ref | GRADUATE |  |  |
| `BANISHED_LIVE_BOAT_CTRL` | 4 files / 5 sites | LiveEnabled | GRADUATE |  | Slot 3 (stream Shutdown/Serialize, retail 0x1400465d0) wired UNCONDITIONALLY — save/load wave 2026-08-28. Previously the boat controller fell through |
| `BANISHED_LIVE_BUILDCTRL` | 5 files / 11 sites | LiveEnabled | GRADUATE |  | Phase 3 / Keystone D step 3 — live ControllerComponent with the REAL state-type table + dispatcher node virtuals. Embedded state node at +0x28 (Create |
| `BANISHED_LIVE_BUILD_ONADDED` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | *_DrawReimpl only because callers and the address registry already use it; Ghidra's "Draw" label is the familiar slot-6 mislabel. |
| `BANISHED_LIVE_CHARACTER` | 2 files / 4 sites | LiveEnabled | GRADUATE |  | is a per-frame no-op; it begins executing actions once the CitizenAI state handlers push them. |
| `BANISHED_LIVE_CHARACTER_MOVETICK` | 7 files / 14 sites | LiveEnabled | GRADUATE |  | body-init because it only depends on +0x68 (the ModelComponent, just resolved) and the char spatial-hash manager (GameWorld+0x88, seeded earlier in Lo |
| `BANISHED_LIVE_CHARACTER_UPDATE9` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | Fifth-priority animals (quota 2/frame, ~120 animals) at ~1/60 speed — visually frozen. Slot 8 full-rate dispatch is the fix (2026-07-30). Slot 9's fai |
| `BANISHED_LIVE_CITIZEN` | 2 files / 5 sites | LiveEnabled | GRADUATE | Makefile | + desc+0x38 (base 15.0, tol +/-2.0). maxAge MUST be set: the inert factory zeroes it, and the Update old-age death check (maxAge <= age) would fire im |
| `BANISHED_LIVE_CITIZENAI` | 2 files / 2 sites | LiveEnabled | GRADUATE | Makefile | Serialize round-trip is not an opt-in feature: wired before the env gate. |
| `BANISHED_LIVE_CITIZENAI_CITID` | 4 files / 4 sites | other/ref | GRADUATE |  | TypeInfo for IsDerivedFrom check in Serialize -- DAT_1401c7158 (CitizenComponent_FC). engine_funcs.h has it as g_TypeId_CitizenComponent_FC, but that |
| `BANISHED_LIVE_CITIZEN_CARRY` | 2 files / 4 sites | LiveEnabled | GRADUATE |  | Verified-working features: the Makefile's former SA_LIVE_ENV set (all |
| `BANISHED_LIVE_CITIZEN_ONADD` | 3 files / 3 sites | LiveEnabled | GRADUATE |  | SetupAgeMesh for citizens whose action stack is EMPTY, so busy ones never heal. |
| `BANISHED_LIVE_CITIZEN_ONREMOVE` | 3 files / 4 sites | LiveEnabled | GRADUATE |  | it just landed on the inert no-op stub, which is why every citizen death left the town's citizen list (townMgr+0xa0) holding a dangling pointer. Retai |
| `BANISHED_LIVE_CITIZEN_STATUSICON` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | still route these five call sites to StatusIcon_Toggle — the port of FUN_14012c3f0 in statusiconcomponent.cpp — so the citizen's own StatusIconCompone |
| `BANISHED_LIVE_CITIZEN_UPDATE` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | and/or EMPLOYED (+0x238 != 0) — both assigned here. Inert until now.  Promoted STARTUP -> _S: Startup stays on BANISHED_LIVE_CITIZEN (unchanged); the |
| `BANISHED_LIVE_CLEARCTRL` | 5 files / 8 sites | LiveEnabled | GRADUATE |  | so the footprint resources are chopped over time (logs/stone/iron). Requires the StateMachineComponent to be live (BANISHED_LIVE_STATEMACHINE). |
| `BANISHED_LIVE_CONSUMEPRODUCE_CMD` | 2 files / 3 sites | other/ref | GRADUATE |  | promotion, and wiring 13/14 alone was measurably NOT enough for them: the base slot 13 derives its mask from the state machine and the base slot 14 po |
| `BANISHED_LIVE_CROPFIELD` | 3 files / 3 sites | other/ref | GRADUATE | perf/results/20260905_132233_ach_abstain/standalone_error.log | Each ON arm asserts a value only the live path can produce (a non-zero dock rate, a moving hunt progress, resource types that do not exist otherwise), |
| `BANISHED_LIVE_DECAL` | 5 files / 8 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_DESTROY` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | InternalFactory lifecycle bug (fixed; see internal_factory_audit.h). The whole building-upgrade path is dead without it — DestroyComponent installs no |
| `BANISHED_LIVE_DESTROY_COLLAPSE` | 3 files / 5 sites | LiveEnabled | GRADUATE |  | mid-frame, which is the churn class that has faulted before. Without it the whole disaster lane is cosmetically dead — a burning building keeps its in |
| `BANISHED_LIVE_DESTROY_WORKERS` | 2 files / 6 sites | LiveEnabled | GRADUATE |  | crash is the pre-existing removal-lifecycle dangling reference, present with the flag off, so it is not a reason to hold this. It IS a reason not to c |
| `BANISHED_LIVE_DISEASELIST` | 3 files / 6 sites | LiveEnabled | GRADUATE | perf/results/20260903_214226_smoke/standalone_error.log (+80) |  |
| `BANISHED_LIVE_ELEMENT_TIDS` | 7 files / 13 sites | other/ref | GRADUATE |  | gw+0xd8 is unseeded. Graduated anyway as an explicit call: the trade lane is verified and the delta is a pathing-efficiency wobble, not a failure. If |
| `BANISHED_LIVE_ENTITYUI_RESETTOOL` | 1 files / 2 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON (=0 disables); re-armed nomad component reset-tool hook |
| `BANISHED_LIVE_EVENTS` | 2 files / 4 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_FIRE` | 3 files / 4 sites | LiveEnabled | GRADUATE | tools/disaster_report.sh | standalone_crash.log empty in both arms. So the mechanic is complete in both directions: fire spreads and destroys a town that cannot fight it, and th |
| `BANISHED_LIVE_FISHING` | 2 files / 2 sites | other/ref | GRADUATE |  | a moving hunt progress, resource types that do not exist otherwise), so these are positive runs rather than clean ones. No crash log in any ON arm. |
| `BANISHED_LIVE_FLOOR` | 3 files / 5 sites | LiveEnabled | GRADUATE |  | -1.853 / -1.883 — a FULL BODY HEIGHT of correction. 126k refine hits, zero crashes; the gate-off arm logged 0 hits and an identical world build. User- |
| `BANISHED_LIVE_FORESTER` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | rather than using CONTROLLER_LIVE_FACTORY_STATES (whose PopulateFn cannot reach the instance). Without that write every lodge would be born with both |
| `BANISHED_LIVE_FRUIT` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | pokes the master switch, CUT clicks a destructive button), not features. |
| `BANISHED_LIVE_GATHERER` | 3 files / 3 sites | other/ref | GRADUATE |  |  |
| `BANISHED_LIVE_GROWTH` | 5 files / 11 sites | LiveEnabled | GRADUATE |  | player stand-ins (WORKERS seats a citizen, GROW fast-forwards maturity, FORCE_START pokes the master switch, CUT clicks a destructive button), not fea |
| `BANISHED_LIVE_HERD` | 4 files / 8 sites | LiveEnabled | GRADUATE |  | early deaths are the faithful uniform-age tail, not a drain). The three gates are one feature: population spawns herds, herd ticks the members, animal |
| `BANISHED_LIVE_HOUSING` | 2 files / 9 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_HUNTING` | 3 files / 3 sites | other/ref | GRADUATE |  | positive runs rather than clean ones. No crash log in any ON arm. |
| `BANISHED_LIVE_IMMIGRATION` | 5 files / 7 sites | other/ref | GRADUATE |  | DENY run whose nomads walked back out and were removed (docs/waves/ achievements_wave.md 5.7). Retail's timer puts a group 2..6 in-game years apart, s |
| `BANISHED_LIVE_INTERACT_ONADD` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | log. It also corrects a mis-attribution: a footprint containing 23 trees built fine with the clear chain OFF, so "a tool-placed site over naturals nev |
| `BANISHED_LIVE_MAP_SOLIDIFY` | 3 files / 5 sites | LiveEnabled | GRADUATE |  | We already ported the state-2 handler and broadcast event 1; this slot was a stub, so the message landed on nothing and no placed building ever regist |
| `BANISHED_LIVE_MAP_SPAWN_CLAIM` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | Proven by the orchard: without it CreateNaturalResource spawns the tree, nothing writes it into the tile record, and OrchardComponent::OnMessage(0) re |
| `BANISHED_LIVE_MARKET_CMD` | 2 files / 3 sites | other/ref | GRADUATE |  | slot 13 derives its mask from the state machine and the base slot 14 posts to the component's own node, so with slots 16/17 stubbed a click measured 0 |
| `BANISHED_LIVE_MODDIALOG` | 3 files / 8 sites | LiveEnabled | GRADUATE |  | persist across a clean Quit. Five defects fixed on the way (int-typed pointer arg to ImageBuffer::Create, fn_MemFree never assigned, wrong OnElementEv |
| `BANISHED_LIVE_MULTIOBJECT` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | buildings), not just msg 4 on tunnels. It is faithful to retail 0x140125490, and components that do not implement slot 12 hold stubs, but it is the wi |
| `BANISHED_LIVE_MUSIC` | 1 files / 3 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; gates MusicEnabled() for the music player |
| `BANISHED_LIVE_ORCHARD` | 3 files / 4 sites | other/ref | GRADUATE |  | The BANISHED_ORCHARD_* harnesses stay opt-in and are NOT part of this: they are player stand-ins (WORKERS seats a citizen, GROW fast-forwards maturity |
| `BANISHED_LIVE_PARTICLE` | 2 files / 3 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_PASTURE` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | Default runs have no pastures until the fenced-area tool lands, so this is inert without the demo rig; organic births still need the herder chain (Pas |
| `BANISHED_LIVE_PICKING` | 2 files / 2 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_PICKMAP` | 1 files / 2 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; StartTown pick-map registration |
| `BANISHED_LIVE_PICKMESH` | 3 files / 5 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; PickingComponent mesh registration |
| `BANISHED_LIVE_PICK_ONADDED` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | bailed, and a hand-placed building was unclickable (measured: pickingId -1 -> 27). RegisterMesh now also honours retail's `pickingId(+0x28) < 0` guard |
| `BANISHED_LIVE_POPULATION` | 4 files / 5 sites | LiveEnabled | GRADUATE |  | gates are one feature: population spawns herds, herd ticks the members, animal update ages/wanders them. |
| `BANISHED_LIVE_PROFESSIONLIST` | 2 files / 6 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; ProfessionList keystone load (crash-free now) |
| `BANISHED_LIVE_QUARRY` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | carved heights (hmin=-358 = -1.4*256), pit + rock decal both visible on screen. The final blocker was TerraformComponent_RegionRecalc silently no-opin |
| `BANISHED_LIVE_RAWMAT_PICKUP` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | self-posts a Pickup work on entity registration; a laborer claims it and dispatches the haul. Held opt-in only because the haul used to crash on execu |
| `BANISHED_LIVE_REGIONMAP` | 3 files / 4 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_RESIDENCE` | 3 files / 8 sites | LiveEnabled | GRADUATE | .vscode/launch.json | and WindEffect.rsc with null SoundBuffers and faults inside XAudio2. |
| `BANISHED_LIVE_RESIDENCE_REGISTRY` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | the first second-generation marriages this project has measured. spouse > 12 is a value only the live path can produce, which is what makes this a pos |
| `BANISHED_LIVE_SCENARIO_GOALS` | 1 files / 2 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; scenario/tutorial goals instance |
| `BANISHED_LIVE_SM_BUILT_HOOK` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | and a non-zero +0x48 on an entity with no BuildComponent), so this is a positive run. Houses/storage keep key 0 - their description declares no profes |
| `BANISHED_LIVE_SM_ONADDED` | 3 files / 14 sites | LiveEnabled | GRADUATE |  | structures taking BuildComponent's 1 and running the real 1 -> 8 -> 2 chain to completion inside the spawn bracket (`state(+0x50)=2 pending=-1`, no fa |
| `BANISHED_LIVE_SPAWN` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | Post-fix: 0/4 crashes, seed output unchanged at CREATED=249. See notes/generation_deviations.md (deviation #4). Each remains a kill-switch (=0) for an |
| `BANISHED_LIVE_STATEMACHINE` | 6 files / 7 sites | LiveEnabled | GRADUATE |  | StateMachineComponent links this component's embedded state node (at +0x28) and ticks its clearing state machine, which posts a ClearArea work-action |
| `BANISHED_LIVE_STATUSICON` | 3 files / 5 sites | LiveEnabled | GRADUATE |  | statusiconcomponent.cpp — so the citizen's own StatusIconComponent (cached at CitizenComponent+0x280) actually shows hunger/cold/unemployed/homeless i |
| `BANISHED_LIVE_STATUSICON_DRAW` | 3 files / 3 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_STORAGE` | 4 files / 11 sites | LiveEnabled | GRADUATE |  | For the BANISHED_LIVE_STORAGE eat-message typeid heal in the |
| `BANISHED_LIVE_STORAGECTRL` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | storageLocs=2, and 150/70/50/50/2400/400/40 placed, identical to the stand-in baseline. The half-configuration double-registers (2 -> 4, d230 has no d |
| `BANISHED_LIVE_STORAGEDISPLAY` | 3 files / 6 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_LIVE_STORAGE_ONADDED` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | Carry capacity of the NEWBORN, read after RegisterEntity — i.e. after ComponentEntity_OnAddedPass has broadcast vtable slot 6. This is the only place |
| `BANISHED_LIVE_TERRAFORM` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | components that do not implement slot 12 hold stubs, but it is the widest-reaching half of this wave. If a multi-part structure starts behaving oddly, |
| `BANISHED_LIVE_TOOLBAR` | 3 files / 8 sites | LiveEnabled | GRADUATE |  | "user-verified working" per its comment) + BANISHED_LIVE_CITIZEN_CARRY (promoted per its own "fold into SA_LIVE_ENV once verified" note). |
| `BANISHED_LIVE_TOOLS` | 4 files / 6 sites | LiveEnabled | GRADUATE |  | the gate-off arm logged 0 hits and an identical world build. User-verified visually on both docks: fishermen stand on the boards and path along them a |
| `BANISHED_LIVE_TORNADO` | 2 files / 3 sites | LiveEnabled | GRADUATE | tools/disaster_report.sh | The two arms are frame-identical through 15000, i.e. the flag changes the fire outcome and nothing else. |
| `BANISHED_LIVE_TRADE_CHECKOUT` | 3 files / 6 sites | LiveEnabled | GRADUATE |  | Its one previously-unported dependency, 0x14005ace0 (the livestock spawn), is now ported in code/Game/Component/tradecomponent_livestock.cpp and regis |
| `BANISHED_LIVE_TRADE_COMPONENT` | 2 files / 5 sites | LiveEnabled | GRADUATE |  | hunting throughput regresses, re-run that A/B before looking anywhere else. |
| `BANISHED_LIVE_TRADE_CTRL` | 5 files / 5 sites | LiveEnabled | GRADUATE |  | FromYawPitch / the Transform and Quaternion ops are NOT inlined — those carry angle and handedness conventions worth taking from the engine. |
| `BANISHED_LIVE_TRADE_SPINNER_RANGE` | 1 files / 1 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; trade spinner SetMinMax wiring |
| `BANISHED_LIVE_TRADE_TIDS` | 5 files / 7 sites | other/ref | GRADUATE |  | update (0x140106420) calls exactly this function at 0x140106650. So the real rule is: a TRADING POST on the tile does not disqualify it; every other o |
| `BANISHED_LIVE_WALKWAY_DROPCELLS` | 4 files / 11 sites | LiveEnabled | GRADUATE |  | Measured 2026-08-15, same harness, single change: default (this flag on) => 44 fish piles spawned, each landing EXACTLY on a drop-spot cell, spots re |
| `BANISHED_LIVE_WEATHER` | 1 files / 2 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; weather sim tick (=0 hard-zeros for A/B) |
| `BANISHED_LIVE_WELL` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | Aim a forced tornado at the town centre before concluding anything either way. GRADUATED 2026-08-22 — the fire disaster lane itself (FireComponent vta |
| `BANISHED_LIVE_WORKCOMP_ONREMOVE` | 2 files / 2 sites | LiveEnabled | GRADUATE |  | Paired with the retail ctor seeds (workId=-1, workFlags=0x80000000) the inert factory was leaving zeroed -- load-bearing, since 0 is a VALID work id a |
| `BANISHED_LIVE_WORKPLACE_ASSIGN` | 1 files / 1 sites | getenv | GRADUATE |  | raw-getenv kill-switch, default ON; workplace UI full startup (restores retail behaviour) |
| `BANISHED_LIVE_WPC_BORN_ACCEPTING` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | flag off, tracked separately, and — as for BANISHED_LIVE_DESTROY below — not a reason to hold this. The soak was NOT clean; it was A/B-IDENTICAL, whic |
| `BANISHED_LIVE_WPC_CONTROLLER` | 5 files / 8 sites | LiveEnabled | GRADUATE |  | workplace hand-off happens alongside this storage reconfigure. Do it first and unconditionally — the storage half below early-returns on several data |
| `BANISHED_LIVE_WPC_PAUSE` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | renders above the building and in the dialog status bar. The compensation guards added while chasing the wrong cause have been removed; one declared d |
| `BANISHED_LIVE_ZONE_RASTER` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | factory just shipped the stub vtable. Default-on 2026-07-28: A/B on clearplaceover measured slot0 mine=16/empty=4('.') CELL-PICK OK vs control mine=0 |
| `BANISHED_SEED_FOOD_FIRST` | 2 files / 4 sites | LiveEnabled | GRADUATE |  | gw+0xd8 is UNSEEDED — BANISHED_SEED pins world-gen only. Frames-to-extinction is a single threshold crossing and must not be used to rank fixes. See r |
| `BANISHED_SELECTION_OUTLINE` | 7 files / 14 sites | LiveEnabled | GRADUATE |  |  |
| `BANISHED_SPAWN_DOSPAWN` | 2 files / 7 sites | LiveEnabled | GRADUATE | perf/results/20260903_220604_baseline/standalone_error.log (+287) | See notes/generation_deviations.md (deviation #4). Each remains a kill-switch (=0) for an A/B run. |
| `BANISHED_SPAWN_INITIAL` | 2 files / 3 sites | LiveEnabled | GRADUATE |  | Each remains a kill-switch (=0) for an A/B run. |
| `BANISHED_AUDIO_AUDIT` | 1 files / 1 sites | LiveEnabled | OPT-IN |  | Diagnostic, not a feature: one-shot scan of the 2D sound-effect table for a null entry (regression hunt for a load-restore bug). Off = never runs (must not run by default per its own comment -- the scan pattern only makes sense once per session). |
| `BANISHED_AUDIO_PROBE` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Diagnostic: loads ambient sound resources directly to isolate a load path. Off = skipped. |
| `BANISHED_CLEAR_FOOTPRINT` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Feature: de-inert ClearAreaComponent so it clears its building's footprint automatically (Track A step 2), no harness needed. Off = footprint clearing stays on the older harness-driven path. |
| `BANISHED_LIVE_ANIMALS` | 1 files / 1 sites | LiveEnabled | OPT-IN |  | One of four gates (with LIVE_HERD/LIVE_POPULATION/LIVE_ANIMAL_UPDATE, any-of OR'd) for WildAnimalComponent's live vtable slot 3 (Serialize). Off (all four unset) = animal serialize slot stays uninstalled. |
| `BANISHED_LIVE_AUTOSAVE` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Feature: creates and adds the AutoSave dialog in the game update loop. Off = no autosave dialog is created. |
| `BANISHED_LIVE_BUILD_ONREMOVE` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Feature: BuildComponent's OnRemove hook, needed only for a construction site torn down before completion (state8 already handles the normal completion removal). Off = a cancelled/demolished foundation leaves a hole (component doesn't clean up). |
| `BANISHED_LIVE_CEMETERY` | 1 files / 1 sites | LiveEnabled | OPT-IN |  | Feature: CemeteryComponent live vtable wiring. Off = component stays inert (see reference_vtable_slot3_serialize_never_installed for the shared serialize-slot caveat across several LIVE_* component gates). |
| `BANISHED_LIVE_CITIZEN_FAMILY` | 2 files / 4 sites | LiveEnabled | OPT-IN |  | Feature: the marriage/partner-linking block on CitizenComponent (sets partner pointers both ways). Off = marriage block stays a dead no-op (declared, null-checked, never resolved). |
| `BANISHED_LIVE_CITIZEN_POSTLOAD_TOOL` | 1 files / 3 sites | LiveEnabled | OPT-IN |  | Known deviation, explicitly opt-in: a post-load tool-reset behavior on CitizenComponent lifecycle. Off = retail-faithful (no post-load reset). |
| `BANISHED_LIVE_CROPDISEASE` | 2 files / 6 sites | LiveEnabled+getenv | OPT-IN | tools/disaster_report.sh | Feature: crop disease infestation/spread/kill timers on DiseaseComponent. Off = crops never get infested by this path. |
| `BANISHED_LIVE_FOUNDING_SPAWN` | 2 files / 4 sites | LiveEnabled | OPT-IN | perf/results/20260903_214226_smoke/standalone_error.log (+334) | Feature: the founding-family spawn path in founding_spawn.cpp, mutually exclusive with the older starttown.cpp family-loop path. Off = starttown.cpp's older path runs instead. |
| `BANISHED_LIVE_HAPPINESS` | 1 files / 4 sites | LiveEnabled | OPT-IN | perf/results/20260903_214226_smoke/standalone_error.log (+334) | Feature: HappinessComponent live wiring (its Shutdown/Serialize slot 3 is unconditional already, same as GrowthComponent/ResidenceComponent). Off = the rest of the component (non-serialize slots) stays inert. |
| `BANISHED_LIVE_HEALER` | 1 files / 3 sites | LiveEnabled | OPT-IN |  | Feature: HealerComponent live vtable wiring. Off = component stays inert (same slot-3-serialize caveat as LIVE_CEMETERY). |
| `BANISHED_LIVE_MARKETLOC` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Feature: MarketLocationComponent live wiring, slot 7. Off = component stays inert. Comment flags a slot-index vs slot-semantics correction was needed here. |
| `BANISHED_LIVE_PRODUCTION` | 3 files / 6 sites | getenv | OPT-IN |  | Harness/diagnostic env gate (raw getenv, default OFF -- inverted from the LIVE_* norm) for production_harness.cpp's forced production path. Off (default) = harness path does not run. |
| `BANISHED_LIVE_TOMBSTONE` | 1 files / 2 sites | LiveEnabled | OPT-IN |  | Feature: TombstoneComponent live wiring, slots 4 (Startup, random-lifetime timer) and 9 (Update, per-tick countdown); slot 3 (Shutdown) already unconditional. Off = spawn timer/countdown never installed. |
| `BANISHED_LIVE_WORKPLACE` | 3 files / 9 sites | getenv | OPT-IN |  | Harness/diagnostic env gate (raw getenv, default OFF) for starttown.cpp's/production_harness.cpp's/forester_force_harness.cpp's forced-workplace path. Off (default) = harness path does not run. |
| `BANISHED_PROBE_WORKPLACE_KEY` | 2 files / 4 sites | LiveEnabled+getenv | OPT-IN |  | Diagnostic: WAVE 1 probe pinning a null-Profession-key failure branch during workplace template load; also gates a LiveEnabled check in starttown.cpp (line 994) in addition to the #if defined(_DEBUG) raw-getenv probe in workplacedescription.cpp. Off = no probe output. |
| `BANISHED_ACH_IGNORE_MODS` | 3 files / 6 sites | getenv | KEEP-T3 | perf/results/20260905_223921_cp_probe/standalone_error.log (+2) | BANISHED_ACH_IGNORE_MODS=1 -- NON-RETAIL TEST FIXTURE. |
| `BANISHED_AUTOLOAD` | 5 files / 7 sites | other/ref | KEEP-T3 | CLAUDE.md (+3) | Standalone: expose the instance so the BANISHED_AUTOLOAD driver |
| `BANISHED_AUTOMENU_BUTTON` | 4 files / 4 sites | other/ref | KEEP-T3 | CLAUDE.md (+2) | GRADUATED 2026-09-05 -- the Mods menu (ModDialog + ModDetailsDialog, code/Game/Dialog/mod_dialog_vtables.cpp). Driven unattended with |
| `BANISHED_AUTOMENU_CLICKS` | 2 files / 3 sites | other/ref | KEEP-T3 | CLAUDE.md (+1) | GRADUATED 2026-09-05 -- the Mods menu (ModDialog + ModDetailsDialog, code/Game/Dialog/mod_dialog_vtables.cpp). Driven unattended with |
| `BANISHED_AUTOQUIT` | 4 files / 4 sites | other/ref | KEEP-T3 | CLAUDE.md (+3) | answer the question "may we take a decision retail would put to the player?". |
| `BANISHED_AUTOSTART` | 4 files / 6 sites | getenv | KEEP-T3 | .vscode/launch.json (+346) |  |
| `BANISHED_AUTOTUTORIAL` | 2 files / 2 sites | other/ref | KEEP-T3 | CLAUDE.md | Expose the instance so the BANISHED_AUTOTUTORIAL driver in |
| `BANISHED_BUILD_DEMO_CROPFIELD` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/results/20260905_132233_ach_abstain/standalone_error.log |  |
| `BANISHED_BUILD_DEMO_HOSPITAL` | 1 files / 1 sites | other/ref | KEEP-T3 | tools/disaster_report.sh | so the generic ring scan sizes them and the fallbacks are never used. |
| `BANISHED_BUILD_DEMO_INDEX` | 1 files / 3 sites | getenv | KEEP-T3 | perf/README.md | building at its foundation mesh (skip the force-complete below) and record its BuildComponent as g_pConstructionDemoSite. State1Enter then drives ONLY |
| `BANISHED_BUILD_DEMO_ORCHARD` | 1 files / 3 sites | getenv | KEEP-T3 | perf/results/20260905_132233_ach_abstain/standalone_error.log | ── BANISHED_BUILD_DEMO_ORCHARD — one COMPLETED orchard (test scaffolding) ───── |
| `BANISHED_BUILD_DEMO_QUARRY` | 1 files / 1 sites | other/ref | KEEP-T3 | CLAUDE.md | observed without a quarry in the world, and the autostart town founds none. 21x17 is the authored CreatePlacedDescription footprint — the biggest thin |
| `BANISHED_BUILD_DEMO_WELL` | 1 files / 1 sites | other/ref | KEEP-T3 | tools/disaster_report.sh | benefit (it divides the death roll and suspends hunger). Both are ordinary placed structures with an authored CreatePlacedDescription, so the generic |
| `BANISHED_CAMERA_PIN` | 4 files / 6 sites | getenv | KEEP-T3 | CLAUDE.md (+3) | the DLL and, with no detour installed, never leaves it. The element, the dialog and the call are all the real ones; only the tab index is chosen by th |
| `BANISHED_CAPTURE_INGAME` | 1 files / 2 sites | getenv | KEEP-T3 | tools/dx11_state_diff/capture.sh (+1) | engine's Present happens after this Draw returns (MainLoop::Draw EndFrame), so a StartFrameCapture here brackets a full frame of GPU work. |
| `BANISHED_CHECKPTR_STATIC_NOP` | 1 files / 2 sites | other/ref | KEEP-T3 | Makefile (+2) | registering costs nothing.  The perf-build half of the old argument is unchanged and is not a reason |
| `BANISHED_CLIMATE` | 1 files / 4 sites | getenv | KEEP-T3 | CLAUDE.md (+327) | --- Climate override (standalone autostart + testing) --- |
| `BANISHED_CROPDISEASE_TRACE` | 1 files / 2 sites | getenv | KEEP-T3 | tools/disaster_report.sh | Opt-in.  BANISHED_LIVE_CROPDISEASE=1 graduates vtable slots 3/4/6/7/9 to the real bodies. Unproven feature => NOT in kDefaultOnFlags. With the gate of |
| `BANISHED_CROPFIELD_FORCE_START` | 3 files / 3 sites | other/ref | KEEP-T3 | perf/README.md (+10) | ⚠ A field with NO crop chosen sits at accepting=0 / state 0 and that is FAITHFUL, not a regression of this flag: CropFieldComponent_Ctor seeds +0x48 = |
| `BANISHED_DIFFICULTY` | 1 files / 2 sites | getenv | KEEP-T3 | perf/README.md | would otherwise get whatever the combo defaults to (Hard/Medium). Resolve the MapGenerator directly from the StartCondition list by index instead. Sta |
| `BANISHED_FACTORY_ENUM` | 1 files / 2 sites | getenv | KEEP-T3 | tools/gen_factory_registrations.py | Accelerator A (notes/PRELOAD_CASCADES_PLAN.md): when BANISHED_FACTORY_ENUM is |
| `BANISHED_FAST_TRIG` | 1 files / 2 sites | getenv | KEEP-T3 | perf/OPTIMISATION_LOG.md (+1) | cos per character per tick). This file now does what the CRT does, in double, and computes both from ONE argument reduction (the move tick always need |
| `BANISHED_FOOD_WORKERS` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/results/20260903_214226_smoke/standalone_error.log (+333) | Seat the first worker at each Fishing Dock / Hunter Lodge / Gatherer's Hut / Crop Field — the one player click those buildings have no tool for yet. T |
| `BANISHED_FORCE_INFEST` | 1 files / 1 sites | other/ref | KEEP-T3 | tools/disaster_report.sh | years (crop rotated). With rollMonths = 0.25 that is 48 rolls a year, each at chance = 1/(48 * years) ~= 1.4e-4. A `[CropDisease] ROLL ... -> no` line |
| `BANISHED_FORCE_SICK` | 1 files / 1 sites | other/ref | KEEP-T3 | docs/runtime_port/PLAN.md (+1) | Disease*, set by Infect 0x140116b90) is non-null, so a healthy town never offers it. A/B in the BANISHED_STRESS town, 200 citizens, 1600 frames: contr |
| `BANISHED_FORCE_SPEED` | 1 files / 2 sites | getenv | KEEP-T3 | perf/README.md | [FORCE SPEED] env BANISHED_FORCE_SPEED=N — pin the EntityUI speed multiplier |
| `BANISHED_FORCE_TORNADO` | 1 files / 1 sites | other/ref | KEEP-T3 | tools/disaster_report.sh | "unproven feature" until the particle half existed; with TornadoInstance::Update ported (commit bb9a1f9) the whole mechanic runs end to end and has be |
| `BANISHED_LIVE_COMPONENTS` | 13 files / 16 sites | getenv | KEEP-T3 | docs/runtime_port/ENDGAME.md | master opt-in toggle: forces every non-graduated LIVE_* flag on at once |
| `BANISHED_LOD_CENSUS` | 2 files / 3 sites | other/ref | KEEP-T3 | docs/runtime_port/PLAN.md | [diag] BANISHED_LOD_CENSUS — replicate MeshInstance's distance-based detail |
| `BANISHED_MAP_SIZE` | 1 files / 4 sites | getenv | KEEP-T3 | CLAUDE.md (+338) | --- Map size override (standalone autostart + testing) --- terrainSize is a row index into the TerrainGenerator's size table (Valleys.rsc / Mountains. |
| `BANISHED_MAP_TRACE` | 7 files / 14 sites | getenv | KEEP-T3 | .vscode/launch.json |  |
| `BANISHED_MENU_SCENE` | 1 files / 3 sites | getenv | KEEP-T3 | docs/runtime_port/ENDGAME.md (+1) | Diagnostic lever: BANISHED_MENU_SCENE pins the menu scene index (0..9) so |
| `BANISHED_MODAL_ANSWER` | 4 files / 10 sites | getenv | KEEP-T3 | CLAUDE.md (+1) | ShowDialog (modal) — blocks until user responds |
| `BANISHED_MOVETICK_TRACE` | 2 files / 5 sites | getenv | KEEP-T3 | perf/README.md | BANISHED_MOVETICK_TRACE: one line per char-index allocation. The owner |
| `BANISHED_ORCHARD_CUT` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/results/20260903_214226_smoke/standalone_error.log (+333) | Click the panel's CUT button (command id 3) on the player's behalf, so state 20 can |
| `BANISHED_ORCHARD_DIAG` | 4 files / 4 sites | other/ref | KEEP-T3 | perf/results/20260903_214226_smoke/standalone_error.log (+333) | entity's StateMachineComponent chain. Nothing ticked, so nothing ever built the per-tree slot array at +0x50/+0x58, nothing posted PlantOrchard work, |
| `BANISHED_ORCHARD_FORCE_START` | 2 files / 2 sites | other/ref | KEEP-T3 | perf/README.md (+334) | One-shot: force an untyped demo orchard's master switch on + assign a real tree type via the live combo-select path, so the state-2->state-9 transitio |
| `BANISHED_ORCHARD_WORKERS` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/results/20260903_214226_smoke/standalone_error.log (+333) | Seat the first worker at each orchard — the one player click the employment allocator structurally cannot make (it gates on roster != 0). No-op unless |
| `BANISHED_PANEL_HOLD` | 1 files / 1 sites | other/ref | KEEP-T3 | tools/capture_window.ps1 | BANISHED_PASTURE_HERDERS / BANISHED_BUILDER_AUTOPOOL cascade-bypass stand-ins (standalone/panel_staffing_driver.cpp). |
| `BANISHED_PAUSEMENU` | 2 files / 2 sites | other/ref | KEEP-T3 | CLAUDE.md (+4) | ToolbarDialog_OnRemove case 3) and click one of its buttons -- by default Quit Game. The front-end Quit is already drivable via BANISHED_AUTOQUIT; thi |
| `BANISHED_PAUSEMENU_BUTTON` | 1 files / 1 sites | other/ref | KEEP-T3 | CLAUDE.md (+1) |  |
| `BANISHED_PAUSEMENU_QUIT` | 1 files / 1 sites | other/ref | KEEP-T3 | CLAUDE.md (+3) | ⚠ ATTENDANCE IS ZERO ON A DLL-PRESENT RUN, AND THAT IS STRUCTURAL --------------------------------------------------------------------------- MEASURED |
| `BANISHED_PERF_ANIM_CULL` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/README.md | root-motion offset still advance exactly as retail's intermediate iterations do); PerfX flushes the pose with a zero-time update if the sim pauses, an |
| `BANISHED_PERF_TREE_SNAPSHOT` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/README.md | a tick land in the staging arrays, never directly in the tree), so it is cached as a flat array and rebuilt when either mutation site marks it dirty o |
| `BANISHED_PLAGUE_TRACE` | 2 files / 4 sites | getenv | KEEP-T3 | tools/disaster_report.sh | WPC+0x48 exactly. This finder was never supposed to reach a school. See [[project_wpc_first_seat_and_attendance_keys]]. [Plague] soak trace — same kil |
| `BANISHED_REDZONE` | 8 files / 9 sites | other/ref | KEEP-T3 | .vscode/launch.json (+3) | no-op unless BANISHED_REDZONE=watch + BANISHED_TEARDOWN_HEAPCHECK=1 |
| `BANISHED_REIMPL_SERIALIZER` | 2 files / 3 sites | other/ref | KEEP-T3 | docs/runtime_port/PLAN.md | HISTORY OF THIS FILE, AND WHAT CHANGED ---------------------------------------------------------------------------- This file existed before the wave |
| `BANISHED_RT_LIBM` | 1 files / 1 sites | other/ref | KEEP-T3 | docs/runtime_port/PLAN.md (+1) | 0x180128110 Crt_sqrtf   0x180127d90 Crt_sinf    0x180126d10 Crt_cosf 0x180126680 Crt_atan2f  0x180127560 Crt_powf    0x180127010 Crt_fmodf `Crt_fmodf` |
| `BANISHED_RT_MINT` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/mintsel/census_after_note.txt (+6) | 2. `?_singleton@SaveLoad@Save@@` is PROMOTED to our own storage on every default run (rt_data_mint.cpp:924 calls RtMintRegisterPromotedRows() |
| `BANISHED_RT_STRICT` | 1 files / 1 sites | other/ref | KEEP-T3 | docs/runtime_port/DIRECTION.md (+4) | The caller address is recorded so the site can be named with `nm` against standalone/build/reimpl-x64.exe (image base 0x140000000), exactly as |
| `BANISHED_SAMPLE` | 1 files / 1 sites | other/ref | KEEP-T3 | CLAUDE.md (+3) | The originals are the MSVC CRT sinf/cosf: the float is widened to DOUBLE, reduced by pi/2, run through a double minimax polynomial and rounded back to |
| `BANISHED_SAVE_TEST` | 3 files / 4 sites | getenv | KEEP-T3 | tools/resave_gate/resave_gate.sh | Trigger the retail autosave path on demand and log the operation's |
| `BANISHED_SCREENSHOT_AT` | 1 files / 3 sites | getenv | KEEP-T3 | docs/runtime_port/PLAN.md | HARNESS, not retail: BANISHED_SCREENSHOT_AT=<frame> raises the same |
| `BANISHED_SEED` | 5 files / 10 sites | getenv | KEEP-T3 | CLAUDE.md (+341) | meanAttempts 1.45 (off) -> 1.76 (on); TRADE_TIDS was indistinguishable from off, so ELEMENT_TIDS is the channel. It is NOT frame pacing (mean fps 140. |
| `BANISHED_SELECT_DIAG` | 1 files / 3 sites | getenv | KEEP-T3 | docs/runtime_port/STATUS.md | BANISHED_SELECT_DIAG=1 traces the live EditTool pick path (A/B-safe: off by default). |
| `BANISHED_SERIALIZE_TRACE` | 1 files / 2 sites | getenv | KEEP-T3 | tools/slot3_resolver_audit.py | Save/load wave debug (BANISHED_SERIALIZE_TRACE): per-component |
| `BANISHED_SIMPROF` | 2 files / 4 sites | getenv | KEEP-T3 | perf/README.md | [CITPROF] Sub-phase profiler for CitizenBehavior::Update ============================================================ BANISHED_CITPROF=<n> — break the |
| `BANISHED_STRESS` | 11 files / 20 sites | other/ref | KEEP-T3 | CLAUDE.md (+5) | ============================================================ Resolve the helpers on first use. AttendanceComponent_InitFuncs had NO caller |
| `BANISHED_STRESS_ALLOW_POCKETS` | 1 files / 2 sites | getenv | KEEP-T3 | perf/OPTIMISATION_LOG.md (+1) | opening onto water or another footprint; every resident then fails the faithful home re-validation and starves (seen 2026-09-04: door at (215,188) bet |
| `BANISHED_STRESS_COMPPROF` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/OPTIMISATION_LOG.md (+1) | [STRESS] BANISHED_STRESS_COMPPROF=2: attribute the per-frame (slot 8) tick |
| `BANISHED_STRESS_KINDMAP` | 1 files / 1 sites | getenv | KEEP-T3 | perf/README.md | One-time kind-grid dump for offline layout analysis (standalone_kindmap.txt: one row per y, one base-36 kind digit per cell; '#' = occupied by a footp |
| `BANISHED_STRESS_SPEED` | 1 files / 1 sites | other/ref | KEEP-T3 | perf/README.md (+1) | [NON-RETAIL] retail stops at 10x. 20x and 50x extend the sequence so the perf |
| `BANISHED_TOOL_HARNESS` | 6 files / 10 sites | other/ref | KEEP-T3 | .vscode/launch.json (+335) | (msg 0x14 / element id 500) into Slot11 SetLivestockType -- exactly what a player picking an animal in-game runs. This helper survives only as the hea |
| `BANISHED_TORNADO_TRACE` | 1 files / 2 sites | getenv | KEEP-T3 | tools/disaster_report.sh | ---- tracing ------------------------------------------------------------- The lane is opt-in and the volume is a handful of lines per tornado (spawn, |
| `BANISHED_TREE_GUARDS` | 1 files / 3 sites | getenv | KEEP-T3 | perf/OPTIMISATION_LOG.md (+1) | A non-null 64-bit value that cannot be a live heap pointer: misaligned, or a half-written pointer (valid high dword, tiny low dword — the freelist-lin |
| `BANISHED_TREE_PREFETCH` | 1 files / 2 sites | getenv | KEEP-T3 | perf/OPTIMISATION_LOG.md (+1) | PERF A/B: successor prefetch in the update-tree walk. OFF by default — measured 2026-09-04 at 800x10: no prefetch 0.25 ms/iter, one level 0.26, two le |
| `BANISHED_UNREG_CENSUS` | 1 files / 2 sites | getenv | KEEP-T3 | tools/unreg_full.py | ...and dump the census a fixed number of gameplay frames later, so an unattended run yields the table without needing a clean process exit (the crash |
| `BANISHED_ACH_TIME_SCALE` | 1 files / 1 sites | other/ref | DELETE |  | AchClock_TimeScale() is 1.0 unless BANISHED_ACH_TIME_SCALE is set, so |
| `BANISHED_ACH_TOAST_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_ACH_TRACE` | 1 files / 1 sites | other/ref | DELETE |  | NB: BANISHED_ACH_TRACE is already the achievement CENSUS interval |
| `BANISHED_ACH_YEAR_BOOST` | 1 files / 1 sites | other/ref | DELETE |  | The debug panel resource lives in a .pkm mounted after Startup; keep trying. Non-retail clock knobs for the time-gated achievements: advance the year |
| `BANISHED_AGRI_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_AMBIENT_FORCE_SPAWN` | 1 files / 2 sites | getenv | DELETE |  | Debug: BANISHED_AMBIENT_FORCE_SPAWN=1 spawns the ambient entity once on |
| `BANISHED_AMBIENT_TRACE` | 3 files / 7 sites | getenv | DELETE |  |  |
| `BANISHED_ARENA_MB` | 1 files / 4 sites | getenv | DELETE |  | realloc-and-copy, every multi-range scan) never executes, and 12.75M shadow-verified operations said nothing about it (WAVE9_LANE_E2.md 5.1). |
| `BANISHED_ARR_WATCH` | 1 files / 2 sites | getenv | DELETE |  | [DIAG] BANISHED_ARR_WATCH="stride,bytes": arm a hardware WRITE watchpoint on the |
| `BANISHED_ATTENDANCE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | One-shot bring-up trace. Default off; BANISHED_ATTENDANCE_TRACE=1 enables. Reports |
| `BANISHED_AUTOSAVE_TRACE` | 3 files / 8 sites | getenv | DELETE |  | BANISHED_AUTOSAVE_TRACE=1: one line every time the timer is (re-)armed. This is |
| `BANISHED_BIRTH_TRACE` | 1 files / 2 sites | getenv | DELETE |  | BANISHED_BIRTH_TRACE — narrate the delivery/conception path. Off by default; the |
| `BANISHED_BOAT_TRACE` | 4 files / 7 sites | getenv | DELETE |  |  |
| `BANISHED_BUILDERS` | 1 files / 1 sites | other/ref | DELETE |  | Hold one building panel open so its layout can be captured (BANISHED_PANEL_HOLD). |
| `BANISHED_BUILDER_AUTOPOOL` | 2 files / 2 sites | other/ref | DELETE |  | for every profession (ProbeAllProfessions), which is how "the site is registered but shows 'of 0'" gets told apart from "the site never registered". |
| `BANISHED_BUILDER_COUNT` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_BUILDUI_TRACE` | 1 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_BUILD_DEMO_CEMETERY` | 1 files / 1 sites | other/ref | DELETE |  | requirement). It registers into *(GameWorld+0xf0)+0x50. Both are ordinary placed structures with an authored CreatePlacedDescription, so the generic r |
| `BANISHED_BUILD_DEMO_CHAPEL` | 1 files / 1 sites | other/ref | DELETE |  | so the generic ring scan sizes them and the fallbacks are never used. |
| `BANISHED_BUILD_DEMO_FISHINGDOCK` | 2 files / 4 sites | getenv | DELETE |  | Picking::Scene and bump each covered cell's floor counter (cellRec+0x2d), and the matching refine in mapquery.cpp's f3e0 ray-casts that scene so a cit |
| `BANISHED_BUILD_DEMO_FORCE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_BUILD_DEMO_FORESTERLODGE` | 1 files / 1 sites | other/ref | DELETE |  | instance in the world there is literally nothing to measure. All four are ordinary placed structures with an authored CreatePlacedDescription, so the |
| `BANISHED_BUILD_DEMO_FRAMES` | 2 files / 5 sites | getenv | DELETE |  | 120 frames) so the mesh progression + completion demonstrate visibly. Real BuildStructure labor (2c) computes and raises the true target. Ramp budget |
| `BANISHED_BUILD_DEMO_GATHERERHUT` | 2 files / 3 sites | getenv | DELETE |  | 0 Apple  1 Berries  2 Cherry  3 Peach  4 Pear  5 Plum   (input flags 0x21) 6 Wheat                                                 (input flags 0x30) |
| `BANISHED_BUILD_DEMO_HOUSE` | 1 files / 2 sites | getenv | DELETE |  | House-demo mode (BANISHED_BUILD_DEMO_HOUSE): pick the FIRST WoodHouse (residence) |
| `BANISHED_BUILD_DEMO_HUNTERLODGE` | 2 files / 4 sites | getenv | DELETE |  | index that is empty during StartTown because characters register during their own Startup. Must run BEFORE FoodWorkers_Tick's first pass (frame 400) s |
| `BANISHED_BUILD_DEMO_MARKET` | 1 files / 1 sites | other/ref | DELETE |  | generic ring scan handles them and the fallback footprints are never used. |
| `BANISHED_BUILD_DEMO_PASTURE` | 2 files / 5 sites | getenv | DELETE |  | SECTION, reached through the master's interact CHILD list, which InteractComponent slot 6 rebuilds from the MultiObject children. children=0 therefore |
| `BANISHED_BUILD_DEMO_SCHOOLHOUSE` | 1 files / 1 sites | other/ref | DELETE |  |  |
| `BANISHED_BUILD_DEMO_TAVERN` | 1 files / 1 sites | other/ref | DELETE |  | reports ConsumeProduceComponent NOT PLACED with a tavern in the world, the component lives on a different template — Tailor/ToolMaker are the next can |
| `BANISHED_BUILD_DEMO_TRADINGPOST` | 1 files / 3 sites | getenv | DELETE |  | ── BANISHED_BUILD_DEMO_TRADINGPOST — one COMPLETED trading post ON THE RIVER ── |
| `BANISHED_BUILD_SYNC_DELIVERED` | 2 files / 4 sites | getenv | DELETE |  | DELIVERED-SYNC: GatherRawMaterials credits delivered (field_48) from its OnComplete, which fires from the citizen's DropOff→CompleteTask action — a n |
| `BANISHED_BUILD_SYNC_LABOR` | 1 files / 3 sites | getenv | DELETE |  | site (BuildStructure_ConstructForSite → the faithful full ctor sizes the build-position byte array from the site footprint) and drive its Execute when |
| `BANISHED_BUILD_TRACE` | 23 files / 36 sites | getenv | DELETE |  |  |
| `BANISHED_CAMERA_SELFTEST` | 1 files / 2 sites | getenv | DELETE |  | Diagnostic self-test (env BANISHED_CAMERA_SELFTEST): inject synthetic input so |
| `BANISHED_CAMERA_SWEEP` | 1 files / 2 sites | getenv | DELETE |  | Harness (env BANISHED_CAMERA_SWEEP=cx,cy,radius[,dist[,periodFrames]]): orbit the |
| `BANISHED_CANDLIST_CENSUS` | 1 files / 2 sites | getenv | DELETE |  | ============================================================ |
| `BANISHED_CARRY_TRACE` | 1 files / 1 sites | getenv | DELETE |  | the sampler showed getenv/strcmp from here inside the EntityList phase. |
| `BANISHED_CITIZENAI_TRACE` | 10 files / 20 sites | getenv | DELETE |  | was a getenv PER CALL (hot: every CitizenComponent_Update) |
| `BANISHED_CITIZEN_CENSUS` | 1 files / 2 sites | getenv | DELETE |  | --------------------------------------------------------------------------- |
| `BANISHED_CITIZEN_SPAWNPOS` | 1 files / 2 sites | getenv | DELETE |  | at startConfig+0x40 (the float just before the citizen ExternalRef at +0x48). Faithful core behaviour (the structure Foundation-F is likewise |
| `BANISHED_CITIZEN_VARIANT` | 1 files / 4 sites | getenv | DELETE |  | (>=0x100 byte buffer) with a 4-gram Markov name from Game/Names/{Male,Female}.ndb. |
| `BANISHED_CITPROF` | 2 files / 4 sites | getenv | DELETE |  | ============================================================ [CITPROF] Sub-phase profiler for CitizenBehavior::Update ================================ |
| `BANISHED_CLEAR_DESTROY` | 1 files / 2 sites | getenv | DELETE |  | with the job (and its icon) left standing. Default ON when the controller is up. |
| `BANISHED_CLEAR_PLACE_OVER` | 1 files / 3 sites | getenv | DELETE |  | std::strcmp (BANISHED_CLEAR_PLACE_OVER) |
| `BANISHED_CLEAR_TRACE` | 3 files / 6 sites | getenv | DELETE |  |  |
| `BANISHED_COMPENT_LOAD_TRACE` | 1 files / 5 sites | getenv | DELETE |  | On load: resize component array to match serialized count |
| `BANISHED_COMP_VALIDATE` | 2 files / 3 sites | getenv | DELETE |  | Serialize each worker InternalBasePtr and validate type |
| `BANISHED_CONTROLUI_TRACE` | 2 files / 4 sites | getenv | DELETE |  | and therefore off by default — so g_TypeId_UIElementBase and g_TypeId_CheckboxElement are both normally null. |
| `BANISHED_CPWORK_TRACE` | 1 files / 4 sites | getenv | DELETE |  | [CPWork] Execute OUTCOME METER (BANISHED_CPWORK_TRACE). One counter per exit point, |
| `BANISHED_CROPFIELD_TRACE` | 11 files / 27 sites | getenv | DELETE |  | [diag] BANISHED_CROPFIELD_TRACE — did the spawn-time scale write land on a real |
| `BANISHED_DC20_TRACE` | 1 files / 3 sites | getenv | DELETE |  | BANISHED_DC20_TRACE=1 — log dc20's stale-occupant eviction decisions (see the branch). |
| `BANISHED_DEBUG_PANEL` | 3 files / 9 sites | getenv | DELETE |  |  |
| `BANISHED_DECAL_TRACE` | 3 files / 12 sites | getenv | DELETE |  | the slot's owner ptr (+0), and shrink the two sub-lists embedded at +8 and +0x18 to zero length. 1:1 port of the exe body. |
| `BANISHED_DEFER_WINDOW_SHOW` | 1 files / 1 sites | other/ref | DELETE |  | the splash has a frame to show. Without this the window is on screen and empty for ~450ms every boot; see boot_window.h for the measurements. No-op (a |
| `BANISHED_DELIVER_BOOST` | 1 files / 2 sites | getenv | DELETE |  | [test-only] BANISHED_DELIVER_BOOST: skip the Time(300.0) carry-timer |
| `BANISHED_DELIVER_TRACE` | 6 files / 9 sites | getenv | DELETE |  | [diag] BANISHED_DELIVER_TRACE — watch the deposit half of the haul-to-storage leg. |
| `BANISHED_DEMO_TRADINGPOST_COUNT` | 1 files / 2 sites | getenv | DELETE |  | puts the post under water and fails for a misleading reason. ── HOW MANY POSTS ────────────────────────────────────────── |
| `BANISHED_DESTROY_DEMO_FRAMES` | 1 files / 2 sites | getenv | DELETE |  | Kept, opt-in, for bisecting a future regression in the labor path — set BANISHED_DESTROY_SYNC_LABOR=1 to auto-credit +0x54 again (ramped over |
| `BANISHED_DESTROY_SYNC_LABOR` | 1 files / 3 sites | getenv | DELETE |  | ============================================================ Demolition-labor progress bridge — RETIRED 2026-08-23, now OPT-IN |
| `BANISHED_DESTROY_TRACE` | 15 files / 21 sites | getenv | DELETE |  | Diagnostic: the +0x38 removed-from-world broadcast is the engine's dangling- drawable protection (each drawable component RemoveObject's its scene geo |
| `BANISHED_DIAG_PAIRCACHE` | 1 files / 3 sites | getenv | DELETE |  | 0x14001ca60 - the shared intrusive-AVL delete (registered by reg_work_cellbst). |
| `BANISHED_DIALOG_TRACE` | 14 files / 30 sites | getenv | DELETE |  |  |
| `BANISHED_DISABLE_SHADOWS` | 1 files / 2 sites | getenv | DELETE |  | MainLoop, so engine+0x30 is garbage). Shadows are now DEFAULT-ON: mirror the original and read VideoConfig+0x30 so SunLight::DrawShadowMap engages (ve |
| `BANISHED_DLG2_NEGCTRL` | 1 files / 1 sites | other/ref | DELETE |  | ============================================================================ NEGATIVE-CONTROL SELECTOR =============================================== |
| `BANISHED_DLGCTRL_VT` | 1 files / 2 sites | getenv | DELETE |  | It exists so the A/B runs on ONE BINARY: with a control that is a different build, "the tables changed" and "the exe changed" are the same observation |
| `BANISHED_DLGCTRL_VT_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_DRAG_CONTROL` | 1 files / 2 sites | getenv | DELETE |  | --------------------------------------------------------------------------- |
| `BANISHED_DRAIN_AUDIT` | 3 files / 6 sites | getenv | DELETE |  | spawned entity gets freed while its ModelComponent is still linked into Graphics::Scene. |
| `BANISHED_DRAW_TRACE` | 1 files / 1 sites | getenv | DELETE |  | TEMP diagnostic: confirm the in-game render present runs per-frame + how many animation groups the AnimationManager holds (this+0x08) — i.e. whether a |
| `BANISHED_DROP_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Standalone drop-target diagnostic (BANISHED_DROP_TRACE, default off). |
| `BANISHED_DUMP_SLOT3` | 3 files / 4 sites | getenv | DELETE |  | lesson "Live vtable slot must be wired in InstallLive": the first factory call runs FillTypedVtable, which RESETS every slot (including [3] back to th |
| `BANISHED_EMPLOY_PROBE` | 1 files / 5 sites | getenv | DELETE |  | --- Whole-registry employment probe (BANISHED_EMPLOY_PROBE, diagnostic-only) ----- |
| `BANISHED_EMPLOY_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_ENTITY_ROSTER` | 1 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_EVENT_PROBE_NAME` | 1 files / 2 sites | getenv | DELETE |  | The scenario/achievement popup (0x1a-0x1d) source op_6830 (0x140096830) and the ribbon / EventPopupDialog paths are unaffected and stay live for any r |
| `BANISHED_EVENT_TRACE` | 4 files / 7 sites | getenv | DELETE |  |  |
| `BANISHED_EXEC_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Per-action executor diagnostic (BANISHED_EXEC_TRACE). Default-off. Logs each |
| `BANISHED_EXTRA_HOUSES` | 1 files / 5 sites | getenv | DELETE |  | ── BANISHED_EXTRA_HOUSES=N — N extra COMPLETED, EMPTY houses (test scaffolding) ── |
| `BANISHED_FINISH_BUILDS` | 2 files / 2 sites | other/ref | DELETE |  | Finish every structure still under construction, the way the stress placer |
| `BANISHED_FIRE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | does not flood the log; the per-tick ignition roll is the only high-frequency site and it is sampled, not printed every tick. |
| `BANISHED_FISHING_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_FISHWORK_TRACE` | 1 files / 5 sites | getenv | DELETE |  | ============================================================ |
| `BANISHED_FLAT_SPLAT` | 1 files / 2 sites | getenv | DELETE |  | 4. Blend/splat texture -> WorldGeom+0x140. Real cf440 produces the rock/grass/AO splat (green grass, rock on slopes/peaks). Set |
| `BANISHED_FLOOD_DIAG` | 1 files / 2 sites | getenv | DELETE |  | BFS enqueues a null queue node, and the head-deref below faults (READ of 0x0). |
| `BANISHED_FLOOR_TRACE` | 2 files / 3 sites | getenv | DELETE |  | BANISHED_FLOOR_TRACE (code/Game/Game/mapquery.cpp) — shared with the consumer side. |
| `BANISHED_FNHOOK` | 2 files / 2 sites | other/ref | DELETE |  | ExternalFactory::ExpandRelativePath / FindOrLoad calls (a Runtime-internal over-release), after which it is freed yet GetDataDirectory keeps AddRef'in |
| `BANISHED_FNHOOK_DATADIR` | 1 files / 1 sites | other/ref | DELETE |  | over-release), after which it is freed yet GetDataDirectory keeps AddRef'ing the dead String -> the +0x100000000 free-list bit-32 flip -> crash. (Pinn |
| `BANISHED_FOOD_DIAG` | 2 files / 2 sites | other/ref | DELETE |  | "we assign workers and nothing gets produced" report. Orchard and Pasture were already default-on, which is exactly why only these four looked broken. |
| `BANISHED_FOOD_PANEL` | 1 files / 1 sites | other/ref | DELETE |  | Dialog harness for the six food-production panels: opens each building's info panel and reports the controllers on it plus the component's CanCommand |
| `BANISHED_FOOD_PLACE` | 1 files / 1 sites | other/ref | DELETE |  | Placement + SLOT-COVERAGE harness: where each food building landed, and which of its 22 component vtable slots are actually live versus still holding |
| `BANISHED_FOOD_TOPUP` | 3 files / 4 sites | getenv | DELETE |  | and died out when they aged past maxAge (~15).  Graduated 2026-08-15 on an A/B at identical seed (12345, FORCE_SPEED=40), food held |
| `BANISHED_FORCE_BIRTH` | 1 files / 1 sites | other/ref | DELETE |  | loops are byte-faithful; retail relies on a zero-count stack never existing, which is exactly what this slot guarantees. Do not guard them. |
| `BANISHED_FORCE_CONSTRUCT` | 2 files / 3 sites | getenv | DELETE |  | caller then skips the competing AI state machine). Self-gated; no-op when env unset. Organic-construction test driver (construction_force_harness.cpp, |
| `BANISHED_FORCE_DEVICE_LOST` | 3 files / 4 sites | getenv | DELETE |  | DLL helpers we called. That is what keeps the check from restating the code above it. |
| `BANISHED_FORCE_FORESTERWORK` | 2 files / 4 sites | getenv | DELETE |  | Phase B-real force-ForesterWork driver (forester_force_harness.cpp, env |
| `BANISHED_FORCE_HARVEST` | 1 files / 2 sites | getenv | DELETE |  | ── PHASE 5 PRODUCE VERIFICATION (env BANISHED_FORCE_HARVEST, default OFF) ── |
| `BANISHED_FORCE_LOD0` | 1 files / 1 sites | other/ref | DELETE |  | compares it against `vtable[3]` of a live object to decide whether the object is a MeshInstance. Register the address and ResolveAddr hands back OUR f |
| `BANISHED_FORCE_SNOW` | 1 files / 2 sites | getenv | DELETE |  | showing snow on ~half the ground. The Weather ctor (FUN_140093f60) zero-inits all four, which is exactly the spring/clear/no-snow state, so push them |
| `BANISHED_FORCE_WORK` | 5 files / 10 sites | getenv | DELETE |  | [STANDALONE GUARD] base is null when obj is not an InteractDescription OR the type-id global (0x1401c7938) is unfilled/unregistered in standalone. The |
| `BANISHED_FORESTER_SCAN_TRACE` | 5 files / 9 sites | getenv | DELETE |  | [diag] Gate trace for the harvest-yield investigation (BANISHED_FORESTER_SCAN_TRACE). |
| `BANISHED_FPS_TRACE` | 1 files / 4 sites | getenv | DELETE |  | [FPS TRACE] env BANISHED_FPS_TRACE: wall-clock fps over 120-frame windows (entry-to- |
| `BANISHED_FRUIT_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Phase-1 bring-up verification trace (env BANISHED_FRUIT_TRACE, default off |
| `BANISHED_FS_RELATIVE` | 1 files / 3 sites | getenv | DELETE |  | draws) — A/B (session 5): FS-rel ON => crash/stall in every run; FS-rel OFF => menu draws. The real menu-crash fix is the FindOrLoad serialize lock (I |
| `BANISHED_FUEL_ACCEL` | 1 files / 2 sites | getenv | DELETE |  | --- Accumulate timers --- |
| `BANISHED_GATHER_TRACE` | 1 files / 2 sites | getenv | DELETE |  | ── Forage-rejection census (BANISHED_GATHER_TRACE) ────────────────────────── |
| `BANISHED_GOAL7_DUMP` | 1 files / 3 sites | getenv | DELETE |  |  |
| `BANISHED_GOAL_PROBE` | 2 files / 4 sites | getenv | DELETE |  | [GOAL PROBE] env BANISHED_GOAL_PROBE: evaluate one synthetic BuildingGoal at |
| `BANISHED_GROWTH_FORCE_HARVEST` | 1 files / 2 sites | getenv | DELETE |  | M-A verification harness (env BANISHED_GROWTH_FORCE_HARVEST, default off → |
| `BANISHED_GROWTH_TRACE` | 2 files / 12 sites | getenv | DELETE |  | of tile-record 0 and wrote to ~0x10 → access violation (misattributed to a "missing resourceMgr grid"; the grid exists — see maptilegrid.cpp). |
| `BANISHED_HAPPINESS_TRACE` | 1 files / 2 sites | getenv | DELETE |  | bytes that select which arm of IsActive decided it -- mode(+0x20)==5 is the plain radius source (chapel), needsStaff(+0x24) the staffed one, needsGood |
| `BANISHED_HAUL_TRACE` | 1 files / 2 sites | getenv | DELETE |  | ── BANISHED_HAUL_TRACE — open C's second half ──────────────────────────────────── |
| `BANISHED_HDR_TRACE` | 1 files / 2 sites | getenv | DELETE |  | --------------------------------------------------------------------------- |
| `BANISHED_HEAT_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_HELP_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_HERD_TRACE` | 3 files / 3 sites | getenv | DELETE |  |  |
| `BANISHED_HOTKEY_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Env-gated one-shot-per-element trace to verify the shortcut->key chain |
| `BANISHED_HUNTERLODGE_AT_START` | 1 files / 2 sites | getenv | DELETE |  | So record the request and let StartTown_DeferredSpawn_Tick place it a few hundred frames in, scoring candidates by live animal density. Set |
| `BANISHED_HUNTERLODGE_FRAME` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_HUNTERLODGE_WINDOW` | 1 files / 2 sites | getenv | DELETE |  | ── Score every candidate footprint by how many animals sit inside the work radius of its centre. Same window/stride as the gatherer hut's resource sca |
| `BANISHED_IBP_TRACE` | 1 files / 2 sites | getenv | DELETE |  | [diag] BANISHED_IBP_TRACE — an IBP that reads null after a load has three |
| `BANISHED_ICON_TRACE` | 2 files / 3 sites | getenv | DELETE |  | BANISHED_ICON_TRACE prints one line per icon that stays out of the scene — see |
| `BANISHED_IMMUI_AUTOCLICK` | 1 files / 2 sites | getenv | DELETE |  | BANISHED_IMMUI_AUTOCLICK=<elementId> (39000 = Allow, 39001 = Deny), every |
| `BANISHED_IMMUI_AUTOCLICK_EVERY` | 1 files / 2 sites | getenv | DELETE |  | BANISHED_IMMUI_AUTOCLICK=<elementId> (39000 = Allow, 39001 = Deny), every |
| `BANISHED_INTERNAL_UNREG` | 1 files / 2 sites | getenv | DELETE |  | Kill-switch for the Object::Internal unregistration below (BANISHED_INTERNAL_UNREG=0 |
| `BANISHED_KILL_CITIZENS` | 1 files / 1 sites | other/ref | DELETE |  | Kill N citizens on a chosen frame so the removal teardown (HandleLifeEvent -> MarkForRemoval -> the +0x38 broadcast -> CitizenComponent::OnRemove) can |
| `BANISHED_LIMIT_TRACE` | 2 files / 3 sites | getenv | DELETE |  |  |
| `BANISHED_LIVE_CITIZENAI_TIDS` | 1 files / 1 sites | other/ref | DELETE |  | fire=InteractComponent c7900; serialize=Pasture/Animal ids). openfuncs step 2 — CitizenAI/Citizen/Happiness type-id globals (c70e8/c7158/c42c0; |
| `BANISHED_LIVE_FORESTER_CMD` | 1 files / 1 sites | other/ref | DELETE |  | ── Forester / Attendance: promoted from "command pair only" to full live controllers 2026-08-15, which is why the two flags are renamed rather than |
| `BANISHED_LIVE_SEAT_SHUFFLE` | 1 files / 1 sites | other/ref | DELETE |  | deterministic (always the first registered building with room). Registering it makes seating random, which is retail-correct but is a real, observable |
| `BANISHED_LOADRES_NEGCONTROL` | 1 files / 2 sites | getenv | DELETE |  | NEGATIVE CONTROL, kept so it can be re-run rather than re-invented. |
| `BANISHED_MAPGEN_TRACE` | 1 files / 2 sites | getenv | DELETE |  | [diag] BANISHED_MAPGEN_TRACE=1 -- an ENTER and an EXIT line per Serialize pass |
| `BANISHED_MAPSTAMP_TRACE` | 2 files / 6 sites | getenv | DELETE |  | ── Footprint-mask census (BANISHED_MAPSTAMP_TRACE) ────────────────────────── |
| `BANISHED_MAP_SEED` | 1 files / 3 sites | getenv | DELETE |  | BANISHED_MAP_SEED=<uint> forces a deterministic map seed (testing — |
| `BANISHED_MARKETBST_TRACE` | 1 files / 2 sites | getenv | DELETE |  | ============================================================ |
| `BANISHED_MARKET_OVERSTOCK` | 1 files / 1 sites | other/ref | DELETE |  | Force-feed the market one resource past 2x its target share so MarketWork slot 4 stage 3 can take its SURPLUS SHED arm (0x1400f09eb) — the only path i |
| `BANISHED_MARRY_TRACE` | 1 files / 4 sites | getenv | DELETE |  | NOTE: skipped for children — start-town citizens currently spawn with age=0 (< adultAge), so this whole assignment section no-ops until they age up or |
| `BANISHED_MATURE_TREES` | 2 files / 5 sites | getenv | DELETE |  | write the growth-staged mesh scale. The shared FUN_140003f60 reimpl finds it via the raw .rdata ModelComponent type-id (0x1401c7a50), which resolves N |
| `BANISHED_MODEL_TRACE` | 4 files / 9 sites | getenv | DELETE |  | BANISHED_MODEL_TRACE — instrumentation for the slot-6 (OnAdded) verification pass. |
| `BANISHED_MOD_PROBE` | 2 files / 6 sites | getenv | DELETE |  | `MOV RAX,[0x1801fc6b0]; RET`, and `Modification::SetupFileSystem` loads `[0x1801fc6a8]` into R15 at 0x1800aebe1 as its FileSystem*. |
| `BANISHED_MOUSE_DRAGSKEW` | 1 files / 1 sites | other/ref | DELETE |  | It must be scored on an ASYMMETRIC subject: the toolbar button this lane drags is 40x40, so its centre grab offset is (20,20) and a transposition of |
| `BANISHED_MOUSE_INJECT` | 1 files / 2 sites | other/ref | DELETE |  | BANISHED_MOUSE_INJECT hover/drag route (standalone/) |
| `BANISHED_NEEDS_ACCEL` | 1 files / 2 sites | getenv | DELETE |  | --- Hunger accumulation --- |
| `BANISHED_NOMAD_ANY_SEASON` | 1 files / 2 sites | other/ref | DELETE |  | Test fixtures (all env-gated, byte-unchanged when unset — NOT retail behaviour): BANISHED_NOMAD_TRACE=1            stderr trace of every state/event/s |
| `BANISHED_NOMAD_AUTOACCEPT` | 1 files / 3 sites | getenv | DELETE |  | BANISHED_NOMAD_TRACE=1            stderr trace of every state/event/spawn/accept BANISHED_NOMAD_TIMER_SCALE=<f>    multiply the 2..6-year wait timer ( |
| `BANISHED_NOMAD_TIMER_SCALE` | 1 files / 2 sites | getenv | DELETE |  | Test fixtures (all env-gated, byte-unchanged when unset — NOT retail behaviour): BANISHED_NOMAD_TRACE=1            stderr trace of every state/event/ |
| `BANISHED_NOMAD_TRACE` | 3 files / 4 sites | getenv | DELETE |  | the component inert exactly as before this port.  Test fixtures (all env-gated, byte-unchanged when unset — NOT retail behaviour): |
| `BANISHED_NO_MENU_GEOM` | 1 files / 2 sites | getenv | DELETE |  | the camera at a zero (degenerate) transform and misaligns the tail (observed: pos/qrot all zero, menu hangs). It is, however, the source of the interm |
| `BANISHED_NO_RES_VARIANT` | 1 files / 1 sites | getenv | DELETE |  | RNG, so the build order decides which seeds each field receives — it must run variant-then-scatter or trees end up with a different distribution than |
| `BANISHED_NO_SKYMESH` | 1 files / 2 sites | getenv | DELETE |  | RT blue mean ~0.18 vs retail ~0.68), so the water — whose shader is sky-reflection-dominated on the tilted camera — renders dark navy. Adding the sky |
| `BANISHED_NO_WATER` | 1 files / 2 sites | getenv | DELETE |  | drawn by geometry_draw.cpp modes 0x6000/0x8000. param_2 = hf (centerlines at hf+0x18, features at hf+0x48). * BANISHED_WATER_CONTOUR=0 -> the minimal |
| `BANISHED_OBJAUDIT` | 3 files / 3 sites | other/ref | DELETE |  | [OBJAUDIT] env BANISHED_OBJAUDIT: periodic InternalFactory handle-table integrity |
| `BANISHED_OBJAUDIT_FRAMES` | 1 files / 2 sites | getenv | DELETE |  | scan (table[i]+0x08 == i for every i) plus the running tally of objects freed while still registered. A growing `count` with a flat live-object count |
| `BANISHED_ONREMOVE_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_OPTIONS_TAB` | 1 files / 2 sites | getenv | DELETE |  | HARNESS KNOB (Wave 10). BANISHED_OPTIONS_TAB=<n> overrides the |
| `BANISHED_OPTIONS_TRACE` | 1 files / 1 sites | getenv | DELETE |  | logic itself is verified correct — see notes: ApplyChangedSettings runs and returns success; the standalone gap is the render backend not recreating t |
| `BANISHED_ORCHARD_GROW` | 1 files / 1 sites | other/ref | DELETE |  | Fast-forward tree maturity, then fruit ripeness, so the harvest half of the orchard state machine is reachable in one run rather than a multi-hour soa |
| `BANISHED_OUTLINE_TRACE` | 1 files / 3 sites | getenv | DELETE |  | -> bail. We do that FindComponent inline (FindModelComp) with the GetTypeIdByName fallback so the citizen's ModelComponent is actually found. (Still g |
| `BANISHED_PAGE4_TEST` | 1 files / 2 sites | getenv | DELETE |  | PAGE-4 STICK TEST (BANISHED_PAGE4_TEST=1, test-only). Forces the user |
| `BANISHED_PAGE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | One-shot page-routing diagnostic (BANISHED_PAGE_TRACE). Dumps every input feeding |
| `BANISHED_PARTICLE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Throttled diagnostic trace (BANISHED_PARTICLE_TRACE) |
| `BANISHED_PASTUREPANEL_SELECT` | 1 files / 1 sites | other/ref | DELETE |  | (msg 0x14 / element id 500) into Slot11 SetLivestockType -- exactly what a player picking an animal in-game runs. This helper survives only as the hea |
| `BANISHED_PASTURE_AUTOTYPE` | 5 files / 8 sites | getenv | DELETE |  | "the pasture stocks itself at town start" behaviour — there is no auto-assignment of pasture+0x28 anywhere in retail. |
| `BANISHED_PASTURE_FILL` | 2 files / 2 sites | other/ref | DELETE |  | One-shot pasture stocking for the animal-wave test rig. No-op unless |
| `BANISHED_PASTURE_HERDERS` | 1 files / 1 sites | other/ref | DELETE |  | THE VIRTUAL PLAYER: clicks workerCountUp (the retail first-seat path) on every under-capacity workplace. Automatic for unattended runs only; replaces |
| `BANISHED_PASTURE_TRACE` | 3 files / 7 sites | getenv | DELETE |  | Reproduction timer: accumulates delta time, fires birth event when threshold exceeded (configInterval * 300.0f seconds -> ticks). |
| `BANISHED_PATHGRAPH_FAST` | 1 files / 3 sites | getenv | DELETE |  | TEST-ONLY accelerator: BANISHED_PATHGRAPH_FAST forces the path branch every |
| `BANISHED_PATH_TRACE` | 1 files / 5 sites | getenv | DELETE |  | Every one of these is called behind an `if (ptr)` with a -1/no-op fallback, so an unresolved helper degrades silently into a bogus route rather than a |
| `BANISHED_PERSIST_TRACE` | 1 files / 2 sites | getenv | DELETE |  | [PersistMap] BANISHED_PERSIST_TRACE — InternalFactory id-space density. |
| `BANISHED_PF_SYNC` | 1 files / 2 sites | getenv | DELETE |  | Async is the real (shipping) pathfinder mode. Default ON when seeded; BANISHED_PF_SYNC=1 |
| `BANISHED_PF_TRACE` | 4 files / 9 sites | getenv | DELETE |  | Bug-A diagnostic (BANISHED_PF_TRACE, default off): the decisive per-query |
| `BANISHED_PHASE1_EMPLOY` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_PICKMESH_PROBE` | 2 files / 3 sites | getenv | DELETE |  | coarse terrain-cell fallback works, which is why buildings are "click-the-middle" picky. We drive this directly at spawn for the (static) start-town b |
| `BANISHED_PICKUP_PORT` | 1 files / 2 sites | getenv | DELETE |  | When s_fn_Slot4_Original resolves (address registered) the dispatcher calls it; otherwise the port runs — the address is unregistered and the old fal |
| `BANISHED_PILE_GRID_ATTACH` | 1 files / 1 sites | getenv | DELETE |  | only "0" disables. |
| `BANISHED_PIN_DATADIR` | 2 files / 3 sites | getenv | DELETE |  | so the erosion can never reach 0 (these dir Strings live for the whole process anyway, so an intentional permanent reference is harmless). If this mak |
| `BANISHED_PKG3_CENSUS` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_PLACEMENT_TEMPLATE` | 1 files / 2 sites | other/ref | DELETE |  | as a foundation and needs builders. But do not cite this comment as evidence that the tools are missing -- to drive the real path instead, use crop fi |
| `BANISHED_PLACE_PICK_DIAG` | 1 files / 2 sites | getenv | DELETE |  | Diagnostic for the placed-building click-pick question: report both pick routes for the entity we just created. cellEntity != 0 => EditTool's cell pic |
| `BANISHED_PLANTAREA_TRACE` | 2 files / 6 sites | getenv | DELETE |  | addressing the same slot twice. |
| `BANISHED_POINTLIST_TRACE` | 1 files / 2 sites | getenv | DELETE |  | [diag] BANISHED_POINTLIST_TRACE — the RAW point list as loaded, before the |
| `BANISHED_POOL_DIAG` | 1 files / 3 sites | getenv | DELETE |  | [POOLDIAG] env BANISHED_POOL_DIAG — each grow doubles the chunk. A pool that |
| `BANISHED_POP_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Phase-1 bring-up verification trace (env BANISHED_POP_TRACE, default off → |
| `BANISHED_PRELOAD_DUMP` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_PRELOAD_REFS` | 1 files / 2 sites | getenv | DELETE |  | restores the same invariant. It is not extra work: every one of these is about to be loaded anyway as the stream resolves its references. |
| `BANISHED_PRODUCE_SPAWN_ENTITY` | 2 files / 4 sites | getenv | DELETE |  | [standalone] Entity-lifecycle gate (BANISHED_PRODUCE_SPAWN_ENTITY, default ON since |
| `BANISHED_PRODUCE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | allocates new entities, positions them, registers with resource tracker. Returns 0 (action complete). ================================================ |
| `BANISHED_PRODUI_FLICKER` | 1 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_PROFESSION_TRACE` | 3 files / 5 sites | getenv | DELETE |  |  |
| `BANISHED_PROFKEY_TRACE` | 2 files / 4 sites | getenv | DELETE |  | BANISHED_PROFKEY_TRACE - pairs with the [ProfKey] lines in |
| `BANISHED_PROFLIST_MODE` | 1 files / 3 sites | getenv | DELETE |  | getenv (BANISHED_LIVE_PROFESSIONLIST / BANISHED_PROFLIST_MODE) |
| `BANISHED_QUARRY_TRACE` | 2 files / 5 sites | getenv | DELETE |  |  |
| `BANISHED_RAWMAT_PICKUP_TRACE` | 2 files / 6 sites | getenv | DELETE |  | BANISHED_RAWMAT_PICKUP_TRACE — log each posted Pickup work. |
| `BANISHED_REGENTITY_TRACE` | 2 files / 9 sites | getenv | DELETE |  |  |
| `BANISHED_REGION_TRACE` | 1 files / 2 sites | getenv | DELETE |  | exe grow-and-copy. Leaf except 0x1400021d0 (registered). |
| `BANISHED_REMOVE_TRACE` | 2 files / 5 sites | getenv | DELETE |  | silently leaves canRemove=false for every tile — indistinguishable in the tile colours from "nothing here is removable". Report the resolve outcomes o |
| `BANISHED_RESOURCEPRODUCE_TRACE` | 1 files / 2 sites | getenv | DELETE |  | One-shot bring-up trace. Default off; BANISHED_RESOURCEPRODUCE_TRACE=1 enables. |
| `BANISHED_RES_CAP` | 1 files / 2 sites | getenv | DELETE |  | with no resources at all. The old 16384 "safety" cap was sized from a Mountains medium map (~10k trees) and clipped every Valleys map (a large Valleys |
| `BANISHED_RES_IN_TOWN` | 1 files / 2 sites | getenv | DELETE |  | 0 -> pile -> 0 normally. |
| `BANISHED_RES_OCCUPANCY` | 2 files / 3 sites | getenv | DELETE |  | Grid claim for the spawned pile (see the long note at the write below). Default ON, |
| `BANISHED_RES_ONLY` | 1 files / 2 sites | getenv | DELETE |  | DIAG harness: BANISHED_RES_ONLY=<poolIndex> spawns only that pool (isolate trees). |
| `BANISHED_RIVER_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_ROAD_NO_MESH` | 1 files / 2 sites | getenv | DELETE |  | Rebuild the road mesh so the new cells render. |
| `BANISHED_ROAD_TRACE` | 2 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_RT_FACTREST` | 1 files / 1 sites | other/ref | DELETE |  | Achievement 4, Collision2D 2, Audio 2, IO 1, Pathfinding 1, Picking 1). Cross-namespace on purpose: what the 35 share is the SHAPE and the fact that t |
| `BANISHED_RT_FREEZE_EARLY` | 1 files / 1 sites | other/ref | DELETE |  | caching door before init finished and compares it with the answer that name gave then. Returns the number of LIVE SPLIT BRAINS (answer changed AND the |
| `BANISHED_SAVELOAD_DTOR_TRACE` | 1 files / 2 sites | getenv | DELETE |  | this project's canonical "green line over dead code" (reference_uncalled_port_is_not_verified.md); a destructor gets the same treatment for the same r |
| `BANISHED_SAVE_TIMING` | 3 files / 9 sites | getenv | DELETE |  | QueryPerformanceCounter for the BANISHED_SAVE_TIMING probe |
| `BANISHED_SCENARIO_TRACE` | 1 files / 1 sites | getenv | DELETE |  | Saved scenario: load the save file first, then start from saved state This branch had never been exercised before 2026-08-30 (the dialog was unreachab |
| `BANISHED_SCENE_CENSUS` | 1 files / 1 sites | other/ref | DELETE |  | so a recycled node is reported the frame it appears rather than whenever Clean's vtable+0x40 dispatch happens to reach it ("EXEC of <heap address>"). |
| `BANISHED_SCENE_WATCH` | 1 files / 1 sites | other/ref | DELETE |  | [diag] BANISHED_SCENE_WATCH — validate the node array BEFORE Clean walks it, |
| `BANISHED_SCREENSHOT_TRACE` | 1 files / 1 sites | getenv | DELETE |  | Signature matches fn_QuickSave_t in gameupdateloop.cpp: void(void*). ============================================================================ |
| `BANISHED_SEAT_TRACE` | 5 files / 9 sites | getenv | DELETE |  | re-identified AssignHousing itself; the interesting frame is one level up, and guessing which of its call sites it was has been wrong three times. Fir |
| `BANISHED_SEED_FOOD` | 2 files / 4 sites | getenv | DELETE |  | fuel clamp ran second and took only what food left (225 units / 900 volume) — so food reserved against ~700 volume of fuel that never landed, `cap - f |
| `BANISHED_SEED_FUEL` | 1 files / 6 sites | getenv | DELETE |  | Firewood stand-in size. Deliberately generous: the point is to take the first winter OUT of the measurement, not to model a fuel economy. Override wit |
| `BANISHED_SER_DUMPCOMP` | 1 files / 2 sites | getenv | DELETE |  | BANISHED_SER_DUMPCOMP=<n> -- destructive one-shot: before component n |
| `BANISHED_SER_TRACE` | 2 files / 4 sites | getenv | DELETE |  | ============================================================ --------------------------------------------------------------------------- |
| `BANISHED_SHOWTOOL_TRACE` | 1 files / 3 sites | getenv | DELETE |  | [diag] BANISHED_SHOWTOOL_TRACE support. ReadableU is used ONLY by the probe, never as a |
| `BANISHED_SI_CENSUS` | 2 files / 4 sites | getenv | DELETE |  |  |
| `BANISHED_SM_STATE_TRACE` | 2 files / 5 sites | getenv | DELETE |  | Graduated 2026-08-20 to fix a REPORTED fault the stand-ins could not fix: the SM_BUILT_HOOK above fires on the entity's FIRST TICK with no state gate, |
| `BANISHED_SM_TRACE` | 2 files / 10 sites | getenv | DELETE |  | slot 0 (+0x00): Step(node /*ignored*/, input) -> index / -1 |
| `BANISHED_SPAWN_ADULTS` | 1 files / 2 sites | getenv | DELETE |  | --- ADULT-AGE SPAWN FIX (env BANISHED_SPAWN_ADULTS) ------------------- |
| `BANISHED_SPAWN_INITIAL_PREREG` | 1 files / 3 sites | getenv | DELETE |  | (natResComp+0x28 >= threshold 1.0, foresterwork.cpp), so we set that field to a mature value at spawn — the faithful END-STATE of a fully-grown tree ( |
| `BANISHED_SPAWN_TRACE` | 1 files / 4 sites | getenv | DELETE |  | ============================================================ SpawnComponent::Update (0x140055ad0) ==================================================== |
| `BANISHED_START_CONDITION` | 1 files / 2 sites | getenv | DELETE |  | ============================================================ SelectStartConditionOverride (standalone-only, additive) ================================ |
| `BANISHED_STATUSICON_TRACE` | 3 files / 3 sites | getenv | DELETE |  | Diagnostic: settle whether the sprite-sheet RESOURCE is present in standalone (the gating unknown for Phase B rendering). desc+0x20 = SpriteSheet, spr |
| `BANISHED_STORAGEDISPLAY_TRACE` | 1 files / 2 sites | getenv | DELETE |  | helpers below make the two raw-.rdata type-ids (ModelDescription, RawMaterial- Description) resolvable in SA and provide a throttled diagnostic trace. |
| `BANISHED_STORAGE_TRACE` | 9 files / 20 sites | getenv | DELETE |  |  |
| `BANISHED_STREAM_DUMP` | 1 files / 2 sites | getenv | DELETE |  | testing which candidate has the town name we just read sitting at the cursor the string read started from: that pins buffer AND proves whether the cur |
| `BANISHED_STRESS_EXCLUDE` | 1 files / 2 sites | getenv | DELETE |  | ── BANISHED_STRESS_EXCLUDE / BANISHED_STRESS_EXTRA ────────────────────────── |
| `BANISHED_STRESS_EXTRA` | 1 files / 2 sites | getenv | DELETE |  | ── BANISHED_STRESS_EXCLUDE / BANISHED_STRESS_EXTRA ────────────────────────── |
| `BANISHED_STRESS_LIMITS` | 1 files / 2 sites | getenv | DELETE |  | BANISHED_STRESS_LIMITS=<cat>:<n>,... — set town-hall resource LIMITS |
| `BANISHED_STRESS_STOCK` | 1 files / 2 sites | getenv | DELETE |  | then put the resource back the way it was loaded. |
| `BANISHED_SUN_TRACE` | 3 files / 7 sites | getenv | DELETE |  | Optional verification trace: BANISHED_SUN_TRACE=1 dumps the four |
| `BANISHED_TAVERN_RECIPE` | 1 files / 3 sites | getenv | DELETE |  | BANISHED_TAVERN_RECIPE=<n> -- pick which of the seven ale recipes the building |
| `BANISHED_TEARDOWN_BRACKET` | 1 files / 2 sites | getenv | DELETE |  | instead of file — see spawncomponent.cpp), marks every live entity for removal, drains the pending-removal array, then removes whatever is left in the |
| `BANISHED_TEARDOWN_HEAPCHECK` | 1 files / 1 sites | other/ref | DELETE |  | no-op unless BANISHED_REDZONE=watch + BANISHED_TEARDOWN_HEAPCHECK=1 |
| `BANISHED_TEARDOWN_HEAPWALK` | 1 files / 3 sites | getenv | DELETE |  | [BANISHED_TEARDOWN_HEAPWALK] Wave 9 lane N, Finding 3 — THE TEST THAT |
| `BANISHED_TOMB_TRACE` | 1 files / 2 sites | getenv | DELETE |  | Phase-1 bring-up verification trace (env BANISHED_TOMB_TRACE, default off → |
| `BANISHED_TOOLBAR_ITEMS` | 1 files / 2 sites | getenv | DELETE |  | Determine template index based on action type |
| `BANISHED_TOOL_DIAG` | 9 files / 25 sites | getenv | DELETE |  | Diagnostic: the per-element enum reads below feed the placement tile-validity mask override in ToolTile_FootprintValidity_d580 (0x14009D580). All-zero |
| `BANISHED_TOOL_TRACE` | 5 files / 7 sites | getenv | DELETE |  | EditTool from the key-0 node). Log the RETURN ADDRESS so the actual caller is identifiable instead of guessed at. |
| `BANISHED_TOOL_ZLIFT` | 1 files / 3 sites | getenv | DELETE |  | forces pos.z=0) and sits ~0.06 below the rendered ground, so the faithful grid can read as "above the (sunk) model" — a model-Z / terrain-baseline iss |
| `BANISHED_TOWNHALL_DIAG` | 2 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_TRACE_TERRAFORM` | 1 files / 2 sites | getenv | DELETE |  | One-shot trace, BANISHED_TRACE_TERRAFORM=1. |
| `BANISHED_TRACKER_TRACE` | 4 files / 8 sites | getenv | DELETE |  | the resource category (88f90) and add `quantity` to entityList's per-category bucket (entityList+0x170, stride 0x20, int field +0x10). Closure satisfi |
| `BANISHED_TRADEUI_ELEMDIAG` | 1 files / 2 sites | getenv | DELETE |  | Outcome diagnostic (BANISHED_TRADEUI_ELEMDIAG=1). Prints all three states |
| `BANISHED_TRADEUI_EVENTDIAG` | 1 files / 4 sites | getenv | DELETE |  | matching the original asm pattern: MOV RAX,[slot]; MOV EDX,[RAX] This avoids stale cached pointers that may point to wrong data. ===================== |
| `BANISHED_TRADE_MERCHANT_SECONDS` | 1 files / 2 sites | getenv | DELETE |  | TEST HARNESS, opt-in: BANISHED_TRADE_MERCHANT_SECONDS overrides the countdown. |
| `BANISHED_TRADE_PRICE_PROBE` | 1 files / 2 sites | getenv | DELETE |  | ── The probe ──────────────────────────────────────────────────────────────── |
| `BANISHED_TRADE_PROBE` | 1 files / 1 sites | other/ref | DELETE |  | Observed live via BANISHED_TRADE_PROBE: CreatePlacedDescription 12x11. |
| `BANISHED_TRADE_SELL` | 1 files / 1 sites | other/ref | DELETE |  | THE VIRTUAL MERCHANT CUSTOMER: with a merchant docked, fill the sell rows and |
| `BANISHED_TRADE_SORT_SELFTEST` | 1 files / 2 sites | getenv | DELETE |  | ── Self-test: BANISHED_TRADE_SORT_SELFTEST=1 ──────────────────────────────── |
| `BANISHED_TRADE_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_TRADINGPOST_PATH` | 1 files / 4 sites | getenv | DELETE |  | TEST HARNESS, opt-in: BANISHED_TRADINGPOST_PATH=<n> considers only |
| `BANISHED_TREE_CATCH` | 2 files / 4 sites | getenv | DELETE |  | HwWatch_Arm � BANISHED_TREE_CATCH writer hunt |
| `BANISHED_TUTHL_DIAG` | 1 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_TUTORIAL_STEP` | 1 files / 3 sites | getenv | DELETE |  | TEST HARNESS (BANISHED_TUTORIAL_STEP=<n>): jump straight to step n on the |
| `BANISHED_TUTORIAL_TRACE` | 12 files / 22 sites | getenv | DELETE |  | keeps its entry, and the failure surfaces much later and somewhere else -- either as a tutorial StorageLocation goal that can never equal its target, |
| `BANISHED_UIDESC_CENSUS` | 1 files / 2 sites | getenv | DELETE |  |  |
| `BANISHED_UIF_NEGCTRL` | 2 files / 3 sites | getenv | DELETE |  | NEGATIVE CONTROL 2 (`BANISHED_UIF_NEGCTRL=2`, declared in |
| `BANISHED_WATER_BAND_MULT` | 1 files / 2 sites | getenv | DELETE |  | carried across chunks -> cumulativeMidlineArc/32 - 16) |
| `BANISHED_WATER_CONTOUR` | 3 files / 5 sites | getenv | DELETE |  | (or +0x158 via BANISHED_WATER_MAT=1). No-op if the mesh/material wasn't built (VB data ptr null / material null). |
| `BANISHED_WATER_CONTOUR_NOPHASE2` | 1 files / 2 sites | getenv | DELETE |  | Phase 2 can be gated separately (it builds shore rings around hf+0x48 features, which on the first-light map are mountain ranges — may look odd; |
| `BANISHED_WATER_DIAG` | 3 files / 7 sites | getenv | DELETE |  | [WATER-DIAG] Phase-2 tarn instrumentation. Entry state first so we can see which early-return gate (if any) fires when nothing emits. |
| `BANISHED_WATER_LEVEL` | 1 files / 2 sites | getenv | DELETE |  | Water level: the real game's water verts use int16 z = -192 = -0.75 * 256 (the b260 -0.75 "below-water" constant; VS scales z by 1/256). Overridable |
| `BANISHED_WATER_MAT` | 1 files / 2 sites | getenv | DELETE |  | (we already returned on the shadow pass), so water casts no shadow — matching the original (GetShader()!=1 guards the water draws). Mode 0xA000 routes |
| `BANISHED_WATER_NO_PERP_GATE` | 1 files / 4 sites | getenv | DELETE |  | is NOT stale — it is [RBP+0x308] = Normalize(interpolated centerline tangent), built at the top of the resample loop. The gate is Abs(Dot(Normalize(ma |
| `BANISHED_WATER_OLD_STAGEA` | 1 files / 2 sites | getenv | DELETE |  | b260 disassembly. * fb00 STAGE A is now a FAITHFUL geometric port (fb00_BuildBody_Faithful), replacing the old per-node offset approximation (kept as |
| `BANISHED_WATER_UV` | 1 files / 2 sites | getenv | DELETE |  | squarely in the measured retail range. int16 wraps every 64 tiles (65536/1024) so tiling stays seamless; the lower step also pushes the int16-overflow |
| `BANISHED_WEATHER_FORCE` | 1 files / 2 sites | getenv | DELETE |  | condition matches when weather is on and the sim is idle (state 0). State (w+0x10) is re-read each iteration since SetEnabled mutates it. --- |
| `BANISHED_WEATHER_PARTICLES` | 1 files / 2 sites | getenv | DELETE |  | it holds the 12 lighting keyframes AND the rain/snow Weather descriptor array (envRes+0x28) that the particle engine builds effects from. Retail's Gam |
| `BANISHED_WEATHER_TRACE` | 1 files / 3 sites | getenv | DELETE |  |  |
| `BANISHED_WINDOW_GUARD` | 1 files / 2 sites | getenv | DELETE |  | window this small can be a render target, and no user ever chose one. 64 matches the sentinel repair in standalone/main.cpp (HarnessGuardUnattendedBoo |
| `BANISHED_WORKAREA_STALL_TRACE` | 1 files / 2 sites | getenv | DELETE |  | DIAGNOSTIC (BANISHED_WORKAREA_STALL_TRACE=1, default off — unset behaviour is |
| `BANISHED_WORKSCHED_PROBE` | 1 files / 2 sites | getenv | DELETE |  | because step 5 destroys the object anyway unfiled                          — the healthy path A run with entryNotFound == 0 and both fatal counters 0 |
| `BANISHED_WORKVALIDATE_TRACE` | 2 files / 3 sites | getenv | DELETE |  | Diagnostic context for the BANISHED_WORKVALIDATE_TRACE probe: name the BST |
| `BANISHED_WORK_SER_PROBE` | 3 files / 3 sites | getenv | DELETE |  | the dead target. This was the in-game "crash when saving", reported 2026-08-29. Pickup::OnComplete returning false is retail-faithful (0x1400f2ec0 doe |
| `BANISHED_WORLDGEN_SEED` | 1 files / 2 sites | getenv | DELETE |  | Optional reproducible seed (BANISHED_WORLDGEN_SEED=N) — pins the worldgen RNG |
| `BANISHED_WP_CITIZEN_CENSUS` | 1 files / 1 sites | getenv | DELETE |  |  |
| `BANISHED_WSWATCH` | 1 files / 4 sites | getenv | DELETE |  | HwWatch_Arm — BANISHED_WSWATCH writer hunt (Finding 3) |
| `BANISHED_ZONE_TRACE` | 1 files / 1 sites | getenv | DELETE |  |  |

## Per-file breakdown (for splitting a follow-up delete/graduate batch)

Count = number of distinct `BANISHED_*` flags read in that file (not raw getenv call count).

### code/Game/Component -- 120 files, 416 flag-file touches

| File | Distinct flags |
|---|---|
| `code/Game/Component/component_factory_base.cpp` | 101 |
| `code/Game/Component/citizencomponent.cpp` | 23 |
| `code/Game/Component/buildcomponent_state8.cpp` | 8 |
| `code/Game/Component/workplacecomponent.cpp` | 8 |
| `code/Game/Component/statemachinecomponent.cpp` | 7 |
| `code/Game/Component/citizenaicomponent.cpp` | 7 |
| `code/Game/Component/growthcomponent.cpp` | 7 |
| `code/Game/Component/destroycomponent_states.cpp` | 6 |
| `code/Game/Component/immigrationcomponent_states.cpp` | 6 |
| `code/Game/Component/wildanimalcomponent.cpp` | 5 |
| `code/Game/Component/buildcomponent.cpp` | 5 |
| `code/Game/Component/charactercomponent.cpp` | 5 |
| `code/Game/Component/spawncomponent.cpp` | 5 |
| `code/Game/Component/populationcomponent.cpp` | 5 |
| `code/Game/Component/pasturecomponent_states.cpp` | 5 |
| `code/Game/Component/statusiconcomponent.cpp` | 5 |
| `code/Game/Component/keystone_a_map.cpp` | 5 |
| `code/Game/Component/citizenai_candidatelist.cpp` | 5 |
| `code/Game/Component/buildcomponent_state1.cpp` | 4 |
| `code/Game/Component/clearareacomponent.cpp` | 4 |
| `code/Game/Component/clearareacomponent_states.cpp` | 4 |
| `code/Game/Component/cropfieldcomponent.cpp` | 4 |
| `code/Game/Component/mapcomponent.cpp` | 4 |
| `code/Game/Component/pickingcomponent.cpp` | 4 |
| `code/Game/Component/rawmaterialcomponent.cpp` | 4 |
| `code/Game/Component/tradecomponent_states.cpp` | 4 |
| `code/Game/Component/ambientenvironmentcomponent.cpp` | 3 |
| `code/Game/Component/animalcomponent.cpp` | 3 |
| `code/Game/Component/citizen_onremove.cpp` | 3 |
| `code/Game/Component/clearresourcecomponent_states.cpp` | 3 |
| `code/Game/Component/decalcomponent.cpp` | 3 |
| `code/Game/Component/floorcomponent.cpp` | 3 |
| `code/Game/Component/fruitcomponent.cpp` | 3 |
| `code/Game/Component/orchardcomponent.cpp` | 3 |
| `code/Game/Component/herdcomponent.cpp` | 3 |
| `code/Game/Component/particlecomponent.cpp` | 3 |
| `code/Game/Component/citizenai_executor.cpp` | 3 |
| `code/Game/Component/statusicon_geometry.cpp` | 3 |
| `code/Game/Component/storagedisplaycomponent.cpp` | 3 |
| `code/Game/Component/boatcomponent_route.cpp` | 3 |
| `code/Game/Component/tradecomponent_dock.cpp` | 3 |
| `code/Game/Component/zonecomponent.cpp` | 3 |
| `code/Game/Component/diseasecomponent.cpp` | 3 |
| `code/Game/Component/tombstonecomponent.cpp` | 3 |
| `code/Game/Component/citizenai_handlers3.cpp` | 3 |
| `code/Game/Component/modelcomponent.cpp` | 3 |
| `code/Game/Component/pasturecomponent_spawn.cpp` | 3 |
| `code/Game/Component/citizenai_housing_idle.cpp` | 3 |
| `code/Game/Component/ambientemittercomponent.cpp` | 2 |
| `code/Game/Component/attendancecomponent.cpp` | 2 |
| `code/Game/Component/boatcomponent_states.cpp` | 2 |
| `code/Game/Component/destroycomponent.cpp` | 2 |
| `code/Game/Component/citizen_updatehealth.cpp` | 2 |
| `code/Game/Component/nomadcomponent.cpp` | 2 |
| `code/Game/Component/fire_trace.h` | 2 |
| `code/Game/Component/nomadcomponent_states.cpp` | 2 |
| `code/Game/Component/interactcomponent.cpp` | 2 |
| `code/Game/Component/livestockcomponent.cpp` | 2 |
| `code/Game/Component/storagelocationcomponent.cpp` | 2 |
| `code/Game/Component/storagecomponent.cpp` | 2 |
| `code/Game/Component/terraformcomponent.cpp` | 2 |
| `code/Game/Component/tornadocomponent.cpp` | 2 |
| `code/Game/Component/tradecomponent.cpp` | 2 |
| `code/Game/Component/workcomponent.cpp` | 2 |
| `code/Game/Component/happinesscomponent.cpp` | 2 |
| `code/Game/Component/character_trig.cpp` | 2 |
| `code/Game/Component/resourceproduce_creatework.cpp` | 2 |
| `code/Game/Component/citizen_assign_citizens.cpp` | 2 |
| `code/Game/Component/citizen_needsupdate.cpp` | 2 |
| `code/Game/Component/citizenai_gather.cpp` | 2 |
| `code/Game/Component/citizenai_growup.cpp` | 2 |
| `code/Game/Component/citizencomponent_serialize.cpp` | 2 |
| `code/Game/Component/citizen_employ_cascade.cpp` | 2 |
| `code/Game/Component/interactdescription.cpp` | 2 |
| `code/Game/Component/resourceproducecomponent_states.cpp` | 2 |
| `code/Game/Component/controller_state_table.h` | 2 |
| `code/Game/Component/ambientenvironmentcomponent_serialize.cpp` | 1 |
| `code/Game/Component/boatcomponent.cpp` | 1 |
| `code/Game/Component/clearresourcecomponent.cpp` | 1 |
| `code/Game/Component/consumeproducecomponent.cpp` | 1 |
| `code/Game/Component/destroycomponent_state6.cpp` | 1 |
| `code/Game/Component/firecomponent.cpp` | 1 |
| `code/Game/Component/fishingcomponent.cpp` | 1 |
| `code/Game/Component/forestercomponent.cpp` | 1 |
| `code/Game/Component/herbalistcomponent.cpp` | 1 |
| `code/Game/Component/huntcomponent.cpp` | 1 |
| `code/Game/Component/immigrationcomponent.cpp` | 1 |
| `code/Game/Component/marketcomponent.cpp` | 1 |
| `code/Game/Component/multiobjectcomponent.cpp` | 1 |
| `code/Game/Component/pasturecomponent.cpp` | 1 |
| `code/Game/Component/pickingcomponent_funcs.h` | 1 |
| `code/Game/Component/quarrycomponent.cpp` | 1 |
| `code/Game/Component/residencecomponent.cpp` | 1 |
| `code/Game/Component/wellcomponent.cpp` | 1 |
| `code/Game/Component/cemeterycomponent.cpp` | 1 |
| `code/Game/Component/citizencomponent_lifecycle.cpp` | 1 |
| `code/Game/Component/diseasecomponent_funcs.h` | 1 |
| `code/Game/Component/healercomponent.cpp` | 1 |
| `code/Game/Component/marketlocationcomponent.cpp` | 1 |
| `code/Game/Component/workplacedescription.cpp` | 1 |
| `code/Game/Component/component_factory_base.h` | 1 |
| `code/Game/Component/controller_live_base.h` | 1 |
| `code/Game/Component/uidescription.cpp` | 1 |
| `code/Game/Component/orchardcomponent_states.cpp` | 1 |
| `code/Game/Component/attendancecomponent_states.cpp` | 1 |
| `code/Game/Component/buildcomponent_sub8630.cpp` | 1 |
| `code/Game/Component/citizenai_work_scheduler.cpp` | 1 |
| `code/Game/Component/citizen_reassign_workers.cpp` | 1 |
| `code/Game/Component/citizenai_handlers.cpp` | 1 |
| `code/Game/Component/citizenai_handlers2.cpp` | 1 |
| `code/Game/Component/citizenai_leisure.cpp` | 1 |
| `code/Game/Component/cropfieldcomponent_states.cpp` | 1 |
| `code/Game/Component/highlightcomponent.cpp` | 1 |
| `code/Game/Component/fishingcomponent_produce.cpp` | 1 |
| `code/Game/Component/residence_heattick.cpp` | 1 |
| `code/Game/Component/citizenai_actionctors.cpp` | 1 |
| `code/Game/Component/quarrycomponent_deform.cpp` | 1 |
| `code/Game/Component/citizenai_findhelpers.cpp` | 1 |
| `code/Game/Component/createplaceddescription.cpp` | 1 |
| `code/Game/Component/toolbardescription.cpp` | 1 |

### code/Game/Game -- 43 files, 254 flag-file touches

| File | Distinct flags |
|---|---|
| `code/Game/Game/starttown.cpp` | 85 |
| `code/Game/Game/gameupdateloop.cpp` | 64 |
| `code/Game/Game/gameupdateloop_load.cpp` | 11 |
| `code/Game/Game/gameupdateloop_serialize.cpp` | 8 |
| `code/Game/Game/weather_sim.cpp` | 6 |
| `code/Game/Game/worldgen.cpp` | 6 |
| `code/Game/Game/water_contour_mesh.cpp` | 6 |
| `code/Game/Game/builder_pool_driver.cpp` | 5 |
| `code/Game/Game/game_main.cpp` | 5 |
| `code/Game/Game/construction_force_harness.cpp` | 5 |
| `code/Game/Game/forester_force_harness.cpp` | 3 |
| `code/Game/Game/pathfinder_seed.cpp` | 3 |
| `code/Game/Game/town_storage_seed.cpp` | 3 |
| `code/Game/Game/camera_input.cpp` | 3 |
| `code/Game/Game/eventlog_achievement.cpp` | 3 |
| `code/Game/Game/geometry_draw.cpp` | 3 |
| `code/Game/Game/filesystem_setup.cpp` | 3 |
| `code/Game/Game/serialize_heightmap_mesh.cpp` | 3 |
| `code/Game/Game/mapquery.cpp` | 2 |
| `code/Game/Game/production_harness.cpp` | 2 |
| `code/Game/Game/menuscene.cpp` | 2 |
| `code/Game/Game/town_workplace_registry.cpp` | 2 |
| `code/Game/Game/toolbar.cpp` | 1 |
| `code/Game/Game/charmanager_seed.cpp` | 1 |
| `code/Game/Game/force_work.h` | 1 |
| `code/Game/Game/unreg_lane5_musicplayer.cpp` | 1 |
| `code/Game/Game/region_seed.cpp` | 1 |
| `code/Game/Game/founding_spawn.cpp` | 1 |
| `code/Game/Game/minimap_paint.cpp` | 1 |
| `code/Game/Game/minimap_upload.cpp` | 1 |
| `code/Game/Game/menudialogloopt.cpp` | 1 |
| `code/Game/Game/geometry_serialize.cpp` | 1 |
| `code/Game/Game/serialize_resize_helpers.cpp` | 1 |
| `code/Game/Game/force_work.cpp` | 1 |
| `code/Game/Game/eventlog.cpp` | 1 |
| `code/Game/Game/mapgenerator.cpp` | 1 |
| `code/Game/Game/professionlist.cpp` | 1 |
| `code/Game/Game/w2_takescreenshot.cpp` | 1 |
| `code/Game/Game/goals_serialize.cpp` | 1 |
| `code/Game/Game/weather_sun.cpp` | 1 |
| `code/Game/Game/worldgen_f390_erosion.cpp` | 1 |
| `code/Game/Game/gameupdateloop_teardown.cpp` | 1 |
| `code/Game/Game/worldgen_f390_ranges.cpp` | 1 |

### code/Game/Dialog+Player+Entity+Work+Action+Tutorial+other -- 127 files, 247 flag-file touches

| File | Distinct flags |
|---|---|
| `code/Game/Dialog/ui_dialog_stub_factories.cpp` | 12 |
| `code/Game/Entity/entitylist.cpp` | 9 |
| `code/Game/Dialog/saveloaddialog.cpp` | 7 |
| `code/Game/Dialog/newgamedialog.cpp` | 6 |
| `code/Game/Entity/componententity.cpp` | 6 |
| `code/Game/Entity/citizenbehavior.cpp` | 5 |
| `code/Game/Player/tool_base_tile.cpp` | 5 |
| `code/Game/Action/action_base_cnr.cpp` | 5 |
| `code/Game/Player/tool_base_render.cpp` | 4 |
| `code/Game/Dialog/mod_dialog_vtables.cpp` | 4 |
| `code/Game/Dialog/tradeui.cpp` | 4 |
| `code/Game/Dialog/workplaceui.cpp` | 4 |
| `code/Game/Player/edittool.cpp` | 4 |
| `code/Game/Dialog/optionsdialog.cpp` | 4 |
| `code/Game/Work/harvestarea.cpp` | 4 |
| `code/Game/Action/action_base_complete.cpp` | 4 |
| `code/Game/Dialog/entityui.cpp` | 4 |
| `code/Game/Dialog/productionui.cpp` | 3 |
| `code/Game/Dialog/mainmenudialog.cpp` | 3 |
| `code/Game/Action/action_pf_ctors.cpp` | 3 |
| `code/Game/Dialog/toolui.cpp` | 3 |
| `code/Game/Dialog/toolbardialog.cpp` | 3 |
| `code/Game/Action/action_pf_roots_route.cpp` | 3 |
| `code/Game/Work/foresterwork.cpp` | 3 |
| `code/Game/Work/work_base_reimpl.cpp` | 3 |
| `code/Game/Object/factory_registrations_bulk.cpp` | 2 |
| `code/Game/Work/consumeproduce.cpp` | 2 |
| `code/Game/Player/tool_base_highlight_tess.cpp` | 2 |
| `code/Game/Dialog/controlui.cpp` | 2 |
| `code/Game/Dialog/cropui_promote.cpp` | 2 |
| `code/Game/Dialog/orchardui.cpp` | 2 |
| `code/Game/Player/tool_activation_standalone.cpp` | 2 |
| `code/Game/Work/fishingwork.cpp` | 2 |
| `code/Game/Work/harvestorchard.cpp` | 2 |
| `code/Game/Work/plantorchard.cpp` | 2 |
| `code/Game/Dialog/tradeui_price.cpp` | 2 |
| `code/Game/Work/marketwork.cpp` | 2 |
| `code/Game/Work/plantarea.cpp` | 2 |
| `code/Game/Player/bridgetool_calcpath.cpp` | 2 |
| `code/Game/Work/gatherrawmaterials.cpp` | 2 |
| `code/Game/Action/dropoffrawmaterial.cpp` | 2 |
| `code/Game/Action/pickupstoredrawmaterial.cpp` | 2 |
| `code/Game/Action/huntanimal.cpp` | 2 |
| `code/Game/Dialog/pastureui.cpp` | 2 |
| `code/Game/Dialog/townhallui.cpp` | 2 |
| `code/Game/Dialog/dialog_controller_vtable.cpp` | 2 |
| `code/Game/Action/action_base_rawmat.cpp` | 2 |
| `code/Game/Action/createrawmaterial.cpp` | 2 |
| `code/Game/Tutorial/tutorialgoalcount.cpp` | 2 |
| `code/Game/Work/pickup.cpp` | 2 |
| `code/Game/Player/roadtool_calcpath.cpp` | 2 |
| `code/Game/Player/tool_base_tilerender.cpp` | 2 |
| `code/Game/Tutorial/tutorial_dialog_vtables.cpp` | 2 |
| `code/Game/Tutorial/tutorialhighlightdialog.cpp` | 2 |
| `code/Game/Tutorial/tutorial_wave.cpp` | 2 |
| `code/Game/Action/performwork.cpp` | 1 |
| `code/Game/Dialog/pagetoggleui.cpp` | 1 |
| `code/Game/Dialog/toolbar_vtable_standalone.cpp` | 1 |
| `code/Game/Dialog/tradeui_checkout.cpp` | 1 |
| `code/Game/Dialog/edittool_select.cpp` | 1 |
| `code/Game/Dialog/mapui.cpp` | 1 |
| `code/Game/Dialog/moddialog.cpp` | 1 |
| `code/Game/Dialog/autosavedialog.cpp` | 1 |
| `code/Game/Dialog/dialog_exe_helpers.cpp` | 1 |
| `code/Game/Action/completetask.cpp` | 1 |
| `code/Game/Work/buildstructure.cpp` | 1 |
| `code/Game/Work/build_actions.cpp` | 1 |
| `code/Game/Action/showtool.cpp` | 1 |
| `code/Game/Work/cleararea.cpp` | 1 |
| `code/Game/Dialog/controluiconfig.cpp` | 1 |
| `code/Game/Action/createnaturalresource.cpp` | 1 |
| `code/Game/Action/removeanimal.cpp` | 1 |
| `code/Game/Work/huntwork.cpp` | 1 |
| `code/Game/Dialog/debugui.cpp` | 1 |
| `code/Game/Action/action_pf_roots_drop.cpp` | 1 |
| `code/Game/Action/action_base_hunt.cpp` | 1 |
| `code/Game/Dialog/destroyui.cpp` | 1 |
| `code/Game/Dialog/upgradeui.cpp` | 1 |
| `code/Game/Dialog/upgradeuiconfig.cpp` | 1 |
| `code/Game/Work/clearstorage.cpp` | 1 |
| `code/Game/Work/work_act_path.cpp` | 1 |
| `code/Game/Action/region_paircache.cpp` | 1 |
| `code/Game/Dialog/citizenui.cpp` | 1 |
| `code/Game/Dialog/radiusui.cpp` | 1 |
| `code/Game/Dialog/radiusui_promote.cpp` | 1 |
| `code/Game/Dialog/residenceui.cpp` | 1 |
| `code/Game/Dialog/statusiconui.cpp` | 1 |
| `code/Game/Dialog/storagecapacityui.cpp` | 1 |
| `code/Game/Dialog/storageui.cpp` | 1 |
| `code/Game/Object/factory_registration.cpp` | 1 |
| `code/Game/Object/factory_registration.h` | 1 |
| `code/Game/Dialog/eventlogui.cpp` | 1 |
| `code/Game/Dialog/eventnotifydialog.cpp` | 1 |
| `code/Game/Action/action_pf_astar.cpp` | 1 |
| `code/Game/Action/removenaturalresource.cpp` | 1 |
| `code/Game/Work/herbalistwork.cpp` | 1 |
| `code/Game/Goal/buildinggoal.cpp` | 1 |
| `code/Game/Dialog/hotkeytooltipui.cpp` | 1 |
| `code/Game/Dialog/resourcelimituiconfig.cpp` | 1 |
| `code/Game/Object/internal_factory_audit.cpp` | 1 |
| `code/Game/Object/internal_factory_audit.h` | 1 |
| `code/Game/Work/pasturework.cpp` | 1 |
| `code/Game/Player/pathtool_setselected.cpp` | 1 |
| `code/Game/Action/action_base_move.cpp` | 1 |
| `code/Game/Action/moveto.cpp` | 1 |
| `code/Game/Action/action_pf_pools.cpp` | 1 |
| `code/Game/Dialog/professionui.cpp` | 1 |
| `code/Game/Action/action_pf_region.cpp` | 1 |
| `code/Game/Player/removaltool.cpp` | 1 |
| `code/Game/Player/tool_base_path.cpp` | 1 |
| `code/Game/Action/action_pf_road.cpp` | 1 |
| `code/Game/Dialog/scenariodialog.cpp` | 1 |
| `code/Game/Dialog/statusiconuiconfig.cpp` | 1 |
| `code/Game/Dialog/buildrequirementui.cpp` | 1 |
| `code/Game/Player/placementtool.cpp` | 1 |
| `code/Game/Player/tool_base_createbar.cpp` | 1 |
| `code/Game/Player/tool_base_ray.cpp` | 1 |
| `code/Game/Player/tool_base_screenrect.cpp` | 1 |
| `code/Game/Player/dragplacementtool.cpp` | 1 |
| `code/Game/Player/fencedtool_calcpath.cpp` | 1 |
| `code/Game/Dialog/tradeui_sort.cpp` | 1 |
| `code/Game/Goal/goal_exe_helpers.cpp` | 1 |
| `code/Game/Tutorial/find_ui_element_by_path.cpp` | 1 |
| `code/Game/Tutorial/tutorialgoalbutton.cpp` | 1 |
| `code/Game/Tutorial/tutorialimagetoolbaricon.cpp` | 1 |
| `code/Game/Tutorial/tutorialsetuprestrictplayer.cpp` | 1 |
| `code/Game/Tutorial/tutorial_base.cpp` | 1 |

### runtime+dx11 -- 34 files, 45 flag-file touches

| File | Distinct flags |
|---|---|
| `runtime/src/graphics/mesh.cpp` | 3 |
| `runtime/src/graphics/batchc_gfx_leaves.cpp` | 3 |
| `runtime/src/object/external_reftable.cpp` | 2 |
| `runtime/src/object/internal.cpp` | 2 |
| `runtime/src/ui/userinterface.cpp` | 2 |
| `runtime/funcs/rt_resolve_cache.cpp` | 2 |
| `dx11/src/devicedata.cpp` | 2 |
| `runtime/src/ui/userinterface_dragstart.cpp` | 2 |
| `runtime/funcs/rt_port.h` | 2 |
| `runtime/src/graphics/edgemesh_vtable.cpp` | 1 |
| `runtime/src/achievement/achievement_manager.cpp` | 1 |
| `runtime/types/io/FileSystem.h` | 1 |
| `runtime/src/save/saveload_update.cpp` | 1 |
| `runtime/src/io/modification_setupfilesystem.cpp` | 1 |
| `runtime/src/ui/sprite.cpp` | 1 |
| `runtime/src/engine/mainloop_run.cpp` | 1 |
| `runtime/src/ui/userinterface_showdialog.cpp` | 1 |
| `runtime/src/collection/array.cpp` | 1 |
| `runtime/src/collection/array_destroy.cpp` | 1 |
| `runtime/src/graphics/scene.cpp` | 1 |
| `runtime/src/io/persistence_worker_win.cpp` | 1 |
| `runtime/src/picking/scene.cpp` | 1 |
| `runtime/src/io/serializer.cpp` | 1 |
| `runtime/src/io/serializer.h` | 1 |
| `runtime/funcs/rt_libm.h` | 1 |
| `runtime/src/ui/dialog_ctor_dtor.cpp` | 1 |
| `runtime/src/system/filename.cpp` | 1 |
| `runtime/src/engine/mainloop_video.cpp` | 1 |
| `runtime/src/engine/rt_engine_port.cpp` | 1 |
| `dx11/src/contextdata.cpp` | 1 |
| `runtime/src/engine/updateloop_loadresources.cpp` | 1 |
| `runtime/src/io/package_file_read_win.cpp` | 1 |
| `runtime/src/save/saveload.cpp` | 1 |
| `runtime/src/ui/dialog.cpp` | 1 |
