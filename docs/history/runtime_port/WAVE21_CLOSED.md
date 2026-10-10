# WAVE 21 CLOSED — the gap is 28% body work, and four well-formed tags were all wrong

Opened 2026-09-11 on `wave21/base` at `018426f2`, cut from Wave 20's tip `5ce25be7`.
Seven lanes, each in a private worktree. All seven reported.

---

## 1. THE NUMBER — and §2 before you quote it

> **CORRECTED AT MERGE. READ §1.1.** The figures first published in this section were
> **1311 / 1124 / 85.7%** and **52 of 187 (28%)**. Those were measured **per lane, in
> isolated worktrees**. On the **MERGED tip they are wrong**, and the corrected numbers are
> in §1.1. The decomposition table below is the **pre-merge** one and is kept only because
> §1.1 is a diff against it.

**The headline is not a percentage. It is a decomposition.**

```
GAP DECOMPOSITION BY SYMBOL KIND -- subject: the 187 called-but-NOT-ported rows
namespace       gap |  BODY  CTOR  DTOR OPNEWDEL  VTABLE  DATA  UNKNOWN
internal         41 |     0     0     0        0       0     0       41
Graphics         29 |     9     1     1        2       8     8        0
System           25 |    11     1     1        0       1    11        0
Video            18 |     0     0     0        0      12     6        0
Achievement      16 |     7     0     1        0       4     4        0
IO               16 |     3     0     1        0       8     4        0
Engine           10 |     3     0     0        0       4     3        0
Object           10 |     0     0     0        0       4     6        0
Audio             7 |     2     1     2        0       0     2        0
UI                5 |     2     0     0        1       2     0        0
Input             4 |     1     0     1        0       0     2        0
ordinal           4 |     0     0     0        0       0     0        4
Application       1 |     0     0     0        0       0     1        0
Save              1 |     1     0     0        0       0     0        0
TOTAL           187 |    39     3     7        3      43    47       45
```

**52 of 187 gap rows (28%) are a function to reimplement.** The rest is 43 vtable
promotion rows, 47 data mint rows, and **45 rows the classifier REFUSES to guess about.**

---

## 1.1 THE MERGED TIP — and the reason this section exists

All seven lanes merged onto `wave21/base`. One content conflict
(`rt_graphics_port.cpp`, two lanes adding a declaration at the same spot; both kept).
Build clean. `unpromotable_slot_check.py` went **RED at merge exactly as lane CLASS
predicted** — 2 UNDECLARED tags and 1 missing scope — and is green again after adding the
tokens the tool asked for. Both new declarations **pass under the exact reading chosen**,
which is what confirms the slot-count reasoning behind them: the checker tests a declared
tag under that reading ONLY, so a wrong declaration fails loudly instead of passing.

**MEASURED ON THE MERGED TIP:**

```
TOTAL   4632   1313 called   1129 ported   9 detour   86.0%
Graphics 599    100 called     75 ported             75.0%   ceiling 94.0%
Video    573    159 called    140 ported             88.1%   ceiling 88.7%
GAP DECOMPOSITION  TOTAL 184 | BODY 39  CTOR 3  DTOR 7  OPNEWDEL 1  VTABLE 43  DATA 46  UNKNOWN 45
=> 50 of 184 gap rows (27%) are a function to reimplement.
```

| figure | published from lane reports | **merged tip** |
|---|---|---|
| TOTAL called / ported | 1311 / 1124 | **1313 / 1129** |
| TOTAL covered | 85.7% | **86.0%** |
| gap rows | 187 | **184** |
| body rows | 52 (28%) | **50 (27%)** |
| Graphics called / ported | 99 / 70 | **100 / 75** |
| Video | 158, `AT CEILING` | **159, no longer AT CEILING** |

### THE FINDING: A CLOSE WRITTEN FROM LANE REPORTS DESCRIBES A TREE THAT NEVER EXISTED

**Every lane measured in its own isolated worktree, where only ITS changes existed.** Each
number was honest about the tree it was taken against. **No lane was taken against the
tree that now exists**, because that tree did not exist until the merge — and the
integrator published §1 from those reports *before* merging.

