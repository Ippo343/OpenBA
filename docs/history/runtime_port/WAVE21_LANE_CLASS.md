# WAVE 21 LANE CLASS — instruments

Branch `wave21/class`, cut from `wave21/base` at `018426f2`. Closed 2026-09-11.

Three subjects as briefed: the kickoff's **candidate doctrine** (§1), **item 11** the
`RT_UNPROMOTABLE` convention check (§2), **item 13** whether the denominator gap is
mechanisable (§3). Four more arrived mid-lane from SHOT, ENC, GFXVT and GFXBODY and are
in §2.6 and §6.

## THE ONE-LINE VERSION

**§A CONFIRMED** (13 of 14 namespaces). **§B built, and it would have caught NONE of the
four tag defects this wave found** — so it was extended with two new FIELDS rather than
reported green. **§C answered NO**, precision 0/16 and recall 0/3.

**The finding that unifies all three: what is mechanisable is forcing a CHOICE at the
point of PRODUCTION; what is not mechanisable is any judgement about whether the choice
is RIGHT.** A classifier can refuse to guess and print UNKNOWN. A tag form can require a
route scope. A census can print its own denominator. None of the three can tell you the
answer is correct — and every defect this wave actually found was a correctness defect
inside a perfectly well-formed artefact.

---

## 0. RE-DERIVATION FIRST — the kickoff held on every point this lane touched

Rule 1 of kickoff §5. All three of this lane's inputs re-derive TRUE. That is the
headline in the boring direction, and it is worth stating because Wave 20's lesson was
that a handover's cheap items go stale: **the three items handed to this lane were
measured ones, and measured sections held.**

| input | claim | re-derived |
|---|---|---|
| kickoff §2 board | 18 namespace rows, TOTAL 1311/1124 85.7% | **EXACT**, every cell |
| kickoff §1 | Graphics 29 = 9 BODY + 1 CTOR + 1 DTOR + 2 OPNEW + 8 VTABLE + 8 DATA | **EXACT**, reproduced by an independently written classifier |
| §8 item 11 | `unpromotable_slot_check.py` "accepts either convention silently" | **TRUE** — ran it: `20 of 20 scoreable tags fit one of the two conventions`, exit 0, and nothing in its output distinguishes a tag that declares from one that does not |

Command for the first two: `tools/ghidra_mcp/.venv/Scripts/python.exe
tools/runtime_census.py --csv <out>`; for the third,
`tools/unpromotable_slot_check.py`.

---

## 1. §A — THE CANDIDATE DOCTRINE: **CONFIRMED**

> *A namespace gap figure is uninterpretable until decomposed by symbol kind, and the
> census should print that decomposition rather than leave each wave to rediscover it.*

### 1.1 What landed

`tools/runtime_census.py` now classifies every called-but-not-ported row by mangled-name
kind and prints a **GAP DECOMPOSITION BY SYMBOL KIND** block. `classify_symbol()` and
its fixture table are in the same file; `--selftest-classifier` runs the fixtures alone.
The `--csv` output gains a `symbol_kind` column, **appended** so every existing column
keeps its index.

### 1.2 The measurement

Population: **187 called-but-not-ported rows**, i.e. exactly the gap in the census's own
namespace table (1311 called − 1124 ported). Ported rows are not classified and appear
in no figure here.

```
namespace       gap |   BODY  CTOR  DTOR OPNEWDEL OPERATOR VTABLE  DATA UNKNOWN
internal         41 |      0     0     0        0        0      0     0      41
Graphics         29 |      9     1     1        2        0      8     8       0
System           25 |     11     1     1        0        0      1    11       0
Video            18 |      0     0     0        0        0     12     6       0
Achievement      16 |      7     0     1        0        0      4     4       0
IO               16 |      3     0     1        0        0      8     4       0
Engine           10 |      3     0     0        0        0      4     3       0
Object           10 |      0     0     0        0        0      4     6       0
Audio             7 |      2     1     2        0        0      0     2       0
UI                5 |      2     0     0        1        0      2     0       0
Input             4 |      1     0     1        0        0      0     2       0
ordinal           4 |      0     0     0        0        0      0     0       4
Application       1 |      0     0     0        0        0      0     1       0
Save              1 |      1     0     0        0        0      0     0       0
TOTAL           187 |     39     3     7        3        0     43    47      45
```

**52 of 187 gap rows (28%) are a function to reimplement.**

### 1.3 The verdict, with its denominator — and the criterion was chosen AFTER the data

Denominator: **14 namespaces with a non-zero gap** (of the 18 rows the census prints;
Math, Volume, Picking and Utility have gap 0 and are excluded — they have nothing to
decompose).

I did not fix a threshold before computing the table, so I report three criteria rather
than defend one:

