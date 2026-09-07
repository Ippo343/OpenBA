# Wave 5 — Algorithm packages (Achievement + Save + Picking)

In progress on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.

## Scope correction, stated up front

PLAN §4.2 lists Wave 5 as "Collision2D, Picking, Pathfinding, Achievement, Save". The census
reports **Collision2D 0 and Pathfinding 0 called** — our tree never resolves a single function in
either namespace, despite the DLL holding 76 and 45 of them. Lane E is establishing whether that
zero is true or a measurement artifact before the wave claims those two namespaces as "done".

The wave's real scope is therefore the **69 functions our tree actually calls**:
Achievement 41, Save 20, Picking 8.

| Lane | Scope | File |
|---|---|---|
| A | `Achievement::Goal` / `GoalInstance` / `GoalList` (20) | `runtime/src/achievement/goal.cpp` |
| B | `Achievement` / `AchievementInstance` / `AchievementManager` (21) | `runtime/src/achievement/achievement_manager.cpp` |
| C | `Save::SaveLoad` / `Save::SavedState` (19) | `runtime/src/save/saveload.cpp` |
| D | `Picking::Scene` / `Picking::PickingMesh` (8) | `runtime/src/picking/scene.cpp` |
| E | Scope + census/audit tooling (no port code) | `tools/runtime_census.py`, `tools/export_name_audit.py` |

Kill switches: `BANISHED_RT_ACHIEVEMENT` / `_GOAL` / `_ACHMGR`, `BANISHED_RT_SAVE` / `_SAVELOAD`,
`BANISHED_RT_PICKING` / `_PICKSCENE`.

## Pre-wave controls (measured 2026-09-06, commit 1a86921 — scaffolding only, 0 Wave 5 ports)

Taken BEFORE any lane registered anything, so a post-wave comparison means something. Both on the
`-O0` `standalone-debug` exe.

| Run | Result |
|---|---|
| `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6` | **76 sites, 24,752,689 calls, 101 divergences in 2 sites.** Clean exit, `standalone_crash.log` empty. |
| `BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1` + the same self-quit | **83 sites, 29,638,857 calls, 54 divergences in 3 sites.** World built (`[GUL::Load] EXIT — world built`), clean exit, `standalone_crash.log` empty. |

The divergent sites are the SAME pre-existing Wave 1 1-ULP family in both runs, and nothing else:
`Math::Transform::FromYawPitchOffset` (44), `Math::Vector3::FromYawPitch` (9) and
`Math::Vector3::Heading` (1) — all sin/cos/atan2-fed under our own libm. Any NEW divergent site
after Wave 5 lands is a Wave 5 defect.

### One harness trap this wave paid for, recorded so the next reader does not

`BANISHED_MODAL_ANSWER=1` **breaks the `BANISHED_AUTOLOAD` path**. Wave 4 lane H made
`SaveLoad_ModalShowDialog` answer the `modsMissing` prompt with **2** (confirm) on any unattended
run, and `BANISHED_MODAL_ANSWER` overrides that in *either* direction — so passing `1`, which is
the value CLAUDE.md's *mods menu* recipe uses for a different dialog, cancels the load. The symptom
is indistinguishable from the stall lane H fixed: the Load click fires, `[AutoStart] AUTOLOAD
sequence fired` is logged, and the process then loops in `MenuDLT::Draw` forever with no world
created. **The correct invocation passes no `BANISHED_MODAL_ANSWER` at all.**

---

## Verification — every run, and what each one actually proved

All on the `-O0` `standalone-debug` exe with `BANISHED_RT_DIFF=1` and a pause-menu self-quit
(`RtDiffSummary` prints from `WinMain`, so a killed process reports nothing).

| # | Run | Sites | Calls | Divergences |
|---|---|---|---|---|
| 0a | control, autostart (pre-wave, `1a86921`) | 76 | 24,752,689 | 101 / 2 sites |
| 0b | control, `AUTOLOAD=Save9` (pre-wave) | 83 | 29,638,857 | 54 / 3 sites |
| 1 | autostart | 91 | 24,050,239 | 99 / 2 sites |
| 2 | `AUTOLOAD=Save9` | 111 | 29,914,569 | 57 / 2 sites |
| 3 | `AUTOSTART + SAVE_TEST=700` | 111 | 28,111,981 | 98 / 2 sites |
| 4 | round-trip: `AUTOLOAD=Auto` on the save run 3 wrote | 104 | 9,952,450 | 67 / 2 sites |
| 5 | autostart + `RT_INTERNAL_SHADOW=1` | 99 | 23,972,823 | 97 / 2 sites |
| 6 | `AUTOTUTORIAL=1` + `RT_INTERNAL_SHADOW=1` | 133 | 5,953,991 | 80 / **3 sites** |
| 7 | autostart + `ACH_TIME_SCALE=800 ACH_YEAR_BOOST=2 ACH_TRACE=600` | 110 | 28,283,271 | 112 / 2 sites |
| 8 | `AUTOTUTORIAL=1` again, after lane D's predicate fix | 133 | 6,226,761 | 69 / 2 sites |
| 9 | `STRESS=1 FRAMES=1200 CITIZENS=800 ACH_YEAR_BOOST=250 ACH_TRACE=400` | 127 | **67,539,645** | 92 / 3 sites |

