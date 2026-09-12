# WAVE 26 — LANE LOCK

Branch `wave26/lock` off `wave26/base` (`7de42dd1`).

Scope: the two game-dir-lock defects from the Wave 25 integrator queue (§E1), and
machine-load recording in `tools/stress_run.sh`.

**This lane ran no game.** Nothing here touched `C:/GOG Games/Banished`. Every
number below comes from scratch directories under `$TMPDIR`, which is how the
existing selftest already worked.

---

## 0. MEASURED vs REASONED — read this before quoting anything below

**MEASURED** (I ran it, output quoted):

* `tools/game_dir_lock_selftest.sh` — **15/15 PASS**, including the 11 new cells.
* `tools/game_dir_lock_mutations.sh` — **5/5 PASS**: five deliberately broken
  copies of the subject, each caught by *exactly* the cells named for it and no
  others.
* `tools/machine_load_controls.sh` — **9/9 PASS**, including three dead-parser
  cells that must read UNEXAMINED rather than CLEAN.
* `wmic`, `tasklist` timings on this machine (§6.2), the real sampler end to end
  (§6.4), and the `--lock` re-exec against a scratch dir (§6.5).

**REASONED, NOT MEASURED** — stated so it cannot inherit the credibility of the
lines above:

* That `STALE_AFTER=3600` is the right floor. It is a judgement. What is
  *measured* is that the floor is no longer the decision — see §1.
* That the conservative direction (refuse to break a live-looking lock) is the
  cheaper error. Argued in §1.3, not measured.
* The machine-load figures `stress_run.sh` now records have **never been read
  back from a real stress run** (§6.4, §7). The recorder is exercised end to end
  against its own real sampler; its output has not yet been used to explain a
  perf delta.

---

## 1. Defect (a): `STALE_AFTER` shorter than a legitimate measurement

### 1.1 What was wrong

`tools/game_dir_lock.sh:131` (pre-change):

```sh
STALE_AFTER=1800                 # 30 min; a lane run that long is already wrong
```

and `break_if_stale` consulted **one fact**: `now - started`. Wave 25 lane ALLOC's
lock was broken at **1802s** with its process alive and a write to the protected
directory **1 second earlier**.

### 1.2 The fix is not the number

The brief's observation is the whole finding and it holds: **`status` already
computed exactly the evidence needed, in the same file, and the expiry path next
door ignored every line of it.** `live_exe_pids()`, `game_dir_idle()` and
`run_in_progress()` all existed and were all printed to humans; none was ever
read by the code that made the decision.

So `STALE_AFTER` is now a **floor**, not a verdict. Past it, `lock_staleness()`
returns one of `YOUNG | ALIVE | STALE | STALE_HARD` after consulting, in order:

| signal | attributable to the holder? |
|---|---|
| `$LOCK/heartbeat` fresher than `HEARTBEAT_STALE` (900s) | **yes** |
| a live `reimpl-x64.exe` pid | no |
| game-dir handles held open (`run_in_progress`) | no |
| game dir written within `STALE_IDLE_REQUIRED` (600s) | no |

Any one of them ⇒ `ALIVE` ⇒ not broken. New constants:

```
STALE_AFTER=3600      # floor past which evidence is consulted (was: the verdict, 1800)
STALE_HARD=10800      # broken regardless of evidence -- the deadlock bound
STALE_IDLE_REQUIRED=600
HEARTBEAT_STALE=900
```

`status` now prints `staleness verdict: <word>` plus the evidence line, so the
tool that decides and the tool that reports are visibly the same tool.

### 1.3 The cost, stated rather than hidden

Three of the four signals are **not attributable to the holder**. A live
`reimpl-x64.exe` started by an unrelated lane running unlocked reads here as "the
holder is alive" and will keep a genuinely dead lock alive.

I took the conservative direction deliberately: refusing to break costs *waiting*,
breaking wrongly costs a *measurement*, and Wave 25 paid the second price. But
"safe" must not mean "forever", so `STALE_HARD` (3h) breaks on the wall clock
alone and says loudly that it did not consult the evidence. **That bound is
tested** — see the `hard ceiling` cell.

The `heartbeat` verb, and `run`'s automatic heartbeat touch, are the way a lane
gets an *attributable* signal. A lane holding the lock across a long analysis gap
should call `tools/game_dir_lock.sh heartbeat <lane>`; `run` does it every watch
interval for free.

