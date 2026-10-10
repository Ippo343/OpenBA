# Worldgen Generation Deviations — CLOSED REGISTER

Status: **CLOSED** (re-closed 2026-08-13). #4 closed with its crash root-caused; #3 and
#7 re-argued and re-measured after the `BANISHED_LIVE_*` graduation voided the
inert-component premise their old rationales rested on. All verdicts held. One latent
non-worldgen bug (eviction leaves a live scene node) is recorded but is not a worldgen
deviation. See "Closure summary" at the bottom.

Original framing (2026-07-01), retained for context: every remaining worldgen deviation
has a decided disposition below. None are bugs: all are consequences of the standalone build's
**inert-component architecture** — a deliberate project policy where ECS components
are instantiated + registered but never made live (no `Startup`, no per-frame
`Update`, no scene remove/re-add), the same gate applied to structures and citizens.
The authoritative, byte-level rationale for each lives **in-code** in
`code/Game/Game/starttown.cpp` (the port of the `FUN_1400a12b0` natural-resource
scatter); this file is the decision summary.

Dispositions:
- **ACCEPTED** — produces output net-identical to retail; not observable in the
  generated world. Permanent; nothing to revisit.
- **DEFERRED→LIVE** — moot in the default inert build (the divergent code path never
  executes). Only relevant if/when the live-component subsystem is built
  (`BANISHED_LIVE_GROWTH`); carries the exact revisit trigger so it is not lost.

(#1 and #2 were closed in earlier passes — RNG-alignment of the scatter stream via
`FUN_1400a12b0` prologue Perlins + `RandFloatTol` jitter/yaw; commits `1c7d7ac`,
`b4efaac`. This register tracks what remained.)

| # | Deviation | Root cause | Disposition | Anchor |
|---|-----------|-----------|-------------|--------|
| 3 | Variant mesh pick (`FUN_1401253d0`) runs **before** registration, so `OnAdded` does a single scene-add instead of the original's vtable+0x38/+0x30 scene **remove + re-add** (`ModelComponent_helper_2b70`). | The remove/re-add slots need a live `ModelComponent`; inert components never dispatch them. Running the variant pick pre-registration reaches the **same final scene contents** (correct growth-stage mesh present, exactly once). | **ACCEPTED** — net-identical scene state. ⚠ The "inert components never dispatch" wording in this row is RETIRED — see "#3 — re-dispositioned" below for the rationale that actually holds. | `ApplyResourceModelVariant`, starttown.cpp:1257–1283 |
| 4 | `ComponentEntity_impl_5cc0` (SpawnComponent initial sub-resource spawn) skipped entirely. | Recurses via `impl_5780` and reads `GrowthComponent+0x28`, valid only when `GrowthComponent` is live. Skipping omits recursively-spawned sub-resources — a live-only content path, consistent with the whole resource block being guarded off for inert components. | **DEFERRED→LIVE** — revisit under `BANISHED_LIVE_GROWTH` once `GrowthComponent::Startup` populates `+0x28`. | starttown.cpp:1289–1292 |
| 5 | Growth init (`FUN_140120db0`) is a no-op, and its `RandFloat` draw is latent-reordered vs the variant `Rand`. | Trees carry no `GrowthComponent` in the standalone factory (`FindComponent` returns null on every tree), so **neither** the original nor the port draws here — the reorder is unobservable today. It would only misalign the world RNG under a live build. | **DEFERRED→LIVE** — under `BANISHED_LIVE_GROWTH`, restore the `120db0` `RandFloat` draw to its original position in the scatter stream (before the variant `Rand`) to keep the seed byte-faithful. | `StartTown_SetGrowthProgress`/`ActCNR_SetGrowthProgress_120db0`, starttown.cpp:1563–1596 |
| 6 | Height/slope sampled via `fn_MapGetTerrainPos` instead of the `QuarryComponent_helper_aa20` picking ray. | `aa20` is null in standalone. `a12b0`'s picking ray is **vertical**, so a heightmap vertical sample returns the same height + slope — mathematically equivalent. | **ACCEPTED** — equivalent result. | starttown.cpp:1211–1214, 1361–1372 |
| 7 | `dee0` cell-grid terrain-type marking folded into placement, and `dc20`'s two-pass occupancy collapsed to a single cell+0x08 write. | `MapComponent::OnCreate`/`Draw` never fire on inert components, so the real `OnCreate → impl_4750 → dee0` and lazy `Draw → sub_41b0 → dc20` chains never run. The fold writes the **net end-state** both passes would leave (classified terrain type + owner-entity occupancy), which is what pathfinding + harvest-search read. | **ACCEPTED** — net-identical cell state. ⚠ This row's premise is now FALSE: `dc20` DOES run (slot 6 → `ApplyOccupancy_41b0`, both gates default-on). The verdict holds for a different reason — see the closure summary below. | `StartTown_MarkResourceCellGrid`, starttown.cpp:1317–1520 |

