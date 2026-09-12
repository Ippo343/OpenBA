# WAVE 25 INTEGRATOR QUEUE — what must happen between the last merge and attempt 26

Written during the wave, not after it, because this project's recorded defect class is
**a residual that names no addressee never fires**. Every item here has an owner: me.

Tip when written: six of eight lanes merged, `1333 called / 1196 ported / 89.7%`, 137 gap
rows. ALLOC and SWEEP outstanding.

---

## A. BEFORE re-deriving the final board

### A1. Regenerate `standalone/rt_data_mint_tables.inc` — ONCE
`tools/rt_mint_gen.py --check` reads **STALE** on the wave base (verdict confirmed by me;
`ENTERED (1)`, `LEFT (0)` — the row is `?_typeInfo@Base@Object@@`). Pre-existing, found by
lane DATA, addressed to the integrator.

**Why it waited:** regenerating shifts mint indices, and eight lanes were reading that table.
Do it **once**, after the last merge, so indices move exactly once. Re-derive the board
**after** the regeneration, never before — the board that goes in the close is the
post-regeneration one.

### A2. Sweep the tree for PHANTOM-LITERAL gap rows
Lane VTIO's oracle negative control passed a **fabricated mangled name as a string literal**,
and the census **minted a permanent gap row for a symbol that does not exist**. VTIO fixed its
own (assembles the name at run time) and warned that *any* lane doing this silently inflates
the board. **It did not sweep the tree.** Lane SWEEP will attempt it and has agreed to say
plainly if it did not get to it.

A fabricated symbol in the gap is **work that can never be completed**, so it inflates the
denominator forever. Grep the tree for mangled-name string literals at probe/control sites and
confirm each names a symbol the DLL actually exports.

---

## B. ONE RUN THAT DECIDES SEVEN ROWS

### B1. `BANISHED_W17_ATTEND=1` after DATA's watch table has landed
Lane DATA adjudicated its seven remaining `?_typeInfo@` rows as READY, safe, consumer present,
blocked on **exactly one** criterion: a real boot. It added the three Achievement rows to the
attendance watch table so one run decides all seven, **each then a two-line promotion if
ATTENDED**.

**Lane SWEEP could not decide them** — DATA's watch-table additions were not on SWEEP's branch,
so its run carried the 63-row Wave 17 table (41 of 63 attended, 22 never; the six Video
`?_typeInfo@` rows read `never` and no Achievement row existed). This is a merge-order
artefact, not a result.

**Action:** after DATA and SWEEP are both merged, one armed run with `BANISHED_W17_ATTEND=1`.
Cheapest row-closing available.

---

## C. CARRY INTO ATTEMPT 26

### C1. `BANISHED_RT_MAINLOOPFRAME=0` on the first attempt
Lane BODY ported `?Frame@MainLoop@Engine@@` and `??1PackageFile@IO@@` and **both are
default-on**, so `MainLoop::Frame` runs every frame from the merge. BODY recommends the kill
switch for the first cutover attempt so a new failure is not confounded with a new default-on
body. Honour it, then re-run without it.

### C2. The manifest is a GENERATOR — invoke it, do not hand-edit
`tools/cutover_manifest.py` re-derives from four live inputs, so DATA's 14 newly-tagged rows
are picked up automatically without MANIFEST being alive to receive them.
`standalone/cutover_manifest.cpp` **detects cutover itself** (same module-handle test the mint
shadow uses), so **no new env var is needed** — attempt 26 just runs.
Expect `published 44, withheld 1, conditional 1, unminted 0, failed 0`, and re-derive rather
than trusting those numbers, since DATA's 14 tags landed after MANIFEST measured.

`--check` fails on a stale `.inc`, a REQUIRED row with no minted storage, or a tag that lost
its scope. **Run it after A1**, because A1 changes minted storage.

### C3. Do NOT use the blanket `BANISHED_RT_MINT=1` switch at cutover
MANIFEST measured that the blanket switch **publishes the WITHHELD row against its own
verdict**. The manifest alone suffices.

