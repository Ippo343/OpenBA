# Food Production Wave — architecture brief

Scope: Crop Field, Orchard, Pasture, Fishing Dock, Hunting Cabin, Gatherer's Hut.
Recovered from Application-x64.exe (Ghidra), 2026-08-11.

## The six split into TWO families

| Building | Template | Component | Size | Parent |
|---|---|---|---|---|
| Crop Field | `CropField.rsc` | `CropFieldComponent` | 0x68 | ControllerComponent |
| Orchard | `Orchard.rsc` | `OrchardComponent` | 0x68 | ControllerComponent |
| Pasture | `Pasture.rsc` | `PastureComponent` | 0x68 | ControllerComponent |
| Fishing Dock | `FishermansDock.rsc` | `FishingComponent` | 0x58 | **ResourceProduceComponent** |
| Hunting Cabin | `HunterLodge.rsc` | `HuntComponent` | 0x58 | **ResourceProduceComponent** |
| Gatherer's Hut | `GathererShelter.rsc` | **`HerbalistComponent`** | 0x50 | **ResourceProduceComponent** |

**Family A** (0x68, ControllerComponent) = drag/fence-created *areas* with their own
per-class state machine. Orchard is fully ported and is the reference.

**Family B** (0x50–0x58, ResourceProduceComponent) = placed *radius* buildings that
all share ONE state machine. Porting that base covers three of the six at once.

> The Gatherer's Hut is backed by `HerbalistDescription`/`HerbalistComponent` — there is
> no "GathererComponent". Confirmed from `GathererShelter.rsc`'s description list.

## Family B: the shared ResourceProduce state machine

`ResourceProduceComponent`: ctor `0x14012b3d0`, vtable `0x1401643f0` (**22 slots**),
controller vtable `0x1401643a0` (9 slots). Subclasses do **not** replace the controller
vtable, so all of them share one state table.

State table `DAT_1401db920` (16 × 0xf8 entries), count `DAT_1401dc8a0`.
Constructed by `ResourceProduceDescription_StaticInit` (0x14014f910) — which only
runs the array ctor and sets count=0 — and *populated* by
`ResourceProduceDescription_helper_b230` (0x14012b230), reached via
`ResourceProduceDescription_Setup` (0x14014f970).

Registration helpers (the sub_f500/sub_f620 analogues):
- `0x140037dc0` register state — args `(id, enterHandler, …, trans38Handler)`;
  the middle two args are passed through and **ignored**. Writes id→+0x00,
  enter→+0x08, 0→+0x10 (trans30), arg5→+0x18 (trans38), 0→+0x20 (update).
- `0x140037ee0` add transition
- `0x140037f90` add event

### The table (exactly three states)

| State | Enter | trans38 (per-frame) | Transitions | Events |
|---|---|---|---|---|
| **2** (idle) | `helper_b1b0` 0x14012b1b0 | — | →13 `helper_af90`, →14 (null), →3 (null) | — |
| **13** (producing) | `helper_ae60` 0x14012ae60 | `sub_b190` 0x14012b190 | →14 (null), →3 `helper_af70` | 3 → `helper_adf0` |
| **14** (paused/full) | `helper_ad20` 0x14012ad20 | — | →13 (null), →3 (null) | 3 → `helper_acb0` |

- **State 2 enter** picks the next state from the component's `+0x48` byte
  (`produceFlag`): 0 → request state **13**, non-zero → request state **14**.
  It uses the standard "nested Tick if index<0, then node slot+0x08 RequestTransition"
  idiom already modelled by `ControllerLive::RequestStateTransition`.
- **State 13 enter** (if `param_1[9]`==0): resolves the owner entity, calls component
  vtable **+0x90 (CreateWork)**, hands the resulting work to `CitizenAI_impl_ec60(ai, 1, 0, work)`,
  flips the entity flag via `Entity_impl_fc00(entity, 1)`, then tail-calls **+0x98**.
  If `param_1[9]`!=0 it instead requests state 14.
- **State 14 enter** clears work (`FUN_14012ead0`/`FUN_14012eb50`),
  `Entity_impl_fc00(entity, 0)`, then tail-calls **+0xa0**.
