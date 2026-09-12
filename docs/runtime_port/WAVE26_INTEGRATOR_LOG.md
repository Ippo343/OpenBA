# WAVE 26 INTEGRATOR LOG

Written DURING integration, not after it. Wave 25's queue was written during its
wave and its most important item was still skipped, so writing-during is
necessary and not sufficient — every item below names an ADDRESSEE, and the ones
addressed to a future wave say what makes them fire.

## MERGE ORDER AND STATE

| # | Lane | Branch tip | Merged | Conflict |
|---|---|---|---|---|
| 1 | LOCK (part 1) | `f4c1ae83` | yes | none |
| 2 | PROMOGFX | `9a671c8c` | yes | none |
| 3 | DOORS | `76f66ac8` | yes | none |
| 4 | ATTEND | `813155ec` | yes | none |
| 5 | LOCK (part 2) | `65f12ecc` | yes | lane doc — **kept both** |
| 6 | FACTORY | — | pending | — |
| 7 | INT33 | — | pending | — |

DOORS was merged AFTER PROMOGFX deliberately: DOORS owns `runtime_census.py`
this wave. In the event there was no conflict at all, which is itself worth
recording — Wave 25's only conflict came from three lanes editing that file.

The one conflict was in `WAVE26_LANE_LOCK.md`, between the lane's own sections
and the integrator's independent verification note. **Kept both.** Neither
supersedes the other: one is what the lane built, the other is what a second
party measured. Collapsing them would have destroyed exactly the distinction
that makes the verification worth anything.

## GATES, ALL RUN ON THE MERGED TIP

```
mingw32-make standalone-debug      exit 0
mingw32-make standalone-perf       exit 0     <- Wave 25 E3; a clean debug build
                                                 hid a broken perf build TWICE
tools/runtime_census.py --selftest-all        ALL GREEN (16/16 on the new fold)
tools/phantom_symbol_scan.py       NUL screen: none, controls pos+neg ALIVE
tools/game_dir_lock_selftest.sh    15/15
tools/game_dir_lock_mutations.sh   5/5
tools/machine_load_controls.sh     9/9
tools/rt_mint_gen.py --check       UP TO DATE
tools/cutover_manifest.py --check  UP TO DATE  <- was STALE until ATTEND merged
```

Two of these were run a SECOND way rather than trusted:
- the NUL screen was corroborated by a raw `grep -rlP '\x00'` over the whole
  tree, which is a different mechanism from the tool's own detector;
- the promoted-vtable fold was checked from the BINARY end
  (`??_7BinaryFileStream@IO@@6B@` demonstrably dispatches 18,906 times and the
  old census printed it unported), not by re-reading the lane reports that
  found it.

**`cutover_manifest.py --check` read STALE on the merged base through three
merges** and only went fresh when ATTEND landed. That is the Wave 25 defect
reproducing in flight — and this time the check was run, which is the only
difference.

## BOARD

`1333 called / 1185 ported / 88.9%` -> `1355 / 1212 / 89.4%`, gap 148 -> 143.

**Do not read that as 23 rows of porting.** The movement decomposes as:
- **+18 rows entered `called`** and 11 flipped to `ported` from lane DOORS,
  which **ported nothing** — it taught the census to see registrations it was
  already making. The board did not improve; the instrument stopped lying.
