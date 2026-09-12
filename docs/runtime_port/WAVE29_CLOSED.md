# WAVE 29 CLOSED — 2026-09-12

Eight lanes, all merged onto `wave29/base`. Final tip `4accbf30` plus this
document. **Every figure below was re-derived on the final merged tip**, after the
last lane commit and after the `cutover_manifest.inc` regeneration. The running
record of how each figure was reached — including every place I got one wrong —
is `WAVE29_INTEGRATOR_LOG.md`, entries 1-13. Read it; this document is its summary,
and a summary is exactly the artefact this project has learned not to trust.

```
lane      tip        merged   unmerged at close
instr     a2fa200d   yes      0
mint      b01654a0   yes      0
slots     caa5ff11   yes      0
wall      69c39061   yes      0
iolane    26896fb2   yes      0
avbody    db5cc9c4   yes      0
verify    28e8df93   yes      0
gfxbody   06415b17   yes      0
```

**Every lane tip was re-checked at close, not at report time.** Four lanes edited
after reporting (IOLANE, WALL, INSTR twice). All four declared it; all four were
docs-only; I verified each with the source-root `git diff --stat` rather than
accepting the label. **INSTR's last commit (`a2fa200d`) arrived after I had merged
its previous tip** — Wave 28 §7.5's exact failure, caught this time by the
re-check, and confirmed to touch neither source nor `tools/` before merging.

---

## 1. THE BOARD — and why three rows of it are not one comparison

```
                                        called  ported          gap  BODY
  opened      (kickoff, OLD census)      1453    1294   89.1%   159    24
  re-based    (INSTR: FIXED census,      1460    1295   88.7%     -    30
               no work landed)
  closed      (merged tip, FIXED census) 1472    1317   89.5%   155    21
```

**Do not subtract the first row from the third.** They were produced by different
instruments. Lane INSTR fixed two DEFLATING census blind spots mid-wave, which
moved the board **1453/1294 → 1460/1295 with zero work landed** (+7 called from 32
recovered macro-door sites, and six unported UI bodies the board had never been
able to see). **The only like-for-like delta is re-based → closed: +12 called,
+22 ported, BODY gap 30 → 21.**

Lane AVBODY refused, unprompted, to quote its own board against INSTR's figure for
exactly this reason. It is the discipline this document asks of every reader.

### 1.1 THE IDENTITIES — score the wave on these, not the percentage

- **The live cutover wall MOVED** (§2) — and the merged tip lands exactly on the
  wall WALL predicted.
- **VIDEO's BODY surface CLOSED on AVBODY's tree** — then **reopened by one row on
  the merged tip**, because SLOTS' faithful `MeshInstance::Update` port newly calls
  the unported `?SetBones@Context@Video@@`. Neither lane is wrong; AVBODY's claim was
  branch-scoped. Recorded because "closed" is exactly the word that gets quoted.
- **The ~280-body verification debt is DECOMPOSED rather than estimated** (§3.3).
- **The mint tables are UP TO DATE** (132 vtables / 104 BaseTypeInfo / 39 storage).
- **`??_7PackageFile@IO@@6B@` PROMOTED**, with a measured two-arm warrant.
- **Ledger defect #7 was DEFLATING and hid four LIVE bodies** (§3.2).
- **Factory family still 103/103**; manifest gate OK (59 rows).

### 1.2 THE REAL REMAINING BODY WORK: 14, itemised

`BODY 21 − 7 gated-but-written MemoryAllocator rows = 14.` Work this list, not the
count (kickoff warning 2):

