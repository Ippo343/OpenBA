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

---

## ENTRY 2 — IOLANE's interim check CORRECTS entry 1's mechanism

Lane IOLANE reported a registration check (not yet closed). Its result was the
negative one I asked for — all four IO targets confirmed unported at
`wave29/base`, by four `Register*` verbs, by name AND by address, over 1581
source files, with comments stripped before matching and both controls scored
separately. But its **caveat** is the valuable part.

### 2.1 The concat branch was UNEXERCISED, and saying so was the whole win

IOLANE built its screen with a branch for the adjacent-string-concatenation shape
entry 1 described, found **zero** instances in the whole population, and reported
that branch **UNEXERCISED rather than passing** — kickoff warning 7, applied to
itself. That one honest line is what exposed the error below. A lane that had
written "concat shape: 0 found, clean" would have buried it.

### 2.2 Re-derived: the mechanism in entry 1 is WRONG; the defect is BIGGER

```
A: split literal on ONE line
   grep -rnE 'RegisterRuntime(Export|Addr)\([^)]*"[^"]*"\s*"'  ->    0
B: open paren, literal on the NEXT line
   grep -rnE 'RegisterRuntimeExport\(\s*$'                     ->   49
C: what the sweep regex matches, line-by-line
   grep -rhoE 'RegisterRuntimeExport\(\s*"[^"]+"'              -> 1355
D: total call sites
   grep -rhoE 'RegisterRuntimeExport\('                        -> 1442
```

- **There are ZERO adjacent-string-concatenated registration literals.** SLOTS'
  stated mechanism — "a mangled name split across two source lines (C adjacent-
  string concatenation)" — **does not occur in this tree.**
- The real shape is **B**: open paren, literal on the FOLLOWING line.
- The real size is **D − C = 87** call sites the line-by-line regex cannot match.

**Three of us produced three different numbers for one defect — SLOTS 67, me 49,
the derivation 87 — by each measuring a slightly different thing.** That is the
cross-tool consistency problem from entry 1, reproduced *inside the investigation
of the cross-tool consistency problem*, by the people who had just written it
down. The defect's EXISTENCE was never in doubt; its shape and size were wrong in
every account including mine.

**87 is not yet decomposed and must not be quoted as "87 multi-line literals".**
The population is "call sites where `RegisterRuntimeExport(` is not followed *on
the same line* by a quoted literal", which folds together (a) the 49 open-paren-
EOL sites, (b) first arguments that are a **named constant** — a *separate,
already-known* census blind spot — and (c) variable first arguments. Decomposing
it is INSTR's.

### 2.3 My error, stated plainly

Entry 1 says I "verified this independently rather than taking it". **I verified
the CONCLUSION and inherited the PREMISE.** My grep `RegisterRuntimeExport\(\s*$`
tests form B; SLOTS' claim was about form A. The two agree that the tool is
blind, and they agree for *different reasons*, so my 49 was never a confirmation
of what SLOTS actually said — and I wrote it up as one. This is
`reference_two_checks_one_hidden_premise.md`: a check that reproduces someone
else's answer while sharing none of their reasoning is not independent
corroboration, it is a coincidence that reads as agreement.

**The tell I missed:** SLOTS said 67 and I said 49 and I did not ask why. A
number that lands close enough to feel confirmatory is exactly where this project
has repeatedly stopped looking — and entry 1 §1.3 congratulates SLOTS for asking
that question about 16 vs 18 in the same breath that I failed to ask it about 67
vs 49.

### 2.4 Consequential correction sent

INSTR was building a fix against the mechanism I gave it. Corrected: a
`"a" "b"` concat branch would be **dead code**; handle the next-line form, and
note that a positive control for the concat shape must be **constructed**,
because the tree contains none.

### 2.5 IOLANE's other findings, recorded

- **A DEFLATING bug in its own hidden-return-slot screen, caught only by its
  known positive:** the mangling branch fell back to `sym.rfind("QEBA")` and so
  read the last character of the name for any `UEBA` symbol, reporting a by-value
  `String` return as a reference. Every other row it was asked about happened to
  be right. Same failure family as the sweep's: a fallback that silently
  mis-parses a *neighbouring* mangling.
- **`SetupFileSystem`'s extent was DERIVED, not taken:** `0x1800aebb0..0x1800af5e8`
  = **0xA39 = 2617 bytes** from the instruction stream (first int3 after the last
  ret), agreeing with Wave 28 lane MISC to within one byte. **Not a `.pdata`
  figure**, so entry 1's 70.3x understatement warning does not bite here. The
  body is materially the size advertised — a handed-down figure that survived
  re-derivation, which is worth recording as often as the ones that do not.

### 2.6 Interim lane figures — NOT a board

IOLANE reports IO 234/222 → 239/231 and TOTAL 1453/1294 → 1459/1304 (89.4%),
IO gap 12 → 8, BODY 4 → 1. **Unverified by me and not a published board**: the
lane has not closed, and per kickoff warning 18 no figure is final until I
re-derive on the merged tip with every lane tip checked. Recorded as an interim
claim only.

---

## ENTRY 3 — INSTR refutes entry 2. SLOTS' 67 was right all along.

Lane INSTR fixed `mint_slot_sweep.py` at `wave29/instr` `e3c09f5b`. It now prints
7 traps and the four table identities. **Entry 1's expiry condition is met.**

### 3.1 I ruled out a spans-lines defect with a line-based grep

Entry 2 asserted **"There are ZERO adjacent-string-concatenated registration
literals in the tree"** and instructed INSTR to drop that branch as dead code.
**There are 28.** INSTR caught it; I then verified with a *multiline* matcher
rather than `grep`:

```
(multiline)  RegisterRuntimeExport\(\s*"[^"]*"\s*\n\s*"
   -> 28 occurrences across 16 files

runtime/src/achievement/achievement_state.cpp:447
    RegisterRuntimeExport("?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@"
                          "VAchievementInstance@Achievement@@@Object@@XZ",
```

My entry-2 grep required **both fragments on one line**, and `grep` is line-based.

> **I used a line-based screen to rule out a defect whose definition is "spans
> lines".** It was structurally incapable of returning anything but zero, and
> zero read as a refutation.

That is the absence-screen rule and the name-your-subject rule firing together,
committed by the integrator **while correcting someone else for insufficient
rigour**, one entry after writing "a caveat is not a derivation". INSTR's concat
control is drawn from the tree, not constructed as I advised, and it passes.

### 3.2 The decomposition, and SLOTS was exactly right

INSTR's whole-file, comment-stripped count over all five source roots:

```
  RegisterRuntimeExport( call sites                    1416
    literal SAME line, one fragment                    1312   fine
    literal same line, CONTINUED on next line            17   TRUNCATED
    literal starts on the NEXT line, one fragment        39   INVISIBLE
    literal starts on next line AND continued            11   INVISIBLE
    first argument is NOT a literal                      37   DIFFERENT, known
  affected population = 17 + 39 + 11                =    67
```

