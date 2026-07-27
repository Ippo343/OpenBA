# Bridges are built end-to-end — the three open bridge items, closed

Session 2026-07-26, on `wave/upgrade-path-complete`, following `405997e` ("Open the bridge").
The three items left open there were:

1. Bind `Picking::Scene::SetMeshUserData` — make bridges click-selectable, and fix the root
   cause behind the `edittool.cpp:833` guard rather than the symptom.
2. Watch a bridge through to completion — confirm builders finish it.
3. `BuildRequirementUI` count mismatch (`reqCount=4` vs `labelCount=3`), guarded in `e700`.

All three are closed. (2) took four more fixes than it looked like it would, and each one was
a different flavour of the same standing trap: **ported, registered, and reached by nothing.**

## The reproducible test: `BANISHED_TOOL_HARNESS=bridge`

New harness scenario (`standalone/tool_harness.cpp`). It is cursor-free, like the road one:

```
BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_FORCE_SPEED=40 \
BANISHED_TOOL_HARNESS=bridge BANISHED_BUILDER_AUTOPOOL=1 \
BANISHED_LIVE_INTERACT_ONADD=1 ./reimpl-x64.exe
```

* Activates the BridgeTool through the real `SetEditMode` path (`Template\WoodBridge.rsc`;
  override with `BANISHED_BRIDGE_TEMPLATE`).
* Scans the map for every bank→water→bank run using the tool's own `bridgeDesc` masks, then
  tests them **nearest-town-first** and lets **CalcCost be the judge**. The town centre is the
  average of the founding houses' `MapComponent` cells (map centre if the town isn't up).

  This matters more than it sounds. The first version took the first *valid* span, which on
  seed 12345 was row 6 — the top edge of the map, ~118 tiles from town. It built, but the
  builders spent the whole job walking and several died on the way. Nearest-first picks
  col 140, rows 126..136 (`dist² = 113`, ~11 tiles out) and the same bridge finishes at
  **frame 661 instead of 6061.** The candidate buffer evicts its farthest entry when full
  rather than dropping the overflow, because a plain cap silently biased the pick toward
  whatever the scan reached first (that alone was the difference between `dist²` 349 and 113).
* Commits with `CalcPath`, dumps the master and every segment's component set, then polls the
  build sites every `BANISHED_BRIDGE_POLL` frames (default 600) until each reports
  `delivered >= required`. `BANISHED_BRIDGE_WAIT_FRAMES` bounds the wait.
* `BANISHED_BUILDER_AUTOPOOL=1` is **required** — nothing gets built without it.

The verdict line is `[Bridge] COMPLETE — builders finished the bridge.`

## What a bridge actually is

Worth writing down, because it drove every diagnosis:

```
master   = 15 components incl. StateMachine + Build + WorkPlace + MultiObject + Interact
segments = MapComponent, ModelComponent, PickingComponent, HighlightComponent,
           FloorComponent   (+ InteractComponent on the two BANK END PIECES only)
```

The **master is the construction site** — one `BuildComponent`, one work order for the whole
span. The segments are floor/deck pieces and carry no `BuildComponent` at all. So "did the
builders finish it" is a question about the master, and the harness polls both.

Cost scales with the span: `costQty=4 mult=12` → 48 logs, plus 12 of the second material,
then 36 units of build labour.

## The five fixes

### 1. Picking user data — `PickingComponent+0x30`, not the `SetMeshUserData` call

Retail's per-segment tail (`0x1400faab3`..`0x1400fab7b`) finds the segment's
`PickingComponent` and does:

```
*(pickComp + 0x30) = masterEntity;                    // always
if (*(int*)(pickComp + 0x28) >= 0)                    // only if a mesh is already registered
    Picking::Scene::SetMeshUserData(scene, meshId, *(pickComp + 0x30), 0, 0, nullptr);
```

`+0x30` is what `PickingComponent::OnAdded` later hands to `Picking::Scene::AddMesh` as the
pick user data, so **seating it is the load-bearing half**; `SetMeshUserData` only covers the
already-registered case. The call is now bound (slot `0x1401529a0`), but the fix that matters
is the seat plus an explicit `PickingComponent_RegisterMeshForEntity` where retail's OnAdded
would run — `PickingComponent` is an inert factory here, so that broadcast lands on a stub.

