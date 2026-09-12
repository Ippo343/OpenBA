# WAVE 29 INTEGRATOR LOG

Running log, appended as lanes report. Base `wave29/base` at `c11511a1`.

---

## ENTRY 1 — lane SLOTS closed, and it REFUTES §1.3 of my own kickoff

Lane SLOTS closed at `wave29/slots` `caa5ff11`, clean tree, no edits after report.

### 1.1 The kickoff's table identities are WRONG and must not be used

`WAVE29_KICKOFF.md` §1.3 reproduced `mint_slot_sweep.py`'s `~adjacent-export`
hints as table identities, with a warning that the hint is an upper bound.
**The warning was correct and insufficient: the hints are wrong on ALL SIX
tables, off by one row.** The three rows I described as the tool "declining to
name" are not that either — the tool prints the `.inc`'s **extent** note where
the **identity** belongs, so `~tail-not-a-pointer` is a name being discarded on
a line the tool already parsed.

The six tables, identified by SLOTS from the **PE export address table** (an
export whose address *is* the table VA), superseding my §1.3:

```
  0x1801550e0   ??_7ParticleInstance@Graphics@@6B@        (NOT Particle@Graphics)
  0x180155040   ??_7ParticleSystemInstance@Graphics@@6B@   (NOT ParticleSystem@Graphics)
  0x180158e10   ??_7SunLight@Graphics@@6B@
  0x180154dc0   ??_7MeshInstance@Graphics@@6B@
  0x180154d70   ??_7EdgeMeshInstance@Graphics@@6B@
  0x180154950   ??_7Stream@Audio@@6B@                      (NOT FileStream@IO)
```

`Stream@Audio`'s pointer run reads 17 long because `FileStream@IO` starts two
slots later with no separator — which is exactly how the adjacent-export hint
acquired the wrong name.

**This is my error, and it is the specific one this project logs most often: I
carried a tool's output forward with a caveat attached instead of deriving the
fact.** A caveat is not a derivation. The kickoff's own warning 1 told every lane
to re-derive my figures; SLOTS did, and that is the only reason the wave did not
build on three wrong names.

### 1.2 THE FINDING THAT OUTLIVES THE WAVE — a fixed defect did not propagate

`tools/mint_slot_sweep.py` parses `git grep -h` output line-by-line with
`RegisterRuntimeExport\(\s*"([^"]+)"`, so a **mangled name split across two
source lines** (C adjacent-string concatenation) yields only the first fragment.

Verified independently by me on `wave29/base`, not taken from the lane:

```
$ grep -rnE 'RegisterRuntimeExport\(\s*$' --include=*.cpp --include=*.inc runtime/ standalone/ | wc -l
49
$ grep -rn 'SetParticle' --include=*.cpp --include=*.inc runtime/ standalone/ | grep -i register
(no output -- no single-line literal registration exists for either override)
```

SLOTS counts 67 registrations invisible to the tool; my narrower probe finds 49
call sites in the multi-line form. **Both figures agree that the blind spot is
large and real; they count slightly different things and I have not reconciled
them.** Consequence in the subject:

```
  UNPORTED -> named trap, the tool says : 16
  UNPORTED -> named trap, the truth     :  7
```

**Nine of the sixteen slots my kickoff handed the lane were already ported** —
six by Wave 27, one by Wave 28. `ParticleInstance` and `ParticleSystemInstance`
are **both 10/10 COMPLETE and have been since Wave 27, and nobody knew.** SLOTS
was four keystrokes from re-porting both `?SetParticle@` overrides.

**The parser bug is not the finding. This is:** `runtime_census.py` was
**ALREADY FIXED** for exactly this multi-line-literal blind spot, and the fix did
not propagate to the tool sitting beside it. Two instruments that both answer
"what is unported" disagreed by nine, and **nothing in this project compares two
tools' answers to one question.**

> **A defect repaired in one instrument does not propagate to the one beside it,
> and the repair leaves no trace that would let anyone notice.** Every previous
> lesson in this project's ledger is about a single instrument being wrong. This
> is about a *correct* fix silently failing to apply — which is undetectable by
> construction, because the fixed tool now gives the right answer and nobody
> re-asks the broken one.

Direction: the sweep's blind spot is **INFLATING** (manufactures work,
self-limiting). The cross-tool gap that let it survive is the mechanism by which
a **deflating** fix could equally fail to propagate, and that would not be
self-limiting. Pushed to lane INSTR, which owns the instrument ledger, and as a
population caveat to VERIFY, GFXBODY, IOLANE and AVBODY.

### 1.3 16 vs 18 was NOT a discrepancy — different units

My kickoff asked SLOTS to reconcile the census's "18 real vtable rows" against
the sweep's "16 unported slots" on the theory that two near-identical numbers
from two tools is where defects hide. The reconciliation came back clean and the
units differ:

- census **18** = vtable **data rows** (promotion work)
- sweep **16** = **slot bodies** (port work)
- decomposition nothing prints: 18 = 12 COMPLETE-blocked-on-promotion + 6 with
  unported slots, of which 2 were already COMPLETE -> **4 tables, 7 slots**
- and the 16 slots were only **13 distinct bodies**

**The question was worth asking and the answer was "no defect here" — while the
real defect sat in the same tool, in the identity column I had not thought to
question.** Recording that, because a check that comes back clean is normally
where a wave stops looking.

### 1.4 What landed

`??_7MeshInstance@Graphics@@6B@` **6/9 -> 8/9**: `?Update@` (343 B) and
`?SetSharedInstanceData@` (280 B). True unported slots **7 -> 5**.

Census `called` **+4**, `ported` **+0** — correct and expected: slots of an
unpromoted vtable move no ported column. All four new `called` rows were declared
in advance at their call sites. **This is the board falling for a third
consecutive wave while the wave succeeds**, exactly as §3 of the kickoff predicts.

Declined: five slots (1458 / 1757 / 900 / 1029 / 1800 B), **all stated as size
judgements in those words, not dressed as blockers** — including `Stream::Run`,
now declined by three lanes on the same honest grounds.

New worst `.pdata` understatement on record: `?Draw@EdgeMeshInstance@` at
**70.3x** (25 B annotated, 1757 B actual). Previous record was 24x.

The lane's first build **failed the §0.2 gate** on a stray `std::printf` in its
own `Summary()` — i.e. the gate caught a change that would have manufactured the
exact warning-10 signal the kickoff warns about. The gate earned its place again.

### 1.5 Pairing result

GFXBODY found a **DEFLATING** defect in SLOTS' v1 extent walker; SLOTS rebuilt it
parent-indexed and the two now agree on 12 subjects including five controls drawn
from three lanes. SLOTS' operand reader found no discrepancy in GFXBODY's bodies
and independently confirmed three claims GFXBODY had made by eye.

**Across the pair: four instrument defects, all four found by the other lane's
author.** That is Wave 28's result reproduced rather than merely cited, and it is
now two waves of evidence that pairing is the highest-yield rule in the kickoff.

### 1.6 Integrator actions

- Kickoff §1.3 table identities: **REFUTED**, superseded by §1.1 above. Do not
  read §1.3 of `WAVE29_KICKOFF.md` for identities; the slot counts there stand.
- New ledger item for INSTR: cross-tool answer consistency. **No screen in this
  project compares two instruments' answers to one question.**
- ADDRESSEE for the propagation question: **lane INSTR, this wave.** If INSTR
  does not take it, it returns to the Wave 30 kickoff author as item 1, not to
  "a future lane".
