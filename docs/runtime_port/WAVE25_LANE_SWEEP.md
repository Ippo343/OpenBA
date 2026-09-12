# WAVE 25 LANE SWEEP — instrument debt, paid down so attempt 26 is readable

Branch `wave25/sweep`, cut from `wave25/base` at `fc56353c`, then merged with
`wave25/manifest` (`33206d7f`), `wave25/vtgfx` (`d0c504c3`) and `wave25/base`
(`fe332725`) **before** touching `tools/runtime_census.py`, because three lanes
were editing that one file in parallel.

**Every figure here is BRANCH-LOCAL and is not a board figure.** The armed run
measured THIS branch, which is not the merged tip. The integrator must repeat it
after the merge. Wave 21 published a wave number assembled from lane figures and
it was wrong in both directions.

---

## 0. HEADLINE

**The harness now states, on every row of its own summary, what that row's zero
means — and it is derived per run, not asserted in a name.**

| | before | after |
|---|---|---|
| `IO::MemoryStream::MemoryStream` | **18 div / 18 calls, permanently** | **0 div / 18 calls**, and it fails under either control |
| sites whose class the summary states | 0 of 630 | **634 of 634** |
| sites with NO coverage signal at all | 458 of 630 (73%) | **141 of 634 (22%)** |
| `RT_DIFF_REPLAY_PRE/POST` adopters | **0** | 2 (compiled and selected; **not observed running** — §4) |
| promoted `??_7` rows behind a trap guard | 1 of 12 | **12 of 12**, and the refusal **was observed firing 11/11** |
| census-visible phantom symbol rows | unknown | **0**, measured, with both controls alive |

Three runs, all exit 0, `standalone_crash.log` 0 bytes. Lock held ~11 minutes
and released. Evidence in `docs/runtime_port/wave25_sweep/`.

---

## 1. THE EIGHT ITEMS, BEFORE AND AFTER

### Item 1 — the dead `MemoryStream` oracle. FIXED, and two more like it.

