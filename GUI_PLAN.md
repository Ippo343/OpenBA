# GUI / Keystone F — Playable HUD + Tools — Sequenced Plan

**Branch:** `wave/phase3-keystone-d`
**Written:** 2026-07-02, after the economy loop closed (commit a4d06a5, Phases 1-7 done).
**Purpose:** turn the standalone from "a world that simulates but you can't touch" into
"a world you can PLAY" — click a toolbar button and have it actually *do* something:
place a building, activate a tool, open a management dialog. This is `final_steps.md`
Keystone F (HUD + input + tools = playable) and the last big gate before the
Definition-of-Done "Playable" checkbox.

This is the GUI sibling of `ECONOMY_PLAN.md`. Each phase below is designed to be spun
out to a successive agent; the lead integrates + verifies offset-by-offset vs Ghidra.

> **⚠️ Reconcile the stale one-liner:** the MEMORY.md summary "toolbar buttons CRASH on
> click" is MISLEADING. Verified 2026-07-02: the toolbar renders and **navigation clicks
> already work** — clicking a group button (ShowGroup) opens sub-toolbars, highlights,
> tooltips/hotkeys/icons are all correct, and the in-game pause-menu Resume works
> (Phase-5 memory through commit 5df5c5f). What does NOT work is a click on an *action*
> item actually *doing* something. The toolbar today is a working **navigation shell**
> whose buttons don't yet act.

---

## Current state (measured / code-verified 2026-07-02)

**What already works — do NOT re-investigate:**
- **Toolbar HUD renders** (bottom-right, themed icons, correct order) crash-free, gated on
  `BANISHED_LIVE_TOOLBAR` (+ `BANISHED_GAME_PRELOAD` for the button externals) — both are
  in the default `SA_LIVE_ENV` combo. See [[project_phase5_gui_keystone_f]].
- **Real mouse input reaches the buttons** via the engine's own pipeline
  (`Input::Device` → `UserInterface::Update` → `DialogInstance::Update` hit-test →
  `ToolbarDialog_OnEvent` +0x40). Clicks highlight buttons + open nested sub-toolbars.
- **Tooltips/hotkeys/cost-icons** — BuildRequirementUI name+desc+cost, HotKeyToolTipUI
  `[key]`, icon ordering, Pasture wood cost — all fixed + user-confirmed.
- **In-game pause menu** — Resume/Options/Quit work.

**The click dispatcher** (`code/Game/Dialog/toolbardialog.cpp` ~L1500, `ToolbarDialog_OnEvent`
event 0x12 CLICK, `eventData` = button index) reads the toolbar item's action-type at
`item+0x48` and branches. The actionable branches are the "playable" gap:

| action type (`item+0x48`) | Branch | Standalone state |
|---|---|---|
| 0 = **Tool** | `OpenToolDialog` → PlacementTool/RoadTool/RemovalTool | **NEVER exercised** — Part B (this plan's spine) |
| 1 = ShowGroup | opens a sub-toolbar | ✅ works |
| 2 = **Utility** | `Toolbar_Utility_Impl` (townhall/trade dialog) | ported (L513) but the DialogInstance-ctor + ToolUI-vtable + BST path is untested → likely the "Utility null-call" crash |
| 3 = ToggleAll | — | untested (low value) |
| 4 = FocusEntity / 5 = CenterOnPoint | camera | untested |
| 6-10 = Speed/Pause | game-speed control | untested (cheap wins) |

Plus the in-game MENU sub-buttons (Save/Load/Menu/Goal/Map/Tutorial) are still stubbed
(`StandaloneStub_Slot8_OnCommand`).

**So there are three independent tracks:**
1. **Tool activation** (case 0) → place a building / run a tool = **the playable spine**. Bulk of the work.
2. **Utility dialogs** (case 2) → open management UIs = a self-contained follow-on.
3. **Menu buttons + speed controls** → small, mostly-independent quick wins.

---

## The gating chain (dependency order)

```
Phase 1  PlacementTool bring-up — construct + Startup on a real building template
             │  (descriptionPtr from a resolved Template\<name>.rsc; the 4 *Description
             │   type-id globals filled in standalone; createComponent guard)
             ▼
Phase 2  PlacementTool CREATE path — OnKey key-5 → sub_da00 → helper_d650 places a
             │  building entity at a map cell (harness-driven; chase crash dominoes)
             ▼
Phase 3  Wire it to the REAL click — case-0 OpenToolDialog opens the PlacementTool so a
             │  real toolbar button click (not the harness) starts placement; mouse
             │  drives position; a click confirms the place
             ▼
Phase 4  Utility dialogs (case 2) — Toolbar_Utility_Impl end-to-end (townhall/trade)
             ▼
Phase 5  Menu buttons + speed controls — Save/Load/Menu stubs; game-speed cases 6-10
             ▼
Phase 6  Retire the economy FORCE harness — place a real ForesterLodge through the
             │  player path (feeds ECONOMY_PLAN's producer-employment loop)
```

Phases 1→2→3 are the critical path to "place a building." Phase 4 is independent and can
run in parallel. Phase 5 is a bag of quick wins. Phase 6 is the payoff that ties GUI back
to the economy work.

---

## Cross-cutting facts (read before any phase)

- **Harness mechanism** (`standalone/tool_harness.{cpp,h}`, env `BANISHED_TOOL_HARNESS`):
  `ToolHarness_Tick(entityUI, frameNum)` is called from `GameUpdateLoop::Update`; it fires
  a tool once at settle-frame 60. Today only `"removal"` is wired (`FireRemovalTool`
  constructs a RemovalTool, wires cells, calls Startup/CalcCost/CalcPath). **Increment-1 =
  add `FirePlacementTool` + a `"placement"` env value**, mirroring that shape.
- **EntityUI mode-tree @+0x10 is NULL in standalone** → the real `SetEditMode`
  (`FUN_140102b60`) path is DEAD → the harness must BUILD the tool directly
  (`PlacementTool_Factory_SA`, placementtool.cpp:1109 — returns a MemAlloc'd 0x70 struct
  with the standalone vtable installed).
- **PlacementTool field map** (engine/types/PlacementTool.h, size 0x70, vtable 0x140161d20):
  - `+0x10` **dialogPtr** — pointer to an `InternalBasePtr<DialogInstance>` SLOT (8 bytes).
    `Startup` never touches it; `OnKey`/`Serialize` read it. Harness must supply a
    **writable 8-byte slot init to 0** (OpenToolDialog fills it).
  - `+0x18` RotationSetting IBP (Startup CheckPtr's it → +0x48).
  - `+0x20` **descriptionPtr** — the building **DESCRIPTION ComponentEntity**. `Startup`
    reads its component array (`+0x28` ptr / `+0x30` count) to find the 4 *Description comps.
  - `+0x28` descComponent (SizeDescription), `+0x30` mapComponent (AreaDescription),
    `+0x38` **createComponent** (FenceDescription — used as the MESH-bearing model comp:
    Startup:1037 derefs `createComponent+0x28` UNGUARDED → a template lacking it null-derefs;
    **first expected domino**), `+0x40` radiusComponent, `+0x48` rotationIBP.
  - Slots: `+0x20` Startup (0x1400fe580), `+0x30` OnKey (0x1400fe2a0; key-5=confirm →
    da00 → d650), `+0x38` Serialize (per-frame placement preview), `+0x40` Draw.
- **The 4 *Description type-id globals** (`g_TypeId_SizeDescription 0x1401c7468`,
  `AreaDescription 0x1401c7a18`, `FenceDescription 0x1401c7a88`, `RadiusComponent 0x1401c4870`)
  are filled in `engine_funcs.cpp` via `ResolveAddr(Exe, .rdata)` — which is **NULL in
  standalone** ([[reference_standalone_rdata_null_fallback]]). They are read by dormant
  tools (fencedtool/dragplacementtool/placementtool) that have never run in standalone, so
  they are almost certainly UNFILLED. **Fix = lazy `Object::GetTypeIdByName("<Name>")` fill
  post-Engine::Startup** (the proven citizen-brick / ToolbarStandalone_Init pattern). Verify
  each id via its `*_RegisterType` DATA writer in Ghidra before trusting it.
- **Template resolve:** `ResolveTemplateByPath("Template\\<Name>.rsc")` exists
  (`starttown.cpp:760`, static; used for ForesterLodge at :779). Increment-1 either expose
  it or mirror it in the harness. Easy start-town spawns WoodHouse/StorageBarn/StorageYard —
  use one of those simple buildings first, then graduate to ForesterLodge (Phase 6).
- **The CREATE closure is ALREADY REGISTERED** (verified): `sub_da00` (0x1400fda00 position
  calc), `helper_d650` (0x1400fd650 validate + CREATE entity), finalize 0x1401000f0 + fail
  0x140100140. Registered ≠ exercised — this path has NEVER run in standalone; expect crash
  dominoes to chase (nm-symbolize, ImageBase 0x140000000).
- **Test env combo** (from project dir, NOT cd'd):
  `BANISHED_TOOL_HARNESS=placement mingw32-make sa-run` plus the default SA_LIVE_ENV.
  Logs: `C:/GOG Games/Banished/standalone_error.log` (stderr, world-build + `[ToolHarness]`),
  `standalone_debug.log` (stdout). Wait ~40s, `taskkill //F //IM reimpl-x64.exe`.
- **Verification method:** symbolize crash RVAs via `nm reimpl-x64.exe | grep ' [tT] '` +
  awk nearest-symbol-below `0x140000000+RVA`. A/B every change: default build (harness env
  off) must stay byte-identical + crash-free.
- **Process:** prefer opus subagents for RE/port; LEAD verifies every offset vs the Ghidra
  disasm; disjoint-file contract when fanning out. See [[feedback_prefer_opus_for_complex_re]],
  [[feedback_verify_final_file_not_agent_report]], [[feedback_agent_merge_safety]].

---

## Phase 1 — ✅ DONE (2026-07-02) — PlacementTool constructs + Startup runs on a real template

**✅✅ PHASE 1 COMPLETE + A/B-VERIFIED.** The harness (`BANISHED_TOOL_HARNESS=placement`)
resolves `Template\WoodHouse.rsc`, constructs a PlacementTool, points `descriptionPtr`
(+0x20) at it, and the faithful `Startup` runs to completion crash-free.

- **🔑 The 4 *Description type-id globals were ALL mislabeled in engine_funcs** (Ghidra
  RegisterType DATA writers, verified): `0x1401c7468`=**CreatePlacedDescription** (not
  Size), `0x1401c7a18`=**MapDescription** (not Area), `0x1401c7a88`=**ModelDescription**
  (not Fence — this is the mesh-bearing comp Startup:1037 derefs unguarded),
  `0x1401c4870`=**RadiusDescription** (not RadiusComponent). Filled in standalone via a
  STANDALONE-gated lazy `ResolveTypeIdByNameLazy` in `EnsureStartupFuncs`
  (placementtool.cpp) with the CORRECT names — the globals resolve non-zero.
- **The WoodHouse template IS the right descriptionPtr source** — its top ComponentEntity
  holds 22 components directly, including CreatePlacedDescription (comp[4]), MapDescription
  (comp[1]), ModelDescription (comp[6]). Radius absent (a house has none; not deref'd).
- **Startup ran clean**: MeshInstance allocated + variant selected + mesh set
  (`meshInstance=…12a8 variant=0`), 0-byte crash log, world built.
- **Files:** `standalone/tool_harness.cpp` (`FirePlacementTool` + `"placement"` env branch),
  `code/Game/Player/placementtool.cpp` (type-id lazy-fill + `PlacementTool_HarnessStartupProbe`
  scaffolding that logs the comp array + confirms Model present before calling Startup),
  `code/Game/Game/starttown.cpp` (`StartTown_ResolveTemplateByPath` extern "C" wrapper),
  `engine/types/PlacementTool.h` (corrected the 4 field-comment type names).
- **A/B GREEN:** default `sa-run` (no harness env) = 0 `[Placement]` traces, world built,
  0-byte crash log — the change is fully dormant off-env.

### Phase 1 — original spec (superseded by the DONE block above)

**Objective:** the harness constructs a PlacementTool, points `descriptionPtr` at a resolved
simple-building template, and `Startup` runs to completion — finding the 4 *Description
components and building the preview MeshInstance — crash-free.

**Entry points:**
- `standalone/tool_harness.cpp` — add `FirePlacementTool()` + a `"placement"` env value.
- `PlacementTool_Factory_SA` (placementtool.cpp:1109), `PlacementTool_Startup_Impl`
  (placementtool.cpp:949, orig 0x1400fe580).
- `ResolveTemplateByPath` (starttown.cpp:760) — expose or mirror; pick WoodHouse.rsc.
- The 4 type-id globals — add a lazy `GetTypeIdByName` fill (new small init, or fold into
  an existing post-Startup fill site).

**Expected dominoes:** (1) the type-id globals are null → `findComp` returns null for all 4
→ `createComponent` null → Startup:1037 `*(createComponent+0x28)` null-deref. Fill the
globals first; then guard/verify the mesh path if a simple building genuinely has no
FenceDescription. (2) MeshInstance ctor/vtable IAT slots (0x140151490 / 0x14015e240) may be
unregistered.

**Verify:** `[ToolHarness]` logs Startup entered, the 4 findComp results (which are non-null),
MeshInstance allocated; 0-byte crash log; A/B default byte-clean.

**Agent spin-out:** single agent (harness + type-id fill), lead verifies Startup offsets.

---

## Phase 2 — ✅ DONE (2026-07-02) — PlacementTool CREATE path places a building entity

**✅✅ PHASE 2 COMPLETE + A/B-VERIFIED.** With `BANISHED_TOOL_HARNESS=placement`, after Phase-1
Startup the harness drives the faithful CREATE path and a WoodHouse entity **materializes in
the world crash-free** and survives the next-frame `EntityList` tick (30s+ soak, 0-byte crash
log). Default `sa-run` (harness off) is byte-clean: 0 `[Placement] PLACE` traces, harness
`disabled`, 35s+ crash-free.

- **Harness bypasses `sub_da00`'s cursor pick.** `sub_da00` (→ `ScreenToCell` → `GetMouseRay`
  → terrain-march) needs a live cursor over the map, which the harness has none of at the
  settle frame. Instead the harness (`PlacementTool_HarnessPlaceAtCell`, placementtool.cpp)
  mirrors `sub_da00`'s exact footprint-rectangle + world-centre math (orient 0 → never derefs
  `rotationIBP`, yaw 0) for a chosen centre cell, wires a **zeroed dialog-controller** at
  `+0x10` (so `helper_d650`'s `*(self+0x10)+0x80` existing-placement flag reads 0 and the
  constraint block is skipped), then calls the registered `helper_d650`
  (`ToolTile_ValidateCreate_fd650`) directly.
- **🔑 The keystone crash was `003f60`'s NULL c7a50 (ModelComponent) type-id.** `helper_d650`
  calls the shared component-finder `FUN_140003f60` **twice** (transform set + model-variant
  fixup); it resolves ModelComponent from raw `.rdata 0x1401c7a50`, which is **NULL in
  standalone** → returns a null component → `SetTransformFromYaw(null)` writes `null+0x148`
  (crash). FIX = a **scoped** `GetTypeIdByName("ModelComponent")` fallback at both call sites
  in `tool_base_tile.cpp` (the proven M-D `pf_road` pattern; the shared c7a50 global stays
  unfilled so the produce/animation callers of `003f60` are unregressed). The DLL-mode guards
  (`if (modelComp != 0)` / `if (plVar4 != nullptr)`) are behaviour-identical (never null in
  the real game). No further dominoes — the ComponentEntity standalone vtable (Brick M-B,
  `0x14015fa78`) already covers the copied entity's dispatch.
- **Occupancy scan.** Map centre (124,124) is occupied by the start-town buildings/trees, so
  the per-cell validity (`d580`→`f5d0`: empty + terrain-mask + slope) rejected it. Since
  `helper_d650` is side-effect-free on validity failure, the harness scans outward in
  expanding Chebyshev rings and places at the first buildable cell (landed at (127,121),
  ring 3, 32 attempts).
- **Files:** `code/Game/Player/placementtool.cpp` (`PlacementTool_HarnessPlaceAtCell` +
  `HarnessTryPlaceAtCell` ring-scan driver), `standalone/tool_harness.cpp` (`FirePlacementTool`
  calls the CREATE driver after the Startup probe), `code/Game/Player/tool_base_tile.cpp`
  (`ToolTile_FindModelComponentScoped` + the two guarded `003f60` sites).

### Phase 2 — original spec (superseded by the DONE block above)

**Objective:** drive `OnKey` key-5 (confirm) at a valid map cell → `sub_da00` (position) →
`helper_d650` (validate + CREATE) → a building entity materializes in the world, crash-free.

**Blocking dependency:** Phase 1 (Startup populates the components OnKey/create read).

**Entry points:** `PlacementTool_OnKey_Impl` (placementtool.cpp, orig 0x1400fe2a0, key-5
path), `sub_da00` (0x1400fda00), `helper_d650` (0x1400fd650), finalize 0x1401000f0. The
create reads `dialogPtr` (+0x10, harness-supplied zeroed slot), rotation IBP (+0x48), and
the desc components. Feed a valid cell (256-wide grid, `**(int**)(gameWorld+0xf8)==256`;
map centre valley ~cell(124,124), same as the RemovalTool harness).

**Expected dominoes:** entity-materialization faults (the same class the economy
`BANISHED_PRODUCE_SPAWN_ENTITY` work hit — partially-built standalone entities fault in the
next frame's `EntityList_PreUpdate → Serialize`). May need the ComponentEntity standalone
vtable (0x14015fa78, [[project_economy_brick_mb_done]]) + grid mark/un-mark. Reuse the
economy materialization precedent.

**Verify:** the placed building appears in the entity list / renders; `[ToolHarness]` traces
the create returning a non-null entity; 0 crash; A/B clean.

**Agent spin-out:** 1-2 agents (OnKey/create port verify + materialization), lead integrates.

---

## Phase 3 — Wire to the REAL click (case-0 OpenToolDialog)

**Objective:** a real toolbar button click (case 0 = Tool) opens the PlacementTool via
`OpenToolDialog`, mouse position drives the placement preview per-frame (Serialize +0x38),
and a confirming click places the building — no harness.

**Blocking dependency:** Phases 1+2 (the tool works when driven directly).

**Entry points:** `ToolbarDialog_OnEvent` case 0 (toolbardialog.cpp ~L1500),
`ToolUI_OpenToolDialog_ff9e0` (tool_base_toolui.cpp:214, already registered + self-sufficient
vtable per Part A), `PlacementTool_Serialize_Impl` (per-frame preview using mouse ray),
`PlacementTool_Draw_Impl`. The toolbar item for a building must carry the right
descriptionPtr source (the item's BuildRequirementComponent / description at `item+0x58`).

**Expected work:** connect the button's building descriptor to the tool; ensure the mouse
pick-ray feeds the preview (camera + Picking). This is where the input pick-ray from Part B's
open questions gets resolved.

**Verify:** click the toolbar's House button in-game → placement preview follows the mouse →
click on the ground → a house is placed. User-visible, screenshot-verified.

---

## Phase 4 — Utility dialogs (case 2) [independent, parallelizable]

**Objective:** clicking a Utility toolbar item (townhall / trade post) opens its management
dialog crash-free.

**Entry points:** `Toolbar_Utility_Impl` (toolbardialog.cpp:513, orig 0x140101a80) — ported
but untested: it gets the UserInterface singleton (`*(gameWorld+0x98)`), reads the
UIDescription child (+0x28 external res), constructs a DialogInstance
(`fn_DialogInstance_Ctor`), sets the ToolUI child vtable, and inserts into the position BST
at `gameSubsys+0x50`. Likely dominoes: null `fn_DialogInstance_Ctor` / ToolUI vtable /
UIDescription type-id in standalone. The ToolUI child vtable already exists
(`g_ToolUI_ChildDialog_Vtable_Standalone`, Part A).

**Verify:** click Town Hall / Trade → the dialog opens + renders; 0 crash.

---

## Phase 5 — Menu buttons + speed controls [quick wins]

- **In-game menu Save/Load/Menu/Goal/Map/Tutorial** (StandaloneStub_Slot8_OnCommand,
  mainmenudialog.cpp) — each needs its sub-dialog's standalone vtable (null today).
  Do the cheap/safe ones (e.g. Menu→main menu) first.
- **Game-speed controls** (OnEvent cases 6-10: SpeedDown/Pause/Unpause/SpeedUp/SpeedDisplay)
  — likely just call the game-speed setter; verify the target isn't null in standalone.

---

## Phase 6 — Retire the economy FORCE harness (place a ForesterLodge the player way)

**Objective:** place a real ForesterLodge through the Phase-1-3 player path, so the economy
loop's producer employment runs WITHOUT `BANISHED_FORCE_WORKPLACE`
([[project_economy_sequenced_plan]] Phase 4, [[project_economy_loop]]). Ties GUI back to the
economy work: a player-placed lodge activates its WorkPlaceComponent → registers into the
town profession BST → FindAvailableBuilding → citizens get employed organically.

**Verify:** place a ForesterLodge in-game, assign workers via the ProfessionUI, watch a
citizen path to it and work — no force env.

---

## Definition of Done (this keystone)

- [ ] Click the toolbar's House button in-game → a house is placed on the ground (Phases 1-3).
- [ ] A Utility button opens its management dialog (Phase 4).
- [ ] Pause/speed buttons work (Phase 5).
- [ ] A player-placed ForesterLodge employs a citizen with no force env (Phase 6).
- [ ] Default build (all harness/GUI envs off) stays byte-identical + crash-free throughout.

See [[project_phase5_gui_keystone_f]] (prior GUI wins), [[project_player_tool_harness]]
(harness + 6 dormant Player ports), [[project_final_steps_roadmap]], [[project_economy_loop]].