| criterion | count |
|---|---|
| the gap is **not** purely code rows | **13 of 14** (only Save, 1/1, is pure port work) |
| a **majority** of the gap is not a function to reimplement | **8 of 14** |
| a lane scoped as "port N functions" would be wrong by ≥25% | **13 of 14** |

**CONFIRMED on all three readings.** A doctrine confirmed only by the case that
generated it would not be confirmed; this is confirmed by thirteen namespaces of which
Graphics is one.

### 1.4 The two cases that are stronger evidence than Graphics

* **`internal`, 41 — the LARGEST raw gap on the board, and the classifier can say
  nothing about it.** Every row is address-keyed: unexported, no mangled name. It is
  100% UNKNOWN. A secondary section probe (different evidence, printed as a hint and
  never folded into a bucket) splits it **29 `.text` / 8 `.rdata` / 4 `.data`** — so
  even here the gap is not 41 bodies; **12 of the 41 are data**.
* **`Video`, 18 — printed `AT CEILING`, and its gap contains ZERO bodies.** 12 VTABLE +
  6 DATA. The decomposition explains a board row that otherwise looks contradictory.

`System` (13 code of 25) is the largest gap where "port N functions" is closest to true,
and `Save`/`Audio` are the only rows where reading the raw number would not mislead.

### 1.5 The classifier names its subject, and refuses to guess

Per `reference_a_screen_that_cannot_name_its_subject`: an identifier a tool cannot
resolve is UNKNOWN, not ABSENT. Collapsing the 45 UNKNOWN rows into BODY would have
claimed 29 unexported `.text` rows and 12 unexported data rows are all port work — the
**inflating** direction, the one that manufactures work.

It reads **the mangled name and nothing else**. It is blind to body size, reachability,
and whether a DATA row is already minted. It cannot classify: address-keyed rows
(no name), ordinal rows (no name), and any name whose grammar it cannot finish. All
three are printed as separate UNKNOWN reasons with counts.

The section-hint probe returns `None` rather than `{}` when pefile or the DLL is
unavailable, and the census then prints *"not probed — this is a REFUSAL, not a zero"*.

### 1.6 CONTROLS — and the classifier is scored against the BINARY, not against itself

**(a) Additivity control.** With the new section stripped, the census output is
**byte-identical** to `wave21/base`'s (`diff --strip-trailing-cr`, exit 0). The change
moved no existing number. A fix that changes a number nobody audits is
`reference_uncommented_checker_manufactures_work`'s recorded trap.

**(b) Fixture control, with a handed-in shape.** `--selftest-classifier`: 15 fixtures,
**15/15 correct, 8 of 8 buckets exercised**. It includes a *deliberately malformed*
name (`?BrokenName@NoTerminator`) because nothing in the live tree currently trips the
grammar branch — so "0 unparseable names" would otherwise be indistinguishable from a
dead parser (`reference_absence_screen_must_be_handed_a_shape`). It also pins the
back-reference case (`?GetDescription@Achievement@1@…`) that once filed three
Achievement methods under System.

**(c) The fixture table is true by construction, so it is NOT the score.** A classifier
validated only against the naming convention it implements proves nothing. Ground truth
was taken from the **shipped DLL**:

* **Section table** — each named gap row's own export address resolved to a PE section;
  `.text` ⇒ code, `.rdata`/`.data` ⇒ data. **142 of 142 scoreable rows AGREE, 0
  disagree.** Denominator: 142 of the 187 gap rows; the other **45 are the UNKNOWN
  bucket, which the classifier declined and which this check therefore cannot score.**
* **First-slot probe** — the section table cannot separate VTABLE from DATA (both live
  in `.rdata`), so the bytes were read. **All 43 VTABLE rows hold a `.text` pointer at
  slot 0; all 47 DATA rows do not.** The DATA side is the effectiveness control: a probe
  that always answered "code" would have shown 47 hits there and showed 0.
* **Ghidra spot-confirmation** (`switch_program` → `get_current_program_info` verified
  `Runtime-x64.dll`, 15290 functions, base `0x180000000`, before the reads):
  `0x1801559f8` holds four consecutive `.text` pointers (`0x18000BAFE0/D0/C0/00`) — an
  unexported vtable, matching `iobase_vtables.cpp`; `0x1801fc6d0` is zeroed `.data` —
  mutable storage. **Sample of 2 of the 12 non-`.text` UNKNOWN rows.**

**Chose the pefile route over a Ghidra sample deliberately:** it covers the *whole*
named population rather than a sample, takes no free parameter beyond the DLL path, and
is immune to the shared-instance hazard where another lane's `switch_program` lands
between two of my reads.

**What (c) does NOT validate:** the coarse CODE/DATA split and the VTABLE/DATA split
only. Nothing external distinguishes BODY from CTOR from DTOR from OPNEWDEL — that
split rests on the `??0` / `??1` / `??2` mangling convention alone, and is unverified
here.

