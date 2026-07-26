# Next-session brief — after the town-registry and D–I waves

Rewritten 2026-07-26 (commits `07155bb`, `255d5e4`), then updated the same day after the
**D–I wave**. Run data behind every claim is from the standard town-health run (below) on
branch `wave/upgrade-path-complete`.

**Jump to [Investigative opens for next session](#-investigative-opens-for-next-session)**
for the A–I list.

**Update 2026-07-26 (A/B/C wave):**

* **A — CLOSED, and the flag is default-on.** The town grows: **24 → 35 over 21 000 frames**,
  fourteen births, no crash. Two bugs (an unwired OnAdded vtable slot, an unassigned
  `RandFloat`) plus a naming correction that had the delivery roll's two branches backwards.
* **B — ANSWERED.** The player *is* the growth vector. The town fills its six houses to the
  occupant cap and plateaus with 18 adults over marry age and nowhere to move to. That is
  faithful; stop treating a frozen population as a bug.
* **J — new, found and FIXED.** A mistyped `Math::Scalar::Abs` mangled name left a null
  function pointer on the **mainline raw-material drop-off path**. Latent for a long time;
  surfaced once the town had enough citizens to hit it.
* **C — RESCOPED and left for next session.** The town starts with 400 firewood in its barn
  and never touches it, so the missing piece is the **haul**, not a woodcutter. C is the only
  substantial open left.

### What the D–I wave changed (2026-07-26)

* **WorkPlaceComponent is a live controller** (`BANISHED_LIVE_WPC_CONTROLLER`, opt-in) —
  both stand-in hooks retired, the two spawn-site blocks consolidated, and the previously
  unexercised founding-employer path verified.
* **Three bugs fixed that nobody was looking for**: an 8-byte `Time` ctor writing through
  a 4-byte local (killed every upgrade click), inverted BST child offsets that meant the
  scheduler's order tree was never erased, and three wrong dispatch-slot bindings.
* **The `+0x52` lazy-link guard** now rejects both sentinels (0x00 and Tick's 0xff).
* **The retail founding spawn is ported** (`BANISHED_LIVE_FOUNDING_SPAWN`, opt-in).
* **G is half-answered**: mass-death teardown does NOT reproduce the ResidenceUI crash.

## The standing town-health run

```
BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_FORCE_SPEED=40 \
BANISHED_FAMILY_CENSUS=3000 BANISHED_LIVE_CITIZEN_FAMILY=1 \
BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_BUILDER_AUTOPOOL=1 ./reimpl-x64.exe
```

Two env vars matter more than they look:

* `BANISHED_LIVE_CITIZEN_FAMILY=1` is what makes the founding cohort six **couples with
  children** instead of 24 unrelated singles. Without it the census reads
  `spouse=0 host=0 ELIGIBLE=12` forever and nothing about family dynamics is measurable.
  It is still opt-in.
* `BANISHED_BUILD_DEMO_HOUSE=1` gives the town a **seventh, empty** house. Nothing in the
  founding town is spare — six families in six houses — and a grown child cannot leave home
  without a free one to leave *to*. That is faithful (in retail the player builds more
  housing), but it means a default run can never demonstrate move-out or marriage.

---

## 1. ✅ CLOSED — the town registries

Both registries populate and the four dead systems behind them are alive.

```
[FamilyCensus] frame 3000: 24 citizens — spouse(+0x250)=14 ... host(+0x258)=10
[FamilyCensus]   buildingRegistry(+0x20)=000001f73af8fc68 count(+0x28)=7
                 profBST(+0x100)=000001f73af8c9f8
```

Founding state is `spouse=12 host=12` (six couples, twelve dependants). By frame 3000 it is
`spouse=14 host=10`: **two grown children moved out and married, with no
`BANISHED_MARRY_HARNESS`** — the brief's own done-when test.

### What the gap actually was (`townMgr+0x20`)

Not a missing writer. The writer is `ResidenceDescription_helper_9de0` (`0x140129de0`), the
residence controller's **state-2 (built) ENTER** handler — registered by the state table
`ResidenceDescription_helper_a350` (`0x14012a350`, `sub_7690(2, 9de0, …, 9970)`), and it was
already ported as `ResidenceComponent_EnterBuilt`. It never ran:

`StateMachineComponent_LinkControllers_Standalone` unconditionally wrote the first live
controller's `GetDefaultState` into `+0x52`. Retail links controllers at component-creation
time, when `+0x52` is still factory-zero, so that write can never destroy a caller's intent.
Standalone links **lazily, on first Update** — i.e. *after* `StartTown_SpawnStructures`
stamps its pre-built structures with `+0x52 = 2`. The stamp was overwritten with
BuildComponent's `1` (under construction), a state the residence table has no entry for, so
every founding house sat dormant. Fix: a non-zero `+0x52` at link time now wins
(`gotDefault = (*(base + 0x52) != 0)`).

Verified in the same run that this does **not** wrongly promote a foundation: the demo
foundation (`+0x52` deliberately unstamped) registers at log line 589, long after the six
start-town houses at 485–496 — i.e. by its real 1→8→2 construction transition.

### What the old brief got wrong about `+0x100`

The old item 1a said retail's `WorkPlaceComponent::OnCreate` (`0x14012f090`) appends the
owner to the profession node. It does the opposite — that function calls `FUN_140002cc0`,
which is **erase-by-value**, so `0x14012f090` is the *OnRemove* side. (Same trap one slot
over: `0x14012a0e0`, labelled `ResidenceComponent_OnCreate` in Ghidra, is likewise the
remove — it evicts every occupant via `helper_9ef0` and then erases from `townMgr+0x20`.
Ghidra's slot naming is off by one for this component family; the ADD/OnCreate slot is the
one *before* it in the vtable, and `CitizenComponent`'s pair — `0x140114000` add /
`0x140113ca0` remove — is the reference alignment.)

The real registration path is `WorkPlaceDescription_helper_edf0` (`0x14012edf0`) = f670 +
`AddWorkplaceToProfessionTree`, wired as the WorkPlaceComponent controller's **state-2
enter** (`WorkPlaceDescription_helper_f3a0` @ `0x14012f3a0`). `f670`'s tail is what seats
`WPC+0x48 = WorkPlaceDescription+0x20`, but only for a building whose key is *currently* the
Builder profession — i.e. one that really went through construction. A start-town structure
never held the builder key, so `f670` no-ops and `edf0` early-outs on `key == 0`.
`StartTown_SpawnStructures` now seats the key `f670` would have left and runs the
registration half.

Note the default start town has **no employer buildings at all** (a stockpile and a barn,
neither with a profession), so an empty `profBST` at spawn is faithful. It fills the moment
anything is under construction (the Builder-profession bucket) or an employer is placed.

---

## 2. ⚠️ WITHDRAWN — "capacity-bounded residence assignment"

The premise was a misreading of `ResidenceDescription+0x20`. That field is the **household
SLOT count**; the occupancy cap is the *next* field, **`+0x24`**, and the founding pass does
not violate it. Measured: `occupantCap(desc+0x24)=5`, and the largest founding family is 5,
so no house is over-filled. (`+0x24` is not decorative — see item 5, it is the birth gate.)

Why `+0x20` is the slot count:

* `ResidenceComponent::Serialize` resizes `+0x50` to `desc+0x20` and stores it in `+0x58`.
* Every reader (`sub_0e70`, `ResidenceUI::Slot7`) walks `+0x58` entries at stride `0x10`,
  each entry being its own `{data,count,cap}` list of citizens.
* A wood house reads `desc+0x20 == 1` at runtime, and retail's home finder **prefers**
  `*(comp+0x58) == 1` and only falls back to `1 < *(comp+0x58)`. That preference would be
  inverted nonsense if the field were an occupant count.

So one family in `slot[0]` is exactly what retail's own housing solver produces, and
"is this house full?" *is* answerable — `slot[0].count >= 1`, which is precisely what
`sub_0e70` tests. Nothing to fix; the comment in `residenceui.cpp` has been corrected.

The row-bound guard from `668d366` **stays**, but for a different reason than recorded:
`0x1400b51f0`'s populate walk really is unbounded (only the blank-out loop afterwards is
bounded by `controller+0x38`), so a household grown past the table's row count by births
would run off the end in retail too. It has still never been observed to fire — and note the
autostart soak never opens a residence panel, so "never fired" is no evidence either way
about the `ResidenceUI_Slot7+0x295` crash. **Open:** open a house panel in a run that has had
births, and see.

---

## 3. ⚠️ PARTIAL — firewood (stand-in only, the winter is NOT fixed)

`StartTown_SeedHouseInventory` now seeds fuel (`RawMaterialDescription+0x5c` bit 6 = 0x40)
next to the existing food seed, 400 units/house, `BANISHED_SEED_FUEL=<n>` to change or `0` to
disable. Verified live — the heat tick consumes it:

```
[HeatTick] house=… BURN 1 fuel (temp=29.98 < 50.00) slotLeft=368 refill=60.00s
```

and the warmth need (`+0x48`) now sits at ~2 of a death threshold of 20 where it previously
climbed to 20 and killed the town at ~frame 16000.

Fixing the cold death immediately exposed the next wall: **starvation**. With fuel but the
default food seed (~130/house), the same seeded run went 24 → 3 citizens between frames 6000
and 9000 on hunger alone. `BANISHED_SEED_FOOD=<n>` raises the food floor the same way; with
both, the town is intact and unchanged at 24 through frame 12 000.

**This is a stand-in and is labelled as one in the code.** The real fix is unchanged: a
woodcutter producing fuel plus the haul into house storage. Note the tick burns
*continuously* (the map's temperature 29.98 is already below the house's `desc+0xF4` of 50),
so a real producer has to sustain roughly one unit per house per 60 in-game seconds
year-round, not just in winter.

---

## 4. ⏳ OPEN — retire the hand-rolled founding spawn (recon done, no code)

Unchanged in scope, but the port is now fully mapped and is smaller than it looked.

`CitizenComponent_impl_0dd0_sub_1040` (**`0x1400a1040`**) is ~40 lines:

1. `MemAlloc(0x48, 8)` + `ComponentEntity_CopyCtor(mem, template)` — template from the start
   config at `startConfig+0x58`.
2. `Entity_FindComponent_Citizen` → the CitizenComponent (returned via `param_2`).
3. `desc = *(comp+0x18)`, `IsDerivedFrom(c7190 CitizenDescription)`.
4. If `age < desc+0x44` (adult age), force the `educated` arg to 0.
5. `pos = base + RandFloatTol() * (startConfig+0x40)` (the spawn jitter).
6. `laborerProf = *(*(gw+0xb0)+0x30)`.
7. **`FUN_140117080(comp, pos, gender = (param_5 == '\0'), isChild = age < desc+0x44, age,
   educated, &nullIBP, laborerProf)`** — the whole per-citizen init, already registered and
   proven on the birth path.
8. `FUN_1400c48a0(*(gw+0xe8), &entityIBP, *(template+0x20))` — the EntityList register.

`impl_0dd0` (`0x1400a0dd0`) is then just the family loop: per family, `sub_1040` with
`param_5='\0'` then `'\x01'`, cross-link `+0x250` both ways, then `Rand(children+1)` children
each joined with `CitizenComponent_helper_68a0`.

⚠ **One trap before writing any of it**: the existing typedef in `citizencomponent.cpp`
declares argument 5 of `FUN_140117080` as `int`, but retail passes a **float** age there. It
happens to work today only because the birth path passes literal `0` (same bit pattern as
`0.0f`). Fix the typedef to `float` or every founding colonist gets a garbage age.

Still a **replacement, not an addition**: most of `117080`'s effects (mesh variant, spawn
transform, name, colour, age, laborer seed, housing) are reproduced piecemeal by
`SpawnOneCitizen` and would double-apply. Gate the two paths against each other so the switch
is A/B-able.

