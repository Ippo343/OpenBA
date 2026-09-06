# Achievements wave — state, method, and what is left

**Branch** `wave/upgrade-path-complete` · commits `be610ee`..`f6d3d98` (13)
**Status** 24 of 36 achievements unlock and persist. 12 remain, each with a named
blocker below. Highwaymen and Jack of all Trades both closed 2026-09-05
(`Ach25 : true`, `Ach5 : true`).

Before this wave the achievement system was **structurally dead**: no achievement had
ever been evaluated, in any run, ever. It is now live end to end — retail's
`AchievementManager::Update` evaluates our ported `Goal::Check` overrides every tick,
awards are recorded, and they persist as `HKCU\...\Banished\Ach<N>` exactly as retail
writes them.

---

## 1. The gate that hides everything

`AchievementManager::Update`'s first act (Runtime `0x18009e6c6`) is

```
if (*(int*)(IO::FileSystem::_singleton + 0x38) > 0) return;
```

That field is the **user-mod count** — `AreUserPackFilesEnabled()` is literally
`count > 0`. Retail disables achievements whenever any `.pkm` sits in `WinData`. This is
faithful behaviour, not a defect, and it is a **silent, total** disable: every goal stops
being evaluated and nothing in the game says so.

**Any achievement run needs the mods moved aside.** This machine's two
(`betterFields.pkm`, `DebugMenu.pkm`) are currently in `WinData/_mods_off/`. The census
prints the gate on every block; a non-zero value makes every other number in it
meaningless as evidence.

---

## 2. Defects fixed (all committed)

1. **`AchievementManager::Reset()` was never called** (`be610ee`). It is not a "clear the
   awards" call — it is the *instance builder* (Runtime `0x18009e5c0`: resize `mgr+0x18`
   to the achievement count, then `Achievement::CreateInstance` per entry). Retail calls
   it in `GameUpdateLoop::Load` at `0x14009757d`, the else-arm of the same
   `if (self+0x58)` branch worldgen sits in; we had ported the worldgen arm and not this
   one. With `mgr+0x18` null, `Update` returned on its `count <= 0` guard every tick. The
   dialog looked healthy throughout because it reads the *achievement* list (`mgr+0x00`),
   never the instance list.

2. **Goal vtable slots 4/7/9 were `nullptr`** on StatGoal / PopulationGoal /
   BuildingGoal. Invisible while only tutorial goals ran (those are TimeGoal /
   FinishTutorialGoal, which override slot 9). The moment Reset built instances,
   `GoalList::Initialize` asked a StatGoal for its instance type through slot 9 and
   executed address 0.

3. **`StatValueDispatch` case 0xe missing a dereference** — `World() + 0xf0` where retail
   has `[gameWorld + 0xf0]`. It read its count from an unrelated field and walked off the
   heap. Every other helper in that file already used `WorldPtr(0xf0)`.

4. **Death harness capped at 64 kills** by a fixed stack array, invisible except as the
   `/64` in its per-kill log line.

---

## 3. Harness added

| Knob | Purpose |
|---|---|
| `BANISHED_ACH_TRACE=<frames>` | Per-achievement census: awarded / met / failed, and per goal its class, requirement and **live value** (`stat=5 have=802 >= 900`). `_DUMP=1` adds struct dumps. |
| `tools/ach_reset.sh` | Clear persisted `Ach<N>` values. `Reset()` does **not** clear them and the manager skips awarded achievements, so without this a re-test inherits earlier awards and looks like a pass. |
| `BANISHED_ACH_TIME_SCALE` / `_YEAR_BOOST` / `_YEAR_MAX` | Non-retail clock. Scales the one `Time` every `TimeGoalInstance` accumulates (goals are *accelerated*, not bypassed — the `AreGoalsMetExcept` reset still wipes lapsed progress), and moves the year counter stat 26 reads. Neither touches the simulation. |
| `BANISHED_STRESS_EXCLUDE` / `_EXTRA` / `_STOCK` | Drop or multiply stress-set buildings by tag; set the seven start-stock counts to absolute values. |
| `BANISHED_DEMO_TRADINGPOST_COUNT` | Place N trading posts (the stress set excludes every river-sited building). |
| `BANISHED_TRADE_SELL` / `_SEED` / `_PERROW` | The virtual merchant customer — opens the panel, fills sell rows, runs the ported checkout. |
| `BANISHED_FINISH_BUILDS` | Drive unfinished structures through state 1 → 8 → 2. |
| `BANISHED_BRIDGE_LONGEST`, `BANISHED_ROAD_RUNS`/`_LEN`, `BANISHED_FENCED_RUNS`/`_SIDE` | Longest-qualifying bridge span; N road spans; N fenced areas. |
| `BANISHED_PASTURE_AUTOTYPE=Cow,Sheep,Chicken` | Round-robin livestock types (a single name gave every pasture the same animal). |