---

## 2. §B — ITEM 11: THE CONVENTION-DECLARATION CHECK NOW EXISTS

### 2.1 Re-derivation of the premise

Ran `tools/unpromotable_slot_check.py` on the untouched tree first, as instructed. It
printed `20 of 20 scoreable tags fit one of the two conventions, 0 STALE` and exited 0.
**Nothing in that output distinguishes a tag that declares its convention from one that
does not** — the premise of item 11 is true.

### 2.2 The agreed form, now written in both places

Defined in `standalone/reg_mint_promoted.cpp`'s format block (the doctrine's home) and
enforced by the tool. A numeric `<slots ported>` field must carry exactly one of:

```
[CONV: M=TABLE]        M is the WHOLE table
[CONV: M=NONPURECALL]  M counts only non-purecall slots; table = M + K
```

**A declaration is load-bearing, not paperwork.** A declared tag is checked under *that
reading only* instead of "either of two" — so declaring is what retires check 1's
stated blind spot for that row.

### 2.3 Grandfathering is on the PAIR, which is the whole design

WAVE20_CLOSED §6.1 forbids rewriting the eighteen existing verdicts, so the 20 numeric
tags present at `018426f2` are frozen in the checker as **(symbol, EXACT field text)**
pairs and pass without declaring. Freezing the *symbol* alone would have let an edit
slip through undeclared forever. Because the freeze is on the pair, **the moment a lane
edits one of those fields — exactly the moment §6.1 permits, "normalise a tag only when
the lane that owns it re-measures it" — that tag must declare.** The list is a hardcoded
literal and therefore a free parameter; it is **printed beside the verdict** with how
many entries a live tag matched, and it must never grow.

### 2.4 Controls — both asked for, plus three more, and one of them found a bug

Four controls run inside the tool on every invocation (synthetic tags injected into the
tag set) and are printed before any finding:

```
CONTROL 1pos : 40 tags parsed, 20 of them scoreable against a minted row -> GREEN
CONTROL 1neg : synthetic 0/999 row reported                              -> GREEN
CONTROL 2neg : synthetic UNDECLARED numeric tag reported                 -> GREEN
CONTROL 2pos : synthetic WELL-FORMED declaration NOT reported            -> GREEN
```

If any fails the tool prints `*** MEASURED NOTHING ***` and exits 2.

**Five LIVE controls against the real file** (edited, run, reverted; the tree is clean
and `git status` shows only my three intended files):

| # | what was handed to the check | expected | result |
|---|---|---|---|
| 1 | a new tag `??_7LaneClassLiveControl@IO@@6B@ \| 3/9 (2 purecall)`, no declaration | FAIL | **UNDECLARED at :1203, exit 1** |
| 2 | the same tag with `[CONV: M=NONPURECALL]` | PASS | **exit 0, "1 declared"** |
| 3 | the same tag with `[CONV: M=WHATEVER]` | FAIL | **BADCONV + UNDECLARED, exit 1** |
| 4 | a grandfathered field edited (`??_7Dialog@UI@@6B@ 9/9` → `9/9 ported`) | FAIL | **exit 1, freeze reports `19 of 20 matched` + the UNMATCHED pair, and the tag becomes UNDECLARED** |
| 5 | `??_7Base@Object@@6B@ 2/2 (1 purecall)` declared `[CONV: M=TABLE]` (mint row = 3) | STALE | **exit 1, "Tried: the DECLARED M=TABLE reading only (K=1)"** |

Controls 1 and 2 are the two the brief asked for. **Control 5 found a real bug:** the
`STALE` print had six arguments for five placeholders and raised `TypeError`. That path
had **never executed** — check 1 has reported zero stale rows since it landed — so a
live tree would have hit a crash the first time it caught anything. That is the
`reference_absence_screen_must_be_handed_a_shape` lesson landing on a *reporting* path
rather than a detection one.

### 2.5 Subject, and the comment-stripper hazard inverted

This tool **deliberately reads C++ comments** — the tags are a comment block by design.
So it must never acquire a comment stripper; that would blind it completely, the mirror
of `reference_uncommented_checker_manufactures_work`. This is written into the module
docstring so a future tidy-up does not "fix" it. The protection instead is a narrow tag
grammar plus a printed coverage line:

```
COVERAGE : 2383 file(s) scanned outside standalone/reg_mint_promoted.cpp;
           0 carry a full tag-shaped line
           LIVENESS: 9 line(s) elsewhere mention 'RT_UNPROMOTABLE:' without the
           full tag grammar -- a non-zero here says the walk ran.
```

The `0` is absence-shaped, so the liveness figure is printed beside it: **9 loose
mentions** (a doc quotation, the census's own regex, this tool's own docstring). A bare
"0 elsewhere" from a dead walk would read identically to a clean tree.

