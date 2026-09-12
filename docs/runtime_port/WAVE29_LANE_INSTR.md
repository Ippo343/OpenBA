# WAVE 29 LANE INSTR — the board-owning instrument lane

Branch `wave29/instr`, base `wave29/base` `c11511a1`.
Commits: `47e97001` (census / stamper / dtor-verify), `e3c09f5b` (slot sweep).
**Nothing outside `tools/` is touched.** `git diff --stat c11511a1...wave29/instr
-- runtime/ standalone/ code/ engine/ dx11/` is empty by construction.

---

## 0. THE BOARDS MOVED. READ THIS BEFORE QUOTING ANY FIGURE.

Two published numbers changed, and **both are measurement changes, not work
landing.** Nothing was ported this wave by this lane.

```
                                    kickoff §1        this tip      delta
  runtime_census  called                  1453            1460         +7
                  ported                  1294            1295         +1
                  ported %               89.1%           88.7%      -0.4pp
                  gap                      159             165         +6
                  gap BODY                  24              30         +6

  mint_slot_sweep ported slots             430             439         +9
                  UNPORTED -> named trap    16               7         -9
```

**The percentage fell because `called` grew.** Seven live cutover dependencies
that no door in this tool could see are now counted, and six of them are
unported. That is the instrument getting less wrong, printed as the board
getting worse. Score the lane on the identity, not the percentage:

> **IDENTITY: four DEFLATING blind spots closed in the instrument the project
> scores itself with, one INFLATING one in a checker, and one INFLATING one in
> the sweep — with the sweep's correction re-deriving lane SLOTS' figure
> independently, and a new cross-tool screen that found a fourth sweep defect on
> its first run.**

**Two kickoff sections are now refuted and must not be quoted:**

- **§1.2 "The remaining BODY work in the entire runtime port is SEVENTEEN
  functions"** — it is **23**. Six unported UI bodies were invisible to the
  census: `?Begin@SpriteContext@UI@@`, `?CleanRemovedDialogs@UserInterface@UI@@`,
  `?ComputeLayout@DialogInstance@UI@@`, `?GetMouseCursorIndex@UserInterface@UI@@`,
  `?Update@DialogInstance@UI@@`, `?UpdateEvents@UserInterface@UI@@`. All six are
  reached from `runtime/src/ui/userinterface_{frame,close,draw}.cpp` through a
  macro door whose name argument is a `kName` constant. UI is **221/208 = 94.1%**,
  not 215/208 = 96.7%.
- **§1.3's vtable table identities** — three of the six names in it are wrong,
  because `mint_slot_sweep.py` printed an *extent* note where the *identity*
  belongs and, failing that, an adjacent-export hint that is systematically off
  by one row. `~??_7Particle@Graphics@@6B@` is `ParticleInstance`,
  `~??_7ParticleSystem@` is `ParticleSystemInstance`, and
  `~??_7FileStream@IO@@6B@` at `0x180154950` is `??_7Stream@Audio@@6B@`.
  And the row count itself was wrong: 16 traps, truth 7.

---

## 1. WHAT LANDED

### 1.1 `runtime_census.py` — the macro doors' name argument  (ledger #2, DEFLATING)

`first_literal_after()` bailed on the first `)` **and** demanded a string
literal, so **36 of 780** `RT_EXPORT_ONCE` / `RT_DATA_SLOT` sites were dropped in
silence. Replaced by `macro_door_name_arg()`: a **paren-depth** scan that returns
the literal, the identifier, or an explicit `"unreadable"` — never `None`.
Identifiers resolve through the same name-constant tables, cross-file table and
phantom guard the plain door has used since Wave 10, and every site the scan
still cannot read is **printed**, by file and line.

Measured: **32 sites recovered, 26 distinct symbols, 7 of them in `called`
through no other door**, 4 sites correctly recognised as the macros' own
definitions forwarding a parameter, 0 unreadable.