### 1.4 PROVEN ON BOTH SIDES (measured)

`tools/game_dir_lock_selftest.sh`, defect 4. Seven cells, **all at the same lock
age except where stated, so only the evidence can explain a difference**:

```
PASS -- defect 4/dead holder must still break: BREAK (legacy age-only rule: BREAK)
PASS -- defect 4/live exe pid (ALLOC, process alive): KEEP (legacy age-only rule: BREAK)
PASS -- defect 4/recent write (ALLOC, wrote 1s before the check): KEEP (legacy age-only rule: BREAK)
PASS -- defect 4/handles held open: KEEP (legacy age-only rule: BREAK)
PASS -- defect 4/fresh holder heartbeat: KEEP (legacy age-only rule: BREAK)
PASS -- defect 4/under the floor: KEEP (legacy age-only rule: KEEP)
PASS -- defect 4/hard ceiling breaks despite every live signal: BREAK (legacy age-only rule: BREAK)
```

Two things make this more than a green screen:

* **The dead holder is the handed-in positive case.** Five cells assert a lock
  was *not* broken, which is an absence, and an absence screen with no positive
  case cannot distinguish a correct subject from a dead one. `dead holder must
  still break` is that shape, and `hard ceiling` is a second one.
* **Every cell states what the LEGACY age-only rule would have done, and that is
  asserted too.** A KEEP cell whose legacy verdict is also KEEP never reached the
  changed code; its pass would be vacuous. The `under the floor` cell legitimately
  agrees with the old rule and declares `KEEP KEEP` rather than being scored
  against a control that does not apply to it. (This caught a real error in my
  own first version, where I applied the control to all KEEP cells and the
  under-the-floor cell failed for a reason that was not a defect.)

New test seams, inert unless set: `GAMELOCK_FAKE_PIDS`, `GAMELOCK_FAKE_IDLE`,
`GAMELOCK_FAKE_RUN_FILE` (alongside the pre-existing `GAMELOCK_FAKE_RUN`).

---

## 2. Defect (b): the holder was never told

ALLOC: *"the lock was broken as stale WHILE MY RUN WAS LIVE and I got no signal at
all — I only learned from your message."*

Note what a bigger `STALE_AFTER` would **not** have fixed. The old code's silence
was structural: breaking a lock removed `$LOCK` and left nothing behind, so the
victim's next `release` printed `not held; nothing to release` **and exited 0**,
and its `verify` said `no lock is held at all` as if it had simply never acquired
one. Both sentences were true. Neither carried the fact.

**The fix**: `record_breach` writes `$GAME/.lane_lock_broken` — *outside* `$LOCK`,
which is what gets removed — naming the victim, the breaker, the age, the verdict,
and the deployed exe's md5 at that instant. `breach_notice` then shouts from every
verb a wronged lane plausibly runs next: `acquire` (on grant), `assert-held`,
`release`, `heartbeat`, and `status <lane>`. **`release` now FAILS** instead of
printing the true-but-useless sentence.

Clearing is manual (`ack-breach <lane>`), and only the victim may clear it. The
entire defect was a signal nobody had to acknowledge.

### PROVEN (measured) — defect 5, with both controls

```
PASS -- defect 5: the broken holder is told (acquire/assert-held/release), only the victim, and only until acked
```

That cell is a sequence, and it fails on any step:

1. **Negative control first**: on a scratch dir with *no* breach record,
   `assert-held laneA` must still fail (no lock) **and must not print the
   banner**. A banner printed unconditionally would make every positive step
   below vacuous — this is the dead-parser check in its other direction.
2. Positive: laneA's lock broken by laneB ⇒ `assert-held laneA` prints the banner
   and names `laneB`.
3. `release laneA` exits **non-zero** and shouts.
4. **Negative control**: a breach naming laneA must **not** shout at laneB.
5. `ack-breach laneB` refused; `ack-breach laneA` accepted; afterwards the banner
   is gone.

---

## 3. The pattern the brief asked me to consider generalising

ALLOC md5-verified the deployed exe against its own build before every launch and
recorded the hash; six completed runs, same hash. **That per-run check is the only
reason "was my measurement contaminated?" was answerable at all** — without it,
it would have been two plausible stories arguing. It was also per-lane discipline,
which means it protected exactly one lane.

