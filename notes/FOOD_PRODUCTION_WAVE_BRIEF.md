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