---

## 5. Scheduler id-map — ❌ CLAIM WAS WRONG. Birth gate — 🔎 root-caused, not yet fixed

### The id-map DOES populate

Measured under `BANISHED_BUILD_TRACE=1` at the point of insert:

```
[BuildTrace] WorkScheduler_Insert id=1 prio=0 code=0x2001 …; idMap(sched+0x30) root=…c638 count=1
[BuildTrace] WorkScheduler_Insert id=2 prio=0 code=0x2001 …; idMap(sched+0x30) root=…c6e8 count=1
```

So "the flat id-map never populates" is false, and neither the `workComp+0x2c = 0` writes nor
the re-key's `NOT in the sched+0x30 id-map` miss can be blamed on it. Two things that
misled the earlier session, both worth keeping:

* **Most posts never touch the `ec60` wrapper.** They call the scheduler insert directly
  (exe `0x1400f88a0`). A trace placed only in `CitizenAI_WorkScheduler_Post` sees nothing,
  which reads exactly like "the post path is dead".
* Note the root **changes** between the two inserts while the count stays 1 — consistent
  with the item being claimed and erased between posts. That is also the most likely reason
  a later `f8400` re-key finds nothing: the work is already gone, not never filed.
* `prio=0` is the *caller's* argument, not a lookup failure. The direct-insert callers pass
  0; only the `ec60` wrapper passes `workComp+0x2c`. Chase the callers, not the map.