**67 is SLOTS' figure, exactly.** Of the four numbers in play **the lane's was
correct and all three of mine were wrong**: 49 counted only the open-paren-at-EOL
form; 87 was `1442 - 1355` with comments *included*, folding in the unrelated
non-literal blind spot, and 1442 also counted the macro definitions in
`rt_port.h`.

Entry 2 scolded SLOTS for a number that was right, on the authority of two
numbers that were not.

### 3.3 TWO shapes, and the smaller one is worse

50 sites were **INVISIBLE**; 17 were **TRUNCATED** to their first fragment, so
the old scan carried **23 PHANTOM names**. A phantom is worse than an omission:
it matches nothing and **cannot be noticed as absent**. The tool now prints a SET
DIFFERENCE against the old scan in both directions rather than a count — diff the
set not the count, applied by the lane without being told.

### 3.4 THE LESSON THAT OUTLIVES THE WAVE, and it is not the one I recorded twice

**The regex was never the bug.** Its `\s*` matches newlines, so applied to whole
files it reads all 1379 literal sites correctly. The defect was **feeding it
`git grep -h` OUTPUT** — line fragments.

> **Fixing the regex would have changed nothing. Anyone who reads this as "a
> regex was too narrow" will write the same defect again.**

The bug was in the **input pipeline**, not the pattern. Entries 1 and 2 both
described it as a pattern defect. That framing is retired.

### 3.5 The cross-tool screen paid on its FIRST RUN

INSTR built the screen entry 1 asked for — the sweep's registered-name set vs the
census's `ported_exports`, set difference **both ways**, printed every run — and
it immediately found a **fourth** sweep defect: **no comment stripper.** It had
registered the names `<lit>`, `<literal>` and `<slot name>`, read out of comments
in `renderer.cpp` and two gfxbody files **that quote this project's own match
patterns while explaining a blind spot.** Harmless placeholders here, but a
comment naming a REAL symbol would mark a minted slot PORTED when nothing
registers it — for a hazard list, the dangerous direction.

`concat_literal` and `strip_comments` are now **IMPORTED from `runtime_census`
rather than re-implemented, so the next fix to either cannot fail to propagate.**
That is the structural answer to entry 1's finding, not a patch to one symptom.

The screen also surfaced 27 names the sweep sees and the census does not. INSTR
checked **all 27, not the printed 8**: every one is a `MemoryAllocator` row gated
behind `BANISHED_RT_ALLOC`. The sweep does not model gating, so each reads PORTED
and **cannot fill a minted slot at cutover.** Recorded with an ADDRESSEE (the
Wave 30 kickoff writer) rather than fixed, because it would move the trap count UP
and INSTR had already moved two boards — a correct refusal to stack a second
variable inside one measurement.

### 3.6 Board movement, all of it MEASUREMENT

```
  census  TOTAL  1453/1294  89.1%  ->  1460/1295  88.7%
          (+7 called from 32 recovered macro-door sites,
           + SIX new unported UI bodies the board could not see)
  sweep   ported 430 -> 439 ; traps 16 -> 7
```

**No work landed in that delta.** And kickoff §1.2's headline — "the remaining
body work in the entire runtime port is SEVENTEEN functions" — **is now 23.**

### 3.7 Entry 1 §1.6 status

- Kickoff §1.3 identities: refuted (entry 1), now **fixed at source**.
- Kickoff §1.2 "17 real bodies": **superseded, 23** (and see entry 5: +1 more).
- Cross-tool consistency item: **TAKEN and DISCHARGED this wave**, helpers
  imported rather than duplicated. Does not return to Wave 30.
- New Wave 30 item, addressee named by INSTR: the sweep does not model gating; 27
  `BANISHED_RT_ALLOC` rows read PORTED and would not fill a minted slot at cutover.

---

## ENTRY 4 — IOLANE closed, and it names a NEW CLASS: a promotion has a LIFETIME

Closed at `wave29/iolane` `ea8a2839`, clean tree, stopped editing.

**Landed:** three `TextFileStream` bodies ported, registered by name AND address,
oracle-verified ours-vs-retail via `ResolveExportRaw` (`calls=2/3/3`, `div=0`).
`??_7PackageFile@IO@@6B@` **PROMOTED** — 9/9, guard-refusal path exercised *on
that row* with a synthetic trap (50->49 promoted, 1 refused), attended `post=1` in
armed AND unarmed runs with a green control, kill switch
`BANISHED_RT_PROMOTE_PKGFILE=0`.

**Declined, obstacle class named:** `?SetupFileSystem@` as a **size judgement** —
extent DERIVED (not `.pdata`) at `0x1800aebb0..0x1800af5e9`, 0xA39 = 2617 B, 714
instructions — plus a new budget fact: **three of its 18 callees are absent from
our tree.** `??_7TextFileStream@IO@@6B@` not promoted **and not for want of
slots** (10/10 COMPLETE): attendance `post=0` in both arms, stampers 3 of 8.

### 4.1 The new class

IOLANE wrote *"checked rather than assumed"* into a promotion warrant about
something it had **assumed**, and the Reconstruct oracle **DIVERGED** on the
first promoted run. Retail's `Reconstruct` destructs through the object's own
slot 0 (ours), then **tail-jumps retail's constructor by an in-DLL rel32 — not a
door** — so retail **re-stamps its own table over an object we built.**

> The known rule is "we cannot get our table in". **This is the mirror: our table
> can be EVICTED after it is in. A promotion has a LIFETIME, not just a route.**

Caught by a `printf`, not by review. Correctness unaffected; coverage is not.
IOLANE also **nearly reported Wave 26's route condition for
`?_typeInfo@PackageFile@IO@@` as satisfied** — it reads `post=1` armed and
`post=0` unarmed, and **its own instrumentation was the consumer.** Not promoted.

### 4.2 Its own instrument defects, both DEFLATING, fixed at the fix site

- the retslot screen reported a by-value return as a reference for any `UEBA`
  symbol (an `rfind("QEBA")` fallback) — found **only** by its known positive;
- `file_stream_probe`'s verdict line hard-bounded `att` to 1..5, which would have
  printed a working control as "NOT BANKED".

### 4.3 A pairing PREMISE that was false, and the pairing paid anyway

AVBODY had not reported; IOLANE self-controlled and said so. Its screen confirms
**both** AVBODY Video rows carry a hidden RDX return slot (two mechanisms, 0x44 B
each) and it sent AVBODY the typedef shape. **Its own three came back `ref/ref` —
the shared trap I built the pairing on does not exist.** The pairing still paid,
in the direction nobody predicted: IOLANE verified its PARTNER's subject, not its
own. Recorded because I chose that pairing on a guess about shared shape, and the
guess was wrong while the pairing was right.

Six runs, every `standalone_crash.log` 0 bytes. Lock unpiped, unbackgrounded,
0 s wait, `assert-held` against its md5 before every launch. No `tools/` touched.

---

## ENTRY 5 — AVBODY: an "unexported internal" was an EXPORT, and Audio is 4 bodies