- **4 rows genuinely promoted** by PROMOGFX (2 Graphics vtables, 2 Achievement
  `_typeInfo` on ATTEND's ATTENDED verdict).

The honest headline is the DECOMPOSITION, not the percentage:
**real remaining vtable work is 27 rows, not 47** — Video's entire 12-row
vtable gap is permanently unpromotable and now prints as such every run.

## DECISION: `--lock` STAYS OPT-IN THIS WAVE

Lane LOCK built `stress_run.sh --lock LANE` (re-execs under `game_dir_lock.sh
run` so the acquire's status is checked where no pipe can discard it) and
**explicitly declined to make it the default**, addressing the choice to me.
That was the right call and the reasoning was correct: six lanes depended on
that harness mid-wave, and a harness that starts refusing to run is worse than
the defect it prevents.

**DECISION (integrator, 2026-09-12): `--lock` remains opt-in for Wave 26 and
becomes the DEFAULT at the Wave 27 kickoff, when no lane is in flight.**

- **ADDRESSEE: whoever writes the Wave 27 kickoff.** Not "a future lane", not
  "the next person to touch stress_run.sh" — this project has a recorded case
  of an expiry clause naming a condition and no addressee that then waited
  forever.
- **CONDITION: the Wave 27 kickoff is written.** That is a thing that will
  definitely happen, not a state someone must notice.
- **WHAT MAKES IT SAFE THEN:** no lanes in flight, so a refusal costs one
  person one command instead of stalling seven.

## RESIDUALS CARRIED, EACH WITH AN ADDRESSEE

1. **The load columns have never been read back from a real 6000-frame run.**
   LOCK named this itself rather than letting it be inferred. The recorder is
   exercised by 9 controls; its USEFULNESS is reasoned, not measured. The
   `run` identity watcher has never met a real `make sa-run` and its 10s
   interval was chosen, not tuned. ADDRESSEE: the first Wave 27 lane that takes
   a perf measurement.
2. **Whether a probe-only resolve belongs in `called` at all** — DOORS
   deliberately did not decide; changing it REMOVES existing rows. Population
   printed every run under `INSTRUMENT-SOURCED` (6 IO rows). ADDRESSEE: Wave 27
   kickoff.
3. **`??_7UpdateLoop@Engine@@6B@`** is 7/7 COMPLETE and deliberately NOT tagged
   unpromotable: 6 of its 8 stampers are base-subobject stamps in three derived
   loop classes and PROMOGFX did not determine transient-vs-final. **That is an
   unfinished check, not a verdict** — do not read the absence of a tag as a
   green light.
4. **Four `?_typeInfo@` rows are ROUTE-UNDECIDED**, not rejected. ATTEND
   measured them `never` and established the `never` is route-never-reached.
   **Re-running autostart is explicitly NOT the condition** — PROMOGFX wrote
   that down for the three Graphics ones.
5. **`mint_slot_sweep.py`'s validation claim was prose, and decayed.** Its
   docstring names the `[RtMint] minted ...` boot line as its validation,
   character for character; it had silently stopped holding at some revision
   after `fc884df` and holds again now. A `--verify-against <log>` mode would
   make it an assertion instead of a claim. ADDRESSEE: Wave 27 kickoff.

## TWO CAVEATS THAT MUST NOT BE LOST IN THE SUMMARY

- **PROMOGFX's own two vtable promotions have NO dispatch evidence either
  way.** Both boots print `[W17Attend] never init=0 post=0` for Geometry and
  AnimationEvent, and that is NOT evidence: the watch counts `ResolveExport`
  doors and both rows are reached by `ResolveAddr`-by-address. Installed,
  guarded, boot-clean, unobserved. The lane disclosed this itself.
- **ATTEND's attendance verdicts count DOORS, NOT EXECUTIONS.** "Route never
  reaches it" and "reaches it only via non-door in-DLL calls" are
  indistinguishable to that instrument. Same consequence for promotion; NOT
  proof the class is unused.

## THE NUL BUG HAS A FOURTH INSTANCE, AND THE SCREENS FOR IT HAD THE WRONG POPULATION

Found by the integrator during the PROMOIO merge, by accident:

```
$ grep -n "stampers" docs/runtime_port/WAVE26_LANE_PROMOGFX.md
Binary file docs/runtime_port/WAVE26_LANE_PROMOGFX.md matches
```

**Lane PROMOGFX's own lane doc contained 2 NUL bytes** — in the sentence explaining
the NUL bug. That is the third time in one wave the fix for this defect reproduced
the defect, and the file it happened in is the one that records the lane's verdicts.

### Why both screens missed it: the POPULATION, not the parser

- `tools/phantom_symbol_scan.py`'s NUL screen examined **1546 source files** and
  printed `none` with live positive and negative controls. The controls were
  sound. **The population excluded `docs/`.**
- The integrator's "independent corroboration" was
  `grep -rlP '\x00' --include=*.cpp --include=*.h --include=*.py ...`, which
  **also excluded `.md`**, and which — separately — **does not work at all**:
  it returns empty even on a file that demonstrably contains NULs.

**So the second method was not a second method.** It agreed with the first because
it was blind in the same place, and it would have agreed anyway because it never
detects anything. Reported to the user at the time as corroboration; it was not.

This is the recorded denominator family: **a denominator failure is the WRONG
POPULATION or the WRONG UNIT, not a missing one.** Both screens named a population
(1546 files; a `--include` list) and both named the wrong one. A screen that
reports `none` while excluding the directory where verdicts live is worse than no
screen, because it licenses the belief that docs were checked.

### The authoritative sweep, and what it found

Python, every file type, no `--include`, 13,903 files examined: **exactly one
offending text file in our own tree** — the PROMOGFX lane doc. The other ten hits
are genuine binaries (a `.psd`, and third-party `renderdoc_build` artefacts).
Fixed; the doc is now readable by `git grep`.

### THE MECHANISM, DEMONSTRATED ON THE INTEGRATOR

The first fix attempt **silently did nothing and reported success**:

```python
d.replace(b'\x00', b'\0')      # intended: NUL -> backslash-zero
```

Inside a `<<'PY'` heredoc the backslash is eaten, so `b'\0'` arrives as `b'\0'`,
**which is itself a NUL**. The replacement was NUL -> NUL. `before: 2, after: 2`
printed, and only re-reading the bytes caught it. The second attempt died with
`SyntaxError: unterminated string literal` — the same eaten backslash, failing
loudly this time purely by luck of where it landed.

**The working form uses no escape sequence at all:**

```python
NUL  = bytes([0])
REPL = bytes([92, 48])          # backslash, '0'
assert d2.count(NUL) == 0       # and ASSERT, because this is the failure that reports success
```

**ADDRESSEE: whoever next edits `phantom_symbol_scan.py`.** Widen the NUL screen's
population to every text file in the repo including `docs/`, and keep the assert
form above in any tool that strips NULs. **CONDITION: the Wave 27 kickoff**, which
will definitely be written.

## RECONCILIATION: DOES THE DEFLATING STAMPER CENSUS INVALIDATE PROMOGFX'S VERDICTS?

**No, and the answer is checkable rather than reassuring.**

Lane PROMOIO measured that `tools/mint_stamper_census.py` buckets a stamp site as
`UNATTRIBUTED (no .pdata) -- NOT OURS` in **45 of 47 cases wrongly**: MSVC emits no
`.pdata` for a frameless leaf, which is the shape of every 3-instruction ctor/dtor.
**It DEFLATES** — it makes work look impossible rather than cheap, which is the
error direction that silently kills promotions.

Lane PROMOGFX's stamper analyses ran on that same tool, so its held-back verdicts
were candidates for being wrong in the deflating direction. They are not:

> `??_7UpdateLoop@Engine@@6B@` — **ours-capable 2, retail 6, unattributed 0.**

**`unattributed 0`.** The verdict rests entirely on sites the tool attributed by
name, with addresses given (e.g. `??0UpdateLoop@Engine@@QEAA@AEBV01@@Z` at
`0x1800317ee`). The defective bucket is empty for that row, so the defect cannot
have reached the conclusion. Same for Geometry and AnimationEvent, whose stampers
are named individually.

**This is only a reconciliation because PROMOGFX printed the unattributed COUNT
next to its verdict.** Had it printed only "2 ours, 6 retail", the question would
have been unanswerable without re-running everything. A count of the bucket you
did NOT use is what makes a verdict auditable after the instrument is impeached.

## A LANE FIX THAT NEVER REACHED ITS BRANCH — NOT MERGED, DELIBERATELY

```
$ git -C .claude/worktrees/agent-<promoio> status --porcelain -- tools/mint_stamper_census.py
 M tools/mint_stamper_census.py
$ git log --oneline -1 wave26/promoio -- tools/mint_stamper_census.py
52c23e6d Wave 21 lane CLASS: ...          <- untouched since Wave 21
```

Lane PROMOIO **modified the tool in its worktree and never committed it.** The lane
reported the defect as diagnosed and the fix as recommended; a partial
implementation exists that no branch carries and no merge could have picked up.

**NOT MERGED, and the reason is not process pedantry:** the uncommitted version is
flagged `"resolved_unattr" is possibly unbound` on three lines — it is incomplete.
Merging an uncommitted, unreviewed, provably-unfinished change to a tool whose whole
problem is that it reports confident wrong answers would replace a known deflation
with an unknown one.

**This project has the matching recorded lesson twice** — *a lane recorded CLOSED
can be entirely UNMERGED*, and *a lane that keeps improving after it reports
invalidates the integrator's build*. This is the first instance where the unmerged
work is a FIX FOR THE INSTRUMENT THE SAME LANE IMPEACHED, which makes it worse than
the usual case: the impeachment is in the wave record and the remedy is not.

**STATE AS OF THIS COMMIT: `mint_stamper_census.py` IS STILL DEFLATING.** Every
stamper ratio quoted anywhere in Wave 26 was produced by the defective tool; the
ones that survive impeachment do so because their `unattributed` count is 0, and
that must be checked per row, not assumed.

**ADDRESSEE: the Wave 27 lane that does promotion work — named in the Wave 27
kickoff, not left to "whoever notices".** Fix is known and stated in
`WAVE26_LANE_PROMOIO.md` §2: resolve an UNATTRIBUTED stamp site against the export
directory instead of against `.pdata`. **CONDITION: before any Wave 27 promotion
verdict is written**, because a deflating stamper census does not produce a wrong
promotion — it produces a promotion that never happens, and nothing detects that.

### SUPERSEDED: the stamper census WAS fixed, and the integrator's own entry above went stale within the hour

**The section above says "STATE AS OF THIS COMMIT: `mint_stamper_census.py` IS STILL
DEFLATING." That was true when written and is now FALSE.** Lane PROMOIO committed the
fix at `7698efdb` — *after* the integrator merged its first tip and *after* the
integrator recorded the fix as existing only as an uncommitted worktree edit. Merged.

The per-site verdict now reads **`UNSCORED`** and says whether the hint resolves,
instead of asserting `NOT OURS`. The lane also put its retraction at the row and in
the tool's printed output, on the stated principle that **a commit message is a
permanently uncorrectable artefact**.

**HOW IT WAS CAUGHT, which is the only part worth generalising.** Not by noticing.
The integrator ran a sweep for uncommitted work across every lane worktree; it
returned **empty**. An absence result is indistinguishable from a dead loop, so it
was control-tested against a **known positive** — the one worktree measured
half an hour earlier as having ` M tools/mint_stamper_census.py`. The control came
back empty too, which is what exposed that the world had moved rather than that the
loop was broken.

**Had the sweep not been control-tested, the wave would have closed on a committed,
confidently-worded, false claim** — in the same document that criticises Wave 25 for
exactly that. The recorded lesson (*a lane that keeps improving after it reports
invalidates the integrator's build*) did not prevent this; a control on an
absence-shaped screen did.

**The general form, which is the fourth face of the control rule this wave has now
collected:** a screen that reports "nothing changed" must be tested against something
that *did* change, or "nothing changed" and "I can no longer see changes" are the
same output. This wave has now hit the control rule from four sides — a control must
corrupt something the check compares; it must not corrupt something it should not; it
must be able to OBSERVE the channel it polices (PROMOGFX's `git grep -l`); and it must
be handed a known positive when its normal answer is empty.

**Every stamper ratio quoted in Wave 26 still came from the DEFECTIVE tool** — the fix
changes what future runs print, not what past ones printed. PROMOGFX's verdicts survive
because their `unattributed` count is 0, which is a per-row check and is recorded above.
