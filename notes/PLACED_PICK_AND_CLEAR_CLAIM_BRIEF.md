# The two ClearArea-wave opens: placed-foundation click-pick, and who runs the clear job

Session 2026-07-28, on `wave/upgrade-path-complete`, following `14da117`. The two items the
previous session left (both found by hand-play):

* **#1** A hand-placed building cannot be clicked to open its info dialog.
* **#2** The ClearArea job is POSTED but no tree was ever seen falling — does anyone claim it?

Both are answered. **#2 is fully closed and the blocker was already fixed by an existing
opt-in flag.** #1 is root-caused and fixed behind `BANISHED_LIVE_PLACE_PICKMAP`, with one
consequence that needs a decision before it graduates.

Repro for both: `BANISHED_TOOL_HARNESS=clearplaceover BANISHED_CLEAR_PLACE_OVER=1`, which
drives the real CREATE path to place a WoodHouse on a resource cell.

---

## #2 — a citizen DOES claim it. The blocker was `WorkPlaceComponent+0x50`.

The previous session's "no tree fell" was not a scheduling failure. Instrumenting with
`BANISHED_CLEAR_TRACE=1` shows the whole chain working: a laborer claims `kind=0x401`,
`ClearArea::Execute` is entered — and then bails on its second gate, every tick:

```
[ClearExec] #1 ClearArea::Execute self=... trees=0 ground=0 flags(t/g/b)=1/0/1
[ClearExec]   OWNER field_10=... compCount=19 field_18(WorkComp)=...
[ClearExec]   EARLY-RETURN buildState-inactive: bsComp=... +0x50=0     <-- 11 times
```

The gate is faithful — retail `0x1400e5ee0` has it verbatim:

```c
pIVar15 = FUN_140002e10(param_1[2], &local_res8);          // FindComponent(WorkPlaceComponent)
if (*(longlong *)pIVar15 != 0) {
    pIVar15 = FUN_140002e10(param_1[2], &local_res20);     // re-find (retail really does it twice)
    if (*(char *)(*(longlong *)pIVar15 + 0x50) == '\0') return 1;
}
```

`FUN_140002e10` searches `DWORD_1401c80e0` = **WorkPlaceComponent** (confirmed by
decompiling it, not by the name). And `WorkPlaceComponent+0x50` is the **accepting-workers
byte, which retail's ctor `0x14012f2e0` initialises to 1** — a fact already recorded when
the WPC controller was ported, along with the note that *"the inert zero-fill missed it"*.

So the clear job was blocked by a component field our inert factory leaves at 0. Nothing in
the ClearArea wave was wrong.

**Measured with `BANISHED_LIVE_WPC_CONTROLLER=1` (already-existing opt-in flag):**

```
[ClearExec] area=(106,105)-(4,5) totalSlots=2 tileCount=7 clearTrees=1
[ClearExec]   P1 tile[0] ent=... natRes=... cell=27502 buildResult=1
[ClearExec]   P1 tile[0] ent=... DISPATCHED PerformWork chop (result=2) workId=1
[ClearExec] #2 ... trees=1 ...
[ClearExec]   P1 tile[1] ent=... DISPATCHED PerformWork chop (result=2) workId=1
[ClearExec] #3 ... trees=2 ...
[ClearExec]   EARLY-RETURN totalSlots-done: slots=2 (trees+ground=2)
```

Citizens chop. The tree counter advances 0 → 1 → 2 and the job retires itself on
`totalSlots-done`. **Open #2 is closed.**

Consequence for the flag ledger: `BANISHED_LIVE_CLEARCTRL` has a hard dependency on
`BANISHED_LIVE_WPC_CONTROLLER` — with CLEARCTRL alone the work is posted and then refused
forever. Neither is default-on today, so the pair must graduate together (the same coupling
`SM_ONADDED`/`STORAGECTRL` have).

---

## #1 — both pick routes were dead, and only one of them was supposed to work

