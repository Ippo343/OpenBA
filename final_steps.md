# Final Steps — The Push to a Playable Standalone `Application-x64.exe`

> Living checklist. Check items off session-by-session. Recompute the gap and re-read the
> blocker graph each session — registrations and bring-ups shift what's unblocked.
> Last ground-truthed: 2026-06-19 on branch `wave/component-gap` (cumulative tip; `main` is an ancestor).

---

## 0. Definition of Done (what "the exe is finished" means)

The end goal (CLAUDE.md Goal #2): a drop-in replacement `Application-x64.exe` that links
`Runtime-x64.dll` + `VideoDX11-x64.dll` and **actually plays Banished**:

- [ ] **Builds & boots** standalone (✅ already true) — menu → New Game → in-game.
- [ ] **World renders** — terrain (✅) **+ buildings, citizens, resources, trees visibly drawn** (✗ today).
- [ ] **World simulates** — citizens path/work/eat, resources grow/deplete, population changes (✗ today — inert).
- [ ] **HUD works** — toolbar, status bar, event notifications, resource counts (✗ today — gated off).
- [ ] **Playable** — place buildings, assign work, control via mouse/keyboard tools (✗ today).
- [ ] **Stable** — clean shutdown without crash (✗ today); no seed-dependent heap crash.
- [ ] **Registration gap = 0** — all `Module::Exe` refs from `code/Game` resolve (95 left, all complex defers).
- [ ] (Stretch) merged to `main`; the ~20 stacked `wave/*` branches consolidated.

After the exe ships, Goal #3 (reimplement `Runtime-x64.dll`) begins — **out of scope for this document.**

---

## 1. Where We Are (state snapshot, verified 2026-06-19)

**✅ DONE**
- Standalone builds, links the two engine DLLs, boots.
- **Menu** works — the heap race is fixed and **merged to main** (e95a8d5).
- **Autostart → New Game → world builds** (~1477 entities: terrain + ~15–20 citizens + ~1200 resources + 257 env + 2 structures) → `[GUL::Load] EXIT — world built` → **`GameUpdateLoop::Update` ticks live at 60 Hz, no crash.**
- **Terrain renders lit** (no shadow pass).
- **0 NullFactory landmines** — factory cascade complete (185a4a5); 311 types instantiate.
- **Foundations A–F** built: deserialize, ECS CopyCtor/RegisterEntity, all 82 `*Description` factories, all 68 component inert factories, and a **renderable `ModelComponent` precedent** (real vtable +0x20 Startup / +0x30 Draw).
- **Registration coverage** driven to "complex-defers-only" across every module:
  - Action / Entity / Goal / Dialog / Tutorial gaps = **0**.
  - Component 40 / Work 23 / Game 23 / Player 9 = **95 remaining, ALL complex defers.**

**✗ THE REMAINING PUSH — three structural reasons the world ticks but is dead:**
1. **All 68 components have INERT vtables.** `MakeInertTyped` wires only +0x00 (dtor) and +0x10 (GetTypeId); every other slot — including the per-tick **Update at +0x48** — is a no-op `Inert_Stub`. **The world is a static snapshot that ticks but never simulates.**
2. **Spawned entities are not visually rendered.** Buildings/citizens/resources exist + are registered, but render/init dispatch is **GUARDED OFF** in `starttown.cpp` because the components are inert (Foundation D/E zeroed their bodies → `Geometry`@+0x28 / config@+0x38 are null).
3. **No HUD.** Toolbar/EventNotify/StatusBar/AutoSave dialogs are gated off (`kCreateToolbar = false`) pending the toolbar UI vtable + `g_TypeId_ToolbarItem` + a populated `m_resourceList`.

Plus: a clean-shutdown crash, a guarded seed-dependent heap crash, and the 95-address latent registration gap.

---

## 2. The Key Insight — registration is done; everything now waits on ~6 keystones

The registration waves have hit diminishing returns: **the 95 remaining gap addresses are NOT simple registrations** — each is blocked on a subsystem that isn't wired in standalone. Crucially, **those same subsystems are exactly what gameplay needs.** So the final push pivots from "Track A — grind reg waves" to "**Track C — bring up the keystones**," and the registration gap collapses as a side effect.

### The six keystones (and what each unblocks)

| # | Keystone | Unblocks (registration) | Unblocks (gameplay) | Size |
|---|----------|-------------------------|---------------------|------|
| **A** | **Pathfinder worker-thread queue** reimpl (`System::Thread::Start` spin-loop + region flood-fill + A* path-builder `FUN_14001ce00`) | Action pathfinder family (~12), Component map/zone (4), Game `Game_Dtor`, Player roadtool/RemovalTool CalcPath | Citizens can **path**, roads build | **L** |
| **B** | **Action child-ctor cascade** (ShowTool ctor `0x140042890`, `0x1400c29a0`, the action copy-ctors `c59d0/c63b0/ca520/c8400…`) | ~15 Work action-ctors, the Action ctor defers | StateMachine **actions instantiate** → citizen behavior | **M** |
| **C** | **Audio/Scene subsystem bring-up** (`m_audioScene`, `m_ambientSoundData`, timed-event queue) | Component visuals/audio (~6), Game GUARD-M5 (4: AmbientSound, CalcWindFactor, UpdateTimedEvents, Weather draw) | Ambient sound, audio cues | **M** |
| **D** | **Component live-wiring** (the 4-step recipe: real +0x48 vtable + Startup/Serialize body-init) | (consumes the reg helpers) | **The world SIMULATES** | **XL** (per-component) |
| **E** | **Rendering generalization** (Foundation-F renderable components across spawn pools) | — | **Buildings/citizens/resources VISIBLE** | **L** |
| **F** | **HUD + input/tools** (toolbar vtable + `g_TypeId_ToolbarItem 0x1401c6128` + `g_ToolbarDialog_Vtable 0x14015fa00`; then the Player tool harness → real tools) | Player 9 (debugger-validated backlog), Game dialog ctors | **Playable / interactive** | **L** |

**Dependency order:** D depends on B (actions) and A (pathing) for the *citizen* chain, but is independent for *self-contained* components (Growth/Fruit/Disease/etc.). E is largely independent. F depends on the menu-child-vtable conflict being resolved. C is polish. So the citizen chain is the long pole: **A + B + D(citizen).**

---

## 3. Recommended Sequence (phases → sessions)

> Each phase is independently checklistable. Phases 1, 4 can run in parallel with 2/3.
> The critical path to "simulating + visible + playable" is **1 → 2 → 3 → 4 → 5**.

### Phase 1 — Prove the component-bring-up recipe end-to-end (self-contained sim)
*Goal: get a visible, observable simulation on components that need neither pathing nor actions.*
The 4-step recipe (per `project_component_bringup_frontier`): (1) real per-type vtable routing **+0x48 → `<Comp>_Update`** (keep +0x00 dtor, +0x10 GetTypeId, +0x40 safe no-op); (2) real **Startup/Serialize** to populate the body from its Description; (3) register the exe helpers `<Comp>_Update` calls; (4) bucket/flag eligibility (entity bit 3 + comp bit 3 + processCount>0).

- [ ] **Finish GrowthComponent** (worked example — step 1 routing already done, env `BANISHED_LIVE_GROWTH`).
  - [ ] Debugger-confirm `GrowthComponent_Update` is entered each tick via +0x48 (proves routing).
  - [ ] Implement step 2 (Startup/body-init: config@+0x38, resource@+0x28 from Description).
  - [ ] Register the helpers it calls (step 3); confirm growth/temperature/harvest actually advances.
  - [ ] Promote a worked component-factory **macro/template** (`COMPONENT_LIVE_FACTORY`) generalizing the GrowthComponent pattern so the next components are mechanical.
- [ ] Bring up the other **self-contained sim components** (Update body already exists, no actions/pathing):
  - [ ] **FruitComponent** (`FruitComponent_Update`)
  - [ ] **TombstoneComponent** (`TombstoneComponent_Update`)
  - [ ] **DiseaseComponent** (`DiseaseComponent_Update`) — note: registration deps 48610/48840 still complex.
  - [ ] **FireComponent** (`FireComponent_Update`) — note: GUARD-M5 audio/scene; may need Keystone C.
  - [ ] **SpawnComponent** (`SpawnComponent_Update`)
  - [ ] **PopulationComponent** (`PopulationComponent_Update`)
  - [ ] **AmbientEnvironmentComponent** (`AmbientEnvironmentComponent_Update`)
- [ ] **Exit criteria:** at least one component class observably changes state per tick in-game (debugger-verified), recipe documented & templated, default run byte-unchanged with env off.

### Phase 2 — Keystone A (pathfinder worker-thread queue) + Keystone B (action child-ctors)
*Goal: the two big reimpls that the citizen chain (and most of the remaining reg gap) depend on.*

- [ ] **Keystone A — Pathfinder worker-thread queue** (NEW; no files exist yet).
  - [ ] Decompile + reimplement the path-builder `FUN_14001ce00` and region flood-fill `FUN_14001a9b0` (`ZoneRefresh` `0x14009c170/9c480`).
  - [ ] Reimplement the thread queue: `0x1400a68b0/a6a30/a6b20/a6c10` enqueue, `FUN_1400a6430`, the `System::Thread::Start` spin (`a6fe0/a8c90/9f2f0/a6e30`). Decide single-thread-synchronous vs. real worker thread (single-threaded standalone hangs on the spin-loop — likely run the queue synchronously).
  - [ ] Runtime mangled names for the queue primitives are in `reference_runtime_worker_mangled_names` memory (CritSec Lock/Unlock, Thread::Start, static MemAlloc behind IAT slots 0x140152540/550/558/b60).
  - [ ] Register the now-closure-safe addrs: Action move family (`9cfb0/9e550/10a1f0/c8c10/116650/22970` + deps), Component map/zone (`17230/9d360/1241b0/124750`), roadtool `0x14009ce00`, RemovalTool CalcPath `0x1401035e0`.
- [ ] **Keystone B — Action child-ctor cascade.**
  - [ ] Reimplement + register the action child-ctors: ShowTool ctor `0x140042890`, `0x1400c29a0`, SendComponentMessage/CompleteTask/PerformWork/MoveTo/*NaturalResource ctors (`c5da0/cb1d0/c9720/c9170/cadf0/c57d0/c6a20/c54e0`), and the copy-ctors (`c59d0/c63b0/ca520/c8400`).
  - [ ] This unblocks ~15 Work action-ctors (`ba9f0/881d0/bb0a0/f2370/bb7b0/bbbc0/bdd20/be6e0/beae0/c2490/bef70/bf790/bffd0/c1eb0/c1530/c8a0`) — register them after the cascade lands.
  - [ ] Also: `RemoveAnimal_Action_Ctor 0x1400bbea0` (deferred SKIP) becomes registerable once child copy-ctors land.
- [ ] **Exit criteria:** Work gap 23 → ~4 (MarketWork AVL only); Action deferred set → 0; pathfinding exercised + verified (Ghidra + debugger, since autostart doesn't hit it).

### Phase 3 — Keystone D (citizen): the heart of gameplay
*Goal: citizens actually live — AI plans, state machine drives actions, characters animate.*
Depends on Phase 2 (A + B). Module priority #1 per CLAUDE.md.

- [ ] **StateMachineComponent** live (`StateMachineComponent_Update`) — drives every citizen's action stack.
- [ ] **CitizenAIComponent** live (`CitizenAIComponent_Update`) — note the deep tier: `AssignWorkplace 1163d0`, `LongTermPlan 0x140115e60` (Level-2), happiness `10dca0`, 15-state `10f1f0`; the name-DB tier (`117080→a4c60→a4b30`).
- [ ] **CitizenComponent** live (`CitizenComponent_Update`) — lifecycle: `UpdateHealth 1127d0`, `AssignState 113af0`, `HandleLifeEvent 116dd0`. (The AssignHousing→FindWorkplace/Storage/Partner cascade is already registered.)
- [ ] **CharacterComponent** live (`CharacterComponent_Update`) — movement/facing/animation.
- [ ] **AttendanceComponent** live (`AttendanceComponent_Update`).
- [ ] **RawMaterialComponent / StorageDisplayComponent** live (economy feedback).
- [ ] Resolve the **name-DB tier** (Keystone C-adjacent): citizen naming via `a4c60/a4b30`.
- [ ] **Exit criteria:** a citizen, in the debugger, picks a job → paths to it → performs work → consumes food; population responds.

### Phase 4 — Keystone E: make the world visible
*Goal: buildings, citizens, resources, trees actually drawn. Largely parallel to 2/3.*

- [ ] **Generalize Foundation F** (renderable `ModelComponent`) — wire +0x48 too, not just +0x20/+0x30, and apply real renderable component bodies across spawn pools.
- [ ] **Un-guard the 4 render blocks in `starttown.cpp`** (structures, citizens, resources, environment) once their components carry real (non-inert) `ModelComponent`/`Geometry`@+0x28.
- [ ] **CharacterComponent rendering/animation** for citizens (shares the Foundation-F gate).
- [ ] Verify visually (RenderDoc MCP available, or eyeball): the town renders with models, not just terrain.
- [ ] **Exit criteria:** New Game shows a populated, model-rendered town, not an empty lit terrain.

### Phase 5 — Keystone F: HUD + input + tools (playable)
*Goal: interactive — see resource counts, place buildings, assign work.*

- [ ] **Resolve the menu-child-vtable conflict** (the 2 Player MUST-DEFERs): make the menu `BuildId` child fully constructible with the real vtable so registering `g_ToolUI_ChildDialog_Vtable 0x14015f5a0` no longer crashes the menu.
- [ ] **Toolbar HUD** (`gameupdateloop.cpp` `kCreateToolbar`): wire `g_TypeId_ToolbarItem 0x1401c6128` + `g_ToolbarDialog_Vtable 0x14015fa00` (no standalone toolbar UI vtable exists yet — build one), confirm `m_resourceList` resolves `Dialog/Toolbar.rsc`, flip `kCreateToolbar = true`.
- [ ] **StatusBar / EventNotify / AutoSave** dialogs (`kCreateGameDialogs`) — Game dialog ctors `5e230/68ab0`.
- [ ] **Input → tools:** drive `OpenToolDialog 0x1400ff9e0`, then the Player tool harness (`project_player_tool_harness`) connects PlacementTool/RemovalTool/RoadTool to real input.
- [ ] **Player gap 9 → 0:** the large geometry/render ports (`fcc90`/`dd710`/`101380`/`1035e0`/`55430` ace40 tessellation) — debugger-validated via the tool harness (these are NOT blind registrations).
- [ ] **Exit criteria:** can place a building and a road, assign a profession, and watch citizens execute it; resource counts update on the HUD.

### Phase 6 — Polish: audio, weather draw, stability
- [ ] **Keystone C — Audio/Scene bring-up:** wire `m_audioScene` + `m_ambientSoundData` + timed-event queue; un-GUARD-M5 `AmbientSound_Update a40e0`, `CalcWindFactor 1027c0`, `UpdateTimedEvents a5d10`, `Weather_DrawUpdate 938d0`; register Component audio visuals (particle/fire/ambientemitter ~6).
- [ ] **Weather/animation manager** (`fn_AnimationManager_Update` slot 0x140152050, `fn_Weather_ResetTime`).
- [ ] **Clean shutdown crash** — root-cause the exit crash (`Game_Dtor 9ed60` worker-thread teardown — needs Keystone A; `EntityList_Dtor c3e60`, `CitizenList_Dtor 907b0`).
- [ ] **Seed-dependent heap crash** — root-cause the currently-guarded ABSURD-value crash.
- [ ] **Save/load** — `QuickSave 95da0`, `SaveLoad_Update`, the deep serialize defers.
- [ ] **Image capture / screenshot** path (`ImageCapture_DrawCheck a5140`) — low priority.

### Phase 7 — Ship the replacement exe
- [ ] **Registration gap = 0** (should fall out of Phases 2–6; recompute and mop up stragglers).
- [ ] **Consolidate branches:** merge the stacked `wave/*` chain forward to `main`; prune the ~20 `worktree-agent-*` and dead `wave/*` branches.
- [ ] Full playthrough smoke test: New Game → play several minutes → save → load → quit cleanly.
- [ ] Update CLAUDE.md (Goal #2 done; Goal #3 — Runtime DLL — becomes next phase).

---

## 4. Per-Session Working Loop (the proven process)

1. `git status` clean; pick the phase/checklist item.
2. **Recompute the gap** (don't trust stale numbers):
   ```bash
   grep -rhoiE "Module::Exe,\s*0x1[0-9a-f]+" code/Game | grep -oiE "0x1[0-9a-f]+" | tr 'A-F' 'a-f' | sort -u > /tmp/ref.txt
   grep -rhoiE "Register(ExeAddr|DispatchSlot)\(\s*0x1[0-9a-f]+" code engine standalone | grep -oiE "0x1[0-9a-f]+" | tr 'A-F' 'a-f' | sort -u > /tmp/reg.txt
   comm -23 /tmp/ref.txt /tmp/reg.txt | awk 'strtonum($0)>=0x140001000 && strtonum($0)<=0x14014ffff' > /tmp/gap.txt
   wc -l /tmp/gap.txt
   for a in $(cat /tmp/gap.txt); do grep -rilE "Module::Exe,\s*${a}" code/Game|head -1; done | sed -E 's#code/Game/([^/]+)/.*#\1#' | sort | uniq -c | sort -rn
   ```
3. **Decompile in Ghidra → reimplement bottom-up** (closure-safe: register a helper only when its *entire* exe sub-callee closure is also registered — a half-null structure is strictly worse than the safe no-op skip).
4. **Lead-verify every intricate port offset-by-offset against the disasm** — agent ports (even opus) compile clean but drift on pointer/tree/lane logic. Use OPUS subagents for complex RE; SONNET only for mechanical/identical patterns ([[feedback_prefer_opus_for_complex_re]]).
5. **Build:** `mingw32-make standalone-debug` (GREEN). `make clean` when switching release↔debug (stale-object gotcha).
6. **A/B test:** `make sa-run` (BANISHED_AUTOSTART=1) → confirm world builds (~1477 entities), `0 NOT IMPLEMENTED`, 0-byte crash log. World-build trace is in `standalone_ERROR.log` (stderr), NOT `standalone_debug.log`.
7. **Debugger-validate** anything the autostart path doesn't exercise (tools, pathfinding, in-game subsystems) — use `vscode-debugger` MCP; for tools use the injection harness (`BANISHED_TOOL_HARNESS`).
8. Commit per family; update memory + this checklist.

### Hard-won gotchas (do not relearn)
- **Per-tick Update = vtable +0x48 (slot 9), NOT +0x40** (+0x40 is the empty `*_OnRemove` stub). Driven by the 5-bucket round-robin in `EntityList_Update`, gated on entity flag bit 3 + comp flag bit 3 + processCount>0 + delta>0.
- **A live vtable swap ALONE crashes** — the inert factory zeroes the body, so Update null-derefs config@+0x38 / resource@+0x28. Always do body-init (step 2) too, or add a null-guard early-return.
- **Gap-metric counts `RegisterExeAddr` source lines, not wired calls** — a deferred batch must be a true no-op (no `RegisterExeAddr` text) or it falsely reads as registered.
- **Closure-safety spans 3 axes:** `ResolveAddr(Exe, code)`, `ResolveAddr(Exe, dispatch-slot 0x14015xxxx)`, and `ResolveExport`. Check all three + intra-file sibling calls.
- **Confirm closure from the DECOMPILE, not a static-pointer grep** (the grep mislabeled `116650`/`022970` as safe).
- **Watch dropped-digit typos** in `ResolveAddr` literals (caller bugs, not registrations) — verify the addr is a real function ENTRY via `get_function_by_address`, and the true callee via `get_xrefs_to`.

---

## 5. Appendix A — The 95 remaining gap addresses, bucketed

> Source: `notes/STANDALONE_REG_WAVES_PLAN.md` (the per-wave sections have full per-addr detail).
> Recompute the live list with the command in §4 each session.

### Component (40) — `wave/component-gap` residual, all complex defers
- **Citizen-AI deep tier (10):** `1163d0` AssignWorkplace + `112040/1168a0` (blocked on `Audio::Scene::RemoveEmitter/StopEffect3D` helper `12a550`); `117080→a4c60→a4b30` name-DB; `1127d0` UpdateHealth, `113af0` AssignState, `116dd0` HandleLifeEvent, `302a0`; `10dca0` happiness, `10f1f0` 15-state. → **Keystone C + D(citizen)**
- **Worker-thread region family (4):** zone `17230/9d360`, map `1241b0/124750`. → **Keystone A**
- **Disease (2):** `48610/48840` Infect/Cure/CheckCure. **Trade name-DB (1):** `a4c60`.
- **Build (1):** `128f50` (`Picking::Scene::AddMesh`, no export). **Rawmat L2 (2):** `f72e0`, `12ec60`.
- **Self-resolving passthroughs (~8):** animal `106420`, cemetery `47200`, particle `52b30`, tornado `57cd0/585a0`, decal `11b560`, statusicon `12bbd0`, ambientenv `43310`, nomad `102b60`, herd `107400`, orchard `2d40`. (Do NOT register — infinite recursion / Factory-vtable crash class.)
- **GUARD-M5 audio/scene visuals (~6):** particle `52990/52a50/53660`, fire `49130/494c0`, ambientemitter `42e10`. → **Keystone C**
- **Deep-unreg (2):** particle `52480`, fire `49610`. **Save/load serialize (1):** `19a0`.

### Work (23) — `wave/work-gap` residual, all complex defers
- **Action-ctor/copy-ctor family (15):** `ba9f0 881d0 bb0a0 f2370 bb7b0 bbbc0 bdd20 be6e0 beae0 c2490 bef70 bf790 bffd0 c1eb0 c1530 c8a0`. → **Keystone B**
- **MarketWork AVL family (4):** `2a9f0/2aaa0/2ac40/2b290` — needs a full AVL-family port (node key@0/data@+8/parent@+0x18/left@+0x20/right@+0x28/balance@+0x32) + arg-count fix.
- **Large Serialize/Slot4 (3):** `eff80` (MarketWork Slot4, 1408 lines), `f2f00` (Pickup_Serialize). **Level-2 (1):** `115e60` CitizenAI_LongTermPlan.

### Game (23) — `wave/game-gap` safe floor, all Level-2 / subsystem bring-up
- **Core-sim Updates:** `c4090` EntityList_Update, `8c2b0` CitizenList_Update, `1026a0` EntityUI_Update. → **Keystone D**
- **GUARD-M5 audio/event:** `1027c0` CalcWindFactor, `a40e0` AmbientSound_Update, `a5d10` UpdateTimedEvents, `938d0` Weather_DrawUpdate. → **Keystone C**
- **Live UI/startup:** `99260` SetupUIRefs, `a90a0` ProfessionList_Startup, `101ee0` EntityUI_Startup. → **Keystone F**
- **Dialog ctors:** `5e230` AutoSaveDialog_CopyCtor, `68ab0` EventNotifyDialog_Ctor. → **Keystone F**
- **Thread/unreg dtors:** `907b0` CitizenList_Dtor, `9ed60` Game_Dtor (thread teardown → **Keystone A**), `c3e60/c3d30` EntityList_Dtor/Shutdown, `1024d0` EntityUI_Dtor, `f8ff0` AVL teardown.
- **Deep serialize:** `95da0` QuickSave, `a3630` MenuScene_Slot5. **Huge render:** `a5140` ImageCapture_DrawCheck. **Event fire:** `95580/95750` EventNotifier_Fire(WithName).

### Player (9) — `wave/player-gap` debugger-validated backlog
- **2 MUST-DEFER (registration alone = crash/hang):** `0x14009ce00` roadtool → pathfinder worker-thread (**Keystone A**); `0x1400ff9e0` OpenToolDialog → shared child-dialog vtable `0x14015f5a0` (menu conflict, **Keystone F**).
- **6 large dormant geometry/render ports** (debugger-verify via tool harness): `fcc90` EdgeMesh chain, `101380→1008c0` Video::Context vertex render, `55430` HighlightAdd → `ace40` ~884-line Transform2/BST tessellation (draft held at `notes/wave5_drafts/ace40.draft.cpp`), `1035e0` RemovalTool CalcPath (**Keystone A** for road branch), `ffc60` UpdateToolUI, `dd710` PathTool AVL builder.

---

## 6. Appendix B — Components ready for bring-up (Update body already reimplemented)

These 19 already have a real `<Comp>_Update` in `code/Game/Component/` — they need steps 1/2/4 of the
recipe (vtable routing + body-init + flags), not a new Update reimpl. GrowthComponent + ModelComponent
already have env-gated wiring started.

`AmbientEnvironmentComponent` · `AnimalComponent` · `AttendanceComponent` · `CharacterComponent` ·
`CitizenAIComponent` · `CitizenComponent` · `DiseaseComponent` · `FireComponent` · `FruitComponent` ·
`GrowthComponent`✱ · `HerdComponent` · `HuntComponent` · `LivestockComponent` · `PopulationComponent` ·
`RawMaterialComponent` · `SpawnComponent` · `StateMachineComponent` · `StorageDisplayComponent` ·
`TombstoneComponent`   (✱ = bring-up wiring started)

The other ~49 inert components either have no per-tick Update (data/visual/economy passive) or need their
Update reimplemented first (a deeper task).

---

## 7. Appendix C — Key files & references

- **Master plan / per-wave detail:** `notes/STANDALONE_REG_WAVES_PLAN.md`
- **Component bring-up recipe:** `project_component_bringup_frontier` (memory) + the "CORRECTION 2026-06-18" section of the wave plan.
- **Inert factory:** `code/Game/Component/component_factory_base.{cpp,h}` (`MakeInertTyped`, `Inert_Stub`).
- **Worked examples:** `code/Game/Component/growthcomponent.cpp` (live Update routing), `code/Game/Component/modelcomponent.cpp` (renderable vtable).
- **Spawn / render guards:** `code/Game/Game/starttown.cpp` (4 GUARDED-OFF render blocks), `gameupdateloop.cpp` (21 `[GUARD-M5]` sites, HUD bring-up at ~L1322).
- **Tool harness:** `standalone/tool_harness.{cpp,h}` (`BANISHED_TOOL_HARNESS`), `project_player_tool_harness` (memory).
- **Worker-thread mangled names:** `reference_runtime_worker_mangled_names` (memory).
- **Address registry:** `standalone/address_registry.cpp` (~581 registered) + per-family `standalone/reg_*.cpp`.
- **Branch state:** all `wave/*` gap closures stacked on `wave/component-gap` (main is ancestor); menu fix is on `main`.
