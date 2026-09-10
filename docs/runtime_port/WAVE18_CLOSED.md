# WAVE 18 — CLOSED. Video is AT CEILING, and the boot wall moved.

**Closed 2026-09-10 by the integrator.** Four lanes, all merged, all closed.
Plan: [WAVE18_VIDEO.md](WAVE18_VIDEO.md) · Kickoff and corrections:
[WAVE18_KICKOFF.md](WAVE18_KICKOFF.md) · Denominator:
[WAVE17_LANE_CEIL.md](WAVE17_LANE_CEIL.md)

## 1. Both objectives, and they were different objectives

The plan scoped Wave 18 as a **cutover** wave and said explicitly *"do not scope
this wave against Video's percentage."* The user asked for the census target. The
kickoff took the census target as a **superset** — the six blocker bodies were 6
of the 23 rows and stayed lane 1 because they were also the wall. **Both landed.**

| | before | after |
|---|---|---|
| **Video** | 150 called / 109 ported · 72.7% | **152 / 134 · 88.2% AT CEILING** |
| **Graphics** | 99 / 66 · 66.7% | 99 / 70 · 70.7% *(pre-paid from Wave 19)* |
| **TOTAL** | 1164 / 938 · 80.6% | **1175 / 971 · 82.6%** |
| **boot wall** | `DeviceData::EnumerateInterfaces` (attempts 23 **and** 24) | **`?PlatformConfigure@Engine@1@`** |

**The identity, not the percentage** (§6.15 / §5): `ported >= called −
unpromotable` → **134 >= 152 − 18 = 134, HOLDS.** `--unported` minus the 18
UNPROMOTABLE rows returns **empty**. The percentage is the lossy rendering of that
and moved for reasons unrelated to progress — see §4.

Gates on the closing tip: `standalone-debug` and `standalone-perf` both build ·
`check-log-strip` 0 · `dispatch_slot_map --check` **110 checked / 0 mismatches**.

## 2. The lanes

| lane | rows | outcome |
|---|---|---|
| **GFXIF** | 9 | All ported. Rows 1-8 VERIFIED by lockstep oracle (55 cases, 0 mismatches, ten effectiveness counters non-zero, four controls caught). Row 9 `AdjustDisplay` **PORTED, NOT BYTE-VERIFIED**, expiry written. |
| **VDATA** | 11 | All seven Tranche C rows promoted **plus four Graphics rows** from the next wave's subject. Seven ImageBuffer slots **PORTED, NOT VERIFIED**, expiry written. |
| **VRES** | 8 | Six rows + `SaveImageToFile` + `PlatformSerialize`. Seven verified; `??1Configuration` **PORTED, NOT VERIFIED**. |
| **GATE** | 0 | Zero rows by design. Measured the wall, scored the prediction, produced three bounded screens. |

## 3. The wall moved — and the prediction was scored, not rationalised

New wall: **`?PlatformConfigure@Engine@1@QEAA_N_NPEAX@Z`**, retail `0x1800a1670`,
EXEC-of-0 at `RtEngine_Engine_Startup+0x777`.

Lane GFXIF pre-registered two deliberately separable halves. **The scoring rule
was fixed in writing (§6.15) BEFORE any data existed**, because GFXIF — the party
whose prediction it scored — had proposed a reading of an ambiguous outcome after
predicting and before measuring. That is the shape of a reconciliation that makes
everyone right and stops the search.