**The ledger described the wrong shape, and the correction matters.** Wave 28's
item 2 named the *inline function-pointer type* case (`RT_EXPORT_ONCE(void
(*)(int), "?X@@")`), proved by moving a typedef. That shape has **ZERO live
instances today**. All 36 live drops are the *named-constant second argument*
shape — the idiom `RE_NAME_CONST`'s own comment **encourages**. One root cause,
two triggers; the fix covers both and `--selftest-macro-doors` hands the tool a
constructed positive for the shape that does not occur, alongside live ones for
the shape that does.

### 1.2 `runtime_census.py` — the factory family, said out loud  (ledger #1, DEFLATING)

New section `OUT OF POPULATION BY CONSTRUCTION`. **The family is NOT folded in**
— that would redefine `called` and move the board +103/+103 for zero new work.
Instead the blindness is now a *checked* property with a printed decomposition:

```
  IN THE BOARD POPULATION            :   0
  REGISTERED AND VISIBLE TO THE SCAN :  74
  FAMILY SIZE (minted table)         : 103  + their CreateInPlaceObject twins
```

and two causes with **different fixes and different lifetimes**:

1. **INTRINSIC** — nothing *resolves* a factory. It arrives as a slot-0 pointer
   in a minted `BaseTypeInfo`: a data stamp, not a door. Those 74 registered rows
   are therefore on the **orphan list**, which is exactly what they are.
2. **MISSING CAPABILITY, and it expires** — the rest are registered by
   `rt_factories_core.cpp`'s roster loop as `r.newName`, a non-literal
   registration. Addressee named: the Wave 30 kickoff writer, with the note that
   following it **cannot move the board** while (1) holds and must not be sold as
   closing a gap.

### 1.3 `runtime_census.py` — a probe is not a caller  (ledger #10)

**This is the finding I would keep if I could keep one.** The kickoff and the
carried ledger said *"8 STALE VERDICT rows: retire or rewrite each."* **Seven of
the eight were stale only because their single caller is
`standalone/rt_orphan_probe.cpp` — the probe that produced the verdict.** The
census was counting its own verification harness as a consumer, dropping the row
off the orphan list, and then reporting the row's triage as out of date.

**Following the instruction literally would have deleted the triage of seven
ports that nothing in the shipping tree calls.** The machinery to prevent it
already existed (`INSTRUMENT_FILE_MARKERS`, Wave 26) and had never been applied
here. Probe-only callers now keep a row on the orphan list, marked `probe-only`;
**24 rows** are affected (7 carrying Wave 12 verdicts, 17 previously invisible
and untriaged). Orphan exports 284 -> 307.

Stale verdicts **8 -> 1**, and that one — `?StopEffect@Scene@Audio@@QEAAXI@Z`,
the eighth, the one the kickoff flagged as new — is **really** stale: it is now
called from `runtime/src/audio/audio_scene_update.cpp`, a port. Retired, with its
Wave 8.5 evidence preserved in the GRADUATED block rather than deleted.

### 1.4 `runtime_census.py --csv`  (ledger #9)

`--csv > rows.csv` exited 2 **after the shell had created rows.csv and filled it
with the argparse usage message** — a non-empty file whose first field is
`usage:`, which is worse than an empty one. Now `nargs="?"`: bare `--csv` writes
the CSV to stdout and the report to stderr.

### 1.5 `mint_stamper_census.py` — the frameless-leaf fold-in  (the highest-value item)

Wave 26 lane PROMOIO measured this defect and wrote that folding it in *"would
move a number other lanes quote. It is a deliberate decision for a lane that owns
the board."* This is that lane, and it took it.

The recorded stamp `site` is the **LEA's own VA**. If that address is an export's
**entry point**, the stamp is that function's first instruction — the site does
not lie *near* the export, it **is** the export. `owner()` now returns
`EXPORT-LEAF` and scores it identically to any other export.

```
  re-derived BEFORE : 47 unattributed groups, 45 resolving exactly, 0 scored
  AFTER             : 45 EXPORT-LEAF groups SCORED, 24 ours-capable,
                       2 still UNSCORED
```

Wave 26's prediction (45 of 47, 24 of 47) reproduced **exactly** on 64 rows.
`ours-capable` remains an **upper bound** — a registration is a door, not a
guarantee every caller uses it — and the output says so where it prints.

### 1.6 `rt_deleting_dtor_verify.py` — the bare `else`  (ledger #8, INFLATING)

