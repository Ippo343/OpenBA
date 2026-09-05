# Achievements wave — state, method, and what is left

**Branch** `wave/upgrade-path-complete` · commits `be610ee`..`f6d3d98` (13)
**Status** 22 of 36 achievements unlock organically and persist. 14 remain, each with a
named blocker below.

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

## 4. Unlocked (22)

`Ach{0,1,2,4,6,10,11,12,13,15,17,19,20,26,28,29,30,31,32,33,34,35}` — Settlement,
Village, Town, Uneducated, Mountain Men, Isolationist, One with Nature, Trader,
MasterTrader, Firefighter, Livestock, Miner, Mason, Golden Gate, Smiles all Around,
Healthy, Built from Stone, Ready for Anything, Builder, MasterBuilder, Established,
Tenure.

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

## 5. The 14 that remain, and what each needs

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

### 5.3 Highwaymen — 2,000 paved road tiles

**Road tiles now get paved.** `stat 23` moved off 0 for the first time in this port
(measured: 8). The whole chain — RoadTool commit -> BuildRoad work item -> builder picks
it up -> PerformWork -> CompleteRoad action -> `Pf_FinalizeRoadTile_9c640` -> road-mesh
rebuild recount — runs end to end. The port was not missing a road function; two things
upstream of it were missing, and both are now measured rather than inferred.

> Correction to the earlier diagnosis: "road tiles carry no BuildComponent so they are
> invisible to whatever marks a road paved" was the wrong model. Nothing "marks" a road.
> `stat 23` (StatValueDispatch case 0x17) reads one int out of the road manager's
> per-road-type counter array at `gw+0x110` `+0x220`, and that array is recomputed from
> the map by the road-mesh rebuild (`Pf_RoadMeshRebuild_f370`, 0x1400af370):
>
>     batchId = cell+0x2a; blendId = cell+0x2b;
>     if (batchId != 0xff && blendId == 0xff) counter[batchId]++;
>
> RoadTool's commit writes `+0x2a = roadType` and `+0x2b = 0x80` (queued); only
> `CompleteRoad` overwrites `+0x2b` with 0xff. So a drawn road is *by design* not
> counted, and 3,000 tiles with stat 23 at 0 meant 3,000 tiles nobody had built.

**Defect 1 — nobody in town could do the work.** The scheduler files every work item into
a per-PROFESSION bucket chosen by the work's vtable slot +0x50, and a citizen's idle
search (`0x1400f7830`) only ever looks in the bucket keyed by its own profession
(`citizen+0x1f0`). BuildRoad's slot 10 returns the **Builder** key, and the stress town
has no builder: the Professions panel shows Builder `0 of 0` because its capacity is
summed over registered construction sites and a finished town has none. Measured: 620
tiles queued, 620 items in the (BUILDER) bucket, 0 taken, ever.

Retail seats builders from the town-hall Professions spinner, which loops
`CitizenList::AddWorker` (0x140091130). That call tolerates a NULL "available building"
on purpose — `c5f0`/`c4b0` only STORE the pointer, so the citizen's profession flips with
no roster attach (see the Strat_c5f0 note in `citizen_employ_cascade.cpp`). That is what
makes a builder possible in a town whose only pending work is a road. Nothing in the
project called it. New harness: **`BANISHED_BUILDERS=<n>[,<settleFrame>]`**
(`standalone/builder_seat_harness.cpp`) — the same class of player stand-in as
`panel_staffing_driver.cpp`; the seat is retail's, only the pressing is ours.

**Defect 2 — the roads were sited where no builder could walk.** The span scan started at
a fixed row 124 regardless of the town. Every span was legal ground and committed fine
("20/20 placed"), but `BuildRoad::Execute`'s first act is
`Map::CheckPassable(0xe1f, citizenCell, targetCell)`. Measured on the old scan: **262,832
of 263,649** Execute calls bailed on that guard and not one tile was paved. The scan now
averages the founding houses' MapComponent cells and sweeps outward from the town, the
way the bridge harness does — same trap §5.1's cemetery siting hit. After the change,
`unreachable` fell to 0 on a 20-span run.

Harness: **`BANISHED_ROAD_CENSUS=<frames>`** (`standalone/road_probe.cpp`) prints the
counter array stat 23 reads, a `(+0x2a,+0x2b)` histogram of every road cell, the pending
work BST, the scheduler's profession buckets (Laborer/Builder labelled), the idle
work-search counters, and where in `BuildRoad::Execute` the calls are going. That census
is what turned every step above from a guess into a number.

**What is left — throughput, not correctness.** Best run so far: 150 builders, 40 requested
spans, 8,000 frames at 10x -> only 155 tiles placed and 8 paved. Three named blockers:

1. **Only one decal batch is registered.** The counter array is length 1
   (`mgr+0x218 = 1`), so only road type 0 (stone) has a slot; dirt is type 1 and its
   increment writes past the array. Find the per-road-type callers of
   `RoadTool_DecalStore_acd90` (0x1400acd90) — retail must register one slot per road type.
2. **`structOnCell` dominates.** 59,034 of 635k Execute calls found an entity at
   `cell+0x08` that is neither a RawMaterial nor a NaturalResource, so the branch returns
   1 forever. Those tiles can never be built. Identify what that entity is.
3. **Siting runs out of ground.** The town-centred scan found room for 155 of ~1,240
   requested tiles. 2,000 tiles needs a ring sweep outward from the town (the same fix
   §5.1 still wants), not a denser band through it.

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

### 5.6 Jack of all Trades

Two goals short: a citizen with the **builder** profession (`stat 9`), and a **worker at a
pasture** (`stat 22`). Pastures are now in the stress set, so the second should follow
from staffing; the first is **solved** — `BANISHED_BUILDERS=<n>` (§5.3) seats real
builders through retail's own `CitizenList::AddWorker`, and the profession count was
measured going 0 -> 40. Re-run the census with it set.

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
* The trade harness calls `TradeUI_OnTradeButton` **directly**, bypassing retail's button
  gate (`displayBuyTotal > 0 && !bStorageFull && !bOverBought`). A player could not commit
  a pure sell with no buy. The checkout body and the credit arithmetic are retail's;
  pressing the button is not.
