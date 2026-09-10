# WAVE 19 CLOSED — IO to 12 short, and the metric that could not see half the work

**Closed 2026-09-10 by the integrator.** Five porting lanes and three phase-0
cells, all merged. Build green, every standing checker clean.

Read with **[WAVE19_INTEGRATOR_CORRECTIONS.md](WAVE19_INTEGRATOR_CORRECTIONS.md)**
(45 entries — §C34 and §C43 are the load-bearing ones) and
**[WAVE19_IO.md](WAVE19_IO.md)** (the plan, whose §0b and §4 this wave corrected).

---

## 1. The number, and why it is only half the story

```
wave start   83 >= 122 - 3 = 119      36 SHORT
wave close  116 >= 133 - 5 = 128      12 SHORT
```

**`called` grew 122 → 133 (+11)** as lanes opened doors. The plan predicted this
and told lanes to report a set-diff rather than a percentage; they did.

**DO NOT read 36 → 12 as "the wave did 24 rows of work."** It is mostly a
**CODE-work** measurement. §2 says why, and it is this wave's most important
finding.

Namespace state at close: **IO 133 called / 116 ported**, ceiling 96.2%.
TOTAL 4632 / 1195 called / 1007 ported (82.6% → **84.3%**).

---

## 2. THE HEADLINE FINDING: the success criterion is blind to half the work

Found by lane CSTREAM, **measured not argued**, confirmed structurally by the
integrator, with a precedent neither had.

CSTREAM registered **12 exports** by name and address. `ported` moved **+3**.

`runtime_census.py` builds its row set by **iterating `called_exports` and
`called_addrs`**. A registration with no `called_*` entry **never becomes a row**.
So `ported` is exactly `|called ∩ registered|`, and **a body that runs by any
mechanism other than a resolve door is invisible to it.** Nine of CSTREAM's twelve
are vtable **slot bodies**, dispatched through a minted vtable — not a call door.

Refined against IOBASE's numbers (which *did* move, because it registered `??_7`
symbols our source references):

> **The identity sees a vtable row being CLAIMED, and is blind to the work that
> makes the claim TRUE.**

**The precedent sits twelve lines from the code that would change.** Wave 10's
`UI::TabElement::UpdateContainers` was *"ported, registered under both doors,
detoured, verified, exercised in a live run... and printed in the 'ports with no
caller' list."* The fix folded detour targets in as called+ported+detoured —
*"the detour is what makes it run."* **Minted vtable slots are the second instance
of the identical root cause.** That makes this a **precedented, concrete Wave 20
fix**, not merely a caveat.

**Not changed mid-wave, deliberately:** every lane measured against the current
definition, and moving the instrument under the measurements would invalidate all
of them.

### Consequence, applied throughout this document

**DATA lanes are reported in bodies closed and trap slots cleared. CODE lanes are
reported on the identity. They are never added together.**

---

## 3. Per lane — on the right metric for each

| lane | kind | result |
|---|---|---|
| **PERSIST** | CODE | **7 rows, identity +7.** 7 verified by synthesised oracle; **3 additionally live-verified** (28/14/13 calls, 0 divergences each on 13 real saves); 2 live-exercised-but-uncompared; 2 not reached live |
| **FILE** | CODE + DATA | **8 code rows + 4 unbriefed closure bodies + 10 slot bodies.** Both DATA rows **TRAP=0 and oracle-verified** |
| **IOBASE** | DATA | **14 bodies, 19 trap slots, 8 rows, 6 rows out of the unported set.** 5 vtable rows promoted behind an 8-of-8 kill-switch control |
| **CSTREAM** | DATA | **14 bodies, 20 trap slots.** All three rows BLOCKED → **READY 9/9 TRAP=0**. **Promote none** — see §5 |
| **MINT** (0b) | tooling | mint tables were **STALE**, not the generator wrong; five rows minted; `--check` added |
| **NOOP** (0a) | analysis | both abstract bases **NO-OP**, settled from shipped bytes; worklist 36 → 34 |
| **TRAPS** (0c) | measurement | CEIL §3.4 re-measured: **16 of 17 rows unchanged** |