`PickingComponent_RegisterMesh` now **defaults** `+0x30` instead of overwriting it, which is
what lets CalcPath's re-own survive the fused Startup+OnAdded.

Verified: every segment reports `pickId=31..35 userData=<master>`. Clicking any deck piece
selects the whole bridge, which is retail's behaviour.

> Finding the slot: the exe's `.rdata` dispatch table stores an **RVA of the import name
> string**, and each name string has a 2-byte prefix that is the export's **ordinal**.
> `0x1401529a0` holds `0x1c0608`; at `0x1401c0608` sits `0f 4c` (= 3916, SetMeshUserData's
> ordinal) followed by the mangled name. That is a general recipe for identifying any slot.

### 2. The transform offset — the misread that made bridges unbuildable

`bridgetool_calcpath.cpp` used to say retail "computes a per-segment world position … but then
passes a ZERO offset to `FromYawPitchOffset`, so the value never reaches the transform."

**The argument order was misread.** At `0x1400faa5d`:

```
LEA R9,[RSP+0x50]     ; arg4 = offset  <- the computed world position
LEA R8,[RBP-0x70]     ; arg3 = pitch   <- this is the zero
LEA RDX,[RBP-0x18]    ; arg2 = yaw
LEA RCX,[RBP+0x54]    ; retbuf
```

The zero is the **pitch**. The position is the offset, and it reaches the transform. Dropping
it parked every bridge entity's transform at the world origin.

That is not cosmetic: `FUN_140122be0` (`CalcBestTargetPos`, how `GatherRawMaterials` finds a
tile a builder can stand on) transforms the description's access points **by the entity's
`ModelComponent+0x148` transform**. At the origin, no access point is anywhere near the
bridge, so the gather bailed `no target pos (found==0)` on every tick, forever.

Ported as `CellRectCentre`: sample `fn_MapGetTerrainPos` at the two opposite corner cells of
the rect and average, with cell→map-space `(col - c, row - c)`, `c = (gridSize>>1) - 0.5`, and
z forced to 0 — exactly `0x1400fa218..0x1400fa363`.

### 3. `MultiObjectComponent`'s type-id global was never registered

`0x1401c7ac0` (writer confirmed: `MultiObjectComponent_RegisterType` @ `0x14014f475`) had no
entry in `standalone/reg_economy_typeids.cpp`, so `ResolveAddr` returned 0 and
`FUN_140004740` — the shared "find the owner's `MultiObjectComponent`" walk — compared every
component against a **null type-id pointer**, matched nothing, and every caller silently took
its no-MultiObject branch. Same silent-null class as the `CreateBridgeDescription` gap.