**Stat id map** (read off live census data): 0 educated adults · 3/4 happiness/health ·
5 population · 6 adults · 9 citizens in profession X · 12 graves · 13 units traded
(`townMgr+0x1ac`) · 14 stored quantity · 15/16/17 livestock/crop/orchard seed types owned ·
18 animals of type · 19 plants harvested this year · 20 cumulative production · 21 annual
production by building type · 22 workers at building type · 23 paved road tiles ·
24/25 count of building type · 26 years elapsed · 27 map size · 28 nomads admitted ·
29 bridge length.

---

## 4. Unlocked (24)

`Ach{0,1,2,4,5,6,10,11,12,13,15,17,19,20,25,26,28,29,30,31,32,33,34,35}` — Settlement,
Village, Town, Uneducated, Mountain Men, Isolationist, One with Nature, Trader,
MasterTrader, Firefighter, Livestock, Miner, Mason, Highwaymen, Golden Gate, Smiles all Around,
Healthy, Built from Stone, Ready for Anything, Builder, MasterBuilder, Established,
Tenure, Jack of all Trades.

Recipes worth keeping:

* **Abstinence trio** (Uneducated, Isolationist, One with Nature) — these report
  `failed=1` in a normal stress town and are unreachable for the life of that world.
  `BANISHED_STRESS_EXCLUDE=schoolhouse,cropfield,orchard` plus the matching
  `BUILD_DEMO_*` kill switches unlocks all three in one run.
* **Ready for Anything** — storage capacity is shared, so a huge food figure starves the
  other six goals. Balanced: `STOCK=4000,4000,4000,4000,40000,4000,4000` plus
  `EXTRA=storagebarn:24,storageyard:24`.
* **Trader / MasterTrader** — `TRADE_SELL=600 _SEED=800 _PERROW=700` gave credit
  0 → 117,600 over four visits.
* **Mountain Men** — nearly free: `BANISHED_MAP_SIZE=small BANISHED_CLIMATE=harsh` plus
  the time scale. The default stress terrain is already Mountains.

---

## 5. The 12 that remain, and what each needs

### 5.1 Tombstone — 400 graves *(closest to done)*

**Burials now work for the first time in this port.** `Cemetery.rsc` declares
`createfenced`: a cemetery is a **fenced area like a pasture**, and the FencedTool is
what creates its gate as a MultiObject child — which `CemeteryComponent::BuildPlotGrid`
needs before it can lay out plots. Every cemetery had been dropped by the stress placer
via plain `placeStructure`, so `multi+0x48` was null, the plot grid bailed at "no child
array", and *every death since this port began* logged `NO GRAVE`.

> Correction to an earlier diagnosis in this wave: the MultiObject child spawner is
> **not** unported. It exists (`FencedTool::CalcPath`); it was never being asked.

Placing through the FencedTool (`BANISHED_PLACEMENT_TEMPLATE='Template\Cemetery.rsc'`)
gives `plots=12` per 8×8 area and real burials.

**What is left: siting.** `Cemetery_FindNearestWithSlot_9740` requires
`ValidateCell(0xe1f)` region reachability *from the death cell*, and deaths happen in the
town. Measured, all at 900 kills:

| scan | areas placed | graves |
|---|---|---|
| whole map | 25 | 53 (≈4 cemeteries' worth reachable) |
| band on **map** centre | 2 | 2 |
| band on **town** centre | 0 | 0 |

The town centre resolves correctly (col 229, row 270 from 326 founding houses) and is
now computed the way the bridge harness does — the right basis. **Next step: scan a RING
just outside the built footprint rather than a band through it** — clear ground, still in
the town's region. ~35 working cemeteries × 12 plots clears 400. Sides 14 and 16 place
nothing (the scan needs a fully clear block and 950 houses leave few), so raising plots
per area means finding room first, not just asking for a bigger side.

### 5.2 Exports, Farmer, Food Variety, Foodie — the material pool

All four need ale, warm coats, or extra seed types, and the town's raw-material pool
(`townMgr+0x110`) only gains a material once the town has **produced** it. Nine thousand
frames with three taverns and three tailors staffed registered neither ale nor coats.
Exports' steel-tools goal already passes (700/300) via the storage seed; the other two
read 0 because there is nothing to seed.

The round-robin crop/orchard fix is in and works, but has nothing to spread: the town
owns exactly **2** crop seed types and 2 orchard types, and more come only from trade.

**Next step:** get the tavern/tailor production chains running end to end, or drive the
**buy** side of the trade panel (the sell side is proven — see §6).

### 5.3 Highwaymen — 2,000 paved road tiles — **CLOSED 2026-09-05**

`Ach25 : true`. Measured at the award: `stat=23 have=3521 >= 2000`, from 3,521 paved
tiles laid by hand in roughly eight minutes of play.

> Correction to the earlier diagnosis: "road tiles carry no BuildComponent so they are
> invisible to whatever marks a road paved" was the wrong model. Nothing "marks" a road.
> `stat 23` (StatValueDispatch case 0x17) reads one int out of the road manager's
> per-road-type counter array at `gw+0x110` `+0x220`, recomputed from the map by the
> road-mesh rebuild (`Pf_RoadMeshRebuild_f370`, 0x1400af370):
>
>     batchId = cell+0x2a; blendId = cell+0x2b;
>     if (batchId != 0xff && blendId == 0xff) counter[batchId]++;
>
> RoadTool's commit writes `+0x2a = roadType` and `+0x2b = 0x80` (queued); only
> `CompleteRoad` overwrites `+0x2b`. A drawn road is *by design* not counted, so 3,000
> tiles with stat 23 at 0 meant 3,000 tiles nobody had built.

**Defect 1 — nobody in town could do the work.** The scheduler files each work item into
a per-PROFESSION bucket chosen by the work's vtable slot +0x50, and a citizen's idle
search (`0x1400f7830`) only reads the bucket keyed by its own profession
(`citizen+0x1f0`). BuildRoad's slot 10 returns the **Builder** key, and a finished town
has no construction site, so Builder reads `0 of 0`. Measured: 620 tiles queued, 620
items in the (BUILDER) bucket, 0 taken, ever. Retail seats builders from the town-hall
Professions spinner, which loops `CitizenList::AddWorker` (0x140091130) — and that call
deliberately tolerates a NULL "available building": `c5f0`/`c4b0` only STORE the pointer,
so the profession flips with no roster attach. That is what makes a builder possible in a
town whose only pending work is a road. Nothing called it. New harness
**`BANISHED_BUILDERS=<n>[,<settleFrame>]`** (`standalone/builder_seat_harness.cpp`).
Also closes §5.6's builder-profession goal.

**Defect 2 — the roads were sited where no builder could walk.** The span scan started at
a fixed row 124 regardless of the town. Every span committed fine ("20/20 placed"), but
`BuildRoad::Execute` opens with `Map::CheckPassable(0xe1f, citizenCell, targetCell)` and
**262,832 of 263,649** calls bailed there. It now sweeps outward from the founding-house
centroid like the bridge harness. Same trap §5.1's cemetery siting hit.

**Defect 3 — the save crashed once roads existed** (fixed in `09fa91c`, see that commit).
`CompleteRoad::Serialize` never lazy-resolved (its action is built by the copy ctor, which
bypasses the factory), and its `Serialize(unsigned char&)` export name was misspelled
`AEAuchar` instead of `AEAE` so the pointer was permanently null. Four sibling
exe-ctor-built actions had the same missing lazy-resolve. Behind them, a pre-existing and
road-independent one: `MarketWork` read `Serializer::GetVersion` from slot `0x140151510`,
which is `UI::TableElement::_typeInfo`, a data export.

**Correction to the earlier "throughput" verdict.** This wave previously recorded road
building as throughput-blocked (150 builders, 8,000 frames, 8 tiles). That was the
harness's siting, not the game: hand-played, with roads drawn where people already walk,
3,500 tiles get built in minutes. Two of the three blockers listed there were artefacts
of the measurement. The one that is real: only ONE decal batch is registered
(`mgr+0x218 = 1`), so road type 0 (stone) has a slot and dirt (type 1) increments past
the end of a length-1 counter array — find the per-type callers of
`RoadTool_DecalStore_acd90` (0x1400acd90). Stone = type 0, dirt = type 1; only stone
counts toward this achievement.

### 5.4 Lumberjack, Stonework, Smelter — cumulative production

`stat 20` sums a production **history table** over a 100-year window. The clock knobs
move goals and the year counter; they do not fabricate production records. Needs either
genuine sustained production or an understanding of where that table lives.

### 5.5 Educated, Blacksmith, Stylish — ageing and carried goods

`stat 0` counts adults whose education (`citizen+0x210`) meets the threshold — 0 with 538
adults is *correct* for a town where nobody has been to school. Needs real children
growing up through a school; the achievement clock deliberately does not age citizens.
Blacksmith and Stylish need every adult *carrying* a steel tool / warm coat (stat 1/2),
which is distribution, not stock.

### 5.6 Jack of all Trades — **CLOSED 2026-09-05**

`Ach5 : true`, awarded natively with the mod gate at 0 (no `BANISHED_ACH_IGNORE_MODS`).
Met between f=600 and f=1200 of a 50x stress run; all 21 goals green:

    BANISHED_BUILDERS=40 BANISHED_ACH_TRACE=600 BANISHED_ACH_TIME_SCALE=50 \
    BANISHED_PASTURE_AUTOTYPE=Cow,Sheep,Chicken \
    tools/stress_run.sh --label jack1 --speed 50 --frames 3000

Twenty of the 21 goals pass on **real measured values** — one worker at each of 18
building types plus a laborer (`have=190`) and a builder (`have=40`). Only `goal20`
(a 5-year TimeGoal) is carried by `BANISHED_ACH_TIME_SCALE`, the declared clock
deviation in §7. The pasture goal needed nothing special: `stat=22 Pasture have=4`
came straight from the stress town's own seat cascade.

Both blockers the earlier draft named were already fixed, and neither needed further
work — the wave had simply never re-run the census with `BANISHED_BUILDERS` set. The
control run confirms the causality rather than assuming it: with the flag removed and
everything else identical, `stat=9 builder have=0` and the achievement reports `met=0`.

> **A "should follow from" is not a measurement.** This achievement sat on the open
> list carrying two blockers, one marked *solved* and one marked *should follow from
> staffing*. Both were true. The cost of confirming it was a single run — cheaper than
> the paragraph that deferred it.

**Found while closing it — a live crash the builder seat triggers.** The 40-builder run
died at ~f=2500 (the control at the same settings ran 3000 frames clean, 0-byte crash
log), in the citizen idle work search:

    Helper_7830 (idle work search, 0x1400f7830)
      -> HarvestArea_Slot9_Impl        harvestarea.cpp:855
      -> WorkBase_ValidateTask_f7100   work_base_reimpl.cpp:336  (the fn_IsA(obj, ciTid) gate)
      -> Runtime +0x491CB              READ of 0000079700000562

`RAX=0000079700000552` — a correct-looking low dword with a garbage high dword, i.e. a
`{u32,u32}` handle dereferenced as a pointer. Same shape as the load-path wild write in
§8.1 and the stale handle-table family in
`reference_internalfactory_swapremove_wild_write`. The likely mechanism: the seat
harness converts *existing* workers to builders through `CitizenList::AddWorker`, and a
citizen converted while it held a HarvestArea task leaves a stale entry behind that the
next idle search validates. Retail's Professions spinner performs the same conversion,
so either our `AddWorker` port skips a de-registration retail does, or the area's roster
needs the detach. Not achievement-specific, and it does not affect the award above
(which lands ~1300 frames earlier).

### 5.7 Immigrants — 200 nomads

`stat 28` (nomads admitted) is 0; nomad arrival events have not been exercised at all.

---

## 6. Method notes worth reusing

* **A merchant does arrive.** `[TradeCtrl] event 9: MERCHANT DOCKED` fires on its
  ~6,583 s timer (~12,000 frames at 50x) with a dock-reachable post. Arrival works;
  purchasing is what nobody does.
* **`fn_FindComponent_Building` is misnamed** — `DAT_1401c4b80` is the *TradeComponent*.
  The sell table is built from the post's **trade stock** tree, not its
  `StorageComponent`. Seeding the wrong one produced a successful-looking `AddItem` every
  tick and `sellRows` stuck at 0.
* **Panel tables are built once, at open.** Seeding afterwards needs an explicit
  `TradeUI_RefreshSellTable` or the rows never change.
* **A sell row's `+0x10` is a component pointer, not a stock count.** Reading it as one
  skipped every row whose resource lacks a `RawMaterialDescription`.
* **`EditTool_SelectEntity_Impl` faults on a bare entity pointer** — use
  `EditTool_HarnessSelectEntity`, which builds the InternalBasePtr it expects.
* **Stamping `pending = 2` finishes a placer-spawned structure but not a tool-created
  one.** A bridge segment is genuinely mid-construction with no 8→2 edge to take on
  demand; drive state 1 → request 8, state 8 → saturate progress, and let `State8Update`
  finish it.
* At 50x, **one in-game year ≈ 4,500 frames**. Tenure at 200 years would be ~900,000
  frames — hours per attempt, which is why the clock knobs exist.
* A run whose deploy fails (`cannot create regular file ... Device or resource busy`)
  silently measures the OLD exe. Kill stray `reimpl-x64.exe` first.

---

## 7. Known deviations (test fixtures, not retail)

* `BANISHED_ACH_TIME_SCALE` / `_YEAR_BOOST` — accelerate goal time and the year counter
  without simulating. They prove the goal wiring fires, **not** that a town could have
  survived 200 years to get there.
* `BANISHED_FINISH_BUILDS` — stands in for builders delivering material. Not the evidence
  that the build path works; the short bridge finishing on its own is that.
* `TradeStock_SeedForTest` / `BANISHED_TRADE_SELL_SEED` — stands in for haulers stocking
  the post.
* `BANISHED_ACH_IGNORE_MODS` — masks retail's user-pack gate (§1) for the duration of
  the `AchievementManager::Update` call only, then restores it. It lets the goal wiring be
  exercised in a modded session. It does **not** show that a retail player could earn the
  achievement: retail would have evaluated nothing at all. The award it produces writes a
  real `Ach<N>` registry value indistinguishable from a legitimate unlock, so record which
  runs used it. Highwaymen's `Ach25` was awarded under this flag; the same town without the
  mod would award it natively.
* The trade harness calls `TradeUI_OnTradeButton` **directly**, bypassing retail's button
  gate (`displayBuyTotal > 0 && !bStorageFull && !bOverBought`). A player could not commit
  a pure sell with no buy. The checkout body and the credit arithmetic are retail's;
  pressing the button is not.

---

## 8. Open, found while closing Highwaymen (both on the LOAD path)

Neither is achievement-specific; both block loading a large hand-built town.
`Save9.sav` (+ `Save8.sav.roads3500_0905`) reproduce them on demand.

1. **Wild write / heap corruption on load.** The allocator dies walking a corrupted free
   list. The redzone's "OVERFLOWER" block differs in size run to run (1056 bytes once,
   40 twice), so it names the neighbour by address and is **not** evidence of a specific
   overrunning buffer — this is a wild write, not an overflow. The signature is sharp:
   the faulting address is a VALID heap pointer whose high dword is the true value plus
   exactly **0x7FC**, three times over, across two different saves and different heap
   bases:

   | run | real block | fault address | delta |
   |---|---|---|---|
   | Save8 #1 | `0000022b_d613d078` | `00000a27_d613d308` | +0x7FC |
   | Save8 #2 | `000001bd_12f40250` | `000009b9_12f40908` | +0x7FC |
   | Save9    | `000001b1_23fd4e78` | `000009ad_23fd5530` | +0x7FC |

   A correct low half with a systematically wrong high half is what a `{u32,u32}` handle
   dereferenced as a pointer looks like — close to the stale handle-table family in
   `reference_internalfactory_swapremove_wild_write`. Next step: `BANISHED_REDZONE=watch`
   plus `standalone/hw_watch.cpp` to catch the writer live.