It is now part of the tool, in two places:

**(i) `assert-held <lane> [your-build.exe]`** — with a path, the gate also fails
unless the exe deployed in the game dir *is* that build. Ownership is necessary
and not sufficient: the exe under the lock can have been replaced before you took
it.

**(ii) `run` samples the deployed exe's md5 for the life of the wrapped command**
and reports what it saw (`exe identity: N samples, K distinct md5 …`), dropping
`exe_identity.tsv` into `--snapshot` next to the logs. It also re-checks ownership
every interval, so a lock lost mid-run is noticed **by the holder, within one
interval**, rather than by an integrator's message afterwards. A flagged run
returns **7** even when the wrapped command succeeded.

**The discriminator is the part that had to be right.** "Did the exe change" is
useless — `make sa-run` deploys, that is the point. The question is whether it
changed **while a run was live**, and `run_in_progress()` already answers "is a
run live" by the one test that cannot go stale between the check and the answer.

### PROVEN (measured) — defect 6, three cells

```
PASS -- defect 6/exe replaced WHILE the run is live: FLAG (rc=7)
PASS -- defect 6/exe replaced while idle (an ordinary deploy): CLEAN (rc=0)
PASS -- defect 6/live run, exe untouched: CLEAN (rc=0)
```

The corrupted value is the **exe's bytes**, which is exactly what the check
compares — corrupting anything else would report NOT CAUGHT and indict a sound
instrument. The middle cell is the mirror guard: a screen that flagged *every*
change would catch the real clobber and be worthless, because every lane would
learn to ignore it.

**RESIDUAL, stated in the code too:** if the game exits between two samples and a
deploy lands in the same interval, the previous sample still reads live and this
reports a clobber that was not one. It errs toward shouting, and the full sample
table is kept on disk (and named in the message) whenever it flags, so the claim
is checkable rather than a bare verdict.

---

## 4. `tools/game_dir_lock_mutations.sh` — the controls, made permanent

A suite that has only ever been run against a *correct* subject cannot tell a
passing screen from a dead one. This new script points the same suite at five
deliberately broken copies and requires the suite to fail **exactly** the named
cells — the set is diffed, not the count, because a count that moves the right
direction by the wrong amount reads as success.

```
baseline: the unmutated subject passes every cell

PASS -- M1 (staleness decided by the wall clock alone (the Wave 25 code)) -- caught by exactly the expected cells
PASS -- M2 (a lock is never stale (deadlock forever)) -- caught by exactly the expected cells
PASS -- M3 (the lock is broken and nothing records it (ALLOC got no signal)) -- caught by exactly the expected cells
PASS -- M4 (the identity watcher flags an ordinary deploy as a clobber) -- caught by exactly the expected cells
PASS -- M5 (the identity watcher never notices a clobber) -- caught by exactly the expected cells

PASS=5 FAIL=0
```

* **M1 is literally the Wave 25 code.** It must break the four live-holder cells
  and must **not** break `dead holder` or `under the floor` — the old rule agreed
  with the new one on those. A mutation that fails everything proves nothing
  about aim.
* **M2 is the opposite error** and is the one the five KEEP cells cannot catch on
  their own: with only M1 run, a subject that deadlocks every lane behind a corpse
  would score 15/15.
* **M3 is ALLOC's actual experience** — the break happens, nothing records it.
* **M4/M5** are the two directions of the identity check.

Two harness defects this script found and fixed in itself, both of the recorded
project shapes:

* **A mutation whose anchor no longer matches writes no file**, the suite then
  runs against a nonexistent path and fails *every* cell — which reads as
  "caught!" to anything looking at a count. `check_mutation` now refuses to score
  a mutation it cannot confirm was applied. This fired for real on M3.
* **A baseline gate**: if the unmutated subject does not pass clean, every result
  is meaningless, because a mutation's "caught" would be the baseline's own
  failure wearing the mutation's name.

---

---

## 5. Two design choices that are load-bearing, not incidental

Recorded at the integrator's request, because both are the kind of thing a later
wave quietly undoes while tidying.

### 5.1 The breach file lives OUTSIDE `$LOCK`, and that IS the fix