---

## 4. WHAT THIS WAVE DOES NOT CLAIM

**The load-bearing section. Read it before quoting anything above.**

1. **THE SCREENSHOT CUTOVER HOLE IS NOT SHUT.** It is narrower by ten bodies and
   **the remainder is enumerated**: the door board stands at **18 of 25 SHUT, 7
   OPEN**, unmoved by the final tranche. Three of the seven open doors are the
   very tables lane FILE cleared — **open for lack of PROMOTION, not for lack of
   ports**. Two are the allocator behind `BANISHED_RT_ALLOC`.
2. **Correction C2 said the debt was six exports, not four. Six was still not
   enough** — a call-graph walk found four more bodies between the named rows and
   Win32. The mechanism no row list could show: **a write goes to `%TEMP%` first
   and is renamed into place on close.**
3. **TRAP=0 IS NOT A PROMOTION, AND READY IS NOT A ROUTE.** Lane FILE's ten bodies
   read **TRAP=0 with attendance ZERO on every one** — the tables are minted but
   **not promoted (5 of 62 vtables -- see §7.5a; "31 of 62" was a CATEGORY ERROR)**, so `RT_DATA_SLOT` falls through to the DLL and
   *retail's* bodies run. **These bodies do not run in play.** Every non-zero count
   came from an oracle.
4. **Only 5 of 62 VTABLES are promoted** (§7.5a corrects the "31 of 62" printed here originally; 31 mixes kinds, 62 is vtables only). **23 complete tables are minted and unregistered.** The census objective and the
   cutover objective **have diverged**, exactly as the plan's §8 warned. This is
   the first hard measurement of that gap.
5. **The CompressedStream writer is not cutover-clean.** `?SetWindow`'s GROW branch
   has the DLL on **both** sides of the oracle — neither blamed nor cleared.
6. **31 `adjacent-export` rows may still be over-sized.** `mint_extent_audit.py`
   reads clean across all 62 rows **and says in its own output that clean is not
   proof** — a table stamped other than by a plain RIP-relative LEA is invisible to
   it. **Narrowed, not closed. Wave 20 must not scope a DATA lane against an
   unverified slot count.**
7. **`??1FileSystem@` is PORTED, NOT VERIFIED**, with a written expiry read off
   the callee (§7.5c). **Row 7 `??0TextFileStream@` is CLOSED** — exercised and
   covered by an enclosing oracle.
8. **`?BeginDelete@` and `??1Persistence@` have no live-route exercise.**
   `??1Persistence@` is verified by synthesised oracle only.
9. **`?SetupFileSystem@Modification@` (~600 instructions) and the PackageFile
   cluster are unstarted**, re-scoped as one real body with three cheap ones behind
   it.
10. **Attendance is one route on most rows.** `post=0` frequently means *not
    driven*, not *unreachable* — proved twice, once as a **harness fault**.

---

## 5. Two criteria this wave established, both reusable

### 5.1 A row is promotable insofar as WE OWN THE FUNCTION THAT STAMPS IT

Every stamp of these vtables is an **in-DLL RIP-relative `LEA`**, not a door. So
TRAP=0 says nothing about who installs the table. Validated by **predicting the
attendance data row by row**, via two instruments gathered by different means.

Lane CSTREAM's stamper census is the sharpest demonstration — the readiness screen
ranks all three of its rows identically at 9/9/TRAP=0:

| row | stampers | ours |
|---|---|---|
| `??_7CompressedStream@` | 9 | 2 |
| `??_7CompressedMemoryStream@` | 7 | 3 |
| `??_7CompressedFileStream@` | 6 | **0** |

**Recommendation accepted: promote none of them.**

**Strongest promotion candidates for Wave 20**, both still unpromoted:
`??_7BinaryFileStream@` (TRAP=0, **both** stampers ours, oracle-verified) and
`??_7VisitorStream@` (7/7/0 **and** ATTENDED via `External::Add` — a proven door
plus a complete slot set).

