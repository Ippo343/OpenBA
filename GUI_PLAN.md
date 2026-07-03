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

## Phase 3 — ✅ DONE (2026-07-02) — real click activates the tool + drives it per-frame

**✅✅ PHASE 3 COMPLETE + A/B-VERIFIED.** With `BANISHED_LIVE_TOOLS=1` a build-button
click activates a PlacementTool through the **real `SetEditMode` path**, and the tool is
driven every frame (OnKey → CreateBar dialog + confirm; Serialize → mouse-pick ghost
preview) **crash-free**. Automated proof via `BANISHED_TOOL_HARNESS=click` (calls the same
`SetEditMode` the toolbar case-1 click invokes): the PlacementTool materializes at
`entityUI+0x28` with `descComp`/`meshInstance`/`rotationIBP` all populated, the pick-ray
resolves cells, and the per-frame tick soaks 60s+ with a 0-byte crash log. Default `sa-run`
(LIVE_TOOLS off, harness off) is byte-clean: 0 tool markers, world builds + ticks, 0-byte
crash log.

- **🔑 The plan's action-type table was BACKWARDS (Ghidra-verified).** `ToolbarDialog::OnEvent`
  (FUN_1400b92a0) **case 0** = open a sub-toolbar (recursive ToolbarDialog); **case 1** =
  `FUN_140102b60` = **SetEditMode** = the real tool activation (descriptor = `item+0x58`).
  `OpenToolDialog` (ff9e0) is called by the tool's OWN OnKey, not the dispatcher.
- **The tool-mode tree** (`entityUI+0x10`) is built by `EntityUI::Startup` (101ee0, 8 inserts
  triggerTypeId→toolTypeId). That's NULL in standalone (101ee0 unregistered) → SetEditMode
  dead-ends. Fix = `EntityUI_BuildToolModeTree_SA` (tool_activation_standalone.cpp): faithful
  AVL insert (`helper_ddb0`/`sub_df10` + registered `4b40` rebalance) of the
  `CreatePlacedDescription→PlacementTool` node (covers ALL placeable buildings) + fills the
  `Tool`/`ToolSetting` base type-ids. Called at gameupdateloop.cpp:1706.
- **Per-frame tick** = `EntityUI_ToolTick_SA` (the tool half of EntityUI::Update + helper_f6e0
  that camera_input.cpp omitted): active tool `+0x30` OnKey(activeFlag) + right-click cancel +
  `+0x38` Serialize(&time,activeFlag) + `+0x60` cursor. Wired in gameupdateloop.cpp Step 10.
- **4 real bugs fixed to reach crash-free:** (1) PlacementTool vtable `+0x58` GetToolSettingTypeId
  was a `Tool_Ret0` stub → SetEditMode deref'd null; ported it to return RotationSetting typeid
  (+`+0x60`→5). (2) `s_pTypeId_RotationSetting` (raw .rdata 0x1401c63c8) NULL in standalone →
  Startup skipped the setting → rotationIBP null → sub_da00 `rot+0x10` crash; filled via
  GetTypeIdByName. (3) `s_fn_IBP_Ctor` IAT slot NULL in standalone → an IBP wrapping the
  non-null RotationSetting came out 0; fell back to the resolved `fn_InternalBasePtr_Ctor`.
  (4) **`Vector2::Normalize` ABI bug** (tool_base_ray.cpp): `?AV12@` returns Vector2 BY VALUE
  (hidden retbuf RCX=retbuf/RDX=this) but was called in-place `void(void*)` → wrote through a
  code address; fixed to `(retbuf,self)`. (5) `PlacementTool_Factory_SA` (Tool_Alloc) zeroed the
  preview-index caches, but the Ctor sets `+0x50/+0x58/+0x60 = -1` → Serialize removed a bogus
  scene/highlight index 0 → crash; replicate the -1 inits.
- **Files:** `code/Game/Player/tool_activation_standalone.cpp` (NEW: tree build + tick + draw),
  `code/Game/Player/placementtool.cpp` (vtable +0x58/+0x60, RotationSetting typeid fill,
  IBP_Ctor fallback, ctor-field init), `code/Game/Player/tool_base_ray.cpp` (Vector2::Normalize
  ABI), `code/Game/Game/gameupdateloop.cpp` (LiveToolsEnabled gate + tree-build + tick calls),
  `standalone/tool_harness.cpp` (`click` harness value).