While adding the doctrine example to the source block I deliberately wrote the marker
apart from its colon so the **example cannot be parsed as a real tag** by this or any
future reader. Verified: the tag count stayed at 20.

### 2.6 THE FOUR DEFECTS THIS WAVE FOUND — AND MY CHECK WOULD HAVE CAUGHT NONE

Relayed mid-lane from lanes SHOT, ENC and GFXVT. **This is the most important thing in
§2 and it is against my own deliverable.**

| # | defect | lane |
|---|---|---|
| 1 | an expiry with **no ADDRESSEE** never fires — `??_7ReaderWriter@`'s `?` waited a wave for "the next lane that counts ported slots on this row" | ENC (closed it in one lane once named) |
| 2 | an expiry whose **CONDITION IS FALSE** — `??_7TextFileStream@` slot 0 "unblocks when `??1TextFileStream@` lands"; it does not | ENC |
| 3 | an expiry of the **WRONG SHAPE** — even had that dtor landed, the deleting-dtor thunk pattern does not require the callee to be ported, so the clause named a real event that would never unblock anything | ENC |
| 4 | a **verdict RIGHT for a reason that is FALSE** — `?_singleton@FileSystem@IO@@`'s HARM is real while the DLL is loaded (89 reads / 44 functions, 26 still retail, vs 2 writes, both ours) and FALSE at cutover, where the row is REQUIRED | SHOT |

> **Every one of the four sat in a tag that was perfectly WELL FORMED. My check 2 —
> which validates the FORM of the existing tag — would have passed all four.** A checker
> that passes all four is worth very little, and reporting it green would have been the
> same error class this lane exists to fight.

**So the deliverable became a convention EXTENSION, not a checker for the existing
form.** CHECK 3 adds two fields to the expiry:

```
[SCOPE: PERMANENT]     true on EVERY route that could ever exist, cutover included
[SCOPE: DLL-PRESENT]   true only while Runtime-x64.dll is loaded; at cutover it CHANGES
[SCOPE: CONDITIONAL]   lapses on the stated condition
[ADDRESSEE: <who or what will evaluate this>]
```

**Measured motive, not an argument.** The forty existing expiries are **36 "NONE, EVER" /
3 "NONE while the DLL is present" / 1 a real condition** — the phrase had become the
default spelling — and SHOT proved at least one of the 36 is really the second kind.

**Honest scoring of the extension against the same four:** it reaches **2 of 4** (1 and
4) and only in the sense that the *form would have forced the author to confront the
question*. It reaches **0 of 4 as a correctness check**. Defects 2 and 3 are out of reach
of any form check — a condition can be both false **and** beside the point, and nothing
syntactic sees either.

**I was leaning toward "route scope is a judgement call, not worth a field". GFXVT's
outcome changed my mind and I am recording the reversal rather than the conclusion.**
GFXVT re-tagged `??_7Geometry@` from `NONE, EVER` to route-scoped, and **naming the route
inverted a priority**: under the old framing the offending read was an opportunistic
unlock ("fix it and we may promote a row"); under the route framing it is a **cutover
blocker on the critical path**, because once the DLL is gone the bad read hits our own
storage and retail's table is no longer there to absorb it. Same evidence, same lane,
opposite ranking, visible only once the route was named. GFXVT further found **all 8 of
its rows are REQUIRED at cutover**, none optional, so for that namespace the DLL-present
verdict and the cutover verdict differ on **every** row. A field whose absence files a
critical-path blocker as a nice-to-have earns its place.

### 2.7 A DENOMINATOR DIVERGENCE BETWEEN TWO LANES ON ONE INSTRUMENT — resolved

The coordinator flagged that GFXVT reports **"21 of 21 scoreable tags, both controls
green"** from this same tool. **That is not my number and not my instrument.** Checked
directly against `wave21/gfxvt`:

* GFXVT's branch carries **42** `RT_UNPROMOTABLE` tags; mine carries **40** (**41** after
  §6.2's new tag). Their scoreable population is 21 because they added
  `??_7Geometry@Graphics@@6B@ | 6/6 (3 purecall)`, which has a minted row; mine is 20.
* **"both controls green" is the `wave21/base` tool**, which had exactly two controls.
  Mine has **six** and runs three checks.

So the two readings are **different populations measured by different instruments**, and
neither is wrong. **Neither is the other's corroboration**, which is precisely the trap.

**Concrete merge consequence, so the integrator does not meet a red tool by surprise:**
GFXVT's two new tags are in **neither** of my freezes, so at merge check 2 will require a
`[CONV: ...]` on the Geometry row and check 3 will require `[SCOPE:]`/`[ADDRESSEE:]`. Its
expiry reads `ROUTE-SCOPED (DLL-PRESENT ONLY); voids when statusicon_geometry.cpp:341
reads 'i < 9'; REQUIRED at cutover` — **it states the scope in prose but not in the
token.** So my check will flag the best-written tag in the file, on form, while agreeing
with it completely on substance. That is the sharpest limitation of a form check and it
is a one-line fix at merge, not a disagreement.