**Before:** `runtime/src/io/compressedstream.cpp` neutralised the Encoding
pointer at `+0x10` and three never-written gaps, then compared the **vptr at
`+0x00`, which MUST differ** (`??_7MemoryStream@IO@@6B@` is promoted, so ours is
our minted table and theirs is the DLL's). `RtDiffReportBytes` memcmps the whole
buffer for ONE verdict, so byte 0 decided every call: **18 divergences out of 18
calls, checking nothing.**

**After:** `TagVptrIdentity(copy, off, expected, side)` replaces each side's vptr
with a **verdict on that side's own identity** — 1 when this side holds the table
this side is supposed to hold, 0 when it does not, and a per-side sentinel when
the expectation itself did not resolve.

The byte stays **compared**. Excluding it would have been the second defect: a
ctor stamping the wrong vtable is exactly the failure a ctor oracle exists to
catch. Ours is checked against `ResolveExport` (which follows the promotion),
theirs against **`ResolveExportRaw`** — `ResolveExport` would have compared the
port with itself, which is the trap that left a probe dark for five waves.

Measured, unarmed: `IO::MemoryStream::MemoryStream calls=18 entered=18 divergences=0`.

**Two more sites carried the identical defect, LATENT, and are fixed too:**
`IO::CompressedStream::CompressedStream` and
`IO::CompressedMemoryStream::CompressedMemoryStream`. They are green today only
because their tables are not promoted yet — and `??_7MemoryStream@IO@@6B@` (the
embedded sub-object at `+0x68` of CompressedMemoryStream) **already is**, so that
site was one route away from going permanently red. Lane VTIO is promoting IO
vtables in this very wave.

**Controls, both directions, on a subject the probe COMPARES** (§3.2):

```
BANISHED_RT_DIFF=1 BANISHED_RT_DIFF_CONTROL=memstream-vptr   -> 18/18 red at +0x00
BANISHED_RT_DIFF=1 BANISHED_RT_DIFF_CONTROL=memstream-field  -> 18/18 red at +0x08
BANISHED_RT_DIFF=1                            (unarmed)      ->  0/18
```
`ctl_memstream_{vptr,field}.log`. The second is the **effectiveness half**: a
vptr control alone would leave the other 0x30 bytes untested, which is precisely
how the original site scored 18/18 while testing one byte. The controls corrupt
the comparison COPY, never the live object, so an armed control run is still a
correct game.

### Item 2 — label the NO-COMPARISON sites. DONE, but DERIVED, not written.

**The kickoff asked for the class in each site's NAME. I did not do that, and
the reason is a project lesson:** a class token written into a name literal is an
assertion that goes stale the first time a site gains or loses a comparison
macro, **with no detector** — and it would have put a 133-site rename across ~40
files into a wave where seven other lanes are committing.

Instead the class is **derived per run, inside the harness** (`rt_diff.h` /
`rt_diff.cpp`, ~130 lines, no port file touched):

* `RtDiffSite::entered` — bumped by `RT_DIFF_BEGIN` on every entry. `calls` was
  only ever incremented by `RtDiffReport`, i.e. it counts COMPARISONS, so
  `entered=N calls=0` now separates "**ran N times and compared nothing**" from
  "never ran" — which the summary previously printed identically.
* `RtDiffSite::kindMask` — set AT each comparison macro, so it records what RAN.
  **ATTENDANCE is detected by the COMPILER**: `__builtin_constant_p(EQUAL) &&
  (EQUAL)` is true exactly for a site whose verdict is the literal `true`.
* every summary row now prints `entered=` and `class=`, and a **population
  rollup** prints the four populations separately with a pointed note on
  NO-COMPARISON.

**Column order is load-bearing and was checked, not assumed:**
`tools/rt_matrix_cell.sh` selects dirty sites with `grep -av 'divergences=0$'`,
so a class token appended AFTER `divergences=` would have made all 634 sites read
as dirty. The new columns go BEFORE it. The two joiners
(`rt_diff_run_join.py`, `rt_diff_triage_callerless.py`) were widened to accept
the new fields **optionally**, and they still reproduce Wave 24's numbers
exactly on Wave 24's log (630/99,753,623/35) — that is this change's regression
control.

What the runtime CANNOT decide, and does not claim: whether a `[SHADOW]`
verdict consulted the DLL earlier in its function, and whether an `[ORACLE]`
row's `theirs` came from the DLL at all. `tools/rt_diff_site_classify.py` is
still the tool for that, and both labels say so in their own comments.

### Item 3 — 73% UNKNOWN coverage -> 22%.

**Before:** 458 of 630 sites (73%) never declared `RT_DIFF_WORK`; their coverage
was UNKNOWN, and 48 of them were in the `calls=0 worked>0` state where the body
ran every time and the oracle covered none of it.

**After, measured on the armed run:**

```
[RtDiff] ==== coverage: 493 of 634 sites have an ENTRY count; 141 still have NO
         coverage signal at all (no RT_DIFF_WORK and no entry count) ====
[RtDiff] ==== did-work: 4880208 calls did work across 172 declaring sites;
         462 sites have NOT declared (their coverage is UNKNOWN, not zero) ====
```

**73% -> 22%.** Stated precisely, because these are two different questions and
merging them would be the defect this lane exists to remove:

* `worked=` (semantic work, site-declared) is **unchanged**: 172 declaring,
  462 not. I added no `RT_DIFF_WORK` declarations.
* `entered=` (entries, automatic) now exists for **493 sites**, and for those the
  coverage question is answered arithmetically: entries vs comparisons.
* The residual **141** are the hand-rolled `static RtDiffSite s = {...}` sites,
  which never pass through `RT_DIFF_BEGIN`. They print `entered=?`, never
  `entered=0` — same rule as `worked=?`, for the same reason.

### Item 4 — `RT_DIFF_REPLAY_PRE/POST`: ADOPTED (2 sites), and honestly weak.

Adopted at `UI::SpinnerElement::SetIncrement` and
`UI::ProgressElement::SetCurrentValue` (`runtime/src/ui/widgets_input.cpp`).
Both write ONE scalar into a field neither side allocates, refcounts or frees,
so PRE and POST are equivalent there by construction and the conversion cannot
arm a use-after-free. `SetMinMax` and `SetValue`, two lines away, are **NOT**
converted: they go through `NumberEditElement::SetValue`, which reformats text
through a refcounted buffer — that is Wave 9 lane M's Finding 1 exactly, and
converting it would be arming a crash rather than adopting a tool.

**What this does NOT establish:** neither site registered on Wave 24's route or
on mine — no autostart path reaches a spinner or drives the progress bar through
that setter. So the PRE arm is **compiled and selected but never observed to
run**, and `BANISHED_RT_DIFF_INVERT=1` remains unexercised. The line that would
prove it is `[RtDiff] INVERT arm: UI::SpinnerElement::SetIncrement` on a route
that touches a spinner. I considered deleting the machinery instead; Wave 9
measured a real finding with it, so retiring it would have destroyed a working
instrument to fix a documentation defect.

### Item 5 — `?Serialize@AnimationManager@` STAYS UNVERIFIED, and is now undeletable.

`runtime/types/graphics/Animation.h` keeps the note, and the note's **stale
premise is corrected**: it said "Serialize (0x1800fc190) is still unported",
which stopped being true in Wave 24. The verdict is unchanged — GFXMAT ported
that body and answered Wave 23 GFXBODY's R4 with **no**: it reads only `+0x00`
and `+0x08`, so `+0x10` stays UNVERIFIED and the body named as its answer is not
its answer.

**Enforcement:** `tools/check_standing_notes.py`. It names its subject (3 files,
85,475 bytes, 8 rules over 4 notes), and its `--selftest` carries **both**
controls: the real files must produce 0 findings, and **deleting each needle one
at a time must produce exactly that rule's finding — 8/8 caught** (the
effectiveness counter). An absence screen with no positive shape cannot tell
"nothing lost" from "dead reader", so it is handed both.

