# Next-session brief — closing the gaps the marriage/birth soak exposed

Written 2026-07-25 after commit `668d366` (see `notes/MARRIAGE_BIRTH_SOAK.md` for the run
data behind every claim here). Branch `wave/upgrade-path-complete`.

Ordered by retail behaviour unlocked per unit of work. Items 1 and 2 are one wave: item 1
without item 2 registers houses that can never report themselves full.

---

## 1 ⭐ Register the start-town buildings in the town registries

**Why this is first.** One gap explains four dead systems: nobody marries, nobody moves out
of their family, nobody gets a job on their own, and nobody switches workplace. All four run
through finders that read two town-manager registries, and in a default standalone run both
are empty:

```
[FamilyCensus]  townMgr=… buildingRegistry(+0x20)=0000000000000000 count(+0x28)=0
                profBST(+0x100)=0000000000000000 bldgCount(+0x68)=0
```

**The machinery already exists and is proven.** `TownRegistry_RegisterWorkplace`
(`code/Game/Game/town_workplace_registry.cpp`, a faithful port of the lazy find-or-create
AVL insert) and its reader `FindAvailableBuilding` (`0x14008d100`) work end-to-end today —
for the ForesterLodge under `BANISHED_LIVE_WORKPLACE` (`starttown.cpp:1449`) and for
construction sites under `BANISHED_BUILDER_NATURAL` (`buildcomponent_state1.cpp:470`).
Nothing registers when neither of those paths runs, which is why the BST root is null.

### 1a. Houses never get a profession/category key

Retail's `WorkPlaceComponent::OnCreate` (`0x14012f090`) reads the key at `WPC+0x48`, walks
`townMgr+0x100` for that key, and appends the owner entity to the node's array; if `+0x48`
is 0 or the key is absent it falls through to `WorkPlaceComponent_OnCreate_sub_ee60`
(`0x14012ee60`), which is the roster-release tail, not a registration. In standalone `+0x48`
is 0 for every spawned building because the `WorkPlaceDescription` Serialize that fills it
from `desc+0x20` is one of the bailing slots.

Do for each founding residence exactly what the lodge path already does:

```
seat WPC+0x48 from WorkPlaceDescription+0x20
WorkPlaceComponent_SetAccepting(wpc, true)      // 0x14012fc00 — also gates FindAvailableBuilding
TownRegistry_RegisterWorkplace(townMgr, wpc)
```

Then check the reader agrees: `FindAvailableBuilding(townMgr, &out, key)` should return the
house.

### 1b. `townMgr+0x20/+0x28` has no writer at all — find it

This is a **separate, flat** building array from the `+0x100` BST, and it is the one
`CitizenComponent_Update_sub_0e70` (`0x140110e70`) scans when a grown child looks for a home
of its own. I did not find its retail writer this session — that is the first Ghidra job.
Start at the TownManager ctor `0x140090a30` (Ghidra mislabels the class as
`TradeComponent`) and xref the `+0x20` / `+0x28` fields; the writer is most likely a
building-registration entry point called from entity registration or from build completion.
Do this **before** writing any code: if the flat registry has a different population rule
than the BST, 1a alone will not make anyone move out.

### Done when

* The census prints a non-null `buildingRegistry` and `profBST`.
* `ELIGIBLE>0` appears in a census sample.
* A citizen moves out and marries **without** `BANISHED_MARRY_HARNESS`. That harness exists
  precisely so this can be A/B'd: run with it to confirm the marriage code still works, then
  run without it to prove the natural path now reaches the gate.

---

## 2. Capacity-bounded residence assignment

`StartTown_AssignFoundingHousing` rosters an entire family into `slot[0]` regardless of the
`ResidenceDescription+0x20` occupancy cap (the roster is even resized to that cap by
`StartTown_EnsureResidenceRoster` and then ignored). Consequences:

* "Is this house full?" has no answer, so the free-house search item 1 depends on cannot
  work correctly even once the registries are populated.
