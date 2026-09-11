# WAVE 21 LANE VFY — verifying what Wave 20 only dispatched

Owns Wave 20 §8 items 2 and 4, neither re-derived at kickoff. Branch `wave21/vfy`
off `wave21/base` (`5ce25be7`), in a private worktree.

## Headline

1. **Item 2's probe (`file_stream_slots_probe.cpp`) is a genuine oracle**,
   confirmed from source and independently from the binary via Ghidra — not a
   self-check. Running it found a **real instrument defect**: it byte-compared
   a vtable pointer that is *supposed* to differ between our object and
   retail's once the table is promoted, which both produced a false
   "MISMATCH" on the positive run and had silently disabled the copy-ctor's
   own negative control. Fixed; after the fix, **all nine of the promoted
   table's stamped bodies (including four with zero attendance on the only
   live route ever measured) verify clean against retail**, and both negative
   controls fire and are caught.
2. **Item 4's "28 live vs 36 static" gap does not reproduce.** A fresh live run
   at this branch's tip reports **36 of 62**, matching the static count
   exactly — same 62-table population, same definition, same 36 tables **by
   name**. Reading WAVE20_INTEGRATOR_CORRECTIONS.md §C5 closely shows why: the
   "28" was a **Wave 19** live measurement and the "36" a **Wave 20** static
   one — two different instruments run at two different waves, not one
   snapshot. There is no set of eight tables to name because the live count
   simply caught up to the static one in between.

## 0. Re-derivation of the kickoff inputs

`tools/runtime_census.py` re-run at this branch's tip reproduces the board in
WAVE21_KICKOFF.md §2 exactly (TOTAL 1311/1124, IO 216/200) and the promotion
backlog it prints is:

```
11 vtables PROMOTED (reg_mint_promoted.cpp, static scan)
51 unpromoted vtables total (62 - 11), of which the printed table lists all 51
48 of those 51 have at least ONE registered slot (the "%d unpromoted vtable(s)"
   figure the script prints on its summary line counts only THESE)
25 of the 51 are COMPLETE (every non-purecall slot registered)
```

**First finding, not part of either assigned item but load-bearing for how I read
every other number below:** the script's own headline sentence ("N bodies across
48 unpromoted vtable(s)") and its own printed table disagree on the vtable count
(48 vs 51) because they use two different definitions of "vtable in the backlog":
the headline counts only rows with >=1 *registered* slot, the table lists every
unpromoted row with >=1 non-purecall slot (adding 3 zero-ours rows: the unexported
`0x180157c60`, `??_7EdgeMesh@Graphics@@6B@`, `??_7ReaderWriter@IO@@6B@`). Neither
number is wrong; they answer different questions. I flag it because it is exactly
the shape item 4 turned out to have — two truthful counts, two silently different
denominators — and it cost nothing to notice here first.

---

## 1. ITEM 2 — are the eight promoted `BinaryFileStream` bodies correct, not just dispatched?

### 1.1 What the probe actually compares, and against what

`runtime/src/io/file_stream_slots_probe.cpp` (`BANISHED_RT_FSSLOTS_PROBE=1`,
`neg1`/`neg2` for controls) is a **genuine oracle, not a self-check** — verified
from both the source and the binary:

* **"ours"** calls our compiled bodies directly by C++ symbol
  (`RtIo_FileStream_Write`, `RtIo_BinaryFileStream_CopyCtor`, etc.) — not by
  address, so this is not the "ours reaches the body by address and compares
  retail with itself" trap.
* **"theirs"** resolves every address through `ResolveRuntimeRaw(ghidra_addr)`,
  which (confirmed by reading `standalone/resolve_standalone.cpp:827-834`) does
  **pure RVA arithmetic against the loaded DLL module** (`ghidra_addr + delta`)
  and never touches the port registry (`RtExportReg`/`RtAddrReg`). It cannot
  return our own body.
* **Confirmed against the binary with Ghidra** (program `Runtime-x64.dll`,
  `switch_program` result checked before each read):
  * `0x180023ca0` = `BinaryFileStream::BinaryFileStream(BinaryFileStream const&)`,
    body `180023ca0-180023d10` — the real retail copy ctor.
  * `0x1800a8200` = `BinaryFileStream::BinaryFileStream()`, body
    `1800a8200-1800a82e7` — the real retail scalar ctor.
  * `0x180023d80` = the deleting-dtor thunk (`FUN_180023d80`, body
    `180023d80-180023df6`).
  * `0x1800aa8f0` = `FileStream::Create`, body `1800aa8f0-1800aaa09`.
  * `0x1801554d0` has **no function** at that address — consistent with it being
    the `??_7BinaryFileStream@IO@@6B@` vtable **data**, not code.
  All five match the probe's own comments exactly. This is not a shadow-marker
  self-check (`reference_shadow_marker_is_not_an_oracle.md`): the DLL is
  genuinely consulted on the "theirs" side, and the values compared (return
  booleans, positions, sizes, a byte-identical artefact file on disk) are real
  cross-module observations.

