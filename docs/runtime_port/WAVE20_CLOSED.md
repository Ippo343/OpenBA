# WAVE 20 CLOSED — the instruments were the wave, and three of the guide's named items were stale

**Closed 2026-09-10 by the integrator.** Five lanes plus two post-report tranches, all
merged onto `wave20/base`. Build green; every standing checker clean at the tip
(`dup_runtime_export` 0, `rt_mint_gen --check` 0, `dispatch_slot_map --check` 0,
`unpromotable_slot_check` 0, `screenshot_door_walk` 0).

Read with **[WAVE20_INTEGRATOR_CORRECTIONS.md](WAVE20_INTEGRATOR_CORRECTIONS.md)**
(C1–C10; **C2, C4 and C10 are load-bearing**) and **[WAVE20.md](WAVE20.md)** (the plan
as dispatched).

---

## 1. The number — and read §2 before quoting it

```
                       WAVE 19 CLOSE          WAVE 20 CLOSE
                      (broken census)        (fixed census)
IO           133 called  116 ported     ->   216 called  200 ported   ceiling 97.7%
Video        152 called  134 ported     ->   158 called  140 ported   AT CEILING
Graphics      99 called   70 ported     ->    99 called   70 ported   ceiling 96.0%
Engine        33 called   23 ported     ->    33 called   23 ported
TOTAL       1195 called 1007 ported     ->  1311 called 1124 ported   84.3% -> 85.7%

IO identity: 200 >= 216 - 5 = 211   ->  11 SHORT
vtables promoted: 10 -> 11 of 62    (the wave START was 10, NOT the 5 in the guide -- C2)
promotion backlog: 164/49/26 -> 161 bodies / 48 vtables / 25 COMPLETE
```

> **THESE TWO COLUMNS ARE NOT COMPARABLE AND THE WAVE DID NOT "ADD 116 CALLED".**
> The census was **fixed mid-wave**; +111 of that movement is rows that always existed
> and were **invisible to the old tool**. The honest statement is
> **§3's set-diff**, not a delta.

**The one asymmetry, accounted for rather than waved past.** IO moved +5 `called` and
**+6** `ported` across Phase 1 — more ported than called, which C6 said the fold-in
could not produce. It is not the fold-in: lane PROMOTE contributed **+0 called / +1
ported** (`??_7BinaryFileStream@` was already a `called` row and *flipped* to ported on
being registered — which is what a data-row port looks like), and lane SAVETEXT
contributed **+5 / +5**. C6's claim was about the **fold-in mechanism**, and it holds.

---

## 2. WHAT THIS WAVE DOES NOT CLAIM

**Read this before quoting anything above.**

1. **`85.7%` IS NOT `84.3%` PLUS WORK.** The instrument changed underneath the number.
   Any wave-over-wave percentage comparison across the Phase 0a boundary is meaningless.
2. **The promoted row is DISPATCHED, NOT VERIFIED.** 18,906 dispatches landed on our
   table; **correctness of the eight bodies was not tested** — that is
   `file_stream_slots_probe.cpp`'s tier and nobody ran it.
3. **THE SCREENSHOT CUTOVER HOLE IS NOT SHUT, AND 29 OF 29 IS NOT REACHABLE** on the
   board's own rules while `?_singleton@FileSystem@IO@@` carries a written
   `NONE EVER / HARMFUL` verdict. It stands at **23 of 29 SHUT, 6 OPEN**.
4. **Two of the six open doors cannot be scored by that screen at all** (the allocator
   pair). They are **counted OPEN**, deliberately, and unmeasured in either direction —
   no DLL-absent boot was run.
5. **29 is not known to be the whole door set.** The transitive half is still
   hand-built, and **four doors were missing from a list reviewed three times.**
6. **`??_7TextFileStream@` is NOT promotable** — 7/10, three trap slots remain.
7. **`??1FileSystem@`, `?BeginDelete@` and `??1Persistence@` were not touched.** Wave 19
   §4 items 7–8 stand unchanged.