- **State 13 trans38** is a one-line forward to component vtable **+0xa8** (produce tick).

### Component vtable slots 18–21 = the per-class production hooks

| Slot | Offset | Meaning | RPC base | Fishing | Hunt | Herbalist |
|---|---|---|---|---|---|---|
| 18 | +0x90 | **CreateWork** | `_purecall` (abstract) | `0x14011efe0` → FishingWork (0x70) | `0x1401226a0` → HuntWork (0x58) | `0x14004b160` → HerbalistWork (0x60) |
| 19 | +0x98 | OnEnterProducing | `0x140006b10` no-op | `0x14011ee80` | *(base)* | *(base)* |
| 20 | +0xa0 | OnExitProducing | `0x140006b10` no-op | `0x14011ee50` | *(base)* | *(base)* |
| 21 | +0xa8 | **ProduceTick** | `0x140006b10` no-op | `0x14011ec10` | *(base)* | *(base)* |

Slots 4–17 are byte-identical across all three subclasses (inherited from the base).
Each subclass overrides only 0,1,2,3 (dtor family + Shutdown) plus the table above.

- **Fishing** is the only one with a real produce tick. `0x14011ec10` scans a circular
  radius (radius from `+0x28` of the Radius description component) around the dock,
  counts tiles whose terrain-flag bit is set against the total in-radius count, and
  stores `count / (total * DAT_140172114)` into `FishingComponent+0x54` (`fishingRate`).
- **Hunt** additionally overrides component slot **9** (`0x140122740`, the per-frame
  Update — a time accumulator scaled by the terrain speed ratio) and its ctor
  (`0x1401228e0`) ORs capability bit **8** into `+0x20` to opt into update ticking.
  Fishing and Herbalist do NOT set bit 8.
- **Herbalist/Gatherer** overrides CreateWork only; all production happens citizen-side
  in HerbalistWork / GatherRawMaterials.

## Ghidra naming traps found in this wave