### 1.2 A real instrument defect found by actually running it, and the fix

The **first** run (`ctl=0`, before any change here) reported:

```
[FSSlots] MISMATCH copyCtor bytes         byte +0x00 ours=00 theirs=d0
[FSSlots] cases=2 mismatches=1 comparisons=15 file-bytes-compared=11 controlFired=0
```

This looked like a real port defect in `RtIo_BinaryFileStream_CopyCtor`. It is
not. `CaseCopyCtor` byte-compares the **whole 0x50-byte struct**, which includes
the vptr at offset 0x00. Once Wave 20 **promoted** `??_7BinaryFileStream@IO@@6B@`,
`RT_DATA_SLOT("??_7BinaryFileStream@IO@@6B@")` on our side resolves (registry
first) to **our own minted table's address** (`kVTable7`, living inside
`reimpl-x64.exe`), while retail's copy ctor writes the **DLL's own table address**
(`0x1801554d0` + ASLR delta) on the other side. These are two different, valid
addresses in two different modules — comparing them byte-for-byte fails on
*every* run, correct port or not, and always will. Before Wave 20's promotion,
`RT_DATA_SLOT` fell through to the DLL and returned the *same* address retail
used, so this comparison passed by coincidence; the promotion silently
invalidated the probe and nobody re-ran it until now.

Confirmed by low-byte arithmetic: `theirs=0xd0` is exactly the low byte of RVA
`0x1554d0` (page-aligned DLL base contributes nothing to the low byte), i.e. the
DLL's real table address — not a random value.

Worse, the probe's own **CONTROL 2** was built to null out this same byte
(`+0x00`) as its injected defect — a control aimed at a byte that (once excluded)
can never be compared, which is exactly
`reference_control_must_pick_a_compared_subject.md`'s failure shape one level
up: a control whose subject the instrument does not examine reports false
NOT-CAUGHT and indicts a sound body.

**Fix applied** (`runtime/src/io/file_stream_slots_probe.cpp`):
1. Exclude bytes `0x00..0x07` (the vptr) from `CaseCopyCtor`'s byte compare, with
   the reasoning above recorded in a comment.
2. Retarget CONTROL 2 from `+0x00` (now excluded) to `+0x18`, the refcounted
   `_filename` pointer — both sides copy it from the *same* `src` object, so it
   is a legitimate, comparable field.

After the fix, the **same** positive run (`ctl=0`) reports:

```
[FSSlots] cases=2 mismatches=0 comparisons=82 file-bytes-compared=11 controlFired=0
[FSSlots] VERDICT: ours matches the shipped bodies on every comparison above.
```

Comparisons rose from 15 to 82 because the old run's loop **broke on the first
mismatch** (the false-positive vptr byte) and never reached the other ~67
byte comparisons in `CaseCopyCtor` at all — the false positive was also hiding
whatever real coverage those bytes had. This is a second, smaller instance of
`reference_truncated_tool_output_manufactures_calls_zero.md`'s family: a report
that stops early manufactures under-coverage that looks like something else (in
this case, a lone false mismatch instead of a silent absence).

### 1.3 Per-body verification tier

`CaseDriven` exercises: `Write` (x2), `SetCurrentPos`, `GetCurrentPos` (x3),
`GetSize`, `Open`, `Read`, `GetSource`, and the deleting-dtor scalar path (x2).
`CaseCopyCtor` exercises the `BinaryFileStream` copy constructor field-by-field.
Together these are the eight promoted-table slot bodies plus the copy ctor
(which is a stamper of the same table but has no live caller in either binary —
WAVE20_LANE_PROMOTE.md §1). All of them return/produce byte-identical results to
retail on this run (`mismatches=0`).

Cross-referencing WAVE20_LANE_PROMOTE.md §4's live-route attendance table
(`BANISHED_RT_FILE_CUTOVER=1`, gameplay route) fills in the tier each body sits
at **on top of** this probe result:

| body | gameplay-route attendance (Wave 20, unchanged this wave) | this probe (calls=) |
|---|---|---|
| `Read` | 9,453 | exercised, matches |
| `SetCurrentPos` | 3,759 | exercised, matches |
| `GetSize` | 2,847 | exercised, matches |
| `Open` | 2,847 | exercised, matches |
| `Write` | 0 (never dispatched on this route) | exercised, matches |
| `GetCurrentPos` | 0 | exercised, matches |
| `GetSource` | 0 | exercised, matches |
| deleting dtor (scalar) | 0 | exercised, matches |
| copy ctor | 0 (no caller in either binary) | exercised, matches |

**So the honest tier statement is: all nine bodies are now DIRECTLY VERIFIED
against retail by this probe, independent of whether the promoted table is ever
reached in play.** That is a strictly stronger and separate claim from Wave 20's
attendance figures — attendance says a body was *dispatched*; this probe says a
body, dispatched or not, *computes the same thing retail's does*. Four of the
nine (`Write`, `GetCurrentPos`, `GetSource`, the dtor) had **no other route to
verification at all** before this run, since they have zero attendance on the
only live route measured.

**calls= vs attendance, read correctly, per
`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`:** the
gameplay-route "0" figures above are not evidence of a dead port — WAVE20 already
established that route simply never calls `Write`/`GetCurrentPos`/`GetSource`/the
dtor scalar path (the sequence never seeks after the last write, never asks for
the current position, never asks for the source name, and the object is never
closed mid-run). This probe's own driven sequence was built specifically to
force all of them, which is why it, not the gameplay route, is their only
oracle. Read raw, not through `tail` — the full `[FSSlots]` block was grepped
out of the untouched log both times, never truncated.

### 1.4 Negative controls

Both controls were run under `tools/game_dir_lock.sh`, same route
(`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`),
against the fixed probe.

**`BANISHED_RT_FSSLOTS_PROBE=neg1`** (seeks to offset 7 instead of 6 on our side
only — a comparable divergence, never UB):

```
[FSSlots] MISMATCH GetCurrentPos#2        ours=7 theirs=6
[FSSlots] MISMATCH GetCurrentPos#3        ours=12 theirs=11
[FSSlots] MISMATCH Read buffer            the 16 bytes read back differ
[FSSlots] MISMATCH file bytes             file SIZE ours=12 theirs=11
[FSSlots] cases=2 mismatches=4 comparisons=82 file-bytes-compared=0 controlFired=1
[FSSlots] control 1 CAUGHT (4 mismatch(es)) -- the screen can see a wrong body.
  (It diverged enough that the two files differed in SIZE, so the byte
  comparison was skipped -- caught by the value comparisons alone.)
```

CAUGHT, on both the return-value comparisons (`GetCurrentPos`, `Read`) and the
file-artefact comparison independently — the subject (the seek position and
everything downstream of it) is squarely inside `CaseDriven`'s compared set.

**`BANISHED_RT_FSSLOTS_PROBE=neg2`** (retargeted per §1.2 from the excluded
`+0x00` vptr byte to `+0x18`, the refcounted `_filename` pointer — a byte both
sides genuinely copy from the same `src` and that the fixed loop still
compares):

```
[FSSlots] MISMATCH copyCtor bytes         byte +0x18 ours=1f theirs=e0
[FSSlots] cases=2 mismatches=1 comparisons=31 file-bytes-compared=11 controlFired=1
[FSSlots] control 2 CAUGHT (1 mismatch(es)) -- the screen can see a wrong body.
```

CAUGHT. Both controls fire (`controlFired=1`) and are caught
(`reference_control_must_pick_a_compared_subject.md`'s check: the corrupted byte
is one the instrument actually examines, unlike the control's *original*
target before this lane's fix, which would have landed on the now-excluded
vptr and produced a false NOT-CAUGHT indicting a sound body). Together with the
positive run's `mismatches=0`, this is a complete, working oracle: it passes
clean on the real bodies and fails loudly and specifically on two independent
injected defects.

### 1.5 What this does NOT claim

* Not that these bodies are dispatched in an ordinary game session — that is
  WAVE20_LANE_PROMOTE.md's attendance measurement (2,847-9,453 depending on
  body) and it is unchanged by this lane.
* Not that the `deletingDtor`'s `flags & 2` array-delete arm is exercised — the
  probe's own header says it would terminate the process and it is not driven.
* Not a claim about `??_7FileStream@`'s (as opposed to `??_7BinaryFileStream@`'s)
  eight slots as a distinct row — slots 1..7 are byte-identical between the two
  tables (WAVE20_LANE_PROMOTE.md §4) and this probe drives the `BinaryFileStream`
  object throughout, so it verifies the shared bodies but says nothing separately
  about `??_7FileStream@`'s own promotion state (it remains unpromoted per the
  Wave 21 kickoff backlog: `??_7FileStream@IO@@6B@ 8 8 COMPLETE`, unpromoted).