- **⏳ Remaining (manual/UX):** screenshot-verify a real mouse click on the House button places
  a house (the machinery is proven crash-free; needs a human with the cursor over the map, as
  the pick + left-click confirm read the live OS cursor). Footprint-tile overlay (tool +0x40
  `EntityUI_ToolDraw_SA`) is written but not yet wired into GameUpdateLoop::Draw (the ghost
  MESH preview via Serialize's SceneAddObject already renders). Fold `BANISHED_LIVE_TOOLS` into
  `SA_LIVE_ENV` once user-confirmed.

### Phase 3 — original spec (superseded by the DONE block above)

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

## Phase 3B — Placement preview + CreateBar rendering [core DONE]

**Status (2026-07-02, updated):** the ghost preview mesh renders (item 1 ✅) and the CreateBar
cost/size panel populates + positions (item 2 ✅) — both root-caused, ported, and crash-free
verified. Item 3 (footprint tiles) is wired but gated on a deferred shared sim-gap. The
remaining confirmations are VISUAL (a human with the cursor over the map) — the machinery is
proven crash-free.

### ✅ Item 1 — Ghost mesh renders (DONE, verified, committed a5bf926)
**Root cause found:** the Phase-3B groundwork `TransformFromYaw` fix was DEAD. In
`placementtool.cpp` the `#ifdef STANDALONE_EXE` export fallback for `s_fn_TransformFromYaw`
ran BEFORE the unconditional `s_fn_TransformFromYaw = resolveIAT(0x140151288)` a few lines
later, which — the slot being unregistered in standalone → null — **clobbered the fallback
back to null**. So the fn ptr was null, `localTransform` stayed all-zero (cos=0 → degenerate
rotation matrix), the mesh collapsed to a point = invisible. "Resolved but insufficient" was a
misdiagnosis: it never actually took effect. `SceneAddObject`/`GeomSetTransform`/`MeshUpdateHier`
were all fine (registered in `address_registry.cpp` 1008-1013).
**Fix:** register dispatch slots `0x140151288` (Transform::FromYawPitchOffset) and `0x140151460`
(Geometry::GetTransform) in `address_registry.cpp` alongside the sibling Graphics Transform
slots; reorder `placementtool.cpp` EnsureFuncs so the export fallback runs AFTER resolveIAT.
**Verified** (`BANISHED_TOOL_HARNESS=preview BANISHED_LIVE_TOOLS=1`, new cursor-free ghost-preview
probe drives the exact Serialize pipeline at a fixed cell): `TransformFromYaw` non-null,
`transform=[negSin=0, cos=1.0, pos=(-3,-3.5,0)]` (matches the known Math::Transform layout —
non-degenerate, carries the world pos), `sceneObjectIdx` valid (mesh submitted to the live
gameWorld+0x60 scene — same scene the working ModelComponent path uses), 0-byte crash log. A/B
default clean. **Ghost mesh now stands in the live scene** at the probe cell — a human can
launch `BANISHED_TOOL_HARNESS=preview BANISHED_LIVE_TOOLS=1 make sa-run` to SEE the WoodHouse
ghost at (124,124) (visual confirm still recommended).

### ✅ Item 2 — CreateBar populate + position (DONE, crash-free-verified)
**CreateBar OPEN confirmed** (probe: OnKey → `OpenToolDialog(*(tool+0x10))` loads
`Dialog/CreateBar.rsc`, builds the DialogInstance, AddDialog's it; controller+0x00 dialog IBP
goes 0→live). It was empty+top-left ONLY because `operator_fc60` (0x1400ffc60, the populate +
`SetUpperLeft` positioner) was a null unregistered exe fn. **Ported** `operator_fc60` + its
sub-helpers (e600 "textSize", e420 "textStatus", e700 size-region, e360 "imageIcon" tint, ebb0
box-corner project; ec70/fe960 reused via ResolveAddr) into `code/Game/Player/tool_base_createbar.cpp`
+ `standalone/reg_player_createbar.cpp`, registered at `RegisterCodeFunctions()`. **Verified**
(preview harness drives operator_fc60 against the live-open dialog): controller resolves non-null
(`ctrlTid`=real CRC, `ctrl`≠0 — populate does NOT no-op), every ABI-sensitive call runs crash-free
(GetController[R8=type-id, Ghidra-verified], FindElement×3 [hidden-retbuf], UIString ctor [3-arg
R8], Vector3::Min/Transform::operator* [by-value], SetUpperLeft), 0-byte crash log. Final
SetUpperLeft is gated on `fe960`'s on-screen projection (returns 0 for the harness's synthetic
off-screen box) → real cursor positioning is the user's visual confirm.
🔑 type-ids (Ghidra RegisterType-writers): `DAT_1401c4d40`=**BuildRequirementUI** (controller),
`DWORD_1401c7f58`=**ToolbarDescription** (description comp) — filled via GetTypeIdByName in SA.
🔑 `tool+0x10` = the EntityUI/dialog **controller** (SetEditMode sets `tool+0x10 = entityUI`,
`+0x20 = descriptor`, `+0x18 = ToolSetting IBP` — Ghidra 0x140102b60); PlacementTool.h's "+0x10
dialogPtr" comment is misleading. Controller layout: +0x00 dialog IBP, +0x08/0x0c cached screen
x/y (seeded −1.0), +0x18 its DialogInstance, +0x30 descriptionPtr, +0x38/0x40/0x50/0x58 size-region
fields, +0x68..0x74 root bounds. GeomGetTransform (0x140151460) registered (item-1 commit).