---

## 3. §C — ITEM 13: **NO, NOT MECHANISABLE IN THIS FORM.** Precision 0, recall 0

`tools/denominator_screen.py` is committed. Its verdict is in its own docstring so it
cannot decay into prose.

### 3.1 The probe, and what "nearby" means

Scans `docs/runtime_port/WAVE*.md` for numeric claims and flags those whose enclosing
block names no population. **The window is the enclosing markdown BLOCK — a
blank-line-delimited paragraph, a whole table, or a whole fenced block — not N lines.**

That is the direct lesson of `reference_documenting_a_check_can_delete_it` (a 12-line
comment pushed a call site out of a fixed 6-line window and *deleted* the check, which
read as a pass) and of `reference_checker_pairs_across_function_boundaries` (scanning
back N raw lines paired a number with an unrelated function's population and
manufactured confident wrong findings). A block is derived from the document's own
structure: it grows when the author writes more, and it never crosses into the next
paragraph.

### 3.2 Scored against a hand-read sample

**Sample, stated before the score:** `WAVE21_KICKOFF.md` **in full**, plus
`WAVE20_CLOSED.md` **§5.7–§5.10 and §6–§8 in full** — the sections I read in full for
this lane's brief. I did not hand-read the rest of either file and make no claim about
it.

**Criterion, fixed before scoring:** a true denominator failure is *a quoted count or
percentage where a reader cannot tell, from the document itself, what population it was
taken against.*

```
flags raised inside the hand-read sample     16
of those, real denominator failures           0     PRECISION  0 / 16 = 0%
real denominator failures found by hand       3     (5 with §3.2b's two)
of those, flagged by this screen              0     RECALL     0 / 3 = 0%  (0/5)
```

The three real failures, by hand:

1. §0 / §3 *"Wave 20 paid for that **three** times"* / *"**three** items in this position
   were stale last wave"* — three of how many? The cheap-wins list's size is never given.
2. §8 item 6 *"the transitive half of the door set… was **four** short"* — short of what
   total? The door set's size is not in the document.
3. (the same claim as 1, counted once.)