* It breaks the invariant retail's `ResidenceUI::Slot7` (`0x1400b51f0`) relies on — it
  indexes the dialog row array by resident index with **no bound**. The row-bound guard added
  in `668d366` papers over that; fixing the assignment retires the guard.

Note the guard has never been observed to fire, so the `ResidenceUI_Slot7+0x295` crash it was
written for is **not** confirmed to be a row overflow — the mass-death teardown is an equally
good explanation. Re-check that crash after this item lands.

---

## 3. Firewood, so a soak can outlive the first winter

Every long run ends the same way at ~frame 16000 (`FORCE_SPEED=10`, seed 12345): the whole
population dies within ~2000 frames. `+0x48` is the **warmth** need, not thirst — `Update`
pins it at 0 while `weatherData+0x18` is above `desc+0xF4` and lets it climb once the
temperature falls, with death at 20. Hunger never gets near its own threshold (max ~14 of
25); the houses' seeded food holds. There is no firewood.

* Retail-matching: a woodcutter producing fuel + the haul into house storage. The heat tick
  itself already works (`0x140129970`, real state table) and `RawMaterialFlags` bit 6 = Fuel
  is known — this is the missing producer, the same "missing woodcutter economy" already
  noted against residence heating.
* Interim: seed fuel next to the existing food seed (`StartTown` house-food precedent). That
  buys soak time but is a stand-in — label it as one, do not call the winter fixed.

Until this lands, **no birth-rate number is measurable**: the usable window is one pre-winter
season, in which 4–6 births occur and then the surviving mothers pin on the storage-capacity
branch (`+0x4c = desc+0x58`).

---

## 4. Retire the hand-rolled founding spawn

`SpawnOneCitizen` (`starttown.cpp`) stands in for retail's `sub_1040` and reproduces
`FUN_140117080`'s effects piecemeal. That divergence is exactly what produced a founding
cohort with no gender at all — a whole class of bug that disappears if the founding path is
retail's own. `117080` is now registered, live on the birth path, and proven, so the port of
`CitizenComponent_impl_0dd0` (`0x1400a0dd0`) / `sub_1040` is tractable.

**Not a small change**: most of 117080's effects (mesh variant, spawn transform, name,
colour, age, laborer seed, housing) would double-apply until the hand-rolled equivalents come
out. Plan it as a replacement, not an addition.

---

## 5. Smaller items worth closing while in the area

* **Scheduler id-map at `sched+0x30` is still empty** (buckets at `+0x120` populate, the flat
  id-map never does). It silently writes priority 0 into every posted work (`workComp+0x2c`)
  and blocks verification of the `f8400` re-key.
* **Birth cooldown magnitude.** The decode is fixed (`XORPS` with `0x80000000` = negate the
  float, not the integer truncation the decompiler's `(uint)(…) ^ DAT` rendering suggested),
  but the resulting rate is unmeasured. Also settle whether the storage-capacity branch
  pinning `+0x4c = desc+0x58` is faithful or is starving the birth rate.

---

## Process: make a town-health run part of every session

A default-path crash on the **first childbirth** survived this long because no run ever got
that far. The pieces for a standing check now exist:

```
BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_FORCE_SPEED=10 \
BANISHED_FAMILY_CENSUS=2000 BANISHED_ONREMOVE_TRACE=1 ./reimpl-x64.exe
```

on the **release** build (printf stripped, `fprintf(stderr, …)` kept, ~10× the sim throughput
of the `-O0` build). Twelve minutes gives population, marriages, family links, survival
margins and every removal, against a pinned seed. Worth wiring into a script and running at
the end of each session.

Two traps this session paid for:

* `standalone_crash.log` is created **empty** at startup — a poll loop must test `-s`, not
  existence.
* A quiet log is not a hung game. RTSS's frame limiter blocks in Present, so an idle-but-fine
  game samples inside `ContextData_Finish` every time. Read state, not stacks:
  `gdb -p <pid> -batch -ex "p/x **(unsigned long long**)&g_pGameWorldSingleton"` then
  `p *(int*)(<townMgr>+0xa8)` — 0 means the town is extinct.