Three rows (`EdgeMeshInstance`, `MeshInstance`, `Light`) printed **BAD** for a
shape the reader does not **know**, identically to a shape it had **checked and
refuted**. Now `UNREC`, counted apart, still failing the run (`--check`-style
tools must fail, not warn). `problems: 3 -> 0`, `unrecognised: 0 -> 3`. The
negative control still passes and is unaffected.

### 1.7 `mint_slot_sweep.py` — four defects, referred here by SLOTS and the integrator

Detail in commit `e3c09f5b`. Summary: the registration scan read `git grep -h`
**line fragments**, so **67 sites** were invisible or truncated; traps **16 -> 7**,
ported **430 -> 439**, nine slots already ported (six since Wave 27), two whole
tables COMPLETE since Wave 27 with nobody aware. Fixed by reading whole files and
**importing** `concat_literal` / `strip_comments` from `runtime_census` rather
than re-implementing them. The extent note is no longer printed where an identity
belongs. A **cross-tool consistency screen** now prints the set difference against
the census's `ported_exports` in both directions on every run — and **found a
fourth defect on its first run**: the sweep had no comment stripper and had
registered `<lit>`, `<literal>` and `<slot name>` out of comments quoting this
project's own match patterns.

### 1.8 The decomposition that settled a four-way disagreement

Three people produced three numbers (49, 67, 87) for the sweep's blind spot.
Whole-file, comment-stripped, at this tip:

| shape | sites | effect on the old scan |
|---|---|---|
| literal same line, one fragment | 1312 | fine |
| literal same line, **continued next line** | 17 | **TRUNCATED to a phantom** |
| literal starts on the **next line** | 39 | invisible |
| next line **and** continued | 11 | invisible |
| first argument not a literal | 37 | a *different*, already-known blind spot |
| **total call sites** | **1416** | |

**Affected population = 17 + 39 + 11 = 67 — lane SLOTS' figure exactly.**

Two corrections fall out, and both are about *how* the numbers were taken:

- The integrator reported **zero** adjacent-string concatenations and asked for
  that branch to be dropped as dead code. **There are 28.** Its grep required
  both fragments **on one line**, so a line-based screen was used to rule out a
  defect whose definition is *"spans lines"*.
- **The old pattern was never the bug.** Its `\s*` matches newlines; applied to
  whole files it reads all 1379 literal sites correctly. The defect was the
  **input**. Anyone who records this as "a regex was too narrow" will write it
  again.

---

## 2. WHAT THIS DOES NOT CLAIM

- **Nothing was ported and nothing was verified to run.** Six new UI gap rows
  appeared; this lane did not triage them. They are live DLL dependencies of our
  own port bodies, which is what `called` means — whether each is "a body to
  write", an empty retail body, or already ported-but-unregistered is **not**
  determined here (kickoff warnings 5 and 12 both apply).
- **`88.7%` is not `89.1%` minus work.** `+7 called / +1 ported` is rows that
  always existed and were invisible. Likewise `439/7` on the sweep: **no slot was
  ported this wave**; nine were already ported and mis-scored.
- **The factory screen was never handed a live positive.** Its positive case is
  **synthetic**. Lane VERIFY, which owns that evidence, looked and reports there
  is none — so score it **HANDED-NO-POSITIVE**, not PASS. The screen is a check
  on a *premise*, and only the negative direction is exercised by the tree.
- **The `unreadable` macro-door list is empty today**, and a live zero from an
  absence-shaped list proves nothing. Its positive case is the `F_NONAME`
  fixture, not the live run.
- **The stamper's `ours-capable` is still an upper bound.** The fold-in fixed
  *who a stamp site belongs to*, not *whether every caller reaches us*. An in-DLL
  `call rel32` still runs retail's body.
- **`EXPORT-LEAF` rests on one premise I did not re-derive from the binary**:
  that `mint_extent_audit.disasm_channel` records the LEA's VA rather than the
  store's. I read that from its source, not from a disassembly. If it ever
  records the store, the exact-match test silently stops matching and 45 groups
  revert to UNSCORED — which `--selftest` would catch, because it requires the
  leaf count to be non-zero.