| ns | n | symbols | status |
|---|---|---|---|
| Graphics | 5 | `Scene::AddCulledDrawItems` (2950 B), `AnimationState::CopyInstancedToUnique`, `AnimationState::UpdateNodes`, `SunLight::GetSunCamera` (**3775 B**, largest) | GFXBODY: declined, **size judgements** |
| | | `MeshInstance::GetBones` | **newly `called`** by SLOTS' `MeshInstance::Update` |
| UI | 6 | `Begin@SpriteContext`, `CleanRemovedDialogs`, `ComputeLayout@DialogInstance`, `GetMouseCursorIndex`, `Update@DialogInstance`, `UpdateEvents@UserInterface` | **always existed; invisible** until INSTR's census fix |
| IO | 1 | `Modification::SetupFileSystem` (2617 B, derived) | IOLANE: declined, size; 3 of 18 callees absent |
| Audio | 1 | `Device::PlayPlatform` (632 B) | AVBODY: declined, size + evidence; fake-backend oracle sketched |
| Video | 1 | `Context::SetBones` | **newly `called`** by SLOTS |
| System | 0 | (7 gated behind `BANISHED_RT_ALLOC`) | a graduation decision, not a port |

**Kickoff §1.2 said SEVENTEEN. It was wrong when written** — six UI bodies were
invisible to the census I derived it from. Two of today's 14 are rows a faithful
port *created* this wave. That is the standing rule that porting grows the cutover
surface, landing in a different lane's namespace.

---

## 2. CUTOVER ATTEMPT 33 — on the merged tip, the wall WALL predicted

| attempt | tree | wall |
|---|---|---|
| 31 | Wave 28 close | `RtUi_SpriteSheet_Serialize+0x1b7` — EXEC-of-0 |
| 32 | `wave29/wall` only | `RtGraphics_MaterialInstance_Serialize+0x83` |
| **33** | **all eight lanes merged** | **`RtGraphics_MaterialInstance_Serialize+0x83`** |

```
  #0  0x0                                       <-- EXEC of 0
  #1  RtGraphics_MaterialInstance_Serialize       +0x83
  #2  RtObject_External_Load                      +0x592
  #3  RtObject_ExternalFactory_FindOrLoad         +0x2b1
  #4  (anon)::CallFindOrLoad                      +0x26
  #5  RtObject_ExternalPtr_Serialize              +0x25a
  #6  RtObject_ExternalPtrSerialize_MaterialInstance +0x7c
  #7  RtUi_SpriteSheet_Serialize                  +0x1ca   <-- PAST the old +0x1b7 wall
  #8  RtObject_External_Load                      +0x592
```