### Why there are no births

Nothing to do with the cooldown decode. `CitizenComponent::Update`'s reproduction timer
(`+0x4C`) only ever *counts*; the one place that seeds it positive is gated on
`0x14012a4f0`, which is **not** the storage query its old name (`CheckStorageCapacity`)
suggested. It is:

```
ResidenceDescription(+0x18)->+0x24  <=  residence->slot[citizen+0x240].count
```

— i.e. **"is this household full?"**, indexed by the citizen's own residence slot. The timer
starts only when there is room. With `desc+0x24 == 5`, **three of the six founding families
are 5-strong and are therefore full at spawn: those couples can never have a child.** The
other three have room for two each, and still produced no birth in 12 000 frames at
`FORCE_SPEED=40`.

Next steps, in order: (a) confirm a birth happens at all for one of the three non-full
households — if not, the gate above it (`+0x38 == 0` female, `+0x208 == 0`, age within
`desc+0x50..+0x54`, and the hunger/thirst reset at `+0x4C`) is where to look; (b) only then
is a *rate* meaningful, and the `desc+0x58` timer plus the negative-cooldown reseed can be
calibrated. The naming in `citizencomponent.cpp` has been corrected so this does not have to
be rediscovered.

---

---

# ⭐ Investigative opens for next session

**A, B and C are the only opens left** (D–I are recorded above as outcomes). They are one
thread: the town is **alive but frozen** — 24 citizens, 7 couples, zero births, zero
move-outs — and each is a separate reason it cannot grow. A is the one that unblocks the
most, including the untested half of G.

⚠ Two notes before running any of them, both new on 2026-07-26:
* Prefer the **defaults** (no `BANISHED_LIVE_FOUNDING_SPAWN`) for A/B/C work — the retail
  spawn path changes the founding cohort's RNG stream and therefore its size, so old
  census baselines do not carry across it.
* `BANISHED_LIVE_WPC_CONTROLLER=1` is safe to leave on and is the more faithful
  configuration.

## A. ✅ CLOSED (2026-07-26) — the town grows. Births always worked; two other things didn't

Verified live: **24 → 25 → 26 → 27**, the count stepping at the exact moment of each
delivery. Two independent bugs, and a naming correction that was the reason the open was
framed as "get one birth to happen" in the first place.

### The block at `LAB_140113873` is not a fertility roll — it is the DELIVERY

From the `0x140113923` disassembly, the corrected semantics of `+0x4C`:

| `+0x4C` | meaning |
|---|---|
| `> 0` | **gestation**, counting down; seeded to `desc+0x58` (= 900) by the conception gate |
| `== 0` | **delivery fires here** |
| `< 0` | **post-partum recovery**, counting back up to 0 |

and the roll at the delivery is **maternal mortality**, not conception:

```
pDeath = (health/maxHealth) * (desc+0x64 - desc+0x68) + desc+0x68
```

Measured live: `desc+0x68 = 0.0250` (at health 0), `desc+0x64 = 0.0063` (at health 10) —
i.e. a sick mother is ~4x likelier to die. **That monotonicity is the proof**: a fertility
chance that FELL as health rose would be nonsense. So `+0x50` is **HEALTH** (max
`desc+0x70` = 10), not happiness.

* `roll >= pDeath` (~99%) → `helper_1b90` = **the birth** (allocates the newborn), then
  seeds the post-partum cooldown.
* `roll < pDeath` (rare) → `HandleLifeEvent(self, 6, null)`, which is the **death**
  handler (tombstone + queue-for-removal). Died in childbirth.

The old comments had these two branches **exactly backwards** — `helper_1b90` was called
"the failed reproduction roll" and the death handler was labelled "(childbirth)". Names
corrected in `citizencomponent.cpp`.