## Deviation #4 — CLOSED 2026-08-13

The initial sub-resource spawn is **restored** (`SpawnComponent_InitialSpawn` in
spawncomponent.cpp, called from the scatter loop under `BANISHED_SPAWN_INITIAL`).
Its revisit trigger had been met: `BANISHED_LIVE_GROWTH` went default-on 2026-08-10.

Closing it surfaced a second, larger fault that #4 had been masking. **The exe
address `0x14000c520` — the stride-0x10 resize behind `SpawnDescription+0x20` —
was registered nowhere.** `SpawnDescription::Serialize` reads the entry count into
a *local* and only commits it through that resize call, so with the address
unregistered every SpawnDescription in the game deserialized to **count=0 with a
null array**. All 8298 trees carried an empty spawn table. Measured before/after
on the same build:

| | before | after |
|---|---|---|
| trees with a non-empty SpawnDescription | **0** of 8298 | **8298** of 8298 |
| `SpawnComponent::Update` outcome | 139999 of 140000 calls bail at `noTimerArray` | 0 bail; 39999 reach the loop |
| DoSpawn dispatches | 0 | 31928 (per-tick) + 39629 (worldgen seed) |
| wild-food entities created at worldgen | 0 | 338 |
| gatherer hut's accepted resources in radius | 0 | 9 |