It also states what it cannot do: it checks that the TEXT is present, not that
the text is still TRUE — so every entry carries the measurement that re-derives
its claim, printed by `--why`.

### Item 6 — the `0x180157c60` comment. ALREADY FIXED. I changed nothing.

`singleton_ctors.cpp:719-742` already carries the correction, written by **Wave
24 lane SMALLBODY** (`6e14b3f5`), naming both re-derivations and explicitly
noting that Persistence's `0x180155860` is NOT discharged.

```
grep -n '180157c60' standalone/rt_data_mint_tables.inc
  608:  // (unexported, carried by address)  DLL 0x180157c60  7 slots
  1542: { nullptr, 0x180157c60ull, 7, kVSlots2, kVTable2, kVTraps2, ... }
```

**This is the kickoff's own cheap-wins list going stale in one wave** — the
documented pattern (`reference_cheap_wins_list_is_the_stalest_section.md`),
reproduced here at a one-wave interval. One of eight items was already done.

### Item 7 — `[VideoInterface] *** DETOUR FAILED ***`. ANNOTATED, not silenced.

**Before:** at cutover the detour cannot install (there is no `0x1800F79F0` to
patch), so this message was **guaranteed** to print, and its third paragraph
claimed "something MUST call `VideoInterfaceFillDx11Slot()`" — while the ported
`RtVideo_Device_InitializeInterfaces` (`video_interfaces_win.cpp:266`,
registered by name AND address) calls it as its **first statement**. A false
alarm at the one moment it is certain to appear.

**After:** it is not silenced, because with the DLL PRESENT a failed detour IS a
real problem. It **asks the registry** — `RtPortLookupExport(...)`, which
inspects without resolving — and prints one of two messages: "EXPECTED, NOT A
FAULT" with the reason and the real detector (`VideoInterfacePrefilled()`), or
the original alarm, sharpened to say the body is not in the registry either.
The stale Wave 9 paragraph above it is corrected rather than deleted, so the
reader sees what changed. Both spellings are pinned by
`check_standing_notes.py`.

### Item 8 — the census's shadow-site count. DONE, and WIDENED as instructed.

`tools/runtime_census.py --diff-sites` now derives the count with the file's own
comment stripper plus a new `blank_string_contents()` pass, over its own scoped
roots, and prints **code and prose in separate columns**:

```
subject: 1539 *.cpp/*.h files under code/engine/runtime/standalone/dx11,
         comment- and string-stripped
  RT_DIFF_BEGIN        787 code /  20 prose      RT_DIFF_WORK        332 /  30
  RT_DIFF_SHADOW       315 code /  56 prose      RT_DIFF_REPLAY_PRE    3 /   4
  RT_DIFF_BYTES        193 code /  11 prose      RtDiffSite{handrolled} 108 / 1
```

Hand-checked **while the tool agreed with me** (§3.1): raw `grep -rn` over
`--include=*.cpp` gives **366**, and the tool splits that same population into
**314 code + 52 prose**. 314 + 52 = 366 exactly; the remaining 1 of 315 is the
macro's own definition in `rt_diff.h`. Wave 24's 305/49 was the same
decomposition on a smaller tree.

Its control is handed a positive shape (§3.4): a fixture containing every macro
in a line comment, a block comment, a string literal and real code, which the
stripper must split **exactly** — `--selftest-diff-sites`, 4/4, with the count
of bytes the stripper actually blanked as the effectiveness counter. The first
version scored 2/4 and the failure was real: `strip_comments()` deliberately
preserves string CONTENTS (the port scan reads mangled names out of them), so a
macro named inside a string counted as code.