2. **Achievement instances are not restored on load.** After loading a save, the manager
   is a valid pointer with `mgr+0x18` null. `GetAchievementCount` faults `READ of 0x28`
   on it and `AchievementManager::Update` faults `READ of 0` just past the mod gate — the
   gate had been hiding the second one. `Reset()` builds the list at new-game and
   `Persist` is meant to restore it on load; on these saves neither has. Independent of
   #1 and likely the cheaper fix. No achievement can be evaluated in a loaded game until
   this is fixed.

---

## 9. The work-area BST invariant — one broken invariant, two symptoms (2026-09-05)

Chasing the `ValidateTask` crash from §5.6 turned up a second, louder symptom of what
looks like the same defect. Neither is achievement-specific; both live in the
harvest-area work registration.

### 9.1 The shared invariant

`HarvestArea` keeps a BST at `self+0x48` (count at `self+0x50`) whose nodes are
`{key(int) +0x00, citizen IBP +0x08, parent +0x10, left +0x18, right +0x20}` — 40 bytes.
Retail's node layout was read off `0x1400ec930` and matches our port exactly.

Two retail functions assume **every entry's citizen still holds the matching task**:

* `HarvestArea::Slot9` "Validate" (`0x1400ec930`) walks the tree calling
  `FUN_1400f7100` (ValidateTask) per entry and removes the invalid ones.