Read the vtable **bytes**, not Ghidra's names — several are wrong by position:
- `HerbalistComponent_Slot13` @ `0x14012b520` is shared by *every* RPC subclass
  (it is the base's slot 13), not Herbalist-specific.
- `ResourceProduceComponent_Controller_Slot13` @ `0x14012b4a0` is actually the
  **component** vtable slot 3 (Shutdown), not a controller slot.
- `HerbalistWork_Ctor` @ `0x14004b160` is **HerbalistComponent's vtable slot 18**
  (CreateWork), not the HerbalistWork constructor.

## Session 1 results (2026-08-11)

### Landed
- Shared ResourceProduce state machine ported (`resourceproducecomponent_states.cpp`).
- Slot 18 CreateWork for all three subclasses (`resourceproduce_creatework.cpp`).
- Fishing slots 19/20/21 (`fishingcomponent_produce.cpp`).
- Fishing / Hunt / Herbalist promoted off `COMPONENT_INERT_FACTORY` to live controllers
  (`BANISHED_LIVE_FISHING` / `_HUNTING` / `_GATHERER`, all still **opt-in**).
- Placement harnesses: `BANISHED_BUILD_DEMO_CROPFIELD` / `_HUNTERLODGE` / `_GATHERERHUT`
  (ring scan) and `_FISHINGDOCK` (terrain-scored, see below).
- `BANISHED_FOOD_WORKERS=<n>` staffing stand-in for all four.
- `BANISHED_FOOD_DIAG=<frames>` per-class census + town-storage totals.
- 8 unregistered type-id globals (`reg_food_typeid.cpp`) + PI (`0x140172184`).

### Measured state at end of session
10 800 frames, no crash, all three buildings:
`state 13 (producing) / staffed=2 / accepting=1`, dock rate `1.283`, hunt Time
accumulator advancing. **Town storage only declines** (citizens eating) — nothing is
deposited yet, which is why the three flags stay opt-in.

### What is proven to work
Component goes live → state 2 → state 13 → CreateWork mints the class's Work → posted
to the reservation ledger → WorkPlace opened → worker seated → the Work's `Slot4`
(Execute) runs. Breakpoint counts over one run: Herbalist Execute 145 / Slot6 54,
Hunt 149 / 148, Fishing 114. So the works genuinely execute and complete.

### The remaining gap: deposit
The yield is NOT produced by the Work object. `HerbalistWork::Slot6` is only a BST
removal (worker de-registration), and none of the three work files touch
CreateRawMaterial. `HerbalistWork::Slot4` instead locates a NaturalResource in radius
and pushes a **PerformWork action** (`0x1400c1eb0`, registered) onto the citizen; the
yield must come from that action running to completion. That action stack is the next
thing to trace.

Two hypotheses already **refuted**, so don't re-spend them:
- *"the buildings have no resources in range"* — the diag measures it: 851 resource
  cells in the gatherer's radius 30, 748 in the hunter's radius 34.
- *"the dock is badly placed"* — the placement harness scores candidates by exactly
  the slot-21 predicate; the dock rates 1.28–1.32 on 327/611 water tiles.

### Traps this wave paid for
- **Out-param finder double-deref** (3 sites). These finders write the component into
  the out-param and RETURN the out-param's ADDRESS, so retail's `*ret` and a local that
  received the value are the same thing. The extra deref passes a VTABLE where a
  component belongs.
- **CreateWork must call the Work's `*_ResolveFuncs()`** — only the FACTORY path calls
  it, so a directly-constructed Work carries null helper pointers.
- **"VCALL-of-0" with `RAX == RCX == the object`** is a null *function pointer*, not a
  vtable dispatch. The VEH dump's `vtable=` line is a guess.
- **A 0-byte `standalone_crash.log` is not a crash** — the file is pre-created. Test its
  SIZE, not its existence.
- An unregistered type-id global makes a correct predicate silently always-false: the
  dock rated `0.000` on measurably perfect water.

### Session 1b — two root causes found

**Gatherer: blocked UPSTREAM by worldgen, not by the port.** `HerbalistWork::Slot4`
ran 115 times and never reached its "valid herb found" branch, so the PerformWork
action was never built and `RemoveNaturalResource::Main` (which actually produces) was
reached ZERO times. A new nine-way rejection census (`BANISHED_GATHER_TRACE`) shows
`herbTypeMismatch` dominating with `ACCEPTED=0`. Our comparison matches retail
(0x1400ee3b0) key for key — the hut simply has nothing it accepts nearby: 605 resource
cells in radius, `acceptList=4`, `GATHERABLE=0`, and only **3 distinct
natural-resource templates exist anywhere within ±90 tiles**, none of them accepted.

> ⚠ The earlier "851 resource cells, so it is well sited" reading was WRONG — it
> counted any entity in a cell, overwhelmingly trees. Always count against the
> building's own accept list. The diag now reports `GATHERABLE` for exactly this.

**Fishing: unbounded recursion, fixed.** `FishingWork::Slot4 -> f8620 -> Slot4 …`
until STACK_OVERFLOW. Slot4's fallback looks up whatever workplace stands on its attach
cells; in retail `cell+8` there holds a NATURAL RESOURCE, in our standalone it holds the
BUILDING ENTITY, so the dock found its own work. f8620's re-entrancy guard cannot help
— retail stamps `work+0x2c` only AFTER Execute returns. Fixed by skipping our own
entity, documented as a divergence whose real cause is upstream (`cell+8` should not
carry the building entity).

## Session 2 results — SLOT COVERAGE COMPLETE

**132/132 component vtable slots across all six classes**, machine-checked every run by
`BANISHED_FOOD_PLACE`. Each slot is either ported from the retail body or verified
byte-for-byte equivalent to a retail no-op (`C2 00 00` + 0xCC padding — ten such
addresses, listed in `standalone/food_placement_harness.cpp`).

Both harness types now exist for all six:
- `BANISHED_FOOD_PLACE=<class>|all` — placement facts + per-slot coverage map.
- `BANISHED_FOOD_PANEL=<class>|all` — opens each panel, lists its controllers, reports
  the command verdicts that grey or enable each button.

### The single most productive tool
The coverage harness caught **five separate "ported but unwired" cases** — Fishing and
Herbalist `Shutdown`, then all three Family-A `Shutdown` bodies. In every case the
source tree looked complete while the vtable slot still held the inert stub. None would
have been found by reading code. If you add a slot, verify it with the harness.

### Deposit chain — where each class actually stops
Nothing deposits into storage yet. Each blocker is one named component:

| Class | Reaches | Stops at |
|---|---|---|
| Crop Field | state 9 (PLANT), staffed, crop type set | the posted PlantArea work is never CLAIMED — `PlantArea_Slot4_Impl` is never called |
| Fishing | accepts real spots (`ACCEPTED tile=... coverage=1`) | strictly downstream of the accepted spot, inside the action chain |
| Gatherer | forages, all filters instrumented | `GATHERABLE=0` — worldgen spawns only 3 natural-resource templates near the start, none in the hut's accept list |

Fixed this session on the way: PlantArea's cell index (out-param buffer too small,
READ side — the callee reads two ints, we passed one), and the crop-type selection
(`comp+0x50` is the chosen crop, filled from the world resource-type list at town
manager +0x120/+0x128).

