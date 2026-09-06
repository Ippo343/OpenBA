# exe closeout — lane A (farming)

Branch: `worktree-agent-ad73684faf0e898c0` (worktree
`.claude/worktrees/agent-ad73684faf0e898c0`), based on `4eeed89`.

Scope handed to this lane:

1. Port the crop-field state-machine handler bodies, which were reported as registered
   `nullptr` at `code/Game/Component/cropfieldcomponent_states.cpp:41`.
2. Port the drag-area / placement tools for crop field, hunter lodge and gatherer hut,
   reported as unported at `code/Game/Game/starttown.cpp:3041`.

**Both premises were FALSE.** Each was a stale comment that outlived the port it
described — the same trap as
`reference_stub_comment_outlived_the_port_that_replaced_it`. This document records the
measurement, because "already done" is only a valid answer with evidence behind it.

---

## Item 1 — crop-field state handlers: ALREADY PORTED (closed)

### The claim

`cropfieldcomponent_states.cpp` carried a banner reading:

> ⚠ FIRST PASS: the handler BODIES are not ported yet, so every handler is registered
> as nullptr.

### The measurement

Every one of the eleven handlers the recovered table names has a real body in that same
file, and each body carries its retail address in its own banner comment:

| retail address | role | body |
|---|---|---|
| `0x14011a920` | state 2 enter | `CropField_State2Enter` |
| `0x14011a860` | state 2 trans38 (per-frame) | `CropField_State2PerFrame` |
| `0x14011a7b0` | state 2 event 3 (plant command) | `CropField_Event3Plant` |
| `0x14011a310` | state 9 enter | `CropField_State9Enter` |
| `0x14011a1e0` | state 9 trans38 | `CropField_State9PerFrame` |
| `0x14011a100` | state 10 enter | `CropField_State10Enter` |
| `0x140119fd0` | state 10 trans38 (harvest trigger) | `CropField_State10PerFrame` |
| `0x140119f70` | state 10 update | `CropField_State10Update` |
| `0x140119f00` | state 10 event 4 (harvest command) | `CropField_Event4Harvest` |
| `0x140119e60` | state 11 enter | `CropField_State11Enter` |
| `0x140119d80` | state 11 trans38 | `CropField_State11PerFrame` |

States 3 and 21 are transition TARGETS with no registered body in retail either, so
their absence is faithful, not a gap.

### Why it read as unported

`CropFieldCtrl_PopulateTable` still contains four literal `nullptr` arguments. They are
the **trans30** slot, which retail genuinely leaves empty for this controller — the
register helper at `0x1400330d0` hard-writes 0 into `+0x10` for every entry. Reading
those `nullptr`s as unported handlers requires ignoring `TableRegisterState`'s signature,
which is `(id, enter, TRANS30, TRANS38, UPDATE)`
(`code/Game/Component/controller_state_table.h:103`). Argument order verified against the
struct at `controller_state_table.h:75-81`; per-frame and update are NOT swapped.