Symbolised **against this linked image**, not by matching attempt 32's raw offsets
(the two exes have different layouts; MINT's rule). Arm proven by the four lines
printed by the code that read each variable — `[Standalone] BANISHED_RT_NO_DLL=1`,
`[RtAlloc] ATTEST: ARMED (cutover, DLL absent) (exports=27)`,
`[RtMint] ... registration is ON`, `[AutoStart] ENABLED` — and `RtDiff] ON` = 0.
DLL restored and verified at 2,762,752 bytes.

**Two results in one run:**

1. **Frame #7 is `SpriteSheet_Serialize+0x1ca`, past `+0x1b7`.** WALL's fix holds in
   the merged tree.
2. **WALL's branch-only binary and the eight-lane merge agree on the wall.** So the
   other seven lanes — including MINT's 68 newly minted tables — **neither moved nor
   regressed it.** That agreement is the control; a different wall would have forced
   a separate pre-merge control run, which is why none was needed.

**The next wall is a FAMILY, but an enumerated one:** `0x180061100` =
`ExternalPtr<Graphics::Material>::Serialize`, the 7th of **25** instantiations of one
template, 6/25 registered and all 19 remaining enumerated by WALL.

**`RtUnresolved` printed 0 — on a wall that is itself an address door.** That is a
second independent confirmation of WALL's headline finding (§4.2): the counter is
structurally blind to the exact fault class it was used to exclude.

Evidence: `docs/runtime_port/cutover33/attempt33_*.txt` — copied as `.txt` **because
the script's own `.log` copies were eaten by the gitignore on this run too**; they
never staged. Two lanes reported that defect; it reproduced on the integrator's run.

### 2.1 The DLL-PRESENT path did not regress

Smoke on the final merged exe (md5 `bb9e65c5…`, verified as my build by
`assert-held` before launch): autostart → world → pause-menu Quit.
**Deliberately not `BANISHED_AUTOQUIT`**, which overrides the New Game path — MINT
nearly certified the ordinary path this wave on an AUTOQUIT run that never built a
world.

```
  exit 0 · standalone_crash.log 0 bytes
  [AutoStart] sequence fired — new-game transition queued     (4 lines)
  startTown placement lines                                    154
  [PauseMenu] dispatching OnCommand(msg=0x12, id=6)            frame 721
  AnimationState::Update                                       >1,000
  GetVolumeForPosition (AVBODY's bonus export)                 1,070,262
```

**I nearly concluded "no world was built."** `CLAUDE.md` says to grep
`standalone_debug.log` for `[AutoStart]`; the driver prints to
`standalone_error.log`. The documented log returned **0** on a healthy run — the
same signature MINT recorded for a run that genuinely never built a world. Fixed in
`CLAUDE.md` at close.

---

## 3. WHAT EACH LANE DID

| lane | landed | declined / not closed |
|---|---|---|
| **WALL** | Fault at `+0x1b7` identified **by offset**: an address door onto an unported DLL internal. Ported `0x180078ad0` (reserved field emitted **fifth**); registered `0x180014520`, the **next** `CALL RAX` 19 B on. Control + negative control. `tools/cutover_attempt32.sh`. | Next wall enumerated (19 of 25), addressed to attempt 34's runner |
| **MINT** | `rt_data_mint_tables.inc` regenerated **in its own commit**; purely additive, proved with a live control; empty exclusion list **argued**. Census byte-identical — **correct**. | Cutover A/B moved nothing, as a layout-only change should |
| **VERIFY** | Defect #7 fixed: **4 live bodies had never been counted**. New per-row attendance instrument. Debt decomposed. FACTUI1 oracle **promoted, re-targeted at the registered function pointer**. | 5 FAIL rows **not claimed** as defects — unstable across runs |
| **INSTR** | 2 deflating census blind spots fixed; stamper fold-in (**reproduced Wave 26's prediction exactly**); `mint_slot_sweep` 16 → 7 traps; **cross-tool screen, which found a 4th defect on its first run**; shared helpers now imported, not copied | Factory family decomposed, deliberately **not** folded into the board |
| **SLOTS** | All six tables **named from the PE export directory**; `MeshInstance` 6/9 → 8/9. **Nine of sixteen "unported" slots were already ported.** | 5 slots, all size judgements; `Stream::Run` now declined by 3 lanes |
| **GFXBODY** | 4 of 8 Graphics bodies; an `Array::SetSize` address-door residual converted into a ported row; Wave 28 R6 closed; **R5 shown stale on arrival** | 4 size judgements; all 4 ported bodies attend **0**, traced to the unpromoted `SunLight` vtable |
| **IOLANE** | 3 `TextFileStream` bodies, oracle-verified; `PackageFile` **PROMOTED**, guard-refusal exercised on that row | `SetupFileSystem` (size); `TextFileStream` vtable not promoted **on measurement**, not for want of slots |
| **AVBODY** | 5 bodies incl. **an export nobody listed**; 49 sweeps vs shipped exports, 0 divergences, 6/6 negative controls | `PlayPlatform`, with a no-hardware oracle design |

### 3.1 The pairing result, reproduced — and scored honestly, twice against the scorer

The kickoff's central methodological claim was Wave 28's: instrument defects are
found by the *other* lane. It reproduced, and **two lanes scored it against
themselves rather than rounding up**:

- **SLOTS ↔ GFXBODY: four defects across the pair, all four found by the other
  lane's author.**
- **IOLANE ↔ AVBODY:** IOLANE's controls found 2 in its own work; **AVBODY found 2
  more, including a wild-write prescription**; IOLANE found **0** in AVBODY's.
  IOLANE published that asymmetry in its partner's favour.
- **INSTR ↔ VERIFY:** VERIFY changed INSTR's tree three times; **INSTR ran none of
  VERIFY's instruments and scored the pairing one-directional.** VERIFY then found
  INSTR's factory screen **never matched `?CreateInPlaceObject@` while its printed
  subject line said it did** (38 where the family is 74).
- **WALL ↔ MINT:** WALL caught MINT's two-variable arm, **under which all 132
  regenerated rows would have bound nothing** and the A/B would have reported "no
  difference" about an unarmed subject.

---

## 4. THE FINDINGS THAT OUTLIVE THE WAVE

### 4.1 A defect fixed in one instrument does not propagate to the one beside it

`runtime_census.py` had been fixed for multi-line registration literals.
`mint_slot_sweep.py`, beside it, had not — and reported **16 unported slots where the
truth was 7**. Two whole tables had been **COMPLETE since Wave 27 with nobody aware**.
Nothing in the project compared two tools' answers to one question. **Discharged
this wave:** INSTR built the cross-tool screen, and `concat_literal` /
`strip_comments` are now **imported** from the census rather than re-implemented,
so the next fix cannot fail to propagate.

**And the lesson recorded twice by me was the wrong one.** The regex was never the
bug — its `\s*` matches newlines. The defect was **feeding it `git grep -h` line
fragments**. Fixing the regex would have changed nothing.

### 4.2 A counter used to EXCLUDE a fault can be structurally blind to it

Wave 28 §2: *"`RtUnresolved` is ZERO, so this is not a missing registration."*
**It was one.** An unregistered *address* door prints no `[RtUnresolved]` line
(`mainloop_run.cpp:13` says so in-tree). The inference was carried into this wave's
kickoff and into WALL's own brief. **Attempt 33 reproduced it: `RtUnresolved` = 0 on
an address-door wall.** A third crash-log-adjacent field with this property, joining
the two already recorded.

### 4.3 A summary guarded by the counter it reports cannot distinguish zero from never

Three `*Summary()` functions over **18 rows across three waves** were written and
**never called**, each under a comment promising it would "say so rather than stay
silent". Four of the rows were live: `Renderer::Draw` 13,125;
`AnimationState::Update` 17,950; `BeginDrawGroup` 26,249; **`Camera::operator=`
52,486** — the row the kickoff warns about by name. **Wave 28's "six of eight
printed nothing" was partly silence.** AVBODY's summaries print from a static
destructor, unconditionally; `audio_scene_update.cpp`'s `if (g_updates)` guard makes
"never ran" and "ran zero times" the same silence.

### 4.4 A promotion has a LIFETIME, not just a route (IOLANE)

Retail's `Reconstruct` destructs through the object's slot 0 (ours), then
**tail-jumps retail's constructor by an in-DLL rel32** — not a door — and **re-stamps
its own table over an object we built.** The known rule is "we cannot get our table
in". This is the mirror: **it can be evicted after it is in.**

### 4.5 A present/absent control cannot validate an ORDERING claim derived beside it (AVBODY)

IOLANE's screen had two mechanisms, both answering *"is there a hidden return
slot?"*, both correct. The claim that the slot comes **first** sat beside them and
inherited their credibility — it is the free-function rule, and for a member
function it produces **a wild write**. And inside the fix: **a known positive can be
genuine for one half of a screen and blind to the other** (`GetSource@Stream@IO` is
abstract and never touches RCX). The union-control defect, inside a single control.

### 4.6 Applying a warning as a blanket rule MANUFACTURES a hazard (AVBODY)

AVBODY applied the kickoff's `Camera::operator=` width trap to `OnSoundEnded`, built a
control, and it reported NOT CAUGHT — **correctly**: `OR` with the constant 2 is
width-invariant. **A not-caught control indicts the control first; here it indicted
the premise.** The kickoff issued that warning to every body lane as a standing rule.

### 4.7 A guessed known-positive is not a known positive (AVBODY)

A control symbol written from memory had the wrong mangling; the screen refused to
report and exited 2. **Hand a control a symbol you have READ.** It can only fail
closed by luck.

### 4.8 A tree-reading scan run after you start work fails in the ALARMING direction (GFXBODY)

GFXBODY's first "was this already ported?" scan reported **4 of 8 already
registered** — all four were its own registrations from minutes earlier. For ten
seconds it looked exactly like SLOTS' finding reproducing in a second namespace.

### 4.9 The debt is decomposed, and most of it is not work

In-game, **816 of 1526 name rows and 1424 of 1576 address rows were never asked
for.** Classified by what the tree does with each literal (name door):

| class | n | |
|---|---:|---|
| A — no door exists (stamp only) | 224 | silence carries **no information** |
| B — no route walked this run | **471** | the real "build or walk a route" set; IO 205, UI 135 |
| C — route exists in a probe left unarmed | **15** | **payable today, by setting an env var** |
| D — no consumer site in the tree | 106 | nothing consumes the row |

The attendance instrument reads the **live registry**, so it is **immune by
construction** to the source-parsing blind spot of §4.1. The right answer to a
source-parsing defect is an instrument that does not parse source.

### 4.10 A ported body can be gated to zero by an unpromoted table upstream (GFXBODY)

All four GFXBODY bodies attend **0** in a live world. Measured in one run: their only
callers are in `SunLight::AddDrawItems`, a slot of the **unpromoted**
`??_7SunLight@Graphics@@6B@`, so retail dispatches the DLL's body and our chain never
starts. **The promotion backlog does not just block its own rows; it silences correct
ports downstream of them.**

---

## 5. WHAT IS EXPLICITLY NOT CLOSED

- **The cutover wall** — `ExternalPtr<Material>::Serialize`, 19 of 25 instantiations
  enumerated, not ported.
- **14 real bodies** (§1.2), plus 7 written-and-gated.
- **VERIFY's 5 FAIL factory rows are not a defect list.** The set changed across three
  runs of one build. The missing piece is a **stability screen** (keep only bytes
  that fail in every run), not another comparison. Addressee: the Wave 30 lane that
  next touches `rt_factories_*.inc`.
- **FACTUI1's 31 UI factory classes are not covered** by the promoted oracle —
  `factory_ui1.cpp` has no roster accessor. A three-line addition covers them.
- **The slot sweep reads 551 traps over 132 tables. That is POPULATION, not
  regression** — reconciled exactly on the merged tip by running the same fixed tool
  against the pre-MINT table in a probe commit: **5** traps on the original 64 tables
  (SLOTS' figure), **546** on the 68 newly minted ones; 723 new slots = 546 unported +
  175 ported + 2 purecall. **The regeneration made 546 unported vtable slots visible
  that always existed.** MINT's "60 of 132 COMPLETE" is not readiness — a complete
  vtable is not a bootable one (§3, WALL).
- **The sweep does not model gating**: 27 `BANISHED_RT_ALLOC` rows read PORTED and
  would not fill a minted slot at cutover. Addressee: the Wave 30 kickoff author.
- **`BANISHED_RT_ALLOC` graduation** — 7 bodies written and unarmed. A measurement
  and a risk decision; needs more than one run per arm.
- **The `System::String` RELEASE idiom rests on one reading of the bytes.** The ADOPT
  half is corroborated across two namespaces. Addressee: whoever takes
  `SetupFileSystem`.
- **INSTR's §10.2 overlapping counts**: "A-involving 28" and "B-involving 50" **overlap
  and must not be added**; the non-overlapping split is **A 17 + B 39 + AB 11 = 67**,
  confirmed site-for-site on all four counts by GFXBODY's independently written
  scanner. The "28 / 28" match across the two scanners was a coincidence.
- **GFXBODY's all-eight-unported scan has not been re-run by anyone else.** Addressee:
  Wave 30; `gfxbody_regscan.py` is in its scratch directory and its method is in its
  lane doc §3.

---

## 6. INSTRUMENT AND HARNESS DEFECTS

**Fixed this wave:** census factory-family decomposition; macro-door drops (36/780,
and the ledger had named the **wrong trigger** — zero live instances of the
inline-fn-ptr shape, all 36 were `kName` constants); `--csv` corruption; stamper
`unattributed` fold-in; dtor-verify BAD → UNREC; `mint_slot_sweep` input pipeline,
table identity column, and comment stripping; ledger defect #7; the stale-verdict
screen (and see §7.5).

**Open, deflating first:**

1. **`isolation: worktree` did NOT reliably land lanes on the current HEAD.**
   GFXBODY's worktree was on a **Wave 21-era commit that did not contain the
   kickoff**; MINT's was also stale. Both lanes caught it themselves before
   branching; my audit found all eight correctly rooted **only because of that**.
   `git checkout -b <branch>` names a branch and says nothing about the commit.
2. **The Bash tool auto-backgrounds any command at 600s**, which launders a false lock
   grant past kickoff §0.3 — and it composes with a piped acquire into a **third**
   mechanism (a trailing command's exit 0 read as a GRANT). **Three hits this wave.**
   The mid-wave mitigation (short acquires, lane-controlled retry, `assert-held`
   before every launch) was confirmed effective by the lane that hit it.
3. **`*.log` is gitignored, so cutover evidence directories silently come out empty.**
   Two lanes hit it; it reproduced on attempt 33.
4. **The build is not byte-reproducible**, so md5 cannot certify "built from my
   source" — `assert-held <exe>` certifies *this deployed file is that file*, which is
   weaker than it reads.
5. **`addr_door_audit.py` is totally blind to data-driven doors** — it would report 0
   for a file describing 1312 (MINT, sharpening WALL's "40 is a floor").
6. **`mint_slot_sweep.py` reads `git show HEAD:`**, silently returning the pre-change
   answer for uncommitted work. (My §5 reconciliation had to use a probe commit.)
7. **No instrument crosses "vtable COMPLETE" → "vtable bootable".**
8. A generator progress line ("+45/+4 BaseTypeInfo closure") **reads as a delta and is
   not one**.
9. A mangled-name `strings` grep is not a valid build discriminator — which weakens
   one of the kickoff's four §0.2 checks. `nm` carried the gate at close.
10. `CLAUDE.md` names the wrong log for `[AutoStart]` (§2.1). Fixed at close.

---

## 7. MY OWN ERRORS, RECORDED

I wrote a kickoff that told eight lanes, eighteen times, to re-derive rather than
quote. **Then I quoted, inferred, and ruled things out with instruments that could
not see them — repeatedly, in the log I was keeping to catch exactly that.**

1. **Kickoff §1.3 carried `~adjacent-export` hints forward as table identities** with
   a caveat attached instead of deriving them. **Wrong on all six.** A caveat is not a
   derivation.
2. **Kickoff §1.2's "17 remaining bodies" was 23 when written.** I derived it from the
   census whose blind spots lane INSTR was being dispatched to fix — measuring with the
   instrument under repair, and publishing the result as the wave's scoping headline.
3. **I "independently verified" SLOTS' 67 with a grep that tested a different shape**
   (49), then **"corrected" SLOTS with an 87 that folded in an unrelated blind spot**,
   then **asserted zero adjacent-string concatenations exist — using a line-based grep
   to rule out a defect whose definition is "spans lines."** There are 28. **SLOTS' 67
   was right; all three of my numbers were wrong.** I instructed INSTR to delete a
   branch as dead code on the strength of the third one.
4. **I copied Wave 28's `RtUnresolved = 0` inference into WALL's brief as a premise.**
   It was false, and WALL's fault was precisely the case it excluded.
5. **My kickoff §0.4 arm was incomplete by two variables** (`BANISHED_RT_MINT`,
   `BANISHED_AUTOSTART`). The *method* I gave (recover from ATTEST lines) is what found
   the gap; my *application* of it did not.
6. **I instructed INSTR to "retire the 8 stale verdicts."** Seven were stale only
   because their sole caller is the probe that produced them; **obeying me would have
   deleted the triage of seven uncalled ports.** INSTR interrogated the instruction
   instead of executing it.
7. **I paired IOLANE with AVBODY on a guessed shared shape** (both return `String`).
   IOLANE's three returned `ref`, AVBODY's `byval`; the shared trap did not exist. The
   pairing paid anyway, in a direction I did not predict — which is luck, not design.
8. **At integration I ran `mingw32-make` without a timeout; the harness backgrounded it
   at 120s** — kickoff §0.1's *first* infrastructure warning, broken by its author. The
   §0.2 link gate is what made that build trustworthy, not the exit code.
9. **I guessed GFXBODY introduced the `SetBones` row.** It was SLOTS. I checked before
   writing it down, which is the only reason it is not in this document.
10. **I nearly declared the DLL-present smoke a failed world build** by grepping the log
    `CLAUDE.md` names (§2.1).

**Errors 1-6 are one failure**: I held lanes to derivation and held myself to
plausibility. Every one was caught by a lane — SLOTS, INSTR, IOLANE, WALL — and **not
one by me.** The pairing rule applies to the integrator too, and this wave had no
partner for the integrator. **Wave 30 should.**

---

## 8. GATES, ON THE FINAL MERGED TIP

```
mingw32-make standalone-debug     exit 0 (backgrounded; see §7.8 -- gate, not rc, is the evidence)
  artefact                         34,413,879 bytes, 15:37:21, after last source commit 15:37:16
  find <sources> -newer <exe>      EMPTY
  nm, one symbol per code lane     all present in the LINKED IMAGE
                                   (WALL, SLOTS, GFXBODY, IOLANE, AVBODY, VERIFY; MINT is data, INSTR is tools)
conflict resolution                1 conflict (standalone/main.cpp, VERIFY vs GFXBODY summaries):
                                   union by hand; 0 markers tree-wide; each summary called exactly once;
                                   rt_graphics_port.cpp auto-merge keeps BOTH SLOTS and GFXBODY registrations
cutover_manifest.py --inc          regenerated on merged tip: 2 door counts, rows + dispositions unchanged
cutover_manifest.py --gate         OK (59 rows)
rt_mint_gen.py --check             UP TO DATE (132 / 104 / 39)
mint_factory_census.py             103 PRESENT / 0 ABSENT
runtime_census.py                  1472 / 1317   89.5%   gap 155 = 21 BODY + 37 VTABLE + 49 DATA + 48 UNKNOWN
runtime_census.py --selftest-all   ALL GREEN
mint_slot_sweep.py                 132 vtables / 1312 slots / 616 ported / 551 traps  (reconciled, §5)
lane tips re-checked at close      8 of 8, 0 unmerged commits
DLL-present smoke                  world built, pause-menu Quit, exit 0, crash log 0 bytes
cutover attempt 33                 RtGraphics_MaterialInstance_Serialize+0x83 -- WALL's predicted wall
```

---

## 9. WAVE 30 INPUTS

1. **The wall: `ExternalPtr<T>::Serialize`, 25 instantiations, 19 enumerated.** Port
   the family, not the instance — attempts 26-30 already showed a one-symbol-per-boot
   family burns a wave per wall.
2. **Pin the lane base.** `git checkout -b wave30/<lane> <explicit-sha>`, and require
   every lane to print and report its merge-base **before** doing any work (§6.1).
3. **Give the integrator a pair.** Every consequential error in §7 was caught by a
   lane. A second reviewer on the kickoff's derived figures, before dispatch, would
   have caught §7.1-§7.3.
4. **The promotion backlog is now the lever on attendance** (§4.10). `SunLight`
   promotion would un-gate four already-ported GFXBODY bodies at once. Still gated on
   owning the stamper; `ours-capable` is now correctly scored (INSTR).
5. **VERIFY class C: 15 rows payable today by arming an existing probe.** Cheapest real
   verification in the tree — **and re-derive it first**, because it is phrased as a
   cheap win and this project has three waves of evidence about that section.
6. **The 471-row class-B route debt**, IO 205 / UI 135.
7. **Factory oracle: add the stability screen**, then add the UI roster accessor.
8. The **six newly visible UI bodies** (§1.2).
9. **`BANISHED_RT_ALLOC` graduation**, with more than one run per arm.
10. Harness: gitignore exception for cutover evidence; teach `mint_slot_sweep.py` about
    gating and about the working tree.