* `HarvestArea::Slot8` "Cleanup/CancelAll" (`0x1400ed270`) loops
  `while (count > 0)`, takes the leftmost node, and cancels that citizen's action list —
  relying on the cancel to reach `Slot7` (`0x1400ecb60`, keyed by CELL) and drop the entry.

`ActCt_TaskCleanup` calls slot 9 and then slot 8 on the same area, back to back.

### 9.2 Symptom A — the teardown hang (reproducible enough to root-cause)

Quitting a stress town wedges the process at 100% CPU with no further log output, right
after the `[Residence] ERASE via SLOT-7 removal hook` burst. Measured: **5.03 CPU-seconds
in 5 wall seconds** — one core spinning, not a deadlock. `standalone_boot_state.txt` still
says RUNNING; the tell is that the log stops growing.

gdb on the spinning thread:

    EntityList_Shutdown            gameupdateloop_teardown.cpp:249  (the mark loop)
     -> MarkEntityRemoval          action_base_hunt.cpp:193
     -> WorkComponent_RegisterWorkSlot   workcomponent.cpp:310
     -> ActCt_TaskCleanup(taskIdx) action_base_complete.cpp:298     (owner vtable +0x40)
     -> HarvestArea_Slot8_Impl     harvestarea.cpp:807
     -> WorkBase_TaskListCleanup_c5400

