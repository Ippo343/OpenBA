# WAVE 20 — GUIDE

**Written 2026-09-10 by the Wave 19 integrator, at close. Not yet dispatched.**

Every number here was **measured on the Wave 19 close tip**, not carried over. Where
a fact is inherited and not re-verified it says **[INHERITED]**, and re-verifying it
is a lane's first job.

Read first: **[WAVE19_CLOSED.md](WAVE19_CLOSED.md)** §2 (the metric is blind to half
the work), §4 (what Wave 19 does NOT claim), §7.1–§7.4 (what makes a check able to
fail correctly). Then **[WAVE19_INTEGRATOR_CORRECTIONS.md](WAVE19_INTEGRATOR_CORRECTIONS.md)**
— 45 entries, of which C34 and C43 are load-bearing.

---

## 0. STOP. Fix the instruments BEFORE scoping any lane.

Wave 19 spent its whole length discovering that its own measurements were wrong.
**Three instruments are known-broken today and two of them will silently mis-scope a
Wave 20 lane.** Fixing them is Phase 0 and it is not optional.

### 0a. `runtime_census.py` cannot see minted-vtable slot bodies — THE SCOPING BUG

**This is the one that matters most.** The census builds its row set by *iterating*
`called_exports` and `called_addrs`, so a registration with no call door **never
becomes a row**. Lane CSTREAM registered 12 exports and `ported` moved **+3**; nine
were vtable slot bodies.

> **The identity sees a vtable row being CLAIMED and is blind to the work that makes
> the claim TRUE.**

**The fix is precedented and sits twelve lines from the code that would change.**
Wave 10 hit the identical root cause with `UI::TabElement::UpdateContainers` —
ported, registered both doors, detoured, verified, exercised live, and printed in
"ports with no caller" — and folded detour targets in as called+ported+detoured,
justified as *"the detour is what makes it run."* **The minted vtable is what makes
a slot body run.** Same argument, same place in the file.

Until this lands, **any DATA lane scored on the identity under-reports by ~4x**, and
Wave 19's own `36 → 12` headline is mostly a CODE-work number.

### 0b. `boot_state.sh` reports RUNNING off a stale state file — THE HARM ONE

It checks **pid liveness, never file freshness**. A crashed process whose watchdog is
dead reads as healthy. Its `STALE` code (4) covers a pid that is *gone* — a different
and rarer condition.

**This caused measured harm, not hypothetical:** a lane's run printed `BOOTSTATE_RC=0`
for an already-crashed process, which is the proximate cause of a **24-minute outage
that starved three lanes** and produced an unkillable orphan. Fix: compare the state
file's mtime against now, and report STALE when it stops advancing.

### 0c. 14 duplicate registrations in compiled code

`tools/dup_runtime_export.py` (exit 3 today). Two are cross-file and one is a real
defect:

* **`?PlatformDestruct@RenderBuffer@Video@@`** — `buffers.cpp:1308` and
  `context_lifecycle.cpp:296` define **two distinct bodies under one export name**
  (different linkage, so both link). VID2 wins both doors, so **Wave 9 lane GFX2's
  body is shadowed dead code the census counts as PORTED.** The inverse of
  `reference_half_registered_port_reads_as_absent.md`.
* **`0x1800e0c80`** — `input.cpp:733` vs `input_config_startup.cpp:220`. Same shape,
  **unexamined**.
* The rest are same-file pairs (`internal.cpp` ×4, `dialog_create.cpp`) plus the
  `allocator.cpp`/`rt_alloc_swap.cpp` atomic-swap pair, which is probably deliberate.
  **Nobody has checked whether the same-file pairs are conditionals** — a pair under
  an `#if` is legitimate and a checker cannot tell.

### 0d. Close the extent risk — 31 rows