This is not the denominator failure of §5.6 (wrong population) nor the timestamp failure
of §5.4 (different waves). It is a third variant: **every figure had the right population,
the right unit AND the same wave — and a DIFFERENT TREE.** Worktree isolation is what makes
parallel lanes safe, and it is exactly what makes their numbers un-addable. Note the
direction: the merged figures are BETTER than the published ones, so nothing here would
have been caught by a sanity check looking for an implausible claim.

Two second-order effects the merge produced that no lane could have reported:

- **`Video` lost `AT CEILING`** — 158 → 159 called, gap now 19. No lane touched Video. A
  door opened elsewhere moved it, which is the moving-target property in
  `reference_the_ceiling_percentage_is_a_moving_target.md` acting ACROSS lanes.
- **`Graphics`'s ceiling** is 94.0% on the merged tip, against the 96.0% inherited from
  Wave 20 and the 94.9% lane CLASS measured. **Three different values in one wave, all
  correctly derived.** Quote the set-diff, never the percentage.

> **THE RULE: a wave's headline number is measured ONCE, on the MERGED tip, by the
> integrator. Lane figures are set-diffs for their own lane and MUST NOT be summed,
> inherited into a close, or published before the merge.**

---

## 2. WHAT THIS WAVE DOES NOT CLAIM

- **`TOTAL` did not move.** No lane claims a percentage gain. Six symbols' worth of
  movement happened inside namespaces and is set out per lane in §3.
- **The 28% is a KIND count, not a SIZE or EFFORT estimate.** The table says nothing about
  how big those 52 bodies are, whether they are reachable, or whether a DATA row is
  already minted. Reading it as "72% of the work is easy" repeats the error it exists to
  prevent — in the opposite direction.
- **`Graphics`'s ceiling moved DOWN, 96.0% → 94.9%**, because a row entered
  `unpromotable`. Quote it as a one-row set-diff, never as a regression. The ceiling has
  `called` in both halves and moves whenever a lane opens a door.
- **EXERCISED is not CORRECT.** The one body that reached EXERCISED this wave did so with
  no oracle, compared no pixel, and was counted by ENTRIES not BRANCHES.
- **Attendance is 0 of 8 on the Graphics vtable rows.** No promotion and no live route is
  claimed for any of them.

---

## 3. PER LANE — set-diffs, not percentages

| lane | what moved | what it found |
|---|---|---|
| **CLASS** | census prints the decomposition; `unpromotable_slot_check.py` extended | doctrine CONFIRMED on 13 of 14 namespaces; the denominator screen is NOT buildable |
| **GFXVT** | 8 slot bodies ported; unported slots on its 8 rows **29 → 21**; `AnimationEvent` now 4/4 COMPLETE | the `i < 12` loop is a **cutover blocker**; a 6th census wrapper blind spot |
| **GFXDATA** | **2 rows promoted** with attendance and content matching the DLL | 4 rows HARMFUL with in-DLL writers; 2 READY but unattended, correctly withheld |
| **ENC** | 3 encoding slots + 2 deleting-dtor rows; `ReaderWriter` backlog **0/1 → 1/1 COMPLETE** | item 12's `?` numerator resolved; slot 0's expiry **false AND the wrong shape** |
| **VFY** | probe fixed; comparisons **15 → 82**; 9 bodies now oracle-verified | the probe compared the VPTR; **the negative control corrupted that same byte** |
| **GFXBODY** | 3 bodies ported; 1 EXERCISED, 2 STRUCTURAL; `called` 1311 → 1312 | **`.pdata` is a LOWER bound**; its own prediction FAILED and it led with that |
| **SHOT** | walker extended to the transitive half, with a two-direction control | "29 of 29" is **incoherent**; the price is 14 bodies, not 39 |

---

## 4. THE HEADLINE FINDING: FOUR TAG DEFECTS, ALL WELL FORMED

Three independent lanes each hit a defect in the tag-and-expiry vocabulary, and a fourth
was already on the books. **A syntactic checker for the existing convention would have
passed all four.** Lane CLASS built that checker and confirmed it: it caught none of them.