`$GAME/.lane_lock_broken`, not `$LOCK/broken`. The old code was not silent by
oversight — it was **structurally incapable** of speaking: the only place it
could have written a notice was the directory it was in the act of removing.
`rm -rf "$LOCK"` deletes the evidence and the messenger together.

Anyone who later moves that record inside `$LOCK` "where it belongs" restores the
silence exactly, and the restored bug will look like a tidy-up in the diff. It is
outside on purpose.

### 5.2 `STALE_HARD=10800` is protecting against the mirror failure

The ceiling is not an arbitrary number, and it is not a second staleness rule. It
exists because **three of the four liveness signals are not attributable to the
holder** (§1.3). A live `reimpl-x64.exe` belonging to an unrelated unlocked lane
satisfies the ALIVE test, so evidence-based staleness alone can keep a dead lock
alive **forever**, on a signal that was never the holder's.

This project's recorded failure mode is the *opposite* one — expiring a live lock
— so the temptation for a later lane will be to raise or delete this ceiling.
Deleting it converts a bounded wait into an unbounded one with no detector. It
breaks on the wall clock alone **and says in its output that it did not consult
the evidence**, which is what makes it honest rather than a second guess. The
`hard ceiling breaks despite every live signal` cell exists to stop it being
silently weakened, and mutation **M2** is exactly the failure it guards.

---

## 6. `tools/stress_run.sh` — machine load

### 6.1 The problem

For much of Wave 25, up to seven lanes were compiling and running Python on this
machine at once. A perf number taken under unknown contention is not comparable
to one taken under different unknown contention — and **nothing recorded the
contention**, so the comparison could neither be made nor refused. It was simply
unknown, and unknown reads like fine.

This is not hypothetical on this tree. The sampler's very first real output, run
while writing this doc, was:

```
t_s,cpu_pct,free_mb,procs,make,compiler,python,reimpl,other_game
-1,87,9255,310,1,0,6,0,0
0,,9147,314,1,3,6,0,0
```

**87% CPU, one `make`, three compilers, six Python interpreters** — while six
other lanes work. Any perf number taken in that second is a number about this
machine as much as about the code.

### 6.2 What was added

`perf/results/<stamp>_<label>/` now also contains:

* **`machine_load.csv`** — one row per poll of the existing 5s supervision loop,
  plus a **baseline row at `t_s = -1` taken before the launch**. Columns:
  `t_s,cpu_pct,free_mb,procs,make,compiler,python,reimpl,other_game`.
* **a machine-load section appended to `report.txt`** — the verdict from the new
  `tools/machine_load_report.py`, sitting next to the number it qualifies rather
  than in a separate file nobody opens.
* **`exe_identity.txt`** — md5 of the deployed exe **before** and **after** the
  run, plus the md5 of the build this invocation produced. If `--no-build` /
  `--debug` was used, `built` is empty, which is itself the fact worth recording.
  A build-and-deploy run now **aborts** if the deployed exe is not the one just
  built, and a run whose deployed md5 changed between start and end says so in
  the log and in the file.

Cost discipline: the sampler must not become the contention. Measured on this
machine — `tasklist` 0.26s, `wmic OS get FreePhysicalMemory` 0.09s, `wmic cpu get
loadpercentage` **1.09s**. So the cheap pair is taken every 5s poll and the CPU
reading every sixth (~30s); on the other rows the CPU cell is **left empty rather
than carried forward**, because a repeated value reads as a measurement.

### 6.3 The part the integrator's note is about: absence must not read as quiet

`machine_load.csv` is written **even with `--no-load`** (header plus an explicit
`NOTHING WAS SAMPLED` comment), and the report section is emitted
**unconditionally**. A results directory with no load file would be *silently
absent*, and a silently absent load record reads exactly like a quiet machine.

So the reporter's verdicts are `UNEXAMINED | CONTAMINATED | BUSY | CLEAN`, and
**`UNEXAMINED` is a distinct verdict from `CLEAN`** — "nothing was measured" is a
statement, not a gap the reader has to notice. Every verdict also prints *what it
examined* (sample count, how many carry a CPU reading, run duration), because a
screen that prints a verdict without naming its subject is this project's
most-repeated instrument defect.

Note the reporter separates the **baseline** row from the run rows. That is what
lets "6 python processes" be reported as *already there* rather than as something
this run caused. Flagging the baseline's own processes would be an inflating
blind spot, and an inflating screen manufactures work.

### 6.4 PROVEN (measured)