8. **`?SetupFileSystem@Modification@` and the PackageFile cluster are still unstarted.**
9. **No screenshot or save-artefact comparison was run.** Nothing here claims a player
   sees any difference.
10. **The 8-table gap between two COMPLETE counts (28 live vs 36 static) is UNEXPLAINED**
    and open (C5).
11. **`RT_UNPROMOTABLE` tags use two undeclared denominator conventions.** Now declared
    in source, **deliberately not normalised** (§6).

---

## 3. Per lane — set-diffs, not percentages

| lane | kind | result |
|---|---|---|
| **CENSUS** (0a) | instrument | **+111 rows made visible**, all PORTED. Ruling: only a **PROMOTED** row's slots fold in. Delivered the Phase 1 board **derived**: 164 bodies / 49 vtables / 26 COMPLETE |
| **TOOLS** (0b/0c) | instrument | `boot_state.sh` freshness with a **derived** threshold, 16/16 controls · duplicates **exit 3 → 0**, two real shadowed-body defects fixed, **a third cross-file pair the guide never listed** |
| **EXTENT** (0d) | measurement | 31 of 31 rows re-derived from the writer; **+ a post-report tranche** closing premise P3 (31 PREMISE-CHECKED-PASS) |
| **PROMOTE** | port + data | **`??_7BinaryFileStream@` promoted, attendance 0 → 18,906.** Six rows **declined** on the stamper gate. **+ a tranche** closing premise P2 |
| **SAVETEXT** | port | `??_7TextFileStream@` **2/10 → 7/10**, dedicated differential oracle, 81/81. Found the wave's top-billed item **already ported nine waves ago** |
| **DOORS** | instrument | Cutover board **dark → quotable**, control **derived** not renamed; call-graph walk **25 → 29 rows** |

**Entered `ported` in Phase 1: six symbols** — `??_7BinaryFileStream@IO@@6B@` and
SAVETEXT's five `TextFileStream` slot bodies. That is the wave's porting output, and it
is small on purpose: **this was an instruments wave.**

---

## 4. THE HEADLINE FINDING: a handover's CHEAP-WINS list is its stalest section, and the first one acted on

**Three separate items the guide named as cheap, high-leverage or do-this-first failed
on contact — in one wave.** Meanwhile its **measured** §1 table was honest about the
tool that produced it, and **all three of its §0 instrument diagnoses were correct and
worth the entire of Phase 0.**

| guide's claim | reality | found by |
|---|---|---|
| `??_7VisitorStream@` "still unpromoted", a top candidate | **already promoted**, on Wave 19's last day | integrator (C2) |
| promote the NO-OP pair **first**, "lowest-risk on the board" | **safest because pointless** — already tagged `RT_UNPROMOTABLE \| NONE, EVER` | lane PROMOTE |
| `?SaveText@` "**the highest-leverage single item left**" | **ported NINE WAVES earlier**, 407 lines, registered by name *and* address | lane SAVETEXT |

**So the defect is not "the guide was unreliable." It is concentrated in ONE SECTION
KIND — and that is the section lanes pick up FIRST.**

**The mechanism.** A closing integrator's opportunistic list is assembled **at close**,
from **lane reports**, about **territory the writer did not personally work**, at the
moment when **nobody is re-measuring**. Every property pushes toward staleness: it is
about other people's work, so the writer has no working knowledge to contradict it; it
is **cheap by construction, so nobody audits it** — no one reviews a five-minute item;
it is written when the wave's own lanes have *just* changed the facts it describes
(`??_7VisitorStream@` was promoted **the same day** the guide called it unpromoted); and
being cheap and specific, **it gets picked up first.** The measured sections carry an
instrument and a denominator. **The cheap-wins list carries neither.**

> **THE RULE: every entry in an opportunistic list gets a one-line "re-derive this by
> running X" beside it, or it does not go in the list — and the CHEAPEST entries need it
> MOST, because their cheapness is what exempts them from scrutiny.**

**Cost, honestly: near zero per instance** — every lane caught it immediately on being
told to grep first, which is *why* it survived to be seen three times. **The damage is
misdirected SCOPING, not wasted work.** A nine-waves-stale item was billed as the wave's
highest-leverage subject.