### ⏳ Item 3 — Footprint-tile overlay (wired; tiles gated on a deferred sim-gap)
The Draw call is ALREADY in place: `GameUpdateLoop::Draw` Step 11 (gameupdateloop.cpp ~L1398)
dispatches the active tool's `vtable+0x40` Draw `(tool, videoCtx, drawFlag)` every frame,
faithful to the exe Draw (0x140096010). `PlacementTool_Draw_Impl` runs but **early-returns
because `s_fn_sub_1380` (0x140101380) is null** — sub_1380's closure needs the DEFERRED shared
sim-gap `CharacterComponent_impl_08c0` (0x1401008c0) + `_f5d0` (0x14009f5d0), avoided by all
prior waves. So the footprint TILES await that closure; the wiring itself is done + crash-safe
(guarded). `EntityUI_ToolDraw_SA` in tool_activation_standalone.cpp is a redundant alt path.

- Once the ghost + confirm-place + CreateBar are visually verified, fold `BANISHED_LIVE_TOOLS`
  into `SA_LIVE_ENV`.

---

## Phase 3C — Placement UX polish [in progress]

**Status (2026-07-02):** ✅ **The ghost building now FOLLOWS THE CURSOR** (user-confirmed,
crash-free). Getting there meant fixing the entire cursor→terrain pick chain, which had NEVER
run in standalone — a 6-bug cascade (all in the placement raycast, gated behind `sub_da00`
returning invalid so nothing downstream ever ran):

1. **`Vector2::Normalize` returned (0,0)** (tool_base_ray.cpp `PlayerRay_TerrainMarch_aa530`).
   `Math::Vector2` is 8 bytes → MSVC x64 returns it **in RAX, not via a hidden retbuf**; the
   `(retbuf,self)` call normalized (0,0). → the DDA had a zero direction (step 0, seed cell
   only). FIX = compute the 2D unit direction inline.
2. **`aa20` degenerate triangles** (action_pf_road.cpp `Pf_CellQuadRay_aa20`). The port passed
   `&local_58` etc. to `aa810` as `Vector3*` from **non-contiguous float locals** AND
   `(void)`-cast the corner **heights** — so every tri handed to Möller-Trumbore had garbage/no
   z. FIX = contiguous `float V00/V01/V10/V11[3]` incl. heights (the recurring Vector3-locals bug).
3. **Height scale null** — `f1720bc()` (`DAT_1401720bc`) is a raw .rdata const → 0 in standalone
   → flat terrain. FIX = 1/256 (0x3b800000) fallback.
4. **Wrong `fUV` constant** — the corner spacing read `0x1401720b8` (≈0.001) but the disasm
   (0x1400aaaf7) uses **`0x14017214c` = 1.0**; ≈0.001 collapsed the quad → `det≈0` → every ray
   missed. FIX = read 0x14017214c with a 1.0 fallback.
5. **`GetMouseRay` crash guard** — reject implausible display dims before the camera-ray math.
6. **`Geometry::GetTransform` by-value crash** (placementtool.cpp Serialize). It returns
   `Transform` by value (member, hidden retbuf RCX=this/RDX=retbuf); the 1-arg call left RDX
   garbage → it wrote the Transform into Runtime code → hard-fault the moment `valid=1`. FIX =
   call with a retbuf. This is the "crash on click" the earlier passes hit.

Diagnostics (all `BANISHED_TOOL_DIAG=1`, A/B-safe): `[SerDiag]` `[MouseRay]` `[ScreenToCell]`
`[March]` trace the pick chain; `[Placement] mesh built` / `[fc60]` trace the mesh + populate.

**3C remaining (user-observed, deferred):**
- **CreateBar still stuck top-left + solid (not repositioning).** `operator_fc60` runs and
  populates, but `SetUpperLeft` isn't moving it — the `fe960` screen-projection likely returns 0
  (empty rect) or the centre math is off. Investigate `Pf`/`fe960` projection of the footprint
  corners + the `GetRoot` bounds + the smoothing (`param_1+0x08/0x0c` seeded −1). It renders
  "solid colored" because it's showing the raw CreateBar.rsc template, not the live-positioned panel.
- **Ghost is opaque, not translucent.** `PlacementTool::Startup` overwrites the preview
  MeshInstance vtable with `0x14015e240` — **skipped in standalone** (raw .rdata vtable → null),
  so the mesh keeps the base opaque material instead of the ghost/translucent one that vtable
  selects. Build a standalone `0x14015e240` MeshInstance vtable (or set the preview material/alpha
  another way).
- **Red/green validity tiles missing** = Item 3 (the `sub_1380`→`08c0`/`f5d0` deferred sim-gap).

Files touched this pass: `code/Game/Player/tool_base_ray.cpp` (Normalize inline + GetMouseRay
guard + pick diagnostics), `code/Game/Action/action_pf_road.cpp` (`aa20` contiguous verts +
heights + hscale/uv constants), `code/Game/Player/placementtool.cpp` (GetTransform retbuf ABI +
mesh/Serialize diagnostics).

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