- **The cross-tool screen compares two sets, not two answers to a question.** The
  census reads the WORKING TREE and the sweep reads a committed rev; the screen
  warns when the tree is dirty but cannot correct for it.
- **I did not run any instrument belonging to lane VERIFY** (§4).

---

## 3. THE DEFECTS I HIT, AND THE DIRECTION EACH ERRS

| # | where | direction | found by |
|---|---|---|---|
| 1 | census drops a macro door whose name arg is a constant | **DEFLATING** | re-derivation (ledger named a different trigger) |
| 2 | census counts a PROBE as a caller, retiring live triage | **DEFLATING** | re-derivation of the "8 stale verdicts" claim |
| 3 | census factory blindness stated as total; it is two causes | both | lane VERIFY's live measurement |
| 4 | `--csv >` leaves a usage message in the .csv | **DEFLATING** (an empty/garbage CSV reads as "no rows") | reproduction |
| 5 | stamper says NOT OURS for 24 functions we own | **DEFLATING** | Wave 26, re-derived here |
| 6 | dtor-verify reports UNRECOGNISED as BAD | INFLATING | reproduction |
| 7 | sweep reads line fragments: 67 sites | INFLATING (9 phantom slots of work) | lane SLOTS |
| 8 | sweep prints an extent note as an identity | MISIDENTIFYING | lane SLOTS |
| 9 | sweep has no comment stripper | **DEFLATING for a hazard list** | **my own cross-tool screen, first run** |
| 10 | sweep counts gated-off registrations as ported | **DEFLATING for a hazard list** | my own cross-tool screen |

Carried in from partners, recorded as theirs:

- **Lane VERIFY, ledger #7, and it was DEFLATING.** Wiring three unreachable
  `*Summary()` reporters produced attendance no wave had ever seen printed:
  GFXBIG27 `Renderer::Draw` **5045**, `AnimationState::Update` **18725**;
  GFXBODY28 `BeginDrawGroup` **10089**, `Camera::operator=` **20166**. Wave 28
  §5's "six of eight printed nothing" is partly **silence**, not zero. GFXMAT24
  is genuinely all-zero including its own effectiveness counter — and that zero
  had never been printed either.
- **Lane VERIFY:** a door scanner that enumerates **spellings** is wrong by
  construction; rewriting it spelling-agnostic took its blind spot 57 -> 6, and
  all 6 residuals were multi-line literals.
- **Lane IOLANE (via the integrator):** a hidden-return-slot screen fell back to
  `rfind("QEBA")` and read the last character of the name for any `UEBA` symbol,
  reporting a by-value `String` return as a reference — **DEFLATING**, caught
  only by its known positive. Same family as #7 above: *a fallback that silently
  mis-parses a neighbouring mangling.*

---

## 4. MY PARTNER'S INSTRUMENT — WHAT I ACTUALLY DID, AND ONE ERROR OF MINE

**I ran none of lane VERIFY's instruments. Score the pairing as one-directional.**

VERIFY replied twice, before and after my commit, and its input changed my tree
three times: the factory-screen premise (§1.2), a multi-line-literal caution that
is now the `F_SPLIT` fixture in `--selftest-macro-doors` (it passes — the macro
door joins adjacent literals, so my fix does **not** carry VERIFY's residual-6
blind spot), and the HANDED-NO-POSITIVE scoring in §2.

**I declined one half of the trade and the reason is an obstacle class, not a
judgement.** VERIFY asked me to flip a bit in `runtime/src/object/rt_factories_*.inc`
to exercise its byte oracle's negative control. Warning 18 makes the integrator
run a diff over `runtime/` on my tip at merge time, and a non-empty result
invalidates the wave's board — which is this lane's entire output. I referred it
to GFXBODY or AVBODY, who already have edits there.

**MY OWN ERROR, recorded in full.** In my first reply I wrote that my critique of
VERIFY's oracle came "from reading `standalone/factory_byte_oracle.cpp` at
`wave29/verify`". **I had not read it.** `wave29/verify` is still at the kickoff
commit and contains no such file; it is uncommitted in VERIFY's worktree and was
never visible to me. The critique was derived entirely from VERIFY's **prose
description** of the construction. I sent a correction within the same turn,
retracting the source rather than softening it, and the review is recorded — here
and to VERIFY — as **a design review from a description, not a measurement**.
This is the "a real check widened to a population it did not examine" shape, and
I committed it while writing a lane whose subject is instruments that overstate
what they examined.