⚠ The old "worldgen spawns only 3 natural-resource templates, none in the hut's
accept list" verdict was **wrong twice over**. Three pool templates is retail's
own number (Rock/Ore/Tree — the gatherer's four resources are not a worldgen pool
at all, they are spawned by the *tree's* `SpawnDescription`). And the "3 distinct
keys near the start" census that supported it was taken by the demo-hut siting
code, which runs in `StartTown_SpawnStructures` — **before** the resource scatter,
so it measured a map that had no wild food on it yet by construction. That scan
still runs early (it needs the structures pass's occupancy data); a
`POST-SCATTER recount` line now reports the number that means anything.

### FIXED 2026-08-13 — the seed-pass crash was a CALL-ORDER deviation

The crash is gone. Root cause: **`5cc0` ran BEFORE the parent entity was registered.**
The "not yet checked" ordering assumption was the bug — it was backwards.

**A/B, same pinned seed, byte-identical code differing only in WHEN the seed runs**
(`BANISHED_SPAWN_INITIAL_PREREG=1` restores the old order and is kept as a harness):

| arm | dc20 evictions | outcome |
|---|---|---|
| fixed order (default) | 61 | **SURVIVED 2/2** |
| old order (`PREREG=1`) | **105** | **CRASHED ~10s 2/2** |

The 44 extra evictions are the measured mechanism: a wild-food child placed on a cell
the parent tree then claims is DESTROYED by `dc20`'s stale-occupant eviction
(`MarkForRemoval` + push onto the deferred-destroy queue at `entityList+0xd0`). With
the correct order the tree claims first and `DoSpawn`'s `TileFree` rejects those cells,
so the children are never placed there. `CREATED=249` in both arms — the seed pass
produces the same entities either way; the old order then killed 44 of them.

Retail's `a12b0` per-resource block, read off the disassembly:

```
1400a1a4e  CALL 0x1400c3720   CopyCtor / CreateEntity
1400a1afe  CALL 0x140126540   SetTile
1400a1bcf  CALL 0x140120db0   SetGrowthProgress
1400a1bf2  CALL 0x1400c48a0   <-- REGISTER the parent entity
1400a1c9e  CALL 0x14001b110   find SpawnComponent (DAT_1401c48e0)
1400a1cb2  CALL 0x140055cc0   <-- 5cc0 initial seed
1400a1de0  CALL 0x1401253d0   ModelComponent::SetVariant
```

Our call site sat just BEFORE `fn_EntityList_RegisterEntity`. **Registration is not
bookkeeping**: `0x1400c48a0` tails into `FUN_1400c2f60`, which walks the entity's
component array (`+0x30` / count `+0x38`) and dispatches component vtable **`+0x30`
(slot 6, OnAdded)** on every component whose `+0x20` bit 0 is set — the pass that puts
a live `ModelComponent` into `Graphics::Scene`. Seeding children first ran the whole
DoSpawn closure against a parent that had not yet joined the scene.

Fix = move the `SpawnComponent_InitialSpawn` block after `fn_EntityList_RegisterEntity`
in starttown.cpp (one statement). Measured, same pinned seed (`BANISHED_SEED=12345`),
same flags (`LIVE_SPAWN + INITIAL`), 150s per run:

| | before | after |
|---|---|---|
| ACCESS_VIOLATION | **4 of 4** runs | **0 of 4** runs |
| `[HeatTick]` lines reached | 6 | 18 (watch ended, still running) |
| worldgen-seed `CREATED` | 249 | 249 (unchanged) |

The seed pass output is bit-identical, so this removed the crash without changing what
gets spawned.

**Crash site, pinned exactly** (keep — it identifies the fault *class*):
`Runtime+0x102448` is `CALL qword ptr [RAX + 0x40]` inside `Graphics::Scene::Clean`
(0x1801023f0), where `RAX = *(sceneObject)`. The `CMP dword ptr [RCX+0x2c],0x0` one
instruction earlier had already succeeded, so the object memory is **mapped but its
vtable pointer is garbage** — a freed object still linked in the scene array, NOT a
null vtable slot. Any future dangling-scene-node bug will land here again.

The clincher is in the register dump: `RAX=41FC00003F800000`. That is not a pointer,
it is two packed floats (`31.5f`, `1.0f`) — the freed allocation had already been
RECYCLED and refilled with vector/transform data while the scene array still pointed
at it. Textbook use-after-free, not a null or an uninitialised slot.

**Ruled out, do not re-spend** (all four MEASURED, not argued):
1. Guarding the two `0x140120db0` (SetGrowthProgress — the `SetOrient` typedef name is
   wrong) calls in `DoSpawn` on the GrowthComponent's sibling links. `LIVE_GROWTH` is
   default-on so the links ARE populated, the guard never fired, crash reproduced
   byte-identically. Note is in spawncomponent.cpp above `DoSpawn`.
2. The **DeletePtr arm** of `0x1400c48a0`: it opens with
   `if (*(char*)((char*)list + 0xE0)) { InternalBasePtr::DeletePtr(ibp); return; }`,
   i.e. a closed EntityList DESTROYS the entity instead of filing it — which would
   strand a scene node exactly as observed. Our census counted `created` unconditionally
   and so could not see it (the recurring "a counter that also counts the non-event"
   trap). Instrumented as `listClosed`: **measured 0**. Not it.
3. **Evicted entities skipping the scene-teardown broadcast.** `MarkForRemoval`
   (0x1400c2ea0) only dispatches the component `+0x38` "removed from world" pass — where
   `ModelComponent` RemoveObjects its scene geometry — when the entity's aggregated flag
   bit 1 (`+0x40 & 2`) is set. Plausible, and wrong: `skipSceneTeardown=0` in BOTH arms.
4. **The broadcast reaching no component.** It only visits components whose OWN `+0x20`
   has bit 1, so an entity could pass the gate and still tear down nothing.
   `noParticipatingComp=0` in both arms. Also wrong.

#### ✅ ROOT CAUSE FOUND 2026-08-14 — the mark happens BEFORE the entity's own scene add

Both earlier explanations were right to die: the teardown never failed. **The scene
registration happens AFTER the removal mark**, so there is nothing for the teardown to
undo and nothing left to undo it afterwards.

Measured with `BANISHED_DRAIN_AUDIT=1` in the repro arm
(`BANISHED_SPAWN_INITIAL_PREREG=1`, seed 12345). The log shows a **mutual eviction pair
on one cell**, then the fatal add:

```
dc20 EVICT victim=A claimant=B cell=9676  A comps=5 [1](MODEL id=-1   var=3)
dc20 EVICT victim=B claimant=A cell=9676  B comps=4 [1](MODEL id=2206 var=0)
Scene::AddObject on MARKED entity=A id=2206
```

1. In the wrong order, `SeedInitialSpawn(A)` runs before `RegisterEntity(A)`. Child `B`
   is created AND registered while its parent tree `A` is not registered yet.
2. `B`'s registration claims cell 9676 and `dc20` evicts `A` — whose `ModelComponent`
   is still `modelId == -1`, because `A` has not been registered. `MarkForRemoval`'s
   `+0x38` broadcast runs correctly and unlinks **nothing**. `A` is now marked (0x400)
   and queued.
3. `RegisterEntity(A)` then runs anyway. `0x1400c2f60`'s OnAdded (slot 6) pass has **no
   notion of the 0x400 removal bit** — `ModelComponent::Draw` adds `A`'s geometry to
   `Graphics::Scene`, taking id 2206, the id `B`'s teardown had just released.
4. The drain frees `A`. Its allocation is recycled with `Scene` still holding a pointer
   to `A + 0x28`; `Scene::Clean` walks it → `RAX = 41FC00003F800000`.

Quantified at the free point, same seed, 2 arms:

| arm | dc20 evictions | entities freed | freed WITH a live scene node |
|---|---|---|---|
| wrong order (PREREG) | 105 | 109 | **44** |
| correct order (shipping) | 61 | 70 | **0** |

44 is exactly the 44 extra evictions the ordering fix removes — the two numbers were
the same entities all along.

**Invariant retail relies on**: nothing registers geometry after a removal mark. Retail
holds it by construction (register, THEN seed) and by draining in the same frame.

**Net added** (`AuditDyingEntity`, entitylist.cpp): the drain now re-fires each
ModelComponent-derived component's own `+0x38` handler for any component still holding
`modelId >= 0` before the free. Self-guarding (a no-op unless something re-registered
after the mark), and it lives in our already-deviating deferred drain rather than inside
a byte-faithful function. It unlinks all 44 in the repro arm (which then survived the
full watch) and fires **0 times** on the shipping path — so it is a net, not a crutch.

Second finding from the same instrument, left OPEN: ~5–6 entities per run reach the
drain with the **0x400 bit gone again** (`unmarked=` in the `[drainAudit:drain]` line),
in BOTH arms. `NotifyEntityChanged` (0x1400c3470) zeroes `entity+0x40` and rebuilds it
from the component flag words, which do not carry 0x400 — so a marked entity that
changes any component flag before the drain becomes un-marked and is ticked like a live
entity for the rest of our one-frame deferral window. It produced no live scene nodes in
either arm (`both=0`), so it is not a crash source today, but it is the mechanism to
check first if a marked entity is ever seen doing work.

### #3 — re-dispositioned ACCEPTED 2026-08-13, on a CORRECTED rationale

The verdict survives; the reasoning behind it did not, and the old wording would have
misled the next reader.

The old rationale was *"the remove/re-add slots need a live `ModelComponent`; inert
components never dispatch them."* **That premise died when the `BANISHED_LIVE_*` flags
graduated to default-on (2026-08-10).** `0x1401253d0` is `ModelComponent::SetVariant`
and it dispatches the scene slots directly, live or not:

```
if (-1 < *(short*)(this + 0x166)) (**(code**)(*this + 0x38))(this);   // scene REMOVE
*(short*)(this + 0x166) = newSubVariant;
if (-1 < newSubVariant) { MeshInstance::SetMesh(...);
                          (**(code**)(*this + 0x30))(this); }          // scene ADD
```

Retail calls it at `0x1400a1de0` (after registration); we call it
(`ApplyResourceModelVariant`) before. The worry was a DOUBLE scene add — SetVariant
adds, then registration's `FUN_1400c2f60` OnAdded pass adds the same object again.

**Measured against the crash dumps: it does not happen.** `Scene::Clean`'s object count
(`RDX`) is **8851**, against ~8840 entities in that run (8490 scattered resources + 249
wild food + town structures/citizens) — almost exactly one scene object per entity. A
double add on the 8081 variant-carrying trees would put the count past 16 000. It is
~8.8k. The scene ends up with the correct contents exactly once, which is what #3
always claimed. ACCEPTED stands.

(Mechanism, for the record: on a fresh component `+0x166` is the ctor's -1 sentinel, so
the REMOVE arm is skipped; whatever makes the second add idempotent is not established
here, only that the net count is right.)

## Closure summary
- **3 ACCEPTED** (#3, #6, #7): net-identical to retail output. #3 and #7 were
  re-argued from scratch 2026-08-13 after their inertness premise expired; both
  verdicts held, on measurements rather than on the old reasoning.
- **#4 CLOSED** (2026-08-13) — seed pass restored AND its crash root-caused to the
  `5cc0`-before-registration call order, proven by A/B (2/2 crash old order, 2/2
  survive fixed, 44 extra `dc20` evictions in the old arm).
- **1 DEFERRED→LIVE** (#5): carries the `BANISHED_LIVE_GROWTH` revisit trigger and fix.
- **No open worldgen deviations.** One NON-worldgen latent bug was uncovered and is
  recorded above: destroying an entity via `dc20` eviction leaves a live scene node
  even though the teardown broadcast runs. Our fix avoids the trigger; the underlying
  fault is unexplained and belongs to whoever next touches an eviction path.

⚠ **The inert-component premise was load-bearing across this whole register**, and the
`BANISHED_LIVE_*` graduation of 2026-08-10 invalidated it globally. All three ACCEPTED
dispositions were re-read this session; every verdict survived, but two needed new
reasoning:

- **#6 never depended on it.** Its argument is geometric — `a12b0`'s picking ray is
  vertical, so a vertical heightmap sample returns the same height + slope. Untouched.
- **#3 depended on it entirely.** Re-argued and re-measured above; ACCEPTED stands on
  the scene-object count, not on inertness.
- **#7 depended on it too**, and its premise is now simply FALSE. It claimed
  "`MapComponent::OnCreate`/`Draw` never fire on inert components, so the
  `Draw → sub_41b0 → dc20` chain never runs." That chain **does** run today:
  `MapComponent_OnAddedVSlot` (slot 6) calls `MapComponent_ApplyOccupancy_41b0 → dc20`
  whenever `desc[0x30]` is set, which is exactly what natural resources set, and its
  two gates `BANISHED_LIVE_MAP_SPAWN_CLAIM` / `BANISHED_LIVE_MAP_SOLIDIFY` are BOTH in
  `kDefaultOnFlags`. So our folded single write and the real `dc20` pass now both run.
  **Verdict still ACCEPTED, on new grounds:** they write the same owner, so `dc20` sees
  `occupant == newOwner` and takes no action. Supporting measurement — a default run
  evicts **61** of 8490 scattered resources (those are town structures clearing their
  own footprints, the expected case); if the fold and `dc20` disagreed about ownership
  the eviction count would be in the thousands. Keep the fold.

Any future appeal to "it's inert, so it can't matter" is no longer a valid closing
argument in this file.

`BANISHED_SPAWN_INITIAL` is still **opt-in**. It is now crash-clean 4/4 and is a
graduation candidate for `kDefaultOnFlags`, but graduation needs its own positive run
(assert a value only the seed path produces, with no `LIVE_*`/`SPAWN_*` var set).