`tools/mint_extent_audit.py` reads **clean across all 62 rows**, and **says in its own
output that clean is not proof**: a table stamped other than by a plain RIP-relative
`LEA` is invisible to it. **31 rows still carry an `adjacent-export` proof** — the
shape that was wrong twice in Wave 19 (`??_7ReaderWriter@` 36→26, `??_7TextFileStream@`
20→10).

**Wave 20 must not scope a DATA lane against a slot count it has not re-derived from
a ctor's own `LEA`.** The writer beats any number of readers.

---

## 1. State at the Wave 19 close — measured

```
IO          407 total   133 called   116 ported    ceiling 96.2%
                        identity: 116 >= 133 - 5 = 128   ->  12 SHORT
Graphics    599 total    99 called    70 ported    ceiling 96.0%    29 short
Engine      108 total    33 called    23 ported                     10 short
Video       573 total   152 called   134 ported                  AT CEILING
TOTAL      4632 total  1195 called  1007 ported    84.3%
```

`called` grew 122 → 133 in Wave 19 as lanes opened doors. **Expect it to grow again.
Report a set-diff of what entered `called`, never a percentage delta.**

---

## 2. Choosing the wave's subject — three candidates, honestly compared

### Candidate A: PROMOTION, not porting — **recommended**

**Only 5 of 62 VTABLES are promoted, and 23 COMPLETE tables are minted and
unregistered.** (The "31 of 62" first written here was a category error: 31 mixes
vtables, typeinfo and storage; 62 counts vtables only. See WAVE19_CLOSED.md §7.5a.
The corrected figure makes this subject ~4x LARGER, not smaller.) Wave 19's sharpest
measurement is that this is where the value now sits:

> Lane FILE's ten bodies read **TRAP=0 with attendance ZERO on every one**, because
> the tables are minted but **not promoted** — so `RT_DATA_SLOT` falls through to the
> DLL, our objects get retail's table, and **retail's bodies run.**

Ten verified bodies that **do not run in play**. Promotion is what converts them.

**Start with the two proven NO-OP rows** — `??_7Stream@` and `??_7ReaderWriter@`.
§7.5b establishes that nothing dispatches through them, so promotion carries no
dispatch risk at all; they are the lowest-risk items on the board and this wave
twice, wrongly, told lanes to skip them.

**Then these two**, both unpromoted, both with their warrant already written:
* **`??_7BinaryFileStream@`** — TRAP=0, **both** stampers ours, bodies oracle-verified.
  The strongest candidate in IO.
* **`??_7VisitorStream@`** — 7/7/0 **and** ATTENDED via `External::Add`. A proven door
  plus a complete slot set.

**The gate is §5.1's criterion, and it is not negotiable:** *a row is promotable
insofar as we own the function that STAMPS it.* Every stamp is an in-DLL RIP-relative
`LEA`, not a door. Lane CSTREAM's census is the worked example — three rows the
readiness screen ranks **identically** at 9/9/TRAP=0, separated by stampers-owned at
**0 / 2 / 3**. It recommended promoting none, and that was right.

**Enumerate stamp sites from the binary. Do NOT list our ported bodies and assert they
are the stampers** — lane IOBASE made that error twice in one lane, and a stamper
count I relayed would have let a lane declare a row fully owned **with a stamper still
in the DLL putting retail's table into every copied object.**

### Candidate B: the screenshot cutover hole — **finishable, and enumerated**

**Not shut.** The door board stands at **18 of 25 SHUT, 7 OPEN**. But the remainder is
*enumerated*, which is rare:
* **3 open for lack of PROMOTION, not ports** — the very tables lane FILE cleared.
  **Candidate A closes these.** Note §7.5b: a proven NO-OP row is the SAFEST
  promotion, not one to skip, because nothing dispatches through it.
* **2** are the allocator behind `BANISHED_RT_ALLOC`.
* **2** remain.

Wave 19 twice found this debt deeper than stated (the plan said four exports, C2 said
six, a call-graph walk found **ten**). **Do a call-graph walk before estimating.**