The sixteen flags, by hand: *"the **29** unported Graphics rows"* (population named in
the same phrase), *"The **29** rows, classified"* (ditto), *"backlog: ours 5 / slots
**10**"* (a ratio that *is* its own denominator), *"Raw gaps by size: internal **41**,
Graphics **29**…"* ×5 (derivable from the table directly above), *"Items 7, 9 and
**10**"* (item numbers), *"did not add **116** called"* (a delta being explicitly
repudiated, with both columns in §1), *"+**111** of that movement"* (population one line
up, in the adjacent block — the window's own boundary), *"**407** lines"* (a size),
*"ordinal **4085**" / "ordinal **3199**"* (ordinals), *"a fixed **2017** artefact"* (a
year). **None is a denominator failure.**

Tree-wide: **6094 flags of 11622 numeric claims across 207 `WAVE*.md` files — 52%.** A
screen that flags half its input is not a work list, and an inflating blind spot
manufactures work.

### 3.2b TWO MORE HAND-VERIFIED SPECIMENS, FROM LANE SHOT — and both are structurally uncatchable

Better than inventing test cases, and they **sharpen the finding rather than repeating
it**. SHOT found two real denominator bugs in this tree:

1. **a board printed 30 rows for a 29-row array** — the population named is *wrong*, not
   missing.
2. **25 NAMES quoted as 29 ROWS** — a **units error**: a count of one kind of thing
   quoted as a count of another.

**Neither can ever be flagged by this screen, and the reason is a proof rather than an
estimate:** the screen's flag condition is *"the enclosing block names no population"*.
**Both specimens NAME a population.** They are therefore outside the screen's flag
condition by construction, regardless of wording, tuning, or window.

That raises the honest recall to **0 of 5** and, more usefully, **changes the diagnosis**:
the denominator failures this project actually commits are not *"no population stated"*.
They are **the wrong population** and **the wrong unit** — and both require knowing what
the number counts, which is the one thing a text screen cannot know. A screen aimed at
absence is aimed at the rarest of the three failure modes.

### 3.3 Why it fails, which is the part worth keeping

**Not a tuning problem, and no filter fixes it.**

* All three real failures are quantities spelled as **English words** — *three times*,
  *three items*, *four short* — with no digit anywhere for a numeric screen to see.
* All sixteen false positives are digits whose population **is** stated, in prose, one
  clause away.

The discriminating signal is not syntactic **in either direction**. To separate these
cases the screen would have to understand the sentence. Widening the SKIP list moves
false positives around and cannot reach a single one of the true failures.

### 3.4 So: is the gap mechanisable? **NO for prose. PARTLY, at the point of PRODUCTION**

Wave 20 §5.10's four denominator failures were in *an instrument*, *a lane*, *a close*,
and *a self-verification*. **Only the first of the four is reachable by a tool at all**
— the other three are sentences a person wrote. A screen over documents is aimed at the
three it cannot reach.

The version that demonstrably works is enforcing the denominator where the number is
**produced**: a tool's output is machine-readable and has one author, and this tree
already has the evidence — `runtime_census.py` prints SUBJECT / PARAMETERS / POPULATION
before any finding, Wave 18's six instruments were each repaired by printing what they
could not see, and both instruments this lane touched now do it (§1.5, §2.5).

**A bounded observation, with its denominator and its weakness:** of **63 `tools/*.py`**,
**3** compute a percentage and **2** of those 3 print a SUBJECT/POPULATION header;
`tools/sample_report.py` is the one that does not. A population of 3 supports nothing,
and *"a tool that publishes a number"* has no crisp definition — which is itself a
denominator problem inside a measurement about denominators. Offered as a pointer, not
a finding.

### 3.5 The honest bound on §C

My sample contains **3 true failures**; that is a very small denominator, and **I am
both the tool's author and its scorer**, which
`reference_a_working_number_does_not_validate_the_recipe` warns about. The mitigating
fact is the direction: the result is *negative about my own tool*, which author bias
does not explain. A precision of 0/16 and a tree-wide flag rate of 52% would each need
to be wrong by a large margin to change the verdict.

---

## 4. WORK ROUTED IN MID-LANE (GFXVT, GFXBODY) — three more instruments

### 4.1 The SIXTH census blind spot: a screen that NAMES UNKNOWN WRAPPERS

GFXVT found that three live cutover dependencies (the `EdgeMesh`, `EdgeMeshInstance` and
`MeshInstance` vtables) **never enter the census `called` column at all**, because they
are reached through `RtVtable("…")` at `runtime/src/graphics/mesh.cpp:305` — a one-line
project-local wrapper around `ResolveExport` that no door pattern recognises. Its control
is what makes that a diagnosis: `RtVtable` has exactly five call sites, and the two other
rows it names (`Base@Object`, `External@Object`) appear in the census **only** because a
second, known door also names them. The wrapper door is invisible; the survivors are
coincidences.

**This is the sixth instance of one family, and every previous fix added one more literal
to match — which is why there was a sixth.** So I implemented GFXVT's prescription rather
than a seventh pattern: the census now matches **any identifier called with a
mangled-name string literal** and prints the ones it cannot classify, **by name, with
counts and a file**.

```
subject     2339 call site(s), 18 distinct identifiers
known DOOR        1114 sites / 3 identifiers
known NOT a door  1168 sites / 2 identifiers
UNCLASSIFIED        57 sites / 13 identifiers   <- each named in the output
```

**57 / 13 independently reproduces GFXVT's "57 of 2271 through a wrapper, 12 unknown
identifiers".** (Its 2271 vs my 2339 is a different denominator — I include the
known-not-a-door identifiers; 13 vs 12 is one more identifier my classification lists.)

**It changes NO count, deliberately.** Deciding a given wrapper *is* a door is a
judgement about that wrapper's semantics, and making it here would move numbers other
lanes are quoting mid-wave. The screen hands the list forward and names `RtVtable` in its
own caveat as a known live example. It is comment-stripped, unlike the tag checker,
because *this* subject is code.

### 4.2 A `.pdata` ENTRY IS A FRAGMENT, NOT A FUNCTION — measured, and DECLARED not fixed

GFXBODY found that MSVC chains fragments through `UNW_FLAG_CHAININFO`, so one `.pdata`
record gives the first fragment and silently omits the rest — it measured
`AnimationManager::Update` at 50 bytes when it is **430**, with 3 of 13 rows understated,
the worst by **8.6x**.

**I measured the exposure in this DLL before prescribing anything: 932 of 6045 `.pdata`
entries (15.4%) carry `UNW_FLAG_CHAININFO`, covering 116,964 bytes.** The exposure is
real and large.

**Audit of `tools/`: five tools read `.pdata`. NONE of them computes a function size from
it** — `mint_extent_audit`, `mint_p2_arith_scan`, `mint_stamper_census` and `vtable_dump`
use it for **attribution** (which function owns this VA), and `rt_mint_writer_audit`
explicitly records that `.pdata` "is NOT usable" and does not use it. So the *sizing*
defect does not currently live in this directory. The *attribution* use is affected by
the same fact in a narrower way: for those 932 entries, a raw range names a **fragment's**
start, which is not the owning function's entry point.

