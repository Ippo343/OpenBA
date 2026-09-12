# WAVE 29 LANE MINT — regenerating `standalone/rt_data_mint_tables.inc`

Branch `wave29/mint`, cut from `wave29/base` at `c11511a1`.
Regeneration commit: **`70c43851`**, and it contains that one file and nothing else.

**Headline: the regeneration landed, it is purely additive, it changes no
behaviour that is not explicitly armed, and it moves the cutover wall in neither
direction.** The board does not move and could not have — see §7.

---

## 0. WHAT THIS DOES NOT CLAIM

1. **It does not claim the 68 new tables are CORRECT, only that they are what the
   generator derives and that they are INERT unless armed.** Their slot targets
   are re-read from the DLL by the generator and cross-checked against the Wave 9
   harvest (253 rows, 0 disagreements), but I ported nothing and verified no
   body. A minted row records what the DLL's slot holds; it does not make it ours.
2. **It does not claim the 68 rows are PROMOTABLE.** None was promoted, and
   promotability turns on owning the function that STAMPS the table, which no
   source scan and no run of mine can answer.
3. **It does not claim the slot COUNTS of the new rows are exact.** 68 of the 132
   rows are size-proved by adjacent-export, which is an **UPPER BOUND** on
   identity and extent, not an exact measure — a row can swallow an unexported
   table. 62 are proved by tail probe. I did not independently re-derive any
   extent; I inherited the generator's proof and am labelling it rather than
   dressing it as exact.
4. **The cutover A/B says the wall did not move. It does NOT say the 68 rows are
   harmless at cutover in general.** It says that on this arm, on this tip, boot
   reached the same instruction. A row that would matter only after the current
   wall is invisible to this test, and the current wall is early.
5. **It does not claim the ordinary-path run exercised the new tables.** With
   `BANISHED_RT_MINT` off — the ordinary arm — `RegisterMintedData()` does not
   run at all, so on that run the 68 rows were laid out and bound nothing. The
   DLL-present run proves NON-REGRESSION, not exercise.
6. **It does not claim anything about attendance.** No new row was executed by a
   live route in any run I made, and I did not try to arrange one.
6a. **"60 of 132 COMPLETE" is NOT a readiness claim.** COMPLETE means every
   non-purecall slot is registered. Lane WALL demonstrated on `SpriteSheet` — a
   9/9 COMPLETE row inside that 60 — that a ported slot body can still call
   unported internals **by address** and kill the cutover boot. **A complete
   vtable is not a bootable one** (§6).
7. **My board figures are from MY worktree's tools at MY tip.** Lane INSTR changed
   `tools/mint_slot_sweep.py` and moved the published board to 1460/1295 88.7%
   mid-wave. My before/after were taken with the same tool at the same tip,
   swapping only the one file, so the DELTA is valid; my ABSOLUTE numbers are
   pre-INSTR and must not be mixed with post-INSTR ones.

---

## 1. RE-DERIVED, AND WHAT REPRODUCED

Every figure handed to me reproduced exactly on this tip.

| handed to me | command | my result |
|---|---|---|
| `STALE: 68 ENTERED, 0 LEFT` | `python tools/rt_mint_gen.py --check` | **reproduced exactly** |
| `TOTAL 4632 1453 1294 89.1%` | `python tools/runtime_census.py` | **reproduced exactly** |
| `OK (59 rows)` | `python tools/cutover_manifest.py --gate` | **reproduced exactly** |
| `vtables 64 ; slots 589 ; ported 430 ; purecall 143 ; 16 unported` | `python tools/mint_slot_sweep.py` | **reproduced exactly** |

Nothing in my brief was stale. That is worth recording precisely because warning 3
says to expect the opposite: the *measured* sections of this kickoff held, and the
one thing that did not exist was an *evidence pointer* (§5.1).

---

## 2. WHAT ACTUALLY CHANGES — DECOMPOSED BY ROW CLASS

`--check` reports ENTERED/LEFT through **one regex, pinned by what follows the
VA**. A row class whose emitted text does not match that regex is invisible to it
in both directions. So I classified by the enclosing table declaration instead —
a screen that cannot share that premise — and diffed the FILE.