**The widening you asked for — every `(\S+)`-shaped field, not just MANIFEST's.**
Audited all four harness/census parsers (3,410 lines) for capture groups that
cannot match a space, with MANIFEST's pre-fix regex as the **positive control**
(it must be flagged — it is, 2 groups). Four hits, each triaged:

| hit | verdict |
|---|---|
| `runtime_census.py:1714 (\?\S+)` | SAFE — an MSVC mangled name has no spaces |
| `rt_diff_triage_callerless.py:42 export\s+(\S+)` | SAFE — same |
| `rt_diff_run_join.py:129 worked=(\S+)` | **would have broken on MY new columns** — widened |
| `rt_diff_triage_callerless.py:48 worked=(\S+)` | same — widened |

So MANIFEST's defect has **no untriaged twin** in these files; but the same
family nearly bit this lane, from the other end — I was about to insert two
columns between `worked=` and `divergences=`, which both joiners required to be
adjacent. They would have printed a confident **"0 sites"** rather than failing.

---

## 2. THE ADDED ITEM (A) — the vtable promotion guard, extended and FIRED

Lane VTGFX added `RtMintVtableTrapCount()` and made its own new row refuse
itself, flagging the older promotions as this lane's. **It is eleven rows, not
five:** 4 UI (Wave 10B), 1 Video (Wave 18), 5 IO (Waves 19-20), plus
`??_7BinaryFileStream@` — every one of which published **whatever the live
registry held**, unchecked, straight into a live object's vptr.