### 5.2 A bound that DELETES slots must carry a named ctor a human checked

`adjacent-export` proves *"the last slot ends where the next EXPORTED symbol
starts"* — an **upper bound read as exact**. Every concrete IO stream ctor
heap-allocates an 8-byte `IO::Encoding` and stamps an **unexported** vtable
interleaved in `.rdata`.

Two rows were over-sized: **`??_7ReaderWriter@` 36 → 26** (two hidden tables) and
**`??_7TextFileStream@` 20 → 10**.

The rule rests on an **asymmetry**, not on any scan being wrong: *an over-sized row
is a visible surplus; an under-sized one has no symptom.*

---

## 6. Instruments — what changed, and what is still lying

**Fixed this wave:** `abi_retslot_audit` headline now carries its own denominator ·
`rt_mint_gen.py --check` added (the real defect had no detector) ·
`tools/dup_runtime_export.py` written (`dispatch_slot_map --check` is
**structurally blind** to `RegisterRuntimeExport`) · `game_dir_lock.sh` advisory now
**measures handles** instead of asserting from pid presence ·
`tools/mint_extent_audit.py` written.

**STILL BROKEN — Wave 20 must fix:**
* **`tools/boot_state.sh` reports `RUNNING` off a stale state file.** It checks pid
  liveness, never file freshness. **This caused measured harm**: a lane's run
  printed `BOOTSTATE_RC=0` for an already-crashed process, which is the proximate
  cause of a 24-minute outage that starved three lanes.
* **`runtime_census.py` cannot see minted-vtable slot bodies** (§2).
* **14 pre-existing duplicate registrations** in compiled code, including
  `?PlatformDestruct@RenderBuffer@Video@@` — **two distinct bodies under one export
  name**; one is shadowed **dead code the census counts as ported**. Also
  `0x1800e0c80` in Input, same shape, unexamined.

---

## 7. The methodological result, and it outranks the number

Lane PERSIST hit the same reasoning slip **twice within an hour of naming it** —
the second time while quoting the rule:

> **What caught both was not vigilance but A CHECK THAT RAN REGARDLESS OF WHAT I
> BELIEVED.** Neither required me to suspect anything.

**45 corrections were recorded this wave. Almost none were caught by anyone being
careful.** They were caught by a control that had to print non-zero, a set-diff run
because the protocol said so, a **linker refusing a duplicate symbol**, a second
lane re-deriving a number, and tools printing their own subject and denominator.

Three people broke a rule they had personally articulated — two within the hour,
**and the integrator did it four times**. **§7.1 supersedes this section's framing**
with what two lanes worked out between them: *what makes a check able to fail
correctly.* The remedy is not better memory. It is
**cheap unconditional checks whose output cannot be read as success when they have
measured nothing.**

### The integrator's own errors, since they are the seat with the widest blast radius

* Reported Ghidra **down** having probed the wrong port (C3).
* Read my own output as matching a prediction it contradicted (C5).
* Relayed a lane's **negative claim about the environment** — "no save games at
  all" — into a merge commit as a wave-level fact. **13 saves existed.** It cost a
  lane its verification route (C14).
* Quoted `dispatch_slot_map --check` **twice** as proof of no shadowed
  registration; it cannot see that class (C29).