`EditTool_OnKey_Impl` (`edittool.cpp:700`) picks in two stages, the second overriding the
first:

1. **cell route** — map ray-hit → cell index → `FUN_1400a0280` → `cell+0x00 ?: cell+0x08`
2. **mesh route** — `Picking::Scene::Intersect` → `GetUserData`

For a hand-placed building, measured with the new `BANISHED_PLACE_PICK_DIAG=1`:

```
[PlacePick] entity=... footprint 20 cells: mine=0 foreign=16 empty=4 CELL-PICK DEAD
            | modelVariant=-1 pickingId=-1 MESH-PICK DEAD | pickmap=off
```

Zero of its own footprint cells, no pick mesh. Both routes dead → `foundEntity == 0` → no
dialog. The 16 "foreign" cells are still held by the **trees**, which is why the cell pick
does not merely fail — it resolves the tree instead.

### It is an ORDERING bug in the mesh route. The cell route is a red herring.

> **This section replaces a wrong first answer.** The first pass concluded the cell route was
> load-bearing and wrote the building into `cell+0x08` via `41b0`/`dc20`. A user test of the
> shipped game refuted it: placing a building over trees/rocks in retail does **not** remove
> them — it drops a foundation decal, and clicking that decal opens a dialog reading
> "remove X of Y" for the resources. Both halves of that observation are incompatible with
> the first answer. What follows is the measured version.

**Retail never runs the occupancy write for a house at register time.** The chain is
`MapComponent::Draw` (slot 6) → gate `MapDescription+0x30 != 0` → `41b0` → gate
`desc+0x31 == 0` → `dc20`. Measured on the placed WoodHouse:

```
mapDesc=... +0x30(drawGate)=0 +0x31(skipOccupancy)=0 +0x32(minimap)=1
```

`+0x30` is **0**, so `Draw` returns before `41b0`. No cell write, no eviction — the trees
keep their cells, which is exactly what makes "remove X of Y" possible.

**What makes the foundation clickable is the mesh route, and it is decided by component
array order.** `ComponentEntity_OnAddedPass` dispatches slot 6 in array order; measured on
the placed WoodHouse:

```
component order: Model=[3] Picking=[5] Build=[16] Map=[0]  -> Picking BEFORE Build
```

So in retail: at index **[5]** `PickingComponent::OnAdded` registers the pick mesh while the
model still carries the **foundation decal** (`modelVariant = 0`, set by
`ToolTile_SetPlacedModel(foundation=true)`); only later, at index **[16]**, does
`BuildComponent`'s own OnAdded Stage 3 stamp `modelVariant = -1` to hand the model to the
build ramp. The `modelVariant < 0` guard in `PickingComponent::OnAdded` never sees `-1`
because it runs eleven components earlier.

Standalone left `PickingComponent` slot 6 a stub and compensated with explicit
`PickingComponent_RegisterMeshForEntity` calls at each creation site — and the placement site
calls it **after** `RegisterEntity`, i.e. after index [16] has already stamped `-1`. So the
guard bails and a hand-placed building registers no pick mesh at all. **That is the bug.**

### Fix — wire slot 6 (`BANISHED_LIVE_PICK_ONADDED`)

`PickingComponent_InertVtable[6] = PickingComponent_OnAddedVSlot` → `RegisterMesh`, putting
the registration back in its retail window. `RegisterMesh` now also honours retail's
`pickingId(+0x28) < 0` guard, which it previously skipped on the (since-obsolete) grounds
that standalone left `+0x28` zeroed — `PickingComponent_InitBody` has stamped `-1` per
instance for some time. The guard makes the legacy explicit calls idempotent no-ops rather
than double-registrations. A caller that deliberately re-points userData (BridgeTool re-owns
each segment to the master) seats `+0x30` *before* `RegisterEntity`, so slot 6 sees the
right owner.

### Measured A/B (`clearplaceover` harness, default flags)