Interim (lane still running). All five targets confirmed UNPORTED on
`wave29/base` by a scan that strips comments, physically JOINS adjacent literals,
and reports its population (1583 files, 2870 `Register*(` sites, 1467 distinct
names).

### 5.1 A GUESSED known-positive is not a known positive

AVBODY's first run **failed its own control**: it wrote the `Scene::Update`
control symbol *from memory* as `?Update@Scene@Audio@@QEAAXAEBVTime@System@@@Z`;
the real spelling is
`?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z`
(returns `_N` not `X`, takes `Random&` and `Vector3&`). The screen refused to
report and exited 2.

> **A control symbol you RECONSTRUCTED is not a known positive.** It can only
> fail closed by luck; when it doesn't, you have a screen that silently validates
> nothing. Hand a control a symbol you have READ.

It also chose **all three** positives to be split across lines, so a joiner that
handled only single-line names could not sail through. That construction is
better than the one my messages specified.

### 5.2 The scope correction: `AEBA` means PRIVATE, not unexported

`?GetAttenuatedVolume@` calls a helper at `0x1800dfed0` twice, which AVBODY was
about to inline as "an unexported internal". **It is EXPORTED** — ord 3027,
`?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z`. AVBODY then
verified **every** callee VA against the export directory with `pefile` instead
of assuming, and all three "internals" in its subject are exported name doors:

```
  0x1800dfed0  ord 3027  ?GetVolumeForPosition@Scene@Audio@@
  0x1800a0d10  ord 3417  ?Play@Stream@Audio@@QEAAXHH@Z
  0x1801fe728  ord 4797  ?_secondsPerTick@Timer@System@@0NA
```

**So the lane opens ZERO address doors**, and `GetVolumeForPosition` is ported by
name rather than inlined. **Audio is 4 bodies, not 3.** It was not on any work
list because nothing in our tree resolved it — the census cannot see what nothing
asks for. This is the recorded `AEAA`/`AEBA` trap **inverted**: it usually hides
an address door behind the word "unexported"; here it nearly caused a real export
to be silently inlined away.

Extents derived from the byte stream (first `CC-CC` padding), agreeing with
Ghidra in all four cases: `0x44`/`0x44` Video pair, `0xea` GetAttenuatedVolume,
`0x278` PlayPlatform. No `.pdata` used.

---

## ENTRY 6 — GFXBODY: all 8 confirmed, a false positive worth more than the result, and a HARNESS defect

Interim (lane still running). **All eight Graphics targets confirmed UNPORTED**,
so kickoff §1.2's Graphics list holds and `runtime_census.py --unported` is
confirmed sound for this blind spot on this subject.

GFXBODY deliberately **did not reuse SLOTS' scanner** — two checks sharing a
hidden premise are one check — and wrote its own over 3260 door call sites, of
which **56 are multi-line joins, printed as an EFFECTIVENESS CONTROL**: a scan
that joined zero literals is indistinguishable from the line-by-line scan it
replaces and would give the same clean answer for the wrong reason. Controls
scored **separately**, not on a union: a known multi-line positive, a known
single-line positive, a certain-absent negative. With a comment stripper —
**without it, three of the eight would have read as registered** from prose.

### 6.1 The false positive that is the shape that fools people

Its first run reported **4 of 8 ALREADY REGISTERED** and for ten seconds looked
exactly like SLOTS' finding reproducing in a second namespace. All four were
**its own registrations, written minutes earlier.**

> **A tree-reading scan run AFTER you start work cannot answer "was this already
> there" without an explicit exclusion — and it fails in the ALARMING
> direction.** Entry 1's whole finding has this shape; had GFXBODY reported the
> false positive instead of chasing it, I would have had two lanes "confirming"
> a phantom.

### 6.2 The converse of SLOTS' defect lands on GFXBODY

Its own `?GetRootPoseAtTime@` registration is a **2-fragment multi-line literal**
(the name is too long for one line) — precisely the shape the unfixed
`mint_slot_sweep.py` could not see. Neither SLOTS nor GFXBODY fixed that tool, on
the explicit grounds that **two lanes editing one scoring tool makes the delta
unattributable**. INSTR owned it and has now fixed it (entry 3). That is the
coordination working as designed, without anyone arbitrating it.

### 6.3 Extent walker: SLOTS' 70.3x finding changed behaviour

GFXBODY rewrote its extent walker before trusting it. v1 walked forward and
required the successor `.pdata` row to begin exactly at the predecessor's end,
which **alignment padding defeats (DEFLATING)**. v2 is parent-indexed over the
chained `UNWIND_INFO` tail and **reproduces PARTSYS's 157->5242, SLOTS' 25->1757
and SLOTS' 50->900 exactly** before being used on its own subject. On the eight,
v1 and v2 agree — **sound by luck, now sound by construction.**

### 6.4 Scope correction to kickoff §1.2

`?GetSunCamera@SunLight@` is **3775 bytes**, the largest of the eight and larger
than `Scene::AddCulledDrawItems` (2950). Verified as the only export and only
`.pdata` begin in `[0x180107010,0x180107ecf)`, so it is not a swallowed
neighbour. **My kickoff groups it with a Scene/Renderer family that reads as
tractable; it is not.** Declined on size.

### 6.5 HARNESS DEFECT — no lane can prevent this, and it can deadlock the wave

**The Bash tool auto-backgrounds any foreground command at 600s.** GFXBODY asked
for a 960s timeout on a 900s `game_dir_lock.sh` acquire and **the harness
backgrounded it at 600s anyway.**

§0.3 says never background a blocking acquire, because **the lock is held from
the moment it is GRANTED, not when you notice.** A lane that issued that command
and moved on would hold the lock unknowingly and block every other lane — the
exact failure §0.3 names, arriving through a mechanism §0.3 did not anticipate
and no lane can opt out of.

**Mitigation pushed to all running lanes:** never issue an acquire with a timeout
over ~500s; use short acquires in a lane-controlled retry loop; assert you hold
the lock immediately before every launch; if an acquire is backgrounded, treat
lock state as UNKNOWN and assert rather than assuming failure.

**Wave 30 kickoff §0.3 must carry this.** Addressee: the Wave 30 kickoff author.

---

## ENTRY 7 — WALL closed: the wall MOVED, and it refutes a claim I put in its own brief

Closed at `wave29/wall` `5f653d1a`. Done, no further edits.

### 7.1 The fault, by offset and not by inference

`+0x1b7` is the return address of `CALL *RAX` at `0x1403d7ca8`, whose RAX came
from `EntriesSerialize()` = `ResolveAddr(Module::Runtime, 0x180078ad0)`. RAX=0.

**Fault class: an ADDRESS DOOR onto an unported, unexported DLL internal** —
works DLL-present, null at cutover. Four such doors in that body; two registered,
two not. Fixed UPSTREAM with no guards: `0x180078ad0` ported (extent 0x109 B
derived from the instruction stream, not `.pdata`), and `0x180014520` registered
as a sixth `EXTPTR_THUNK` — **the very next `CALL RAX`, 19 bytes on. Fixing only
the first would have bought nothing**, which is the difference between clearing a
wall and stepping one instruction.

