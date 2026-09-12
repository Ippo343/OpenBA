# WAVE 29 LANE GFXBODY — Graphics' eight remaining BODY rows

Branch `wave29/gfxbody`, cut from `wave29/base` at `c11511a1`.
Source: `runtime/src/graphics/wave29_lane_gfxbody.cpp`.
Partner: lane **SLOTS** (closed at `wave29/slots caa5ff11`).

Written as the lane ran, not at the end.

---

## 0. THE HEADLINE, AND WHAT IT IS SCORED ON

**Four of Graphics' eight remaining BODY rows are ported and registered. Four
are declined, and all four declinations are SIZE JUDGEMENTS stated as such.**

The identity this lane is scored on — not a percentage:

> Four named Graphics `BODY` rows that were `called` and `ported==0` on
> `wave29/base` are now ported and registered, each with its written-operand set
> checked against retail's over a DERIVED extent. One unexported `Array::SetSize`
> instantiation that would otherwise have been an address-door residual is
> proved identical to a known ported template and registered. Wave 28's R6 is
> closed; **Wave 28's R5 is shown to have been STALE ON ARRIVAL** (§2.4.1).
> **All four bodies run ZERO times in a live world-build run, for a structural
> reason this lane traced to the end** (§9.2.1).

Measured: Graphics **108 → 112 ported**, `called` unchanged at 132, BODY gap
**8 → 4**, ceiling unchanged at 97.0%. TOTAL 1453/1294 → 1454/1299.

**Do not read that as progress toward 100%.** `ceiling = (called - unpromotable)
/ called` has `called` in both halves. This lane barely grows `called` (+1, and
that row arrives already ported), which is the expected signature for a lane
porting rows already in the column — the mirror image of lane SLOTS' `called +4,
ported +0`, which is equally correct for its subject. **Score on the identity
above, not on 89.3%.**

---

## 1. A WORKTREE DEFECT THAT WOULD HAVE INVALIDATED EVERYTHING — FIRST FINDING

**My worktree was not on `wave29/base`.** It was checked out on a branch named
`worktree-agent-a1f91b45857490164` at commit `1afa558e`, a *Wave 21-era* tree
whose `CLAUDE.md` still said "Status 2026-09-09 … the UI namespace is FINISHED"
and which **did not contain `docs/runtime_port/WAVE29_KICKOFF.md` at all**.

The tell was that the kickoff file the brief told me to read "IN FULL" did not
exist. Had I instead started from the brief's symbol list — which is complete
enough to work from — I would have ported four bodies onto an eight-wave-old
tree, against type headers that predate `AnimationClipPrefix`, and the diff
would have been unmergeable in a way that looks like a merge conflict rather
than like a wrong base.

Fixed by `git checkout -b wave29/gfxbody wave29/base` before anything else.

**Recorded as a defect rather than as my own slip because the harness reported
the worktree as ready and `git status` reported it CLEAN.** A clean status on
the wrong base is indistinguishable from a clean status on the right one. This
is adjacent to `reference_worktree_reassignment_crosses_lanes.md` (a lane's
worktree re-pointed onto another lane's checkout) but is a distinct instance:
the worktree was pointed at nothing in this wave at all.

**ADDRESSEE: the Wave 30 kickoff author.** The cheap fix is one line in §0.1:
*"before anything else, run `git log --oneline -1` and confirm it is the wave
base commit named at the top of this file."* Direction of the error: **silent**,
and it produces work that cannot be merged rather than work that is wrong.

---

## 2. RE-DERIVATION — WHAT I RE-RAN AND WHAT REPRODUCED

Kickoff warning 1. Every figure I was handed was re-derived.

### 2.1 The subject list — REPRODUCED EXACTLY

```
$ python tools/runtime_census.py --unported --namespace Graphics
Graphics   599 total   132 called   108 ported   81.8%   ceiling 97.0%
  Graphics gap 24 |  BODY 8  VTABLE 8  DATA 8
