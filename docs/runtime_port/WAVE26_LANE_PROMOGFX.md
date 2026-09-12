# WAVE 26 — LANE PROMOGFX

Branch `wave26/promogfx`, cut from `wave26/base` at `7de42dd1`.
Scope: the **Graphics / Engine / Achievement / Audio** half of the promotion
backlog, plus those namespaces' BODY and DATA gap rows. (IO / Video / System
are lane PROMOIO's and were not touched.)

Everything below is split into **MEASURED** and **INFERRED**. Where a number is
quoted it carries the command that produced it.

---

## 0. THE ONE-PARAGRAPH VERSION

The lane's largest result is **not** a promotion. Two instruments disagreed by
five slots about the row this lane had been handed, **nothing in the tree
compared them**, and the cause was a single 0x00 byte in a source comment that
deleted an entire file from every content-based `git grep` in the repo. Fixing
it moved `tools/mint_slot_sweep.py` from *112 unported slots* to *83* — 29 rows
of port work that did not exist. Two vtables were then promoted
(`??_7Geometry@Graphics@@6B@`, `??_7AnimationEvent@Graphics@@6B@`), each with
the stamper question answered first; a third that looked like the obvious cheap
win (`??_7UpdateLoop@Engine@@6B@`, 7/7 COMPLETE) is **held back** with a written
condition and an addressee. Two `?_typeInfo@` rows ATTEND measured ATTENDED were
promoted; the four it measured `never` are recorded ROUTE-UNDECIDED rather than
promoted or dropped. Board: **1333/1185 88.9% -> 1337/1191 89.1%**, Achievement
gap 9 -> 7 — and see §8 for why that number understates and §5 for why.

---

## 1. BOARD, RE-DERIVED (MEASURED)

`tools/runtime_census.py` run from this worktree, on `wave26/base`:

```
TOTAL   1333 called / 1185 ported / 88.9%      gap 148
  BODY 15 · VTABLE 54 · DATA 46 · UNKNOWN 33
```

Identical to the kickoff's figures. Per-namespace gap in this lane's scope,
re-derived with `--namespace <ns> --unported`:

| namespace | gap | BODY | VTABLE | DATA | of which tagged unpromotable |
|---|---|---|---|---|---|
| Graphics | 18 | 4 | 7 | 7 | 5 |
| Achievement | 9 | 1 | 4 | 4 | 1 |
| Engine | 8 | 1 | 4 | 3 | 3 |
| Audio | 4 | 1 | 1 | 2 | 2 |

**The `gap` number is not a work list.** Of Graphics' 18, five carry a written
HARMFUL tag (`?AddDrawItems@MeshInstance@`, `?_frameIndex@ParticleInstance@`,
`?_rng@Particle@`, `?_rootOnlyUpdates@MeshInstance@`,
`?_singleton@AnimationManager@`). Of Engine's 8, three do. Of Audio's 4, two do
— so Audio's *actionable* gap is **two rows**, not four.

### 1.1 Per-table slot status, re-derived from `rt_data_mint_tables.inc`

Independent of the census (own scan, `git grep -a` channel, comments not
stripped on the registration side because `RegisterRuntime*` in a comment is
vanishingly rare and was checked by hand for the four rows acted on):

