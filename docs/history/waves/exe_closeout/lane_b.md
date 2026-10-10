# Exe closeout — lane B (pastures)

Branch `worktree-agent-a49e7259841fe0c8b`, based on `wave/upgrade-path-complete` @ `4eeed89`.
Ground-truthed 2026-09-06 with one deployed debug run of `reimpl-x64.exe` (11:08 build).

Both lane-B rows of `docs/runtime_port/PLAN.md` §1.2 are **CLOSED — false premise**. Neither
feature was unported; both PLAN rows were transcribed from source comments that had outlived
the ports that replaced them. Verified by a single in-game run that places a pasture through
the real fenced tool and then picks its livestock type through the real dialog.

---

## Item 3 — "Fenced-area tool unported (pastures need scaffolding)" → CLOSED

`FencedTool` has been fully ported for some time. Every piece the retail acquisition path needs
is in the tree:

| Retail address | What | Where |
|---|---|---|
| `0x1401605d0` | FencedTool vtable (15 slots) | `code/Game/Player/fencedtool.cpp` |
| `0x1400dd070` / `0x140026930` | ctor / dtor | `code/Game/Player/fencedtool.cpp` |
| `0x1400dcd50` | Startup (slot 4) — the 5 description lookups + preview MeshInstance | `code/Game/Player/fencedtool.cpp` |
| `0x1400dc440` | Draw (slot 8) — rect preview | `code/Game/Player/fencedtool.cpp` |
| `0x1400db490` | CalcCost (slot 14) — rect validation | `code/Game/Player/fencedtool.cpp` |
| `0x1400dae00` | **CalcPath (slot 13) — the drag commit** | `code/Game/Player/fencedtool_calcpath.cpp` |
| `0x1400dbc80` | spawn one fence piece | `code/Game/Player/fencedtool_calcpath.cpp` |
| `0x1400db8c0` | walk one edge, spawn a run | `code/Game/Player/fencedtool_calcpath.cpp` |
| `0x1401290d0` | re-seat the shelter's pick owner onto the master | `code/Game/Player/fencedtool_calcpath.cpp` |

The tool is also reachable the way a player reaches it. `tool_activation_standalone.cpp`
inserts the `CreateFencedDescription -> FencedTool` node into the tool-mode tree
(`[Tools]   fenced node: CreateFenced=2515727256 -> FencedTool=3195986552` in the run log), so
`EntityUI::SetEditMode` — the function a Toolbar.rsc pasture button runs — installs a FencedTool.
`BANISHED_TOOL_HARNESS=pasture` replaces only the mouse (it supplies the drag rect); the
activation, validation and commit are the real retail-shaped path.

**Evidence run** (`BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_TOOL_HARNESS=pasturefull`):

```
[Tools]   fenced node: CreateFenced=2515727256 -> FencedTool=3195986552
[ToolHarness][pasture] SetEditMode — activating FencedTool via the tree...
[ToolHarness][pasture] after SetEditMode: activeTool=000001971d2a5db8
[ToolHarness][pasture]   vtable=... fencedDesc(+0x38)=... areaDesc(+0x40)=... fenceDesc(+0x48)=...
[ToolHarness][pasture] rect: (164,149)-(171,156) naturals=0
[ToolHarness][pasture] CalcCost -> rc=0 (0=ok,1=small,2=big,3=blocked) w=8 h=8 perimeter=28
[FencePiece] idx=2 cell=(164,149) 1x1 angle=90 ...        <- corner
[FencePiece] idx=5 GATE cell=(167,149) 2x1 angle=0 ...    <- gate
... 4 corners, 2 gates, straights + long straights on all four edges ...
[ToolHarness][pasture] CalcPath (commit) -> 1 (1=pasture placed)
[ToolHarness][pasture] sequence complete: 1/1 area(s) placed, side 8
```

The master entity, the AnimalShelter child (`[PickMesh] ... footprint=(169,152 3x5)`) and the
28-cell perimeter all exist, and the master enters `[WPCCtrl] state-0 (construction/f520)` — a
normal building site awaiting builders, exactly like retail.

`BANISHED_BUILD_DEMO_PASTURE` (`starttown.cpp`) is therefore no longer "the only way to get a
pasture"; it survives only as the instant-build shortcut for runs that must not wait for
construction. Its comment (and the matching one on `BANISHED_BUILD_DEMO_ORCHARD`) said "still
unported" and is corrected in this commit — that comment is what PLAN §1.2 row 3 was quoting.