⚠ Do NOT re-spend these: the resource-limit gate is FINE (measured 2400/5000), the dock
is well sited (327/611 water tiles), and "resources in range" must be counted against
the building's OWN accept list — a raw count is meaningless.

### Crop field — 30 000-frame soak result (measured, negative)

Do not assume "more time" fixes it; it was tried:

    [PlantArea] after 40 Execute calls: maxWorkers=14 noMark=600 alreadyInBst=0
                noPassableCell=0 noPath=0 ACCEPTED=25 (slotCount=64 treeCount=0 cap=2)
    frame=10000/20000/30000: CropField smStateIdx=1, staffed=2, accepting=1
    storage 4024 -> 3125 -> 2338 (declining throughout)

Reading:
- Execute is called only ~40–79 times in 30 000 frames (ONE census line, printed every
  40 calls) and `treeCount=0` — so citizens have STOPPED claiming plant work entirely.
  It is not slow progress, it is a stall.
- `ACCEPTED=25` of 64 cells, and `noMark` dominates. Each accept clears the mark for its
  cell (and its pair), so ~25 accepts exhaust ~50 of 64 marks. Once a mark is cleared the
  cell can never be retried.
- The suspected mechanism: a plant that is ACCEPTED but whose action later FAILS
  (CreateNaturalResource bailing) has already had its mark cleared. Retail restores marks
  only via slot 7 (OnCancel). If our chain reports a failed plant as COMPLETED rather
  than CANCELLED, the mark leaks and the cell is lost — the field then runs out of
  plantable cells having actually planted only a handful, and stalls in state 9 forever
  with no way to advance to state 10.
- Worth checking first next session: count `CropFieldComponent_OnMessageVSlot` msg-0
  binds against `ACCEPTED`. If binds << accepts, the leak above is confirmed and the fix
  is routing failed plants through OnCancel rather than OnComplete.

### ⚠ The mark-leak hypothesis above is REFUTED — measured

Counted probes over one run (crop field, 4 workers):

    CropFieldComponent::OnMessage (msg 0 bind)  64
    CreateNaturalResource                       64
    PlantArea::OnComplete                       32   (= 64 cells / 2 per paired action)
    PlantArea::OnCancel                     14 742

So ALL 64 cells are planted and bound. Planting is not leaking marks and is not
short-changing the field — the earlier "ACCEPTED=25 of 64, marks exhausted" reading was
an artefact of sampling a run at 40 Execute calls, not a stall in planting.

The real pathology is `PlantArea::OnCancel` firing **14 742 times** — roughly 230 cancels
per planted cell. OnCancel restores validity marks and removes BST entries, so the work
is being cancelled and re-offered continuously. That churn, not a shortage of plantable
cells, is what keeps the field pinned in state 9.

⭐NEXT: find the caller driving OnCancel. It is dispatched through the work vtable slot 7
(+0x38); candidates are the citizen task-cancel path (`0x1400c5400`) and Work slot 9
(Validate, `PlantArea_Validate_Impl`) returning "invalid" and forcing a re-plan every
tick. Instrument slot 9's verdict first — a Validate that always reports invalid would
produce exactly this signature.