```
??_7AnimationEvent@Graphics@@6B@    4/4   slots=9    COMPLETE
??_7Geometry@Graphics@@6B@          6/6   slots=9    COMPLETE
??_7UpdateLoop@Engine@@6B@          7/7   slots=12   COMPLETE
??_7Light@Graphics@@6B@             5/6   slots=11   missing slot0 (unnamed dtor) 0x180058dd0
??_7Particle@Graphics@@6B@          5/6   slots=9    missing slot5 ?Serialize@Particle@Graphics@@
??_7Stream@Audio@@6B@               1/2   slots=2    missing slot0 ?Run@Stream@Audio@@
??_7Loader@Engine@@6B@              1/2   slots=2    missing slot0 ?Run@Loader@Engine@@
??_7GoalInstance@Achievement@@6B@   3/7   slots=7
??_7Goal@Achievement@@6B@           4/7   slots=10
??_7Achievement@0@6B@               4/9   slots=9
??_7AchievementInstance@Achievement@@6B@ 1/4 slots=4
??_7DialogLoopT@Engine@@6B@        10/12  slots=13
??_7DialogLoaderLoopT@Engine@@6B@   8/12  slots=13
??_7LoaderLoop@Engine@@6B@          6/12  slots=12
??_7ParticleInstance@Graphics@@6B@  5/9   slots=10
??_7ParticleSystemInstance@Graphics@@6B@ 5/10 slots=10
??_7EdgeMeshInstance@Graphics@@6B@  4/9   slots=9
??_7SunLight@Graphics@@6B@          5/11  slots=11
??_7MeshInstance@Graphics@@6B@      2/9   slots=9
??_7EdgeMesh@Graphics@@6B@          0/9   slots=9
```

The `N/M` the census prints is **non-purecall slots**, not table extent. Light
reads `5/6` and is an ELEVEN-slot table with five purecalls. A lane sizing work
from the census column will under-count the table every time.

---

## 2. THE INSTRUMENT DEFECT (MEASURED) — and why it is the lane's main result

### 2.1 What happened

`tools/mint_slot_sweep.py` reported `??_7Geometry@Graphics@@6B@` as **5/9
unported**. `tools/runtime_census.py` scored the same row **6/6 COMPLETE**.
Two instruments, one row, a disagreement of five, both printed in ordinary use.

The census was right. All five slots are registered:
`runtime/src/object/deleting_dtor.cpp:912` (slot 0, by address) and
`runtime/src/graphics/scene.cpp:1884-1899` (slots 4-8, by name).

### 2.2 Cause

`runtime/src/graphics/scene.cpp` carried **exactly one raw 0x00 byte, at file
offset 7277**, inside a comment that had pasted a literal NUL where it meant the
two characters `\0` — in the essay describing the `"ImageBuffer\0..."` string
run that the Geometry vtable overrun used to read into.

`git grep -h` prints

```
Binary file HEAD:runtime/src/graphics/scene.cpp matches
```

**instead of the matching lines** for any blob with a NUL inside git's
first-8000-byte sniff window, and reports success. All 38 `RegisterRuntime*`
lines in that file were therefore invisible to the sweep's registration scan.

The error direction is **INFLATING**: it manufactures port work. That is the
direction this project has a recorded instance of a lane acting on.

### 2.3 Fix, both ends

* The NUL is replaced by the two characters it meant (`scene.cpp` is plain text
  again; verified `count(b"\x00") == 0`).
* `tools/mint_slot_sweep.py` now passes `-a` **and carries a control**:
  `git grep -c` counts matches per file without needing content, so it sees
  binary blobs. If the line count read differs from the count git reports over
  the same pathspec, the tool **refuses** rather than printing a confident low
  number.

### 2.4 Measured delta

| | before | after |
|---|---|---|
| ported (name or addr) | 325 | **354** |
| UNPORTED -> named trap | 112 | **83** |
| purecall | 143 | 143 |

`??_7Geometry@Graphics@@6B@` and `??_7Light@Graphics@@6B@` left the "worst
vtables" list entirely.

### 2.5 THE NEAR MISS — a control that cannot observe the channel it polices

Before finding the real cause I tried to test the binary-suppression hypothesis
by diffing two channels:

```
git grep -l    -E "RegisterRuntime(Export|Addr)\(" ...
git grep -l -a -E "RegisterRuntime(Export|Addr)\(" ...
```

**They were identical, and I nearly concluded the hypothesis was wrong.** They
are identical *by construction*: `-l` prints only file NAMES, and name output is
never suppressed for a binary file — only *content* output is. The control was
structurally incapable of observing the thing it was testing, and its clean
result exonerated a broken instrument.

This project already knows two faces of this family:

* a control must corrupt something the check actually **compares** (a control
  that corrupts an uncompared value reports NOT CAUGHT and indicts a sound
  instrument);
* its mirror, a control that corrupts a byte that should **not** be compared
  reports CAUGHT and exonerates a broken probe.

This is the **third face**: *the control must be able to observe the channel it
polices.* A pass from a control aimed at the wrong channel is not weak evidence,
it is **no** evidence, and it reads exactly like strong evidence.

### 2.6 "Would anything detect it now?" — asked, and answered

**Yes, and the detector already existed and was not being run.**
`mint_slot_sweep.py`'s own docstring states its validation: its three numbers
must match, character for character, the `[RtMint] minted ...` line the boot
prints from `MintCounts`. That claim was made at revision `fc884df` and had
silently stopped holding.

MEASURED on this lane's verification boot (`standalone_debug.log`):

```
[RtMint] minted 63 vtables (580 slots: 354 ported, 83 unported-trap, 143 purecall-trap)
```

against the fixed sweep's `ported 354 · UNPORTED 83 · purecall 143` — exact.
**Before the fix the sweep said 325/112 and the boot said 354/83, and no screen
compared them.** So the detector is a cross-instrument identity that is written
down in the tool and never automated. Turning it into an assertion is cheap and
is left as a named residual (§6).

---

## 3. PROMOTIONS (LANDED)

Both in `standalone/reg_mint_promoted.cpp`, both through
`PromoteVtableGuarded`, which re-asks `RtMintVtableTrapCount()==0` at runtime
and refuses on any non-zero. Stamper evidence from
`tools/mint_stamper_census.py` — **controls green on the run used**: decoder
control 3/3 found, positive control `??0BinaryFileStream@IO@@QEAA@XZ`
REGISTERED, negative control (synthetic symbol) not-registered.

### 3.1 `??_7Geometry@Graphics@@6B@` (0x180154e60) — **handed to this lane by name**

Wave 21 lane GFXVT tagged it HARMFUL. Wave 22 lane GEOM discharged the blocker
and wrote, in the RESOLVED note above its own essay, that *"re-scoring and
actually registering the promotion is a SEPARATE, still-open step lane GEOM did
not take"*. This lane is that step.

RE-DERIVED, not re-quoted:

* **The blocker is gone.** The HARMFUL verdict rested entirely on
  `code/Game/Component/statusicon_geometry.cpp` copying `i < 12` slots out of a
  NINE-slot table, so promoting would move a dormant overrun off the DLL's
  `.rdata` onto `static void* kVTable28[9]`. Read on this tip, that loop is
  `if (i < 9) ... else s_siGeomVtable[i] = nullptr;` and never indexes
  `baseVtbl` at 9 or beyond.
* **Static half:** 9 slots, 3 purecall, 6/6 non-purecall registered.
* **Dynamic half:** 3 stamp sites in 3 functions — ours-capable 2, retail 1,
  unattributed 0.
  * `??0Geometry@Graphics@@QEAA@XZ` 0x1800a4281 — OURS (name+addr)
  * `??1Geometry@Graphics@@UEAA@XZ` 0x1800a41d5 — OURS (name+addr)
  * `??0Geometry@Graphics@@QEAA@AEBV01@@Z` 0x180015b7e — RETAIL (copy ctor)
* **The door reaches it:** `runtime/src/graphics/scene.cpp:212` stamps
  `ResolveAddr(Module::Runtime, 0x180154e60)`, and `ResolveAddr` consults the
  port registry FIRST.

A copy-constructed `Geometry` keeps retail's table. That is a **SPLIT, not a
trap** — both tables dispatch the same six bodies — and at cutover there is no
retail body left for the copy ctor to run.

A/B control: `BANISHED_RT_PROMOTE_GEOM=0`.

### 3.2 `??_7AnimationEvent@Graphics@@6B@` (0x180154bf0)

* **Static half:** 9 slots, 5 purecall, 4/4 non-purecall registered — slot 0 the
  unnamed deleting dtor (0x180011800, by address), slots 1/2/4 `?TypeName@`,
  `?Type@`, `?Compile@External@Object@` by name.