**This close obeys its own rule: §8 carries a re-derivation command per item.**

---

## 5. Doctrine this wave paid for

### 5.1 A screen looking for the ABSENCE of a shape must be HANDED one

Lane PROMOTE's P2 scan. The finding is **not** the zero:

* **Run 1** — the destination-register match read raw `objdump` text, which for a
  rip-relative LEA ends in `# 0x180154960`. **Every rip-relative LEA silently dropped
  out.** `tracked = 146`, control **RED**.
* **Run 2** — stripping the comment globally fixed that and broke the *target*
  extraction. **The mirror image, caught by the same control.**
* `tracked` **146 → 3335** once both were right.

> **A scan whose "0 hits" cannot be told apart from a dead parser would have retired the
> residue FALSELY.**

An absence claim **has no natural positive case** — the subject is by definition what is
not there — so unlike a counting screen it **cannot demonstrate liveness from its own
findings.** It must be fed. **Fourth instrument in two waves whose blind spot pointed
the INFLATING-confidence way.**

### 5.2 A REAL check can be recorded against a WIDER population than it examined

C7, and it is **not** the recorded `AEAA`-means-private trap. The pefile export check in
`input_config_startup.cpp`'s banner **was real and was run** — on
`?Startup@Configuration@Input@@` (ordinal 4085). The conclusion was then extended to the
**second** function ported in the same file. That one is **ordinal 3199, a public
export**, so the name door and address door reached **different bodies**. It cannot be
decayed: the DLL is a fixed 2017 artefact, so it was **false the day it was typed.**

> **A check NAMES THE SYMBOL IT EXAMINED. A second symbol needs a second check.**

The **authoring** form of "a screen that cannot name its subject" — **no tool was wrong;
the sentence was.**

### 5.3 "Safest" and "pointless" can be the same fact

Lane PROMOTE, refining §7.5b rather than refuting it: that section weighs the **risk** of
promoting a proven NO-OP row and **never asks what the promotion buys.**

> **The proof that removes the risk removes the whole benefit.**

Both rows already carry `RT_UNPROMOTABLE | NONE, EVER` tags feeding the census ceiling,
so promoting means **deleting two tags and mechanically lowering IO's printed ceiling for
zero behaviour change.** Expiry written unprompted: lapses when `0x180023bd0` is ported
and registered, or the moment any instrument observes a dispatch through either table.

### 5.4 A control's SUBJECT must be derived, or it decays as lanes do their job

The cutover board went dark because its negative control was a **literal** that "MUST
read NOT-PORTED" — and a lane ported it. **The screen was behaving correctly**; what
failed was that its subject was a name in source.

Lane PROMOTE hit it and **deliberately did not fix it**: *"re-pointing another lane's
control is how a screen quietly stops being able to fail."* **That refusal was the right
call** and is why it got an owner instead of a quiet edit.

The fix is **derivation, not a new literal** — a new literal just resets the same decay
clock. And a derived control still has a failure mode, which lane DOORS **demonstrated
in both directions** rather than asserting: not "the picked symbol got ported" but
**`candidates == 0`**, the registry answering YES to everything.

### 5.5 Re-scoring rows you cannot check is how a board flatters itself

The allocator pair is **structurally unscoreable** by the cutover screen. Lane DOORS
**counted them OPEN** and named them on their own line. Same discipline as lane CENSUS
putting unpromoted slots in a **backlog** rather than the columns: *when an instrument
cannot see something, it says so in the direction that costs you, not the one that
flatters you.*

### 5.7 AN EXPIRY CLAUSE NEEDS AN ADDRESSEE, not just a condition

**Wave 19's standing answer was "require an expiry clause on every stated blocker at seal
time." Lane DOORS found the gap in it**, about its own tag: `??_7ReaderWriter@`'s `?`
numerator expires when *"the next lane that counts ported slots on this row"* does so.

> **If nothing ever routes a lane at that row, the clause NEVER FIRES and the `?` becomes
> permanent.**