A field-order detail worth preserving: retail emits the five ints as
`+0x10,+0x14,+0x1c,+0x20,+0x18` — **the reserved field is FIFTH**, and a
declaration-order loop would silently corrupt every following record.

### 7.2 THE DEFLATING DEFECT, AND IT WAS IN MY BRIEF

`WAVE28_CLOSED.md` §2 reasoned: *"`RtUnresolved` count for the whole run is ZERO
— this is not a resolve miss, so it is a null slot or a null stored pointer, not
a missing registration."* **I copied that into kickoff §2 and into WALL's brief as
a load-bearing premise.**

**It was a missing registration.** `mainloop_run.cpp:13` states in-tree that an
unregistered *address* door prints no `[RtUnresolved]` line.

> **The counter is STRUCTURALLY BLIND to the exact fault it was used to exclude.**

This is the same family as entry 3's: a screen used to rule something out, which
could not have detected it. It is the third time this wave, and the second time
by me. `reference_diagnostic_field_cannot_express_the_answer.md` already records
two crash-log fields with this property; `RtUnresolved` is now a third, and it had
been quoted across two waves.

### 7.3 Measured, with a control and a negative control

- **Control** on the preserved attempt-31 binary reproduced the published wall
  exactly (same offset, frames, registers) — so this is a **moved wall, not a
  moved tree**.
- **Oracle** found by grepping `standalone/` (kickoff warning 13, paying again):
  7151 emissions identical in both side orders.
- **Negative control**: permuting the three order-critical ints gave DIVERGE at
  emission 28 — proving the oracle reaches the body, *can* fail, and confirming
  the field order independently.

### 7.4 The arm: §0.4's recipe was right and its ANSWER was incomplete by two

Attempt 31 also ran `BANISHED_RT_MINT=1` and `BANISHED_AUTOSTART=1`, each proved
by the line printed by the code that read it. My §0.4 named only three variables.
**The method I gave (recover the arm from ATTEST lines, never a log grep) is what
found the two I had missed** — the recipe survived its author's own incomplete
application of it. Captured in `tools/cutover_attempt32.sh`.

### 7.5 The next wall

`RtGraphics_MaterialInstance_Serialize+0x83` -> `RtAddr(0x180061100)` =
`ExternalPtr<Graphics::Material>::Serialize` — the **7th of 25 instantiations of
one family**, 6/25 registered, 19 enumerated. Holdback addressed to *the lane
that runs attempt 33*. A family again, but an enumerated one.

### 7.6 Pairing, in both directions

WALL gave MINT the two missing arm variables — **on MINT's two-variable arm
`RegisterMintedData()` never runs, so all 132 regenerated rows would have bound
nothing and its A/B would have reported "no difference" about an unarmed
subject.** MINT confirmed the correction from the source side and independently
found an evidence-route gap.

Running MINT's `mint_slot_sweep.py` against WALL's subject found its blind spot:
it scores vtable slots only (`grep -c ResolveAddr` = 0), so **SpriteSheet's table
reads 9/9 COMPLETE while one of those bodies took the boot down.**

> **A COMPLETE vtable is not a BOOTABLE one, and no instrument in this wave
> crosses that gap.**

Four further defects recorded with directions: empty evidence dirs from a blanket
`*.log` gitignore; `addr_door_audit.py`'s count is a FLOOR that missed this lane's
own next wall; **the build is not byte-reproducible, so md5 cannot certify a
measured image**; and the Bash 600s auto-background laundering a false lock grant
(entry 6.5, found independently).

---

## ENTRY 8 — INSTR closed: two kickoff sections refuted, and a ledger item that was a TRAP

Closed at `wave29/instr` `9776a690`. Protected-path diff
`c11511a1...HEAD -- runtime/ standalone/ code/ engine/ dx11/` is **EMPTY** — an
instrument lane that touched no compiled input, verified the way kickoff warning
18 asks.

```
                       kickoff §1    closed tip
census called/ported   1453/1294     1460/1295   (89.1% -> 88.7%)
gap BODY                      24            30
slot sweep traps              16             7   (ported 430 -> 439)
```

**Nothing was ported.** `%` fell because `called` grew by 7 rows that always
existed.

### 8.1 Both of my kickoff's headline claims are refuted

- **§1.2 "17 remaining bodies" is 23**, and §1.1's proud scoping correction is
  now itself corrected: six unported **UI** bodies were invisible —
  `Begin@SpriteContext`, `CleanRemovedDialogs`, `ComputeLayout@DialogInstance`,
  `GetMouseCursorIndex`, `Update@DialogInstance`, `UpdateEvents@UserInterface`.
  **UI is 94.1%, not 96.7%.** (Entry 5 adds a 24th in Audio, from a different
  cause.)
- **§1.3's vtable identities are wrong for three of six rows** (entry 1).

I wrote §1.1 as a demonstration of catching a stale figure before dispatch. It
caught one and shipped two.

### 8.2 "Retire the 8 stale verdicts" was a TRAP, and obeying my instruction would have destroyed evidence

I told INSTR to retire or rewrite the 8 stale verdict rows. **Seven were stale
only because their sole caller is `rt_orphan_probe.cpp` — the probe that produced
the verdict.** Obeying the ledger literally would have **deleted the triage of
seven ports that nothing calls.** 8 -> 1; the one genuinely stale row retired with
its evidence preserved.

> A ledger item phrased as an ACTION ("retire these") presumes the finding it was
> derived from is still true. **The instruction outlived its justification**, and
> the lane was right to interrogate the instruction rather than execute it.

### 8.3 The ledger named the WRONG TRIGGER for a real defect

Macro doors: 36 of 780 sites dropped silently (DEFLATING), fixed with a
paren-depth scan plus named-constant resolution, and unreadables now printed.
**But the recorded trigger — "a macro door whose first argument is an inline
function-pointer type" — has ZERO live instances. All 36 are the `kName` constant
shape.** A defect can be real, reproducible and correctly sized while its
*recorded cause* is fiction; the experiment that "proved" it (moving a typedef
moved TOTAL by 1) confirmed a real effect and misattributed it.

### 8.4 The rest

- **Factory family**: deliberately NOT folded in (+103/+103 for no work).
  Decomposed instead into an INTRINSIC cause and a MISSING-CAPABILITY one that
  expires — the right treatment, and it keeps the board honest.
- **Stamper fold-in** — the item WAVE26 PROMOIO explicitly reserved for a
  board-owning lane: 47 unattributed -> **45 scored, 24 ours-capable**,
  **reproducing Wave 26's prediction exactly.** A four-wave-old prediction
  confirmed on the nose.
- `--csv >` left an argparse usage message inside the `.csv`. Fixed.
- dtor-verify BAD -> UNREC (3 rows).
- `mint_slot_sweep` 16 -> 7 traps, identities printed for the first time, and the
  cross-tool screen (entry 3.5).
