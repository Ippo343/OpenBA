# WAVE 19 — integrator corrections to WAVE19_IO.md

Corrections found while dispatching, each measured on `ebbec6e` (= `wave19/base`).
The plan predicted this: Wave 18's plan took six corrections from its own lanes,
all from repeating prose instead of running a query.

## C1. Baseline CONFIRMED (no correction)

`tools/runtime_census.py --namespace IO` on `ebbec6e`:

```
IO   407 total   122 called   83 ported   0 detour   68.0%   ceiling 97.5%
identity: 83 >= 122 - 3 = 119      ->  36 SHORT
```

The census prints `Called but not ported (39)`; 39 - 3 UNPROMOTABLE(HARMFUL)
`?_singleton@{FileSystem,Modification,Persistence}` = **36**. Appendix A's set is
accurate on this tip. Three independent routes agree (identity, lane split
11+7+6+12, and the listing itself).

## C2. The screenshot debt is SIX unported IO exports, not four

**§4 lane FILE says:** "The first four are exactly the four exports
`SaveImageToFile` depends on".

**Measured:** `runtime/src/video/save_image_to_file.cpp` reaches **six** IO
exports that are in the unported set, all six through `RT_EXPORT_ONCE` doors:

| line | export |
|---|---|
| 293 | `?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` |
| 304 | `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` |
| 301 | `??0BinaryFileStream@IO@@QEAA@XZ` |
| 369 | `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` |
| 406 | `??1BinaryFileStream@IO@@UEAA@XZ` |
| —   | `??1FileStream@IO@@UEAA@XZ` (reached via the BinaryFileStream dtor chain) |

**The lane split does not change** — rows 5 and 6 are already lane FILE's rows 5
and 6. What changes is **lane FILE's acceptance criterion**: the cutover hole
shuts on **six**, and a lane that ports four and then runs the DLL-absent BMP
oracle would have found this the expensive way.

### The other deps are ported — and here is the control that makes that sayable

The file also reaches `?GetPathFilename@`, `?GetPathDirectory@`,
`?AdjustDelimiters@Filename@System@@`, both `??HSystem@@` String concats, and
`?MemFree@MemoryAllocator@System@@`. None appears in the unported set.

**My first attempt at this check was UNSOUND and is recorded here rather than
quietly fixed.** I grepped the `--namespace IO`-filtered listing, in which a
`System` symbol cannot appear *whatever its state* — a screen that could not
name its own subject, returning "ported" for a question it never asked
(`reference_a_screen_that_cannot_name_its_subject.md`).

Re-run against the unfiltered listing with two controls:
* **positive control** — `?CreatePath@FileSystem@IO@@` reads **1** (non-zero), so
  the grep matches when it should;
* **subject control** — the listing carries **24 `export System` rows**, incl. 8
  `Filename@System@@` mentions, so `System` is genuinely in the instrument's
  subject and a zero is a real absence, not a filtered one.

## C3. RETRACTED — Ghidra MCP is UP, on port 8089

**This entry originally read "Ghidra MCP is DOWN on this tip" and that was
false.** It is retained rather than deleted because the way it was wrong is the
subject of §6 and of `reference_a_screen_that_cannot_name_its_subject.md`.

**What I did:** probed `127.0.0.1:8080`, got `WinError 10061` (actively
refused), and reported the bridge dead. **The bridge does not serve 8080.** The
configured port is **8089** — `.mcp.json:9` and `bridge_mcp_ghidra.py:23` both
say so, and I had both files available. I probed a port nothing serves and read
its silence as a verdict about a subject I had never contacted.

**Measured:** `127.0.0.1:8089` responds, with **Runtime-x64.dll already open** —
base `0x180000000`, **15,290 functions**, 69,917 symbols. A `javaw` (pid 10268,
started 08:54) owns the listener.

The user caught this, not the process. Nothing in my own checks would have: a
refused connection is exactly what a down bridge and a wrong port both look like
from outside — the "no difference found" / "I measured nothing" confusion §6.6
names, in its network form. **The available control was free and I skipped it:
read the configured port before probing one.**

**Blast radius:** all three phase-0 lanes were briefed "Ghidra is DOWN", lane
NOOP most damagingly, since its entire cell is disassembly. All three have been
sent a correction. No lane had reported when it went out.

**What does NOT change:** §3.0a still requires the answer come from the SHIPPED
BYTES. Ghidra's decompiled C drops dereferences, prints MOVSXD as a zero
extension, and drops whole arguments at a mistyped vtable call
(`reference_ghidra_c_output_vs_disassembly.md`) — so disassembly, not C, and
`objdump` stays the independent second route for the two-route enumeration that
§3.0a's precedent requires. Also standing:
`mcp__ghidra-mcp__get_function_metrics` **ignores its `address=` parameter** and
silently falls back to a name lookup.

## C4. Repo hygiene at dispatch

All 17 wave13–wave18 lane branches are ancestors of `wave10/base`; `wave10/base`
was 51 commits ahead of `origin` and is now pushed, as are the wave17/18 lane
branches.

**One stranded fix was found uncommitted** in `wt-wave14-genname` after wave 14
closed: `ProbeSaveLoadGenerateSaveName` moved out of the boot-time
`RtOrphanProbe()` (which runs from `Init()`, before `Engine::Startup` builds the
`System::MemoryAllocator` singleton) into the in-game probe — the same relocation
`ProbeAudioScene` and `ProbeApplicationLaunchProcess` already carry, for the same
documented "WRITE to +0x648 of a null singleton" fault.