---

## 2. ITEM 4 — the 8-table COMPLETE gap: 28 live vs 36 static

*(section to be completed after the BANISHED_RT_MINT_SHADOW=1 run — the two
populations are named precisely below before any number is quoted)*

### 2.1 Naming both populations before comparing them

**"36 static"**, re-derived from `tools/runtime_census.py` at this branch's tip
(the exact successor of the count Wave 20 quoted, same definition, same
instrument — a **static source scan** of `RegisterRuntimeExport`/
`RegisterRuntimeAddr` call sites over `runtime/ engine/ code/ standalone/ dx11/`
with comments stripped):

```
25  unpromoted vtables where EVERY non-purecall slot has a source-level
    registration  (the "COMPLETE" column of the PROMOTION BACKLOG table)
+ 11  PROMOTED vtables (reg_mint_promoted.cpp), each of which was READY
    (TRAP=0, i.e. complete) as a PRECONDITION of being promoted -- the
    census does not re-print completeness for promoted rows, so this half
    of the 36 is an INFERENCE from the promotion precondition, not a
    number the static scan prints directly for promoted rows
= 36 total vtables, of 62 minted, that the SOURCE TREE says are complete
```

This reproduces Wave 20's 36 exactly, even though the underlying composition
shifted slightly (Wave 20: 26 unpromoted-complete + 10 promoted; now: 25 + 11 —
one row, evidently, moved from the unpromoted-complete column into the promoted
column between the two waves, netting to the same total).

**"28 live"** is `[RtMintShadow] complete vtables : N of 62`, printed by
`standalone/rt_data_mint.cpp`'s `MintVtables()` — which runs **unconditionally
on every boot** (not gated behind `BANISHED_RT_MINT_SHADOW`; that env var only
turns on the *reporting*, per the source at `rt_data_mint.cpp:909-913`) and
computes completeness by asking the **live, populated port registry**
(`RtExportReg`/`RtAddrReg`, i.e. what actually got registered by the time
`RtDataMintInit()` ran during this process's boot) whether a body exists for
each non-purecall slot, over the **same 62-row, same-definition** population as
the static scan (`missing == 0` over non-purecall slots).

**So the two counts share a denominator (62 minted vtables, same definition of
"complete") and the same true population is 62 either way.** This is NOT the
denominator mismatch it could have been (contra WAVE20_CLOSED §C5's caution to
suspect exactly that) — the two instruments read the identical set of rows in
the identical way, one from source text, one from what that source text's
initialization code actually left registered at runtime. Any gap between them
is real: a table that is COMPLETE in the tree but NOT complete live means
something in that table's registration path did not run (or ran and then didn't
survive to when `MintVtables()` checked), not that the two screens were counting
different things.

### 2.2 The measured result: THE GAP DOES NOT REPRODUCE TODAY

Ran `BANISHED_RT_MINT_SHADOW=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` against this branch's own build, under the lock,
with `--snapshot` so the log could not be clobbered by another lane before I
read it (the first, unsnapshotted attempt already got its `tail -20` truncated
by `tools/lane_mint_shadow_run.sh`'s own reporting and had to be redone in
full — see `reference_truncated_tool_output_manufactures_calls_zero.md`; the
raw, untruncated log is what the numbers below come from).

```
[RtMintShadow] vtables            : 62, 578 slots
[RtMintShadow] ground truth       : 578 slot comparisons vs the live DLL, 0 MISMATCH
[RtMintShadow] minted from OUR    : 312 slots, 0 MISMATCH
[RtMintShadow] minted as traps    : 266 (143 purecall by design, 123 not yet ported)
[RtMintShadow] complete vtables   : 36 of 62
[RtMintShadow] effectiveness: 312 of 435 non-purecall slots carry our own code
[RtMintShadow] ==== clean: 0 problem(s) ====
```