* **Dynamic half:** 5 stamp sites, ours-capable 3, retail 2 (the copy ctor
  0x180011777 and `FUN_1800117b0`), unattributed 0.
* **Door:** `runtime/src/graphics/animation.cpp:156` resolves it by address;
  `:240` and `:276` stamp it.
* **No out-of-bounds consumer.** The Geometry hazard was a copy loop over the
  table. Grepped for on this tip: there is none for this row — both consumers
  assign the pointer and nothing indexes it. Minted storage is
  `static void* kVTable5[9]`, the full extent.

A/B control: `BANISHED_RT_PROMOTE_ANIMEVT=0`.

### 3.3 WHAT NEITHER PROMOTION CLAIMS

**Installation only.** No A/B was run for either row and no attendance counter
says either table is ever DISPATCHED. That is the same tier as every other row
in that file, and it is stated here so a later reader does not upgrade it.

The related trap is live in this very namespace and is worth restating:
`??_7MaterialInstance@Graphics@@6B@` is promoted and measured
`[W17Attend] never init=0 post=0` — **the door is not opened at all on the
autostart route**, so its silence means neither working nor broken.

---

## 4. `??_7UpdateLoop@Engine@@6B@` IS HELD BACK — and is deliberately NOT tagged

It is the third COMPLETE table in scope (7/7 of its 12 slots non-purecall, every
one registered) and it is the one a "cheap wins" reading picks up next. It is
**not** promoted, for a reason the COMPLETE column cannot express.

Its stamper census is a different SHAPE from the two above: **8 stamp sites,
ours-capable 2, retail 6, unattributed 0.** The six retail ones are not
UpdateLoop functions at all — they are the **base-subobject stamps in the ctors
and dtors of derived loop classes**:

```
??0UpdateLoop@Engine@@QEAA@AEBV01@@Z   0x1800317ee   (copy ctor)
FUN_180031880                          0x18003189d
??0LoaderLoop@Engine@@QEAA@AEBV01@@Z   0x18004d3be
??0DialogLoopT@Engine@@QEAA@AEBV01@@Z  0x18004d7f3
??1DialogLoopT@Engine@@UEAA@XZ         0x1800db394
??1LoaderLoop@Engine@@UEAA@XZ          0x1800dc190
```

Geometry's single retail stamper splits one object's vptr. These six are the
base-class stamp of **three other classes** whose own tables
(`??_7LoaderLoop@` 6/12, `??_7DialogLoopT@` 10/12, `??_7DialogLoaderLoopT@`
8/12) are all INCOMPLETE and unpromoted — the hierarchy that runs the entire
front end.

**WHAT THIS LANE DID NOT DETERMINE, said plainly:** whether those six stamps are
TRANSIENT (ordinary MSVC base-then-derived sequencing — the base ctor stamps the
base table and the derived ctor immediately overwrites it, so the split never
outlives the constructor and is harmless) or FINAL. It is decidable by
disassembling six functions and it is the whole question.

**No `RT_UNPROMOTABLE:` line was written for this row.** It is not a verdict
that promotion is harmful; it is an admission that a check was not finished.
Tagging it would file an unfinished measurement as a permanent refusal.

```
[SCOPE: DLL-PRESENT]  — at cutover there are no retail stampers and the question
   dissolves; the row is then REQUIRED, because runtime/src/engine/loops.cpp:313
   resolves it BY NAME and ResolveExport returns NULL for an unpublished row, so
   loops.cpp:414 and :583 would stamp a NULL vptr.
[ADDRESSEE: the next lane that owns runtime/src/engine/loops.cpp; failing that,
   the Wave 27 kickoff writer.]
[CONDITION: disassemble the six sites above and record, per site, whether the
   UpdateLoop table is still in the object when that function returns. All six
   transient -> promote. Any final -> write an RT_UNPROMOTABLE tag naming it.]
```