## Item 5 — "Pasture dialog unported (livestock type from env var)" → CLOSED

`PastureUI` is ported *and* promoted off the `UI_DIALOG_STUB` list
(`code/Game/Dialog/pastureui.cpp`, retail vtable `0x14015f610`):

| Retail address | Slot | What |
|---|---|---|
| `0x1400b5090` | idx4 (+0x20) | setup: `SelectResourceUI` combo populate + find the `slider` element, id 501 |
| `0x1400b4e50` | idx7 (+0x38) | per-frame: combo enable + slider text |
| `0x140078d30` | idx8 (+0x40) | `SelectResourceUI::Slot8` combo refresh |
| `0x1400b4e20` | idx9 (+0x48) | events: combo `0x14`/500, slider `0x17`/501 |
| `0x1400b4c40` | idx11 (+0x58) | **SetLivestockType** (drives PastureComponent slot 14 cmd 2) |
| `0x1400b4d00` | idx12 (+0x60) | get current type |
| `0x1400b4cf0` | idx13 (+0x68) | category filter = 2 (livestock) |
| `0x140128570` | — | `PastureCalcMaxCount` |
| `0x1400b4fa0` | — | `PastureUI::UpdateSlider` |

**Evidence run** — same process, 240 frames after the tool commit, the panel is opened on the
pasture the tool just placed, via `EditTool::SelectEntity` (the in-world click path), and a
combo change is delivered as the framework delivers it (msg `0x14`, element id 500):

```
[PasturePanel] registry empty (n=0); EntityList scan of 17149 entities -> pastureComp=000001971c9f2d00
[PasturePanel] select pasture ent=000001971cb531f8 -> opened=1 dialog=000001971c64ebc8
               (comp=... type(+0x28)=0000000000000000 count(+0x38)=0 max(+0x40)=0)
[PasturePanel] SELECT: PastureUI=000001971d28cef8 combo=000001971c58f658 items=1 want=0
[PasturePanel] SELECT fired: item=0 -> pasture type(+0x28)=0000019719ab9980 max(+0x40)=4
```

`type(+0x28)` goes `0 -> 0x…9ab9980` and `max(+0x40)` goes `0 -> 4`: the player's combo pick
is what set the livestock type and recomputed the herd cap. `Pasture_AutoAssignType`
(`BANISHED_PASTURE_AUTOTYPE`) is not the mechanism any more — it survives only as a headless
shortcut for runs that never open a panel (stress towns, achievement census). Its comment said
"while the pasture dialog is unported" and is corrected in this commit; that comment is what
PLAN §1.2 row 5 was quoting.

---

## New harness: `BANISHED_TOOL_HARNESS=pasturefull`

`standalone/tool_harness.cpp`. Fires `FirePastureTool` at the settle frame and then
`FirePasturePanel` `BANISHED_PASTUREFULL_DELAY` frames later (default 240), so both halves are
proven **on the same pasture in one run**. Previously the panel half needed
`BANISHED_BUILD_DEMO_PASTURE`, i.e. the scaffolded master-only pasture — never a tool-placed one,
so the end-to-end path had never actually been run.

Making that work needed one fix inside `FirePasturePanel`: it took pasture[0] from the world
pasture registry (`gameWorld+0xf0` `+0x90`/`+0x98`), and **that registry only holds pastures that
have finished construction** — a freshly tool-placed pasture is a building site, so the registry
is empty (measured `n=0`) while the entity plainly exists. It now falls back to the EntityList
scan `FireOrchardPanel` already used (`gameWorld+0xe8`, match `PastureComponent` by the
component's own vtable+0x10 GetTypeId). That is not a workaround for a defect: clicking an
unfinished pasture opens its panel in retail too.

## Observation, not a defect claim

The livestock combo populated with **`items=1`** on a fresh seed-12345 start. The list comes from
the world resource-type array (`EntityList+0x120`) filtered to category 2, so one entry means one
livestock resource type is currently known to the world — consistent with retail, where the other
animals arrive by trade. Not investigated further; noted because a future reader comparing against
"cow + sheep + chicken" will otherwise read `items=1` as a bug.

## Registrations

None. No new `RegisterExeAddr` / `RegisterDispatchSlot` was required, so
`standalone/reg_closeout_lane_b.cpp` was never created.
`tools/dispatch_slot_map.py --check`: **0 name mismatches** (14 shadowed addresses, all the
pre-existing `IO::Serializer` double-registrations documented in PLAN §1.1).
