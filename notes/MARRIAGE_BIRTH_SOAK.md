# Marriage / birth-rate soak — 2026-07-25

Closes the ⭐ item deferred from `notes/CITIZEN_ONREMOVE_BRIEF.md` OPEN 5 ("the marriage /
birth-rate soak"). Branch `wave/upgrade-path-complete`. All runs are the **release** build
(`make standalone`) — `printf` is stripped there but `fprintf(stderr, …)` is not, so every
diagnostic below still lands in `standalone_error.log` while the game runs ~10× faster than
the `-O0` debug build. That matters: at debug speed a soak never reaches the first birth.

## Answer to the question that was asked

**Marriages work.** With gender real, the marriage block links spouses correctly, the new
spouse leaves their parents' household, and both are re-homed — no crash, no guard trip.

**But no standalone citizen can reach that block on its own**, and that has nothing to do
with gender or with marriage. See "Why the natural path can't fire" below.

Two crashes stood between the old "nothing happened in 36k frames" result and any real
data. Both are fixed; both were on the DEFAULT path (no flags), and the first one meant the
game had never survived a single in-game birth.

## What was measured

Census tooling: `BANISHED_FAMILY_CENSUS=<f>` now also reports, per sample, the four
marriage gates separately, the adult/child split with the description's own `adultAge` /
`marryAge`, the population delta since the last sample, and the town-manager registries.

Marriage harness: `BANISHED_MARRY_HARNESS=<n>` (see below). Finder trace:
`BANISHED_MARRY_TRACE=1` logs each `helper_fe40` call with the caller's gender/age/window
and whether a partner came back.

Run: `BANISHED_AUTOSTART=1 BANISHED_SEED=12345 BANISHED_FORCE_SPEED=40
BANISHED_LIVE_CITIZEN_FAMILY=1 BANISHED_FAMILY_CENSUS=1000 BANISHED_MARRY_TRACE=1
BANISHED_MARRY_HARNESS=8 ./reimpl-x64.exe`

```
[MarryHarness] frame 3000: moved out comp=…de40f68 age=2.01 gender=0 — guardian cleared, home …de00f70 -> …de00f70
[MarryHarness] frame 3000: moved out comp=…de46370 age=2.09 gender=0 — guardian cleared, home …de0abd0 -> …de0abd0
[MarryHarness] frame 3000: moved out comp=…de527e8 age=2.01 gender=1 — guardian cleared, home …de0ae50 -> …de0ae50
[Marry] call 1 comp=…de527e8 gender=1 age=2.01 marryAge=2.00 window=4.00 -> partner=…de40f68 (hits=1)
[Marry] call 8 comp=…de46370 gender=0 age=2.14 marryAge=2.00 window=4.00 -> partner=…de54908 (hits=2)
[FamilyCensus] frame 4000: 24 citizens — spouse(+0x250)=16 … host(+0x258)=8 … female=12 male=12
```

Founding spouse links are 12 (6 couples), so `spouse=16` is **two new marriages**, one
found by a male and one by a female — the opposite-gender test passes in both directions,
which is exactly what the all-female cohort could never do. `host` drops 12 → 8 because the
marriage block moves the chosen partner out of their own household too
(`sub_1fa0(partner, null)`), just as retail does.

## Why the natural path can't fire (the real blocker)

Retail's marriage gate (verified in the `CitizenComponent::Update` tail at `0x140113827`):

```
+0x258 (guardian) == 0  &&  +0x238 (home) != 0  &&  +0x250 (spouse) == 0
                        &&  age (+0x3c) >= desc+0x48 (marryAge)
```

A citizen born into a family clears the FIRST of those only by finding a home of its own:
`Update -> sub_0e70 (0x140110e70) -> sub_1fa0(self, null)`, then it is re-homed by
`AssignWorkplace` in the same tick. `sub_0e70` searches the town building registry at
`townMgr+0x20/+0x28` and the profession/category BST at `townMgr+0x100`. The census now
prints both, and in every sample of every run:

```
[FamilyCensus]  townMgr=… buildingRegistry(+0x20)=0000000000000000 count(+0x28)=0
                profBST(+0x100)=0000000000000000 bldgCount(+0x68)=0
```

Both registries are **empty** — they are filled by `WorkPlaceComponent::OnCreate` in retail
and nothing populates them in the standalone start town (`starttown.cpp` says as much for
`+0x20`, and the founding housing pass works around it with its own captured-house list).
So no standalone citizen ever moves out, `+0x258` never clears, and the marriage block is
unreachable. `ELIGIBLE=0` in every census sample is that, and only that.

**Therefore: the previous negative soak result was doubly uninformative** — the cohort was
all-female *and* the gate was unreachable. The gate is still unreachable; what changed is
that the code behind it is now proven to work when it is reached.

Related: `hasHome=N` equals the unmarried count in every sample because the founding pass
rosters every family member into the family house. That is a standalone stand-in, not
retail behaviour, and it is what breaks the ResidenceUI invariant below.

## The marriage harness (`BANISHED_MARRY_HARNESS=<n>`, default off)

`standalone/death_harness.cpp`. Every 600 frames, until `n` citizens have been moved, it
takes an unmarried citizen that is old enough (`age >= desc+0x48`) and still a dependant,
and performs the move-out **the way retail does**:

1. `CitizenComp_sub_1fa0(citizen, nullIBP)` — the exact call `Update` and the marriage
   block itself make. This detaches from the guardian's ring AND drops the citizen out of
   the house (`+0x238` goes to 0 — a homeless citizen still fails the gate).
2. `CitizenComponent_AssignWorkplace(citizen, &oldHome, slot)` — re-home, exactly as
   `Update` does one line later with whatever house `sub_0e70` found. The harness reuses the
   house the citizen already lived in, so the residence roster count is unchanged.

It never touches `+0x250` or the age gate. Everything after step 2 is the game's own code.
This is to marriage what `BANISHED_KILL_CITIZENS` is to `CitizenComponent::OnRemove`.

## Two crashes fixed (both default-path)

### 1. Every in-game birth crashed the game

`CitizenComp_helper_1b90` (the childbirth path) finds the newborn's `CitizenComponent`
through the `.rdata` type-id global `DAT_1401c7158`. That global is only filled behind the
opt-in `BANISHED_LIVE_CITIZENAI_CITID`/`_TIDS` gate, so on a default run it reads 0, the
find returns null, and `CitizenComp_DeferredInit_117080` was called with a **null `this`**:
`ACCESS_VIOLATION` reading `0x10` at `DeferredInit+0x87`, on the very first birth
(~frame 5000 of the first release-build soak).

Fix (`citizencomponent.cpp`): fall back to the self-contained name→CRC lookup
(`Object::GetTypeIdByName("CitizenComponent")`) — the same pattern
`CitizenComp_FindSiblingByName` uses — instead of filling the shared global, which would
un-dormant ~30 unrelated readers. Plus a null guard that aborts the birth with a
`[Birth] ABORT` line rather than faulting. Retail was checked: `helper_1b90` really does
key on `g_BaseTypeInfo_CitizenComponent`, so the fallback is faithful in effect.

**This is why "births" had never been observed before.** They were never surviving.

### 2. `ResidenceUI::Slot7` walked off its row array

Second soak died at `ResidenceUI_Slot7+0x295` once births started. Retail indexes the row
array at `controller+0x30` by resident index with **no bound** — its invariant is that a
residence can never hold more residents than the dialog table has rows. Standalone breaks
that invariant: the founding housing pass rosters a whole family into `slot[0]` regardless
of the `ResidenceDescription` capacity, and births then push the house past the row count.

Fix (`residenceui.cpp`): stop at the last real row and log `[ResidenceUI] GUARD:` once.
Declared deviation. The real fix is capacity-bounded residence assignment — see "Still
open". Note the guard has **not yet been observed to fire**: the run that crashed had 6
births, the runs since have had fewer, so the row overflow is the best-supported
explanation for that stack, not a proven one.

## Birth cooldown was decoded wrong (birth-rate bug)

After a birth, retail sets the happiness/reproduction timer at `+0x4c` with

```
14011399c: XORPS XMM0, xmmword ptr [0x1401722c0]      ; DAT_1401722c0 = 80000000 x4
```

i.e. `+0x4c = -(rand * desc+0x60 + desc+0x5c)` — a NEGATIVE float that the
`fVar14 < 0` branch counts back up toward 0. Ghidra renders that as
`(uint)(…) ^ DAT_1401722c0`, and this port had taken it literally: a numeric float→int
truncation, then flipping bit 31 of the truncated integer. `12.7` became `0x0000000C`,
which with the sign bit set reads as `-1.7e-44` — a cooldown that expires on the very next
tick. Fixed to flip the sign bit of the float's bits.

Not yet A/B'd for magnitude; the fix is justified by the instruction, not by a measurement.

## The soak's hard ceiling: the town freezes to death in the first winter

Every long run ends the same way, at the same sim time (~frame 16000–18000 at
`FORCE_SPEED=10`, seed 12345): the entire population dies within ~2000 frames. The census
now carries the survival numbers that show it:

```
frame 10000  hunger avg=3.4 max=12.0 (death 25.0)   thirst avg=0.0  max=0.0  (death 20.0)
frame 14000  hunger avg=8.2 max=10.6                thirst avg=5.6  max=5.6
frame 16000  hunger avg=6.0 max=8.8                 thirst avg=14.5 max=14.5
frame 18000  NO CITIZENS
```

Hunger never goes near its threshold — the seeded house food holds. The killer is the
`+0x48` need, which `Update` gates on `desc+0xF4` vs the weather temperature at
`weatherData+0x18`: it is pinned at 0 while the world is warm and starts climbing when the
temperature falls. That is Banished's **warmth/firewood** need, and the standalone town has
no firewood economy, so the first winter kills everyone at once.

Two things worth noting about that die-off:

* `CitizenComponent::OnRemove` handled 24 simultaneous deaths **cleanly** — the
  `[CitizenOnRemove]` trace shows the town list draining 24 → 23 → 22 → … → 0 with no
  crash, no guard trip, including citizens with live spouse and ring links. That is a
  stronger test than the death harness's 5-citizen kill.
* It also means the earlier `ResidenceUI_Slot7` crash has a second candidate cause (a
  residence roster being torn down under the dialog during the die-off) beside the row
  overflow the guard addresses. The guard has **not** been observed to fire, so treat the
  row-overflow explanation as unconfirmed.

To soak past the first winter, the town needs firewood in the houses (or a pinned-warm
weather override). Until then ~16k frames at speed 10 is all the run time there is.

## Still open

* **The town building registries (`townMgr+0x20/+0x28`, `+0x100`) are empty.** This is the
  one thing standing between the current build and a self-sustaining population: no citizen
  can find a home or a job of its own, so nobody moves out, marries, or (naturally) changes
  workplace. It is the same `WorkPlaceComponent::OnCreate` registry gap `starttown.cpp`
  already documents for the founding housing pass.
* **Residence assignment ignores house capacity** (founding pass puts a whole family in
  `slot[0]`), which is what the new ResidenceUI row guard papers over.
* **Birth-rate magnitude** with the corrected cooldown is unmeasured, and cannot be measured
  properly until the winter die-off is solved: the usable window is one pre-winter season,
  in which the observed rate is ~4–6 births from 6 founding couples (seed-dependent) and
  then nothing — the surviving mothers sit pinned by the storage-capacity branch that sets
  `+0x4c = desc+0x58`. The census prints the population delta per sample, so the number is
  one long warm-weather soak away.
* **No firewood economy** — the town's actual cause of death. Houses are seeded with food
  but not fuel.
* `BANISHED_LIVE_CITIZEN_FAMILY` stays opt-in. It is no longer suspected of being broken —
  the marriage path it enables is now exercised — but population dynamics change with it, so
  it wants the birth-rate number before graduating.

## Method notes worth keeping

* **Soak on the release build.** `printf` is stripped and `fprintf(stderr, …)` survives, so
  diagnostics are intact and the sim runs far enough to reach lifecycle events. The debug
  build wrote 6 MB of per-frame logs in two minutes and never reached frame 2000.
* `BANISHED_FORCE_SPEED` is sim iterations per frame. 40 bought little over 10 in wall-clock
  sim throughput.
* **A quiet log is not a hung game.** When a soak went silent I sampled the main thread with
  gdb three times, found it inside `VideoDX11::ContextData_Finish` → `RTSSHooks64.dll` →
  `nvwgf2umx.dll` every time, and called it a Present-side stall. Wrong: RTSS's frame limiter
  *blocks in Present*, so an idle-but-healthy game samples there every time. The town had
  simply died out, and `Census` bailed silently on a zero-length citizen list. The way to
  settle it is to read the state, not the stack:
  ```
  gdb -p <pid> -batch -ex "p/x **(unsigned long long**)&g_pGameWorldSingleton"
  gdb -p <pid> -batch -ex "p *(int*)(<townMgr> + 0xa8)"     # 0 = extinct town
  ```
  The census now prints `NO CITIZENS` instead of printing nothing.
* `standalone_crash.log` is created **empty** at startup. A poll loop must test `-s`, not
  existence, or it reports a crash on every run.
* A census that reports a negative ("ELIGIBLE=0", "no marriages") is worthless unless it
  also reports the state of the mechanism that was supposed to produce the positive.