| defect | the tag | found by |
|---|---|---|
| **no ADDRESSEE** — "the next lane that counts slots on this row" never fires | `??_7ReaderWriter@` `?` numerator | Wave 20, discharged by ENC once a lane was NAMED |
| **CONDITION FALSE** — "unblocks when `??1TextFileStream@` lands" | `??_7TextFileStream@` slot 0 | ENC |
| **CONDITION BESIDE THE POINT** — the thunk pattern never needed that callee ported | same tag, separate defect | ENC |
| **VERDICT RIGHT, PREMISE FALSE** — "there will always be in-DLL readers" is false at cutover | `?_singleton@FileSystem@IO@@` | SHOT |

**The defects live in fields the convention does not have:** an addressee, a route scope,
and a statement of why a condition is RELEVANT rather than merely true. So CLASS extended
the convention (`[CONV:]`, `[SCOPE:]`, `[ADDRESSEE:]`, grandfathered per *(symbol, exact
text)* pair) instead of reporting a green checker.

**ROUTE SCOPE IS NOT COSMETIC — IT INVERTED A PRIORITY.** GFXVT re-tagged
`??_7Geometry@Graphics@@` from `NONE, EVER` to `ROUTE-SCOPED (DLL-PRESENT ONLY)`. Under the
old framing `statusicon_geometry.cpp:341` was an opportunistic unlock. Under the new one it
is a **cutover blocker on the critical path**, because once the DLL is gone that overread
hits OUR storage with nothing left to absorb it. Same evidence, same lane, opposite
ranking. CLASS reversed its own initial position on route scope on that evidence and
recorded the reversal.

---

## 5. DOCTRINE THIS WAVE PAID FOR

**5.1 A namespace gap is not a body count.** Confirmed on 13 of 14 namespaces. `internal`
is the largest raw gap at 41 and is **100% unclassifiable**; `Video` prints `AT CEILING`
with a gap of 18 containing **zero bodies**.

**5.2 A `.pdata` record is a LOWER bound on a function's extent.** MSVC chains fragments
via `UNW_FLAG_CHAININFO`. One body read 50 bytes and is 430 — 8.6x. **This blind spot
UNDERSTATES, which is the dangerous polarity**: an inflating screen manufactures work and
gets noticed; an understating one makes work look CHEAP, and cheap is what gets picked up
without scrutiny. It reranked Graphics on contact: `Renderer::Draw` is 921 bytes,
`BuildParameters` is **1422 with 12 indirect calls**. **Declared, not fixed — 932 of 6045
entries chain. Wave 22 input.**

**5.3 A control can corrupt a byte the probe SHOULD NOT compare.** The known rule is that a
control must corrupt something the probe DOES compare. The mirror is just as dangerous: the
`file_stream_slots_probe` compared the VPTR — which MUST differ between our table and
retail's — and the control corrupted that same byte, so it read CAUGHT and **exonerated a
broken instrument**. Fixing it took comparisons **15 → 82**. The old probe was barely
looking.

**5.4 Two figures from different waves are a changelog, not a disagreement.** The "28 live
vs 36 static" gap never existed: 28 was a **Wave 19** live number, 36 a **Wave 20** static
one. A fresh live run reads **36 of 62**, matching the static scan one for one. Populations
agreed, units agreed; only the timestamps differed, and the handover stripped them.

**5.5 "On an every-frame path" says nothing about whether OUR body runs.** GFXBODY
predicted a defect "should be visible" because `Setup` is on two every-frame paths. Our
body was entered **zero times** on the default route: its callers are the DLL's own `Draw`
functions reaching it by in-DLL call, **which is not a door**. True of the function, false
of our body. **The lane led with the failure and marked its old section superseded rather
than quietly editing it.**

**5.6 The denominator problem is NOT mechanisable, and now there are numbers.** A screen
for counts with no stated population: **precision 0/16, recall 0/3, 52% flag rate**. Every
real failure was a quantity spelled as an English word; every false positive had its
population one clause away. **Both real specimens are uncatchable by construction because
both NAME a population** — a board printing 30 rows for a 29-row array, and 25 NAMES quoted
as 29 ROWS. **The failures are the WRONG population and the WRONG UNIT, not a missing
one.** Stating a denominator is trivial; being right about it is the difficulty, and that
stays a person's job.