```
kMintVtables     64 -> 132    +68 entered, 0 left
kMintTypeInfos  104 -> 104    IDENTICAL
kMintStorage     39 ->  39    IDENTICAL
```

**All 68 entered rows are `??_7` vtable rows.** Nothing else in the file changes.

### 2.1 The generator's stdout reads like 49 more rows and is not

`rt_mint_gen.py` prints, on every run:

```
vtable closure: +45 BaseTypeInfo pulled in by a minted vtable
chain closure:  +4  BaseTypeInfo that exist in the DLL's chain but nothing names
```

That looks like 49 new BaseTypeInfo rows entering. **It is not a delta at all** —
it describes how THIS run derived its set, and all 104 typeinfo rows were already
in the committed file. Reading those two lines as a change is the obvious mistake
and only diffing the file refutes it. Recorded because the lines are printed
unconditionally and the next lane will see them too.

### 2.2 The 59 "changed" rows are index renumbering, proved with a control

A textual diff calls 59 surviving `kMintVtables` rows "changed". They differ only
in their `kVSlotsN`/`kVTableN`/`kVTrapsN` **array index**, which renumbers when a
row is inserted. To separate that from a real content change I wrote a screen that
keys on each vtable's own **name/VA** rather than the array index, and resolves
through to the slot list:

```
RESULT  0 of 64 surviving rows changed in slot count, sizeProof or slot targets
        68 rows present only in AFTER, 0 only in BEFORE
```

Its normal answer is empty, so per warning 7 it was **handed a known positive** —
one slot VA corrupted to `0xdeadbeef`:

```
CONTROL  1 of 64 surviving rows changed    <- screen is live, not a dead parser
         CHANGED 0x180154740 ??_7Achievement@0@6B@ slot[0] ... -> 0xdeadbeef
```

**The regeneration is purely additive.**

---

## 3. IS ANY NEW ROW UNSAFE TO MINT? — NO, AND HERE IS THE ARGUMENT

My brief said a partial regeneration with a stated exclusion list beats a complete
one I cannot defend. **The exclusion list is empty, and this is why.**

Adding a row to `kMintVtables` is **layout only**: `MintVtables()`
(`standalone/rt_data_mint.cpp:351`) writes into its own `static void* kVTableN[]`
arrays and publishes nothing into the resolve registry. A new row can therefore
go live **without anybody arming anything** if and only if some *unconditional*
publisher names it. There are exactly two:

- `RtMintRegisterPromotedRows()` — `standalone/reg_mint_promoted.cpp`, which runs
  precisely when `BANISHED_RT_MINT` is **off**, i.e. on every ordinary run
  (`rt_data_mint.cpp:1068`);
- the `CUTOVER_REQUIRED` rows of `standalone/cutover_manifest.inc`.

A name in either that matched no row before was a silent no-op and would bind the
moment the row appeared. Cross-referenced mechanically:

```
SUBJECT    ENTERED rows: 68
POPULATION reg_mint_promoted.cpp names 55 symbols; cutover_manifest.inc names 59
RESULT     0 of 68 newly-entered rows are named by an unconditional publisher
CONTROL    1 of 69 when a real promoted-list name is injected into the set
```

**Scope of that control, stated because it is narrower than it looks** (and my
partner narrowed it further, §6): it is a control on **binding on the
DLL-present path**. At cutover the arm includes `BANISHED_RT_MINT=1`, and under
that switch *every* row binds by construction, so this control says nothing
there. What covers the cutover case is the measured A/B in §5, not this screen.

---

## 4. MEASUREMENT — DLL-PRESENT PATH

Build gate (§0.2), foreground, exit 0, no source newer than the image:

```
mingw32-make standalone-debug                 exit 0
find ... -newer standalone/build/reimpl-x64.exe   EMPTY
strings reimpl-x64.exe | grep -c '??_7ComboDialog@UI@@6B@'   3
nm      reimpl-x64.exe | grep -c 'RtMintTrapStub_1[23][0-9]_' 105
nm      reimpl-x64.exe | grep 'RtMintTrapStub_131_'           present
```

`RtMintTrapStub_131_*` cannot exist unless a 132nd row does, so the work is in the
LINKED IMAGE, not merely in the source.