The substance that survives, and which I still stand behind as an argument about
the *design* VERIFY described: a mask derived by running **retail twice** and
excluding bytes where retail disagrees with itself is built from the same side it
scores. Its built-in control corrupts only bytes **already in the compared set**,
so by construction it can never report that the masked set is too large — the one
failure direction that is not self-limiting. **The fix is one line: print
`|masked|` beside `|compared|` per class.**

---

## 5. WHAT I RE-DERIVED, AND WHAT REPRODUCED

| claim | source | result |
|---|---|---|
| board 1453 / 1294 / 89.1% | kickoff §1 | **reproduced exactly** before any edit |
| 8 stale verdict rows, 8th is `StopEffect` | kickoff §1 | **reproduced exactly** — and then refuted as to *meaning* (§1.3) |
| 45 of 47 unattributed stamp sites resolve exactly | WAVE26_LANE_PROMOIO §2 | **reproduced exactly** on 64 rows |
| 24 of 47 ours by name and address | WAVE26_LANE_PROMOIO §2 | **reproduced exactly** (24 of 45 leaf groups) |
| `mint_factory_census` 103 PRESENT / 0 ABSENT | kickoff §1 | reproduced |
| `cutover_manifest.py --gate` OK, 59 rows | kickoff §1 | reproduced |
| `--selftest-all` green | — | green before and after, now 16 screens |
| 67 registrations invisible to the sweep | lane SLOTS | **reproduced exactly**, and decomposed (§1.8) |
| "zero adjacent-string concatenations" | integrator | **refuted** — 28 |
| "the ledger's item 2 shape (inline fn-ptr type)" | Wave 28 | **no live instance**; the live shape is the other one |

---

## 6. NEW CONTROLS, EACH SCORING EVERY CHECK SEPARATELY

| control | checks | notes |
|---|---|---|
| `runtime_census.py --selftest-macro-doors` | 14 | every fixture run through the **old** reader too, so the control demonstrates the defect rather than asserting it |
| `runtime_census.py --selftest-factory-blind` | 6 | absence-shaped; handed a real factory name in a fabricated population |
| `runtime_census.py --selftest-csv-sink` | 6 | drives the tool as a **subprocess**, the channel a user drives |
| `mint_stamper_census.py --selftest` | 5 | positive, handed-one, negative, and a no-regression check |
| `mint_slot_sweep.py` (every run) | 3 + set diff | concat control, next-line control, each with its **negative half**, plus the cross-tool set difference |

All three census controls are in `--selftest-all`, which is **ALL GREEN**.
`_st_report` now accepts a third field per check and appends it to the printed
label: **a green line that does not say what it examined is not evidence, and a
red one that does not say what it saw cannot be diagnosed without editing the
tool.**

---

## 7. EXPIRY AND HANDOVER — each with an addressee

1. **[ADDRESSEE: the Wave 30 kickoff writer.]** Quote **1460 / 1295 / 88.7%** and
   **23** remaining bodies, not 1453 / 1294 / 89.1% and 17. Re-derive with
   `python tools/runtime_census.py` and `python tools/runtime_census.py
   --unported --namespace UI`. `CLAUDE.md`'s status block still carries Wave 20's
   figures and is now two waves stale; I deliberately did not edit it, because
   seven sibling lanes are merging into the same file this wave.
2. **[ADDRESSEE: the Wave 30 kickoff writer.]** Decide whether the census should
   follow a **roster-table** registration (`r.newName`). It **cannot move the
   board** while nothing resolves a factory, so it is bookkeeping and must not be
   presented as closing a gap. Printed in the census's own output.
3. **[ADDRESSEE: the Wave 30 kickoff writer.]** Decide whether
   `mint_slot_sweep.py` should honour `GATED_OFF_BLOCKS`. The 27 MemoryAllocator
   rows behind `BANISHED_RT_ALLOC` read PORTED and cannot fill a minted slot at
   cutover. It would move the trap count **up**, which is the safe direction for a
   hazard list. Printed in the sweep's own output.