---

## 6. WHAT THE INTEGRATOR GOT WRONG

Recorded in full in `WAVE21_INTEGRATOR_LOG.md` §3. In brief:

**6.1 I split one subject across two lanes.** The `FrameCopyImageBuffer` cluster went to
GFXBODY, "what the screenshot hole means" to SHOT. They are the same work. GFXBODY found
it, not me — and then found it is **three** items, not two: both `operator new`s, the
FrameCopy block and kickoff item 5 all need **one missing driver**, the save-thumbnail /
screenshot driver. The kickoff's lane table does not check for overlapping subjects at all.

**6.2 I relayed a wrong ranking.** The kickoff repeated Wave 20's framing that
`?Draw@Renderer@` is the large Graphics row. I had no independent basis and passed it on
because it was in the handover — **which is precisely §4 of the last close, committed by
the person who wrote that rule into this kickoff.**

**6.3 Seven lanes against one game directory cost a lane its runs.** GFXBODY's acquire
timed out at 600s and its ports closed STRUCTURAL. The lock behaved correctly and refused
rather than granting falsely; the scheduling was mine. Resolved by resuming the lane once
the directory freed — one of the three reached EXERCISED.

---

## 7. WAVE 22 INPUTS — EACH WITH ITS RE-DERIVATION COMMAND

**Nothing below is scoped. Re-derive first. Three items in this position were stale in
Wave 20, and this close obeys its own rule.**

1. **`statusicon_geometry.cpp:341` is a CUTOVER BLOCKER and is NOT APPLIED.** The loop
   still reads `i < 12`. Both extents are measured (9 and 9), the "12" has no basis in
   either binary, and the derivation sits at the point of use. Two residuals: retail's slot
   9 is `nullptr` where our stub is not, and the no-dispatch finding is **derived, not
   observed**. *Re-derive:* read the comment block at that loop.
2. **`.pdata` chain attribution — declared, not fixed.** 932 of 6045 entries chain.
   *Re-derive:* `tools/` grep for `.pdata` sizing, then §5.2.
3. **The SCREENSHOT DRIVER unlocks three separate items at once** (§6.1). This is the
   single highest-leverage missing harness on the board. *Re-derive:*
   `screenshot_door_walk.py` plus GFXBODY §7.
4. **GFXVT's two new tags need `[SCOPE:]`/`[ADDRESSEE:]` tokens at merge** or
   `unpromotable_slot_check.py` goes red. One-line fix. *Re-derive:* run the tool on the
   merged tip.
5. **The 6th census wrapper blind spot is diagnosed and UNFIXED by design.** `RtVtable`
   hides 3 live cutover dependencies from `called`; 57 of 2271 mangled-literal sites go
   through a wrapper, **12 unknown identifiers, 9 unclassified**. The prescribed fix is a
   screen that NAMES unknown wrappers, not another pattern — this is the 6th instance
   because each previous fix added one pattern. *Re-derive:* GFXVT §, then the census.
6. **`Setup@MaterialInstance` is EXERCISED but branch-unmeasured.** The counter counts
   entries; the body is two conditionals and may have taken the skip path 1000/1000.
   *Re-derive:* instrument the split. **GFXBODY named this as the first thing a follow-on
   should measure.**
7. **`internal`, the LARGEST raw gap at 41, is 100% unclassifiable.** A section probe
   splits it 29 `.text` / 8 `.rdata` / 4 `.data`. *Re-derive:* the census UNKNOWN
   itemisation.
8. **`System` 25 is the largest gap with real body work** — 11 BODY, 11 DATA. Graphics is
   no longer the answer to "where is the port work". *Re-derive:* the §1 table.
9. **`BuildParameters` (1422 bytes, 12 indirect calls) is the largest single Graphics row**,
   not `Renderer::Draw`. *Re-derive:* unwind-chain extents, §5.2.
10. Wave 20 items **7, 9 and 10** were deliberately unscoped and remain `[NOT
    RE-DERIVED]`. Item 9, the cutover wall at `?PlatformConfigure@Engine@1@`, has now gone
    **two waves** without re-verification.