⚠ This global is shared: `BuildComponent_helper_8720` and `StatusIconComponent` read it too.
Filling it un-dormants those paths — A/B the default build (task #20).

### 4. `InteractComponent`'s on-added slot was a stub

`InteractComponent::Draw` (`0x140123110`) rebuilds the interact **use-location list**
(`+0x28` array / `+0x30` count) from the owner's MultiObject children. `FUN_140122b00`
recurses over exactly that list looking for a standable tile.

A bridge master's own footprint is **all water**. Its only reachable access points are the two
bank end-pieces — which live in that child list. Empty list → no access point → no haul.

The function was ported and registered at its exe address, and reached by nothing: slot 6
(`+0x30`, the on-added broadcast that `ComponentEntity_OnAddedPass` sends to every component
whose `+0x20` has bit 0) was an inert stub. `InteractComponent`'s flags are `0x81`, so the
broadcast always arrived and always landed on the stub. **Byte-for-byte the
`CitizenComponent::OnAdd` gap from the A-wave.**

Now wired, opt-in as `BANISHED_LIVE_INTERACT_ONADD=1` (it changes every entity that owns a
MultiObjectComponent, not just bridges). Trace: `[Interact] Draw self=… children=2 from 5
MultiObject entries`.

### 5. `BuildRequirementUI` double-counted every requirement

The real cause of `reqCount=4` vs `labelCount=3`, and it was not accumulation across dialog
opens:

```cpp
fn_ExeArrayPush((void*)(s + 0x50), *(int*)(s + 0x58) + 1, countPtr);
*(uint32_t*)(s + 0x58) = *(uint32_t*)(s + 0x58) + 1;   // <-- push already wrote the count
```

`fn_ExeArrayPush(arrayBase, …)` takes `{data, count@+0x08, cap@+0x0c}` and stores the new
count itself. The extra increment made `+0x58` twice the requirement count — a two-material
building (the bridge: logs + stone) landed on 4 against 3 label elements, and
`UpdateToolUI_e700`, which iterates `+0x58` and indexes the `+0x40` label array, walked off
the end. Retail's inline expansion stores the count once.

Both counters are also reset at the top of the populate now, because standalone reuses the
controller across opens while retail constructs it fresh.

After: `[e700] ctrl=… reqCount(+0x58)=2 labelCount(+0x48)=3`, no WARN lines. The `e700`
null-element guard stays as belt-and-braces.

## Proof

(From the first proving run — the far-map span, before the nearest-town picker.)

```
[Bridge] CalcPath: start=1610 end=1623 snapped=1623 len=12 -> 5 segments, 12 cells
[Interact] Draw self=… children=2 from 5 MultiObject entries
[GRMExec] slot[0] validateWorkAssign=1 availAmt=123 remaining(clamped)=9 targetCell=1623
[BuildTrace] HaveAllMaterials … delivered=48 required=48 (costQty=4 mult=12) OK
[BuildTrace] HaveAllMaterials … mat[1] delivered=12 required=12 OK
[BuildTrace] State8Update: progress 36>=target 36 -> requested 8->2
[BuildTrace] State8Finalize: WorkPlaceComponent state-2 enter (edf0) profession … -> 0x0
[Bridge] frame 6061 master=… delivered=38 required=36
[Bridge] COMPLETE — builders finished the bridge.
```

`standalone_crash.log` 0 bytes throughout. Reproduced four times: the far span (complete at
frame 6061), the same span with the `e700` fix and no `BUILD_TRACE` (4861), the nearest-town
span (1261), and the nearest-town span with cap eviction (**661**).

## A/B against the default build (the shared `0x1401c7ac0` fill)

Standing town-health recipe, default flags, this wave stashed vs applied:

| frame | baseline (pre-wave) | with the wave |
|---|---|---|
| 3000 | 27 citizens | 27 citizens |
| 6000 | **16** (−11)        | **17** (−10) |

No regression — and note the **town loses ~40% of itself between frames 3000 and 6000 in BOTH
builds.** That is pre-existing and does not match the brief's recorded 24→35-over-21 000-frames
soak, so either that soak ran a different configuration or something regressed between then and
now. It is a separate thread from this wave; do not attribute it to the type-id fill.

## Open

* **Graduate `BANISHED_LIVE_INTERACT_ONADD`** — the A/B above was run with it OFF, so the flag
  itself still needs a soak of its own before it goes into `kDefaultOnFlags`.
* **The frames-3000→6000 die-off** above, which nothing in this wave caused or fixed.
* **`PickingComponent::OnAdded` (`0x140128f50`) is still unregistered** and prints
  `NOT IMPLEMENTED` on every entity register. Wiring vtable slot 6 for `PickingComponent` the
  way `InteractComponent` now is would make pick-mesh registration automatic and retire the
  explicit `PickingComponent_RegisterMeshForEntity` calls in `calcpath` and `tool_base_tile`.
* The `[e700]` shape line is a one-shot diagnostic; drop it once the create-bar is trusted.

## Traps this wave paid for

* **A "deliberate, non-fatal omission" is a claim that needs the disassembly re-read.** The
  zero-offset note was written in good faith and was wrong by one argument slot; it turned an
  unbuildable bridge into a documented feature.
* **`RegisterExeAddr` + a correct port + a real caller is still not enough** — if the caller is
  a *vtable slot* on an inert factory, the function is dead. Third instance now
  (`CitizenComponent::OnAdd`, `PickingComponent::OnAdded`, `InteractComponent::Draw`). The tell
  is the same every time: a hand-rolled workaround exists nearby to compensate.
* **An unregistered type-id global disables a whole shared finder silently.** `FUN_140004740`
  returned null for every entity in the game and nothing logged. When a finder "finds nothing",
  check its type-id global before its logic.
* **Follow the material, not the state.** `[GRMExec]`'s first-failure trace named the wall in
  one line after two rounds of guessing from build state alone.