| | control | `BANISHED_LIVE_PICK_ONADDED=1` |
|---|---|---|
| `pickingId` on the placed building | `-1` | **27** |
| mesh pick | DEAD | **OK** |
| `modelVariant` after register | -1 | -1 (unchanged — Build still un-frames, as retail) |
| footprint cells held by the trees | 3–16 | **unchanged** (nothing evicted) |
| duplicate registrations | 0 | **0** |
| `standalone_crash.log` | 0 bytes | 0 bytes |

Bridge harness with the flag on: unchanged, 0-byte crash log. Bridge segments still register
nothing, because their `ModelDescription+0x34` is `-1` — pre-existing and faithful, not
affected by the ordering fix.

Default (no harness, no flags, 65 s): 0-byte crash log, 9 264 entities, 7 registrations,
0 duplicates.

---

## When a building DOES claim its cells: MapComponent slot 12, driven by ClearArea state 2

Chasing the follow-up question — *"was the cell registered?"*, asked after manual play found
the foundation clickable but fiddly — produced the missing half.

`MapComponent` vtable slot **12** (`+0x60`, retail `0x1401243c0`) is three instructions:

```
CMP EDX,1 ; JZ 0x1401241b0 ; RET        // on message 1, run the occupancy write
```

`+0x60` is the per-component **message** slot. The broadcaster is
`ModelComponent_helper_2b70(entity, msgId)` (`0x1400c2b70`), and a byte-pattern sweep of every
`CALL [RAX+0x60]` in the exe shows exactly one caller that passes **1**:

| site | EDX |
|---|---|
| `ClearAreaDescription_helper_77f0` — **ClearArea state-2 ENTER** | **`MOV EDX,1`** |
| `BuildDescription_helper_7600` / `_7ae0` (State8Enter) x3 | `XOR EDX,EDX` |
| `sub_8720` (BuildComponent OnAdded body) x2 | `XOR EDX,EDX` |
| `PlacementTool_helper_d650` | 0 |

So **a building claims its footprint cells when its CLEARING FINISHES**, not when it is
placed. That resolves every loose end at once:

* a fresh foundation is pickable only by its decal mesh — fiddly, and exactly what manual
  play reported;
* the trees keep their cells while the harvest runs, which is what feeds "remove X of Y";
* `dc20`'s stale-occupant eviction is safe *because* it runs then — by state 2 the citizens
  have harvested the trees, so it only sweeps stragglers. The branch is real; the first pass
  simply had it on the wrong trigger.

Standalone already ports the state-2 handler and does broadcast event 1 — but slot 12 was an
unwired stub, so the message landed on nothing. `MapComponent_ApplyOccupancy_41b0` (written
during the wrong-trigger pass) turned out to be the correct body; it now hangs off
`MapComponent_Slot12VSlot`, and the placement-path call is deleted.

**Both flags graduated to `kDefaultOnFlags` 2026-07-28 after user verification in real play**
(`BANISHED_LIVE_PICK_ONADDED`, `BANISHED_LIVE_MAP_SOLIDIFY`). Default regression: 0-byte crash
log, 9 996 entities, 12 pick registrations, 0 duplicates, 0 `dc20` calls (nothing broadcasts
event 1 while `LIVE_CLEARCTRL` is off).

### `PickingComponent::OnAdded` is a REFRESH, not a one-shot (bug introduced and fixed)

The first cut of the slot-6 wiring added a bare `if (pickingId >= 0) return;` guard, justified
from a code comment. The disassembly of retail `0x140128f50` says otherwise — its **first**
act is to call its own vtable `+0x38` (`PickingComponent::OnCreate` = RemoveMesh + reset to
-1), and only then does it re-register under `modelVariant >= 0 && pickingId < 0`:

```c
(**(code **)(*param_1 + 0x38))();                       // teardown FIRST
if ((-1 < sVar2) && ((int)param_1[5] < 0)) { AddMesh(...); }
```