---

## 5. A SECOND BOARD DEFECT, FOUND AND HANDED OFF (MEASURED)

**13 of the 14 promoted vtables are still counted inside the board's 54-row
VTABLE gap.**

`tools/runtime_census.py` scores a vtable SYMBOL row ported only from a LITERAL
`RegisterRuntimeExport("??_7...")`. Wave 25 lane SWEEP routed every promotion
through `PromoteVtableGuarded(symbol, va, &n)`, which registers with a VARIABLE.
`mint_promoted_symbols()` was patched to match that door — that is the 111-row
correction written in its own comment — but `ported_exports` was not, so the
symbol row never flips.

Measured on this branch, comments stripped:

```
promoted vtables: 14 ; literally registered: 1 ; guard-only: 13
literal:    ??_7MaterialInstance@Graphics@@6B@   (Wave 25 VTGFX hand-spelled it)
guard-only: AnimationEvent@Graphics, BinaryFileStream@IO, BinaryReader@IO,
            BinaryWriter@IO, DialogInstance@UI, Element@UI, Geometry@Graphics,
            ImageBuffer@Video, MemoryStream@IO, SpriteSheet@UI,
            ToolTipDialog@UI, Visitor@IO, VisitorStream@IO
```

The clinching case is `??_7BinaryFileStream@IO@@6B@` — the row Wave 20 closed on
(attendance 0 -> 18,906) — which `--unported` still prints. **A row dispatching
18,906 times is not unported.**

Handed to **lane DOORS**, which owns board honesty this wave. Not applied here:
the one-line fix (fold `promoted_vt` into `d["ported_exports"]` at
`tools/runtime_census.py:2341`) moves the whole board mid-wave.

Note for whoever lands it: **the invariant is the SET of rows that flip, not the
count.** Lane PROMOIO measured 15-of-16 and this lane 13-of-14, each after its
own promotions landed on its own branch; both are right and the count keeps
moving as lanes merge.

Also note the trap in my own first pass at this: a quick scan that did **not**
strip comments scored `??_7Geometry@Graphics@@6B@` as "literally registered",
because `reg_mint_promoted.cpp` contains a *comment* reading
`RegisterRuntimeExport("??_7Geometry@Graphics@@6B@", ...)` describing the call
that was never made. An uncommented checker manufactured a *port* that time,
rather than work.

---

## 6. NOT REACHED — named, with addressees

Stated plainly rather than implied. None of the following was attempted.

| item | state | addressee / condition |
|---|---|---|
| The six `?_typeInfo@` promotions | **RESOLVED IN-LANE.** ATTEND reported before this lane closed. Two ATTENDED (`?_typeInfo@Achievement@1@`, `?_typeInfo@GoalInstance@Achievement@@`) are **PROMOTED** here. Four read `never`, all diagnosed (b) route-never-reached, and are recorded **ROUTE-UNDECIDED** in `reg_mint_promoted.cpp` — not promoted, not tagged, not dropped. | See §7.3 and the ROUTE-UNDECIDED block in `standalone/reg_mint_promoted.cpp`. Per-row conditions and the Wave 27 kickoff writer as addressee are written there. |
| `??_7Light@Graphics@@6B@` 5/6 -> COMPLETE | not done. One slot: the unnamed deleting dtor at **0x180058dd0**. Note its stamper census is **9 sites, ours-capable 1, retail 8** — the worst ratio in scope, because `AmbientLight`, `SpotLight` and `SunLight` all stamp this base table. Closing the slot makes it COMPLETE; it does **not** make it promotable. | any later Graphics lane |
| `??_7Particle@Graphics@@6B@` 5/6 -> COMPLETE | not done. One slot: `?Serialize@Particle@Graphics@@UEAA_NAEAVStream@IO@@@Z` @ 0x180103f00. On the WRITE path a Serialize is replayable, so an oracle exists. | any later Graphics lane |
| `??_7Stream@Audio@@6B@` 1/2 and `??_7Loader@Engine@@6B@` 1/2 | not done. One slot each: `?Run@Stream@Audio@@` 0x1800a0f60 and `?Run@Loader@Engine@@` 0x1800dbfc0. **Stream@Audio's stamper census is 2 sites, ours-capable 2, retail 0 — the only clean 2-of-2 in this lane's scope**, so it is the cheapest genuinely-promotable table left here. `Loader@Engine` is 0-of-2 ours and its second stamper is `FUN_180148fe0` writing `?_loader@LoaderLoop@Engine@@`, itself a tagged HARMFUL row — do not treat the two as equivalent. | any later Audio/Engine lane |
| The 3 Graphics BODY rows (`?BuildParameters@MaterialInstance@`, `?Draw@Renderer@Graphics@`, `?Update@AnimationState@Graphics@`), Achievement's `?Initialize@GoalList@`, Engine's `?Draw@MainLoop@`, Audio's `?Update@Scene@Audio@` | **not attempted.** These are 6 of the 8 genuinely unwritten bodies on the whole board. The lane spent its budget on the instrument defect and the promotions instead. | Wave 27 |
| Turning §2.6's cross-instrument identity into an assertion | not done. `mint_slot_sweep.py` states the boot-line agreement as its validation and nothing checks it. | whoever next touches `tools/mint_slot_sweep.py`; condition = a `--verify-against <standalone_debug.log>` mode that fails on any mismatch |