- 5 new control suites, 33 checks, all green; `--selftest-all` is now 16 screens.

### 8.5 Its own errors, and a pairing scored HONESTLY as one-directional

- **INSTR claimed to have read `factory_byte_oracle.cpp` and had not** — the file
  is uncommitted and was never visible to it. **Retracted to VERIFY in the same
  turn** and recorded; its critique stands only as a design review from a
  description.
- **VERIFY changed INSTR's tree three times; INSTR ran none of VERIFY's
  instruments.** It scored the pairing **one-directional** rather than claiming
  mutual validation. That is the honest version of the metric I have been
  celebrating, and the first lane this wave to report the pairing as partly
  unfulfilled instead of rounding it up.
- It declined VERIFY's bit-flip request because it requires editing `runtime/`,
  referring it to a body lane — correct refusal for an instrument lane whose
  whole claim is an empty protected-path diff.

---

## ENTRY 9 — IOLANE edited after reporting, declared it, and the declaration was RIGHT

New tip `26896fb2`. IOLANE announced the late edit loudly per warning 18 and
proved it mechanically. **I verified it independently rather than accepting the
lane's own diff:**

```
$ git diff --stat ea8a2839 wave29/iolane -- runtime/ standalone/ code/ engine/ dx11/
  (EMPTY)
$ git diff --stat ea8a2839 wave29/iolane
  docs/runtime_port/WAVE29_LANE_IOLANE.md          | 194 ++++--
  .../wave29_iolane_retslot_screen.py.txt          |  67 ++-
  2 files changed, 245 insertions(+), 16 deletions(-)
```

Documentation only; the code tree I will measure at merge is byte-identical to
the reported commit. **This is Wave 28 §7.5's rule executing as designed for the
first time** — there, a lane edited after reporting, the integrator did not
detect it, and the lane's own second report did. Here the lane declared it
unprompted, and the one `git diff --stat` separated "a late commit exists" from
"a late commit invalidates the measurement".

### 9.1 What the late edit FIXED, and it was a wrong prescription sent to a partner

IOLANE had told AVBODY — and written into its own §7.1 — that their by-value
`String` accessors need the hidden return slot as the **first parameter**. **That
is the FREE-FUNCTION rule.** For a MEMBER function MSVC x64 leaves `this` in RCX
and puts the return pointer in **RDX**.

The prescription would have made the body dereference the caller's `String` slot
as a `ShaderProgram` and write 8 bytes through `self` — **a wild write**, i.e.
wrong in the worst available direction, and exactly the hazard the kickoff named
for this pair. AVBODY refuted it; IOLANE **re-read `0x18004e4b0` itself rather
than taking the correction on trust** (RCX dereferenced at +0x30, RDX stored
through, RDX returned).

### 9.2 THE FINDING — a present/absent control cannot validate an ORDERING claim

AVBODY's diagnosis, and it is the sharpest instrument lesson of the wave:

> **A control that scores a binary present/absent verdict cannot validate an
> ORDERING claim derived beside it.**

IOLANE's screen had two mechanisms, both answering *"IS there a hidden slot?"*,
and **both were correct**. The ordering claim sat next to them and **inherited
their credibility**. That is
`reference_a_measured_number_beside_an_asserted_one.md` — and IOLANE walked into
it *in the same message in which it was instructing AVBODY to control their own
screen*. Fixed by adding a third column measuring **which register is
dereferenced vs stored through**.

### 9.3 And a second one INSIDE the fix, which generalises further

The new column's first run scored the existing known positive as
`this=? slot=rdx` and IOLANE nearly read that as a pass.
**`?GetSource@Stream@IO@@` is the abstract base and never touches RCX at all**,
so it is structurally incapable of exercising half the mechanism it controls.

> **A known positive can be a genuine positive for one half of a screen and blind
> to the other half.** This is the union-control defect appearing *inside a single
> control* rather than across two screens — a case the existing rule
> ("score each screen separately") does not cover, because there is only one
> screen.

Replaced with `?GetName@PackageFile@IO@@`, which derefs RCX at +0x40 **and**
stores through RDX.

### 9.4 Pairing score, reported honestly in the partner's favour

```
  defects IOLANE's own controls found in its work:   2  (both by a control
                                                        firing, neither by review)
  defects AVBODY found in IOLANE's work:             2  (the argument order;
                                                        a weak size argument)
  defects IOLANE found in AVBODY's work:             0  (corroboration of their
                                                        subject by a different
                                                        method -- not a defect)
```

The kickoff's pairing claim reproduces, and the lane scored it **against
itself**. Note the asymmetry is the opposite of entry 8's, where INSTR reported a
one-directional pairing against itself too. **Two lanes this wave have declined to
round a pairing up.**

### 9.5 One claim got WEAKER, recorded as a non-claim

The retail `System::String` **RELEASE** idiom IOLANE transcribed still rests on
**one** reading of the bytes; AVBODY has no body containing an inlined release and
could not corroborate it. The **ADOPT** half is now corroborated across two
namespaces. That makes IOLANE's reason #1 for declining
`?SetupFileSystem@Modification@IO@@` — half of it is inlined release, where an
error is a **non-reproducing use-after-free** — *stronger* than when written.

**ADDRESSEE: whoever takes `SetupFileSystem` in Wave 30. Treat the release idiom
as UNCONFIRMED.** A lane recording that its own supporting evidence got thinner,
while the conclusion it supports got firmer, is the behaviour this log exists to
capture.

---

## ENTRY 10 — MINT closed: the regeneration landed, and a FLAT BOARD was correct

Closed at `wave29/mint` `b01654a0`, clean tree, lock released. **Two commits: the
regeneration ALONE (`70c43851`), then docs-only** — the "do not fold it into
another change" instruction executed literally.

`standalone/rt_data_mint_tables.inc` regenerated; `--check` now reads **UP TO
DATE** (132 vtables / 104 BaseTypeInfo / 39 storage).

### 10.1 Staleness DECOMPOSED, and the generator's own stdout is misleading

`--check`'s ENTERED/LEFT uses one regex, so MINT classified by **enclosing table**
instead: `kMintVtables` 64 -> 132 (+68, 0 left); **TypeInfos and Storage
identical**. And:

> The generator's "+45/+4 BaseTypeInfo closure" stdout **reads like 49 new rows
> and is not a delta at all** — it describes *this run's derivation*. **Only
> diffing the file refutes that.**

A progress line that reads as a result is the same family as entry 3's phantom
names: output that cannot be distinguished from the thing a reader wants it to be.

### 10.2 Purely additive, PROVED with a control

0 of 64 surviving rows changed in slot count, `sizeProof`, or slot targets; the 59
textually "changed" rows differ **only in array index**. Control: one slot VA set
to `0xdeadbeef` flagged 1/64, so the screen is live rather than vacuously green.