Git history corroborates: `81dc93c` ("reaches state 10"), `cbe5ab0` ("Crop field
HARVESTS"), `ce26616` ("61 harvested with harvestFlag=1").

### Action taken

The stale banner is replaced with a STATUS block naming all eleven addresses and
explaining what the remaining `nullptr`s are, so the next reader does not re-port work
that is already done.

---

## Item 2 — crop field / hunter lodge / gatherer hut tools: ALREADY PORTED (closed)

### The claim

`starttown.cpp` justified its demo spawners with:

> all three are acquired only through tools that are still unported (drag-area for the
> crop field, placement for the two lodges)

### The measurement

The tool-mode registry — the BST at `PlayerController+0x10` that
`Toolbar_ShowGroup_Impl` (retail `0x140102b60`) walks to pick a tool for a clicked
template — is populated by `EntityUI_BuildToolModeTree_SA`
(`code/Game/Player/tool_activation_standalone.cpp:184-302`), called from
`code/Game/Game/gameupdateloop.cpp:2770` under `BANISHED_LIVE_TOOLS`, which is
**default-on** (`kDefaultOnFlags`, `code/Game/Component/component_factory_base.cpp:264`).

All eight of retail's nodes are inserted. The three relevant ones:

| template | declares | description typeId | tool |
|---|---|---|---|
| `Template\CropField.rsc` | `createdrag` | `CreateDragDescription` | `DragPlacementTool` |
| `Template\HunterLodge.rsc` | placed | `CreatePlacedDescription` | `PlacementTool` |
| `Template\GathererShelter.rsc` | placed | `CreatePlacedDescription` | `PlacementTool` |

(The gatherer hut's `.rsc` is `GathererShelter.rsc`, not `GathererHut.rsc`.)

Both tool classes are fully implemented, including the slots that actually commit a
placement: `code/Game/Player/dragplacementtool.cpp` (802 lines, Startup / CalcCost /
CalcPath / Draw all wired into the vtable) and `code/Game/Player/placementtool.cpp`
(1684 lines). Neither file contains a stub or TODO marker.

Confirmed live — from the run below, the registry logs its own nodes:

```
[Tools]   drag node: CreateDrag=3893769075 -> DragPlacementTool=3938475504
[Tools] BuildToolModeTree: ... node{CreatePlaced=4107504200 -> PlacementTool=4034384576} ... count=8
```

### The one thing that was genuinely missing

Not a tool — a way to point the existing tool at these templates in an unattended run.
`FirePlacementTool` (`standalone/tool_harness.cpp`, `BANISHED_TOOL_HARNESS=placement`) is
the only harness path that drives a placement all the way to a materialized building,
and it **hard-coded `Template\WoodHouse.rsc`** with no override, at a single fixed cell.
So the two lodges could be ACTIVATED through the real path (`FireActivatePlacement`
already honoured `BANISHED_PLACEMENT_TEMPLATE`) but never COMMITTED through it.

Fixed in this lane:

* `FirePlacementTool` now honours `BANISHED_PLACEMENT_TEMPLATE`.
* Its fixed `Cell(124,124)` is replaced by an outward ring sweep that stops at the first
  cell the tool accepts, with `BANISHED_PLACEMENT_CELL=<row>,<col>` and
  `BANISHED_PLACEMENT_RADIUS` overrides. A 3x3 WoodHouse always fitted at the valley
  centre; the larger lodges frequently do not, and a single fixed cell would have
  reported `0` for a tool that works — a false negative of the kind this codebase is
  repeatedly burned by. The failure line now names the radius searched.

### Action taken

The `starttown.cpp` comment is corrected to state that both tools ARE ported and
default-on, to name the description→tool mapping for each of the three templates, and to
give the exact env recipe for driving the real path. The spawners themselves are kept —
they are deterministic and instant, which is still the right fixture for most tests.

Two further stale comments found and corrected in the same area:

* `code/Game/Player/tool_activation_standalone.cpp:262-263` listed `CropField.rsc`
  alongside `Pasture.rsc` as carrying `CreateFencedDescription`. It does not — it
  declares `createdrag`. The error was inert (the fenced node is right for Pasture
  either way) and self-contradicted the correct DragPlacementTool comment fourteen lines
  below, but it is exactly the cross-reference that sends the next reader hunting a
  crop-field bug in the wrong tool.
* `code/Game/Player/dragplacementtool.cpp:11-13` claimed "Remaining slots (Draw,
  CalcPath) ... are not yet reimplemented". Both are implemented further down that same
  file. Since `DragPlacementTool` is the ONLY route to a player-placed crop field, this
  stale line reads as "crop fields cannot be placed".

---

## Verification run

Deployed exe timestamp checked against the build before believing any result
(`10:55`, matching `standalone/build/reimpl-x64.exe`).

```
BANISHED_AUTOSTART=1
BANISHED_TOOL_HARNESS=pasture
BANISHED_PLACEMENT_TEMPLATE='Template\CropField.rsc'
BANISHED_CROPFIELD_TRACE=1 BANISHED_CROPFIELD_FORCE_START=2
BANISHED_FOOD_WORKERS=4 BANISHED_FINISH_BUILDS=600 BANISHED_FORCE_SPEED=10
```

A crop field placed **through the real tool path** — not the demo spawner:

```
[ToolHarness][pasture] resolving template "Template\CropField.rsc"...
[ToolHarness][pasture]   -> 0000017a00232400
[ToolHarness][pasture] SetEditMode — activating ... via the tree...
[ToolHarness][pasture] after SetEditMode: activeTool=0000017a034dece0
[ToolHarness][pasture] rect: (252,162)-(259,169) naturals=0
[ToolHarness][pasture] CalcCost -> rc=0 (0=ok,1=small,2=big,3=blocked) w=8 h=8 perimeter=1
[ToolHarness][pasture] CalcPath (commit) -> 1
```

`SetEditMode` is the real activation: it walks the tool-mode tree, matches the
template's `CreateDragDescription` and installs `DragPlacementTool`. `CalcPath` is the
drag-release commit.

That tool-placed field then ran its ported state machine end to end.

**2 → 9 (plant).** The field was found in state 2 with `slots(+0x60)=0`; event 3 was
posted; the slot array then read:

```
[CropSlots] total=64 growing=0 done=0 | state histogram 0:64 ...
[CropSlots] state0 slots: 0(cell=62460,mark=1) 1(cell=62461,mark=0) 2(cell=62462,mark=1) ...
```

64 slots, each carrying a cell index and a shuffled `plantHere` mark. Building that
array — resize to `w*h`, fill `cellIndex`, mark every `spacing`-th cell, Fisher-Yates
shuffle — is **exactly and only** what `CropField_State9Enter` (`0x14011a310`) does.
That body carries no `fprintf` of its own, which is why there is no `[CropXition]` line
for it; the slot array is its signature.

**Farmers plant it for real.** With `BANISHED_FOOD_WORKERS=4`:

```
[FoodWorkers] frame=400: CropFieldComponent ... cascade seated 4/4 — roster(+0x60) 0 -> 2
[CropSlots] total=64 growing=8  done=0 | state histogram 0:56 1:8  ...
[CropSlots] total=64 growing=24 done=0 | state histogram 0:40 1:24 ...
[CropSlots] total=64 growing=62 done=0 | state histogram 0:2  1:62 ...
```

so the `PlantArea` work posted by state-9 enter is being claimed and executed by real
citizens, not stamped in by a fixture.

**9 → 10 → 11, one log line per transition:**

```
[CropXition] state 9 -> 10 TEND (growing=64 done=0 total=64)
[CropXition] STATE 10 ENTER - posting TendArea
[CropXition] event 4 -> STATE 11 HARVEST
[CropXition] STATE 11 ENTER - posting HarvestArea
```

**Both arms of the harvest trigger behave faithfully.** `CropField_State10PerFrame`
(`0x140119fd0`) has two independent arms and the trace shows each doing its job — the
season arm correctly withholding, and the ripeness arm eventually firing:

```
[CropHarvest] ... temp=42.6 (min 33.0) month=0 seasonArm=0 maturity=0.0042 (ripe>0.9900) -> harvest=0
[CropHarvest] ... temp=44.7 (min 33.0) month=0 seasonArm=0 maturity=0.0482 (ripe>0.9900) -> harvest=0
[CropHarvest] ... temp=77.6 (min 33.0) month=4 seasonArm=0 maturity=0.9739 (ripe>0.9900) -> harvest=0
```

`seasonArm=0` throughout because month 0–4 is outside the `[6,11]` window the unsigned
`(month - 6) > 5` idiom encodes — correct. Maturity climbs monotonically from 0.0042
past 0.99, at which point the ripeness arm fires event 4 and the field enters state 11
and posts a `HarvestArea`. A maturity that climbs is itself proof that the growth tick
is advancing the planted crops.

`standalone_crash.log` is 0 bytes across all three runs — no fault.

### A false negative worth recording

An earlier run of the same recipe added `BANISHED_BUILDERS=20` and the field sat in
state 9 forever with `growing=0`, which looks exactly like a broken plant body. It was
not:

```
[FoodWorkers] frame=400: CropFieldComponent ... cascade seated 0/6 — roster(+0x60) 0 -> 0
  <-- NO SEAT LANDED HERE (no idle laborer? ...)
```

The Easy start town has six citizens; converting twenty of them to builders left no
idle laborer for the farm seat. The state machine was fine — there was simply nobody to
do the work. Do not add `BANISHED_BUILDERS` to a small-town food test.

---

## Files touched outside this lane

* `standalone/tool_harness.cpp` — `FirePlacementTool` template + cell overrides (shared
  harness file; additive, default behaviour for `WoodHouse` unchanged apart from the ring
  sweep, which still tries `(124,124)` first).
* `code/Game/Player/tool_activation_standalone.cpp` — comment only.
* `code/Game/Player/dragplacementtool.cpp` — comment only.
* `code/Game/Game/starttown.cpp` — comment only.

No new `RegisterExeAddr` / `RegisterDispatchSlot` was needed, so
`standalone/reg_closeout_lane_a.cpp` was not created.
`tools/dispatch_slot_map.py --check`: **0 mismatches, 0 double-registrations.**