4. **[ADDRESSEE: whoever next reads a `~`-prefixed table name anywhere.]** It is
   gone from `mint_slot_sweep.py`. If you see one in a doc, it came from a run
   before `e3c09f5b` and is probably off by one row. Kickoff §1.3 is the known
   contaminated copy.
5. **[ADDRESSEE: lane VERIFY, or whoever inherits the byte oracle.]** Print
   `|masked|` beside `|compared|` per class (§4).
6. **[ADDRESSEE: the next lane to build ANY tool that reads registrations.]**
   Import `concat_literal` and `strip_comments` from `runtime_census`. Two tools
   in this repo have now independently re-implemented literal scanning and both
   got it wrong; the cross-tool screen at the bottom of `mint_slot_sweep.py` is
   the only thing that would have told you.

---

## 8. POST-REPORT AMENDMENT — I EDITED AFTER REPORTING, AND HERE IS WHY

**Warning 18 says do not edit after reporting, and if you must, say so loudly.
This is that.** The commit carrying this section lands after §0–§7 were written
and after I reported to the coordinator. **The board did not move: TOTAL is still
1460 / 1295 / 88.7%.** Only the factory screen and its controls changed.

Lane VERIFY ran all four of my screens (`wave29/verify` `28e8df93`) and **found a
defect in one of them**, which is the pairing working exactly as kickoff §2.1
intends.

### 8.1 The defect VERIFY found — and a second one under it that VERIFY did not see

`RE_FACTORY_SYMBOL` was `^\?Create(?:New)?Object@`. It was **wrong in both
directions at once**, which is why neither error cancelled the other out of the
printed output:

- **INFLATING (VERIFY's finding).** The optional group also matched
  `?CreateObject@Factory@Object@@QEBAPEAVBase@2@I@Z` — the **AVL dispatcher**, an
  ordinary door and not a per-class factory slot. It put **1** row in the board
  population and **fired my own `*** part 1 above no longer holds ***` alarm**.
  A screen built to check a premise was reporting that the premise had failed, on
  a row that is not in the family. VERIFY corroborated both halves in one live
  run: `?CreateObject@Factory@` **ASKED 2** (a real door, not a factory), while
  `?CreateNewObject@` / `?CreateInPlaceObject@` were **SILENT 200 / ASKED 0**.
- **DEFLATING (found while fixing it, and worse).** The pattern **never matched
  `?CreateInPlaceObject@` at all**, while the subject line the screen *prints*
  read `^?Create(New|InPlace)?Object@`. The registered count read **38** where
  the family is **74**. **The screen named a subject it did not examine** — this
  wave's warning 6, committed inside the section that quotes it.

Fixed to `^\?Create(?:New|InPlace)Object@`, neither group optional. The subject
line is now generated from `RE_FACTORY_SYMBOL.pattern` itself, so it **cannot**
disagree with the regex again. Two fixtures added, scored separately: the
dispatcher must **not** match, an `InPlace` name **must**.
`--selftest-factory-blind` is now 8 checks; `--selftest-all` remains ALL GREEN.

```
  IN THE BOARD POPULATION            :  1  ->  0     (the false alarm is gone)
  REGISTERED AND VISIBLE TO THE SCAN : 38  -> 74
```

**§1.2 above quoted 74 before this fix, and that was luck, not accuracy**: I took
it from my own scratch probe (a substring test) rather than from the tool's
output, which said 38. Quoting a number the instrument does not print is how a
screen's defect stays invisible while its documentation reads correct.

### 8.2 VERIFY's screen results, scored separately as asked

1. `--selftest-macro-doors` **14/14**; `F_SPLIT` confirmed to cover VERIFY's
   residual-6 blind spot. **Q: any false doors among the 26 recovered names?
   ZERO.** 11 confirmed **by execution** on a live in-game run
   (`??0Camera@Graphics` ASKED 2, `??0Geometry` 5, `??1External@Object` 3,
   `?AsUTF8@WideString` 3, `?CloseDialog` 1, `?CloseFile` 2,
   `?Play@UserInterface` 2, `?Set@WideString` 2, `?SetBounds@Geometry` 2,
   `?SetCamera@Context` 3, `?SetText@Error` 2); 5 silent; 10 UNREG, which a
   registry-hit counter **cannot** see by construction. VERIFY states the screen
   is structurally unable to produce a negative — so read this as *no false door
   found*, not *no false door exists*.
2. `--selftest-factory-blind` **6/6, and the live section defective** — §8.1.
3. `mint_stamper_census.py --selftest` **5/5**; VERIFY has **no independent
   evidence on the 24** and explicitly offers none. The fold-in rests on Wave
   26's measurement plus my re-derivation, and on nothing else.
4. `rt_deleting_dtor_verify.py` — **UNRECOGNISED is the right verdict, and there
   are TWO REAL THIRD SHAPES**, read from the disassembly by VERIFY:
   - `Graphics::EdgeMeshInstance 0x1800175d0`: the scalar branch at `0x180017621`
     calls `0x1800a41c0`, not the `0x1800174d0` the array branch passes as its
     element callback. `0x1800174d0` is a frame-setup **thunk** that tail-jumps
     there (`0x1800174e6: jmp 0x1800a41c0`); the real dtor restamps `0x180154e60`
     at `+0x00`. **Shape: the scalar branch calls the thunk's TARGET.**
   - `Graphics::MeshInstance 0x180017330`: the scalar branch at `0x180017381`
     **inlines the whole dtor body**, verbatim from `0x180017060`'s opening.
     **Shape: there is no call to find.**
   - `Light 0x180058dd0` is not a thunk; VERIFY did **not** read its scalar
     branch and makes no claim about it.

   **I did not implement either shape.** I have not read those bytes myself, and
   a written recipe from a good lane can still be wrong. `UNREC` is exactly right
   in the meantime: it says *unrecognised*, not *defective*.
   **[ADDRESSEE: the Wave 30 lane that next touches `rt_deleting_dtor_verify.py`
   — teach the reader to follow a one-instruction `jmp` tail, and to accept an
   inlined dtor body, re-deriving both from the disassembly before trusting this
   paragraph. That takes `unrecognised` 3 -> 1 and settles `Light`.]**

### 8.3 What VERIFY did with my design review — and the failure direction we both missed

VERIFY implemented the `masked=` line I asked for (per class, totalled in a
COVERAGE line **above** the PASS count, with the "the negative control cannot see
this" sentence attached): **1823 bytes compared, 25 masked, over 1944 object
bytes.**

**But the failure that actually bit was neither of the two we named.** The mask
was **one-sided** — retail vs retail only. Across three runs `Graphics::EdgeMesh
+0x60` read ours = 98 / 90 / 60 while retail read 68 every time: *a byte OUR side
varies and retail does not* was scored as a port divergence three runs running.
**INFLATING.** Fixed by constructing OURS twice as well — which immediately broke
the negative control (it flips only `g_ours`, so the victim byte then looked
nondeterministic and was masked) and read **0/20 NOT CAUGHT, loudly**. Final:
negative 18/18, abstract 10/10.

**The lesson is sharper than my review was.** I reasoned that a mask built from
the same side it scores is unsound and named two consequences — both about
*retail's* variance. The live defect was on *our* side, which neither of us
listed. **A design review from a description finds the failure modes the
description mentions.**

VERIFY's headline, recorded as VERIFY's and not folded into mine: **25 concrete
factory rows, 18 byte-identical, 5 FAIL, 2 VACUOUS — and the 5 are NOT presented
as defects**, because the set moved 7 -> 3 -> 5 across three runs of the same
build and needs a stability screen. What is solid: **FACTUI1's clean sheet does
not reproduce when the ours-side is the real port rather than a second copy of
the interpreter.**

### 8.4 One more thing §2 does not claim

Everything in §2 stands. Add one: **I did not review
`standalone/factory_byte_oracle.cpp` even after VERIFY committed it at
`28e8df93`.** VERIFY offered and I declined, in order to stop editing. The §4
design review remains a review of a *description*, and §8.3 is the measured
evidence that such a review is incomplete by construction.