### C4. The wall to expect
Attempt 25 died at `??0Stream@Audio@@QEAA@XZ`, which Wave 24 lane SMALLBODY then **ported**,
and which also converted retail's RIP-relative LEA stamp for `??_7Stream@Audio@@6B@` into a
real door. So attempt 26 should reach a **new** wall. A boot is a GATE, not a planner — it can
only name what it reached before faulting. Expect one new name, not a roadmap.

**Read `standalone_crash.log` FIRST.** Two of its fields cannot carry what they get read for:
`asked-by` names the RESOLVER, not the caller; `VCALL-of-0` is a heuristic on RCX.

---

## D. KNOWN-FALSE ALARMS — do not lose a session to these

1. **`[VideoInterface] *** DETOUR FAILED ***` is a FALSE ALARM at cutover.** The slot is
   filled twelve lines later. Wave 24 named it the line most likely to cost attempt 26 a
   session. (Lane SWEEP was asked to silence or annotate it; verify it did.)
2. **`??_7MaterialInstance@Graphics@@6B@` is promoted but UNOBSERVED.** Lane SWEEP measured
   `[W17Attend] never init=0 post=0` — **the door is not opened at all on the autostart
   route**, so an A/B there would compare two runs in which nothing fetches the table. The
   promotion is safe and unexercised; **the route that would exercise it is not autostart.**
   Do not read its silence as either working or broken.
3. **`?Serialize@AnimationManager@Graphics@@` must STAY UNVERIFIED** — Wave 24 GFXMAT answered
   Wave 23 GFXBODY's R4 and the answer was **no**. Do not let a cleanup pass delete that note.

---

## E. FOR THE CLOSE DOC — defects that outlived their lane

1. **The game-dir lock has TWO defects, and the second is the dangerous one.**

   **(a) The stale threshold is shorter than a legitimate measurement.** ALLOC's lock expired
   at **1802s against an 1800s default while its process was alive and had written to the
   directory 1s before the check.**

   **(b) THE HOLDER IS NEVER TOLD.** ALLOC's own words: *"the lock was broken as stale WHILE MY
   RUN WAS LIVE and I got no signal at all — I only learned from your message."* A holder about
   to lose its lock must be warned, **or the lock must not expire while the directory it
   protects is demonstrably in use** — and `game_dir_lock.sh status` already computes exactly
   that (it reports live pids, held handles and last-write age). The information needed to not
   expire the lock is already in the tool that expires it.

   **This was not hypothetical and it was not free.** During the unlocked window lane SWEEP
   legitimately acquired and **deployed its own binary** (dir md5 moved to
   `a4e467ef…`; ALLOC's build is `87c6c98a…`). **SWEEP did nothing wrong** — it acquired a lock
   the script granted. The system failed, not the lane. Cost: ALLOC lost pair 3's second arm
   and discarded the whole pair.

   **No corrupted data entered the comparison, and that is because of a per-run guard, not
   luck:** ALLOC md5-verifies the deployed exe against its own build **immediately before every
   launch**, and all six completed runs recorded `87c6c98a`. A mid-run overwrite is separately
   harmless because Windows holds a running exe open — the "Device or resource busy" case the
   lock header already documents. **That per-run md5 check is the pattern to keep**; it is what
   turned "was my measurement contaminated?" from an argument into a measurement.
2. **`stress_run.sh` records nothing about machine load** — and for much of this wave up to
   seven lanes were compiling and running Python on the same machine.
3. **A clean `standalone-debug` plus green checkers is NOT evidence the tree builds.** Wave 24
   integration (mine) passed both while `standalone-perf` was broken, because the
   `check_log_strip` guard only runs for the log-stripped target. **Add the perf build to the
   integration gate.**
4. **Three lanes edited `tools/runtime_census.py`** and it produced the wave's only conflict.
   Both parser defects found in it this wave (`(\S+)` on the reason class, `(\?\S+)` on the
   symbol key) were **silent** — a failed match drops the tag whole and the row prints as
   ordinary unported work. Both now have a regression guard; consider whether any file every
   wave quotes should be lane-owned rather than free-for-all.