### Candidate C: IO's last 12 — **smallest, and the least valuable**

`?SetupFileSystem@Modification@` alone is **~600 instructions, a lane on its own**.
The PackageFile cluster re-scopes as **one real body with three cheap ones behind it**:
`?Destruct@` is three instructions and `?Reconstruct@` that plus a tail jump, both
blocked on the slot-0 deleting dtor, itself blocked on `??1PackageFile@` (87
instructions, five unported callees, a 0xf0-byte class of refcounted
Strings/Arrays/Sets).

**Recommendation: A first, B second (A closes 3 of B's 7 doors), C only if a lane is
free.** And note **the cutover boot wall is `?PlatformConfigure@Engine@1@` — an
ENGINE row, neither IO nor Graphics.** Engine is 33 called / 23 ported and
`HarnessGuardUnattendedBoot()` already has a seam on it. A one-lane Engine cell could
run beside any of these.

---

## 3. Named, specified, cheap — take these opportunistically

* **`?SaveText@External@Object@@` is the highest-leverage single item left.** It
  unlocks **three things across two lanes**: lane FILE's `??0TextFileStream@` (row 7,
  attendance 0), the **two IOBASE vtables SaveText stamps** (their `post=0` is
  explained by SaveText not running, *not* by a missing interception point), and
  `??_7TextFileStream@`. **THE HARNESS ALREADY EXISTS**: `external_savetext_probe.cpp`, armed by
  `BANISHED_RT_SAVETEXT_PROBE=1` from `external_add.cpp:277`, with three negative
  controls — found by lane FILE *after* the close, which is the fourth payout in two
  waves of the rule *grep before declaring no route*. This item is much cheaper than
  written above.
* **`??_7TextFileStream@`** — handed back by lane FILE at **10 slots / 8 bodies** (not
  20/20); its slots 1 and 7 are FileStream's and are already done.
* **`?Close@CompressedFileStream` (`0x1800a9490`)** — nine instructions; lane CSTREAM
  retracted its own "unverifiable" and specified the probe (§7.4 of the close).