| line | verdict |
|---|---|
| **H1** the wall moves off `EnumerateInterfaces` | **TRUE** |
| **H2** it does not follow that the boot completes | **TRUE** |
| distance travelled (neither half's) | **+2 named `Engine::Startup` steps** |

H1 was established by **address range** — return address `0x1402DEEF9` inside
`RtEngine_Engine_Startup [0x1402DE782, 0x1402DF472)` against
`DeviceData_EnumerateInterfaces [0x1403F632A, 0x1403F6E3C)`, over 1 MB apart —
and both ambiguous shapes the rule could not pre-decide were checked and excluded.
**The rule decided it cleanly and H1 needed no stretching**, which is what the
strict reading was betting on. H2's own *reasoning* is refuted in the specific
(the next null was nowhere in Video), recorded beside the score and marked as no
part of it.

**Set diff, never a count**: 17 − 11 + 1 = 7. And the number most likely to be
misused, defused by the lane itself: **five of the six survivors are not walls** —
init-time resolvers that stored a null and never called it.

**The new wall is softer than it looks.** `PlatformConfigure` is documented at
length in CLAUDE.md as the Win32 launcher dialog, and `HarnessGuardUnattendedBoot()`
already deals with it by clearing a registry bit. The next lane inherits a full
behavioural description and an existing seam — which `EnumerateInterfaces` never had.

## 4. What the wave learned about its own instruments

**Six instruments printed a verdict without stating what they had examined**, and
the direction matters: a *hiding* blind spot conceals work, an *inflating* one
**manufactures** it.

| instrument | claimed | actually examined | fixed |
|---|---|---|---|
| `abi_retslot_audit.py` | "0 suspicious of 49" | two hardcoded files | `fa85957` |
| `dispatch_slot_map --check` | "0 mismatches" | **63 of 110**, plus 1 phantom from a comment | `636b521` |
| `unsatisfied_export_scan.py` | "111 unsatisfied" | blind to named constants **and** split literals → **88** | `42bd07a` |
| `runtime_census.py` ceiling | "Video 88.0%" | a denominator that MOVES | §5 rewritten |
| `array_destroy.cpp` banner | "eleven addresses" | **fourteen** | recorded |
| lane CEIL's subject | "55 rows" | **62** | recorded |

Two of these had **the fix already present**, applied to a sibling:
`dispatch_slot_map`'s comment stripper sat **four lines above** the function that
lacked it.

**The fix that works is a measured COVERAGE denominator, and it was tested.** With
one present, a lane asked whether a green covered its row, ran the tool, found its
row in the unaudited 77, and **refused to cite it**. Widening alone would not have
done that.

**No single screen can catch its own case.** `unsatisfied_export_scan`'s error
surfaced only when a family partition put UI at 28 against a census gap of 5 — two
numbers that could not both be right.

**A fix that SHRINKS a work list needs a control too.** 111 → 88: all 27 departed
names were verified against a real registration. Zero fabricated.

### The ceiling is not a fixed target
`ceiling = (called − unpromotable) / called`, with `called` in **both halves**.
Closing seven trap slots moved Video 150/109 → 152/110: **ported% DOWN, printed
ceiling UP, zero triage change.** Score the identity; report the set-diff of what
entered `called`; never a percentage delta.

### Instruments that refuted themselves — the wave's best output
* **GFXIF**: 50 cases, 0 mismatches, every control caught — and `dtorFree=0`. The
  destructor's free path, the one-instruction difference the cluster turns on, had
  **never run**. Its population shared one source, so the refcount could never
  reach zero.
* **VRES**, five times, each time reporting **NO SUBJECT** rather than a green
  board — and once, *the two sides agreeing* is what indicted the instrument.
* **VDATA**: refused to claim seven slots on `calls=0`, and scored its own P12 as
  worthless because "at least one will fail" is unfalsifiable in the direction
  that counts.

**An instrument that can say "I measured nothing" is worth more than one that can
only say "no difference found."** From outside they are indistinguishable.

**Denominators fail in both directions**: a *union* denominator overstates a blind
control (GFXIF's NC3 false green); an *inapplicable-case* denominator understates
a working one (VRES's "GATE caught 3 of 5", where two cases never reach the gate).

## 5. Stale blockers: four for four, and one was self-inflicted

1. **GFXIF** — the plan's "wall: six functions" was **nine**; `GetImports` ANDs
   over the whole table, so a lane scoped to six would have finished green and
   moved nothing.
2. **VDATA** — CEIL's "no harness route, synthesise a call" ignored
   `BANISHED_FORCE_DEVICE_LOST`, shipped since Wave 9 and still firing. Cost: one
   grep.
3. **VRES** — the `SaveImageToFile` holdback note is false **and refutes itself**:
   its opening line names the caller its closing line denies, both in one comment
   block, for four waves.
4. **VRES again** — the blocker it inherited on the second pass was **its own
   first-pass scoping judgement**.

**A stated blocker is a claim with a timestamp, and the tree's prose is the least
reliable thing in it.**

## 6. Where the advisor was wrong

Recorded because a close that only lists lane errors is not an honest one.

* **Told VRES that `??1Device` depends on GFXIF's `??1GraphicsInterface`** and to
  rebase. It does not — the destroy path inlines through four unexported helpers.
  VRES verified rather than complied.
* **Propagated "40-entry import table"** from GFXIF §3 into kickoff §6.1 without
  checking. It is **50** — the denominator of GATE's headline zero.
* **Two merges landed with placeholder messages** (`tmp4`, `tmp5`). Cause was a
  habit, not forgetfulness: `merge -m "tmp"` intending to amend later. Fixed by
  writing the message to a file and using `merge -F`.
* **Hit the shell-quoting hazard twice** while quoting the rule to others — a
  backtick ate a word from a merge body; a heredoc turned `\n` into a real newline
  and broke a tool mid-fix. The project's rule was filed under *"heredoc"*, so a
  reader with a double-quoted `-m` would not have matched it; the rule has been
  regeneralised.
* **Ran a census over a tree with unresolved conflict markers** and had to discard
  the result. It happened to agree with the correct one, which is why it would
  have been easy to keep.

## 7. What Wave 18 does NOT claim

* `??1Configuration@Video@@` — **PORTED, NOT VERIFIED.** Its body is one store to
  a slot an oracle would have to redirect to observe, so the oracle would be
  testing the redirection.
* `AdjustDisplay` — **PORTED, NOT BYTE-VERIFIED.** It moves the live window;
  expiry is a synthesised hidden window, and it needs *a* window, not *the* window.
* Seven of the ten `??_7ImageBuffer` slots — **PORTED, NOT VERIFIED.** Never
  dispatched on any route driven; `calls=0` is the uncovered column, not a pass.
  Promotion is justified on `TRAP=0` alone.
* **The screenshot cutover hole is NOT closed.** Porting `SaveImageToFile` moved it
  from *no implementation* to *an implementation with four unresolved IO deps*.
* `PlatformSerialize` is verified **by synthetic oracle**; `calls=5` are all the
  probe's and there is no live route.

## 8. Wave 19 — IO and Graphics, not Video

Corroborated by three independent routes: lane CEIL's ceilings, GATE's three
screens (both ranking IO > Graphics > Video), and the new wall being an **Engine**
row. Video is finished at its proven ceiling; **do not follow this wave with more
Video.**

Real remaining work: **IO 34 > Graphics 29**, whose data rows are BLOCKED on
unported vtable slots — **ordinary port work, not a ceiling.**

Inherited work items, each with a named owner:
* The four IO exports `SaveImageToFile` now depends on — `?CreatePath@FileSystem@IO@@`,
  `??0BinaryFileStream@IO@@`, `?Create@FileStream@IO@@`, `?CloseFile@FileSystem@IO@@`.
* `0x180053f00` (`Array<Array<uint8>>::SetSize`) — **Collection lane** work.
* The five unminted IO vtables (a `rt_mint_gen.py` harvest gap, two ATTENDED).
* The three cheapest trap rows: `??_7BinaryReader@` (1 of 26), `??_7Light@` (1 of
  11), `??_7Stream@` (1 of 7).
* `??_7ReaderWriter@` / `??_7Stream@` — settle the abstract-base NO-OP question
  from the shipped bytes rather than from a comment.

**Scope Wave 19 with three separate bounded screens** — predicted walls (upper),
residue (lower), census gap (lossy both ways) — partitioned by class family, with
the arithmetic cross-checked. **A cutover boot is a GATE, not a PLANNER.**