All of them now go through one `PromoteVtableGuarded()` helper
(`standalone/reg_mint_promoted.cpp`). `?_typeInfo@` rows deliberately do NOT:
they are a single pointer, `RtMintVtableTrapCount` answers `-1` for them ("not a
vtable row", which is not "zero traps"), and routing them through would have
refused all thirty.

**VTGFX said the refusal path had never fired and had no control. It has now
fired, 11 of 11.** `BANISHED_RT_MINT_TRAP_CONTROL=<sym>[;<sym>]|*` makes
`RtMintVtableTrapCount` report a synthetic 99 for the named rows — it corrupts
**exactly the value the guard compares** and nothing else:

```
[RtMint] vtable promotion guard: 11 row(s) evaluated, 11 refused,
         12 synthetic trap injection(s) this run        (armed)
[RtMint] vtable promotion guard: 11 row(s) evaluated, 0 refused,
         0 synthetic trap injection(s) this run  (refusal path UNTESTED ...)  (unarmed)
```

The unarmed line is printed on **every** run, so a future run that has not
tested the refusal says so on its own record. Evidence:
`wave25_sweep/trap_control_run.txt` (armed; provenance stated — it is captured
stdout, not a log file, because the later control runs overwrote the log) and
`run1_debug.log` (unarmed, 0 REFUSING lines).

**What it does NOT check:** that a slot's body is CORRECT. A wrong port passes it.

---

## 3. THE ARMED SWEEP — FOUR POPULATIONS, NEVER MERGED

```
tools/game_dir_lock.sh acquire lane-SWEEP 1800      # bare, never piped
cp standalone/build/reimpl-x64.exe "C:/GOG Games/Banished/"   # md5 a4e467ef... both sides
BANISHED_RT_DIFF=1 BANISHED_W17_ATTEND=1 BANISHED_AUTOSTART=1 \
  BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=120 BANISHED_PAUSEMENU_BUTTON=6 \
  ./reimpl-x64.exe                                  # exit 0, crash log 0 bytes
tools/game_dir_lock.sh release lane-SWEEP
```

Route actually covered, read back out of the log (not assumed): boot -> menu ->
autostart New Game -> in-game -> toolbar pause menu -> Quit -> clean shutdown.
`RtDiffSummary()` runs at shutdown, so **a killed run prints nothing**.

**634 sites, 104,419,139 calls, 17 divergences in 6 sites, ZERO port defects.**

### 3.1 The runtime populations (derived by the harness, this run)

| population | sites | entries | comparisons | diverg |
|---|---|---|---|---|
| ORACLE | 227 | 77,813,300 | 77,820,752 | 2 |
| SHADOW | 148 | 3,378,051 | 3,196,414 | 0 |
| ATTENDANCE | 41 | 6,183,881 | 6,183,878 | 0 |
| UNCLASSED (hand-rolled) | 140 | 16,712,402 | 17,218,095 | 15 |
| **NO-COMPARISON** | **63** | **597,838** | **0** | 0 |
| UNEXERCISED | 15 | 0 | 0 | 0 |

**NO-COMPARISON's 63 sites entered 597,838 times and compared nothing.** Their
`divergences=0` is not evidence, and the summary now says so on the row.

### 3.2 The source populations (Wave 24's screen, re-run on my log)

| population | sites | calls | diverg | calls=0 | c=0,work>0 |
|---|---|---|---|---|---|
| ORACLE | 248 | 78,697,204 | 2 | 2 | 0 |
| SELF-CHECK | 115 | 2,308,897 | 8 | 8 | 0 |
| ATTENDANCE | 38 | 6,183,875 | 0 | 0 | 0 |
| NO-COMPARISON | 135 | 16,712,524 | 7 | 68 | **48** |
| NO-SOURCE-MATCH | 98 | 516,639 | 0 | 0 | 0 |

**The `calls=0 worked>0` trap is still 48 sites** — unchanged, because I fixed no
ports. It is now *visible on the row* rather than only in a post-hoc join.

### 3.3 The two classifications CROSSED — and they disagree

The run now carries its own class, so the same 634 rows are classified twice,
independently. `run1_join.txt` prints the cross-tab. The disagreements are the
finding:

* **57 of the 248 source-`ORACLE` rows never executed an oracle macro** (they
  ran only `RT_DIFF_SHADOW`), and 2 compared nothing at all. Wave 24 said 246
  was an **upper bound** with a hand-sampled 2-of-6 false-positive rate; this is
  the first measurement of the gap rather than an estimate.
* **The source screen over-counts NO-COMPARISON**: of its 135, only **60**
  actually compared nothing. 63 compared through a hand-rolled `RtDiffReport`
  the source screen cannot see, 3 ran SHADOW, 1 ran an oracle macro, 8 never ran.
* 38 of the 98 NO-SOURCE-MATCH residue rows ran real oracle macros.

**Neither screen is authoritative.** The runtime one cannot see a door; the
source one cannot see a hand-rolled call or which branch ran. Quoting either
alone is how a false green survives.

### 3.4 The six divergent sites, each named

| site | class | calls | div | verdict |
|---|---|---|---|---|
| `UI3 pin negative control (sink)` | UNCLASSED | 3 | 3 | declared negative control — correct |
| `DLG2 pin negative control (sink)` | UNCLASSED | 3 | 3 | declared negative control — correct |
| `UI::DialogInstance::StartTimer[negative control]` | UNCLASSED | 9 | 8 | declared negative control — correct |
| `UI::SpriteSheet::TypeName` | ORACLE | 1 | 1 | documented in-source; pointer identity |
| `UI::SpriteSheet::Construct` | ORACLE | 3 | 1 | documented; vtable identity |
| `Object::Factory::Startup [probe, not diffable]` | UNCLASSED | 1 | 1 | Wave 24's unexplained 415-vs-416, **unchanged and still undiagnosed** |

**`IO::MemoryStream::MemoryStream` is absent from this table for the first
time.** Three declared negative controls fired 14 divergences — the run-level
proof that the reporting path is alive, so the zeros elsewhere are measurements
and not a dead pipe.

### 3.5 The attendance ride-along (`BANISHED_W17_ATTEND=1`)

`41 of 63 subject rows had their door opened AFTER init; 22 never did.`
(`run1_debug.log`.) Two results other lanes need:

* **`??_7MaterialInstance@Graphics@@6B@` reads `never init=0 post=0`.** VTGFX's
  newly promoted row's door is **not opened at all** on the autostart route on
  this tree. **This is why I did not run the MaterialInstance A/B** — it is not
  a time decision: an A/B between promoted and unpromoted would have compared two
  runs in which nothing fetches the table, and could not have shown anything.
  The promotion is safe and currently **unobserved**; the route that would
  exercise it is not this one.
* **I could NOT decide DATA's seven `?_typeInfo@` rows.** DATA's watch-table
  additions (the three Achievement rows) are not on my branch, so my run carried
  the 63-row table, in which the six Video `?_typeInfo@` rows read `never` and no
  Achievement row exists. **One `BANISHED_W17_ATTEND=1` run after DATA's table
  lands still decides all seven** — one extra env var on any later armed run.

---

## 4. WHAT I DID **NOT** ESTABLISH

1. **One run, one route.** Boot + menu + gameplay + pause-menu quit. No
   save/load, no stress, no mods route. A namespace exercised only in play got
   one pass, not a sweep.
2. **The INVERT path has never run.** Two adopters exist; neither site
   registered on this route. `BANISHED_RT_DIFF_INVERT=1` is still unexercised
   machinery — less inert than before, not proven.
3. **I added no `RT_DIFF_WORK` declarations.** The semantic-work unknown is
   unchanged at 462 sites. What changed is that 493 sites now have an ENTRY
   count, which answers a *different* question. Do not read 73% -> 22% as
   "coverage improved"; read it as "the unknown was named".
4. **`[ORACLE]` does not mean "consulted the DLL".** It means a macro that
   evaluates THEIRS ran. §3.3 shows the two screens disagreeing on 57 rows and I
   did not adjudicate a single one of them by hand.
5. **`Object::Factory::Startup`'s 415-vs-416 is still undiagnosed**, one wave on.
6. **The guard checks registration, not correctness.** A wrong port passes it.
7. **The trap-control run's log file was overwritten** by the two control runs
   that followed; the evidence file is captured stdout with its provenance
   stated, and the command re-derives it in ~20 seconds.
8. **The ATTENDANCE count (41) is a LOWER bound.** `__builtin_constant_p` is
   one-directional: an expression the compiler declines to fold is recorded as
   SHADOW.
9. **Phantom-literal sweep: 0 census-visible phantoms, with a caveat that
   matters.** `tools/phantom_symbol_scan.py` (new) examined 1,696 distinct
   mangled literals in 1,542 files against the DLL's 4,926 exports; both
   controls alive (a fabricated name is reported absent, a name read out of the
   export table is found). 38 absent literals are PREFIXES of real exports —
   multi-line string concatenation, discharged and **counted**, because a silent
   filter is how a screen stops screening. The 2 genuine absentees
   (`?OnElementEvent@DialogInstance@...` and `?RtSelfTestOnly@@YAXXZ`) are both
   passed as NAMED CONSTANTS, which the census cannot see, so neither mints a
   board row. **The caveat: a phantom passed through a named constant is
   invisible to BOTH the census and to this scan.** VTIO's own fix (assembling
   the name at run time) is not on my branch.