**Functional run, DLL present, `BANISHED_AUTOSTART=1`, 200 s:**

```
[RtMint] minted 132 vtables (1312 slots: 614 ported, 553 unported-trap,
         145 purecall-trap), 104 BaseTypeInfo, 39 storage symbols (11344 bytes)
[RtMint] 60 of 132 vtables are COMPLETE; registration is off
[CutoverManifest] DLL present -- 59 tagged rows held
[AutoStart] ENABLED -> menu settled -> New Game click -> buttonOk -> sequence fired
standalone_crash.log  0 bytes
BootState             RUNNING, uptime 199 s
```

and the debug log's tail is in-world component drawing (`StorageDisplayComp::Draw`,
a placed storage building). **The ordinary path is not regressed.**

### 4.1 My first attempt at this run was a false pass, and I caught it

I first ran with `BANISHED_AUTOQUIT=1` "to get a clean exit". It exited **0**, with
a **zero-byte crash log**, and `BootState=RUNNING` — it looked like a clean pass.
It had **0 `[AutoStart]` lines**: `AUTOQUIT` overrides autostart's New Game path
and quits at the settled menu, so the run never built a world and never touched
the in-game path I was claiming to test. This is a known trap in this project's
ledger and I walked into it anyway; what caught it was checking for POSITIVE
evidence of the thing I claimed (a world) instead of the ABSENCE of failure. An
exit code and an empty crash log cannot distinguish "worked" from "never tried".

---

## 5. MEASUREMENT — CUTOVER A/B

**Arm (four variables), and how I got it wrong first:**

```
BANISHED_RT_NO_DLL=1  BANISHED_RT_ALLOC=1  BANISHED_RT_MINT=1  BANISHED_AUTOSTART=1
BANISHED_RT_DIFF unset      (verified: grep -c "RtDiff] ON" == 0 on both runs)
```

I reconstructed a **two-variable** arm (`NO_DLL` + `ALLOC`) and it was wrong.
Lane WALL corrected it with the attempt-31 artefacts. I did not take the
correction on their evidence alone — two checks sharing one premise are one check
— so I verified the source side independently:

```
rt_data_mint.cpp:1096   EnvOn("BANISHED_RT_MINT") ? "ON (BANISHED_RT_MINT=1)"
                                                  : "off (set BANISHED_RT_MINT=1 ...)"
rt_data_mint.cpp:1062   if (EnvOn("BANISHED_RT_MINT")) RegisterMintedData();
```

The printed string sits on a ternary over the variable, so a log carrying the ON
form cannot exist without it. **And line 1062 is the only thing that publishes the
minted rows.** On my two-variable arm `RegisterMintedData()` never runs, so all
132 rows — my 68 included — would have bound nothing, and my A/B would have
compared two trees with the mint switched off and reported "no difference" about
a subject that was never armed. **That is a DEFLATING null result and nothing
would have detected it.** My own error; see §8.

**Both runs, same arm, same lock hold, `tools/cutover_attempt30.sh`:**

| | CONTROL | TEST |
|---|---|---|
| exe md5 | `081afbec…` | `0529bd9c…` |
| minted vtables | 64 | **132** |
| `RtMintTrapStub_131_*` in image | 0 | 7 |
| attest | `NO_DLL`, `ATTEST: ARMED (exports=27)`, `registration is ON` | same |
| `RtDiff] ON` | 0 | 0 |
| fault | EXEC of 0, `RAX=0 RDX=0x225 RBX=8` | **identical** |
| frames | `+0x3D7CAA / +0x36B463 / +0x36A2C4` | **identical** |

**I did not compare the offsets and stop there.** The two exes are different
images — a different `.inc` means different static arrays and a different layout —
so equal offsets are a *coincidence claim* until symbolised. I resolved each
offset in **its own image** with `nm`:

```
CONTROL  +0x3D7CAA -> RtUi_SpriteSheet_Serialize + 0x1b7
         +0x36B463 -> RtObject_External_Load + 0x592
         +0x36A2C4 -> RtObject_ExternalFactory_FindOrLoad + 0x2b1
TEST     identical, all three
```

That is WAVE28_CLOSED §2's published attempt-31 wall, reproduced on **both** sides.