**DECIDED: declare, do not fix, this wave** — and stating that explicitly rather than
leaving it implicit, as asked. Three reasons: correcting attribution would move numbers
several lanes are quoting mid-wave (the same refusal I applied to §4.1 an hour earlier,
and consistency matters); **9 of GFXBODY's 13 corrected extents rest on the corrected
method alone, only 4 Ghidra-confirmed**, so the correction is less established than the
defect; and the declaration is the half that stops a wrong number being quoted. The four
tools that build `.pdata` ranges now carry the measured statement at the construction
site. **Wave 22 input.**

**Note the polarity, which is why this one matters more than it looks.** An inflating
blind spot manufactures work; a **size-understating** one makes work look **cheap** — and
cheap is exactly what gets picked up without scrutiny, which is this wave's own headline
(`reference_cheap_wins_list_is_the_stalest_section`). It already changed a ranking: the
kickoff and Wave 20 both named `Renderer::Draw` (921 bytes) the monster of the Graphics
gap; `BuildParameters` is **1422** bytes with 12 indirect calls and is the biggest row in
the namespace.

### 4.3 GFXBODY's row 11 — the first tag written in the Wave 21 form

GFXBODY supplied a row and a reason and correctly declined to write the tag, since the
tag machinery is this lane's. `?AddDrawItems@MeshInstance@Graphics@@` (retail
`0x180100f20`, ordinal 1550 — **resolved from the export directory**, GFXBODY gave the
address) is ported and understood, and registering it is **active harm**: our tree's only
use of that address is **not a call**. `standalone/scene_watch.cpp:401` resolves it as an
**identity token** and compares it against `vtable[3]` of a live object to decide whether
the object is a `MeshInstance`. Register it and `ResolveAddr` returns our body, the
compare fails for every object, and `BANISHED_LOD_CENSUS` / `BANISHED_FORCE_LOD0`
silently observe an **empty scene** — a wrong answer with no error. (Verified by reading
both sites; the fuller write-up already existed at `runtime/src/graphics/mesh.cpp:1822`.)

**Tagged `HARMFUL`, and `[SCOPE: DLL-PRESENT]`, not `NONE, EVER`** — applying SHOT's
lesson to a fresh row rather than only citing it. The harm is that *a retail vtable slot
is the thing being compared against*; at cutover there is no retail slot, `scene_watch`'s
identity test must be rebuilt whatever we decide, and the row stops being harmful.
Writing `NONE, EVER` would have filed a cutover task as a permanent refusal. It carries
an `[ADDRESSEE: …]` naming the lane that next needs the row registered, with the Wave 22
kickoff writer as the fallback.

It is **the first tag in the file to carry both new fields**, and the tool confirms it:
`CHECK 3: 41 tags — 1 scoped, 1 addressed, 40 grandfathered, 0 MISSING`.

### 4.4 THE CEILING MOVED DOWN BECAUSE WORK WAS DONE — report the set-diff, not the %

Adding §4.3's tag changed one printed number: **Graphics ceiling 96.0% → 94.9%.**

**It went DOWN, and I had predicted it would go up.** `ceiling = (called − unpromotable) /
called`, so proving one more row unpromotable — which is knowledge gained, work done —
*lowers* the printed ceiling. No `called`, `ported` or `detour` cell moved anywhere on the
board; `TOTAL` is unchanged at 1311 / 1124 / 85.7%.

This is a live instance of `reference_the_ceiling_percentage_is_a_moving_target`, caught
by measuring instead of asserting. **The honest statement is the set-diff: exactly one
named row, `?AddDrawItems@MeshInstance@Graphics@@`, entered `unpromotable`.** Do not quote
the percentage movement as a result.

---

## 5. WHAT THIS LANE DID **NOT** CHECK

* **The finer symbol buckets are unverified.** BODY vs CTOR vs DTOR vs OPNEWDEL rests
  entirely on the `??0`/`??1`/`??2` mangling convention. The binary cross-checks reach
  only CODE/DATA and VTABLE/DATA.
* **The 45 UNKNOWN rows are not classified, only hinted.** The section split
  (29/8/4) is different evidence and is printed as a hint. **Ghidra confirmed 2 of the
  12 non-`.text` rows**; the other 10, and all 29 `.text` rows, were not opened.
* **I did not check whether any DATA row is already minted**, whether any BODY row is
  large or small, or whether any row is reachable. The classifier says what a row *is*,
  never what it *costs*.
* **`?_typeInfo@`-style DATA rows were not separated from other static data.** The
  UI-gap memory distinguishes them; this classifier does not, and a lane that wants that
  split must add it.
* **I did not run the game, build the tree, or execute any C++.** My only source edit
  (`standalone/reg_mint_promoted.cpp`) is inside a comment block; **it is not compiled
  and I did not compile it.** A build check is owed by whoever integrates.