Also worth re-checking once the churn stops: with 64/64 slots bound as "growing", state
9's per-frame should satisfy `growing + done == total` and advance to state 10, so the
state-9 pin may be a symptom of the churn rather than a second defect.

### ⚠⚠ THERE IS NO OnCancel CHURN — the 14 742 figure was a gdb misread

`info breakpoints` omits the "already hit" line entirely for a breakpoint with zero
hits, so every count is attributed to the breakpoint listed ABOVE its true owner when
any earlier breakpoint is cold. The "PlantArea::OnCancel 14 742" figure was actually
`FoodDiag_Tick`'s per-frame count. Re-measured with the cold breakpoint isolated:

    PlantArea::OnCancel               NOT HIT
    PlantArea::Cleanup (slot 8)       1
    CitizenAI_WorkScheduler_Rekey     14 134   (~once per frame, benign)

⚠ This is the SECOND time this session that mistake was made. When reading
`info breakpoints`, match each count to its breakpoint by checking the line immediately
ABOVE it, and treat a missing line as ZERO — never assume the counts line up positionally.

So the cancel-churn narrative is void, and with it the Validate investigation it
motivated. What the measurements actually establish for the crop field:

    64 cells planted, 64 msg-0 binds, 32 OnComplete (= 64/2 paired), 0 cancels

Planting runs to COMPLETION. Every slot should therefore read state 1 "growing", and
state 9's per-frame (`growing + done == total` -> request state 10) should fire.
It does not, and the field sits in state 9.

⭐NEXT, and it is now a narrow question inside OUR OWN port: dump the slot-state
histogram (`slot+0x04` across all 64) from `CropField_State9PerFrame`. Either the slots
are not reading 1 — in which case `s_slotRefresh` (ValidateEntityRefs, 0x14011aa20) is
resetting them, since it demotes any slot whose entity fails validation — or they are,
and the transition request itself is being dropped. Those are different fixes and the
histogram distinguishes them in one run.

### PlantArea::Validate is NOT the churn driver — eliminated by measurement

Named as prime suspect for the 14 742 OnCancel calls, then instrumented
(`BANISHED_PLANTAREA_TRACE` now also counts Validate verdicts) and ruled out: the
verdict census never reaches its 500-verdict print threshold, i.e. Validate's inner
loop barely runs. Two further facts settled while checking:
- retail's Validate (0x1400f3c70) passes a NULL out-task to Work::CanStartWork exactly
  as our port does, so that difference was never a defect;
- retail's Validate restores the validity mark for invalid tasks itself via a direct BST
  remove, NOT through slot 7 — so even a misbehaving Validate could not produce
  OnCancel hits.

So the OnCancel churn is driven from the CITIZEN side, not from the work. Remaining
candidates, in order: the citizen task-cancel path (`0x1400c5400`, CancelAllWork — note
PlantArea slot 8 calls it), and the work-scheduler re-key/eviction path
(`CitizenAI_WorkScheduler_Rekey`, 0x1400f8400). Put a counter on slot 8 first; if slot 8
is hot, the cancel originates in the citizen's action teardown.

### Crop field ROOT CAUSE located — slots demote to state 2

Slot-state histogram from `CropField_State9PerFrame` (BANISHED_CROPFIELD_TRACE):

    [CropSlots] total=64 growing=0 done=64 | histogram 0:0 1:0 2:64 3:0 other:0

Every slot reads **state 2 ("done")**, not state 1 ("growing"), even though msg-0 binds
fire 64 times and set state 1. So something demotes them immediately, and the only
thing that writes 2 is `CropFieldComponent_ValidateEntityRefs` (retail 0x14011aa20,
called every tick as the state-9/10/11 slot refresh): it resets any slot whose stored
entity is gone, is not a ComponentEntity, or whose build-state component link no longer
matches.

That fully explains the stall, and the mechanism is worth understanding before fixing:
- state 9's per-frame checks `done == total` FIRST and requests state **21**;
- 21 is a transition TARGET with no registered body (retail registers none either), so
  `Step(21)` returns -1 and the machine cannot enter it;
- the field therefore re-requests 21 forever and never reaches state 10.

Retail's per-frame has the same `done == total -> 21` branch, so retail must never see
all-slots-done while in state 9 — i.e. in retail the slots legitimately hold state 1.
The defect is the demotion, not the transition logic.