`wave10/base` still calls it from the boot probe. Committed as `3e08528` on
`wave14/genname` and pushed so it is not lost. It is **env-gated** behind the
orphan probe, so it bites only an armed run. **NOT merged** — a wave-14 fix is
not Wave 19's to land silently. Flagged for a decision.
(`reference_lane_closed_is_not_lane_merged.md`.)

## C5. I misread my OWN output to match a prediction (confirmation bias)

Twice — in the tasking message to lane MINT and in the lane TRAPS merge commit
`20aa588`, which is now permanent in the repo — I wrote that the §0b name-grep
counts "reproduce on this tip at precisely the predicted **4, 2, 2, 0, 0**."

**My own command output, in the same session, said 4, 2, 0, 2, 0.**
`??_7FileStream@` reads **0** and `??_7TextFileStream@` reads **2**; the plan has
them the other way round. I did not mis-run the check — the check was right and I
read its result as agreeing with the number I was expecting.

Caught by lane MINT, not by me, and not by any instrument. There was no screen to
fail here: the failure was in reading a correct output. That is the residue §6
cannot reach, and it is worth recording precisely because every mechanical control
in this wave was working.

**Substance unaffected** — five rows unminted by VA either way, and every non-zero
name hit is an `adjacent-export:` label (MINT confirms: 8 hits, all eight labels).
**What is affected is the standing of a "reproduced" claim.** A prediction I said
was "sharp and falsifiable" was in fact PARTLY FALSIFIED by my own evidence, and I
announced it as confirmed.

The plan's §0b should read **4, 2, 0, 2, 0**.

## C6. My harvest-gap hypothesis was WRONG — the file was STALE, not the generator

I gave lane MINT a hypothesis (anchors consumed as coordinates and discarded as
subjects), flagged as a hypothesis, with an explicit request to refute it. **It
refuted it**, and the real cause is simpler and cheaper:

`standalone/rt_data_mint_tables.inc` was last regenerated at **`4f037fd`** (Wave
10D). `tools/rt_mint_gen.py` has not changed since `7bf01e4`, which **precedes**
that. The four consumers that put these five vtables into the census's `called`
set all landed **after** it (Wave 11f lane IOSTREAM). **Running the generator
UNMODIFIED mints all five.**

The generator was *correct* at `4f037fd` — at that commit the only occurrences of
those names were one comment and its own labels, and it already strips comments
and `SCAN_SKIP`s its own `.inc`. The memory I pointed MINT at
(`reference_mint_population_rule_blind_to_its_own_output.md`) is **not** this
defect; its two closure rules are present and firing.

**The generalisable lesson:** a GENERATED artefact in the tree is a measurement
with a TIMESTAMP, exactly like a stated blocker. Nobody asked when the `.inc` was
last produced. Both my hypothesis and the plan's ("a harvest gap; fixing the
generator is cheaper than five rows") assumed the *code* was wrong because the
*output* was wrong.

MINT added `tools/rt_mint_gen.py --check` (exit 3, names entered/left rows),
because **the real defect had no detector at all** — and that checker's own
positive control caught a regex flaw that review had missed.

## C7. §0b's thesis is only HALF right — the five rows are still port work

**Do not let the mint fix read as a worklist reduction.** MINT minted all five and
the identity moved by **zero**: `83 >= 122 - 3`, still 36 short at that point.
`called`, `ported` and `unpromotable` are all unchanged and the census still lists
all five as unported.

Measured: **52 slots, 1 ported, 51 traps, 0 purecall — all five BLOCKED.** 41
distinct bodies, 12 unexported.

So §0b converted these rows from **unmeasured** to **measured and blocked**. That
is real progress — phase 0c could not scope them at all before, and the readiness
screen now covers 13 IO rows instead of 9 — but it is not the "not port work at
all" the plan hoped for.

**The genuine saving is elsewhere, and it is large**: the eight `IO::FileStream`
base slots are traps in **three** of the five rows, so **one body of work clears
8 + 8 + 7 = 23 trap slots.** That compounds with phase 0c's finding that IO's 8
blocked rows carry 27 trap slots across only 20 distinct functions. **Both cells
independently found the same shape: scope FUNCTIONS, not slots.**

## C8. I broadcast two numbers that were later refuted — into two live lane briefs

`27 trap slots / 20 distinct functions` and `??_7ReaderWriter@ slots 31-33 close
6 slots across 2 rows` went into lane CSTREAM's and lane IOBASE's tasking as
**scoping multipliers**. Both are wrong: **24 / 17**, and **3 slots in ONE row**.

Lane TRAPS refuted its own sealed claim from the bytes. Both refuted items had
been flagged in its sealed text as *"confirm from the bytes"* — **the lane marked
its own uncertainty and I passed the items on without the marking.** That is the
failure worth recording: the doubt was written down, and it did not survive
transmission through me.

Retractions were sent to both lanes as soon as the tranche arrived, with
instructions to rebuild any distinct-body list derived from the bad figures.

**The shape survives, the magnitude did not.** "Scope by FUNCTION, not by slot"
holds at 24-across-17 exactly as it did at 27-across-20, and `?GetSource@Stream@IO@@`
as the sole trap in four rows is **not** among the retracted claims.