Every divergence in runs 1-5 and 7 is the pre-existing Wave 1 1-ULP sin/cos family
(`Transform::FromYawPitchOffset`, `Vector3::FromYawPitch`, and in the controls `Vector3::Heading`).
**Run 6 was the exception and it found a real problem; run 8 is the same run with the fix and is
clean.** Every run exited cleanly with an empty `standalone_crash.log`.

### What each run was FOR — because a call count is not coverage

* **Run 4 is the one that settles the Save lane.** Run 3 proved our `Store` agrees with the DLL on
  one buffer; run 4 proved the bytes it wrote are actually a save the game can read back and build a
  world from. For a lane whose warned-about failure mode is a desync surfacing later on an innocent
  record, those are different tests and only the second addresses it.
* **Run 5 is the one that settles the destructor fix.** `Object::Internal::Internal` and
  `~Internal` ran 93,253 calls each through Wave 3's handle-table shadow oracle, 0 divergences,
  with lane A's `~GoalInstance` routed through it. Before this, lane A's base-call helpers used
  `RT_ORIGINAL_ONCE` (GetProcAddress), which bypasses the registry — so `~GoalInstance` would have
  been the ONE path in the tree reaching `~Internal` that the oracle cannot see, and it is exactly
  the path the `863e4f9` fix made live. The swap-remove is now checked against the factory's
  predicted state, not merely observed not to crash.
* **Run 6 reached the Scenario path** and fired lane B's three `scenario.cpp` ports for the first
  time — `Achievement::ctor` agreed on all 0x58 bytes against a 0xCD-poisoned scratch, 4 calls.
  Those three had been registered-but-unreachable (see below).
* **Run 7 exercised the Achievement READ surface** — `IsAchievementAwarded` 108,
  `AchievementManager::Get` 1,862, `GetAchievement`/`GetAchievementInstance`/`GetGoalList` 108 each,
  `Goal::GetText`/`GoalInstance::GetGoal`/`IsMet`/`HasFailed` 402 each, `GoalList::GetGoal` 402.
  It also confirmed lane A's prediction that `GoalInstance::GetGoal` reads `calls=0` without
  `BANISHED_ACH_TRACE`.

### Not verified, stated plainly

* ~~No run has awarded an achievement.~~ **CLOSED by run 9.** Run 7 did not: its census read
  `awarded 6/36` at the first sample and the last, because the time knobs accelerate `TimeGoal`s
  while the achievements they reach are CONJUNCTIONS — `Established` and `Tenure` had their year
  goals met at year 251 and their population goals sitting at `have=23 >= 300`. A fresh autostart
  town has 23 people, so no amount of clock acceleration can award a conjunction whose other half
  needs a real town. Run 9 supplied the town instead of more time: the stress harness's 802
  citizens plus `ACH_YEAR_BOOST=250` awarded **`Settlement` and `Village` during the run**
  (6/36 -> 8/36), so the award branch of `AchievementManager::Update`, its event-handler call and
  its `IsAchievementAwarded` readback all executed, at 1,201 `Update` calls and 0 divergences.
  Credit to lane B for the diagnosis: it predicted the conjunction problem and named the stress
  town as the fix.
* **`Save::SavedState::SavedState` is at `calls=0` in every run.** Its only two callers
  (`moddialog.cpp:722`, `mod_dialog_vtables.cpp:438`) are on the Mods-menu apply/reload path, which
  no run above opens. Lane C's doc claims 14 of 15, not 15.
* **`Save::SaveLoad::Update` cases 3 (`UpdateLoadFromFile`) and 5 (`UpdateDelete`)** are untaken by
  any run. Cases 1, 2 and 4 are covered by runs 2 and 3.
* **`~Goal` is self-check-only end to end** and is recorded that way without qualification: its base
  half reaches Wave 3's `External::~External` port, which has no oracle behind it, so there is
  nothing to appeal to. A self-check that agrees with itself proves nothing about the DLL.