**Live complete vtables = 36. Static complete vtables (§2.1) = 36. They match
exactly, over the identical 62-row/578-slot population.** I named the 36 READY
rows the live run printed and checked them one for one against §2.1's static
36 (25 unpromoted-COMPLETE from the census's PROMOTION BACKLOG table + the 11
PROMOTED rows) — **same 36 tables by name**, not just the same count: `Base@
Object`, `BinaryFileStream@IO` [P], `BinaryReader@IO` [P], `BinaryWriter@IO`
[P], `CompiledExternal@Object`, `CompressedFileStream@IO`,
`CompressedMemoryStream@IO`, `CompressedStream@IO`, `ConstantBufferData@Video`,
`ContextData@Video`, `DeviceData@Video`, `Dialog@UI`, `DialogController@UI`,
`DialogInstance@UI` [P], `Element@UI` [P], `External@Object`, `FileStream@IO`,
`Geometry@Graphics`, `ImageBuffer@Video` [P], `ImageBufferData@Video`,
`IndexBufferData@Video`, `Internal@Object`, `MemoryStream@IO` [P],
`PixelProgramData@Video`, `RenderBufferData@Video`, `RenderStateData@Video`,
`SpriteSheet@UI` [P], `Stream@IO`, `Thread@System`, `ToolTipDialog@UI` [P],
`VertexBufferData@Video`, `VertexLayoutData@Video`, `VertexProgramData@Video`,
`VideoProgramData@Video`, `Visitor@IO` [P], `VisitorStream@IO` [P] (36; `[P]` =
promoted). Independently corroborated by the census's own static scan reporting
**312 ported slots**, exactly matching this live run's **312 minted from OUR
code, 0 MISMATCH** — the same figure from a source-text scan and from what the
port registry actually held at runtime.

**So there are not eight tables to name. The gap Wave 20 recorded (28 live vs
36 static) does not exist at Wave 21's tip, on either axis Wave 20 worried
about**: it is not a denominator artefact (both instruments count the same 62
tables, same "every non-purecall slot registered" definition, and now agree
digit-for-digit and name-for-name), and it is not eight specific tables with a
per-table reason.

**What actually explains the 28, per WAVE20_INTEGRATOR_CORRECTIONS.md §C5,
read closely rather than re-quoted:** the "28" was NOT a live measurement taken
at Wave 20's own tip. §C5's own arithmetic table labels it `28 complete − 5
promoted = 23`, and the 23 is explicitly **WAVE 19's** number
(`WAVE19_CLOSED.md §7.5a`, "live registry lookup in a running process"), while
the comparable **36** (`36 complete − 10 promoted = 26`) is **Wave 20's**,
measured by a **static source scan** at `efb2b5a`. Wave 20 §8 item 4 then
compared these two overall-complete figures (28 and 36) as if they were a
same-moment cross-instrument disagreement, when they are in fact **two
different instruments run at two different waves**, with a whole wave of
ordinary port work (Wave 19→20: at minimum lane PROMOTE's `??_7BinaryFileStream@`
promotion and lane SAVETEXT's five `TextFileStream` slot bodies; likely more
that landed without a live-shadow re-run) sitting between them. Nobody re-ran
the LIVE instrument at Wave 20's own tip before filing the gap as unexplained,
so the "gap" is consistent with simply being **stale — the live count catching
up to a static count that already, correctly, described the tree** as more
registrations actually executed and survived to be counted, not two
instruments disagreeing about the same tree at the same time.

**This is a third kind of answer, distinct from both the kickoff's suggested
possibilities ("eight missing tables" or "two denominators").** It is
"two timestamps, reported as one" — the same shape §5.10 and this wave's own
§1 finding (the 48-vs-51 unpromoted-vtable-count split) keep producing: a
number that is true of the population it names, compared against a number
true of a *different* population (here, a different population in *time*)
without either side's report saying so.

### 2.3 What I did NOT check for item 4

* **I did not locate a live-instrument run from Wave 20's own tip** to confirm
  the 28→36 movement happened specifically *between* Wave 19 and Wave 20 (as
  opposed to, say, Wave 20 itself never re-running the live instrument at all,
  so "28" was simply carried forward unchecked from Wave 19 the whole time).
  Both are consistent with the evidence in WAVE20_INTEGRATOR_CORRECTIONS.md
  §C5 and I could not distinguish them without a log I do not have access to.
* **I did not identify which specific registrations, landing between Wave 19
  and Wave 21, moved the live count from 28 to 36** (i.e. I cannot name "the
  eight" as a historical set, only certify that zero exist as a *live* set
  today). SAVETEXT's `TextFileStream` bodies are a plausible partial
  contributor but `??_7TextFileStream@IO@@6B@` itself is **still not** in
  either the live or static COMPLETE list (7/10 ported, 3 traps remain per
  Wave 21 kickoff item 3) — its slot bodies must be feeding some *other*
  table's completeness, or the movement is unrelated to SAVETEXT entirely. I
  did not chase this further.
* **I ran the mint-shadow measurement only on the unattended
  `BANISHED_AUTOSTART` route** (menu → New Game → quit), the same route every
  prior wave used for this instrument. A save/load or a different menu route
  was not tried and could in principle reach additional registrations this
  run's boot sequence does not (none are known to exist, but I did not survey
  for one).