So the guard is always satisfied by the teardown preceding it. The pick mesh is meant to
**follow the model's frame** — retail re-invokes this from `sub_8720`
(`CALL 0x140128f50` at `0x140108b26`) as a site walks its construction frames. The bad guard
pinned the mesh to whatever frame was current at `OnAddedPass` time and made every later
refresh a no-op. Fixed by calling the teardown, which also subsumes the double-registration
concern that motivated the bad guard.

Consequence, and it is the faithful one: a **pre-construction foundation now has no pick mesh
at all** — index [5] registers, then `BuildComponent` at [16] stamps `modelVariant = -1` and
re-invokes OnAdded, which tears down and declines to re-add. Retail does the same, because
retail's `fd650` also leaves `modelVariant = -1`; the frame-0 mesh we were clicking before was
our own non-faithful `ToolTile_SetPlacedModel` stand-in (and being a small construction-frame
mesh is exactly why it was finicky).

### The one remaining delta

**User-verified in the shipped game: a just-placed foundation — before any builder starts, no
materials delivered — IS clickable immediately.** Ours is not. Everything from construction
onward matches (under-construction and completed buildings click normally).

Since a retail foundation provably registers **no pick mesh** (`modelVariant = -1` ⇒
`OnAdded` bails), retail's foundation click must come from the **cell** route. So something
registers a placed building in the cell array at placement time, and we do not do it. That
contradicts the state-2-only finding above, so the finding is incomplete rather than wrong —
the event-1 sweep was exhaustive for `+0x60`, which means the placement-time write is a
*different* mechanism, not another event-1 caller.

**The lead: `cell + 0x00`, the slot we never write.** `FUN_1400a0280` resolves
`cell+0x00 ?: cell+0x08`, and `FUN_1401018d0` — called by `fd650` with the tile rect just
before the entity is created — **reads `cell + 0x00`** (offset 0, not +8) to removal-mark
same-description occupants. So `+0x00` is a live occupant slot with its own writer, and every
write this project has ever made goes to `+0x08`. Measured: `slot0` is empty across the whole
footprint in every placement (`slot0: mine=0 other=0 empty=20`). Find the writer of `cell+0x00`
and the foundation-click gap almost certainly closes.

Timing was ruled out on the way: a clear-ground placement (`foreign=0 empty=20`) never fired
slot 12 either, because `BuildComponent` intercepts state 1 (component order Build=[16] beats
ClearArea=[14] in the tail-most chain walk), so a house reaches state 2 only at completion.

Everything else in the scenario is user-verified: builders assign, trees are removed by
citizens (not by eviction), hauling works, and the house builds successfully.

Per the user's call, the build **stays faithful** — foundation unclickable until construction
starts — rather than restoring the frame-0 stand-in, so no deviation is baked in before the
`cell+0x00` question is answered.

## The disproven branch, kept because it is easy to re-derive wrongly

`dc20`'s first branch does evict:

```c
if (*param_2 != 0 && *(longlong *)(lVar14 + 8) != 0) {          // cell already occupied
    if (*param_2 != lVar2 && (*(uint *)(local_68 + 0x40) >> 10 & 1) == 0) {
        FUN_1400c2ea0(local_68);                                 // ComponentEntity::MarkForRemoval
        DynArray8_ResizeFill(lVar3 + 0xd0, ...);                 // -> world removal queue
    }
}
```

That reading of the code is correct — `FUN_1400c2ea0` really is
`ComponentEntity::MarkForRemoval`. The error was concluding *when it runs*. Driving `41b0`
from the placement path (the first fix) skipped `Draw`'s `+0x30` gate and evicted **11 trees**
under one footprint, logged by the new `BANISHED_DC20_TRACE=1`:

```
[dc20] cell=27767 occupant=... flags+0x40=0xcb removalBit0x400=0 -> EVICT (MarkForRemoval + queue)
   ... x11
```

`MapComponent_ApplyOccupancy_41b0` and its call site remain in the tree behind
`BANISHED_LIVE_PLACE_PICKMAP`, **default off and marked DISPROVEN at both sites**, purely so
the measurement is reproducible. It should not be enabled for buildings.