**RESULT: registering 68 additional vtables at cutover, fully armed, with 553
unported slots minted as named traps, introduced no new earlier fault and cleared
nothing. The wall is unchanged.** Evidence preserved as `.txt` (not `.log` — see
§5.1) in `docs/runtime_port/wave29_mint/`.

### 5.1 The kickoff's own derivation route does not exist

§0.4 says to reconstruct the arm from "attempt 31's `[RtAlloc] ATTEST:` /
`[RtDiff] ON` lines". **Those logs are not in the repo.** `docs/runtime_port/`
contains no attempt-31 artefact directory; the newest committed cutover logs are
`wave26_attend/` and `wave24_cutover/attempt25*/`. Cause (found by WALL): `*.log`
is in `.gitignore`, so `cutover_attempt30.sh`'s copy step wrote the logs into the
outdir and git dropped them silently. **DEFLATING** — it makes the evidence look
absent and pushes the next lane onto inference, which is exactly what it did to
me. I am writing my own evidence as `.txt`.

**Addressee for the expiry of this item:** the Wave 30 integrator, when writing
the next kickoff's §0.4 — do not cite attempt-31 logs as the derivation route
unless `ls docs/runtime_port/cutover31/*.txt` is non-empty.

### 5.2 Lock wait, as required

Queued behind WALL per §0.3. **Total wait ≈ 16 minutes** across two acquires:
one 540 s acquire that ended `DENIED — TIMEOUT` (VERIFY then IOLANE holding), and
a second ~400 s acquire that was **GRANTED**. The lock tool's own queue printed
`queued behind 'WALL'` throughout, so WALL's priority was enforced by the tool and
not only by my restraint. I never held it during either wait — verified with
`status`, not assumed. Released explicitly (`[gamelock] released by MINT`).

---

## 6. PARTNER — WHAT WALL FOUND IN MY WORK, AND WHAT I FOUND IN THEIRS

**What WALL found in mine (the important direction):**

- **My cutover arm was short by two variables.** Decisive, not cosmetic: without
  `BANISHED_RT_MINT=1` my lane's only real measurement would have been a silent
  null. WALL also sharpened my §3 control correctly — it is a control on
  **binding**, not on **arming** — and I have narrowed it further in §3 myself as
  a result.
- WALL independently reproduced my §5.1 finding and my build-non-reproducibility
  finding, and confirmed my "BANISHED_RT_CUTOVER_MANIFEST is a no-op at cutover"
  reading from the source.

**What I found in WALL's (reported including the null, per §2.1):**

- **`tools/addr_door_audit.py`'s blind spot is TOTAL over my subject, not
  partial.** WALL reports it as "40 unregistered doors is a FLOOR" because an
  address passed to a wrapper rather than spelled at a literal
  `ResolveAddr(Module::Runtime, 0x…)` is invisible. Applied to the mint: **the
  mint reaches none of its slot targets through a literal call.** Every one of the
  1312 slot targets is data in `kMintVtables`, resolved through `OurPort(s)`. So
  that audit would report **0** doors for the file that describes the largest
  data-driven door table in the tree. The sharper statement of the defect is not
  "40 is a floor" but "it cannot see a data-driven door at all".