### The underlying defect, which outlives this wave

`kMintVtables` sizes every row by `adjacent-export:<next symbol>`. That
construction **cannot see an UNEXPORTED table lying between two exported ones.**
One does — `0x1801554a8`, 11 DATA xrefs stamping it as a second vptr.

`??_7ReaderWriter@IO@@6B@` is **31 slots**, not 36; its last five were minted
**twice**; the screen's `546 slots` double-counts 5 and prints
`slot-count suspects: 0` — **a false green on that very row.** Every lane was told
to quote greens with denominators; this is a green that was *wrong*, which no
denominator would have caught.

**Fix, named and not done in this wave:** size rows by **next STAMPED address**,
not next export. It would have caught ReaderWriter automatically. TRAPS warns the
non-IO rows may carry the same defect and should be run through this test **before
Wave 20 scopes them.**

## C9. My "sharp falsifiable prediction" could not discriminate — MINT was right to say so

I told lane MINT its anchor hypothesis made a "sharp, falsifiable prediction":
that every non-zero name hit would be an `adjacent-export:` provenance label.

MINT confirmed it — all 8, read individually — and then pointed out the flaw:
**staleness entails the same prediction.** A stale file can only carry those names
as another row's proof label. **A prediction that BOTH hypotheses make cannot
select between them**, so confirming it was worth nothing as evidence for mine.

I called it sharp; it was merely *true*. That is a different property, and the
distinction is the whole of `reference_two_checks_one_hidden_premise.md` — two
checks sharing a hidden premise are one check. Here it was one check dressed as a
test between two rivals.

What actually settled it was a fact I never asked for: **the generator, run
UNCHANGED, mints all five** — which a generator that structurally discarded
anchors could not do. One decisive observation beat my carefully-framed
prediction, and it was already sitting in the lane's commit `3620e69` **before I
sent the hypothesis.**

## C10. The promotion criterion — the most reusable thing phase 0 produced

From lane MINT, and it generalises well beyond IO:

**Every stamp of these vtables is an in-DLL RIP-relative `LEA`, not a door**
(`1800a8270 LEA RAX,[0x1801554d0] -> MOV [RDI],RAX`). Therefore:

> **A vtable row is promotable exactly insofar as WE OWN THE FUNCTION THAT STAMPS IT.**

This is the operational form of
`reference_a_ported_row_can_be_permanently_unpromotable.md` — we intercept only at
**doors**, and an in-DLL rel32 call or a RIP-relative LEA stamp is not a door.

It was validated the right way: it **predicts the attendance data row by row**, by
two instruments gathered by entirely different means. The ATTENDED rows are
precisely those whose *ported stamper* runs on the driven route.

**Three consequences already acted on:**
1. `post=0` on a row with **no ported stamper** is **not route-limited** — no path
   on this tree could open that door. That is a *diagnosis*, and it retires a
   route-hunt that would have found nothing.
2. **`??0BinaryFileStream` is the ONLY stamper of `??_7BinaryFileStream@`**,
   making it the **cleanest** promotion of the five rather than the hardest — and
   it is *already* one of lane FILE's code rows and a screenshot dependency. Two
   independent reasons it is on the critical path.
3. `??_7FileStream@` has **nine** stampers including `Engine::Run`, so TRAP=0
   there would still leave most stamps on retail's side. **A clean trap count is
   not a promotion.**

**Lane IOBASE has been told to apply this to `??_7BinaryWriter@` BEFORE hunting a
route.** 26/26 TRAP=0 says the table is safe to stamp and says nothing about who
stamps it. If no stamper is ours, "blocked on attendance" is the wrong diagnosis
and no `BANISHED_*` variable fixes it.

## C11. `?GetSource@Stream@IO@@` — arbitration, and the "four rows" figure is stale

Lane CSTREAM claimed `?GetSource@Stream@IO@@UEBA?AVString@System@@XZ`
(`0x18000c6a0`); phase 0c §6.1 had assigned it to lane IOBASE. **Awarded to
CSTREAM**, on measured grounds rather than on who asked first.

Rows whose slot block references `0x18000c6a0`, measured on the merged tip:

| row | VA | slots | lane |
|---|---|---|---|
| `??_7CompressedStream@` | `0x180155640` | 9 | CSTREAM |
| `??_7CompressedFileStream@` | `0x1801555f0` | 9 | CSTREAM |
| `??_7CompressedMemoryStream@` | `0x180155560` | 9 | CSTREAM |
| `??_7MemoryStream@` | `0x1801555b0` | 8 | IOBASE |
| `??_7VisitorStream@` | `0x180155a20` | 7 | IOBASE |
| `??_7Stream@` | `0x1801549a0` | 7 | NO-OP (phase 0a) |

CSTREAM carries **three of the five live rows** and the body sits in its class
family. IOBASE consumes it; CSTREAM registers by NAME **and** ADDRESS so IOBASE's
rows need no second registration — a duplicate would be a **SHADOWED** slot where
the later binding wins and the checked line is dead code.

### The figure itself was stale — it is SIX rows, not four

"Sole trap in FOUR rows" predates phase 0b's regeneration, which minted five more
rows. **I repeated it into two lane briefs after the regeneration was already
merged.** Both lanes have been told it is superseded, and told to re-derive
whether it is still the *sole remaining* trap in their rows rather than inherit
that too.