An expiry clause with no addressee is **a TODO with nobody's name on it** — it satisfies
the seal-time checklist and then waits forever. It is still strictly better than the
wrong `2` it replaced. **The refinement: an expiry must name WHO or WHAT will evaluate
it, not only the condition under which it lapses** — and if the answer is "whoever
happens to pass by", it belongs in the next wave's input list, which is the only thing
anyone is guaranteed to read. Hence §8 items 11 and 12.

### 5.6 A number you cannot measure gets a `?` and an expiry, not a guess

`??_7ReaderWriter@`'s tag read `2/36`. The **36** is corrected to 26 by three independent
sources. The **2** was counted over the wrong extent and may have lived in the ten slots
belonging to other tables — so it is now written **`?` with an expiry clause.**
**Refusing to invent the numerator is the harder call and the right one.**

---

## 6. Integrator decisions on the record

**6.1 `RT_UNPROMOTABLE` denominator conventions: DECLARE, DO NOT NORMALISE.** Lane DOORS
found two undeclared conventions (Object/Video count non-purecall slots; the two IO ones
count the whole table) and asked. Ruling: **no.** Those numbers are **lanes' measured
claims**, and normalising rewrites eighteen written verdicts by a hand that measured
none of them. It would also **destroy the evidence that a split ever existed**, and
convert a **known, declared inconsistency into an invisible, assumed one** — strictly
worse, because the invisible kind cannot be checked. The parameter is **printed beside
the verdict** instead. Normalise a tag only when the lane that owns it re-measures it,
one at a time.

> **THE ENFORCEMENT IS NOT IMPLEMENTED. This sentence originally read
> "`unpromotable_slot_check.py` is the place to enforce it", which is a PRESCRIPTION and
> was correctable as a COMPLETION.** Lane DOORS flagged it against its own lane's
> credit. As merged, that tool **accepts either convention** and reports only rows
> fitting neither; **it cannot tell a tag that declares its convention from one that does
> not.** The doctrine currently lives as PROSE in `reg_mint_promoted.cpp`'s comment block
> and `WAVE20_LANE_DOORS.md` §5 — **the exact decay class this lane existed to fix.**
> Whoever picks it up is **adding a new check, not turning one on.** It is Wave 21 item
> 11.

That is the recorded trap *"a wave doc naming the fix in the past tense reads as proof it
landed"*, committed by the integrator inside the close that catalogues it — and caught by
a sealed lane re-reading a ruling written in its favour.