**`?Serialize@AnimationManager@Graphics@` MUST STAY UNVERIFIED.** Wave 24 GFXMAT
answered Wave 23 GFXBODY's R4 and the answer was **no**. Not touched by this
lane; recorded here so a cleanup pass does not delete the note.

---

## 7. VERIFICATION RUN

Per the wave's mandatory mitigation, md5 of this lane's own build against the
deployed exe, taken immediately before launch:

```
99439614e14fe8fb64581d370e7e765d  standalone/build/reimpl-x64.exe
99439614e14fe8fb64581d370e7e765d  C:/GOG Games/Banished/reimpl-x64.exe
```

Game-dir lock held as `lane-PROMOGFX` for the whole deploy+run (acquired from
`lane-FACTORY` after a 60s wait; never piped, never backgrounded).

Run: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=180
BANISHED_W17_ATTEND=1` — an in-game run that self-quits.

Result: see §7.1.

### 7.1 Run A — the two vtable promotions (MEASURED)

exe md5 `99439614e14fe8fb64581d370e7e765d`, identical on both sides.
Clean exit, **0-byte `standalone_crash.log`**, `[W17Attend] CONTROL
?_singleton@MainLoop@Engine@@ init=1 post=2 -> GREEN`.

```
[RtMint] promoted 42 minted DATA row(s) -- live on every run
[RtMint] vtable promotion guard: 13 row(s) evaluated, 0 refused
[RtMint] minted 63 vtables (580 slots: 354 ported, 83 unported-trap, 143 purecall-trap)
```

`13 evaluated, 0 refused` is the runtime half of the readiness gate: both new
rows passed `RtMintVtableTrapCount()==0` in a live process, not just in a source
scan. (13, not 14, because `??_7MaterialInstance@Graphics@@6B@` carries its own
inline copy of the guard rather than calling the helper.)

The `580 slots: 354 ported, 83 unported-trap, 143 purecall-trap` line is the
cross-instrument identity of §2.6, holding again.

### 7.2 THE ATTENDANCE LINE FOR MY OWN TWO PROMOTIONS IS **NOT EVIDENCE**

Both runs print:

```
[W17Attend] never    init=0    post=0      ??_7AnimationEvent@Graphics@@6B@
[W17Attend] never    init=0    post=0      ??_7Geometry@Graphics@@6B@
```

A reader will take that as "the promotion did nothing". **It says nothing at
all.** `standalone/wave17_door_attend.cpp` counts `ResolveExport(<mangled>)`
doors ONLY — directly, or through `RT_DATA_SLOT -> RtResolveCached ->
ResolveExport`. Both of these rows are reached by **address**:
`ResolveAddr(Module::Runtime, 0x180154e60)` at `scene.cpp:212` and
`ResolveAddr(Module::Runtime, 0x180154bf0)` at `animation.cpp:156`. The
instrument watches a channel neither row uses.

This is the "a diagnostic field structurally incapable of carrying what it is
read for" shape. **Any `??_7` row whose only door is ResolveAddr-by-address is
unobservable to this screen, and it prints the same `never` as a genuinely dead
row.** Reported to lane ATTEND for its own table.

So: the two vtable promotions are **installed, guarded and boot-clean, with no
dispatch evidence either way.** That is the honest state and it should not be
written up as more.

### 7.3 Run B — the two `?_typeInfo@` promotions (MEASURED)

exe md5 `64d17bb5cea5cba43dacf3174073ed2f`, identical on both sides. Game-dir
lock re-asserted (`assert-held`) between build and launch. Clean exit, **0-byte
crash log**, control GREEN.

```
[RtMint] promoted 44 minted DATA row(s) -- live on every run     (42 -> 44)
[RtMint] vtable promotion guard: 13 row(s) evaluated, 0 refused
[W17Attend] ATTENDED init=0 post=1   ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A
[W17Attend] ATTENDED init=0 post=1   ?_typeInfo@GoalInstance@Achievement@@0VBaseTypeInfo@Object@@A
```

Both rows read **ATTENDED with the promotion live**, on a different tip and a
different exe from ATTEND's run. That independently reproduces ATTEND's verdict
*and* shows the promotion did not close the door it was granted on — which is
the failure mode worth checking for and is not implied by either fact alone.

---

## 8. BOARD MOVEMENT (MEASURED, same command as §1)

```
                before            after