- **I checked WALL's wall against my commit rather than asserting independence.**
  `0x180078ad0` (attempt 31's null) appears in **neither** the old nor the new
  `.inc`; `??_7SpriteSheet@UI@@6B@`'s row is unchanged but for its array index;
  and `0x180061100` (WALL's next wall) is likewise in neither. Structurally, my 68
  rows are `.rdata` vtables in `0x180154800`–`0x1801592c0` while the ExternalPtr
  family is `.text` at `0x18006xxxx` — **they cannot collide by construction**,
  which covers all 19 of WALL's unregistered addresses rather than the one I
  checked by name. **WALL narrowed this and the narrowing is right, so it stands
  here too: section disjointness rules out an ADDRESS collision. It does not rule
  out my rows changing which BODIES GET REACHED, and neither of us tested that.**
- **WALL ran `mint_slot_sweep.py` against their subject and the null was the
  finding — and it qualifies a number in my own §7.** `??_7SpriteSheet@UI@@6B@`
  scores **9/9 COMPLETE** and sits inside my "60 of 132 COMPLETE", a green row,
  while one of those nine ported bodies calls unported internals **by address**
  and takes the cutover boot down. `grep -c ResolveAddr tools/mint_slot_sweep.py`
  is **0**. So **a COMPLETE vtable is not a bootable one**, and my COMPLETE counts
  must not be read as readiness. Between the two findings the hole is bounded from
  both sides: **the sweep cannot see address doors at all, the audit cannot see
  data-driven doors at all, and the census sees neither.** The cutover wall has
  lived in that gap twice.
- A defect of WALL's I could not check: their claim rests on attempt-31 logs held
  only in their private scratch. I verified the *source-side* inference instead
  (§5). **The artefact itself I have not seen and am not vouching for.**

---

## 7. THE BOARD DID NOT MOVE, AND COULD NOT HAVE

Measured by swapping only this one file on one tip, same tool both sides:

```
runtime_census.py --csv     1453 rows  -- BYTE-IDENTICAL (diff exit 0)
TOTAL       4632 1453 1294  89.1%      -- unchanged
cutover_manifest.py --gate  OK (59 rows) -- both sides
```

**A flat board is the correct expectation, not a shortfall.** A minted vtable row
is a data descriptor, not a resolve door; it adds nothing to the census's
population. What *does* move is the visibility of work that already existed:

```
MINTED DATA slots        207 -> 275    (+68; 203 -> 271 named, 4 by-address)
PROMOTION BACKLOG        191 bodies / 42 tables -> 366 / 104,  COMPLETE still 38
registered only under
BANISHED_RT_MINT=1       155 -> 223    (+68)
mint_slot_sweep UNPORTED
  -> named trap           16 -> 562    (494 by name, 68 address-only)
```

**The 175 extra backlog bodies were already written and already registered.** They
were invisible because their table had no minted row. Nothing got worse, and
reading `16 -> 562` as 546 new defects would be exactly wrong.

### 7.1 A delta that moved by the wrong amount, chased rather than rounded

The backlog prose moved `42 -> 104` tables (+62) while **68** rows entered. A
count moving the right way by the wrong amount has hidden a phantom in this
project before, so I reconciled it instead of accepting it:

```
of the 68 ENTERED rows, 68 appear in the AFTER backlog TABLE, 0 do not
backlog TABLE rows: 42 -> 110   (+68 exactly)
pre-existing backlog rows whose (ours,slots,complete) CHANGED: 0
bodies contributed by entered rows: 175  == the census's own 366-191 delta
```

**The census's backlog prose says 104 while the table it introduces has 110 rows.**
Both are computed, and differently: the prose is `len(backlog)` (vtables with ≥1
registered body) and the table iterates `set(backlog) | set(backlog_slots)`
(`runtime_census.py:3682` and `:3691`). The 6-row gap is exactly the six newly
minted tables with **zero** registered bodies:

```
??_7AnimationGroup@Graphics@@6B@   ??_7CollisionMesh@Collision2D@@6B@
??_7Font@UI@@6B@                   ??_7PickingMesh@Picking@@6B@
??_7PointList@Pathfinding@@6B@     ??_7SoundEffect@Audio@@6B@
```

Before my commit the two numbers agreed (42 and 42), so this only became visible
now. **Direction: DEFLATING** — the sentence undercounts the table it labels.
**Addressee: lane INSTR**, who owns the census this wave; it is a one-line fix
(print the union's length, or say "with ≥1 registered body").

---

## 8. INSTRUMENT DEFECTS AND MY OWN ERRORS

**Defects, with direction (deflating outranks inflating):**

1. **A backgrounded lock acquire launders its exit status into a false GRANT.**
   *(INFLATING a grant — the dangerous direction here.)* I ran the acquire in the
   foreground, unpiped, exactly per §0.3. **The Bash tool auto-backgrounds at
   600 s**; the completion notification then read *"completed (exit code 0)"*
   while the real result in the output file was `[gamelock] DENIED — TIMEOUT` and
   `ACQUIRE_EXIT=1`. The 0 was my trailing `date`. **Piping is only one instance
   of the documented rule: ANY trailing command in the same compound destroys the
   acquire's status**, and the notification then launders it. Had I trusted it I
   would have deployed on top of IOLANE and both our measurements would have been
   of the wrong binary. Mitigation: make the acquire the **last** command, keep
   timeouts ≤ 500 s, and `assert-held` immediately before every launch. WALL hit
   the identical defect in the same wave — **two independent hits, so it is a
   wave-level item, not a lane anecdote.**
2. **§0.4's stated derivation route does not exist** (§5.1). **DEFLATING.**
   Addressee named there.
3. **The census's backlog prose undercounts its own table** (§7.1). **DEFLATING.**
   Addressee: lane INSTR.
4. **`tools/mint_slot_sweep.py` reads `git show HEAD:`, never the working tree.**
   Documented in its usage line, but in my situation — measuring an *uncommitted*
   regeneration — it silently returned the pre-regeneration answer (64/589/16)
   and named no subject that would reveal it. **DEFLATING**: it reports "no
   change" for a change that is real. Only after committing did it print
   132/1312/562. Any lane measuring uncommitted work with it gets a confident
   wrong answer.
5. **The build is not byte-reproducible.** Rebuilding from *identical* committed
   source gave `3ed328e6…` where an earlier build gave `0529bd9c…`. Consequence
   for the lock tool: `assert-held <lane> <exe>` compares md5s, so it is sound for
   *"is the deployed file the file I just built"* and **unsound** for *"is the
   deployed file built from my source"*. I used it only in the first sense, and
   built each A/B exe once and never rebuilt it. Independently hit by WALL.
6. **A mangled-name `strings` grep is not a valid A/B discriminator.** My first
   attempt to prove the two exes differed used
   `strings | grep '??_7ComboDialog@UI@@6B@'` and the CONTROL exe returned **1** —
   the name is present in the tree independently of the mint. *(INFLATING a
   false match.)* The valid discriminator is `nm | grep -c RtMintTrapStub_131_`
   (0 vs 7), because that symbol cannot exist unless the row does.

**My own errors:**

- **I built a two-variable cutover arm and would have measured an unarmed mint**
  (§5). I had read `rt_data_mint.cpp:1062` — the exact line that refutes it —
  earlier in the lane, and did not apply it. Having the fact is not using it.
- **I nearly certified the ordinary path on a run that never built a world**
  (§4.1), because I scored it on exit code and an empty crash log.
- **I claimed to WALL that I had not checked whether my rows collide with theirs,
  in the same message that flagged the risk.** I then checked it immediately. The
  check took one grep; flagging a cheap question to a partner instead of
  answering it is a small version of the thing this wave keeps punishing.
- **My worktree was checked out at `1afa558e`, not the wave base.** I noticed
  before branching and cut `wave29/mint` from `wave29/base` explicitly. Had I run
  `git checkout -b wave29/mint` as literally instructed, I would have branched a
  stale tree and every figure in this document would have been wrong.

---

## 9. STILL OPEN, WITH THE OBSTACLE CLASS NAMED

- **The 68 new tables have never been executed.** Obstacle class:
  **missing-capability — EXPIRES.** They bind only under `BANISHED_RT_MINT=1`,
  and the cutover boot faults before most of them could be reached. It needs a
  wall further in, not new tooling. Feeds the ~280-body verification debt
  (lane VERIFY).
- **68 of the 132 rows' extents rest on an adjacent-export UPPER BOUND** (§0.3).
  Obstacle class: **intrinsic to that proof method — DOES NOT EXPIRE** without a
  different instrument. Deriving the true extents is lane SLOTS' kind of work.
- **The 6 zero-body tables** (§7.1) are minted and have no registered body at all.
  Not a defect; just the honest state, and they are the cheapest possible
  `mint_slot_sweep` targets if anyone wants whole tables rather than slots.
- **`rt_mint_gen.py` remains a cache with no invalidation.** This file goes stale
  again the moment any lane adds a reference to an exported DATA symbol, and
  nothing fails when it does — a missing row does not fail to compile, does not
  trip the shadow, and shows up as a cutover fault one row at a time.
  **Addressee: the Wave 29 integrator** — run `python tools/rt_mint_gen.py --check`
  at wave close, after all merges, and expect it to be STALE again.