### Bug 1 — the vtable slot that made every birth invisible

`CitizenComponent_LiveVtable` **slot 6 (+0x30, the OnAdded broadcast) was never filled.**
`ComponentEntity_OnAddedPass` (`FUN_1400c2f60`, the tail of `EntityList::RegisterEntity`
`0x1400c48a0`) calls that slot on every component whose `+0x20` bit 0 is set, and
CitizenComponent's flags are `0xCB` — so the broadcast always arrived and always landed on
the inert stub. Retail's slot is `CitizenComponent::OnAdd` (`0x140114000`), the append into
the town citizen list `townMgr+0xa0`, which was **already ported and already registered by
address** — just never wired to the vtable.

So three live births in one soak left the census at a flat 24: the babies existed, were in
the world entity list, and were in no population system.

**GRADUATED default-on 2026-07-26** (`BANISHED_LIVE_CITIZEN_ONADD` now in
`kDefaultOnFlags`; set it to `0` to A/B). 21 000-frame soak: **24 → 27 → 29 → 30 → 32 → 33
→ 35**, fourteen births, gender split staying near even (18F/17M), no crash, no deaths.

⭐ **Corollary worth acting on**: `StartTown_RegisterTownCitizens` is now fully redundant —
it reports `registered 0 founding citizens (24 already registered by the OnAdd broadcast)`.
It dedups now so the two paths cannot double-count. **Every other hand-rolled "standalone
spawns without running the registrar" pass deserves the same check**: the registrar may be
ported and merely unwired from its slot, in which case the workaround can be deleted rather
than maintained.

### Bug 2 — `s_fn_RandFloat` was declared, null-checked, and never assigned

Same class as the old `s_fn_FindPartner` gap. Both call sites silently took their fallback
constant and looked like working code:

* the delivery roll fell back to `1.0f`, so `pDeath <= roll` was **always** true — death in
  childbirth was impossible;
* the cooldown fell back to `0.5f`, so every mother got exactly `0.5*5400 + 1800 = 4500`.

The census signature was two mothers who delivered on the same tick showing byte-identical
`+0x4C` values (`-3400.68` / `-3400.69`). Now bound to `fn_Random_RandFloat`; rolls read
`0.5645 / 0.0270 / 0.5578`.

### The diagnostic, kept

`BANISHED_BIRTH_TRACE=<n>` (`standalone/death_harness.cpp`) walks the whole gate read-only
at each census and reports the **first failing test per citizen**, plus the roll constants:

```
[BirthGate] frame 3000: PASSES-ALL=0 | male=13 child=6 student=0 hungry/cold=0
            unmarried=1 homeless=0 timerBusy=4 ageWindow=1 noResidence=0 householdFull=2
[BirthGate]   thresholds: hungerWarn=21.5 warmthWarn=16.2 bearingAge=[2.20,8.00]
              timerSeed(desc+0x58)=900.00 occupantCap(rdesc+0x24)=5
```

Set to 1 or more it also narrates each delivery from `citizencomponent.cpp`
(`[Birth] DELIVERY … -> LIVE BIRTH`, `[Birth] BABY BORN … townCount(+0xa8)=25`).

The old brief's **#1 suspect was wrong**: step 4 (hunger/warmth zeroing `+0x4C`) never
fires — `hungry/cold=0` with hunger 0.1–11.2 against a warn level of 21.5.

**Rate, now that it is measurable**: the timer moves ~0.668 in-game sec per frame, so one
gestation+recovery cycle is ~8 000 frames at `FORCE_SPEED=40` (≈1.5 age units). Nothing to
tune — the "births are impossibly rare" reading came entirely from computing the mortality
chance and mistaking it for the fertility chance.

## B. ✅ ANSWERED (2026-07-26) — **the player IS the growth vector.** Stop calling it a bug

Open A changed the premise, and the 21 000-frame graduation soak then answered B outright.
The town grows **inside its existing houses** up to the occupant cap
(`ResidenceDescription+0x24` = 5) and then stops dead, by design:

| frame | citizens | over marry age | `noGuardian` | `ELIGIBLE` |
|---|---|---|---|---|
| 3 000  | 27 | 1  | 0 | 0 |
| 9 000  | 30 | 7  | 0 | 0 |
| 18 000 | 35 | 16 | 0 | 0 |
| 21 000 | 35 | 18 | 0 | 0 |

24 → 35 (+46%) in six houses, then a hard plateau with **18 adults over marry age, every
single one still a dependant** (`noGuardian=0`). They are not spinning on a broken system —
`sub_0e70` correctly finds no free house, so nobody moves out, so nobody marries, so no new
household exists to bear children into. Meanwhile the founding households sit at cap.

That is the faithful shape of an un-played Banished town: **it fills the housing it was
given and waits for a builder.** A frozen population at that point is not a defect and
should not be chased as one. The measurable done-when for any future housing work is simply
`ELIGIBLE` going above 0.

What is settled alongside it:

* **Immigration is not an un-played town's growth vector.** `ImmigrationComponent` is a
  ControllerComponent that `ImmigrationUI_helper_2020` (`0x140012020`) finds by scanning a
  single **entity's** component array — i.e. it lives on a player-built structure and is
  read through that structure's panel. In standalone only its `Shutdown`/serialize is
  ported (`immigrationcomponent.cpp`); there is no driver at all, and `NomadComponent` is
  likewise serialize-only. So nothing arrives on its own, in our build or by design.
* **Housing is the player's job.** A grown child cannot leave home without a free house
  (`sub_0e70`), which is why `BANISHED_BUILD_DEMO_HOUSE=1` is load-bearing in the standing
  run. That is faithful.