`tools/machine_load_controls.sh` — **9/9 PASS**. Every cell hands the reporter a
shape and requires it to name that shape:

```
PASS -- CLEAN: CLEAN
PASS -- CONTAM_EXE: CONTAMINATED          (a second reimpl-x64.exe)
PASS -- CONTAM_GAME: CONTAMINATED         (Application-x64 / Tools-x64)
PASS -- BUSY_COMPILE: BUSY                (compilers during the run)
PASS -- BUSY_CPU: BUSY                    (median CPU only)
PASS -- PY_BASELINE: CLEAN                (MIRROR control: already-present pythons)
PASS -- EMPTY: UNEXAMINED                 (dead-parser cell)
PASS -- NOLOAD: UNEXAMINED                (dead-parser cell)
PASS -- MISSING: UNEXAMINED, not CLEAN    (dead-parser cell)
```

`EMPTY`, `NOLOAD` and `MISSING` are the cells that matter most: **if any of them
read `CLEAN`, every `CLEAN` in this project's perf results would be
unfalsifiable.**

End-to-end, real sampler into real reporter (not a re-implementation of the
sampler in a test file — that would be two checks sharing one hidden premise):

```
$ STRESS_LOAD_PROBE=1 bash tools/stress_run.sh --no-build > load.csv
$ python tools/machine_load_report.py load.csv
machine load: examined 1 samples over 0s (0 of them carry a CPU reading)
  baseline (before launch): compiler=0 python=5 cpu=12 free_mb=9460
  ...
machine load: CLEAN
```

`STRESS_LOAD_PROBE=1` runs `machine_sample` **in `stress_run.sh` itself** and
exits, so the sampler under test is the shipping one.

### 6.5 `--lock LANE` (opt-in) — exercised end to end

`stress_run.sh` deploys and launches — exactly what the lock protects — and until
now it neither acquired nor asserted. `--lock LANE` re-execs the whole script
under `game_dir_lock.sh run`, so the acquire's exit status is checked inside the
lock tool where **no caller's pipe can discard it** (this project's recorded
defect: a pipeline exits with the last command's status, so a TIMEOUT reads as a
GRANT).

It is **opt-in, default unchanged**, because six lanes depend on this harness
right now and one that starts refusing to run would be a worse failure than the
one being fixed. Measured against a scratch directory (no real game dir touched):

```
[gamelock] ACQUIRED by probelane
[gamelock] GRANT probelane
t_s,cpu_pct,free_mb,procs,make,compiler,python,reimpl,other_game
-1,57,9543,307,0,0,5,0,0
0,,9513,309,0,0,5,0,0
[gamelock] exe identity: 1 samples, 1 distinct md5 (first=7805ea2e... last=7805ea2e...)
[gamelock] exe identity: clean -- no change while a run was live, lock never lost
[gamelock] released by probelane
rc=0
```

Acquire → re-exec → sample → identity check → release, with no `.lane_lock` left
behind.

**ADDRESSEE: the Wave 26/27 integrator.** Decide whether `--lock` becomes the
default in Wave 27. I did not make that call unilaterally mid-wave.

---

## 7. WHAT I DID NOT DO — named, not implied

* **I ran no game, and no real stress run.** Every number above comes from
  scratch directories, synthetic CSVs, or the one-shot `STRESS_LOAD_PROBE`
  sampler. The load columns have therefore **never been read back from a real
  6000-frame run**, and have never been used to explain a perf delta. The
  recorder is exercised; its *usefulness* is reasoned, not measured.
* **`run`'s identity watcher has not met a real `make sa-run`.** Its 10s default
  interval was chosen, not tuned. Against a real run the deploy and the launch
  are seconds apart, and the residual named in the code (a run ending in the same
  interval as a deploy reads as a clobber) has not been observed either way.
* **`--lock` is not the default**, by choice — see §6.5.
* **I did not touch** the FIFO ticketing, the orphan/husk reporting,
  `boot_state.sh`, or `perf/baselines/`.
* **`STALE_AFTER=3600` is a judgement, not a measurement.** What is measured is
  that the floor is no longer the decision. If a legitimate measurement ever runs
  past an hour *with no heartbeat, no live exe, no handles and a quiet directory
  for ten minutes*, it will still be broken — and I have no evidence about
  whether such a run exists.