**The exclusion list is empty, with an argument rather than by default:** minting
is layout-only, and a row goes live unarmed only if an unconditional publisher
names it (`reg_mint_promoted.cpp`, `CUTOVER_REQUIRED`). **0 of 68** are named, and
the control caught an injected known positive. That is the right shape for "I
excluded nothing" — an argument plus a control, not an absence of findings.

### 10.3 A flat board was the CORRECT expectation, not a shortfall

Census CSV **byte-identical**. Build gate passed; manifest gate OK both sides;
DLL-present run reached a full world build.

> **A minted row is a data descriptor, not a resolve door**, so it cannot move a
> census keyed on doors.

This is the second wave running in which the correct expectation for a large,
real landing was **no board movement at all** (Wave 28's factory family was the
first). Anyone scoring this lane on the board would score it at zero.

### 10.4 The cutover A/B, and why it does NOT contradict WALL

Both arms attested; **both faulted at `RtUi_SpriteSheet_Serialize+0x1b7`** — the
wall moved in neither direction, which is the correct result for a layout-only
change.

**This is NOT in conflict with entry 7, where WALL cleared that exact wall.**
MINT's tree is rooted at the kickoff commit and does **not** contain WALL's fix.
Two lanes reporting different walls in the same wave is exactly the shape that
reads as a contradiction later; recording the reconciliation now.

MINT symbolised the fault **in each image separately**, because the two exes have
different layouts and equal raw offsets would otherwise be luck. That is the
position-dependence rule applied to a crash address rather than to a byte diff.

### 10.5 Six instrument defects, deflating first

1. **§0.4's attempt-31 evidence DOES NOT EXIST** — a blanket `*.log` gitignore ate
   it. My kickoff cited that directory as the arm's source of truth. (WALL found
   this independently; two lanes, same hole.)
2. Census backlog **prose (104) undercounts its own table (110)** — the gap is
   exactly the 6 newly minted zero-body tables.
3. **`mint_slot_sweep.py` reads `git show HEAD:`**, so it silently returns the
   **pre-change** answer for uncommitted work. A lane measuring its own
   in-progress edits gets yesterday's number with no warning.
4. **A backgrounded acquire laundered a trailing command's exit 0 into a false
   GRANT.** MINT counts **three hits this wave** and calls it wave-level. This is
   entry 6.5 confirmed by a second lane through a *different* mechanism — not the
   600s cap but a pipeline exit status. **My §0.3 warned about piping and about
   backgrounding as two separate hazards; they compose into a third.**
5. **The build is not byte-reproducible**, making `assert-held` unsound as
   evidence of "built from my source". (WALL found this too.)
6. A mangled-name `strings` grep is not a valid discriminator — which bears on my
   own §0.2 build gate, where `strings` is one of the four checks.

### 10.6 Its own errors, all three consequential

- **It built a TWO-VARIABLE cutover arm and would have measured an UNARMED
  mint** — `RegisterMintedData()` runs only under `BANISHED_RT_MINT`, so all 132
  regenerated rows would have bound nothing and the A/B would have reported "no
  difference" about a subject that was never armed. **WALL caught it**, and MINT
  records that it *had already read the refuting line and did not apply it*.
- It **nearly certified the ordinary path on an `AUTOQUIT` run that never built a
  world** — exit 0, empty crash log, zero `[AutoStart]` lines. That is
  `reference_autoquit_overrides_autostart_newgame.md` arriving on schedule.
- **Its worktree was checked out at a STALE COMMIT**, and following my kickoff's
  literal `git checkout -b wave29/mint` would have invalidated every figure in
  the lane. See entry 11.

### 10.7 Pairing, both directions, with a result that QUALIFIES its own numbers

WALL corrected MINT's arm (decisive). MINT sharpened WALL's `addr_door_audit.py`
defect from "40 is a floor" to **total blindness over data-driven doors** — it
would report 0 for a file describing 1312 — and closed WALL's open question for
all 19 `ExternalPtr` addresses by section disjointness, which **WALL then
correctly narrowed to *address* collision only.**

And WALL's return finding **qualifies a number MINT itself published**:

> **A COMPLETE vtable is not a BOOTABLE one**, so MINT's "60 of 132 COMPLETE" is
> **not readiness.**

A lane publishing a figure and then, in the same report, recording its partner's
reason why that figure does not mean what it looks like.

**Lock wait: ~16 minutes** across two acquires (one DENIED-TIMEOUT, one granted).
WALL's priority was enforced by the tool's own queue, not by etiquette.

---

## ENTRY 11 — I AUDITED EVERY LANE'S BASE, because MINT was handed a stale one

MINT reported that its worktree was checked out at a **stale commit**, and that
obeying my kickoff's literal `git checkout -b wave29/mint` would have rooted the
lane at the wrong tree and invalidated every figure in it. MINT caught this
itself. **If it had not, I would have merged a lane whose measurements were taken
against a different base than the one I published.**

That is not a lane-local risk, so I audited **all eight**:

```
kickoff commit c11511a1
  wall      tip 69c39061   merge-base c11511a1   OK
  mint      tip b01654a0   merge-base c11511a1   OK
  verify    tip c11511a1   merge-base c11511a1   OK  (no commits yet)
  instr     tip 9776a690   merge-base c11511a1   OK
  gfxbody   tip c11511a1   merge-base c11511a1   OK  (no commits yet)
  slots     tip caa5ff11   merge-base c11511a1   OK
  iolane    tip 26896fb2   merge-base c11511a1   OK
  avbody    tip c11511a1   merge-base c11511a1   OK  (no commits yet)
```

**All eight are correctly rooted.** The hazard was real and was absorbed by the
one lane that hit it.

> **A kickoff instruction that assumes the harness put you where you think it did
> is an unstated premise.** `git checkout -b <branch>` names the branch and says
> nothing about the commit — it inherits whatever HEAD happens to be. Wave 30's
> kickoff must say `git checkout -b wave30/<lane> <explicit-sha>` **and** require
> each lane to print and report its merge-base before doing any work.

### 11.1 WALL also edited after reporting — declared, docs-only, verified

```
$ git log --oneline 5f653d1a..wave29/wall
  69c39061 WAVE 29 LANE WALL: DOC-ONLY amendment -- MINT's final measured partner result
$ git diff --stat 5f653d1a wave29/wall -- runtime/ standalone/ code/ engine/ dx11/
  (EMPTY)
```

Docs only, and the commit **labels itself** as such. Two lanes have now edited
after reporting this wave; both declared it, both were clean, and **I verified
both myself rather than accepting the label.** Warning 18 is working, and the
`git diff --stat` scoped to the source roots is doing exactly the job Wave 28
§7.5 designed it for: separating "a late commit exists" from "a late commit
invalidates the measurement".

---

## ENTRY 12 — AVBODY closed: VIDEO'S BODY SURFACE IS CLOSED

Closed at `wave29/avbody` `db5cc9c4`, clean tree, no edits after report.
Protected-path diff vs base: 4 files, 1361 insertions, all its own.