⭐NEXT: instrument the three reject branches inside `CropFieldComponent_ValidateEntityRefs`
(entity null / not ComponentEntity / build-state link mismatch) and see which fires. If
it is the build-state link, compare what msg-0 stores at slot+0x10 against what the
validator expects — msg 0 stores the NaturalResource component from finder 0x1400052c0,
and the validator re-derives it the same way, so a mismatch there points at the finder
returning a different component than the one the plant chain bound.

### Post-IsA-fix soak (30 000 frames) — the machine now CYCLES

    [CropSlots] total=64 growing=21 done=42 | histogram 0:1 1:21 2:42
    smStateIdx=1 (state 9), staffed=2, accepting=1
    storage 3422 -> 2023 (still declining; no new resource type appears)

Progress vs before the fix: slots hold real states (21 growing, 42 done) instead of an
instant 64x demotion, the field reached state 10 earlier in the run, and 42 slots have
reached state 2 — which in this machine means harvested/finished, so the harvest half is
running, not just planting.

Two concrete things block completion, both now visible in the histogram:

1. ONE slot sits at state 0 (never planted). State 9's per-frame only advances when
   `growing + done == total`; 21 + 42 = 63 against total 64, so the transition can never
   fire and the field re-enters state 9 forever. Find why that single cell is never
   planted — most likely its PlantArea mark was cleared without a successful plant, or
   it is unreachable. A one-cell shortfall permanently stalling the machine is worth a
   guard-free fix, not a >= comparison.

2. No new resource type appears in storage and the totals only fall, so even the 42
   finished cells are not depositing. That is the same remaining question as fishing:
   the produce path from a finished work into town storage.

⚠ Do not "fix" (1) by relaxing the transition to `>=`. Retail uses equality and the
defect is the unplanted cell.

### Harvest NEVER RUNS — and slot state 2 does NOT mean "harvested"

Counted with `grep -B1 "already hit"` so each count is paired with its own breakpoint
(see the gdb-misread warning above):

    CreateRawMaterial_AbstractImpl   NOT HIT
    HarvestArea::Slot4 (Execute)     NOT HIT
    HarvestArea::Slot6               NOT HIT

So the crop field never reaches state 11, no HarvestArea work is ever executed, and no
raw material is ever created. That is why nothing deposits — there is nothing to deposit.

⚠ CORRECTION to the earlier soak entry: it read `done=42` as "42 cells harvested". That
was wrong. Slot state 2 is what `ValidateEntityRefs` writes when it DROPS a stale slot;
it is not a harvest marker. The 42 were slots going stale again over the long run, not
produce. Do not treat state 2 as progress.

So the real crop-field sequence is: plant all 64 -> hold as growing -> slots gradually
demote to 2 as their entities fail validation -> `growing + done` never equals total ->
state 9/10 cycle forever -> state 11 unreachable.

⭐NEXT: state 10's per-frame is the only path to state 11 (event 4 / the harvest
trigger). Instrument `CropField_State10PerFrame`: log `CF_Maturity()` against the 0.99
ripeness constant and the weather/season branch result. If maturity never climbs, the
growth tick (slot 15 msg 1) is not advancing the crops; if it does climb but the branch
never fires, the season window is the blocker.

### Current crop-field blocker: planting stalls at ~37/64, so state 10 is never entered

Latest run (IsA fix in place, harvest-arm trace added):

    [CropSlots] total=64 growing=37 done=0 | histogram 0:27 1:37 2:0
    [CropHarvest] — never printed

Two things this pins down:
- `done=0` across the whole run: the IsA fix HOLDS. Slots no longer demote. The earlier
  "42 done" was the pre-fix stale-demotion, as corrected above.
- 27 cells sit at state 0, never planted. State 9 advances only on
  `growing + done == total`; 37 + 0 of 64 never satisfies it, so state 10 is never
  entered — which is why the CropHarvest trace (inside State10PerFrame) never prints and
  why HarvestArea never runs.