The one loose thread: 18 stuck adults still pick *some* AI state each tick (the tally is
dominated by SeekHappiness). Whether that is retail's own idle behaviour or a fallback we
land on by accident is worth one look — but it is a cosmetic question now, not a growth one.

## C. 🔎 RESCOPED (2026-07-26) — it is not the woodcutter. **The haul never runs.**

Measured, not assumed. Run the standing recipe with `BANISHED_SEED_FUEL=0` and
`BANISHED_STORAGE_TRACE=1`:

```
[StorageSeed] SeedStartingResources counts: … food(0x20)=2400 firewood(0x40)=400 …
[StorageSeed] SpawnResources mask=0x40 want=400 matchedTemplates=1 placed=400
[StorageSeed] dump: loc[1] … mask(+0x3c)=0x1c40 volumeUsed=5550
[StorageSeed] dump:   slot[3] material=… flags=0xc0 unitVol=4 count=400 reserved=0
```

**The town already starts with 400 firewood**, placed by retail's own start config
(`Game_impl_1f00`), in the one storage location whose accept mask has the fuel bit
(`0x1c40 & 0x40`; `loc[0]`'s `0x16220` does not). It is still `count=400 reserved=0` for the
whole run — **nothing ever touches it** — while the houses report:

```
[HeatTick] house=… COLD no fuel available (temp=49.86 < 50.00, heatLeft=-327003586, storageSlots=2)
```

So a producer is **not** the first missing piece. 400 units at ~1 unit/house/60 in-game
seconds is many seasons of runway; the town freezes to death standing next to its own
woodpile. `BANISHED_SEED_FUEL` was never papering over a production hole — it was papering
over a **distribution** hole, by teleporting the barn's job into each house.

### Where the break is

The fetch-for-home states are already ported: `CitizenAI_State_GatherResources` (id 6,
`0x14010bc90`) and `GatherResources2` (id 7, `0x14010b3e0`) in `citizenai_gather.cpp`. They
validate `citizen+0x238` — which is the **HOME**, not a workplace (`0x1401163d0`
"AssignWorkplace" is the residence assigner; the misnomer runs deep in this family) — and
transfer resources into it. The trigger shape is already known and already traced by the
housing pass:

```
[startTown::housing] house food(c7e78): storage=… residents=5 reqPerResident=25
                     threshold=125 seeded=8000 -> GetResourceCount(res)=8000 (SUFFICIENT: no Gather)
```

i.e. `threshold = residents × reqPerResident`, compared against the house's stock.

With `SEED_FUEL=0` every house is far below threshold for fuel — and the AI state tally
across the whole run is:

```
19 × aiState=8 (SeekHappiness)      1 × aiState=12
```

**Not one entry into state 6 or 7.** The handlers are live and the demand signal is real;
nothing ever *selects* them.

### Next step

Chase the **candidate-list** side (`citizenai_candidatelist.cpp`, the `LAB_14010ea39`
idle/leisure tail and the priority table), not a woodcutter. Questions in order:

1. Are ids 6/7 in the candidate list at all, and with what priority against SeekHappiness
   (which is winning 19:0)?
2. Does the fuel demand reach the candidate list, or only the food path? The `no-firewood`
   status-icon bit 7 SET side is documented as living in the Gather/Gather2 **no-target**
   branches — which can never run if the state is never entered, so that icon is not
   evidence either way.
3. Only once a citizen carries firewood home does a producer matter. Then scope the
   woodcutter (`consumeproduce.cpp` is the ConsumeProduce work shape) against the real
   burn rate: ~1 unit per house per 60 in-game seconds, year-round — the map temperature
   (29.98) is already below the house `desc+0xF4` of 50, so the tick burns continuously,
   not just in winter.

`BANISHED_SEED_FUEL` / `BANISHED_SEED_FOOD` stay as soak stand-ins until (1) and (2) land.

## J. ✅ FIXED (2026-07-26) — it was a mistyped mangled name, and it was not the elder path

**Root cause: `Math::Scalar::Abs` resolved to NULL.** `ResolveExport` was asked for
`?Abs@Scalar@Math@@SA?AV12@V12@@Z` — a *static* taking a Scalar by value. No such export
exists; objdump ordinal 1530 is `?Abs@Scalar@Math@@QEBA?AV12@XZ`, a const *member* taking
nothing. The lookup returned 0, the dispatch slot `0x140152420` is unregistered (it prints
`NOT IMPLEMENTED` right before every one of these crashes), and `s_scalarabs` stayed null —
a hard EXEC-of-0 the moment `Pf_FindPlacementCell_bd40` reached its height-delta check.

The ABI was never wrong: a const member returning a class by value passes `this` in RCX and
the return buffer in RDX, which is exactly the existing `(const float* val, float* retbuf)`
typedef. The old comment's own note — *"disasm shows RCX=val, RDX=retbuf — member-style
retbuf order"* — had the shape right while the name said static. **Only the string changed.**
Same string fixed in `fencedtool.cpp`, which had been limping on a hardcoded Runtime address.

⚠ **The original "elder path" framing was too narrow.** A second crash, with a completely
different caller, hit the same terminal function:

```
… Pf_SpillAllStorage_7570  → Pf_SpawnRawMaterialOnCell_78c0 → Pf_FindPlacementCell_bd40   (elder spill)
… Pf_DropOffExec_7620      → Pf_SpawnRawMaterialOnCell_78c0 → Pf_FindPlacementCell_bd40   (ordinary drop-off)
```

i.e. this was on the **mainline "citizen puts a raw material down" route** all along — which
is precisely the machinery open C needs. It was latent, not new: `LIVE_CITIZEN_ONADD`
surfaced it because registered newborns become active citizens that do drop-offs, so traffic
on that path rose. Post-fix the 21 000-frame soak is clean.

### Original write-up, kept for the diagnostic pattern

**Pre-existing and NOT caused by the A-wave changes** — A/B'd with
`BANISHED_LIVE_CITIZEN_ONADD` off and the crash was byte-identical (same stack, same fault).
It was *exposed* by `BANISHED_SEED_FUEL=0`, the first config that let citizens get far
enough down the cold/elder path.

```
Fault: EXEC of 0000000000000000
VCALL-of-0: this=…fedb0  vtable=0000018500000000
```

⚠ **The `vtable = 0x0000018500000000` reading was a red herring, and the dumper handed it
to us.** It looked like the zeroed-low-dword `Time`-ctor signature, and it was not. The VEH
handler *assumes* an EXEC-of-0 is a virtual call and prints `[RCX]` as "vtable" — but RCX
here was a **stack buffer** (a float local), so the "vtable" was just two adjacent stack
floats. A genuine vcall has a heap `this`. **Check that RCX looks like an object before
believing the VCALL-of-0 line**; when it is stack-shaped, the fault is a call through a null
static function pointer, and the thing to do is find which `EnsureFns()` binding failed —
the `NOT IMPLEMENTED` lines immediately above the crash name them.

Symbolized stack (image base `0x140000000`, `nm` against `standalone/build/reimpl-x64.exe`):

```
GameUpdateLoop_Update
  EntityList_Update
    CitizenAIComponent_Update
      CitizenComp_CitizenAIComponent_TransitionState
        CitizenAI_ValidateAndEnterState
          CitizenAI_State_ElderTransition            +0x24f
            CitizenAI_PerformWorkCtor8_bd790         +0x6e
              WorkBase_TaskListCleanup_c5400         +0x60
                Pf_SpillAllStorage_7570              +0x102
                  Pf_SpawnRawMaterialOnCell_78c0     +0x141
                    Pf_FindPlacementCell_bd40        +0x3d0   <- VCALL of 0
```

i.e. an ageing citizen transitions to Elder, its task list is cleaned up, its carried
storage is spilled onto the ground, and the cell-placement helper calls through a null
vtable. Repro (crashes within a couple of minutes):

```
BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_FORCE_SPEED=40 \
BANISHED_LIVE_CITIZEN_FAMILY=1 BANISHED_BUILD_DEMO_HOUSE=1 BANISHED_BUILDER_AUTOPOOL=1 \
BANISHED_LIVE_WPC_CONTROLLER=1 BANISHED_SEED_FUEL=0 BANISHED_SEED_FOOD=8000 \
BANISHED_CITIZENAI_TRACE=1 ./reimpl-x64.exe
```

Note the `Pf_*` prefixes on the top three frames are almost certainly misnomers inherited
from `action_pf_roots_drop.cpp` — check what those functions actually do before trusting
the names, per this codebase's standing record on that family.

## D. ✅ CLOSED (2026-07-26) — the `f8400` miss is retail's own no-op

Repro'd with the upgrade harness: at the moment of the miss the id-map is **empty**
(`count=0`) — the work was claimed and erased long before the re-key. And that is
faithful: `WorkComponent+0x28` is written ONLY by the ctor (`0x14012e820`, seeds -1) and
the post; **retail never resets it when work completes**, so retail calls `f8400` at the
edf0/f670 completion site with the same stale id and early-outs the same way. The
`sched+0x30` lookup IS the engine's liveness test (`ead0` exists solely for it). The
misleading trace wording is fixed.

The two side threads both resolved:
* `prio` is a queue-order key, dead unless `param_4 != 0`. Retail has exactly TWO callers
  of the direct insert — `FUN_14009c960` (passes a dead `-1`) and the `ec60` wrapper — and
  ALL of our posts route through `ec60`. The old "most posts bypass the wrapper" reading
  was wrong.
* `sched+0x120` is keyed by **profession** (work vtable `+0x50` returns the Laborer key —
  `Pickup_Slot10` 0x1400f3a50, `GatherRawMaterials_Slot10` 0x1400ec510). Comments fixed;
  that tree's node size is 0x38, not 0x30.

**Real bug found by the hunt, fixed**: `ActCt_TaskCleanup_8e30` searched the `sched+0x10`
order tree with INVERTED child offsets (the 0x28-node tree is less@+0x10/greater@+0x18;
the 0x30-node trees are left@+0x18/right@+0x20). Order nodes were never erased, so
`sched+0x18` grew forever and re-posted work got steadily earlier queue positions — also
the best explanation for the old "prio=0 twice while the id went 1→2" anomaly.

## E. ✅ DONE (2026-07-26) — WorkPlaceComponent is a live controller

Implemented under `BANISHED_LIVE_WPC_CONTROLLER` (opt-in until the graduation note below):
the retail table verbatim (state 0 enter=ee40→f520 "become a Builder workplace" →{1,3};
state 2 enter=edf0 →{3}; state 3 enter=ed60 →{6,4,5}), GetDefaultState=0 (`0x140003730`),
CreateState=node@+0x28 (`0x140004080`), ctor body replayed (back-ptr +0x40=self,
**accepting +0x50 defaults 1** per ctor 0x14012f2e0). Both hooks stand down under the
flag; the two spawn-site blocks are consolidated into `StartTown_ActivateWorkplace`
(seat key + SetAccepting always — the spawn replay; registration deferred to the state-2
enter except for the lodge, which needs the BST queryable before the first tick for
EmployForesters; `impl_95a0` dedups so both are safe).

Verified live: (a) standing soak — identical census to baseline, all 7 registrations via
the SM; (b) upgrade harness — the demo house's state-2 enter fired through the SM with the
builder key + 3 seated builders (the retired State8 hook's exact job), demolish → f520
re-seat → UPGRADE SPAWN all clean; (c) the previously-unexercised founding-employer path —
ForesterLodge activate → FindAvailableBuilding MATCH → 4/4 foresters seated.

Two lazy-link guards were needed (see F): the 0x00/0xff sentinel test, and "a live
controller whose default is 0 does not claim the default" (WPC's 0 must not block Build's
1 on a foundation).

⚠ Noted, not changed: the **lodge spawner does not stamp `+0x52=2`** (only
SpawnStructures stamps), so a spawned lodge's SM takes Build's default 1 and idles there —
WPC has no state-1 entry, so it stays dormant on the lodge exactly as before the
promotion. Faithfulness follow-up: stamp spawned-prebuilt employers to 2 so their WPC
state-2 enter runs like the structures'.

## F. ✅ CLOSED (2026-07-26) — audited every writer; the fix's premise held but its sentinel was wrong

Full writer enumeration (byte-pattern sweep + decompiles): the ctor writes 0x00, the
linker is the link itself, retail's Serialize restores saved state but **retail's linker
only ever runs on a freshly-constructed component** (ComponentEntity::Serialize calls the
linker slot only for save-absent description components), so retail never faces the
saved-state-vs-default choice — the premise stands. Standalone has no save/load at all.

The real finding: `!= 0` also honoured **0xff**, which is `StateMachine::Tick`'s "no
pending" — and two registered paths (`ConsumeProduceAction_helper_b310`'s priming Tick,
`StateMachine_impl_dfa0`'s inline copy) can write it onto a NOT-YET-LINKED chain. The old
code recovered on the next link pass; the fix made it permanent dormancy. Now
`pending != 0x00 && pending != 0xff`. State 0 is a real state (WPC's table registers it)
but is reachable only via GetDefaultState, never via a pre-stamp — there is deliberately
no way to express "I want state 0" through a stamp. The dead faithful-linker port in
statemachinecomponent.cpp is marked never-register.

## G. ✅ ANSWERED for the testable half (2026-07-26) — mass-death teardown is NOT the mechanism

New harness mode `BANISHED_TOOL_HARNESS=housepanel` opens a founding-house info dialog via
the EditTool select path and leaves it refreshing every frame (`BANISHED_HOUSEPANEL_INDEX`
picks the house). Ran it with `BANISHED_KILL_CITIZENS=24` — the whole town died under the
open panel, the roster tore down live, Slot7 repopulated throughout: **no crash, no guard
trip**. Mass-death teardown is ruled out. What remains untestable is a household grown
past the row table by BIRTHS — still blocked on open A; re-run housepanel after the first
birth. The row-bound guard stays.

## H. ✅ PORTED opt-in (2026-07-26) — retail founding spawn behind `BANISHED_LIVE_FOUNDING_SPAWN`

`founding_spawn.cpp` is the faithful sub_1040 port (all 8 steps, retail RNG draw order,
educated forced 0 for children, `DAT_140172114`=0.5 kept as-is) + the impl_0dd0 age draws;
starttown's family loop branches on the flag (ON = retail path only, links unconditional
as impl_0dd0 has them; OFF = byte-identical hand-rolled path). The float-age typedef trap
is defused at every declaration — `FUN_140117080`'s 5th arg is `float` now, stored as a
float to citizen+0x3c. A/B: standing run + the flag (drop `LIVE_CITIZEN_FAMILY`, the
retail path links by itself).

**First A/B result (2026-07-26)** — the path works: real varied ages (adults 2.47–3.83
against `adultAge` 2.00 and the range `[desc+0x50 2.20, desc+0x54*0.5 = 4.00)`, child
0.49), genders alternating 1/0 per couple as `(param_5 == 0)` dictates, jittered
positions, `educated` forced 0 for children, laborer profession seeded, all six spouse
links + ring joins made with **no `LIVE_CITIZEN_FAMILY`**, houses registered, no crash.

**Colonists now have real ages that ADVANCE.** Census at frame 3000: 15 citizens,
`spouse=12` (all six couples), `ring=6`, ages 0.77–4.39. At frame 6000: ages 1.33–4.94 and
**12 adults / 3 children became 13 / 2** — a child grew up and turned `ELIGIBLE=1`. Under
the hand-rolled path every colonist was stuck at age 0 (the deferred init was guarded
off), so this is the first run where the cohort ages at all. Stable, no deaths, no crash
through frame 6000.

⚠ **The cohort is 15, not 24.** The retail path consumes different RNG draws (one
`RandFloat` age per colonist + two `RandFloatTol` for the jitter) before each
`Rand(maxChildren+1)`, so the child rolls land differently: kids were `0,0,0,1,1,1`
instead of two per family. That is a consequence of matching retail's draw ORDER, not a
defect — but the standing soak recipe's "24 citizens / 12 dependants" baseline does not
carry across the flag, so **A/B census numbers are only comparable within one setting.**
Whether vanilla really produces so few children is unverified; the child count is
`Rand(family._maxChildCount + 1)` in both paths, so a low roll streak is plausible at
this seed. Re-check with several seeds before treating it as evidence about anything.

Watch item, **UNVERIFIED — needs a human eyeball, logs cannot answer it**: 117080 runs
SetupYoung/Elder BEFORE the EntityList register, whereas the hand-rolled path ran its
colour/scale pass after, because `citizen+0x278 -> +0x68` is only wired once OnAdded has
run. Ages advancing in the census suggests `CheckAge` (`0x14010f890`, ported and live) is
self-healing it, but that is inference, not observation. If children render adult-sized or
untinted **and stay that way**, that ordering is the cause; the fix is a second idempotent
SetupYoung/Elder call after the register (no RNG, so it cannot shift the stream).

## I. ✅ DONE (2026-07-26) — all 7 renames applied and saved

Each verified by decompile first. One name corrected en route: `0x140003110` is
**`DynArray8_ResizeFill`** (resize-with-fill-value; append is the caller's count+1 idiom),
not an append — and `0x140002cc0` is `DynArray8_EraseByValue` (order-preserving shift, all
matches). TownManager ctor identification is now decisive (stores into
GameUpdateLoop[0x1e] = gw+0xF0; seeds the 21-entry town-resource-limit table). ⚠ Loose
end: `MainMenuDialog_impl_6b00`'s plate comment still says "TradeComponent" at step 10
(GhidraMCP build lacks get_plate_comment; a dated correction sits at call site
0x140096e75) — and every other `TradeComponent_*` name in that family (e.g.
`TradeComponent_impl_f760_sub_7640`, operates on townMgr+0x140) inherits the bad
assumption and should be re-checked before being trusted.

---

## Traps the A/B wave paid for (2026-07-26)

* **Verify every `ResolveExport` string against `objdump`** — the CitizenList-2.9ms lesson,
  again, and this time it was a hard crash rather than a slowdown. `?Abs@Scalar@Math@@SA…`
  (static) does not exist; the real export is `…@@QEBA?AV12@XZ` (const member). A wrong
  string yields NULL, and a NULL that is only reached on a deep path looks like a stable
  build until traffic on that path rises.
* **`VCALL-of-0` in the crash dump is a guess, not a fact.** The handler prints `[RCX]` as
  "vtable" for any EXEC-of-0. If RCX is stack-shaped, it is a null *static function pointer*,
  not a vtable — and the `NOT IMPLEMENTED` lines printed just above the crash name exactly
  which bindings failed. Reading it as a vtable sent this session looking for a
  `Time`-ctor clobber that was not there.
* **A latent bug surfaces where the traffic goes.** Wiring OnAdd did not create the
  `ScalarAbs` crash; it created *citizens who do drop-offs*. When a change makes something
  crash "for the first time", separate "caused" from "exposed" before reverting — the A/B is
  cheap and the answer changes what you fix.

* **"Declared, null-checked, never assigned" is a repeat offender in
  `citizencomponent.cpp`** — `s_fn_FindPartner` once, `s_fn_RandFloat` now. The idiom
  `s_fn_X ? s_fn_X(...) : <constant>` turns an unresolved binding into working-looking code
  with plausible output, so it never crashes and never logs. Grep for any `s_fn_*` static
  that appears in exactly two places (its declaration and its use) — that is the signature.
* **The rare branch of a roll is not "the failure branch".** Read which function each side
  calls before naming either. Here the *common* branch was the birth and the *rare* one was
  a death, and the inherited names asserted the opposite — which is what made "zero births"
  look like a gate problem instead of a bookkeeping problem.
* **A monotonicity check identifies a probability's meaning.** `chance` falling from 0.0250
  to 0.0063 as health rose 0→10 is only sensible for mortality, never for fertility. When a
  formula's constants are nameless, ask which reading makes the slope sensible.
* **A ported, registered function can still be dead.** `CitizenComponent::OnAdd` was
  correct, registered at its exe address, and reached by nothing, because its *vtable slot*
  was never filled. `RegisterExeAddr` presence is not evidence a function runs; the
  hand-rolled workaround built to compensate is the thing that proves it doesn't.

## Traps the D–I wave paid for (2026-07-26)

* **`System::Time::Time(void*, float)` writes 8 bytes.** A port that models retail's
  reused stack slot as an `int32_t` local clobbers the neighbouring local — the dfa0 pump
  zeroed the SMC pointer's low dword (`0x2b900000000`, fault at +0x52) and killed every
  upgrade click. Fleet-audited all ~126 other call sites: that was the only one. The
  signature to recognize: a valid-looking pointer with a zeroed low dword. And the crash
  survived an A/B with the new feature off — **A/B before debugging the new feature.**
* **Dispatch-slot bindings drift.** Two wrong slots found dormant (`0x140152968` bound as
  "Time ctor" is Geometry::Attach; `0x140152970` as "Vector4 ctor" is
  GetNodeIndexFromName) plus one live (`0x140152a80` "Time" is SoundEffect::_typeInfo).
  ~20 independent local typedefs alias the one Time ctor — the alias sprawl is the root
  risk; consider a shared `MakeTime(float)->int64_t` to make the bug unrepresentable.
* **Two BST node layouts.** The 0x28-node order tree (sched+0x10) is parent@+0x08,
  less@+0x10, greater@+0x18; the 0x30-node trees (id map, category buckets, profession
  inners) are left@+0x18/right@+0x20. Check which tree before trusting offsets.
* **A guard sentinel must reject ALL sentinel values.** `+0x52 != 0` honoured Tick's 0xff
  "no pending" and turned a transient into permanent dormancy.

## Traps this wave paid for

* **Ghidra's OnCreate/OnRemove slot labels lie for this component family.** Check the
  operation, not the name: `FUN_140002cc0` = erase-by-value, `FUN_140003110` = append. A
  "registration" that calls `002cc0` is a teardown.
* **A pre-built entity is not a spawned entity.** Anything retail does at *build completion*
  (residence registration, workplace re-keying, delivered-material bookkeeping) has to be
  replayed explicitly for a start-town structure, because it never ran a construction.
* **Lazy initialisation reorders retail's writes.** The `+0x52` bug is exactly this: a write
  that is safe when it happens at creation time destroys intent when it happens later.
* `standalone_crash.log` is created **empty** at startup — poll with `-s`, not existence.
* A quiet log is not a hung game (RTSS's frame limiter blocks in Present). Read state, not
  stacks: `p *(int*)(<townMgr>+0xa8)` — 0 means the town is extinct.
