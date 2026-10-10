# Wave 9 lane N — the three findings lane M left open

Lane N owns three of [WAVE9_LANE_M.md](WAVE9_LANE_M.md)'s findings. All three are things that
become **unfindable after lane G removes LoadLibrary**: two are instruments, one is a defect
that only the shipped DLL's presence makes visible.

| Finding | Verdict |
|---|---|
| **1** — `UI::NumberElement::SetValueInt` diverges on tutorials 1-3 | **CLOSED.** An ORACLE defect, diagnosed by lane M and FIXED here: the two `NumberElement` setters and `NumberEditElement::SetMinMax` are now NO-ORACLE ports. Layout corrected: `+0xD0` is an owning refcounted `System::String` and the object is **0xE8** bytes, not 0xE0. |
| **2** — `Picking::Scene::Intersect` diverges on the stress route | **CLOSED, and it was a REAL PORT DEFECT.** A moved term: the inverse transform's translation was folded into `localOy`/`localOz` FIRST instead of added LAST. Fixed and proved on a deterministic 244,824-ray repro — 21 divergences -> 0 with the denominator unchanged, and a negative control that fires at 7,520/7,536. |
| **3** — intermittent shutdown crash | **REPRODUCED 4x, RE-CHARACTERISED, family ENUMERATED; culprit NOT confirmed.** Two corrections to the record: the arithmetic (a small integer over a pointer's LOW half, not a decremented boundary tag) and the **stack** — symbolized with `nm` it is the WorkComponent task-cleanup path, not a tutorial tree through `TreeNodeFree`. Every free/erase pair in the family is now tabulated; no path frees a still-linked node, so the leak must be an erase that no-ops while the destroy after it proceeds. One candidate identified, instrumented with a falsifiable probe, **not** yet measured. Also found: retail *hard-faults* where our port silently guards. |

---

## Provenance

| | |
|---|---|
| tree | `921eb23` (lane M's detector commits `eb8317d`, `f9b8d36` included) |
| acceptance-cell exe | **2026-09-08 00:41:26**, 26,213,818 bytes. `runtime/src/picking/scene.cpp` carried lane N's **uncommitted, inert** Picking fixture in that build — recorded here rather than left to be discovered later. It is a different subsystem from the tutorial-1 UI cell the acceptance test measures, and the only always-on part of it is one `PickNegCtl()` branch inside `Picking::Scene::Intersect`. |
| matrix exe | **2026-09-08 00:43:02**, 26,207,232 bytes, built with the fixture **PARKED** (`git stash`), i.e. a clean `921eb23` tree. The matrix is a baseline other lanes will compare against, and an instrument welded to the one function under investigation must not be inside it. |
| earlier finding-2 runs | exes **00:34:34** (26,176,686 B) and **00:37:17**, both lane N's own tree; each `rt_run.sh` line quotes the stamp it measured. |

---

## Finding 1 — CLOSED: three sites become NO-ORACLE ports

### What it actually was

Not a port defect. `UI::NumberElement::SetValue` (both overloads) ends in the DLL's own private
`NumberElement::CacheText` (0x1801189F0), which **replaces a refcounted `System::String`** at
`+0xD0` using retail's inlined `String::operator=`:

```
180118a3d  mov  rcx,[rbx+0xd0]        ; the CURRENT cached text
180118a44  cmp  rcx,[rax]             ; vs the freshly formatted one
180118a4e  dec  DWORD PTR [rcx-0x4]   ; release: refcount at data-4
180118a55  cmp  DWORD PTR [rcx-0x4],0
180118a57  add  QWORD PTR [rbx+0xd0],-8
180118a66  call 0x1800c7ad0           ; MemFree(data-8)
180118a7b  mov  [rbx+0xd0],rax        ; store the new
180118a87  inc  DWORD PTR [rax-0x4]   ; acquire
```

The oracle prefix-replayed those bodies: `memcpy(&replay, self, sizeof(NumberElement))`, run ours
on `self`, run the DLL's on `&replay`. That is a **shallow** copy, so `replay`'s `+0xD0` was the
same buffer `self` held — and ours had already released it. **The oracle handed the DLL's own
body a dangling pointer, on every call that re-formatted text.**

Stated narrowly, because that is what was measured: a **use-after-free** in the shipped order. A
second free in the shipped order is *not* established (no shipped-order run crashed); allocator
free-list corruption was demonstrated only under lane M's order-inversion control.

The rule this violates was **already written in `widgets_input.cpp`'s own header block**, lines
53-58, by the lane that ported it — *"NOT done this way: anything that swaps a REFCOUNTED
pointer … which is exactly why `EditElement::SetText` is a documented NO-ORACLE port"*. The two
sibling lanes applied it: `widgets_layout.cpp`'s `TextElement::SetParameter`/`SetText` are
NO-ORACLE and name `CacheText`'s refcounting as the reason, and `element.cpp`'s
`SetName`/`SetToolTipString` are shadow oracles for the same reason. Lane D's file is the only
outlier, and it is an outlier **because `NumberElement.h` called the owning pointer padding.**
A layout gap stops being a documentation problem the moment an oracle decision is read out of the
header.

### The acceptance test for lane M's released-pointer detector — RUN, AND IT PASSED

Lane M's detector (`eb8317d`) is compiled-but-unrun code inside `RtDiffReportBytes`. Its only
chance to be seen firing on a **real** defect rather than a synthetic one is the tutorial-1 cell
while this site is still broken — which lane N's fix destroys. So the fix was **parked** and the
cell was run first. Raw log archived at `wave9_lane_n/tut01_acceptance.err.log`.

The self-test line first, because its own contract says a run is void without it:

```
[RtDiff] released-pointer detector self-test: ALIVE (fires on a released slot: 1/1; stays silent on a live one: 0/0)
```

Then, verbatim, the detector firing on the real defect:

```
[RtDiff] *** HARNESS DEFECT (not a port defect) *** UI::NumberElement::SetValueInt call #1
[RtDiff]     +0xd0: the DLL's body ran on a scratch still holding 00000256c89681a8,
[RtDiff]     which OUR body had already RELEASED (header at [-8] reads
[RtDiff]     {len=0,rc=0}); ours holds the live 00000256c8b93250. This site prefix-replays
[RtDiff]     a body that swaps a REFCOUNTED pointer, which the oracle
[RtDiff]     contract forbids. Make it a NO-ORACLE port -- do NOT exclude
[RtDiff]     these bytes from the compare, which would hide the
[RtDiff]     use-after-free while turning the report green.
[RtDiff] DIVERGENCE UI::NumberElement::SetValueInt  call #1  ours=+0xd0: 50 32 b9 c8 56 02 00 00  theirs=+0xd0: a8 81 96 c8 56 02 00 00
```

Two things worth reading off calls #1-#3 that are stronger than any argument:

* the header of the block the DLL was handed reads **`{len=0,rc=0}`** — it is not "probably"
  freed, its header has been wiped;
* **theirs on call *N* is ours on call *N+1***: `…c89681a8` is theirs at #1 and ours at #2;
  `…c8b8fe40` is theirs at #2 and ours at #3. The allocator handing the just-freed block straight
  back, in the log, in order.

Cell totals: **335 sites, 10,531,395 calls, 10 divergences in 1 site**, and the summary's own
verdict line: `1 sites hit the RELEASED-POINTER detector`.

### The fix, verified on the same cell

Same route, same harness, exe **2026-09-08 00:49:17**: **335 sites, 10,406,571 calls,
0 divergences in 0 sites**, no `HARNESS DEFECT` lines, and the site still present and named:

```
[RtDiff]   UI::NumberElement::SetValueInt [no oracle: CacheText owns a refcounted String] calls=0 ... divergences=0
```

`calls=0` is correct and expected for a NO-ORACLE port — the counter is incremented by
`RtDiffReport`, which the site no longer calls — and it is the same shape
`UI::EditElement::SetText` has carried since Wave 7. **The site count is unchanged at 335**, so
nothing vanished from the audit. The self-test line also stops printing, for the reason in the
next section, and that absence is itself the confirmation: Finding 1 was the only byte divergence
in the cell.

### A FOURTH BLIND SPOT IN THE DETECTOR, found by running it

Lane M states three limits on reading a clean detector sweep. There is a fourth, and it is the
one that decides how much the matrix below is worth.

**`RtDiffScanForReleasedPointers` is called from inside `RtDiffReportBytes` *after* the
`memcmp(ours, theirs, n) == 0` early return.** So is the self-test. The detector therefore only
ever examines comparisons that **already differ** — it cannot see a site whose two buffers match.

That is circular with lane M's own blind spot #2: a released pointer is invisible *precisely
when* the DLL reallocates into the block we just freed and the two sides agree. The detector is
blind in exactly the case that hides the defect.

The matrix makes this concrete rather than theoretical: **7 of the 10 cells printed no self-test
line at all**, because they had zero byte-compare divergences, so the detector never executed in
them. By its own stated contract their silence means nothing. The three that printed `ALIVE`
(tutorial 1, 2, 3) are exactly the three that had a byte divergence.

**So the matrix did not produce a tree-wide list of sites.** What it produced is: *across every
comparison that was already divergent, in 3 of 10 cells, the only site named is
`UI::NumberElement::SetValueInt`.* That is a sweep of one site, not of ~170, and it must not
close a gate row. Moving the scan **above** the `memcmp` early return would make it a real sweep
(at the cost of running on every byte comparison); that is a change for whoever owns `rt_diff.cpp`,
not one lane N made while holding the directory.

### The fix

`SetValueInt`, `SetValueFloat` and `NumberEditElement::SetMinMax` become NO-ORACLE ports, beside
`UI::EditElement::SetText`, with `RT_DIFF_BEGIN` retained so all three still appear in
`RtDiffSummary()` rather than vanishing from the audit.

**Not** a `+0xD0..+0xD7` exclusion. An exclusion greens the report while leaving the
use-after-free running on every diff run, and hides any real future defect in the field. It is the
inverse of `reference_oracle_must_not_compare_unwritten_bytes.md`: there, neither side wrote;
here, **both** sides write, through the same DLL body, and what they write is an address.

**Not** a deep copy or snapshot-restore either (which is what lane N was originally briefed to
build, and lane M's evidence superseded it): a prefix replay of any body that reaches `CacheText`
is unsound **in principle**, because the field is a reference into globally shared refcounted
state that no per-object snapshot isolates.

**Why these three did not instead adopt `RT_DIFF_REPLAY_PRE/_POST` (INVERT).** You cannot invert
a replay you have removed, and removing it is the stronger action. It would in any case have
demonstrated nothing on two of the three: `UI::NumberElement::SetValueFloat` and
`UI::NumberEditElement::SetMinMax` **never register a site in any cell of the matrix** — they are
never called, so they have zero coverage and no run can give them any. Their inclusion rests on
mechanism, not measurement: `SetValueFloat` is the same body through the same `CacheText`, and
`SetMinMax` reaches `EditElement::_text` at `+0xC8`, which is inside the copied object exactly
the way `+0xD0` is. Both are latent, and both are now disarmed.

### The sweep lane M asked for: what else prefix-replays a body that swaps a refcounted pointer

The criterion is sharper than "the body touches a refcounted pointer", and the distinction is what
keeps the sweep from being a witch hunt:

> **Reaching the pointer through a live shared object is SAFE. Holding a COPY of the pointer is
> not.** In the first case the refcounts stay correct and the work is merely done twice; in the
> second, ours may already have freed what theirs is about to release.

| Site | Verdict |
|---|---|
| `NumberElement::SetValue(int)` / `(float)` | **UNSAFE** — the String is a field of the copied object. Fixed. |
| `NumberEditElement::SetMinMax` | **UNSAFE** — `EditElement::_text` at `+0xC8` is in the copy. Fixed (never called; latent). |
| `SpinnerElement::SetMinMax` / `SetValue` | SAFE. They reach the refcounted `_text` through `_children._data[0]`, a **live child**; `SpinnerElement` itself is `{Element, int32}`. |
| `SpinnerElement::SetElementId` / `SetIncrement`, `RadioElement::SetActiveRadio`, `ProgressElement::SetMax` / `SetCurrent` | SAFE — no refcounted field is released by any of those bodies. |
| `element.cpp`'s `ReplayGuard` family | SAFE; the two that do touch a refcounted String (`SetName`, `SetToolTipString`) were already shadow oracles. |
| `widgets_layout.cpp`'s `TextElement::SetParameter` / `SetText` | already NO-ORACLE, naming `CacheText` explicitly. |
| `sprite.cpp`'s `replay*` family | already balances its extra reference with `ReleaseStringRef`. |

Nothing else in `runtime/src` prefix-replays an object that owns a `System::String`. The three
fixed here were the whole set — and the matrix run below is the empirical half of that claim.

### The layout correction (`runtime/types/ui/NumberElement.h`)

Lane M established from behaviour that `+0xD0` is not padding. Lane N adds the static evidence and
the object's true size:

| offset | field | evidence |
|---|---|---|
| `+0xC0` | `Font* _font` | `ComputeSize` 0x180119000 loads it into RCX for the measure call 0x180116C70 and reads its `+0x50`/`+0x54`; `Draw` 0x1801191E0 the same |
| `+0xC8` | `int32 _mode` | `GetNumberType` 0x18004A9C0 is `mov eax,[rcx+0xc8]; ret` |
| `+0xD0` | `char* _cachedText` — **owning, refcounted `System::String`** | assigned by `CacheText` with the release/acquire idiom above; the **dtor** (0x180118CE0) runs that same release on it *and nothing else*; `ComputeSize` reads the LENGTH at `[p-8]`; `Draw` passes `&this->_cachedText` as a `System::String&` (`lea r8,[rbx+0xd0]`) |
| `+0xD8` | `int32 _intValue` | `GetIntValue` 0x18004A9A0 |
| `+0xDC` | `float _floatValue` | `GetFloatValue` 0x18004A9B0 |
| `+0xE0` | `bool _showSign` | `CacheText`'s format select: `_showSign ? "%+i" : "%i"` (mode 0), `"%+.2f"` / `"%.2f"` (mode 1) — the four literals are at 0x180159144, 0x180154848, 0x180159148, 0x180159150. Written as a BYTE by the ctor |
| **size** | **0xE8** | `CreateNewObject` 0x18004A910: `mov edx,0xe8` into `MemoryAllocator::Allocate(_singleton, 0xE8, 8)` |

**A second harness defect fell out of that size.** The old struct was 0xE0 — eight bytes short —
so the oracle's stack `replay` object was 0xE0 bytes while the DLL's `CacheText` **reads `+0xE0`**
on it. The replay was selecting its format string from a garbage stack byte, a stack
out-of-bounds read. Removing the replay removes it; the size is recorded because it is the
object's true size.

### The three-way `+0xD0` disagreement, resolved without touching `TextElement.h`

`NumberElement.h` said padding, `TextElement.h` says `Array<System::String>.size`, and the field
behaves as an owning pointer. At most one can be right **about one class** — and the resolution is
that they are about **two different classes**:

**`UI::NumberElement` is not a `UI::TextElement`.** Its default ctor (0x180118C70) calls the
`UI::Element` base ctor at 0x180115E50 — not TextElement's — and then writes its own fields from
`+0xC0`; and it has its **own** private `CacheText` at 0x1801189F0, distinct from
`TextElement::CacheText` at 0x18011EC10. So `TextElement.h`'s call-site-derived table and this
header's owning pointer are both correct, about different types that happen to share an offset.
`TextElement.h` is **not** edited: a written-extent fact belongs to a WRITER, not to an offset
(`reference_never_written_needs_by_whom.md`), and one function's behaviour on a `NumberElement` is
no authority over `TextElement`'s call sites.

---

## Finding 2 — CLOSED: a moved term in the local-space ray origin

**It was a real port defect, of exactly the class the port's own no-tolerance comment exists to
catch** — *"anything less than bit-exact means a term was moved."* A term had been moved.

`Intersect` pulls the ray ORIGIN into each sub-mesh's local space. Retail adds the inverse
transform's TRANSLATION **last**, as a single `addss` against a register that already holds the
fully accumulated rotated sum — 0x180124B56-0x180124BB4, three times, identically:

```
180124b56  movss xmm0,[rbp-0x58]   ; invX.translation.x
180124b5b  addss xmm0,xmm3         ; + the whole rotated part      -> local.x
180124b79  movss xmm0,[rbp-0x54]   ; invX.translation.y
180124b83  addss xmm0,xmm2         ; + the whole rotated part      -> local.y
180124ba8  movss xmm0,[rbp-0x50]   ; invX.translation.z
180124bb0  addss xmm0,xmm7         ; + the whole rotated part      -> local.z
```

`localOx` had that shape. **`localOy` and `localOz` did not.** They read

```c
const float localOy = invX.translation.y + (iqw*c23 - c20*niqx) + c19*niqy + c26*niqz;
```

which in C is `((((t + a) + b) + c)` — the translation folded in **first**, with the remaining
terms accumulated on top of it, instead of `t + ((a + b) + c)`. One moved term, in two of three
components. The rotated part's own accumulation order was checked separately against retail's
`xmm2` chain (`((A - B) + C) + D`) and was already correct; only the translation's placement was
wrong.

**Why it hid.** Re-associating an addition changes the *rounded* result only where the operands
nearly cancel. `invX.translation` and the rotated part are both of order tens and largely cancel
on only a small region of one mesh, so the defect is invisible on essentially every ray. That is
the whole explanation for "1 in 23,654 calls" and for "1 stress run of 4" — not a knife-edge
coincidence, a small region of one surface that few rays cross.

### The proof, on a deterministic repro with a working negative control

| | rays | hits | **divergences** | exe |
|---|---:|---:|---:|---|
| before the fix | 244,824 | 29,867 | **21** | 2026-09-08 00:50:34 |
| after the fix | 244,824 | **29,867** | **0** | 2026-09-08 00:54:19 |
| negative control (1-ULP bump of `hitT`), before | 40,344 | 7,536 | **7,520** | 00:50:34 |

The denominator is **identical** across the fix — same rays, same hits — so this is a measurement,
not a lucky clean run, and it is not the "the rays were not cast" failure that made lane M refuse
to accept its own clean re-runs. The whole run went with it: `Picking::Scene::Intersect
calls=270438 divergences=0`, and the run summary `353 sites, 109,085,103 calls, 0 divergences in
0 sites`.

**What made the fixture usable was fixing its two controls first**, and both had been wrong:
the effectiveness control (`hits=`) killed two useless ray sets, and the negative control had to be
replaced after it came back green (below). Neither correction was cosmetic — without the first,
a 5,445-ray sweep with 525 hits and 0 divergences would have been reported as evidence; without
the second, so would the 21-divergence run's successor.

### How the mechanism was located

Not by reading. The scaled-up sweep's diverging rays were **not scattered** — they were contiguous
patches of the target grid (rays #15200-15203, #15302-15304, #15403-15405, #15504-15505: adjacent
targets in adjacent rows), all on entity `id=0`, all from one ring origin
`(78.2423477, 63.3423462, 230)`. A contiguous patch is a *region of one surface*, which is the
signature of an arithmetic difference over a value range — not of a random rounding boundary and
not of a triangle-selection tie. That is what pointed back at the local-space transform rather
than at the triangle test, and the translation placement was then visible in the disassembly.

### The road there: why a clean re-run is not an exoneration, and what was built about it

Lane M measured 90 divergences in 20,868 calls in one stress run of four, and 0 in the others —
with **different call counts** (20,868 / 22,668 / 24,023). Different counts mean different runs:
the rays are mouse-driven, so a clean run may only mean the divergent rays were not cast. That is
`reference_uncalled_port_is_not_verified.md` wearing a different costume.

So lane N built a fixture that removes the cursor from the experiment
(`BANISHED_PICK_SWEEP*` in `runtime/src/picking/scene.cpp`): on the *n*th real `Intersect` call it
casts a ray set **built entirely from constants** over the live scene, through the real function,
so the ordinary oracle diffs every one. Same seed, same flags, same arming point ⇒ the same rays
every run, and a stable denominator.

**Its effectiveness control did its job twice, by killing two useless ray sets before either could
be reported as a clean result:**

| attempt | rays | **hits** | divergences | what the control said |
|---|---:|---:|---:|---|
| vertical drops over ±60, step 6 | 2,205 | **8** | 0 | tested essentially nothing |
| same, aimed at (41.7, 26.6), ±4 step 0.25 | 5,445 | **525** | 0 | now hitting, still the wrong ray shape |
| oblique camera rays, ±8 step 0.4, ring 60 | 40,344 | **7,536** | 0 | ray shape now right; sample still small |

Without `hits=`, the second row is a clean sweep of 5,445 rays and would have read as evidence.

The third row is the fixture as it now stands: the ray set is an **origin ring** (24 positions on
a hemisphere of radius 60 — three elevations at 30/45/60° crossed with eight azimuths) aimed at
every point of a **target grid**, which is the shape of a camera pick rather than a vertical drop.
Hit yield went from 0.4% to 18.7% of rays.

**AND ITS PRESCRIBED NEGATIVE CONTROL CAME BACK GREEN — a defect in the fixture, reported rather
than quietly replaced.** `BANISHED_PICK_NEGCTL=1` moved one association in the `hitT` expression
(`(a+b+c)` → `(a+(b+c))`) and changed **nothing** on all 525 hits. A control that cannot fail
proves nothing, and this is the third instance of that failure mode in this wave.

**A CORRECTION LANE N MADE TO ITS OWN FIRST READING OF THAT RESULT.** It was reported upward as
"evidence against the association hypothesis". **That was wrong and is withdrawn.** The control
had 525 hits; the effect's measured rate is **1 in 23,654** (below), so its expected count in 525
samples is **0.02**. The control was **underpowered**, not informative: it says nothing about the
association hypothesis in either direction, and only proves the fixture cannot detect a rare
effect at that sample size. The green result is still a real defect *in the control* — a control
must fire on the geometry it is actually run on — and the replacement is a **1-ULP bump of
`hitT`** (`BANISHED_PICK_NEGCTL=1`), which no geometry can absorb and which is the exact magnitude
of the real defect. The association move is kept as `=2`, labelled in the source as the hypothesis
under test rather than as a control.

Sample size is not a footnote here: any experiment on this defect needs **>>23,654 hitting rays**
before a zero means anything. The sweeps in the table above are one to two orders of magnitude
too small to have shown it, which is the real reason they were green.

**THE REPLACEMENT CONTROL FIRES, HARD.** Same 40,344-ray sweep with `BANISHED_PICK_NEGCTL=1`
(exe 2026-09-08 00:50:34):

```
[PickSweep] fired after 16000 real calls: rays=40344 hits=7536 divergences=7520 ... negctl=1
[RtDiff]   Picking::Scene::Intersect   calls=66389  divergences=17955
```

**7,520 of 7,536 hits go red** — 99.8%. The fixture detects a one-ULP change in `hitT`, which is
*exactly* the magnitude of the real defect, on essentially every ray that hits anything. A zero
from this fixture at adequate sample size is therefore worth something; a zero from the earlier
association control was not.

### Lane M's cheapest suspect is DISPROVED — twice, independently

Lane M was explicit that `hitT <= bestT` vs `<` was a hypothesis it had neither disassembled nor
tested. It is now both, and it is wrong.

**(a) The disassembly.** Retail 0x180124EF5, read this lane:

```
180124ef5  comiss xmm3,xmm8      ; hitT vs 0.0
180124eff  jb     skip           ;  => require 0 <= hitT
180124f05  movss  xmm0,[rsp+0x28]; bestT
180124f0b  comiss xmm0,xmm3      ; bestT vs hitT
180124f0e  jb     skip           ;  => require hitT <= bestT
```

Both comparisons are **inclusive**, in that order — exactly `if (0.0f <= hitT && hitT <= bestT)`,
which is what the port has. The evidence is now a comment on that line in `scene.cpp` so the
hypothesis cannot be re-proposed by the next lane.

**(b) Stronger, and independent of the instructions.** Lane N's own reproduction and lane M's
report agree that the **`outNormal` bits are IDENTICAL between the two branches**
(`0.464434296, 0, 0.885607…` in both). The normal is derived from the winning triangle's edges
alone. Identical normal bits ⇒ **both branches chose the same triangle**, so no tie-break —
inclusive, exclusive or otherwise — can be the mechanism.

The same reasoning eliminates the guards: `outPos.x` and `.y` are bit-identical and only `.z`
moves, which is what a ~1-ULP difference in `hitT` does to a ray whose `dir.z` dominates
(`z` moves by δ, `x` by δ·dir.x, which stays inside one ULP of x ≈ 41.5). The port's
Möller-Trumbore guards were also checked against 0x180124E96-0x180124EC6 and match term for term,
including the `v + u <= det` ordering.

**`Math::Transform::Inverse` is eliminated too.** `scene.cpp`'s private `TransformInverse` is a
verbatim copy of the ported `RtMath_Transform_Inverse` in `runtime/src/math/transform.cpp`, and
that port is measured in the stress cell at **calls=3096, divergences=0**.

### Reproduced on three binaries, one of them with no lane-N code in `scene.cpp` at all

| run | exe | calls | div | `p.z` ours vs theirs | Δz |
|---|---|---:|---:|---|---|
| lane M, stress 1 | 2026-09-07 23:29:48 | 20,868 | 90 | -0.018081665 vs -0.0180778503 | **3.814697e-6** |
| lane N, sweep 1 | 2026-09-08 00:34:34 | 25,503 | 1 | -0.21415329 vs -0.214149475 | **3.814697e-6** |
| lane N, matrix stress | 2026-09-08 00:43:02 | 23,654 | 1 | 0.091594696 vs 0.0915985107 | **3.814697e-6** |

`p.x` and `p.y` are bit-identical in every one, the `outNormal` bits agree, and the three hit
points are within a world unit of each other. The third run is the important one: the fixture was
**parked** for it, so the measured binary contains no lane-N code in `scene.cpp` whatsoever. The
defect is not an artifact of the instrument.

### It is EXACTLY ONE ULP OF `hitT`, at a rate of 1 in 23,654

`Δz = 3.814697e-6` is not "about 4e-6" — it is **exactly 2^-18**, in all three observations, at
three different `z` values spanning -0.21 to +0.09. 2^-18 is one ULP of a float in [32, 64), which
is the magnitude of `p.x ≈ 41.3-41.9` and therefore of `hitT` for these rays. So:

* `hitT` differs between the two implementations by **exactly one ULP**;
* `outPos.z = hitT*dir.z + start.z` inherits it in full because `|dir.z|` is near 1;
* `outPos.x = hitT*dir.x + start.x` survives bit-identical because `Δ(hitT·dir.x)` is a fraction
  of an ULP at that magnitude and usually rounds away.

Two things follow. First, **the out-position arithmetic is exonerated**: our own binary was
disassembled at that block and emits plain `mulss`/`addss` with no FMA contraction, matching
retail 0x180124F1A-0x180124F44 operation for operation. The difference is upstream, in `hitT`
itself. Second, **the rate is the finding's defining property**: 1 divergence in 23,654 calls
means the two implementations agree bit-exactly on essentially every ray and disagree only where
`hitT`'s exact value lands on a rounding boundary. Any experiment that samples fewer than ~24,000
hitting rays and comes back green has measured nothing.

### What is NOT claimed

Two limits on the above, stated rather than glossed:

* **The interior of `localOz`'s rotated sum is not verified.** The translation's placement is
  proven for all three components from the three `movss`/`addss` pairs quoted above, and
  `localOy`'s interior accumulation was traced against retail's `xmm2` chain. Retail's `xmm7`
  chain, which feeds `localOz`, was **not** traced to the end. The 244,824-ray sweep at zero says
  whatever it does is bit-equal to ours on every ray that sweep casts, which is empirical evidence
  and not a reading.
* **A zero from this fixture is a zero over the rays it casts.** 29,867 hits on one region of the
  stress town is a far stronger sample than any real-ray run (which produced 1 divergence in
  23,654 calls), but it is not every mesh in the game. What closes the finding is the pair: the
  defect was *reproduced* at 21, and the same rays now give 0.

If another `Intersect` divergence ever appears, the next tool is already specified: **retail's
`hitT` is recoverable.** The port knows `dir.z` and `start.z` exactly and the oracle reports
retail's `outPos.z`, so `theirs_hitT = (theirs_z - start.z) / dir.z` recovers the other
implementation's value to within one division's rounding — enough to say which side is one ULP
high, and to test candidate expressions offline against it.

### The fixture

`runtime/src/picking/scene.cpp`, all off by default; the whole thing is inert unless
`BANISHED_PICK_SWEEP` is set, and the only always-on change is one branch on `PickNegCtl()`.

| var | meaning |
|---|---|
| `BANISHED_PICK_SWEEP=<n>` | fire the sweep after the *n*th real `Intersect` call. Arming late matters: at *n*=200 the stress town barely exists and the sweep hit 8 things. |
| `BANISHED_PICK_SWEEP_CX/_CY` | centre of the target grid in world coordinates |
| `BANISHED_PICK_SWEEP_HALF/_STEP` | grid half-extent and spacing → `(2·half/step + 1)²` targets |
| `BANISHED_PICK_SWEEP_RING` | radius of the 24-position origin ring (default 60) |
| `BANISHED_PICK_SWEEP_Z/_DIST` | grid height and `maxDist` |
| `BANISHED_PICK_NEGCTL=1` | **negative control**: bump `hitT` by one ULP. Must go red. |
| `BANISHED_PICK_NEGCTL=2` | the association move, kept as the *hypothesis*, explicitly not as a control |

Every ray is built from constants and the grid is integer-stepped (a `for (float x = -half; x <=
half; x += step)` loop accumulates rounding and can change its own trip count), so the ray set is
bit-identical on every run. Each sweep prints `rays=`, `hits=` and `divergences=`, and each
diverging ray prints its own `(start, dir, maxDist)`.

---

## Finding 3 — reproduced, stack corrected, free/erase family enumerated, culprit not confirmed

### The correction, which supersedes what was recorded

Lane M read the crash as this project's half-written-pointer signature and pointed at
`reference_load_wild_write_high_dword_7fc.md`, where an `fn_String_Dtor` **decremented** an
allocator boundary tag's high dword. **That reading is wrong for this crash, and the difference
changes what to search for.**

From lane M's own register dump: `RBX=000001D9FFFFFFFA`, `RDX=000001DA00000002`, and
`RDX - RBX = 8`. So **`RDX` is the pointer, and `RBX` is `RDX - 8`, its block header** — the
`FFFFFFFA` (= -6) is just the borrow out of the subtraction, not the corrupt value.

The corrupt value is `RDX` itself: `0x000001DA_00000002` — a **valid heap high dword with the LOW
dword replaced by the small integer 2**. Lane N's reproduction has the identical shape:
`0x000001D6_00000022`, low dword `0x22` = 34, high dword matching that run's heap
(`RSI=000001D62B69A7B0`).

That is a **small-integer store through a pointer-typed slot** — an `int32` write landing on the
low half of a live pointer — not a decrement of a tag. The decrement reading is already written
down elsewhere in this project and would send the next reader looking for the wrong thing.

### Reproduced

| run | exe | fault | the bad qword |
|---|---|---|---|
| lane M, stress 3 of 4 | 2026-09-07 23:29:48 | READ of `000001d9fffffffa` (= ptr-8) | `0x000001DA_00000002` |
| lane N | 2026-09-08 00:37:17 | READ of `000001d600000022` | `0x000001D6_00000022` |
| lane N, big sweep | 2026-09-08 00:50:34 | READ of `ffffffffffffffff` | `RAX=0x40000000_7FFCE0A1` |
| lane N, merged-tree verify | 2026-09-08 00:57:50 | READ of `000001ab00000002` | `0x000001AB_00000002` |

Four occurrences in roughly nine stress-route runs. Three of the four are the same shape — a
**valid heap high dword with a small integer in the low dword**, and twice that integer is
literally `2`. `[REDZONE-DUMP]` says the same thing every time: no tag corruption, free-list
forward links intact.

### Why it matters beyond the crash

It fires **after the sim**, during `GameUpdateLoop::Shutdown`, and the `[RtDiff]` summary is
printed after `Game::RunGameMain` returns (`standalone/main.cpp:1895`). So it **silently converts
a recorded cell into no cell at all**, and does it in a run that looked successful. It is a
verification blocker, not only a bug.

### THE STACK IS NOT WHERE LANE M'S SYMBOLIZATION PUT IT

Lane M reported the path as
`GameUpdateLoop_Shutdown → Tutorial_Dtor_f8ff0 → TreeNodeFree_2ce60 → MemFree → FreeLocked`,
i.e. a tutorial tree being freed through the allocator. **On a crash lane N reproduced and
symbolized against its own binary with `nm`, it is not that path at all.** Every frame below,
`nm`-resolved against `standalone/build/reimpl-x64.exe` at image base 0x140000000:

```
exe+0x345FB0  WinMain +0x20c
exe+0x1A731E  Game::RunGameMain +0xe6e
exe+0x1AEB4A  GameUpdateLoop_Shutdown +0x2f6
exe+0x1B9A1E  EntityList_Shutdown_c3d30 +0x104
exe+0x5F69    ActHunt_MarkEntityRemoval_c2ea0 +0x209
exe+0x1178FB  WorkComponent_OnRemoveVSlot +0x18
exe+0x11743F  WorkComponent_OnRemove_CancelWork +0xfe
exe+0x117746  WorkComponent_RegisterWorkSlot +0x181
exe+0x47A6    ActCt_TaskCleanup_8e30 +0x2bf     <-- FAULT
```

`GameUpdateLoop_Shutdown` is common to both readings and is presumably what made the tutorial
guess plausible; nothing below it matches. This is the **WorkComponent work-slot / task-cleanup**
path, not a tutorial tree and not `TreeNodeFree`. Lane M did not have a symbol table for the
frames it named, and its reading should not be carried forward.

### What the fault actually is: a stale BST link to a freed-and-reused node

`ActCt_TaskCleanup_8e30` (`code/Game/Action/action_base_complete.cpp:188`) walks intrusive BSTs by
reading a node's key with `int key = *task;` and descending through child links at `+0x10`/`+0x18`
/`+0x20`. **The faulting address IS the node pointer** — `READ of 000001ab00000002` with
`RAX = 000001AB00000002` — so a child link in one of those trees held `{low dword = 2, high dword
= this run's heap prefix}`.

That is not a pointer anything computed. It is what you read out of a block that **was freed and
handed to a new owner**, whose first fields are now a small integer and something else — while the
tree still links to it. Everything else agrees with that reading: the allocator's tags are intact,
its free-list forward links are intact, and the value's high half is a *valid* heap prefix. It is
also exactly the family of `reference_workarea_bst_bad_entry.md`, which closed a teardown hang and
an `IsA` EXEC crash in this same Work family and ends with the instruction *"ask what should have
ERASED an entry before what WROTE it"* — and `CheckPtr` is a bare `RET` in every shipped build, so
nothing validates the handle on the way through.

Reaching it during **entity teardown** is the aggravating condition: `EntityList_Shutdown` is
removing entities while `RegisterWorkSlot` looks the citizen's id back up in the task manager's
trees (`s_fn_CitizenAI_8e30(gameWorld+0x118, key)`, `workcomponent.cpp:310`), so a node an earlier
teardown step freed can still be linked.

### THE FREE/ERASE ENUMERATION (source + disassembly, no runs)

Per `reference_workarea_bst_bad_entry.md`'s closing rule — *ask what should have ERASED an
entry before asking what WROTE it* — here is every tree the work scheduler
(`*(gameWorld+0x118)`) maintains for a posted work item, with its insert, its erase, and the
key each uses. Insert is `CitizenAI_WorkScheduler_Insert` (retail 0x1400F88A0); erase is
`ActCt_TaskCleanup_8e30` (retail 0x1400F8E30), which is the function that faulted.

| # | Tree | Node | Inserted by | Erased by | **Key used at erase** |
|---|---|---|---|---|---|
| 1 | `sched+0x10` primary AVL (queue order) | 0x28 | 88a0, inline | 8e30 step 3 via `a6e0` | `*(workObj+0x24)` — the work's PRIORITY, re-read at erase |
| 2 | `sched+0x30` id map | 0x30 | 88a0 via `9a40` | 8e30 step 5 via `ca60` | `taskIdx` (the parameter) |
| 3 | `sched+0x40 + i*0x10`, i<14 — category sub-trees | 0x30 | 88a0, inline, per `code` bit | 8e30 step 2 via `ca60`, all 14 unconditionally | `taskIdx` |
| 4 | `sched+0x120` profession buckets | 0x38 | 88a0 via `cb50`→`ccf0` | **never erased — buckets are created and never removed** | — |
| 5 | `bucket+0x08` inner id tree | 0x30 | 88a0, inline | 8e30 step 4 via `ca60`; also `Rekey` step 4 via `ca60` | `slot50(workObj)` to find the bucket, then `taskIdx` |

Every free of a node in 1, 2, 3 and 5 goes through `ca60`/`a6e0`, which unlink before they
free — checked line by line in `WorkCellBST_Remove_ca60`
(`code/Game/Work/work_base_cellbst.cpp:171`), including the two-child in-order-predecessor
splice. The only bulk free (`e320`, which frees a subtree WITHOUT unlinking it from a
parent) is used in this family in exactly one place — `WS_cb50` freeing its own stack
temporary's deep copy — and every call site in our tree passes a null subtree, so it never
runs. **No free path in this family frees a node that is still linked.**

**So the leak is not a free-without-unlink. It is an ERASE THAT SILENTLY DOES NOTHING while
the destroy that follows it happens anyway.** Rows 1 and 5 are the exposed ones, because
they are the only two whose erase key is **re-read out of the work object at erase time**
rather than passed in:

* Row 5: step 4 finds the bucket by `slot50(workObj)` — the work's own "which profession can
  do this" getter. Step 5 then runs `owner vtable +0x48`, `+0x40`, `DeletePtr(task+0x8)` and
  `ca60(sched+0x30, task)` **unconditionally, with no dependence on step 4 having found
  anything.** If the key has drifted since the post filed it, the entry stays in the old
  bucket and its payload becomes a freed work object one instruction later.
* Row 1 has the same shape with `*(workObj+0x24)`.

That the key CAN drift is not speculation — `CitizenAI_WorkScheduler_Rekey` exists solely to
move an item between buckets when a building changes profession, and it is called from
exactly two sites (0x14012F520 demolition/upgrade → Builder, 0x14012F670 completion → natural
profession). Any other change of a work's profession key, or any read of `slot50` through a
component that teardown has already freed, leaves the entry filed under a key nobody will
look under again.

### RETAIL DOES NOT TOLERATE HALF OF THAT, AND THIS PORT DOES

Retail 0x1400F8F2D-0x1400F8F5E, read this lane:

```
1400f8f2d  mov  rcx,[r14+0x120]     ; bucket tree root
1400f8f34  test rcx,rcx / je f8f5c  ; empty          -->
1400f8f40  ...search by slot50 key...
1400f8f5a  jne  f8f40               ; not found, falls through -->
1400f8f5c  xor  ecx,ecx             ;   rcx = 0
1400f8f5e  mov  rdx,[rcx+0x8]       ;   *** READ OF ADDRESS 0x8 — a hard fault ***
```

**Retail asserts that a work's own slot-10 key always names an existing bucket, and dies
immediately and loudly if it does not.** Our port has an `if (node != nullptr)` guard there
and continues. Note the asymmetry, which matters: retail *does* tolerate the entry being
absent from a bucket that exists (`test rdx,rdx / je f8f90`), so only the missing-BUCKET case
is retail-fatal.

That guard should **not** simply be deleted — per CLAUDE.md, removing it would move the crash
rather than fix it, and the deref is faithful only in the sense that retail relies on an
invariant we may be breaking upstream. What was missing is that **nobody has ever measured
whether the invariant holds in our build.** A probe now does, and changes no behaviour:

`BANISHED_WORKSCHED_PROBE=1` (`code/Game/Action/action_base_complete.cpp`) counts, per 8e30
call, and reports at exit:

| counter | meaning |
|---|---|
| `unfiled` | healthy — the entry was found and erased |
| `entryNotFound` | retail-tolerated, but **this is the dangling-entry event**: step 5 destroys the work object anyway |
| `bucketNotFound` | **retail would have FAULTED at 0x1400f8f5e** |
| `bucketRootNull` | same |

The prediction this makes is falsifiable: **if `entryNotFound` and both fatal counters are
zero across a stress run, Finding 3 is NOT this mechanism** and the enumeration above has
eliminated the whole family rather than found the culprit. That is a result either way, and
it needs one run.

### Eliminated, with the evidence

* **Work-id reuse.** `CitizenAI_WorkScheduler_Insert` allocates ids from a monotonic counter
  that wraps only at 0x7FFFFFFE (`sched[0] += 1; id = sched[0];`). `workComp+0x28` is never
  reset when work completes — so a component does carry a stale id forever, and `ead0`'s
  id-map lookup is the engine's liveness test — but with no realistic reuse inside a
  1,200-frame run, a stale id cannot alias someone else's live item.
* **Teardown ordering.** `GameUpdateLoop_Shutdown` calls `EntityList::Shutdown`
  (`gameupdateloop.cpp:2933`) *before* the "free/destroy all subsystems" block that destroys
  `m_game`, so the scheduler is alive for the whole mark pass. The scheduler is not being
  dereferenced after its own destruction.
* **`TreeNodeFree_2ce60` / the tutorial tree**, which lane M's symbolization implicated:
  checked instruction by instruction against retail 0x14002CE60 and faithful, and not on the
  real stack anyway.
* **`ca60`'s AVL erase**, including its two-child predecessor splice: no path frees a node
  while a link to it survives.
* **The child-offset family.** All five walkers of the 0x30 node layout agree: lesser child
  at +0x18, greater at +0x20. One *comment* disagreed — `WorkComponent_CheckWorkSlot`'s
  header labelled them the wrong way round while its code was correct — and that is corrected
  here, because this family has now paid three times for a child-offset or slot mix-up.
* **`abi_retslot_audit.py`**: clean, 0 suspicious of 48 by-value-returning exports.

### MEASURED, on runs that actually crashed

Everything above this point was reasoning. This section is measurement, and three of its four
results are negative — recorded with the same weight a hit would have had.

**1. THE CRASH IS ORACLE-INDEPENDENT.** The stress cell was run with `BANISHED_RT_DIFF`
**unset** (exe 2026-09-08 01:29:23) and faulted anyway: `rc=3`, ACCESS_VIOLATION. Finding 3 is a
real defect in the port, not an artifact of the diff harness. The intermittency is unchanged at
roughly 4 runs in 10, so the oracle is not what makes it fire either.

**2. THE SCHEDULER ERASE FAMILY IS ELIMINATED — on a crashing run.** The probe's tally, from the
run that died:

```
[WorkSchedProbe] TALLY 8e30calls=136 unfiled=135 entryNotFound=0 bucketNotFound=0 bucketRootNull=0
```

135 of 136 calls found their entry and erased it; the 136th is the call that faulted mid-walk.
**Not one call reached either state retail 0x1400f8f5e would have faulted on, and not one erase
no-opped.** The mechanism this lane derived from source — a drifted key leaving an entry behind
while step 5 destroys the object anyway — **does not occur**. The enumeration ruled the family
out rather than finding the culprit, which is what it was built to be able to say.

**3. THE INTERNALFACTORY HANDLE TABLE IS CLEAN THROUGH TEARDOWN.** `ObjAudit`'s only scan point
was inside `Update`, so the table had **never** been checked during teardown; checkpoints were
added bracketing `EntityList::Shutdown`. On the crashing run: `shutdown-enter: count=92492 bad=0`,
`freed-while-registered total=0`. So mode (A) of `Object/internal_factory_audit.h` — *"the next
~Internal writes `*(int*)(freedPtr+0x08) = idx`, a 4-byte store into a reused block"*, which has
exactly this crash's signature — **did not happen**.

**4. THE DISCRIMINATING RESULT: THE HEAP IS CLEAN WHEN TEARDOWN BEGINS.** Two readings were
equally consistent with the evidence — *one latent wild write laid down during the sim, with a
moving victim*, versus *faults created by teardown itself*. They differ in one observable, so
`RedzoneAlloc_HeapDump` (which walks the engine allocator's boundary tags and works on a fully
native run) was called at `shutdown-enter`. On the crashing run:

```
[REDZONE-DUMP] 'shutdown-enter': ... no tag corruption — validating free-list LINKS...
[REDZONE-DUMP] free-list forward links also intact
```

**The heap is intact when teardown starts and corrupt by the time it faults.** That rules out the
latent-sim-write reading. The corruption is produced *by the teardown cascade*, in the window
between the 135th and 136th `8e30` call.

### A CORRECTION I OWE LANE M

Earlier this lane reported, on one symbolized crash, that lane M's stack
(`Tutorial_Dtor_f8ff0 → TreeNodeFree`) was wrong and that "only the first frame survives."
**That was itself wrong, and it was wrong because it generalised from a single sample.** A later
crash captured here symbolizes to exactly lane M's path:

```
GameUpdateLoop_Shutdown +0x615 -> Tutorial_Dtor_f8ff0 +0x330
  -> TreeNodeFree_2df90 (recursive: +0x50 / +0x5f / +0x7f / +0x5f)
```

(`2df90`, not `2ce60` — same recursive-free shape, a sibling function.) Lane M's reading was
right about this fault. **There are at least THREE distinct faulting stacks on this route**, and
they must not be assumed to be one defect:

| # | Stack | Corrupt value |
|---|---|---|
| A | `EntityList_Shutdown +0x104` (mark loop) → `WorkComponent::OnRemove` → `RegisterWorkSlot` → `ActCt_TaskCleanup_8e30`, faulting on `int k = *e` in step 4's entry walk | `{low = 2 or 0x22, high = that run's heap prefix}` |
| B | `EntityList_Shutdown +0x22e` (drain loop) → `RemoveEntity` → `ComponentEntity_Dtor` → `DtorBody` → `InternalBasePtr::DeletePtr` → `Inert_ScalarDtor` → `MemFree` → `FreeLocked` | boundary tag `0x0000000200000010` |
| C | `Tutorial_Dtor_f8ff0 +0x330` → recursive `TreeNodeFree_2df90` | node ptr `0x0000000200000020` |

A and C are both **a pointer whose low dword was overwritten with a small integer** — for A the
high half is provably that run's heap prefix (`0x279`, matching `RSI=0x279AD63A7B0`). B is a
boundary tag, and `0x200000010` is *not* a valid tag: the allocator stores the block size in the
low 41 bits (`blkBytes = (tag & 0x1ffffffffff) * 4`, `redzone_alloc.cpp:867`), so a 16-byte block
would tag as `4`, not `0x200000010`. So all three are genuine corruption, not misread metadata,
and the "free-block tag" reading this lane briefly entertained is **wrong and withdrawn**.

### A DEFECT IN THIS LANE'S OWN PROBE, AND THE RULE IT PRODUCED

The probe's first version registered its tally with `atexit`. The crash handler ends in
`ExitProcess`, which skips `atexit` entirely — so on the one run where the probe fired, it printed
its `ARMED` line and then **nothing at all**, and the all-counters-zero outcome would have been
indistinguishable from *"the probe is not in this binary."*

**A diagnostic whose output is destroyed by the event it diagnoses is not a diagnostic.** Both
tallies now print from the crash handler as well, beside `VideoDX11::Dx11SeamReport()`, which had
been doing exactly this since Wave 6 for exactly this reason. The `ARMED` line is what exposed it:
without a startup marker the silence would have been read as a clean pass.

### THE WRITER HUNT — built, and it did not converge. Here is exactly why.

**This section exists so nobody spends another session rediscovering these three things.**

**1. WHY DR0-DR3 CANNOT BE USED ON THIS VICTIM.** A hardware watchpoint needs an *address*.
This victim's address is not knowable until after it has been written: it is a different block, in
a different profession bucket, at a different heap offset on every run. The precedent in
`entitylist.cpp:498` — arm after the first detection and catch the *next* store — does **not**
transfer, and its own comment says why it worked there: *"the same victim gets re-corrupted on
later frames (observed three times on one node)"*. This victim is not re-corrupted; it faults
immediately on first use. The arm-on-detection path was built anyway (`BANISHED_WSWATCH`, which
validates the whole inner tree before any dereference and arms on the bad slot) and it never got
the chance to fire: on the one crashing run under it, all 205 `8e30` calls completed and the fault
was in a different stack.

**2. THE BRACKET WAS THE RIGHT INSTRUMENT AND IT WENT QUIET.**
`RedzoneAlloc_FindCorruptBlockNative()` (new, shim-independent, silent, returns the first corrupt
block) is called after every *n* teardown steps by `BANISHED_TEARDOWN_BRACKET=n`, so the first
step whose scan comes back dirty names the entity that wrote. It demonstrably works — it reports
`heap at entry is clean` and then stays quiet. But:

| binary | config | runs | faults |
|---|---|---:|---:|
| 02:05:47 | `WSWATCH=1`, no bracket | 1 | **1** |
| 02:09:31 | `TEARDOWN_BRACKET=16` | 10 | **0** |
| 02:09:31 | bracket **off** | 6 | **0** |

**Sixteen consecutive clean runs on the current binary**, against a fault that had been arriving
roughly 4 runs in 10 all evening.

**3. WHAT THAT DOES AND DOES NOT MEAN — including a correction to this lane's own first reading.**
The first reading reported upward was "my instrument suppresses it, p ≈ 0.006". **The control run
refutes half of that and the p-value was overstated; both are corrected here.** The fault does not
reproduce with the bracket *off* either, so it is not the scan's cost that silences it. And the
p-value assumed a 40% rate measured across a *mix* of builds and env combinations — this lane
never established a controlled baseline on one fixed binary, so no rigorous probability can be put
on sixteen clean runs. It is strong, informal evidence and nothing more.

What *is* solid:

* **The lanes E/H merge did not fix it.** Build 02:05:47 already contained that merge and faulted
  on its first run. That candidate is eliminated.
* The only change between 02:05:47 and 02:09:31 is the bracket code itself — which performs **no
  writes at all** and is inert when its env var is unset. It cannot be a fix. What it does change
  is the code and stack layout of `EntityList_Shutdown_c3d30` and the timing of the cascade.
* So the best-supported reading is a **layout- or timing-sensitive fault**, which is also the only
  reading that comfortably explains the rest of the evidence: intermittency, three different
  victims, and one write shape landing sometimes on a pointer's low half and sometimes on a
  boundary tag's high half.

**A RACE IS THE BEST-SUPPORTED DIRECTION, AND IT IS A DIRECTION, NOT A CLAIM.** A fault that moves
under a purely read-only perturbation is timing-dependent; the engine runs worker threads and the
allocator has its own critical section. This lane did not test it and is not asserting it.

**DO NOT CONCLUDE FROM A QUIET BRACKET THAT THIS IS FIXED.** It is not fixed. Nothing was changed
that could fix it.

### THE STRONGEST REMAINING LEAD — and it needs no directory and no run

The tag-corruption variant decodes exactly, and the decode changes what to look for.

On the 02:05:47 fault: `RAX = RDX = 0x0000000200000014`, `RBX = 0x000001273918D110`, and the
faulting read was at `0x0000012F3918D160`. The allocator advances by
`blkBytes = (tag & 0x1ffffffffff) * 4` (`redzone_alloc.cpp:867`), and

```
0x1273918D110 + (0x200000014 * 4) = 0x1273918D110 + 0x800000050 = 0x12F3918D160   ← the fault address
```

exactly. So the allocator read that qword **as a tag**, and therefore:

> **The LOW dword (`0x14` = a 0x50-byte block, `0x10` = 0x40, `0x20` = 0x80) is a LEGITIMATE size
> field. It is the HIGH dword that was overwritten with 2. That variant is a 4-BYTE STORE AT
> `tag+4`.**

This lane's earlier "the tag reads `{size, 2}` and is therefore a free-block header" reading is
**wrong and withdrawn** — a free block's tag would carry its size in the low 41 bits, which it
does; nothing about it is a separate free header.

A 4-byte store at `tag+4` is precisely the family of
`reference_load_wild_write_high_dword_7fc.md`: **a `System::String` / `InternalBasePtr` release
performing `dec [p-4]` on a pointer that is not a refcounted String buffer.** For a user pointer
`p` whose boundary tag sits at `p-8`, `p-4` *is* that tag's high dword. That reference records the
same mechanism being found and closed once already on the load path; this would be a second
instance, on the teardown path.

**That is a SOURCE hunt** — find release-idiom call sites reachable from the teardown cascade that
can receive a non-String pointer — and it needs neither the game directory nor a run. It is the
first thing the next lane should do.

### Where it stands

**Reproduced (7 times now), oracle-independence proved, the heap proved clean at teardown entry,
the scheduler family and the handle table both eliminated by measurement on crashing runs, the
victim located to a single instruction — and the WRITER not found.**

What is honestly open: the writer. Three separate hypotheses have now been *measured and
eliminated* rather than argued away, the search space is much smaller, and the next tool is
named below — but nothing here identifies the code that stores the small integer.

The remaining candidate the evidence points at is a write performed *during the teardown
cascade* that lands on a live pointer's low half. The obvious tool — a **hardware write
watchpoint** (`standalone/hw_watch.cpp` exists for exactly this) — was built and **does not
apply here**; the section above records why, so that nobody rebuilds it. The instrument that
*did* work is the per-step heap bracket, and its result is written up there too, including the
correction to this lane's own first reading of it.

Two things worth carrying forward for whoever does:
* the fault is **not** latent from the sim (result 4), so the watchpoint only needs to cover
  teardown, which is a few hundred milliseconds rather than 1,200 frames;
* stacks A and C are the same shape (a pointer's low dword replaced by a small integer) while B
  is a boundary tag, so **B may well be a different defect** — the coordinator's warning against
  assuming one root cause is not resolved here, and this lane's data does not settle it.

Per CLAUDE.md this is not to be closed with a null check on the child link, and the existing
`if (node != nullptr)` guard was deliberately **left in place**: retail's unguarded deref there
is load-bearing, so deleting the guard would only move the crash to a different line without
restoring retail's behaviour. The upstream question — what makes the key drift — stays open.

Note also the corollary for cutover: **row 4's buckets are never erased at all**, by either
implementation. That is retail's design, not a defect, but it means the profession-bucket tree
only ever grows, and any lane that mints or serialises this structure should know it.

**It is also a verification blocker, and that has now bitten this lane directly:** the merged-tree
confirmation run below produced its `[PickSweep]` line and then died at shutdown, so it has **no
`[RtDiff]` summary at all** despite the sweep having completed cleanly. A cell can be destroyed
after it has succeeded.