* Passed on **stamper counts** that were wrong — one of which would have let a lane
  declare a row fully owned **with a stamper still in the DLL** (FILE's tranche).
* Passed a lane's marked uncertainty downstream **without the marking** (C8).

**Standing rule extracted:** *a lane's negative claim about the environment is the
most dangerous thing to relay, because it cannot be checked by reading the lane's
work — only by running a command the integrator can always run.*

---

---

## 7.1 ADDENDUM — the converged doctrine, from three rounds of lane-to-lane refutation

**Added after §7 was written.** Lanes CSTREAM and PERSIST worked this out between
themselves, each refuting the other twice. It supersedes §7's looser framing —
*"cheap unconditional checks"* — which is true but does not say **what makes a
check able to fail correctly.** Recorded as an addition rather than a rewrite; §7
stands as written.

### It is TWO axes, and every failure this wave hit is placed by them

PERSIST proposed: *a check either **re-derives** its subject from the artefact, or
**restates** it; a derived subject cannot be aimed wrong, because there is no
aiming step.*

**CSTREAM refuted it with a clean counterexample.** A census set-diff that derived
its subject entirely mechanically was still confidently wrong, because it carried
`--namespace IO` — and **a namespace-filtered listing cannot contain a `System`
symbol whatever its state.** Fully derived, confidently wrong. *(This is also
exactly the integrator's C2 error, found independently by a lane.)*

CSTREAM's replacement — *a check is unaimable exactly insofar as it has **no free
parameter*** — is the more surprising half and it is right. PERSIST then showed it
does not replace derivation; they are **separate axes**:

| | **no free parameter** | **has a free parameter** |
|---|---|---|
| **one side derived** | `static_assert(sizeof(kSpecs)/sizeof(kSpecs[0]) == kSpecCount)` — unaimable, cannot go stale | the two lane scans — cannot go stale, **and both lanes aimed them wrong** |
| **nothing derived** | `"37"` in prose — unaimable, **wrong for two waves** | — |

> **Zero free parameters buys UNAIMABILITY, NOT SOUNDNESS.**
> `static_assert(A == 37)` has no parameter either, and is exactly as stale as the
> comment. **Parameters govern WHERE a check points; derivation governs WHETHER it
> can notice change.** You need both.

**Practical ordering, reached independently by both lanes: remove the parameter;
failing that, PRINT IT BESIDE THE VERDICT.** The `SUBJECT ROOT:` line added to
`dup_runtime_export.py` (C39) is the second-best case — it does not stop the tool
being aimed wrong (PERSIST still aimed it at `AppData\Local`), it makes the aim
**visible next to the verdict**, so a restated subject becomes checkable by a
reader. That is what let PERSIST catch its own mistake.

CSTREAM's `static_assert` is the day's one check that fired correctly on its own
lane: it failed the build with `(41 == 39)` on an addition the lane had not thought
about — **while two prose literals in the SAME FILE, left reading `"37"` by two
earlier lanes, went unchecked for two waves.** Same file, same edit shape. The
assertion's subject was the thing that changed; the prose was nobody's subject.

### Where the mechanism runs out — the expensive category

CSTREAM proposed a defence for claims with no instrument at all: **run one screen
whose subject is STRICTLY WIDER than your claim.** A claim of the form *"these are
the N things"* is refutable by any superset enumerator that does not know the claim
exists — which is how its own four-doors-vs-five banner was refuted within the
hour by a whole-tree census diff.

**PERSIST bounded it tighter, and the bound is the important part.** That defence
requires the claim to be about **membership in a set our tools already enumerate.**
Its `??1Persistence@` holdback was not: it was ***"X is impossible"*** — the scratch
route cannot be built because the dtor joins a real thread.

> **No enumerator refutes an impossibility claim.** There is no superset of "things
> that are possible" for a census to list, and a wide screen running daily for a
> year would never have contradicted it.

What refuted it was **one `objdump`** — and the only reason anyone ran it was that
the integrator asked *which of two mechanisms* the blocker was.

**Scored honestly for the day: the wide-screen defence caught 1 of 3
no-instrument claims.** The two it missed were an impossibility claim and a number
quoted from a stale document.

> **Impossibility claims do not fail loudly. They quietly REMOVE WORK FROM THE
> WAVE.**

The aiming failures are the ones that *got caught*, because a misaimed check
eventually contradicts something. The no-instrument claims sat for **two waves**
(`"37"`), **five waves** (a dark probe), or **one session** (PERSIST's holdback) —
and each surfaced only when a person happened to ask.

**Neither lane could name a mechanism for these beyond the procedural one this
wave already runs** (SUPERSEDED — see §7.4, which names one: decompose the claim
and check its premise):

> **A stated blocker is a claim with a TIMESTAMP. Re-check it before believing it.**

**Five for five stale across Waves 18–19** — and the fifth was a lane's own,
written in the same session as the row it blocked.

### A fifth instance, self-reported

Lane CSTREAM quoted *"`?GetSource@` is the sole trap in FOUR rows"* to lane FILE as
background. It is **six**. The figure came from its tasking, which predated the
mint regeneration. It changes nothing in its results and it reported it anyway —
**a fifth instance in one afternoon of quoting a number about a subject that had
changed since the number was written.**

---

## 7.2 An asset recorded before it is forgotten

Lane PERSIST's `BANISHED_AUTOLOAD=Auto` run is a **clean PRE-CSWRITE baseline on
the load path**, and lane CSTREAM identified it as a usable control for its own
change — then explicitly declined to use it, because its verification is an
artefact oracle that needs no route comparison.

Verified by construction rather than asserted: `wave19/persist` is cut from
`wave19/base @ 20aa588`, and `git merge-base --is-ancestor wave19/cstream HEAD` is
**false**, so **none of CSTREAM's twelve bodies is in the binary that run
measured** — both halves of the save path there are retail's.

```
609 RtDiff sites · 20,060,624 calls · 30 divergences in 7 sites
none in IO::Persistence · exit 0 · 0-byte crash log
docs/runtime_port/wave19_persist/live_autoload_run.log
```

If anyone later needs to isolate `BANISHED_RT_CSWRITE` on the load path: same save,
same route. Either the divergence set moves — the delta is CSTREAM's — or it does
not, which is **a real negative result rather than an absence.**

## 7.3 Integrator decision: lane PERSIST is NOT rebased

PERSIST asked whether to rebase onto `wave19/base`, noting it *"changes the binary
both runs' numbers are tied to."* **Decision: no rebase.**

Its code is already merged and building on the wave tip; a rebase of its worktree
would buy nothing and would strand two archived measurements from the binary that
produced them (`d7ab834b`). **The numbers stay tied to the binary that produced
them, and §7.2's baseline stays valid** — which it would not, if rebased. Lane
closed as-is.

## 7.4 ADDENDUM 2 — impossibility claims ARE reachable: DECOMPOSE them

**§7.1 concluded that neither lane could name a mechanism for impossibility claims
beyond the procedural one. That conclusion is now superseded — by the lane that
helped reach it, after it had closed.**

Lane CSTREAM's addition:

> An impossibility claim is a **conclusion over a premise**, and the premise is
> usually an ordinary **existence claim a wide screen CAN reach.** So decompose it.

It then applied this to the three impossibility-shaped claims in its own **sealed**
§8. Two survived with their premises checked (the `?SetWindow` GROW delegation, and
`?PreCommit@` being one instruction — both read from the source and the raw bytes).
**One did not.**

### The retraction, verified independently by the integrator

CSTREAM's §8 held `?Close@CompressedFileStream` unverifiable because it *"needs a
real file and lane FILE's `?CloseFile`"*. **Both halves of that premise are false on
the merged tip:**

```
?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z
    hits in called-but-unported : 0      (control, a genuinely unported row : 1)
    registered by lane FILE      : filesystem_createfile_win.cpp:634
    lane FILE measures 2854 live calls

??0CompressedFileStream@IO@@QEAA@XZ
    IS AN EXPORT at 0x1800a93b0   (control ??0BinaryFileStream@ : found)
```

So a caller can stack-construct one — which CSTREAM's **own probe already does
three times over** for `CompressedMemoryStream` via retail's ctor.

**And the deeper fault is not staleness.** CSTREAM: *"I named two conditions and did
not ask whether either was actually what stopped a synthesised route."* It asserted
a blocker in a document whose own §5 quotes the doctrine that **"no route" is not
"no oracle"** — the thing Wave 18 refuted four times.

### The transferable rule

> **When you write a blocker, write its PREMISE as a separate, checkable line.**

*"A stated blocker is a claim with a timestamp"* tells you to re-check it and gives
you **nothing to re-check**. The premise is the thing a screen can reach. CSTREAM:
*"Mine was checkable and I wrote it as a conclusion, so nothing could reach it —
including me, twice."*

**This upgrades §7.1's score.** Of 3 no-instrument claims, a wide screen reached 1
directly; decomposition reaches a second. The residue is claims whose premises are
themselves unenumerable — a smaller category than §7.1 concluded.

### Integrator decision: NOT reopened; it is a Wave 20 line

CSTREAM offered to build the probe and recommended against reopening. **Agreed, and
its reasoning is right**: the lane is sealed, the wave is closed, and a fourth probe
subject would land unreviewed. **It changes no measured number — it retracts a
stated limitation**, which is the cheaper and more honest deliverable.

The work is specified precisely enough to be cheap for whoever takes it:
`?Close@CompressedFileStream` (`0x1800a9490`) is **nine instructions** — `if
(headerSize>0 && direction==1) Commit(this)`, then `if (*(int*)(this+0x88) != -1)`
tail-jump `CloseFile(_singleton, this+0x88)`. A probe stack-constructs a
`CompressedFileStream` through retail's exported ctor, `Create`s it onto a temp file
via lane FILE's now-ported `?Create@FileStream@`, writes, `Close`s, and **compares
the FILE ON DISK** against retail's — the same artefact oracle CSTREAM already runs,
one subject over. Lane FILE's finding that a `%TEMP%` file is **renamed into place**
on close makes the on-disk comparison the right subject: nothing appears at the
caller's path until `Close` runs.

## 7.5 ADDENDUM 3 — "31 of 62 promoted" WAS WRONG, and a NO-OP row is the SAFEST promotion

Two corrections from lane FILE's final tranche, landed after the close was written.
**Both are load-bearing: the first is quoted four times above and in the Wave 20
guide; the second inverts a decision this wave made twice.**

### 7.5a The promotion figure is a CATEGORY ERROR — it is 5 of 62 VTABLES

`[RtMint] promoted 31 minted DATA row(s)` **carries no denominator and does not name
its kinds.** Lane FILE paired it with the `62` on the adjacent minted-vtables line,
and so did I — in §4 items 3 and 4, and in `WAVE20_GUIDE.md` §2, where it is the
headline of the recommended subject.

**`n` counts a MIX of kinds.** Confirmed structurally by the integrator:
`reg_mint_promoted.cpp` increments the same `n` for storage rows
(`RegisterRuntimeAddr(0x1801fa9a8ull, st); ++n;` and its neighbours), for typeinfo
rows, and for vtables. **62 is a vtable-only count.** Pairing them compares different
populations.

Lane FILE's count from the file: **5 vtables, 25 typeInfo, ~1 storage = 31.**

> **The right figure is 5 OF 62 VTABLES PROMOTED, not 31 of 62.**

**The corrected number is STRONGER, not weaker.** The same run prints
**"28 of 62 vtables are COMPLETE"** — so **23 complete tables are minted and
unregistered.** The promotion opportunity Wave 20 is being pointed at is roughly
four times what §4 implied, not half.

This is the same instrument defect the wave catalogued repeatedly — a green printed
without its denominator or its kinds — and it took a lane that had been *asked to
diagnose that defect in others* to notice it had fallen into it.

### 7.5b A NO-OP row is the SAFEST promotion, not one to skip

§4 and lane IOBASE both concluded that `??_7Stream@IO@@6B@` should stay unpromoted
because it is a proven NO-OP. **Lane FILE answered the open question with the
mechanism rather than an inference, and the mechanism inverts the conclusion.**

Eight sites in six files resolve that symbol, and **every one is a WRITE of the
address into an object vptr — none dispatches.** So:

* **A NO-OP row can be permanently unpromotable AND a live cutover door**, because
  what cutover needs is *the address to stamp*, not our slot pointers. That confirms
  the reading I offered lane FILE earlier and had not proved.
* **And the inversion:** promotion is risky *only* because a wrong slot might be
  dispatched. For a proven NO-OP row **nothing is dispatched**, so there is no risk
  to weigh — **a NO-OP row is the SAFEST promotion available, not one to skip.**

**This reverses guidance I gave twice** (to lane CSTREAM and in §4), and it was
reached by reading what the eight sites *do* rather than by reasoning about what
NO-OP implies. Wave 20 should promote `??_7Stream@` and `??_7ReaderWriter@` **first**,
as the lowest-risk items on the board, before the candidates in §5.1.

### 7.5c Row 7 is CLOSED — and the harness was in the tree

§4 item 7 named `??0TextFileStream@` and `??1FileSystem@` as PORTED, NOT VERIFIED.
**Row 7 is now closed:**

```
[SaveTextProbe] VERDICT ctl=0 checked=13 failed=0 bytes=42 serCalls=1/1 PASS
[FileCutover]   attendance ??0TextFileStream=1
```

Lane FILE states its standing precisely — **"PORTED, EXERCISED, COVERED BY AN
ENCLOSING ORACLE"**, because it is *not* a dedicated oracle on the constructor. That
is the right tier discipline again.

**And the harness already existed**: `external_savetext_probe.cpp`, armed by
`BANISHED_RT_SAVETEXT_PROBE=1` from `external_add.cpp:277`, with three negative
controls. Lane FILE: *"the save-text harness existed in the tree and I had not
looked."* **That is the fourth time in two waves** that the rule *grep `standalone/`
and the `BANISHED_*` list before declaring no route* has paid out — and this time it
also **reduces the Wave 20 SaveText item** (`WAVE20_GUIDE.md` §3), which assumed a
harness had to be built.

**Row 8 (`??1FileSystem@`) gets an expiry read off the CALLEE, not off a failed
search** — the form §7.4 asks for. Its sole caller is `RtEngine_Engine_Shutdown`, and
`engine_lifecycle.cpp:183` already records with evidence that its `calls=` is
**structurally zero**: `game_main.cpp` resolves `Engine::Shutdown` only to satisfy a
null guard and calls its own `ShutdownEngineStaged()` instead. So the blocker is that
**its only caller is deliberately never called by this project's design** — a request
to another lane, not a waiting game.

### 7.5d Lane PERSIST decomposed its own impossibility claims — 2 of 2 FALSE

A second independent confirmation of §7.4, from the lane that originally argued no
mechanism could reach such claims. **Decomposition has now reached every premise it
has been pointed at.**

## 8. Wave 20 inputs

1. **Fix `runtime_census.py`** to fold minted-vtable slot bodies in, on the Wave 10
   detour precedent (§2).
2. **Fix `boot_state.sh`** to check state-file mtime (§6).
3. **Promote `??_7BinaryFileStream@` and `??_7VisitorStream@`** (§5.1).
4. **Close the extent risk** on the 31 remaining `adjacent-export` rows (§5.2).
5. **`?SaveText@External@Object@@` is the highest-leverage single item left** — it
   unlocks lane FILE's row 7, the two IOBASE vtables SaveText stamps, and the
   handed-back `??_7TextFileStream@` (10 slots / 8 bodies).
6. **A save/load route cell** — "open the dialog and produce one save", not
   "produce one save". Its `Save\` directory is created by `?CreatePath@FileSystem@`,
   lane FILE's row, so it shares code with the screenshot debt.
7. **`?SetupFileSystem@Modification@`** — a lane on its own.
8. **The 14 duplicate registrations**, starting with the two cross-file ones (§6).
9. **The cutover boot wall is `?PlatformConfigure@Engine@1@`** — an ENGINE row,
   neither IO nor Graphics. Engine sits at 33 called / 23 ported.