This is the **third** figure in this wave that decayed between measurement and
use (after 27/20 and the plan's 4,2,2,0,0). All three shared one property: they
were quoted downstream **without the timestamp of the tree they were measured on.**
The wave's own doctrine — a stated blocker is a claim with a timestamp — applies
to *measurements*, not just to blockers, and phase 0b already proved it applies to
**generated files**.

## C12. A phase-0 saving can GROW a lane — CSTREAM went 8 -> 14 bodies

Reported up front by the lane rather than at close, which is the behaviour we
want. Phase 0c wrote "scope 8 functions" while `??_7CompressedFileStream@` was
**UNMINTED and UNMEASURED**; phase 0b then minted it as a full 9-slot **ATTENDED**
row. Re-measured on CSTREAM's tip: 3 rows, **27 slots -> 14 distinct functions**,
3 already ported, so **11 DATA bodies + 3 CODE rows = 14**.

**Superseded, not wrong** is the right characterisation, and it is the C7 shape a
second time: **a cell that converts rows from UNMEASURED to MEASURED can only ever
grow the visible denominator.** Both times the wave's numbers moved *against* us
for a reason that is pure progress.

Expect the same from any future measurement cell, and do not let a growing lane
size read as a lane falling behind.

## C13. `?GetSource@Stream@IO@@` moves the identity by ZERO — the wave's own metric misread

Found by lane IOBASE, **verified independently by the integrator before adoption**:

```
GetSource hits in runtime_census.py --namespace IO   : 0
control  ?CreatePath@FileSystem@IO   (must be present): 1
occurrences anywhere in the tree: standalone/rt_data_mint_tables.inc ONLY
```

`?GetSource@Stream@IO@@` appears in the census **neither as called nor as ported**.
Its six `RT_MINT_NAMED` rows are **not a call door**. Therefore porting it enters
as **+1 `called` and +1 `ported` simultaneously**, leaving `called - ported`
unchanged, and it lands in the census's **"ports with no caller"** list.

Phase 0c called it "the cheapest single action in IO's entire DATA set" and **I
amplified that to two lanes without this qualification.** The phrase is right
about cost and right about leverage. It is **wrong if read as an identity move**.
Its whole value is as a prerequisite for four-to-six `??_7` rows reaching READY —
and it is a **`??_7` row's PROMOTION** that adds +1 to `ported` against a `called`
the mint file already contributed.

### Why this one matters more than the arithmetic

**The wave's success criterion is the identity, and here the identity is blind to
real work.** A body that unblocks six rows scores zero. This is the mirror image
of the ceiling trap in §0: there, the percentage moved without progress; here,
progress moves without the metric. **Both failures come from scoring a number
instead of naming what changed.**

Operational rules, now issued to both affected lanes:
- **Do not book a shared prerequisite as progress against the identity.** Book it
  as *unblocking N rows*, and name which N.
- It will report **`calls=0`** from any diff oracle. That is EXPECTED here, not a
  red flag — but it means **the oracle cannot verify it**
  (`reference_uncalled_port_is_not_verified.md`: a port with zero callers reports
  clean). Verification needs a **synthesised** caller;
  `reference_no_route_is_not_no_oracle.md` records a synthesised oracle for a
  provably-unreachable body finding a real defect — a missing third argument
  writing caller garbage into a save. With a hidden return slot in play, that is
  exactly the defect class here.

### The body, disassembled by the integrator to settle a lane disagreement

CSTREAM read 10 instructions, IOBASE read 11. Both are right — 10 plus the `ret`.
A small discrepancy, checked rather than waved through, because this wave has
already lost three figures to unchecked transmission. At `0x18000c6a0`:

```
mov  %rdx,0x10(%rsp)                 ; spill the retslot
sub  $0x18,%rsp
movq $0xfffffffffffffffe,0x8(%rsp)   ; SEH state = -2
xor  %eax,%eax
mov  %eax,(%rsp)                     ; SEH state var = 0   <-- NOT object state
mov  %rax,(%rdx)                     ; THE payload: one qword zero via the retslot
movl $0x1,(%rsp)                     ; SEH state var = 1   <-- NOT object state
mov  %rdx,%rax                       ; return the retslot
add  $0x18,%rsp
ret
```

Both lanes read the semantics correctly: `String GetSource() const { return
String(); }` — an 8-byte null through the **hidden return slot in RDX**. The point
neither stated: the two `(%rsp)` writes are **compiler-generated SEH state, not
object state**. A faithful port reproduces the qword at `[rdx]` and the RDX
return, not the frame bookkeeping.

### Arbitration outcome

**Ownership STAYS with lane CSTREAM** (3 of the 5 live rows; class-family fit).
IOBASE's counter-argument — that it is blocked and CSTREAM is not — was weighed
and rejected on the evidence: **IOBASE's own distinct-body list holds 16 bodies**,
and GetSource gates the *promotion* of two DATA rows, not the writing of anything
else it owns. CSTREAM is required to land and register it early (by NAME **and**
ADDRESS) and report the commit; IOBASE is told not to write a local copy, since a
second registration of the same address is a **SHADOWED** slot where the later
binding wins and the checked line becomes dead code. Both lanes were given the
reversal condition explicitly, so the decision is falsifiable rather than final.

### One framing worth keeping

IOBASE stated its scope as **16 distinct bodies, of which the identity can move by
at most +10**. That is the right way to say it. **Bodies and identity are
different currencies**, and most of this wave's confusion — C7, C11, C12 and this
entry — has come from quoting one as the other.

## C14. "The machine has NO SAVE GAMES AT ALL" is FALSE — and I promoted it to a wave-level fact

Lane PERSIST reported it. **I amplified it into the PERSIST merge commit as "a
WAVE-LEVEL fact, not a lane one", and into a user-facing summary, without running
a single check.** It is wrong.

```
C:/Users/USPC/Documents/Banished/Save/*.sav  ->  13 real save files
  Auto.sav    8,576,259 bytes   Sep 10 15:05
  Auto2.sav   8,428,302 bytes   Sep  9 20:06
  Save0.sav   4,097,377 bytes   Aug 29 13:02
  Save10.sav  7,288,854 bytes   Sep  5 18:44   ... and 9 more
```

Three of them **predate this session by nearly two weeks**, so this is not a case
of saves appearing after the lane looked.

And **`BANISHED_SAVE_TEST` exists** — 4 code files, dispatched from
`code/Game/Game/gameupdateloop.cpp:1097` into `standalone/saveload_harness.cpp`.
It is a **PRODUCER**: it triggers the retail autosave path on a chosen frame and
logs the resulting `.sav`. The lane said plainly that it had **not** grepped it
and would not claim either way — **the honest half of the report was correct, and
I filled the gap with the lane's other claim instead of a grep.**

### Why this one is the worst of my errors this wave

The other corrections cost accuracy. **This one cost a lane its verification
route.** PERSIST's six ported rows are recorded as having **no live gameplay
route**, resting entirely on a synthesised oracle. Its §5 control is sound in what
it proves — the probe-off run reaches none of the six — but that is evidence
`BANISHED_AUTOLOAD` **did not fire**, which is not the same as **no route exists**.
The wave's own §5 names "declaring no oracle or no route" as a failure mode
refuted four times in Wave 18. It has now been refuted a fifth, by me, against a
lane I had personally briefed with that exact warning.

**Standing rule, from three instances now (C8, C11, C14): a lane's NEGATIVE claim
about the environment is the single most dangerous thing to relay, because it
cannot be checked by reading the lane's own work — only by running a command the
integrator can always run.** Relaying it unchecked converts one lane's local
failure into a wave-level fact. `ls` would have caught this.

### Actions
1. Lane PERSIST re-tasked to re-run its six rows against a real save, as a marked
   post-report tranche.
2. The PERSIST merge commit's claim is corrected here rather than rewritten — the
   commit stands with its error, and this entry is the correction of record.

## C15. Lane PERSIST corrected its OWN stated blocker — `??1Persistence@` was never blocked

The lane wrote, and committed, that `0x1800b7740` "joins and closes a real thread;
handing it a fabricated worker is not a controlled experiment, it is a crash with
extra steps." It then read the body and found that **wrong in its conclusion**:

The wait is `WaitForSingleObject` on a handle **at an offset the caller controls**
(`Pers+0x20`), followed by four `CloseHandle`s on handles at `+0x10/0x18/0x20/0x28`.
Seed the scratch image with owned, already-signalled handles and the wait returns
immediately. **The existing oracle applies unchanged in structure** — four seeded
handles instead of two, plus one "the arrays were cleared" assertion.

The lane's own words: **"What had to change for that to become possible: nothing.
It was possible when I wrote the holdback; I asserted a blocker instead of reading
`0x1800b7740`."**

**This is the stated-blocker-with-a-timestamp trap where the stale claim is the
lane's OWN, written the same session** — the sharpest instance the project has
recorded, and it was self-caught and self-reported at the cost of one `objdump`.

Two side effects: `Pers+0x30` becomes a **pinned field** rather than the
`_pad30` the header currently calls it (the dtor pins it), and the dtor's
`0x18002ac40(Pers+0x80)` confirms the saves array is a `Collection::Array` that
`GetSaveInfo`/`GetSaveCount` only ever read.

**Written expiry (Wave 18 form):** verified by the existing
`persistence_saves_probe.cpp` oracle with four owned Win32 handles seeded at
`Pers+0x20/0x28/0x30/0x38` and non-null array pointers. **Expires the moment
anyone runs that probe variant** — under an hour, layout work already done. The
lane has been re-tasked to do exactly that.

## C16. C4 is CLOSED — the stranded fix is merged, on two independent reproductions

Lane PERSIST hit the C4 fault independently: `BANISHED_RT_ORPHAN_PROBE=1` dies at
boot with `WRITE of 0x648` inside `ProbeSaveLoadGenerateSaveName`. It worked
around it rather than fixing another wave's code, which was the right call, and
told me — which was the more useful one.

Two independent reproductions of a fault whose fix already existed and sat
unmerged. Merged as `e983724`.

**The merge conflicted**, and the conflict was informative: `wave10/base` had
since gained `ProbeSaveLoadSingleton()` at the same call site from a different
Wave 14 lane. Resolved to keep **both** intentions — `ProbeSaveLoadSingleton()`
stays at boot (it reads a data slot and never formats a `String`, so it does not
touch the allocator singleton), while `ProbeSaveLoadGenerateSaveName()` moves
in-game. Taking either side wholesale would have silently dropped one lane's work.
Build green.

## C17. C13's MECHANISM was wrong — the conclusion survives, the explanation did not

Lane IOBASE measured what C13 asserted. **C13 said** a caller-less port like
`?GetSource@Stream@IO@@` "enters as +1 `called` AND +1 `ported` simultaneously,
so `called - ported` is unchanged."

**Measured:** it adds **nothing to either.** `runtime_census.py` builds its table
only from symbols that **have a call door**; a registered-but-unresolved symbol
lands in **"Ports with no caller (234 exports)"** instead, which feeds no column.

**The conclusion — zero identity movement — is correct. My reasoning for it was
not, and a right answer from a wrong mechanism is worth flagging**, because the
mechanism is what a lane reuses on the next symbol. It also has a consequence
C13 did not draw: this covers **8 of IOBASE's 15 registrations**, and the census's
own rule then applies to them — **a caller-less port is NOT verified by a clean
run.** IOBASE declined to bank those 8 rather than counting them, which is right.

## C18. The grep-before-you-write rule needs a THIRD form — the linker caught what two greps missed

Lane IOBASE nearly rewrote `~MemoryStream`, **which was already ported.** Its grep
searched the **mangled name** and the **retail VA** and returned only comments.
The body exists as **`RtIo_MemoryStream_Dtor`** — in a file called
`compressedstream.cpp`.

**The LINKER caught it, not the grep.** And Lane W had **predicted this exact gap**
in a comment at `compressedstream.cpp:966-971`.

`reference_grep_for_the_port_before_writing_it.md` has now failed in a new way:
the two forms we tell lanes to grep (mangled name, VA) are both blind to a body
that carries **our own naming convention** in a neighbouring class's file.

**Rule, now issued to the live lanes:** grep the mangled name, the retail VA, **and
the plausible `RtIo_<Class>_<Method>` symbol name** — and **treat a duplicate-symbol
link error as a discovery, not a nuisance.** This is the fourth instance this wave
of an instrument being blind in a way it never announced.

## C19. `??_7Stream@IO@@6B@` is READY and must STAY UNPROMOTABLE — verified

IOBASE's `?GetSource@` filled the last trap on `??_7Stream@`, taking it to
**7/2 TRAP=0 READY**. It correctly did **not** promote it and left phase 0a's tag
alone. Verified on the merged tip:

```
reg_mint_promoted.cpp:1126  RT_UNPROMOTABLE: ??_7Stream@IO@@6B@ ... NONE, EVER
census:  ??_7Stream@IO@@6B@  [PORTED ... UNPROMOTABLE(NO-OP) ... §3.2]
```

**READY means SAFE TO STAMP, not that anything dispatches it**
(`reference_a_green_readiness_screen_is_not_a_route.md`). A lane reading only the
readiness screen would reach for this row. It is a NO-OP abstract base — phase 0a
settled that from the shipped bytes — and promoting it would be wrong however
green it reads.

**A stale-number footnote on the tags themselves:** both `RT_UNPROMOTABLE` lines
still carry `[INHERITED]` slot counts — `2/36` for ReaderWriter and `1/7` for
Stream. ReaderWriter is **31 slots, not 36** (TRAPS tranche 2), and Stream is now
**2 ported, TRAP=0**. **The verdicts are right; the numbers beside them are stale.**
Fifth instance of the wave's recurring failure, and this one sits in the tree
rather than in a document.

## C20. A COORDINATION FAILURE THAT IS MINE: `?GetSource@` was ported TWICE

Both lanes ported `?GetSource@Stream@IO@@` (`0x18000c6a0`):

```
wave19/iobase  : runtime/src/io/iobase_vtables.cpp            (4 refs)
wave19/cstream : runtime/src/io/compressed_stream_write.cpp   (3 refs) + a probe
```

**I arbitrated it to CSTREAM, IOBASE requested it in a message that crossed mine,
I re-affirmed CSTREAM — and IOBASE built it anyway.** The arbitration existed
precisely to prevent this and it failed, because I treated "I sent the ruling" as
"the ruling landed before work started". With four concurrent lanes and messages
in flight, **a ruling on a shared body needs an ACKNOWLEDGEMENT before the losing
lane proceeds**, not just a send.

Resolved by reversing myself: **CSTREAM drops its copy and consumes IOBASE's**, on
the single ground that IOBASE is sealed and CSTREAM is still live. Not a quality
judgement — and CSTREAM was told to **keep its PROBE** if it is the better one,
since IOBASE reports **no differential oracle in its lane at all** as its own
largest named gap.

The specific hazard this creates is a **SHADOWED** registration — two registrations
of one address, later binding wins, earlier checked line silently dead.
`dispatch_slot_map.py --check` on the merged tip reads **0 addresses registered
more than once**, so the tree is clean today; it must be re-checked after CSTREAM
merges.

### Identity after PERSIST + IOBASE

```
97 >= 123 - 5 = 118      21 SHORT     (36 at wave start)
```
Build green. `--check` clean. No percentage quoted.

## C21. C14 is CLOSED — the live route exists, and 13 is the number

`BANISHED_AUTOLOAD=Save0` drove the real route: **"save list ready (13 entries)"**.
The route exists, lane PERSIST's six rows ran, and **13 matches exactly** the count
the integrator measured on disk. Both halves of C14 are now confirmed by a second,
independent route — the lane's own run rather than my `ls`.

### The failure mode underneath it deserves its own name

The lane had **already found, quoted and cited** `BANISHED_AUTOLOAD` at
`standalone/autostart.cpp:147`. Its §2 reasoning then correctly deduced that the
missing thing was **the dialog route, not the save file** — arguing from its own
control that `GetSaveCount` never registered at all, and that
`?UpdateList@SaveLoad@Save@@` would have called it even for an empty list, so the
route never opened a save/load dialog.

That reasoning was **right**. And in the lane's own words:

> *"That reasoning was right and I then failed to act on it... I diagnosed the gap
> accurately and then wrote a negative claim instead of running the driver."*

**This is a distinct failure mode from every other one this wave has catalogued.**
It is not a stale claim, not a blind instrument, not an unchecked inheritance.
**It is stopping one step short of the command that would have settled it**, while
holding all the information needed to run it.

I did no better one level up: I relayed the negative claim into a merge commit and
a user-facing summary without running `ls`. **Same shape, and mine had a wider
blast radius.**

## C22. My supposed instrument defect was WRONG — `abi_retslot_audit` is a good citizen

I asked whether `abi_retslot_audit` states its 77-unaudited denominator "or whether
you had to derive it", and pre-announced that if a lane must derive it, that is an
instrument defect. **The premise was inverted. The tool states it. The lane derived
nothing.** Verbatim from its own output:

```
COVERAGE: 56 of 133 distinct by-value-returning mangled names in the tree reach a
door this scanner can read; 77 do NOT and are UNAUDITED.
BLIND SPOT: it matches a literal ResolveExport("...") assignment bound to a
findable fn_*_t typedef. A resolve through a macro door (RT_EXPORT_ONCE /
RT_DATA_SLOT), a named constant, or a direct call with no typedef is invisible.
```

The macro-door limitation is **documented in the tool**, not discovered by a lane.
On the §6.6 test — *can it say "I MEASURED NOTHING"?* — **it passes.**

**I inferred an instrument defect from a LANE'S SUMMARY of a tool's output rather
than reading the output.** That is C14's error one level up: treating a downstream
report as evidence about an upstream artefact. Had it gone unchecked it would have
**manufactured work** and levelled a false accusation at the best-behaved
instrument in the wave — the INFLATING blind spot of
`reference_a_screen_that_cannot_name_its_subject.md`, this time in the integrator
rather than in a tool.

### The real defect is narrower, and it IS being fixed

The **headline** line reads `0 suspicious of 61 by-value-returning exports checked,
0 unchecked` — one line above `77 UNAUDITED`. The two are reconcilable
(`unchecked` = matched a door, arity unreadable; `UNAUDITED` = reached no readable
door at all) but they **read as contradictory**, and the headline is **quotable out
of context as complete coverage**. A lane quoting it and stopping reports a clean
sweep of a subject that is **58% invisible**. That is §6.1 — quote a green WITH its
denominator — not a missing denominator.

Fix authorised as a separately-revertable commit in PERSIST's tranche: fold the
qualifier into the headline so it **cannot be quoted alone**. Second repair to this
tool after Wave 18's `fa85957`, and to be recorded as an **improvement**, not a
defect finding.

## C23. Wave 20 scoping input: the save route and lane FILE's debt share code

`?GetSaveFilename@Persistence@IO@@` builds `<savePath>Save\<name>.sav`, so the
`Save\` directory is created by **`?CreatePath@FileSystem@IO@@` — lane FILE's row**,
which landed this wave.

Two consequences: a save/load route cell is **cheaper than it looked**, and
`CreatePath` gains a **second independent exercise path** beyond the screenshot
oracle — which matters, because FILE reports its Half-A comparison does not
attribute to the lane at all.

Lane PERSIST's scoping of that cell is accepted as the right shape: **"open the
dialog and produce one save", not "produce one save."** `BANISHED_PAUSEMENU_BUTTON`
exists but the Save/Load id is unknown; (1) opening the dialog alone unlocks
`GetSaveCount` and `BeginGetSaveGameList`, while (2) a real `.sav` is what unlocks
`GetSaveInfo`, `GetSaveFilename`, `BeginLoad` and `BeginDelete` **on real data**.
Keep mod state constant across any save/load comparison.

## C24. TextFileStream vtable is 10 slots, not 20 — the sizing defect, second instance, in a number I passed on

I gave lane FILE `??_7TextFileStream@ 20 slots / 0 ported / 20 TRAP` **in the same
message that warned it the `adjacent-export` sizing was unreliable** — and did not
apply my own warning to the number I was sending. **Fourth figure this wave to
decay in transmission through me** (after 27/20, the name-grep counts, and "sole
trap in four rows").

**Measured: 10 slots.** An unexported table sits at **`0x1801559d0`**, exactly 10
slots in. Lane FILE settled it by **three routes, none of them adjacency**:

1. **The CONSTRUCTOR NAMES THE ADDRESS** — `??0TextFileStream` at `0x1800ba431`
   is `LEA RAX,[0x1801559d0]`.
2. The byte dump shows ten coherent method pointers ending at
   `?Create@TextFileStream@`, then a different class's descending group.
3. `runtime/types/io/FileStream.h` has said 10 since Wave 11.

Route 1 is the strongest evidence for a slot extent produced this wave, because it
is **the WRITER, not a reader**
(`reference_a_writer_beats_readers_agreeing_on_a_sign.md`).

### The generalisation, which is worth more than the correction

Lane FILE identified the mechanism and reported it to lane MINT directly:

> **Every concrete IO stream ctor heap-allocates an 8-byte Encoding and stamps an
> UNEXPORTED vtable into it.**

So `adjacent-export` sizing will keep **over-sizing rows across this entire
family**. That is now **two confirmed instances** — ReaderWriter 36→31 and
TextFileStream 20→10 — **plus a mechanism that predicts more**, which is a
different epistemic standing from two coincidences.

**This settles the Wave 20 prerequisite: size `rt_mint_gen.py` rows by NEXT STAMPED
ADDRESS, not next export.** Lane TRAPS proposed it; lane FILE supplied the
mechanism that makes it necessary rather than tidy.

FILE also **re-derived the other two rows from the bytes** as instructed: both
correct at **8**, with BinaryFileStream slots 1–7 **byte-identical** to
FileStream's. My claim held, and I would rather have it checked than trusted.

## C25. Lane FILE re-scoped its DATA rows by FUNCTION and handed one back

Three rows are **17 distinct bodies, not 36 slots**:

| row | slots | bodies it ADDS |
|---|---|---|
| `??_7BinaryFileStream@` | 8 | slot 0 (`0x180023d80`, unexported) + 7 shared base = **8** |
| `??_7FileStream@` | 8 | slot 0 (`0x18000c090`, unexported) **only — 1**; the other 7 are the same bodies |
| `??_7TextFileStream@` | 10 | slot 0 (`0x18002ba90`) + 7 own; slots 1 and 7 are FileStream's = **8** |

**9 bodies clear BOTH FileStream rows completely.** The scope-by-function
discipline both phase-0 cells converged on, applied a third time and paying out
again.

**TAKING** the 9-body union. **HANDING BACK** `??_7TextFileStream@` — and the
stated reason is the part worth recording: **not size (it is 8 bodies), but
CONNECTEDNESS TO THE HEADLINE.** It is off the screenshot path, and closing
unfinished business beats opening a third front. Held as a scoped follow-up: **10
slots, 8 bodies, driver is SaveText.**

FILE also committed **not to read FileStream's 7 free traps as a promotion** —
nine stampers including `Engine::Run` stay on retail's side. **TRAP=0 IS NOT A
PROMOTION**, held even though it makes the lane's own numbers look worse.

## C26. SaveText is the highest-leverage item left — it unlocks THREE things across two lanes

Lane FILE asked whether to do its two PORTED-NOT-VERIFIED rows before the DATA
work. **Answered YES**, and not merely for consistency with what I had said:

1. FILE's row 7 (`??0TextFileStream@`), attendance 0 today.
2. **Lane IOBASE reports `?SaveText@External@Object@@` STAMPS TWO of the five
   minted vtables** — their `post=0` is explained by SaveText not running on the
   gameplay route, NOT by a missing interception point. A SaveText harness
   converts two IOBASE rows from unmeasured-attendance to measured.
3. `??_7TextFileStream@`, the handed-back row — SaveText is its natural driver.

**No lane could have seen this from inside itself.** It is the clearest case this
wave for the integrator seat existing at all, and the counterweight to that seat's
four transmission failures.

FILE was told to **grep `standalone/` and the `BANISHED_*` list before building
one.** That rule has paid out repeatedly this wave: IOBASE found
`BANISHED_TOOL_HARNESS=dlgsettings` that way and correctly recorded UNMEASURED
when it did not fire; PERSIST's entire save-route gap was a driver it had already
found and not run. **Assume the harness exists until a grep says otherwise.**

Row 8 (`??1FileSystem@`) is an engine-lifecycle dtor and may need a different
answer. If it has no driver it stays PORTED-NOT-VERIFIED **with a written
expiry** — and read the callee before writing the holdback, since PERSIST wrote
one for `??1Persistence@` and retired it with a single `objdump`.

## C27. file_stream_probe.cpp — found independently by TWO lanes, built by neither

Lane IOBASE names "no differential oracle in this lane" as its largest gap and
identifies `runtime/src/io/file_stream_probe.cpp` as a ready-made skeleton. Lane
FILE found the same file from its assigned grep, and adds what IOBASE did not
measure: it **already drives these exact classes and carries five negative
controls** (`BANISHED_RT_IOSTREAM_NEGCTL=1..5`).

**The caveat both lanes must carry: today BOTH its sides go through raw doors, so
it compares retail with retail** — the shape that left a Wave 5 `Goal::Serialize`
probe dark for five waves. It needs an ours-side pass before it can verify
anything.

**Two lanes found the same asset and neither built it.** FILE was told to announce
loudly if it builds the ours-side for rows 7/8, because it likely closes IOBASE's
largest named gap at the same time.

## C28. Prose ABOUT a generated artefact is also a measurement with a timestamp

`runtime/src/io/file_stream.cpp`'s header states all three vtables "appear ZERO
times by VA in `standalone/rt_data_mint_tables.inc`". **True at `20aa588`, false
now** — all three are minted, verified by VA at 2 hits each. FILE's own report
§5 and §8 inherit it.

Phase 0b established that **a generated artefact is a measurement with a
timestamp**. FILE has now shown the rule extends one step further: **prose
DESCRIBING a generated artefact is too**, and it decays silently because nothing
regenerates a comment.

FILE will annotate rather than rewrite, per §6.16. **It was instructed to state in
the annotation that the claim was TRUE WHEN WRITTEN** — that is the entire point
of the timestamp rule, and a reader who sees only "this was wrong" learns the
wrong lesson.