---

## 5. DECLARED SURFACE GROWTH (§1f)

**Zero.** This lane registered no export, no address and no ordinal, and ported
no function. `PromoteVtableGuarded()` changes how existing promotions are
guarded, not how many exist. The `ResolveExportRaw` calls added to the
MemoryStream oracles are inside `if (rt_diff_on__)` and are raw doors that die
with the oracle — they were already the shape this file used for `theirs`.

## 6. TAGS TOUCHED

**None.** No `RT_UNPROMOTABLE` tag was added, removed or re-adjudicated.

Two standing notes were given SCOPE and ADDRESSEE by being pinned in
`check_standing_notes.py`, which is the addressee mechanism itself — a condition
with no addressee never fires, so the addressee here is *any commit that deletes
the text*, and the checker is what makes it fire.

## 7. HANDOVER

1. **The harness's own summary is now the first thing to read**, and the CLASS
   column comes before the divergences column. `NO-COMPARISON` and
   `UNEXERCISED` rows are not passes.
2. **Re-run the two screens on the merged tip.** Both are branch-local.
3. **`BANISHED_RT_MINT_TRAP_CONTROL=*` should be run once per wave** — cheap
   (~20s, AUTOQUIT), and it is the only thing that keeps eleven live promotions'
   guard from decaying back into an unvalidated screen.
4. **One `BANISHED_W17_ATTEND=1` run after DATA's table lands** decides seven
   `?_typeInfo@` rows. Fold it into whatever armed run happens next.
5. **VTGFX**: your promoted `MaterialInstance` table's door is never opened on
   the autostart route. An A/B there measures nothing; find the route first.
6. **NEW INSTRUMENT DEBT, measured this wave:** `tools/game_dir_lock.sh`'s stale
   threshold (1800s default) is **shorter than a legitimate allocator
   measurement**. Lane ALLOC's lock expired as stale at **1802s while its process
   was still alive and had written to the game directory 1 second before the
   check**; the lock then offered the directory to the next lane. Nothing was
   corrupted only because the lane that hit the denial (this one) chose to poll
   rather than kill. **A defect that survives on one agent's judgement is not
   controlled.** The fix is a heartbeat — a holder that is still writing should
   extend its own lease — not a bigger constant.