**6.2 Post-report tranches are permitted and were used twice.** Sealing forbids
**altering** a reported result, not **adding a marked one**. Both tranches (EXTENT's P3,
PROMOTE's P2) were verified by me as **direct children of the sealed commit and purely
additive** — EXTENT's lane doc diff contained **no deletion lines at all** — so both
report states stay quotable.

**6.3 The two residues were closed by the OTHER lane.** EXTENT self-reported P2; PROMOTE
closed it. That is the right way for a residue to die — **the hand that wrote the
limitation is not the one that checked it.**

**6.4 The `ImageBase` hazard was checked tree-wide and came back EMPTY.** Lane DOORS'
provenance check computed a Ghidra address as `loaded - base + OptionalHeader.ImageBase`
and printed `0x7fff975453d0`: **the Windows loader rewrites the in-memory `ImageBase` to
the actual load address**, so the expression collapses to the loaded address — a
plausible wrong number with no error. Verified by me across the tree: every other
`ImageBase` read is `pefile` against the **on-disk** header (correct); the only C++ reads
of a loaded module's header touch `SizeOfImage` and `DataDirectory`, which the loader
does **not** rewrite; the standard idiom is `resolve_standalone.cpp`'s `mi.delta`, which
is right. **No other instance** — and "checked and empty" is a result that can be quoted
where "probably fine" cannot.

---

## 7. What the integrator got wrong, since that seat has the widest blast radius

* **I relayed a stamper count of ONE for `??_7BinaryFileStream@`.** There are **two** —
  a default ctor and a **copy ctor**. On my number the row would have been declared
  fully owned **with the copy ctor's stamp still in the DLL**, putting retail's table
  into every copied object. **The lane caught it by enumerating from the binary as
  instructed**, which is exactly why that instruction exists. Same class of error the
  Wave 19 integrator recorded making, one wave later.
* **I filed C7 under the wrong trap** (`AEAA` means private) and had to be corrected by
  the lane into the sharper and more useful diagnosis in §5.2.
* **I surfaced `??_7FileStream@` as a promotion candidate** on its 8/8 COMPLETE. It
  fails the stamper gate at **6 of 17**. The gate held against an integrator suggestion,
  which is the outcome the gate is for.

**Two of my three errors were caught by a lane following an instruction I gave it.** The
instruction outperformed the instructor — which is the argument for writing the gate
down rather than trusting the seat that hands out the numbers.

---

## 8. Wave 21 inputs — EACH WITH ITS RE-DERIVATION COMMAND (§4's rule, applied to itself)

**Nothing below is scoped. Re-derive first; three items in this position were stale last
wave.**

1. **`0x1801554a8` encoding vtable — the cheapest promotion left.** Three slot bodies
   (`0x1800a81e0` / `81d0` / `81c0`) and it becomes an ordinary candidate.
   *Re-derive:* `runtime_census.py`, PROMOTION BACKLOG section.
2. **Verify the eight promoted `BinaryFileStream` bodies.** They are **dispatched, not
   verified**; the probe exists. *Re-derive:* run `file_stream_slots_probe.cpp`.
3. **`??_7TextFileStream@`'s last three trap slots** (0, 2, 8) — slot 0 has a written
   expiry (unblocks when `??1TextFileStream@` lands). *Re-derive:* the backlog row for
   that vtable.
4. **The 8-table COMPLETE gap, 28 live vs 36 static** (C5). *Re-derive:* a
   `BANISHED_RT_MINT_SHADOW=1` run against the static scan.
5. **Decide what "close the screenshot hole" MEANS** before scoping it — **29 of 29 is
   unreachable** while `?_singleton@FileSystem@IO@@` is `NONE EVER / HARMFUL`, and the
   two remaining promotion-blocked doors need **11 and 28 stamper bodies**, which is
   port work. *Re-derive:* `screenshot_door_walk.py` plus the board's own output.
6. **The transitive half of the door set is still hand-built** and was **four short**.
   *Re-derive:* extend `screenshot_door_walk.py` to the transitive half.
7. **`?SetupFileSystem@Modification@`** (~600 instructions, a lane on its own) and the
   **PackageFile cluster**. *Re-derive:* `runtime_census.py --unported --namespace IO`
   — **and note that flag is a free parameter that made a Wave 19 check confidently
   wrong.**
8. **Graphics is the largest untouched gap: 99 called / 70 ported, ceiling 96.0%.** Wave
   20 spent itself on IO and instruments. *Re-derive:* `runtime_census.py`.
9. **The cutover boot wall is `?PlatformConfigure@Engine@1@`** — an ENGINE row (33/23),
   with a seam already in `HarnessGuardUnattendedBoot()`. **[INHERITED, NOT RE-VERIFIED
   THIS WAVE.]** *Re-derive:* a DLL-absent boot.
10. **P2's residue-of-residue**: arithmetic in a different register, beyond a
    12-instruction window, or across a basic-block boundary. All three are **widenings
    of `mint_p2_arith_scan.py`**, not new work.
11. **IMPLEMENT the `RT_UNPROMOTABLE` convention-declaration check** (§6.1). It does not
    exist; the doctrine is prose today. *Re-derive:* run
    `unpromotable_slot_check.py` and observe that it accepts either convention silently.
12. **`??_7ReaderWriter@`'s `?` numerator NEEDS AN ADDRESSEE.** Its expiry says *"the
    next lane that counts ported slots on this row"* — and **if nothing ever routes a
    lane there, the clause never fires and the `?` becomes permanent.** A better failure
    than a wrong `2`, but still a TODO with nobody's name on it. *Re-derive:* the tag at
    `reg_mint_promoted.cpp:1201`.