* **A save/load route cell** — scope it as *"open the dialog and produce one save"*,
  **not** "produce one save". `?GetSaveFilename@` builds `<savePath>Save\<name>.sav`,
  so the `Save\` directory is created by **`?CreatePath@FileSystem@` — lane FILE's
  row**. The save route and the screenshot debt share code.
* **`??_7BinaryWriter@`'s unported COPY CONSTRUCTOR** — found by lane IOBASE while
  correcting its own stamper count. A previously invisible row.
* **A pre-CSWRITE baseline exists** for isolating `BANISHED_RT_CSWRITE` on the load
  path: `wave19/persist @ 95f7628`, `docs/runtime_port/wave19_persist/live_autoload_run.log`
  — 609 sites, 20,060,624 calls, 30 divergences in 7 sites, none in `IO::Persistence`.
  Valid **only** because `wave19/cstream` is not in its history; do not rebase it.

---

## 4. Doctrine — the parts Wave 19 paid for

### 4.1 On instruments — §7.1–§7.4 of the close, condensed

* **Two axes, and you need both.** *Parameters govern WHERE a check points;
  derivation governs WHETHER it can notice change.* Zero free parameters buys
  **unaimability, not soundness** — `static_assert(A == 37)` has no parameter and is
  exactly as stale as the comment. A fully-derived census diff was still confidently
  wrong for carrying `--namespace IO`.
* **Remove the parameter; failing that, PRINT IT BESIDE THE VERDICT.** Two tools were
  fixed this way in Wave 19, and in both the fix was **code order** — a headline
  cannot carry a number the code has not computed yet.
* **An instrument that names its subject only AFTER its findings has not named it.**
* **Quote every green WITH its denominator.** A screen printed
  `slot-count suspects: 0` while over-counting a row by 10.
* **Every screen needs a positive control that MUST read non-zero**, verified to print
  before any zero is trusted, **scored on APPLICABLE cases only**.
* **Read `calls=`, not just `divergences=`.** `calls=0` with `worked>0` means the body
  ran every time and always took the uncovered branch.
* **Prefer an instrument that can say "I MEASURED NOTHING."**
* **Run the instrument in the tree you are measuring** — several derive their root
  from the script path.
* **Never read an exit code through a pipe.** `$?` gives the *last* command's status.
  Three people hit this in one afternoon on the same tool, and one would have quoted a
  clean exit had a louder error not fired alongside.

### 4.2 On claims

* **A stated blocker is a claim with a TIMESTAMP.** Five for five stale across Waves
  18–19; the fifth was a lane's own, written the same session as the row it blocked.
* **WHEN YOU WRITE A BLOCKER, WRITE ITS PREMISE AS A SEPARATE, CHECKABLE LINE.** The
  timestamp rule tells you to re-check and gives you nothing to re-check. A wide
  screen refutes an *existence* claim but never an *impossibility* claim — **decompose
  it**: the premise is usually an existence claim a screen can reach.
* **"No route" is NOT "no oracle."** Refuted four times in Wave 18 and again in 19.
* **A generated artefact is a measurement with a timestamp** — and so is **prose
  describing** one, which decays silently because nothing regenerates a comment.
* **Grep before you write a port**, in **three** forms: the mangled name, the retail
  VA, **and** the `RtIo_<Class>_<Method>` convention. A body was nearly rewritten
  because greps on the first two returned only comments — **the linker caught it.**
* **`AEAA`/`AEBA` mean PRIVATE, not unexported.** This DLL exports its private members.
* **Register by NAME and by ADDRESS**; `ResolveExport` falls through silently.
* **Do not "fix" a faithful crash with a null check** — find the upstream null.

### 4.3 On process — earned the hard way in Wave 19

* **SERIALISE the game directory from the start.** Three tranches contending produced
  a 20-minute stall plus an orphan. Announce the order.
* **A crashed run is not a finished run.** Confirm the process **EXITED** — and check
  **all three files** (exe *and* both logs), not just the exe. If `taskkill` and
  `Stop-Process` both report *"no running instance"* while `tasklist` still lists it,
  **go UP to the launching shell.**
* **A ruling on a shared body needs an ACKNOWLEDGEMENT before the losing lane
  proceeds**, not just a send. A body was ported twice because a ruling and a request
  crossed in flight.
* **Private worktree per lane. Commit early. Stop editing once you report** — and land
  re-tasked work as a clearly-marked post-report tranche. **Sealing forbids altering,
  not annotating.**
* **`git commit -F <file>`, never inline shell quoting.** `-F -` does **not** read
  stdin. A mangled heredoc committed the literal text `x\nMSGEOF2` in this wave.
* **The integrator's negative relays are the highest-risk output of the whole
  structure.** *A lane's negative claim about the ENVIRONMENT cannot be checked by
  reading the lane's work — only by running a command the integrator can always run.*
  I relayed "no save games at all" into a merge commit; **13 existed**, and it cost a
  lane its verification route.

---

## 5. What would make Wave 20 fail

1. **Scoping a DATA lane on the identity** before §0a is fixed — it under-reports by
   ~4x and the lane will look idle while doing the wave's hardest work.
2. **Reading TRAP=0 as progress-in-play.** READY means *safe to stamp*. Attendance
   means *something dispatched it*. Ten Wave 19 bodies were TRAP=0 with attendance 0.
3. **Trusting a slot count** from `adjacent-export` sizing (§0d).
4. **Believing a stamper count you did not enumerate from the binary.**
5. **Letting a weaker tier carry a stronger claim** — "live-exercised" reads almost
   like "verified" and is not. Name every tier separately.
6. **Quoting a number without the tree it was measured on.** Five figures decayed in
   transmission in Wave 19, four of them through the integrator.