```

The brief's eight symbols are exactly the eight `BODY` rows the census prints.
No correction.

### 2.2 THE ONE FIGURE THAT DOES NOT SURVIVE CONTACT — `GetSunCamera` IS THE BIGGEST

The kickoff groups `GetSunCamera` in a "Scene/Renderer draw-item family" that
reads as the tractable half of the eight. **It is 3775 bytes — the LARGEST of
the eight, larger than `Scene::AddCulledDrawItems` (2950).**

Checked in the direction that could have made it smaller, not just measured:
`GetSunCamera` is a **single** `.pdata` row, and I verified it is the **only
export** and the **only `.pdata` begin** in `[0x180107010, 0x180107ecf)`. So the
3775 is not an adjacent-export upper bound swallowing a neighbour
(`reference_adjacent_export_is_an_upper_bound.md`) — it is one function.

### 2.3 Extents, all eight, DERIVED

| symbol | retail | ord | first .pdata | **full chain** | frags | ratio |
|---|---|---|---|---|---|---|
| `AddGlobalDrawItems@Scene@` | `0x180102330` | 1558 | 122 | **122** | 1 | 1.0x |
| `AddLight@Renderer@` | `0x1801051d0` | 1567 | 104 | **104** | 1 | 1.0x |
| `EndDrawGroup@Renderer@` | `0x180104f50` | 2309 | 108 | **108** | 1 | 1.0x |
| `GetRootPoseAtTime@Animation@` | `0x1800faaa0` | 2880 | 37 | **495** | 3 | **13.4x** |
| `CopyInstancedToUnique@AnimationState@` | `0x1800fd4c0` | 1868 | 274 | **1127** | 3 | 4.1x |
| `UpdateNodes@AnimationState@` | `0x1800fcc20` | 4522 | 126 | **2191** | 5 | **17.4x** |
| `AddCulledDrawItems@Scene@` | `0x1801017a0` | 1543 | 2950 | **2950** | 1 | 1.0x |
| `GetSunCamera@SunLight@` | `0x180107010` | 2951 | 3775 | **3775** | 1 | 1.0x |

All eight are real named exports. `AEBA`/`AEAA`/`QEBA` mean PRIVATE/CONST, not
unexported — this DLL exports its private members
(`reference_aeaa_is_private_not_unexported.md`) — so none needs an address door.

### 2.4 Wave 28's residuals R5 and R6, re-checked FIRST (warning 3)

Both were addressed to "the Wave 29 Graphics lane", i.e. to me. A blocker naming
a MISSING CAPABILITY expires; one naming an INTRINSIC property does not. These
were capability claims, so I checked them before anything else.

| # | Wave 28's claim | verdict |
|---|---|---|
| R5 | `0x1801fe728` is the named export `?_secondsPerTick@Timer@System@@0NA` (ord 4797) but the tree reaches it BY ADDRESS; "a name door is available and **nobody has taken it**" | **HALF TRUE, AND THE FALSE HALF IS THE ACTIONABLE ONE — see §2.4.1** |
| R6 | `?GetRootPoseAtTime@Animation@Graphics@@` entered `called` via Wave 28 §5 and is unported | **CONFIRMED**, and **CLOSED** — ported in §4.4 |
| R7 | `MaterialInstance::BuildParameters` has attendance 0 in a full autostart run, contradicting Wave 27's "goes live immediately" | **NOT ADDRESSED** by this lane. Stated rather than quietly dropped; see §8 |

### 2.4.1 R5 WAS STALE ON ARRIVAL — warning 3, firing on a residual that was one wave old

Wave 28 §12 recorded R5 as: *"`0x1801fe728` is the named export
`?_secondsPerTick@Timer@System@@0NA` (ord 4797), but `engine_funcs.cpp:1546`
reaches it **by address**. A name door is available and **nobody has taken it**."*

The first half is **true** and I re-confirmed it against the export directory.
**The second half was FALSE WHEN IT WAS WRITTEN.** Measured on `wave29/base`,
before I added anything:

- **Nine source files already take that exact name door** —
  `audio_scene_update.cpp:302`, `input_device_update.cpp:170`,
  `input_device_update_probe.cpp:628`, `timer.cpp:166`, `userinterface.cpp:270`,
  `userinterface_close.cpp:174`, `userinterface_frame.cpp:175`,
  `userinterface_hover.cpp:338`, `userinterface_standalone.cpp:141` — via
  `RT_DATA_SLOT`, plus `userinterface_showdialog.cpp:192` via `ResolveExport`.
- The symbol is **already MINTED**: `standalone/rt_data_mint_tables.inc:2291`.
- It is already carried in the publish manifest as **`CUTOVER_REQUIRED`**:
  `standalone/cutover_manifest.inc:75`.
- The census confirms it: `?_secondsPerTick@Timer@System@@0NA` was **already a
  `called` row** on the base tree, and my door changed `called` by **zero**.

**So R5 named a MISSING CAPABILITY that was not missing.** Per warning 3 that is
exactly the class of blocker that expires, and this one had expired before it was
written down — the residual generalised from the ONE address door its author
happened to be looking at (`engine_funcs.cpp:1546`, which is real and is still
there) to a claim about the whole tree.

**This is the third consecutive wave in which a handover's "still open, and why"
list failed on contact** (`reference_the_still_open_section_is_as_stale_as_cheap_wins.md`,
`reference_cheap_wins_list_is_the_stalest_section.md`). The cost here was small —
one grep — but the *shape* is what matters: **I had already written "R5 PARTLY
CLOSED by this lane" into the source banner and into an earlier draft of §8
before running that grep**, crediting myself with taking a door nine files had
taken already. The census `called` column is what caught it, because the row did
not move.

What remains true and unclosed is narrower: **`engine/funcs/engine_funcs.cpp:1546`
still reaches `0x1801fe728` by address.** That is one address door, not a missing
capability. See §8 R5′.

---

## 3. GREP-BEFORE-WRITE — THE CHECK THAT MATTERED, AND MY OWN FALSE POSITIVE

"Three times in one wave a lane ported what the tree ALREADY had", and a body
PORTED but not REGISTERED is indistinguishable from an ABSENT one. Mid-lane,
lane SLOTS found that `tools/mint_slot_sweep.py` **cannot see a registration
whose mangled name is split across source lines**, and that its subject read 16
unported where the truth was 7 — nine already ported, two whole Graphics vtables
10/10 COMPLETE since Wave 27 with nobody aware.

I did not re-use SLOTS' scanner. Two checks sharing a hidden premise are ONE
check (`reference_two_checks_one_hidden_premise.md`), so I wrote my own:
`<scratchpad>/lane_GFXBODY/gfxbody_regscan.py`.

**SUBJECT**: every `RegisterRuntimeExport(` / `RegisterRuntimeAddr(` /
`RT_EXPORT_ONCE(` / `RT_DATA_SLOT(` / `ResolveExport(` call site under
`runtime/ standalone/ code/ engine/ dx11/`, read from the **working tree** — not
`git show HEAD`, which SLOTS reports `mint_slot_sweep.py` uses and which
therefore cannot see uncommitted work at all.
**POPULATION**: 3260 door call sites carrying a literal, **56 of them multi-line
joins**.

**CONTROLS, SCORED SEPARATELY** (warning 9; a positive control scored on a union
hides a blind screen):

| control | result |
|---|---|
| (P1) a known MULTI-LINE registration — `?Draw@SunLight@…MHHH@Z`, split after `AEBVCamera@2@` | **FOUND**, 2 fragments joined |
| (P2) a known SINGLE-LINE registration — `?RemoveState@ParticleInstance@` | **FOUND**, 1 fragment |
| (N1) a name certainly absent — `?ThisSymbolDoesNotExist@Nowhere@@` | correctly **ABSENT** |
| (E) effectiveness: how many literals the joining actually joined | **56** — a scan that joined 0 is indistinguishable from the line-by-line scan it improves on |

Warning 7 in its exact shape: this screen looks for the ABSENCE of a
registration, so "0 hits" and a dead parser look identical. P1 is the positive
it had to be handed.

**RESULT: 0 of 8 already registered.** All eight are CALLED and only called —
`AddCulledDrawItems`, `AddGlobalDrawItems`, `AddLight`, `EndDrawGroup`,
`GetSunCamera` from `wave28_lane_partsys.cpp`; `CopyInstancedToUnique`,
`UpdateNodes` from `wave27_lane_gfxbig.cpp`; `GetRootPoseAtTime` from
`wave28_lane_gfxbody.cpp`. The kickoff's §1.2 Graphics list holds.

### 3.1 MY OWN FALSE POSITIVE, AND IT FAILED IN THE ALARMING DIRECTION

My first run reported **4 of 8 ALREADY REGISTERED** and for about ten seconds
looked exactly like SLOTS' finding reproducing in my namespace. **All four hits
were my own registrations**, written minutes earlier in
`wave29_lane_gfxbody.cpp`. The scan reads the working tree, which by then
included my work.

**A tree-reading scan run AFTER you start work cannot answer "was this already
here" without an explicit exclusion.** The clean result above is the same scan
with `GFXBODY_EXCLUDE=wave29_lane_gfxbody`.

### 3.2 THE COMMENT STRIPPER EARNED ITS PLACE

Without one, **three** of my eight would have read as registered: they appear in
`//` comments in `wave27_lane_gfxbig.cpp` and `wave28_lane_gfxbody.cpp` — in
`wave28_lane_gfxbody.cpp`'s case in a comment *listing them as NOT ported*.
`reference_uncommented_checker_manufactures_work.md` exactly: the prose most
likely to name a symbol is a comment saying it is absent.

### 3.3 THE CONVERSE OF SLOTS' DEFECT LANDS ON ME

**My own `?GetRootPoseAtTime@` registration is a 2-fragment MULTI-LINE literal**
— the mangled name does not fit one line. So the registration I just wrote is
precisely the shape `mint_slot_sweep.py` cannot see. Neither SLOTS nor I fixed
that tool: lane INSTR owns the board, and two lanes editing one scoring tool in
one wave makes the delta unattributable. **ADDRESSEE: lane INSTR** — if anything
scores this lane with `mint_slot_sweep.py` rather than the census, it will
report `GetRootPoseAtTime` unported.

---

## 4. WHAT LANDED

Kill switch **`BANISHED_RT_GFXBODY29=0`** — its own variable, deliberately not
shared with `BANISHED_RT_GFXBODY28` or `BANISHED_RT_PARTSYS`. With PARTSYS armed
and this at 0, PARTSYS's `SunLight::AddDrawItems` runs and reaches the **DLL**
for exactly these callees, which separates "the caller is wrong" from "a callee
is". Sharing a variable destroys that A/B. Default ON (`RtWaveEnabled` is a kill
switch, not an opt-in — a lane that ports behind an opt-in moves the live count
not at all).

### 4.1 `?AddGlobalDrawItems@Scene@Graphics@@` — 122 B

The "global" half of the scene: objects that skip the hash grid and are visited
every frame. `Scene.h` already documented that pairing from the container side;
this body is the visitor.

**Written-operand set: EMPTY outside its own frame.** Three memory destinations,
all register-save homes. It is `AEBA` — private const — and **the operand set
PROVES the const rather than trusting the letter in the name.**

**Zero unexported callees.** Its only call is a virtual, vtable slot 3
(`CALL QWORD PTR [RAX+0x18]`) = `AddDrawItems(const Scene&, Renderer&)`, which is
exactly the family lane SLOTS ports. No door of any kind, no residual.

**THE `0x1f0` DRAW-GROUP STRIDE IS NOW CONFIRMED THREE INDEPENDENT WAYS.** Here
as a literal `IMUL R8,R8,0x1f0`; in `EndDrawGroup` as `IMUL RCX,RCX,0x1f`
followed by `SHL RCX,4`; and by Wave 28 residual R1's independently-derived
`Array<RendererDrawGroup>::SetSize` stride. `Camera.h`'s `static_assert` agrees.
Stated because two of the three were derived in this lane without consulting the
header first.

**TWO FIELDS OF THE VISITED OBJECT ARE READ AND THIS LANE NAMES NEITHER.**
`obj->[0x30]` is AND'd against the group's `passMask`; `obj->[0x28]` is compared
`>` against `Renderer::field_48`. A `>` against a threshold is exactly what an
LOD level, a layer index and a priority share, and **I found no WRITER for
either**. Per `reference_only_a_writer_names_a_field.md` they are left unnamed
and read through byte offsets. Naming them from the read side would have been
free and wrong.

### 4.2 `?AddLight@Renderer@Graphics@@` — 104 B

Appends the light to `Renderer::drawables` and records its index in the current
scene entry's `drawableIds` — **and the order is load-bearing**: the index
recorded is the array size read BEFORE the append.

This body is a **WRITER** for `RendererSceneEntry::drawableIds`, a field
`Camera.h` had named from `Renderer::Draw`'s read side. A writer beats readers
agreeing, so that name is now on firmer evidence than it was.

**THE WRITE-WIDTH TRAP IN THIS BODY, and it is the Camera trap's shape.** The
drawable record is **sixteen** bytes and retail writes **twelve** of them:
`MOV [RSP+0x20],RBX` and `MOV [RSP+0x28],EAX`. The operand reader reports
exactly `0x20-0x2b` **with a hole at 0x2c**. `RendererDrawable::_pad0c` is never
written, and `Array::SetSize` then copies all 16 bytes, so **retail appends
UNINITIALISED STACK on every light.**

So the port declares `RendererDrawable rec;` and assigns two fields.
`RendererDrawable rec{};` would write four bytes retail leaves alone, into every
drawable, every frame. `Camera.h` already carries that note for the sibling
draw-item array — **this instance was found mechanically by the operand reader
before the header was consulted**, which is the point of running the screen.

**A CORRECTION THIS LANE MADE TO ITS OWN FIRST READING.** The int stored beside
the `Light*` comes from `[RDI+0x38]` (`Renderer::field_38`, at `0x180105211`),
**not** `[RDI+0x48]`. My first pass recorded `+0x48` because `+0x48` IS read two
instructions away in the sibling body above and I conflated them. Caught by
re-reading the listing while writing the banner. `Camera.h` says of `field_38`
that `scene_watch.h:73` reads it as "draw groups used this frame" and that lane F
"neither confirmed nor contradicted" it; this body is a reader that stores it
per-light, which is **consistent with but does not settle** that name. Left as
`field_38`.

### 4.3 `?EndDrawGroup@Renderer@Graphics@@` — 108 B

Sorts each of the current draw group's three per-pass item lists, then makes the
argument the current group.

**Written-operand set: EXACTLY `rdi+0x34..0x37`.** Four bytes, one field,
`Renderer::currentDrawGroup`, and nothing else on `this`. On a body this small
that is the whole value of the discipline: it converts "it probably just sets
the index" into a measurement.

Two details a tidy port loses, both kept:
1. `currentDrawGroup` is **re-read inside the loop** and only assigned after it.
   Hoisting is equivalent *only* if the sort cannot reach back into the
   `Renderer` — an assumption about a 0x2306-byte unexported callee I have not
   verified, so I did not take it.
2. Retail tests the count **twice** (`TEST EDX,EDX / JLE` then `CMP EDX,1 / JLE`).
   The first is redundant. Folding it is a judgement that they are equivalent;
   keeping it costs nothing.

The `(idx*0x1f + 0x1c + i) << 4` arithmetic lands on `group + 0x1c0 + i*0x10` =
`RendererDrawGroup::items[i]`, and the sort's element stride is `0x38` =
`sizeof(RendererDrawItem)`. **Neither number was taken from `Camera.h`**; both
were read out of the two bodies and then found to agree with it.

### 4.4 `?GetRootPoseAtTime@Animation@Graphics@@` — 495 B, 3 fragments — CLOSES R6

Samples the ROOT node's translation from the clip's key array at the first
`Time`, and scales it by the second. **The returned rotation is the identity
quaternion, always.**

**HIDDEN RDX RETURN SLOT**, read out of the prologue and not assumed:
`RCX=this, RDX=&ret, R8=&t0, R9=&t1`, and the epilogue does `MOV RAX,RDI` at
`0x1800fac25` where RDI was RDX — it *returns* the slot. An `fn_*` typedef that
omits it is a wild write that still returns the right answer. Lane PARTSYS
already spells its neighbour `GetSunCamera` correctly
(`wave28_lane_partsys.cpp:1042`) and this file matches that shape.

**Written-operand set: EXACTLY `rdi+0x00..0x1b`** — 0x1c bytes, the whole
`Math::Transform`, nothing else. That independently confirms
`sizeof(Math::Transform) == 0x1c` and, more usefully, that the body **fully
initialises its hidden return**, so there is no unwritten window an oracle must
avoid comparing (`reference_oracle_must_not_compare_unwritten_bytes.md`).

**THE MOVED-TERM HAZARD IS LIVE HERE, TWICE**, and it is algebraically invisible:
1. Both time conversions **narrow to float BEFORE** the scale: `CVTSI2SD`,
   `MULSD` by `_secondsPerTick` (a **double**), `CVTPD2PS`, and only then `MULSS`
   by the rate. Doing the chain in double and narrowing once is a different
   number.
2. The interpolation is `a + (b - a) * t`, in that association (`SUBSS`,
   `MULSS`, `ADDSS` per component). **Not** `a*(1-t) + b*t`, which is the same
   value in real arithmetic and not in float.

**THE FRACTION IS TAKEN FROM THE UNCLAMPED INDEX.** `esi` is `(int)keyPos`
computed before any clamp or wrap; the clamp/wrap write `r9d`/`r11d` and leave
`esi` alone; the fraction at `0x1800fab83` is `keyPos - (float)esi`, the RAW
truncation. Using the wrapped index there is a one-line error that produces a
working animation with wrong timing at the loop seam.

**THE NaN ARM.** `UCOMISS / JP / JE`: `JP` goes to the LERP arm when unordered,
`JE` to the single arm when equal. So a NaN fraction takes LERP. `frac == 0.0f`
is false for NaN in C++, so the ordinary spelling reproduces both arms with no
special case — recorded because it *looks* like an omission.

#### 4.4.1 THREE PREVIOUSLY-UNPINNED `AnimationClipPrefix` FIELDS GET A READER — and one NAME TENSION I REPORT RATHER THAN RESOLVE

`AnimationClipPrefix` (`AnimationState.h:112`) was built by Wave 27 from
`AnimationState::Update`'s reads and confirmed by Wave 28 from `ApplyEvents`.
This is a **third** reader:

| offset | header today | what THIS body does with it |
|---|---|---|
| `+0x40` `loopFrame` | named | `((loopFrame-frameFirst)/(frameLast-frameFirst)) * [0x50]` — the **same three-term expression** `ApplyEvents` uses. Third independent agreement. |
| `+0x3c` `looping` | named | exactly as documented — zero CLAMPS to the last key, non-zero WRAPS |
| `+0x44` `_unpinned_44` | **unpinned** | a FLOAT; seconds are multiplied by it to get a key position. Structurally **keys per second** |
| `+0x4c` `_unpinned_4c` | **unpinned** | an INT multiplied by a key index *before* a `*0x1c` stride, i.e. **elements per key** |
| `+0x60` | beyond the 0x60 prefix | the key array base; elements are `0x1c` = a `Math::Transform`; the body reads `+0x10/+0x14/+0x18` of element `keyIndex * [0x4c]` — node 0's TRANSLATION. That is why it is the *root* pose |

**I did not edit the header.** `+0x44` and `+0x4c` have exactly one reader each —
mine. `+0x4c` is a stride multiplier rather than a sign test, so it is a strong
structural reading, but `reference_only_a_writer_names_a_field.md` says a reader
does not name a field and I have no writer for either.

**AND A NAME TENSION I AM REPORTING RATHER THAN SILENTLY RESOLVING.**
`AnimationState.h` calls `+0x50` **`frameRate`**. In this body it is used as the
**exclusive upper bound on a key index** (`CMP ESI,R8D / JL`) and as the modulus
base for the loop wrap — i.e. as a key **COUNT**. Both readings scale
frame→index identically and every existing reader is consistent with either.
**No writer has been found, so the header is left as it is and the tension is
recorded here.** Reconciling it by picking the reading that suits my body would
be exactly the move `reference_a_reconciliation_retires_the_flag.md` warns about.

**ADDRESSEE for `+0x44`, `+0x4c` and the `+0x50` tension: the next lane that
ports `AnimationState::UpdateNodes` or `CopyInstancedToUnique`** — both are
members of this family, both are in §5 below, and a WRITER for these fields is
most likely in the clip *loader* (`Animation::Serialize`), which no lane has
touched.

### 4.5 `0x180063a80` — AN ADDRESS-DOOR RESIDUAL CONVERTED INTO A PORTED ROW

`AddLight`'s second callee is unexported. Rather than leave it an address door,
I compared it against the known `kSpec_Fill16` instantiation at `0x18004f790`
under **position-independent normalisation** (warning 15 — a raw byte diff of
two copies of one function reports ~23% different because rel32/RIP fields
encode distances):

```
POSITIVE CONTROL  0x18004f790 vs ITSELF          79/79 identical, 0 differing
NEGATIVE CONTROL  0x18004f790 vs AddLight         0/79 identical, 79 differing
SUBJECT           0x180063a80 vs 0x18004f790     79/79 IDENTICAL, 0 differing
```

Both are 262-byte `flags 0x2` `.pdata` rows. Registered in
`runtime/src/collection/array.cpp` as `J3_ENTRY3(180063a80, kSpec_Fill16)` plus
its `RegisterRuntimeAddr`.

**This matters at cutover and not before.** Unregistered, `0x180063a80` resolves
to nothing with the DLL gone and `AddLight` silently fails to append the light —
the same class as Wave 28's R1, which is "not a silent artefact: nothing
renders".

`kSpec_Fill16`'s `Assign16` copies all 16 bytes including `_pad0c`, which is
**faithful**: retail's `SetSize` copies the whole element regardless of the
caller having written only 12.

---

## 5. WHAT DID NOT LAND — FOUR SIZE JUDGEMENTS, SAID PLAINLY

**These four are NOT ported. Declining a body on SIZE is legitimate and I am
stating it as a size judgement, not dressing it as a blocker.** Five ported with
defensible oracles beats eight ported on hope; four ported with defensible
operand sets beats eight ported on hope by the same argument.

| symbol | bytes | why not |
|---|---|---|
| `?GetSunCamera@SunLight@Graphics@@` | **3775** | **SIZE.** The largest of the eight. See §2.2 — the kickoff's framing implies it is tractable and it is not. Returns `Camera` by value (hidden RDX slot) and builds the sun shadow camera. |
| `?AddCulledDrawItems@Scene@Graphics@@` | **2950** | **SIZE.** Frustum cull + draw-item emit over the scene quadtree. Wave 28 §7.1 said it "should lead the next Graphics lane's scope"; it did not fit beside four smaller ones and I would rather say so than half-land it. |
| `?UpdateNodes@AnimationState@Graphics@@` | **2191** | **SIZE**, and the reason Wave 28 gave still holds and is INTRINSIC, not a missing capability: dominated by a fully inlined `Math::Transform` composition with operand order scrambled by the scheduler. A wrong sign on one term of a quaternion product produces plausible-looking animation. |
| `?CopyInstancedToUnique@AnimationState@Graphics@@` | **1127** | **SIZE**, same intrinsic reason. |

**The obstacle class for all four is SIZE — a budget fact about this lane, which
expires the moment a lane is scoped on them.** For `UpdateNodes` and
`CopyInstancedToUnique` there is *additionally* an intrinsic hazard (the inlined
quaternion composition) which does **not** expire and which any future lane must
budget verification time for, not just porting time.

**What is already measured about them and should NOT be re-derived**: Wave 28
§7's notes (the packed-handle fifth argument, group stride `0x30`, entry stride
`0x48`, output stride `0x38`, input `0x1c`, node array `0x48`) plus this lane's
extents in §2.3. Wave 28 recorded that it did **not** look up
`CopyInstancedToUnique`'s callees `0x18001e140` and `0x18001e280`; **neither did
I**, and I repeat that so the gap is not inherited as a fact.

---

## 6. INSTRUMENT DEFECTS — MINE, WITH THE DIRECTION EACH ERRS

A deflating blind spot outranks an inflating one: an inflating screen
manufactures work and is self-limiting; a deflating one reports that work does
not exist and nothing detects that.

| # | instrument | defect | direction |
|---|---|---|---|
| G1 | my extent walker v1 | walked FORWARD and required the successor `.pdata` row to begin EXACTLY at the predecessor's end, so alignment padding between fragments terminates the walk early and the body reports SHORTER than it is | **DEFLATING** |
| G2 | my operand reader | a pure `.pdata` walker REFUSES a frameless leaf (MSVC emits no `.pdata` for one), and **both Camera bodies are in that class** — so control (a) ran over an EMPTY population and printed "0 no-displacement stores found", which is warning 7's dead-parser signature exactly | **DEFLATING** |
| G3 | my operand reader's CONTROL (a) | tested `ops[0]` against `^\[reg\]$` while objdump prints `DWORD PTR [rcx]`, so **the control reported the known positive ABSENT while the screen itself was reading it correctly** | **DEFLATING** |
| G4 | my operand reader | counted `call QWORD PTR [rax+0x18]` as an 8-byte WRITE — an indirect branch READS its memory operand. It claimed a `const` function writes through a vtable pointer | inflating |
| G5 | my position-independent comparer v1 | normalised ALL branch/RIP targets function-relative, so two calls to the SAME external function compared as different; reported 6 false differences between two instantiations of one template | inflating |
| G6 | my registration scan, first run | read the working tree AFTER my own work landed and reported 4 of 8 ALREADY REGISTERED (§3.1) | inflating, **alarming** |

**G3 is the one worth carrying, and lane SLOTS independently called it the best
defect either of us found.** It is warning 9 **with the roles reversed**: the
standing rule is that scoring two screens on their UNION hides a blind screen.
Here a **CONTROL was stricter than the screen it polices**, so the control
failed while the screen was sound — and had I scored them together, the pair
would have read as one clean pass with the conclusion "no no-displacement stores
here", which is exactly Wave 27's wrong answer arrived at by a new route.
**Proposed ledger entry: "A control STRICTER than the screen it polices reports
the known positive ABSENT while the screen reads it correctly; scored together
the pair looks like one clean pass."**

**G2 is the one that would have cost a future lane most**, because it is silent
and because the Camera pair is the project's canonical write-width subject.

All six were found and fixed **before** any figure in this document was taken.

### 6.1 A HARNESS DEFECT NO LANE CAN PREVENT

§0.3: *never background a blocking `game_dir_lock.sh acquire` — the lock is held
from the moment it is GRANTED, not when you notice.*

**The Bash tool caps a foreground command at 600s and AUTO-BACKGROUNDS it past
that.** I requested a 960s timeout on a 900s acquire; the harness backgrounded it
at 600s regardless. A lane that issued that command and moved on would be holding
the game directory unknowingly — the exact failure §0.3 names, arrived at without
the lane doing the forbidden thing.

**Mitigation used here:** poll the task's output file rather than waiting on the
process. **ADDRESSEE: the Wave 30 kickoff author** — §0.3 should say *acquire
with a timeout below the harness's foreground cap (e.g. 500s) and retry*, rather
than telling lanes not to do something the harness does for them.

---

## 7. MY PARTNER'S INSTRUMENT, AND MINE IN THEIR HANDS

Pairing is an instruction. Wave 28's result — 15 instrument defects across two
lanes, **zero found by their own author** — reproduced.

### 7.1 What SLOTS found in MY instrument: NOTHING, and the nothing is informative

SLOTS ran `gfxbody_operands.py` against both bodies they ported and reported
**no discrepancy**. I record that as a negative result, not as silence. It also
**confirmed three things they had only read by eye**, which is the more useful
half:

- `?Update@MeshInstance@` writes `rax-0x18..+0x3` — the identity-table entry is
  **28 contiguous bytes with no hole**. They had inferred that from four separate
  stores; the reader proves the union is gapless.
- `rsp` writes print as `0x20-0x38  0x40  0x58-0x73` — `0x40` as a **separate
  range**, i.e. `[0x39,0x40)` is never written, so both `bool` arguments really
  are byte-width and a `bool` typedef is correct rather than merely adequate.
- **Exactly one `rip` write** in the whole body (the lazy-init guard at
  `0x1801ff30c`), which is the check that their port does not touch a second
  global.
- And **21 memory destinations, not one through `rdi`**: `Update` writes nothing
  to `this`.

They scored my controls **separately** per warning 9 and both passed on their
own. Neither of their bodies has a Camera-class write-width trap — a negative
result they report as one.

### 7.2 What SLOTS' instrument found in MY subject

SLOTS' `slots_regscan.py` addresses registrations, not operands, and their
finding reached me as a **method correction rather than a hit**: I ran my own
independent equivalent (§3) and it found **0 of 8 already registered**. So their
defect does **not** reproduce in my namespace — the census was already fixed for
multi-line literals and my eight were genuinely open.

Their extent finding (`?Draw@EdgeMeshInstance@` at **70.3x**) is the one that
changed my work: it caused defect **G1** to be found and fixed before I trusted
a single extent.

### 7.3 The pair's scoreboard

Four instrument defects between us, **every one found by the other lane's
author**: SLOTS' `mint_slot_sweep` multi-line blindness and its identity-vs-extent
misprint (found by SLOTS in a shared tool, confirmed independently by the
integrator at 49 sites — I measured 56 under a wider door set); my G1 (prompted
by SLOTS' 70.3x); and SLOTS' verdict of *nothing* in my reader. Plus five more
I found in my own instruments while building them (G2–G6), which does not
contradict the rule — those were found by CONTROLS, not by review.

### 7.4 Agreement on the SunLight pair

SLOTS and I overlap on exactly one subject and we reached the same answer from
opposite sides: I decline `?GetSunCamera@SunLight@` at **3775 B** and they
decline `?DrawShadowMap@SunLight@` at **900 B**, both as size judgements. **The
whole sun-shadow-camera path therefore stays in the DLL after this wave**, and
each of us cites the other's figure so nobody reads one declination as the small
half of a solved pair.

---

## 8. RESIDUALS — EACH WITH A NAMED ADDRESSEE

Not "the next lane that touches this" — that clause satisfies a checklist and
then waits forever (`reference_expiry_clause_needs_an_addressee.md`).

| # | item | addressee | consequence at cutover |
|---|---|---|---|
| **R1** | `0x180062d60`, the per-pass draw-item sort. ~0x2306 bytes of introsort with a 0x400-byte recursion stack, **confirmed absent from the export directory**. Called at its retail address from `EndDrawGroup`. Declined on SIZE. | the next Graphics lane | with the DLL gone it resolves to nothing and the three per-pass lists go out **UNSORTED** — transparency and depth artefacts, not a crash. This is Wave 27 R1 and Wave 23's finding, unchanged and now reached from a second caller. |
| **R2** | `?GetSunCamera@SunLight@` (3775 B) unported — §5 | the next Graphics lane, **jointly with whoever takes `?DrawShadowMap@SunLight@`** (SLOTS declined it) | already a `called` row; the sun camera is built in the DLL |
| **R3** | `?AddCulledDrawItems@Scene@` (2950 B) unported — §5 | the next Graphics lane | as R2, every frame |
| **R4** | `?UpdateNodes@` (2191 B) and `?CopyInstancedToUnique@` (1127 B) unported — §5, with an INTRINSIC verification hazard on top of the size | the next Graphics lane | as R2 |
| **R5′** | Wave 28's R5 **was stale on arrival** (§2.4.1): nine files + one `ResolveExport` already took that name door, the symbol is already minted and already `CUTOVER_REQUIRED`, and my door moved `called` by zero. What is genuinely open is ONE address door — `engine/funcs/engine_funcs.cpp:1546` reaches `0x1801fe728` by address. I did not edit that shared file: outside my subject, read by everything, and I had no measurement route that would show a regression. | whoever next audits address doors, **or lane INSTR** | **none** — the symbol is minted and REQUIRED, so cutover is already covered. This is a tidiness item, not a cutover risk, and Wave 28's framing overstated it. |
| **R6** | Wave 28's R6 — **CLOSED.** `?GetRootPoseAtTime@` is ported and registered. | — | — |
| **R7** | Wave 28's R7 — `MaterialInstance::BuildParameters` attendance 0 in a full autostart run, with five ported rows hanging off that route. **NOT ADDRESSED BY THIS LANE.** I did not find the route and did not look for it; saying so rather than letting it fall off the list. | the next Graphics lane | five rows remain unexercised, not wrong |
| **R8** | `AnimationClipPrefix +0x44` and `+0x4c` have exactly one reader each (mine) and `+0x50`'s `frameRate` name is in tension with its use as a key COUNT (§4.4.1). A **WRITER** is needed and is most likely in `Animation::Serialize`, which no lane has touched. | the next lane that ports `UpdateNodes` or `CopyInstancedToUnique` | none — a naming question, not a behaviour one |
| **R9** | `mint_slot_sweep.py` still prints 16 unported slots and still cannot see a multi-line registration — **including my own `GetRootPoseAtTime` one** (§3.3). Neither SLOTS nor I fixed it, deliberately. | **lane INSTR** | none at cutover; it mis-scores the board |

---

## 9. VERIFICATION — MEASURED vs INFERRED, SEPARATED

### 9.1 MEASURED — static

- **Build gate (§0.2), FOREGROUND, exit code recorded.**
  `mingw32-make standalone-debug` → **exit 0**, `standalone/build/reimpl-x64.exe`
  **33,872,181 bytes**. `find runtime standalone code engine dx11 -newer <exe>`
  over `*.cpp *.h *.inc` → **EMPTY**. `nm` on the LINKED IMAGE finds
  `RtGraphicsWave29GfxbodyRegister`, `RtGraphicsWave29GfxbodySummary` and
  `rtcoll::RtColl_ArraySetSizeFill_180063a80`. `strings` finds each mangled name
  I added (2 occurrences each — registration site plus resolve/mint site, the
  discriminator PARTSYS documented). **Zero compiler warnings from my file.**
- **The build's own guard caught a real defect in my code**, which is worth
  recording as a *working* instrument in a document full of broken ones:
  `check-log-strip` failed the build because I wrote `std::printf`, and
  `standalone/log_strip.h` does `#define printf(...) ((void)0)`, which the
  preprocessor cannot apply to a qualified name. It named my three lines and
  explained that the compiler would otherwise have blamed `log_strip.h`.
- **Export directory (`pefile`)**: all eight names present at the addresses and
  ordinals in §2.3; `0x180023830`, `0x180063a80`, `0x180062d60` all confirmed
  **absent**; `0x1801fe728` confirmed **present** as `?_secondsPerTick@Timer@System@@0NA`
  ord 4797; `0x18016169c` confirmed **not** an export.
- **`.pdata` with chain validation**: the fragment counts in §2.3, from a
  parent-indexed walker that first reproduced three independently published
  extents exactly.
- **Raw file bytes**: `0x18016169c` == `00 00 80 3f` == `1.0f`.
- **Written-operand sets**: §4, from a reader whose three controls are scored
  separately and pass.
- **Position-independent identity**: `0x180063a80` ≡ `0x18004f790`, 79/79, with
  both controls.
- **Registration scan**: §3, 0 of 8, with four controls.

### 9.2 THE LIVE RUN — MEASURED, AND ALL FOUR ARE ATTENDANCE ZERO

**Game-directory wait: ~26 minutes across three acquires** (900s DENIED behind
MINT/AVBODY/VERIFY, 420s DENIED behind AVBODY, then 480s **GRANTED**). Held as
`GFXBODY`, `assert-held` verified before deploying, released immediately after
the last measurement. Deployed md5 `e75fd039a2b125fdad31645a5dad096f`, verified
equal to this lane's build — a clobbered run does not announce itself.

`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=2400 BANISHED_PAUSEMENU_QUIT=180`,
clean self-quit, **empty crash log**, world built (4,506
`StorageDisplayComp::Draw` entries).

```
[RtPort]    GFXBODY29 (Wave 29 lane GFXBODY): 4 Graphics body exports registered
[GFXBODY29] Scene::AddGlobalDrawItems calls=0 visited=0 emitted=0
[GFXBODY29] Renderer::AddLight calls=0 | Renderer::EndDrawGroup calls=0 sortedLists=0
[GFXBODY29] Animation::GetRootPoseAtTime calls=0 lerped=0 single=0
```

**ALL FOUR ARE ZERO. THESE ARE *PRINTED* ZEROS, NOT SILENCE**, and that
distinction is the whole reason the summary was wired (Wave 28 defect #7): a
counter only an unwired summary prints is indistinguishable from a body that
never ran.

#### 9.2.1 THE ZEROS ARE INHERITED, NOT THIS LANE'S DEFECT — and the SAME RUN proves it

The run is emphatically not dark. The renderer path is live in our code:

```
[GFXBIG27]  Renderer::Draw           calls=100000     (milestone ladder)
[GFXBODY28] Renderer::BeginDrawGroup calls=200000
[GFXBODY28] Camera::operator=        calls=400000
[GFXBIG27]  AnimationState::Update   running
```

The causal chain, each link measured in this one run:

1. **All four of my rows are reached from our tree through exactly one caller
   each**, and §3's scan printed those callers: `AddGlobalDrawItems`, `AddLight`
   and `EndDrawGroup` only from PARTSYS's `SunLight::AddDrawItems`;
   `GetRootPoseAtTime` only from Wave 28's `UpdateRootOnly`.
2. **`[PARTSYS28] … SunLight AddDrawItems=0`** — printed in this same log, from a
   lane whose summary IS wired, with `[RtPort] Wave 28 lane PARTSYS: 5
   particle/sunlight exports registered` confirming it is armed. Its three
   dependants therefore cannot have run.
3. **`SunLight::AddDrawItems` is a vtable slot of `??_7SunLight@Graphics@@6B@`,
   and that table is NOT PROMOTED** (the census prints SunLight 9/11, unpromoted).
   So retail's vtable still points at the DLL's body and our slot never
   dispatches. This is the PROMOTION BACKLOG behaving exactly as PARTSYS §1.2
   recorded, observed a second time from the far end of the chain.
4. `GetRootPoseAtTime`'s only caller, `UpdateRootOnly`, is the OFF-SCREEN cheap
   path, which **Wave 28 §9.3 also measured at 0** and predicted would be 0
   whenever every animated mesh stays in frustum.

**So the four zeros are a statement about ROUTES, not about these bodies.** They
are registered, they are correct as far as the operand sets go, and with the DLL
present **nothing in this configuration can reach them** — retail calls
`EndDrawGroup` and `AddGlobalDrawItems` from inside the DLL by `rel32`, and an
in-DLL `rel32` call is not a door
(`reference_a_ported_row_can_be_permanently_unpromotable.md`). At cutover, when
the DLL's callers cease to exist, these rows become the only implementation.

**This is the third consecutive wave to hit the same wall** (Wave 28: two of
eight live, six at zero; Wave 27: `BuildParameters` "goes live immediately",
measured 0). The project's verification debt — ~280 registered-and-unexecuted
bodies — is **structural**, not a series of lane oversights, and my four join it.
**ADDRESSEE: lane VERIFY**, whose subject this is.

#### 9.2.2 DEFECT #7 DEMONSTRATED, NOT JUST CITED

In this same log, `grep -c "UpdateRootOnly\|ApplyEvents\|GetConstantName"`
returns **0**. Wave 28 lane GFXBODY's `RtGraphicsWave28GfxbodySummary()` is
**still unwired** on `wave29/base`, so **I could not read `UpdateRootOnly`'s
attendance from a run in which I could read PARTSYS's and my own.** Wave 28's
own §9.3 figures for those six rows came from a differently-instrumented run and
cannot be reproduced from an ordinary one today.

I wired mine (`standalone/main.cpp`, unconditional, beside PARTSYS's) and **did
not** wire Wave 28's — per warning 1, and because editing another lane's
reporting mid-wave makes its numbers unattributable. **ADDRESSEE: lane VERIFY** —
one line beside mine makes six Wave 28 rows readable again.

### 9.2.3 CENSUS DELTA — ATTRIBUTED ROW BY ROW, RECONCILED EXACTLY

Re-derived on the built branch with the same tool and interpreter:

```
                    wave29/base                 this branch
Graphics     132 called / 108 ported 81.8%   132 called / 112 ported 84.8%
TOTAL       1453 called /1294 ported 89.1%  1454 called /1299 ported 89.3%
Graphics BODY gap                       8                            4
Graphics ceiling                    97.0%                        97.0%
```

**`ported` +5 and `called` +1.** Every row read out of `--csv` and attributed,
not inferred from the totals:

| row | before → after | why |
|---|---|---|
| `?AddGlobalDrawItems@Scene@Graphics@@` | 0 → **1** | ported here |
| `?AddLight@Renderer@Graphics@@` | 0 → **1** | ported here |
| `?EndDrawGroup@Renderer@Graphics@@` | 0 → **1** | ported here |
| `?GetRootPoseAtTime@Animation@Graphics@@` | 0 → **1** | ported here |
| `0x180063a80` (addr, internal) | **absent** → called=1 ported=1 | §4.5 — the ONLY new `called` row, and it arrives already ported |

And three rows that did **not** move, checked because a lane that only reports
movement cannot be audited:

- `0x180062d60` — my file joins the `callers` column beside
  `wave27_lane_gfxbig.cpp`; it was already `called`, still `ported=0`.
- `0x180023830` — already `called` and already `ported`; my file just joins.
- `?_secondsPerTick@Timer@System@@0NA` — **already `called`**, which is what
  refuted R5 (§2.4.1).

**`called` grew by exactly one and `ceiling` did not move at all**, which is the
expected signature for a lane that ports rows already in the `called` column
rather than opening new doors. Contrast lane SLOTS, which reports `called +4,
ported +0` — the opposite signature, and equally correct for its subject.

**Do not add these to a wave total.** Seven other lanes land on this base and
`ceiling` has `called` in both halves. The integrator must re-derive on the merge.

### 9.3 NOT MEASURED — stated, not described as unavailable

- **`tools/trace_oracle/` was NOT run.** It compares the draw-call/pipeline-state
  COMMAND STREAM and needs no frame determinism, which makes it the right
  instrument for `EndDrawGroup` and `AddGlobalDrawItems` — the two bodies here
  that can change what reaches the GPU. Wave 28 recorded the same omission for
  `BeginDrawGroup`. **A pixel diff is not the alternative**: Wave 23 measured
  five runs at identical seed and pin splitting ~98% apart *independently of the
  flag under test*. **ADDRESSEE: the next Graphics lane** — `EndDrawGroup`
  reorders draw items, which is exactly what that oracle can see and nothing
  else in this project can.
- **No differential (`RT_DIFF`) oracle was built for any of the four.** For
  `AddGlobalDrawItems` and `EndDrawGroup` a call-for-call A/B is not
  straightforwardly available because both mutate shared renderer state, so
  running ours and theirs in sequence appends/sorts twice
  (`WAVE9_LANE_B2.md`'s double-append shape). Stated as the reason rather than
  as "no oracle possible", because **"no gameplay route" is not "no oracle"** and
  that argument does not apply here — these bodies have a route, they lack a
  *replayable* one.
- **`GetRootPoseAtTime` IS the one of the four that admits a synthesised
  oracle** — it is a pure function of `(this, t0, t1)` with no side effect
  outside its return slot (proved by its operand set), so an input sweep against
  the shipped export would verify it outright
  (`project_pure_functions_verify_by_sweep.md`). **I did not build it.**
  **ADDRESSEE: lane VERIFY**, whose subject is exactly the ~280 registered-and-
  unexecuted bodies, and this is the cheapest possible instance.

---

## 10. WHAT THIS DOES NOT CLAIM

1. **It does not claim eight bodies.** It claims **four**, and names the other
   four as SIZE declinations in §5.
2. **It does not claim the four are VERIFIED. It claims the opposite, measured.**
   All four report **`calls=0` in a live world-build run** (§9.2) — printed
   zeros, from a wired summary, in a run where three sibling lanes' bodies
   report hundreds of thousands of calls. Ported + registered + *never executed*.
   What IS verified is that each port's **written-operand set matches retail's
   over a derived extent** — a structural check that catches the Camera and
   `MeshInstance` trap classes, and **nothing else**. No differential oracle was
   run against any of the four (§9.3).
2a. **It does not claim the zeros are unfixable, nor that they are this lane's
   fault.** §9.2.1 traces them to an unpromoted `??_7SunLight@Graphics@@6B@` and
   to `UpdateRootOnly`'s off-screen route, with `[PARTSYS28] … AddDrawItems=0`
   printed in the same log as the load-bearing evidence. That is a
   route-and-promotion fact. It is not proof that no route exists — only that
   none was reached on the autostart route in this configuration.
3. **It does not claim the operand-set method proves semantic equivalence.** Two
   bodies can write the same byte set with different values. It refutes a
   specific, historically expensive class of error; it is not a proof of
   correctness.
4. **It does not claim any field name for `AnimationClipPrefix +0x44` or
   `+0x4c`**, and it explicitly does **not** resolve the `+0x50`
   `frameRate`-vs-key-count tension. Readers do not name fields.
5. **It does not name `obj->[0x28]` or `obj->[0x30]`** in
   `AddGlobalDrawItems`. A `>` test and a mask test are what several different
   meanings share.
6. **It does not claim `0x180063a80` is semantically an `Array<RendererDrawable>`
   SetSize.** It claims something narrower and stronger: that its 79 instructions
   are identical, position-independently, to a template instantiation the tree
   already ports. The element *type* is an inference from the caller.
7. **It does not claim to have closed Wave 28's R5, and it retracts an earlier
   draft of its own that did.** R5's actionable half was false before this lane
   started (§2.4.1). One address door in `engine_funcs.cpp:1546` remains, and it
   carries **no cutover risk** because the symbol is already minted and already
   `CUTOVER_REQUIRED`.
7a. **It does not claim to have audited the other residuals Wave 28 left.** Only
   R5, R6 and R7 were re-checked. R1–R4 were taken as read.
8. **It does not address Wave 28's R7** (`BuildParameters` attendance 0).
9. **It does not claim any census figure as a wave total.** Seven other lanes
   land on this base and `ceiling` has `called` in both halves, so the integrator
   must re-derive on the merge.
10. **It does not claim my instruments are now correct** — only that six named
    defects were found and fixed, three of them DEFLATING, and that the controls
    which would catch a seventh are documented and were run.
11. **It does not claim lane SLOTS' `mint_slot_sweep.py` finding was verified by
    me against their subject.** I verified only that it does **not** reproduce
    against mine, with my own independent scanner.