Sampling the same wedge four times over 12 s — **every value frozen**:

| probe | value |
|---|---|
| HarvestArea `self` | `0x204419342d8` (same each sample) |
| `self+0x50` (loop counter) | **1**, never drops |
| BST root | `0x20441a30ed8` (same) |
| root `+0x18` (left) | `0` — the root *is* the leftmost, so the same node is re-picked forever |
| root `+0x08` (citizen) | `0x2043f68b250` (same) |
| citizen action list count | 16, **all 16 slots NULL** |

So slot 8 cancels an empty action list, nothing reaches slot 7, the count stays at 1, and
the identical node is chosen again — forever. **Retail has this exact loop**, so the fault
is the stale entry, not the loop.

### 9.3 Symptom B — the `IsA` crash

`Object::IsA` (Runtime `0x1800491c0`) is three instructions:

    MOV RAX,qword ptr [RCX]      ; the object's VTABLE
    MOV EBX,EDX
    CALL qword ptr [RAX + 0x10]  ; vtable slot 2 — GetTypeId

It **CALLs** through the object, so a bad entry is an EXEC-of-garbage, reported as
`READ of <vtable+0x10>`. Seen twice, and the two are the same structure:

| run | RAX ("vtable") |
|---|---|
| jack1 | `0000079700000552` |
| stall run | `0000077B00000552` |