* **`Achievement::~Achievement`** has not appeared in any run; its refcount-delta shadow verdict is
  unexercised. It is the only heap-touching body in lane B, so the ctor's clean 0x58-byte result
  does NOT carry it. Reaching it needs a Scenario *destroyed* — a quit-to-menu from a scenario, or a
  second scenario load — rather than the pause-menu quit every pass above used.
* **`Picking::Scene::GetUserData` and `SetMeshUserData`** are still at `calls=0`. Run 9 closed
  `PickingMesh::Vertices` (2,382 calls) and drove `SetTransform` to 963,202, `Intersect` to 23,274
  and `AddMesh`/`RemoveMesh` to 1,810 each, all 0 divergences — but `GetUserData` needs a click on a
  placed building (the `Intersect`+`GetUserData` pair `EditTool` uses) and `SetMeshUserData` needs a
  bridge. Neither is driven by any harness in these runs.
* **A correction to this lane's own brief:** `mapquery.cpp` was described to lane D as a live
  `Intersect` consumer hit "on essentially every click". Lane D checked: the call is gated on the
  terrain cell's floor counter (`cellRec+0x2d > 0`), so it fires only over a dock or bridge deck,
  and not at all in a town without one. `edittool.cpp` is the real per-click consumer.

### The one new divergent site Wave 5 produced, and what it turned out to be

Run 6 reported `Picking::Scene::RemoveMesh calls=17 divergences=16` — the only new divergent site
in the whole wave. It was **not a defect in the port**, and the way it was settled is worth keeping.

Retail `0x180125290` is exactly the port's two calls with the same arguments, and both callees stay
in the DLL, so the port's post-state IS retail's by construction and a post-state predicate that
disagrees with it must itself be wrong. That narrowed it to one of 14 conjuncts — and the failure
message rendered only 3 of them, so it could not say which.

The answer was found statically, in retail's own bytes. `0x18007b390` builds a zeroed `Entry`
template on its stack with **eleven** stores covering 0x44 of the 0x48 bytes:

```
18007b3af  MOV dword [RSP+0x28],ECX        template +0x00  _queryStamp = 0
18007b3b3  MOV dword [RSP+0x2c],0xffffffff template +0x04  _nextFree  = -1
18007b3bb  MOV qword [R11-0x48],RCX        template +0x08  _cellX/_cellY
18007b3bf  MOV dword [RSP+0x38],ECX        template +0x10  _cellW/_cellH
18007b3cb..d7  four qwords = 0             template +0x18..+0x37
18007b3db  MOV dword [RSP+0x60],ECX        template +0x38  _userValue
18007b3df  MOV qword [R11-0x10],RCX        template +0x40  _callback
```

`R11 = RSP+0x78` and the template base is `R11-0x50` = `RSP+0x28`, so template `+0x3c`
(`Entry::_userValue2`) is at `RSP+0x64` — **and nothing ever writes it.** `0x18007b590` then copies
`+0x3c` faithfully (`MOV EAX,[RBX+0x3c] ; MOV [RDI+0x3c],EAX`).

**So every retail `RemoveMesh` stores an uninitialised stack dword into `Entry::_userValue2`.**
That is a genuine uninitialised read in the shipped DLL. It explains the exact pattern observed —
call #1 passed because the slot happened to hold zero, #2 onward inherited a previous frame's
leftovers. It is unobservable in practice, which is presumably how it shipped: `_userValue2` has no
exported getter (`GetUserValue` reads `+0x38`) and `0x18007b210` overwrites `+0x3c` from the
descriptor before the slot is reused.

The `_userValue2 == 0` term was **dropped, not weakened** — there is no value it could assert.
Every other term stands, including both of the orchestrator's suspects (`_queryStamp == 0` holds;
the `_transforms` triple holds). Run 8 re-ran the same tutorial pass: `RemoveMesh 17 calls,
0 divergences`.

**The standing lesson, which cost this wave two investigations:** a shadow predicate is a verdict
AND a description, computed by different means, and the description is only ever exercised when the
verdict fails — exactly when it cannot afford to be uninformative. If `eq` is a conjunction of N
terms, the message must be able to name all N. Lane D found the same latent flaw in `Intersect`
before it fired (it compared the out normal but printed only the position) and fixed both; all five
Picking shadow sites now report the first failing term by name with its actual and expected values.

---

## Closing the last five gaps: five more runs, four justified holds

The wave was not closeable while five ports sat listed as "not verified". Each was chased to a
run or to a stated reason. Runs 10-14:

| # | Run | Result |
|---|---|---|
| 10 | `TOOL_HARNESS=selectcitizen` | **`Scene::GetUserData` CLOSED** — 1 call, 0 divergences. 108 sites, 27,135,817 calls. |
| 11 | `TOOL_HARNESS=bridge BUILDERS=30 FORCE_SPEED=40` | Bridge built and COMPLETE; `SetMeshUserData` still 0. 114 sites, **160,416,395 calls**, 0 new divergences. `AddMesh`/`RemoveMesh` 95 each, `SetTransform` 100,025, `Intersect` 168 — all clean. |
| 12 | in-game Mods menu | **CRASHED** in `ModDialog_ToggleMod_Impl`. See below. |
| 13 | same, `BANISHED_RT_PORT=0` | **Crashed identically** — pre-existing, not Wave 5. |
| 14 | `AUTOSTART_GAMES=2` | `Achievement::ctor` stayed at 4 across TWO games; `~Achievement` never fired. |

### The four holds, each with the reason that makes it a hold and not an omission

1. **`Picking::Scene::SetMeshUserData` — unexercised, and the harness cannot reach it.**
   Both call sites (`bridgetool_calcpath.cpp:211` `SeatSegmentPickOwner`, `fencedtool_calcpath.cpp:143`
   `SeatPickOwner_1290d0`) guard on `pickComp+0x28 >= 0` and return early otherwise. The function
   *re-points an already-registered* pick mesh; a freshly built entity has no mesh at the moment its
   owner is seated, so the guard fires. Run 11 built a bridge to completion and the call was skipped
   for exactly this reason. The port is registered, the dispatch slot `0x1401529a0` is verified
   correct, and nothing exercises it. **Not claimed as verified.**

2. **`Save::SavedState::SavedState` — blocked by a PRE-EXISTING crash, proven not ours.**
   Its only callers are `moddialog.cpp:722` and `mod_dialog_vtables.cpp:438`, both on the Mods
   apply/reload path. Driving that path (run 12) crashes first, in `ModDialog_ToggleMod_Impl`
   (`moddialog.cpp:561`), `ACCESS_VIOLATION` reading `0xd0`, stack
   `ModDialog_ToggleMod_Impl <- ModDialog_Slot8 <- ModDialog_VOnElementEvent <- PauseMenuHarness_Tick`.
   Run 13 re-ran it with `BANISHED_RT_PORT=0` — **every runtime port disabled** — and got the
   identical fault at the identical offset with the identical `RAX=0xD0`. So it predates this wave
   and is not a Wave 5 defect. `fn_Modification_GetMod` returns null and `mod+0x50` is dereferenced
   unguarded at `moddialog.cpp:561`. **Recorded as a separate open defect; not fixed here** (it is
   outside Wave 5's scope, and CLAUDE.md's rule is to find the upstream null rather than guard the
   deref).

3. **`Achievement::~Achievement` — not reachable by any path this harness can drive.**
   It is called only from `Scenario_Dtor` (`scenario.cpp:110`), which has **no direct caller in our
   tree**: it is installed at Scenario vtable slot +0x00 (`scenario.cpp:289`) and invoked by the DLL
   when the Scenario *resource* is released. Scenario resources are `Object::External`s loaded once
   and held for the process — run 14 confirms it, `Achievement::ctor` reads 4 across ONE game and
   still 4 across TWO — so the dtor runs at process teardown if at all, which is after
   `RtDiffSummary` prints. Its refcount-delta shadow verdict has never executed and the ctor's clean
   0x58-byte result does NOT carry it: different body, different failure mode, and it is the only
   heap-touching body in lane B. **Not claimed as verified.**

4. **`Save::SaveLoad::Update` cases 3 (`UpdateLoadFromFile`) and 5 (`UpdateDelete`) — untaken.**
   Cases 1, 2 and 4 are covered (runs 2 and 3). Case 3 is the scenario/command-line entry and case 5
   needs the save dialog's delete button, which no harness drives. Both are a single integer compare
   in a switch that is otherwise transcribed and shadow-checked, and all five `UpdateXxx` targets
   stay on the DLL. **Stated, not claimed.**

`~Goal` remains self-check-only end to end, as recorded above.

## Wave 5 result