**Five bodies ported and registered**, verified by **49 sweep comparisons against
the SHIPPED exports** via `ResolveExportRaw` — **registry bypassed, so this is a
real ours-vs-retail oracle and not a self-check** (the trap that left ~250
`RT_DIFF_SHADOW` sites unable to catch a shared misunderstanding). **Zero
divergences, 6/6 negative controls caught, scored SEPARATELY** rather than on a
union.

```
  ?GetConstantName@ShaderProgram@Video@@  0x18004e4b0  ord 2507
  ?GetTextureName@ShaderProgram@Video@@   0x18004e420  ord 2971
  ?GetAttenuatedVolume@Scene@Audio@@      0x1800dfde0  ord 2453
  ?OnSoundEnded@Scene@Audio@@             0x1800dffe0  ord 3283
  ?GetVolumeForPosition@Scene@Audio@@     0x1800dfed0  ord 3027   <- NOT on my list
```

### 12.1 The identity, which is not the percentage

```
  Audio  35/29  82.9% ->  36/32  88.9%
  Video 162/142 87.7% -> 162/144 88.9%   AT CEILING
  TOTAL 1453/1294 89.1% -> 1454/1299 89.3%
```

> **Video's BODY surface is CLOSED.** Its entire remaining gap is the 18 rows
> already proved unpromotable. Audio goes 3 bodies -> 1.

**AVBODY refused to quote its board against INSTR's re-based 1460/1295**, on the
grounds that its tree lacks INSTR's fix and doing so would compare two
instruments. Same tool both sides, stated as such. That is the discipline entry 3
says nothing in this project had — arriving from a body lane, unprompted.

### 12.2 Ledger defect #7 reproduced CONCRETELY, on the counter that mattered

`RtGraphicsWave28GfxbodySummary()` has **ZERO callers** — and it is the counter
that would have reported AVBODY's own Video bodies' live route. **It would have
read 0 and concluded "never called".**

AVBODY's own summaries print from a **static destructor, UNCONDITIONALLY**, so
"never ran" and "ran zero times" are **distinguishable**. By contrast
`audio_scene_update.cpp`'s `if (g_updates)` guard collapses both into the same
silence.

> A summary guarded by the counter it reports **cannot distinguish zero from
> never**. Print unconditionally, or the zero means nothing.

This is warning 10 paying off with a named instance, and it retroactively
qualifies attendance zeros in earlier waves.

### 12.3 Its own error, and the best-stated control lesson of the wave

AVBODY **asserted the `Camera::operator=` width trap applied to `OnSoundEnded`**,
built a control for it, and the control reported **NOT CAUGHT**. The control was
**right**: `OR` with the constant 2 is **width-invariant**, so the control
corrupted nothing.

> **Applying a kickoff warning as a blanket rule, rather than checking its
> mechanism, MANUFACTURED a hazard.** And: **a not-caught control indicts the
> CONTROL first** — here it correctly indicted the *premise* instead.

This is the mirror of the recorded rule that a negative control corrupting a
value nothing compares indicts a sound instrument. My §2.2 handed every body lane
that trap as a standing warning; **AVBODY is the lane that checked whether the
mechanism applied before obeying it.** Comment and control corrected in place.

### 12.4 A faithful transcription that is NOT theoretical

Retail's `OnSoundEnded` **not-found path writes `channel+0x5f`** — the MSB of
`platformChannel[7]`, **one byte before the flags array it indexes**.
**Transcribed, not guarded**, per the project's standing rule that retail's
unguarded writes are load-bearing.

Measured attendance proves it is live, not a curiosity: `OnSoundEnded=16` with
`notFound=4` on an ordinary autostart run. Audio is properly exercised —
`GetVolumeForPosition` = **1,052,286 calls**, the bonus export from entry 5 now
shown to be one of the hottest bodies in the namespace.

**AVBODY reports its two Video counts of 8 as ITS OWN SWEEP, not gameplay
attendance**, explicitly. A lane distinguishing "my instrument called it" from
"the game called it" without being asked.

### 12.5 `PlayPlatform` declined, and a prerequisite that EXPIRED

Declined as a stated **size-and-evidence judgement** (632 bytes, six indirect
XAudio2 calls, effects in the driver), decoded in full regardless. §6.2 sketches
a **fake-backend oracle** that would retire the "UNCOMPARABLE" verdict: synthesise
the engine/voice vtables, call **RETAIL's** `PlayPlatform`, and it hands you the
exact `XAUDIO2_BUFFER` it built — **no hardware required**. ADDRESSEE named.

> **Half of `WAVE9_LANE_ENGIO`'s stated prerequisite has EXPIRED**: it asked for
> `OnSoundEnded` first, and `OnSoundEnded` is now ported.

Warning 3 firing on a twenty-wave-old holdback — a blocker naming a missing
capability expires, and this one did.

### 12.6 The lock warning worked, and was verified independently

3 attempts, **~19 minutes** behind MINT and VERIFY. **Attempt 1 (900s) was
auto-backgrounded by the harness at 600s exactly as warned — AVBODY KILLED it and
confirmed with `assert-held` that it did not hold the lock.** Attempts 2 (240s,
denied) and 3 (280s, granted at ~150s). Held ~7 min, released and verified
released.

It verified ownership with an **independent `assert-held` rather than the
acquire's exit status, because it had piped that call and a pipeline exits with
the LAST status.** Both §0.3 hazards live, both handled, and the mitigation I
pushed mid-wave is confirmed effective by the lane that hit the exact failure.

---

## ENTRY 13 — VERIFY closed: the debt is MEASURED, and "attendance 0" meant SILENCE

Closed at `wave29/verify` `28e8df93`. Its substantive report never reached me as
a message — I read the deliverable directly, which is the correct fallback and is
recorded so nobody later assumes I was briefed.

Protected-path diff vs base: **585 insertions across 8 files** — a real code lane,
including `standalone/factory_byte_oracle.cpp` (357 lines).

### 13.1 DEFECT #7 WAS DEFLATING, AND IT HID FOUR LIVE BODIES

Three lane `*Summary()` functions were written and **nothing called them**:
`RtGraphicsWave24GfxmatSummary`, `RtGraphicsWave27GfxbigSummary`,
`RtGraphicsWave28GfxbodySummary` — **18 registered rows across three waves**,
every one behind a **default-on kill switch**, whose attendance line had never
been printed by any run in this project's history. Each carries, directly above
it, the standard comment promising *"a run that entered none of these says so
rather than staying silent."*

Wired. The numbers no run had ever printed:

```
[GFXBIG27]  Draw = 13,125  (entries 26,249  sorts 39,373)
            AnimationState::Update = 17,950
[GFXBODY28] BeginDrawGroup = 26,249
            Camera::operator= = 52,486
```

> **Four of the 18 rows were LIVE and had never appeared in any published
> figure.** The other 14 are genuine zeros — **and those zeros had never been
> printed either**, including GFXMAT24's oracle *effectiveness* counter, the one
> number that says its differential oracle compared nothing at all.

**`Camera::operator=` is the row my §2.2 warns about by name** — the one that
"writes three bytes retail leaves alone into a live Camera 3.3 million times a
session". It runs **52,486** times a session and nobody had ever counted it.