The `BANISHED_WORKVALIDATE_TRACE` probe dumped the object behind the second one:

    +0x00: 52 05 00 00  7b 07 00 00     0x552, 0x77b   <- read as the vtable
    +0x08: 56 05 00 00  00 00 00 00     0x556, 0
    +0x10: ff ff ff fe  00 00 00 00     0xFEFFFFFF sentinel
    +0x18: 68 46 0e 00  24 01 00 00     a real heap pointer (0x124_000E4668)
    +0x20: 6f 00 00 00  7b 07 00 00
    +0x28: 52 05 00 00  97 07 00 00     0x552, 0x797   <- the pattern repeats, stride 0x28

**A regular 40-byte record array with a sentinel — not freed garbage.** And `0x797`, the
high dword of the *first* crash's vtable, sits at `+0x2c` of *this* dump: both crashes read
the same array one element apart. The low dword is `0x00000552` in both.

> Correction to the working hypothesis this section started from. "A citizen was freed and
> the entry dangled" is **not** supported. A freed-and-recycled object would differ run to
> run; this is a stable, live, regularly-strided structure, and 40 bytes is exactly the BST
> node stride. The likelier reading is that some entries' `+0x08` never held a citizen —
> a wrong pointer stored, not a lifetime bug.

### 9.4 What was ruled out

* **The builder seat is not the trigger.** It crashed once in the first run and then
  survived five more with the same config; a later crash reproduced on a run whose only
  changed variable was unrelated. The single clean control run quoted in §5.6 never
  established causation and should not have been read that way.
* **Mass death is not the trigger.** `BANISHED_KILL_CITIZENS=400` at frame 1200, no
  builders: no crash, no stall.
* **Our BST walk is not the bug.** Retail `0x1400ec930` and `0x1400ed270` were decompiled
  and match our ports field for field, including the single-qword entry read.

### 9.5 Harness added (all default-off, byte-unchanged when unset)

| Knob | Effect |
|---|---|
| `BANISHED_WORKVALIDATE_TRACE=1` | In `ValidateTask`, before the `IsA` that would EXEC the object: report any entry whose vtable is not on a committed readable page, dumping the object, and the BST node it came from. Uses cached vtable/region checks (a bare `VirtualQuery` per call is a syscall on a hot path and perturbed the race enough to hide it). |
| `BANISHED_WORKAREA_STALL_TRACE=1` | In `HarvestArea::Slot8`, detect "cancel made no progress", log the area/node/citizen/flags/action-list state, and **break instead of spinning**. Turns the teardown hang into one log line and a clean exit. |

### 9.6 Next step

Find what writes a non-citizen pointer into a work-area BST node's `+0x08`. The insert
side is `HarvestArea::Slot4` (Execute, `0x1400eccd0`); identify the record array by its
40-byte layout with the `0xFEFFFFFF` sentinel and the heap pointer at `+0x18` — that names
the structure whose address is being stored. Both symptoms should close together.

### 9.7 Method note

`tools/stress_run.sh --no-build` skips the **deploy**, not just the build. A hand-run
`make standalone-perf` followed by `--no-build` measures the *previous* exe: six probe runs
reported "zero hits" because the probe was not in the running binary. Either drop
`--no-build` or `cp standalone/build-perf/reimpl-x64.exe "$GAME_DIR/"` yourself, and check
the deployed timestamp before believing a negative result.