| | |
|---|---|
| Ported | **58 of the 74 functions our tree calls** in these namespaces — Goal 13, AchievementManager family 21, Save 15, Picking 9 |
| Census | Achievement 34/50, Save 15/20, **Picking 9/9 (100%)**; TOTAL 727 called, 248 ported (34.1%) |
| Diff oracle | **14 runs; the largest single run 160,416,395 calls; 0 divergences in every Wave 5 site across all of them** |
| Residual | The pre-existing Wave 1 1-ULP sin/cos family only, in every run |
| Held back | 7 with written reasons: `Goal::Serialize`, `GoalInstance::Persist`, `AchievementManager::Persist`, `Achievement::Serialize` (all four construct an `IO::Serializer` — a 12-byte stream header, no oracle possible), `SaveLoad::Draw` (builds five Wave 6/7 classes by hand), `~SaveLoad` (frees through an un-exported element dtor), plus the DATA exports which cannot be registered before cutover |
| Unexercised | 4, each with the justification above |
| Defects found | 2 live (the 4 phantom Goal manglings incl. the skipped handle-table swap-remove; the `SavedState` mid-function fallback), 1 pre-existing crash recorded, 1 uninitialised read **in the shipped DLL** |
| Collision2D / Pathfinding | **TRUE ZEROES** — our tree calls neither; pathfinding is reimplemented natively in the exe |

---

## One operational fact for later waves: the diff oracle SURVIVES the perf build

Raised by lane B and verified here, because it decides whether PLAN §5's "passes the diff harness
on `sa-run` **and `sa-stress`**" box is even reachable.

`standalone/log_strip.h` strips **unqualified `printf` only**. `runtime/funcs/rt_diff.cpp` emits
every one of its 8 output calls through `fprintf(stderr, …)` and contains no bare `printf`. So
`BANISHED_RT_DIFF=1` works on the `standalone-perf` exe that `tools/stress_run.sh` builds, and the
summary is read from `standalone_error.log`, not `standalone_debug.log`.

What IS stripped there is the `[RtPort] … registered` line — cosmetic; the registrations still
happen. Do not read its absence as "the ports did not load".

Wave 5's run 9 used `BANISHED_STRESS=1` on the **debug** exe, which is a functional check and not a
perf measurement (`perf/README.md`: never record a baseline from `-O0`). A perf-exe stress run under
the diff oracle is therefore available to the next wave and was not needed by this one.

## A note on `tools/ach_reset.sh`, deliberately NOT run

Lane B recommended clearing the `Ach*` registry values before the award run, on the reasoning that
an inherited award is indistinguishable from a fresh pass. The reasoning is sound and the concern is
real, but the action was declined and the wave closed without it, for two reasons:

1. **It was not necessary.** Run 9's census moved 6/36 -> 8/36 *during the run*, with `Settlement`
   and `Village` reading `awarded=1` at the first sample after their population goals were satisfied
   by the stress town's 802 citizens. The transition was observed, not inferred from a final count,
   so there was nothing for an inherited award to fake.
2. **It destroys project state that is not this wave's to spend.** The achievement record is a
   tracked artifact of several earlier waves (`project_production_achievements_stat20.md`,
   `project_immigrants_nomads_closed.md`, `project_jack_of_all_trades_closed.md` — Ach5, Ach25,
   Ach27 were each closed by a long run). Wiping it to tidy one branch's evidence is a bad trade,
   and it is the user's call rather than a lane's.

Lane B also withdrew the premise behind the suggestion once it saw the census: it had assumed ~25 of
36 were flagged and that the registry was the blocker. At 6/36, 30 indices were eligible and the
registry was never the blocker — the conjunction problem was the whole explanation.

### Two rules this left behind, both from lane B's closing note

**The cheapest reproducer for the achievement award branch.** `Settlement` and `Village` are single
population StatGoals — no TimeGoal, no profession goals — satisfied outright at `have=802 >= 300`
and `>= 600` by any stress town at default citizens. So the award branch of
`AchievementManager::Update`, its event-handler call and the `IsAchievementAwarded` readback are all
reachable in one short run:

```
BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200 BANISHED_STRESS_CITIZENS=800 BANISHED_ACH_YEAR_BOOST=250 BANISHED_ACH_IGNORE_MODS=1 BANISHED_ACH_TRACE=400 BANISHED_RT_DIFF=1
```

Cheaper than the `Jack of all Trades` recipe in
`project_jack_of_all_trades_closed.md`, which needs `BANISHED_BUILDERS=40`, a pasture autotype and a
5-year clock deviation. Use Jack when the professions matter; use this when only the branch does.

**A rule for `tools/ach_reset.sh`: run it when the evidence is a COUNT, never when it is a
TRANSITION.** Its whole purpose is stopping an inherited award from being mistaken for a fresh pass,
which is a real risk when the measurement is "N of 36 at the end" and no risk at all when the
measurement is `6/36 -> 8/36` observed mid-run with `awarded=1` appearing at the first sample after
the goals were met. An inherited award cannot fake a transition. Since the script destroys a record
that `Ach5`, `Ach25` and `Ach27` each cost a long run to produce, the default is not to run it.