**And a second instance one level down:** `RtGraphicsWave28GfxbodySummary`'s
`printf` covered 6 of its own lane's 8 rows; three counters were incremented by
`Attend()` and **printed nowhere.** A summary can be wired and still silent.

**Direction: DEFLATING.** A zero from an unreached `printf` and a zero from an
unentered body are **the same character in a log** — the instrument could not
express the difference. **Wave 28 §5's "GFXBODY: six of eight printed nothing" is
partly silence, not zero**, and every wave that read "attendance 0" for a
GFXMAT24 / GFXBIG27 / GFXBODY28 row read silence.

VERIFY stated the mitigation rather than overclaiming: `Attend()` prints
milestones at n = 1, 10, 1000 and every 100k, so those four rows were not
*totally* invisible. What was invisible is the **totals**, the **zeros**, and
every row with no milestone printer at all.

### 13.2 THE DEBT: ~280 was never a work item, and now there is a number

New per-row port-door attendance instrument (`BANISHED_RT_ATTEND=1`) splitting the
live registry into asked/silent. In-game run: **816 of 1526 name rows and 1424 of
1576 address rows were never asked for.** Decomposed by what the tree does with
each literal:

| class | name | addr | meaning |
|---|---:|---:|---|
| **A — NO DOOR EXISTS** (only site is a mint/factory table: a stamp, not a door) | 224 | 406 | silence carries **no information** |
| **B — NO ROUTE WALKED THIS RUN** (a real consumer exists) | 471 | 420 | payable by building or running a route |
| **C — ROUTE EXISTS in a harness/probe left unarmed** | 15 | 31 | **payable TODAY, by setting an env var** |
| **D — NO CONSUMER SITE IN THE TREE AT ALL** | 106 | 567 | nothing consumes the row |

> **"~280 unexecuted bodies" is not a work item of 280.** The real
> "build or walk a route" set is **471** on the name door, dominated by
> **IO (205) and UI (135)**. And **15 rows are payable today by arming a probe
> that already exists.**

This is `reference_a_namespace_gap_is_not_a_body_count.md` applied to the debt
itself, and it is the first time the debt has had a decomposition instead of an
estimate. **Class A is the important one: 224 name rows whose silence is
structurally uninformative** — counting them as debt was always wrong.

**On my multi-line-registration item (entries 1-3):** the attendance instrument is
**IMMUNE BY CONSTRUCTION** — it reads the LIVE REGISTRY, not source, and a split
literal is one string to the compiler and one key in the map. VERIFY's
*source-side classifier* did have the blind spot, measured it at 6 rows with its
own control, and fixed it. **The right answer to a source-parsing defect is an
instrument that does not parse source.**

### 13.3 THE ORACLE: PROMOTED, AND THE CHANGE IS THE WHOLE POINT

> FACTUI1's ours-side **replays the table**. The port is a **second interpreter
> over that same table**, so the oracle **could not see an interpreter defect.**

`standalone/factory_byte_oracle.cpp` calls **the registered function pointer**
instead. That converts a self-check into an oracle — the exact distinction
`reference_shadow_marker_is_not_an_oracle.md` records, applied to the artefact I
asked VERIFY to judge.

**Placement was MEASURED, and the two obvious placements both failed:**

| placement | outcome |
|---|---|
| process exit | **ACCESS_VIOLATION** in `MemAlloc` on the 3rd row — engine had torn its allocator down |
| before `GameMain` | singleton does not exist yet; screen aborted having compared **0** rows |
| bootstrap our own allocator at exit | `Startup` **returned 0 and the slot was populated anyway** — it *looked* like it worked and the next allocation died. **A populated singleton slot is not a started allocator.** |
| **`ToolHarness_Tick`, in-game frame 120** | the engine's own allocator is live — **and it is the allocator these bodies use at cutover**, so it is the RIGHT one, not merely a working one |

**Cost stated, not buried:** this inserts roster-class handles into a live
`InternalFactory` mid-game. Half the original objection is defused (buffers are
file-scope statics that outlive the process); **the other half is not**, and it is
paid only on an explicitly armed run.

**Coverage measured, and it is not 200:** the enumerable roster is FACTREST's 35
non-UI classes — **25 concrete + 10 abstract, 0 skipped**; 1823 bytes compared,
25 masked, over 1944. FACTUI1's 31 UI classes are **not** covered (no roster
accessor in `factory_ui1.cpp`); **ADDRESSEE named** — a three-line accessor covers
them with no other change.

### 13.4 THE RESULT IT REFUSED TO CLAIM

```
  18 OK / 5 FAIL / 2 VACUOUS of 25 concrete rows
  negative control 18/18 · abstract control 10/10
```

**VERIFY does NOT present the 5 FAILs as a defect list**, because the set is
**unstable across runs of the same build** (7, then 3, then 5; EdgeMesh's
first-bad byte moved from `+0x60` to passing to `+0x73`). A byte whose offset
moves between runs is residual nondeterminism, not a port defect.

What IS solid, and it is the finding: **FACTUI1's clean sheet does not reproduce
when the ours-side is the real port.** The two VACUOUS rows are reported as **NOT
VERIFIED rather than as passes** — 0 bytes compared, which is the
`oracle_must_not_compare_unwritten_bytes` trap caught in the honest direction.

**ADDRESSEE named:** before treating any of the 5 as a defect, run N times and
keep only bytes that fail in EVERY run. **"The stability screen is the missing
piece, not the comparison."**

### 13.5 The pairing, scored honestly by BOTH sides

INSTR's factory screen defect that VERIFY found was **inflating**; the one
underneath it — found only while fixing VERIFY's own — was **deflating and
larger**: INSTR's `RE_FACTORY_SYMBOL` **never matched `?CreateInPlaceObject@` at
all while the subject line it printed claimed it did**, so it read 38 where the
family is 74.

> **A screen naming a subject it did not examine — inside the section of the tool
> that quotes that very warning.**

INSTR's §8.1 records that its doc's correct "74" came from a **scratch probe
rather than from its own tool's output**, and that had it quoted the instrument it
owns, it would have caught VERIFY's defect itself.

Symmetrically: INSTR's design review of VERIFY's oracle named two failure modes,
**both about retail's variance**; the defect that actually bit was on **our** side
and neither listed it. INSTR also never upgraded its prose review to a code review
after the oracle was committed, and **records that it claimed to have read a file
it had not** — retracted in the same turn.

### 13.6 INSTR's late edit — verified

```
$ git log --oneline 9776a690..wave29/instr
  62985dc7 INSTR POST-REPORT EDIT: lane VERIFY found a defect in my own factory screen
$ git diff --stat 9776a690 wave29/instr -- runtime/ standalone/ code/ engine/ dx11/
  (EMPTY)
```

Self-labelled, protected paths untouched, board unmoved at 1460/1295. **Three
lanes have now edited after reporting; all three declared it, all three were
clean, and I verified each myself.**
