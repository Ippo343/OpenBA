# Next-session brief — after the town-registry and D–I waves

Rewritten 2026-07-26 (commits `07155bb`, `255d5e4`), then updated the same day after the
**D–I wave**. Run data behind every claim is from the standard town-health run (below) on
branch `wave/upgrade-path-complete`.

**Jump to [Investigative opens for next session](#-investigative-opens-for-next-session)**
for the A–I list. **A, B and C are the only opens left** — the town is still *alive but
frozen* (24 citizens, 7 couples, 0 births, 0 move-outs) and those three are the separate
reasons it cannot grow. D–I are all closed or landed; their entries below now record the
outcome rather than the question.

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
