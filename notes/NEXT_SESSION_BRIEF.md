# Next-session brief — after the town-registry wave

Rewritten 2026-07-26 (commits `07155bb`, `255d5e4`). The previous version of this file
listed five opens; this records what closed, what turned out to be a misreading, and what is
left. Run data behind every claim is from the standard town-health run (below) on the
**release** build, branch `wave/upgrade-path-complete`.

**Jump to [Investigative opens for next session](#-investigative-opens-for-next-session)**
for the A–I list. Headline going in: the town is now **alive but frozen** — 24 citizens,
7 couples, 0 births, 0 move-outs — and opens A/B/C are the three separate reasons it cannot
grow.

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

Ordered by what they unblock. A–C are one thread: the town is now **alive but frozen** —
24 citizens, 7 couples, zero births, zero move-outs — and each of the three is a separate
reason it cannot grow. Everything below is a *question* except where marked TASK.

## A. Get exactly one birth to happen

Three of six founding households are full at spawn (`desc+0x24 == 5`), but three have room
for two each and still produced nothing in 12 000 frames at `FORCE_SPEED=40`. Walk the gate
in `CitizenComponent::Update` in order and log which test fails, on a known non-full house:

1. `citizen+0x38 == 0` (female) — census says 12 female, fine.
2. `+0x208 == 0` (not a student).
3. `desc+0x44 <= age` (adult).
4. The hunger/thirst reset: `isHungryOrThirsty` **zeroes `+0x4C` every tick it is true**.
   With hunger running 5–12 against a `desc+0xCC` warn threshold this is the first suspect —
   if the warn point is below the citizens' steady-state hunger, the timer can never survive
   long enough to expire. **Check `desc+0xCC/+0xD0` against the observed hunger first.**
5. The seed itself: `+0x250 != 0 && +0x238 != 0 && +0x4C == 0` and age within
   `desc+0x50..+0x54`, then `!helper_a4f0`.

Only after one birth is observed does the cooldown magnitude (`desc+0x58`, and the negative
reseed) mean anything. **Do not tune the rate before then.**

## B. Can a town with no spare housing do anything but stall?

`noGuardian=0` at frame 12 000 with 10 citizens over marry age: every grown child is stuck
because `sub_0e70` finds no free house, which is faithful — in retail the player builds more.
Questions: does a grown child that cannot move out take any other action (retail must not
just spin), and is there an immigration/nomad path that is the intended growth vector for an
un-played town? If the answer is "the player is the growth vector", say so in the notes and
stop treating a frozen population as a bug.

## C. The woodcutter economy — the real fix behind two stand-ins

`BANISHED_SEED_FUEL` and `BANISHED_SEED_FOOD` are both papering over the same hole. Note the
heat tick burns **continuously** (map temperature 29.98 is already below the house's
`desc+0xF4` of 50), so a producer must sustain roughly one unit per house per 60 in-game
seconds year-round. Scope the smallest real producer + haul that closes it.

## D. Why does `f8400` miss, now that the id-map is known to populate?

The re-key's `NOT in the sched+0x30 id-map` early-out is real but its cause is unknown. The
root changes between inserts while the count stays 1, which suggests the item is claimed and
erased before the re-key runs. Repro with the upgrade harness
(`BANISHED_TOOL_HARNESS=upgrade …`, see [[reference-harness-env-gotchas]]) and log the map at
the moment of the miss, not just at insert.

Two smaller threads in the same file:
* Is `prio=0` on the direct-insert callers (`0x1400f88a0`) faithful? Only the `ec60` wrapper
  passes `workComp+0x2c`. Check what retail's direct callers pass.
* `Insert`'s comment calls `sched+0x120` a "per-work-entity tree" while `f8400` keys the same
  tree by PROFESSION (`wpc+0x48`). Both hold only if the work's vtable slot `+0x50` returns
  the profession key. Confirm and fix the wrong comment.

## E. Promote WorkPlaceComponent to a live controller (TASK)

Its state table is known: `helper_f3a0` (`0x14012f3a0`) → state 0 `ee40`, state 2 `edf0`,
state 3 `ed60`. Doing this retires **two** hooks that currently stand in for the missing
controller node: the call from `BuildComponent_State8Finalize`, and the block this session
added to `StartTown_SpawnStructures`. Watch for double-registration while both exist —
`ResidenceComponent_impl_95a0` dedups, but `f670` does not.

⚠ The StartTown block added this session is **currently unexercised in a default run**: the
start town has no employer buildings (a stockpile and a barn, neither with a profession), so
nothing takes that path. It needs a run with a founding employer before it can be called
verified. The ForesterLodge spawner does the same three steps inline — consolidate.

## F. Blast radius of the `+0x52` change (TASK)

`gotDefault = (*(base + 0x52) != 0)` changes a default path. It can only bite an entity whose
`+0x52` is already non-zero at link time, which today means only start-town stamped
structures — but that is an argument, not a measurement. Enumerate what else writes `+0x52`
before the first Update (the load/save path is the obvious candidate) and confirm none of
them wants the controller default to win.

## G. Is the `ResidenceUI_Slot7+0x295` crash a row overflow at all?

Still unexplained. The guard has never fired, but the autostart soak **never opens a
residence panel**, so that is not evidence. Open a house panel in a run that has had births
and deaths. With `desc+0x24 == 5` and the founding pass respecting it, mass-death teardown
now looks like the better explanation.

## H. Retire the hand-rolled founding spawn (TASK — carried over)

Not restated here: **section 4 above** has the full port map for `sub_1040` (`0x1400a1040`)
and `impl_0dd0`, and the float-vs-int typedef trap on `FUN_140117080`'s age argument. It is a
replacement for `SpawnOneCitizen`, not an addition. Worth doing *after* A, so the birth path
is understood before the spawn path changes underneath it.

## I. Ghidra hygiene (TASK)

Rename what cost this session a detour, so it is not re-derived:
`0x14012a0e0` → `ResidenceComponent_OnRemove`, `0x14012f090` → `WorkPlaceComponent_OnRemove`,
`0x140129de0` → `ResidenceComponent_EnterBuiltState`, `0x14012a4f0` →
`ResidenceComponent_IsHouseholdFull`, `0x140002cc0` → `DynArray_EraseByValue`,
`0x140003110` → `DynArray_Append`. Also fix the `TradeComponent` mislabel on the TownManager
ctor `0x140090a30`.

---

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