TOTAL        1333 / 1185  88.9%   1337 / 1191  89.1%
Graphics      108 /   90  83.3%    110 /   92  83.6%
Achievement    52 /   43  82.7%     52 /   45  86.5%
```

**Read the identity, not the percentage.** Graphics' `called` and `ported` BOTH
rose by 2 — that is the vtable slot fold-in, not two bodies written. The
Achievement move is the only one that is a row leaving the gap (9 -> 7), and it
is two DATA rows published, not two functions reimplemented.

And the movement is **understated**, by §5: the two vtables promoted here are
two of the 13 rows that stay inside the VTABLE gap because the census's
`ported_exports` scan cannot see `PromoteVtableGuarded`. When lane DOORS lands
that fold, both will flip and Graphics' gap will drop by two more without
anybody doing further work.

---

## 9. LESSONS THIS LANE WOULD PUT IN THE INDEX

1. **The control must be able to OBSERVE the channel it polices.** (§2.5) A
   third face of a family this project already knows from two other sides. A
   control aimed at the wrong channel returns a clean pass that is not weak
   evidence but *no* evidence, and reads like strong evidence.
2. **A raw NUL byte in a text source silently deletes that file from every
   content-based `git grep` in the tree.** (§2.2) The tool reports success, the
   error direction INFLATES, and there is no warning. Worth a one-off tree
   scan — this lane fixed `mint_slot_sweep.py` and nobody has swept the rest of
   `tools/`.
3. **Two instruments disagreed by five on a single row, in ordinary use, and
   nothing compared them.** (§2.6) The detector existed, written into the
   tool's own docstring as its validation claim, and had silently stopped
   holding. A validation claim that is prose rather than an assertion decays
   without a detector.
4. **A `never` from an attendance screen must be read against WHAT THE SCREEN
   WATCHES.** (§7.2) Two freshly-promoted rows printed `never` because their
   door is an address, not a name.
5. **`COMPLETE` plus a green stamper census is still not enough.** (§4)
   `??_7UpdateLoop@Engine@@6B@` passes both and its six retail stampers are the
   base-class stamp of three other classes. The gate that mattered was one
   neither screen asks about, for the third time in this file's history
   (Geometry's overrun was the second).
