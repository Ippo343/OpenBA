# Wave 9 lane N — the three findings lane M left open

Lane N owns three of [WAVE9_LANE_M.md](WAVE9_LANE_M.md)'s findings. All three are things that
become **unfindable after lane G removes LoadLibrary**: two are instruments, one is a defect
that only the shipped DLL's presence makes visible.

| Finding | Verdict |
|---|---|
| **1** — `UI::NumberElement::SetValueInt` diverges on tutorials 1-3 | **CLOSED.** An ORACLE defect, diagnosed by lane M and FIXED here: the two `NumberElement` setters and `NumberEditElement::SetMinMax` are now NO-ORACLE ports. Layout corrected: `+0xD0` is an owning refcounted `System::String` and the object is **0xE8** bytes, not 0xE0. |
| **2** — `Picking::Scene::Intersect` diverges on the stress route | **CLOSED, and it was a REAL PORT DEFECT.** A moved term: the inverse transform's translation was folded into `localOy`/`localOz` FIRST instead of added LAST. Fixed and proved on a deterministic 244,824-ray repro — 21 divergences -> 0 with the denominator unchanged, and a negative control that fires at 7,520/7,536. |
| **3** — intermittent shutdown crash in the allocator | **REPRODUCED and re-characterised; writer NOT found.** Lane M's arithmetic is corrected here: it is a small integer written over a pointer's LOW half, not a decremented boundary tag. |

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

## Finding 3 — reproduced, arithmetic corrected, writer not found

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

| | lane M | lane N |
|---|---|---|
| exe | 2026-09-07 23:29:48 | 2026-09-08 00:37:17 |
| rate | 1 of 4 stress runs | 1 of 3 stress-route runs |
| fault | READ of `000001d9fffffffa` | READ of `000001d600000022` |
| faulting module | `Runtime-x64.dll + 0xC6C5F` (the DLL allocator) | `reimpl-x64.exe + 0x5E7D` |
| redzone | no tag corruption, forward links intact | **identical wording** |

Both land in free-block bookkeeping with intact tags and intact forward links, which puts the
corruption in a **prev link or a bucket head** — i.e. a write into a block *after* it was freed,
not a mis-sized allocation.

### Why it matters beyond the crash

It fires **after the sim**, during `GameUpdateLoop::Shutdown`, and the `[RtDiff]` summary is
printed after `Game::RunGameMain` returns (`standalone/main.cpp:1895`). So it **silently converts
a recorded cell into no cell at all**, and does it in a run that looked successful. It is a
verification blocker, not only a bug.

### Not found

The writer. `tools/abi_retslot_audit.py` is clean (0 suspicious of 48 by-value-returning exports),
so the hidden-return-slot family is not it. `TreeNodeFree_2ce60` was checked against retail
0x14002CE60 instruction by instruction and is faithful, and the node layout it walks is confirmed
by the node copy-constructor at 0x14002CEB0 (`+0x00` key, `+0x08`/`+0x10` sub-tree {root,count},
`+0x18` parent, `+0x20`/`+0x28` children, `+0x30` int32) — so the "one free routine, two node
layouts" family is not it either.

**Reproduced, characterised, writer not found.** Per CLAUDE.md this is not to be closed with a
null check: the freed block is being written by something, and a guard at the free site would move
the crash rather than fix it. The open experiment is a stress A/B with `BANISHED_RT_DIFF` unset
against set — the oracle itself performs use-after-free *writes* on some routes (Finding 1 is one),
so whether the crash rate depends on the oracle is the cheapest next discriminator.