So the crop-field chain is now blocked at ONE point: PlantArea stops planting after
~37 of 64 cells. Its own byte array at +0x58 is the gate (`validityArray[i] == 0` ->
skip), and each accepted plant clears the mark for its cell and its pair. A plant that
is accepted but never completes therefore consumes a mark permanently.

⭐NEXT: in `PlantArea_Slot4_Impl`, log the surviving mark count each pass alongside
ACCEPTED. If marks fall to zero while ~27 cells are still state 0, accepted-but-failed
plants are eating them, and the fix is to restore the mark on the failure path (retail
does this through slot 7 / OnCancel — check whether our failure path routes there at
all, since OnCancel was measured NOT HIT).

### Final measured state of the crop-field chain

Census threshold lowered to every 10 Execute calls so it actually reports:

    [PlantArea] after 40 Execute calls: maxWorkers=10 noMark=812 alreadyInBst=0
                noPassableCell=0 noPath=0 ACCEPTED=29 (slotCount=64 treeCount=0 cap=2)
    [CropSlots] total=64 growing=60 done=0 | histogram 0:4 1:60 2:0

Reading, and it is unambiguous:
- **Pathfinding rejects NOTHING** (noPassableCell=0, noPath=0). The leftover cells are
  not unreachable — an earlier guess that they were is wrong.
- `noMark` dominates: the leftover cells have had their marks CLEARED but were never
  planted. So marks ARE being consumed without a plant landing — the mechanism first
  guessed several rounds ago, but for the right reason only now that pathing is excluded.
- ACCEPTED=29 with ~60 cells planted confirms accepts are PAIRED (each clears two marks:
  its own cell and `i+1`).

⭐So the defect is the PAIRED path: an accept clears the mark for BOTH `i` and
`pairedIdx`, but if the action only plants one of the pair the other cell is lost
permanently — it can never be re-offered, and the field ends 2–4 cells short forever.
Retail clears both marks too, so retail must plant both; check whether our
`ShowTool_Action_Ctor3` call actually produces TWO CreateNaturalResource actions when
`harvestMode == 2`, or only one. Compare the action chain built for harvestMode 1 vs 2.

That single question is the last link in the crop-field chain: fix it and
`growing + done == total` becomes satisfiable, state 10 is entered, the harvest arms get
evaluated, HarvestArea runs, and the deposit path finally gets exercised.

### Next steps, in order
1. Trace the PerformWork action to the deposit — the one missing link for all three.
2. Crop Field: still entirely unported (`CropFieldComponent` 0x68, ControllerComponent,
   ctor `0x14011afd0`, factory `0x140032ed0`); mirror the Orchard state machine.
3. Port the state-2 -> 13 transition action `0x14012af90` (work-position map stamping),
   currently registered null.
4. Pasture gap closure, then the six dialogs.

## Current port status (start of wave)

- Orchard: COMPLETE and default-on (`BANISHED_LIVE_ORCHARD/GROWTH/FRUIT`).
- Pasture: substantially ported, `BANISHED_LIVE_PASTURE`/`_HERD` opt-in.
- Crop Field / Fishing / Hunt / Gatherer: **no env flags exist at all**. Fishing and
  Hunt have only `Shutdown` ported plus a `COMPONENT_INERT_FACTORY` — i.e. the
  components are created inert and never run.

## Crop field — corrected state (session 2026-08-11, commit 3b9a635)

The paired-plant theory is REFUTED. Measured, not inferred:

| claim | measurement |
|---|---|
| state-9 gate faithful | retail 0x14011a1e0 counts slot+0x04 over comp+0x60, transitions on `done==total` (->21) / `growing+done==total` (->10). Identical to ours. |
| all 64 cells claimed | fruitless-scan census never fires; every PlantArea mark ends 0 |
| pair binds are distinct | 61 msg-0 arrivals, **0 duplicate slot indices** |
| PlantArea_Ctor faithful | retail also fills the work validity array all-1, does NOT copy slot+0x18 |
| passability not systematic | 9 cells stalled in one run, 0 in the next two — placement-dependent |

Remaining: **3-4 of 64 claims never deliver a msg 0.** Claimed, both marks cleared,
both BST entries inserted, action chain built, no report. Accept-side bookkeeping is
byte-faithful and WAH_Append delegates to retail's own 0x1400c54e0.