Corollary: with the eviction gone, the previous session's ClearArea demo is *not* an artifact
— placing over trees genuinely is the scenario the harvest serves, and the "remove X of Y"
dialog is `ClearArea_CountRemaining` counting occupants of `cell+0x08` that are not the owner
(retail `BuildUI_helper_8330` has that self-skip verbatim).

---

## Harness / diagnostics added

| env | effect |
|---|---|
| `BANISHED_PLACE_PICK_DIAG=1` | after a real placement, report both pick routes: footprint coverage of **both** cell slots (`+0x00` and `+0x08`), `modelVariant`, `pickingId`, the `MapDescription` gate bytes, and the component array order |
| `BANISHED_LIVE_PICK_ONADDED=1` | **the fix** — wire `PickingComponent` slot 6 so the pick mesh registers in its retail window |
| `BANISHED_DC20_TRACE=1` | log `dc20`'s stale-occupant eviction decisions (occupant, `+0x40` flags, EVICT vs SKIP) |
| `BANISHED_LIVE_PLACE_PICKMAP=1` | ⚠ DISPROVEN, default off — drives `41b0` on the placement path and evicts the trees. Kept only to reproduce the measurement |

`BANISHED_CLEAR_TRACE=1` is **not** the per-frame flood the previous session's note warned
about — the `IsDone` dump is capped at 24 lines. It is safe to leave on for a soak.

## Traps this pass paid for

* **A gate one call up the chain silently changes what a function means.** `dc20`'s eviction
  branch was read correctly and the conclusion ("retail destroys trees under a footprint")
  was still wrong, because `MapComponent::Draw`'s `MapDescription+0x30` test means the whole
  chain never runs for a house. **Decompiling a function tells you what it does, never that
  it runs.** Walk up to the dispatcher and read every gate before asserting behaviour — the
  same lesson as `BuildComponent`'s slot-6 pass, one level higher.
* **Two wrong structural theories in a row, both from pure inference.** "Retail evicts the
  trees" and then "buildings live in `cell+0x00`" were each internally consistent and each
  refuted in minutes by one measurement (the `+0x30` gate byte; retail `8330`'s self-skip).
  When a theory is about *which slot / which order / which gate*, print the value — the probe
  that settled the whole thing was twelve lines.
* **A user's observation of the shipped game outranks a decompile.** "Placing over trees does
  not remove them, and the dialog says remove X of Y" contained both the refutation and the
  answer: trees survive ⇒ no occupancy write; the decal is clickable ⇒ the mesh route ran.
* **Component array order is load-bearing behaviour, not a detail.** `Picking=[5]` vs
  `Build=[16]` is the entire bug: the same function, the same guard, called eleven components
  later, does the opposite thing. Any workaround that "does what OnAdded would have done" but
  outside `OnAddedPass` has silently picked a new position in that order.
* **Sampling one cell libels a footprint fix.** The first probe read the rect ORIGIN, which
  `dc20` deliberately leaves empty (`'.'` = the ground in front of the building), and
  reported `CELL-PICK DEAD` on a run where 16 cells were correctly marked. Report coverage
  over the whole rect, never one corner.
* **A dead code path is not always an unported one.** Both 41b0 and dc20 were complete,
  correct and registered for months; a single `IsDerivedFrom` that standalone cannot satisfy
  was enough to make every caller route around them and hand-roll a subset. Before porting,
  grep for the address — it may already be there, one guard away from working.
* **"Nobody claimed the work" was false; it was claimed 11 times and refused.** The trace
  that would have shown it existed already and was behind a flag believed to be too noisy to
  enable. Check the noise claim before designing around it.
* **A component field left 0 by the inert factory reads as a legitimate state.** `+0x50 = 0`
  is "not accepting workers", indistinguishable from "never initialised" — the same shape as
  the SM default-0 trap and the `-1`-sentinel-reads-as-0 trap. Every retail ctor field the
  inert zero-fill misses is a latent gate of this kind.