* **Check 1's numerator is still unchecked** — the tool verifies the denominator only,
  as before. `??_7ReaderWriter@`'s `?` numerator is untouched by this lane (that is
  item 12, lane ENC's).
* **Item 11's check does not verify that an UNPROMOTABLE verdict is CORRECT** — only
  that its convention is declared and its denominator fits. Whether a row really is
  unpromotable turns on who stamps the table, which is not a question either check
  asks. The coordinator suggested Ghidra could answer it; I deliberately kept the two
  apart and did not attempt it.
* **§C's recall is measured over 1.5 documents.** I did not hand-read the other 206
  `WAVE*.md` files, so the 6094 tree-wide flags are unscored.
* **I did not check whether any other tool consumes the census CSV positionally.** I
  appended the new column rather than inserting it precisely so that question would not
  need answering; a grep found no programmatic consumer, only prose mentions.
* **The additivity control no longer covers the whole tree state.** It was run and passed
  on the classifier change alone (§1.6a). §4.3's tag then deliberately moved the Graphics
  ceiling, and §4.1's screen is pure added output. I did not re-run a byte-identity
  control after §4.3, because it would necessarily fail by design.
* **I did not classify the 13 unclassified wrapper identifiers.** The screen names them;
  deciding which are doors is the next lane's judgement, and `RtVtable` is the only one I
  assert is a door (on GFXVT's evidence, not my own).
* **I did not fix the `.pdata` attribution in any tool**, did not verify GFXBODY's
  corrected extents, and did not check whether anything OUTSIDE `tools/` sizes a function
  from `.pdata` — my audit's subject was `tools/*.py` only.
* **I did not verify SHOT's, ENC's or GFXVT's underlying measurements.** I took their
  findings as given and used them to shape the convention; the only one I checked
  independently was GFXVT's tag text and tag count on its own branch (§2.7).
* **CHECK 3 is prospective only.** All 40 pre-existing expiries are grandfathered and the
  tool exits 0 with every one of them missing both fields. That backlog is printed as a
  counted NOTE, not fixed.

---

## 6. FOR THE NEXT WAVE'S INPUT LIST (an expiry needs an addressee)

1. **The freeze in `unpromotable_slot_check.py` must never grow.** Addressee: whoever
   reviews a commit that adds an entry to `GRANDFATHERED`. *Re-derive:* run the tool;
   `FREEZE : 20 … 20 matched` is the healthy state, and any `UNMATCHED` line means a
   grandfathered tag was edited and now owes a declaration.
2. **BODY/CTOR/DTOR/OPNEWDEL is unvalidated.** Addressee: the next lane that scopes work
   off a per-kind count. *Re-derive:* `runtime_census.py --selftest-classifier` shows
   what the fixtures cover; the binary cross-check in §1.6 shows what they do not.
3. **`internal`'s 41 rows are the largest undecomposed population on the board.**
   Addressee: whoever scopes after Graphics. *Re-derive:* the UNKNOWN section-hint lines
   in the census output (29 `.text` / 8 `.rdata` / 4 `.data`).
4. **Item 13 is answered NO for prose and should not be re-proposed in that shape.**
   Addressee: the Wave 22 kickoff writer. *Re-derive:* `tools/denominator_screen.py` and
   the VERDICT block in its docstring.
5. **MERGE ACTION: GFXVT's two new tags need `[CONV:]` and `[SCOPE:]`/`[ADDRESSEE:]`
   tokens.** Its Geometry expiry already states the route scope in prose; it needs the
   token. Addressee: **the integrator merging `wave21/gfxvt` and `wave21/class`** — this
   is a one-line edit, and without it the tool goes red on merge. *Re-derive:* run
   `tools/unpromotable_slot_check.py` on the merged tree.
6. **`.pdata` attribution: DECLARED, NOT FIXED (§4.2).** Four tools carry the statement;
   none follows the chain. Addressee: **the Wave 22 kickoff writer**, and any lane sizing
   a function before then. *Re-derive:* 932 of 6045 entries carry `UNW_FLAG_CHAININFO` —
   recount from the shipped DLL's exception directory.
7. **The 13 unclassified wrapper identifiers (§4.1) are an open work list**, and
   `RtVtable`'s three Graphics vtable rows are still absent from `called`. Addressee:
   **the lane that next changes the census door set** — and it should classify the
   identifiers, not add a seventh pattern. *Re-derive:* the WRAPPER-DOOR SCREEN block in
   `runtime_census.py`'s output.
8. **The 40 grandfathered expiries declare neither route scope nor addressee.** SHOT
   proved one of them wrong; GFXVT's evidence suggests DLL-present vs cutover verdicts
   differ on whole namespaces. Addressee: **each row's owning lane when it next
   re-measures** (§6.1 forbids anyone else), with the Wave 22 kickoff writer to decide
   whether that is fast enough. *Re-derive:* the CHECK 3 NOTE line.