### ⭐Next lead: the CANCEL path may never run

A dropped action leaves its BST entry and its cleared mark in place forever. Retail has
a recovery route for exactly this — PlantArea Slot7 (cancel) restores marks and drops
BST entries, and `ValidateTask` (0x1400f7100) is what detects a stale claim. In our run
the marks are never restored, so **either the Validate slot is never dispatched or our
Slot7 never fires**. If retail loses claims at the same ~5% rate and simply recovers
them, then the missing recovery — not the loss — is the divergence, and the field would
complete on the second pass. Check whether anything calls our PlantArea Validate slot
before hunting the loss itself.

Trap recorded: `noMark` counts every ALREADY-CLEARED cell each scan, so it is large on
a HEALTHY field. Reading it as loss produced the wrong theory.

### Sharper still: it is ONE hung claim, not a 5% loss rate

New [PlantSlots] census counts the three work slots. Measured on a 62/64 field:

    OnComplete=30  OnCancel=0  Validate=30      (~31 accepts)

So claims are NOT dropped -- 30 of 31 completed normally, and nothing was ever
cancelled. Exactly ONE claim is outstanding, and a claim covers a PAIR of cells,
which is exactly the 2 slots left at state 0. The field is not losing work at a
rate; it is hung on a single claim that never completes and never cancels.

That also retires the "3-4 lost per field" framing from earlier in this file: the
count varied (2, 3, 4, 9) because runs were cut at different points and because one
run had genuine passability rejections on top. The steady-state figure is ONE.

Next: find why the last claim never reports. OnCancel is never called at all, so the
recovery path is not merely ineffective -- it is never entered. Worth checking whether
the citizen holding it is still alive and still has the action chain, and whether
Validate (which DOES run, 30 times) is expected to reclaim a stale claim and is
returning "still valid" for a claim whose citizen has moved on.

## Crop field: the chain now runs to HarvestArea (2026-08-11, later)

Two fixes, in order, each unblocking the next stage:

1. **Harness siting** (`a42b433`) — ringPlace checked occupancy but not workability, so
   a demo field straddled a cell no citizen could work and `growing + done == total`
   was permanently unsatisfiable. Blocked cell terrain 0x07 vs 0x04 on its working
   neighbour. Now every footprint cell is tested with PlantArea's own 0xe0f predicate.
   -> planting completes 64/64 and state 9 -> 10 fires.

2. **Ripeness constant** (`cbe5ab0`) — `0x140172144` (0.99f) was never registered, so
   `s_fRipe` was null and state 10's per-frame returned on the constants guard, BEFORE
   its own census could print. Both harvest arms were dead from ONE missing .rdata
   float. Siblings 0x1401721d4 (33.0f) and 0x14017225c (3600.0f) were registered
   elsewhere, which made it look behavioural rather than structural.
   -> event 4 -> state 11 -> HarvestArea posted -> HarvestArea::Execute runs.

Measured end state:

    [CropXition] state 9 -> 10 TEND (growing=64 done=0 total=64)
    [CropXition] STATE 10 ENTER - posting TendArea
    [CropXition] event 4 -> STATE 11 HARVEST
    [CropXition] STATE 11 ENTER - posting HarvestArea
    [HarvestArea] Execute call 1
    [CropHarvest] temp=72.0 (min 33.0) maturity=0.6971 -> ripe

### Still unproven: the deposit

`BANISHED_PRODUCE_TRACE=1` counts CreateRawMaterial executions ([ORGANIC-PRODUCE]).
On the run that reached state 11 the trace was not enabled; on the run with the trace
enabled maturity had not yet reached ripe inside the window. **Get both in one run** --
that single observation decides whether the crop field deposits.

⚠Run-to-run variance is large: placement, weather and maturity timing all move, so a
600s window sometimes reaches state 11 and sometimes does not. Never rank two fixes on
one run each ([[reference-banished-seed-pins-worldgen-only]]).

⚠Do NOT read slot state 2 as "harvested" when judging this -- it is also what
ValidateEntityRefs writes when it DROPS a stale slot. One run showed done=51 while
still in state 9. Only a CreateRawMaterial in storage settles it.
