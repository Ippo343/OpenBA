# WAVE 28 LANE PARTSYS — the particle simulation, and three SunLight slots

Branch `wave28/partsys`, cut from `wave28/base` at `5d2594de`.

Everything below is labelled **MEASURED** or **INFERRED**. Nothing in a MEASURED
section was taken from the kickoff, from Wave 27's docs, or from Ghidra's
decompiled C.

---

## 0. HEADLINE

**`?Update@ParticleSystemInstance@Graphics@@` LANDED.** 0x1800a5600, the whole
particle simulation, ported in full and registered default-ON. It was the only
thing between `??_7ParticleSystemInstance@Graphics@@6B@` and COMPLETE, and the
census now prints that table at **10/10 COMPLETE**.

Also landed: `?RemoveState@ParticleInstance@` and **three** of the five open
`??_7SunLight@Graphics@@6B@` slots (6/11 → 9/11).

**Not landed:** the two big SunLight bodies, `?DrawShadowMap@` (0x384 bytes) and
`?Draw@…HHHH@Z` (0x405 bytes). §7 has the addressee.

**Nothing this lane ported has been observed to EXECUTE.** §6 states that in the
word UNVERIFIED and explains why it is the expected result, not a defect.

---

## 1. CENSUS DELTA, ATTRIBUTED ROW BY ROW (MEASURED)

`tools/runtime_census.py` run **twice in this same worktree with the same
interpreter** — once with the tree detached at the base commit `5d2594de`, once
on the finished branch. Not predicted, not quoted.

```
                    before (5d2594de)              after (this branch)
TOTAL       1430 called / 1277 ported / 89.3%   1434 called / 1278 ported / 89.1%
Graphics     126 called /   98 ported / 77.8%    130 called /   99 ported / 76.2%
             ceiling 96.0%                       ceiling 96.2%
```

**The headline percentage went DOWN. That is this lane working, not failing** —
the ceiling identity again (`called` is in both halves, so opening a door lowers
the percentage). Score this lane on the table identities and the row list, never
on 89.1%.

### 1.1 The FOUR newly-`called` rows, and the ONE newly-`ported` row

Read out of `runtime_census.py --csv` on both sides and **diffed as sets, not
compared as counts** (`reference_diff_the_set_not_the_count.md`). The diff is
exactly seventeen lines and every one is accounted for below.

**`ported` +1 — exactly one row:**

| row | namespace | kind | before → after |
|---|---|---|---|
| `?RemoveState@ParticleInstance@Graphics@@IEAAXXZ` | Graphics | BODY | 0 → **1** |

**`called` +4 — exactly four rows, all callees of `SunLight::AddDrawItems`:**

| newly `called`, still unported | namespace | kind |
|---|---|---|
| `?AddGlobalDrawItems@Scene@Graphics@@AEBAXAEAVRenderer@2@@Z` | Graphics | BODY |
| `?AddLight@Renderer@Graphics@@QEAAXPEBVLight@2@@Z` | Graphics | BODY |
| `?EndDrawGroup@Renderer@Graphics@@QEAAXH@Z` | Graphics | BODY |
| `?GetSunCamera@SunLight@Graphics@@QEBA?AVCamera@2@AEAVRenderer@2@@Z` | Graphics | BODY |

Graphics +4 called, +1 ported. TOTAL +4 called, +1 ported. **The two figures
reconcile exactly; there is no unexplained movement.**

The other twelve diff lines are this file's name appearing in the `callers`
column of rows that were ALREADY `called` — it joined them, it did not grow
them: `?AddCulledDrawItems@Scene@`, `?BeginDrawGroup@Renderer@`,
`?SetBounds@Geometry@`, `?_frameIndex@ParticleInstance@`, `?_rng@Particle@`,
`?_typeInfo@ParticleSystem@`, `?IsDerivedFrom@Base@Object@`,
`?NextGroup@Random@System@`, `?SecondsToTicks@Timer@`, `?TicksToSeconds@Timer@`.

### 1.2 WHY FIVE REGISTRATIONS MOVED `ported` BY ONE

Four of the five symbols this lane registers are **vtable slots**. The census
counts a vtable slot only when its table is PROMOTED, and none of these tables
is (§4). This is the PROMOTION BACKLOG behaving exactly as Wave 27 lane GFXPART
recorded it, seen a second time. It is not a registration that failed.

### 1.3 Vtable state (MEASURED, re-run not predicted)

| table | before | after |
|---|---|---|
| `??_7ParticleSystemInstance@Graphics@@6B@` | 9 / 10 | **10 / 10 COMPLETE** |
| `??_7SunLight@Graphics@@6B@` | 6 / 11 | **9 / 11** |

---

## 2. THE EXTENT, AND WHY .pdata WOULD HAVE UNDERSTATED IT BY 24x (MEASURED)

The brief said 0x1479 bytes and told me to confirm it independently. Re-derived
from the shipped DLL's `.pdata`, following `UNW_FLAG_CHAININFO`:

```
   0x1800a5600..0x1800a569d  size 0x09d  flags 0x0
   0x1800a569d..0x1800a57f6  size 0x159  CHAININFO
   0x1800a57f6..0x1800a62f1  size 0xafb  CHAININFO
   0x1800a62f1..0x1800a6867  size 0x576  CHAININFO
   0x1800a6867..0x1800a6a7a  size 0x213  CHAININFO

   FIRST ENTRY ALONE says  0x9d
   FULL CHAIN         says 0x1800a5600..0x1800a6a7a = 0x147a bytes
```

**A lane that read one `.pdata` row would have sized this body at 0x9d — 3.9% of
the truth, a 24x understatement**, and would have scoped it as a slot rather
than a lane. `reference_pdata_is_a_lower_bound_on_function_extent.md` records the
worst previous instance as 50 bytes for a 430-byte body (8.6x). **This is a
worse instance by a factor of three, and it is the function that instance was
written about.** The brief's 0x1479 is the same span counted to the last byte
(0x1800a6a79); it agrees.

---

## 3. THE RNG CONSTANT TABLE — every constant, with the address it was lifted from

**This is the artefact the next particle lane needs (deliverable requirement).**

### 3.0 CLOBBER CLEARANCE — every value below is CLEARED, and here is on what basis

The integrator's rule, adopted from lane FACTUI1: **an artefact that cannot be
cheaply re-derived cannot be cleared of the shared-scratchpad clobber at all**,
and anything not re-derived must be marked UNCLEARED rather than quoted.

The inventory is **nine float constants, two RNG masks and one extent** — twelve
values (the brief's "twelve RNG draws" and this "twelve values" are a
coincidence; the draw count is eleven, §3.4).

| value(s) | first derivation | re-derived in `<scratchpad>/partsys/` | verdict |
|---|---|---|---|
| the 9 float constants | `rd.py`, shared scratchpad root | `partsys_rederive.py` | **CLEARED — byte-identical, all 9 bit patterns** |
| `0xff3a58ad` / `0xffffdf8c` | read off an `objdump` listing by eye | re-counted **out of the raw bytes** as `AND EAX,imm32` immediates | **CLEARED — 11 sites each, and a NEGATIVE CONTROL on the two textbook masks returned 0 sites each** |
| the 0x147a extent | `pdata.py`, shared scratchpad root | `partsys_rederive.py`, full CHAININFO walk | **CLEARED — identical chain, identical total** |
| the 11-vs-12 draw count | — | `partsys_rederive.py` (five independent instruction signatures) | **CLEARED — 11, 11, 11, 11, 11** |

**Nothing in this lane is UNCLEARED.** The masks are the strongest of the four
because their re-derivation did not read a disassembler's output at all — it
searched the shipped bytes — and because it was handed a known positive (the
shipped masks) alongside the absence claim (the textbook ones), which is the only
way an absence screen means anything.

Every value below was read out of `C:\GOG Games\Banished\Runtime-x64.dll` on
disk with `pefile`/`objdump`, in a **lane-private** scratch directory
(`<scratchpad>/partsys/partsys_rederive.py`) after the integrator warned that
the shared scratchpad was clobbering sibling lanes' `.pdata` and export scripts.
Nothing here was retyped from a reference or from a sibling lane's document.

### 3.1 The tempering masks and their shifts

| what | value | sites in this body | first site |
|---|---|---|---|
| `AND EAX, imm32` before `<< 7` | **`0xff3a58ad`** | 11 | 0x1800a589c |
| `AND EAX, imm32` before `<< 15` | **`0xffffdf8c`** | 11 | 0x1800a58a8 |
| `SHR EAX, 11` | — | 11 | — |
| `SHL EAX, 7` | — | 11 | — |
| `SHL EAX, 15` | — | 11 | — |
| `SHR EAX, 18` | — | 11 | — |
| `CMP EDX, 0x270` (the wrap) | — | 11 | — |

All eleven `0xff3a58ad` sites:
`0x1800a589c 0x1800a5936 0x1800a59b3 0x1800a5a2f 0x1800a5b51 0x1800a5be3
0x1800a5c75 0x1800a5cfc 0x1800a5d7d 0x1800a5e09 0x1800a5e83`

All eleven `0xffffdf8c` sites:
`0x1800a58a8 0x1800a5942 0x1800a59bf 0x1800a5a3b 0x1800a5b5d 0x1800a5bef
0x1800a5c81 0x1800a5d08 0x1800a5d89 0x1800a5e15 0x1800a5e8f`

**NEGATIVE CONTROL, and it is the one that matters here.** The textbook MT19937
masks `0x9d2c5680` and `0xefc60000` were searched for in the same byte range as
`AND EAX, imm32` immediates: **0 sites each.** The shipped masks are not merely
different-looking, the textbook ones are absent. A screen looking for the
ABSENCE of a shape has no natural positive case
(`reference_absence_screen_must_be_handed_a_shape.md`), so the same script was
handed the SHIPPED masks as its positive: 11 and 11.

### 3.2 The float constants

| address | bits | value | what it is |
|---|---|---|---|
| `0x180161658` | `0x00000000` | 0.0f | the `qw * 0.0f` term in the quaternion multiply |
| `0x18016165c` | `0x2f800000` | 2⁻³² | the RNG word → unit float scale |
| `0x18016167c` | `0x3c8efa35` | π/180 | degrees → radians |
| `0x180161694` | `0x3f000000` | 0.5f | the spawn-offset half-extent |
| `0x18016169c` | `0x3f800000` | 1.0f | |
| `0x1801616d8` | `0x40000000` | 2.0f | the second half of the ±1 mapping |
| `0x180161710` | `0x42b40000` | 90.0f | the elevation angle's base, in DEGREES |
| `0x180161718` | `0x437f0000` | 255.0f | the colour byte divisor |
| `0x1801617c0` | `0x80000000` | −0.0f | the XORPS sign-flip mask |

Each draw becomes `r = 1.0f − (float)(uint32)y * 2⁻³² * 2.0f`, in (−1, +1].
**Retail performs the two multiplies SEPARATELY** (`MULSS xmm7` then
`MULSS xmm8`) rather than folding them into one 2⁻³¹. Folding is algebraically
identical and numerically different, so the port does not fold.

### 3.3 THE ELEVEN DRAWS ARE NOT ELEVEN COPIES — set-differenced, not assumed (MEASURED)

I wrote **one** `RngNext()` for all eleven sites on the assumption that they are
eleven copies of one sequence. The integrator relayed lane GFXBODY's
`Camera::operator=` finding mid-lane — *"the same loop as the sibling" was wrong
and the difference was a WRITE WIDTH, found as a written-byte SET DIFFERENCE over
the instruction stream, not by eye* — and pointed it straight at these draws. So
the assumption was **converted into a measurement** rather than left standing.

`<scratchpad>/partsys/draw_core_diff.py` extracts the tempering+advance core of
each site (`[anchor−0x09, anchor+0x27)`, which contains no RIP-relative field, so
a raw byte compare is sound there) and groups the eleven:

```
DISTINCT CORES: 2
  group 0 : 10 site(s)  0x1800a589c 0x1800a5936 0x1800a59b3 0x1800a5a2f
                        0x1800a5b51 0x1800a5be3 0x1800a5c75 0x1800a5d7d
                        0x1800a5e09 0x1800a5e83        byte-identical
  group 1 :  1 site(s)  0x1800a5cfc
       byte anchor+0x01c : ref c1 -> c8

CONTROL positive (1 bit flipped in the mask): CAUGHT
CONTROL negative (untouched copy):            identical, OK
```

**One site really is different**, and it is draw 8, the spin-direction coin flip:
its final tempering `XOR` is encoded `33 c8` (result into `ECX`) instead of
`33 c1` (result into `EAX`), because its consumer is `TEST $0x1,%CL` rather than
a `CVTSI2SS`. Same value, different destination register, **no semantic
difference** — so one `RngNext()` is correct for all eleven. That conclusion is
now MEASURED with both controls instead of assumed.

A first pass used a ±0x40 window and reported **eleven distinct windows**, which
is true and useless: that window captures each draw's *use* site. Recorded
because it is the failure mode the method invites — **a set-difference over the
wrong extent reports a difference that is real and answers a different
question.** GFXBODY's own third point (a reader with a blind spot reporting the
same impossible hole in two bodies, *consistent and wrong*) is the mirror of it,
and both were guarded here by running the positive control before believing the
negative result.

### 3.4 IT IS ELEVEN DRAWS, NOT TWELVE (MEASURED)

The brief, Wave 27 §6.1, and WAVE27_CLOSED §5 all say twelve. **Counted out of
the raw bytes, every one of the five signature instructions appears exactly
ELEVEN times**, and the eleven `0xff3a58ad` sites are listed above with their
addresses. All eleven are in the spawn loop; the age loop and the compaction
pass draw nothing.

This is a small correction and it is recorded because it is the shape the
project keeps paying for: a count quoted forward through three documents without
re-derivation. The count does not change any port decision — it is a checksum on
whether the body was read.

---

## 4. PROMOTION — CLOSING WAVE 27'S RESIDUAL 1 WITH A MEASUREMENT

Wave 27 lane GFXPART's RESIDUAL 1 was addressed to "the Wave 28 lane that owns
Graphics vtables" and asked for `tools/mint_stamper_census.py` to be RUN, not
predicted. **It was run.**

```
??_7ParticleSystemInstance@Graphics@@6B@   0x180155040   10 slots   3 stamp sites
   ??0ParticleSystemInstance@Graphics@@QEAA@AEBV01@@Z   RETAIL (copy ctor)
   ??1ParticleSystemInstance@Graphics@@UEAA@XZ          RETAIL (dtor restamp)
   ??0ParticleSystemInstance@Graphics@@QEAA@XZ          OURS-CAPABLE via NAME+ADDR
   STAMPERS 3 ; ours-capable 1 ; retail 2 ; unattributed 0

??_7SunLight@Graphics@@6B@                 0x180158e10   11 slots   3 stamp sites
   ??0SunLight@Graphics@@QEAA@AEBV01@@Z                 RETAIL (copy ctor)
   ??1SunLight@Graphics@@UEAA@XZ                        OURS-CAPABLE
   ??0SunLight@Graphics@@QEAA@XZ                        OURS-CAPABLE
   STAMPERS 3 ; ours-capable 2 ; retail 1 ; unattributed 0

??_7ParticleInstance@Graphics@@6B@         0x1801550e0   10 slots   8 stamp sites
   STAMPERS 8 ; ours-capable 3 ; retail 5 ; unattributed 0
```

### VERDICT: **DO NOT PROMOTE ANY OF THE THREE.** (MEASURED premise, INFERRED verdict)

A table is worth promoting only if we own **every** function that stamps it.
`??_7ParticleSystemInstance@` is COMPLETE at 10/10 and we own **one of three**
stampers. Promote it and objects built through `??0…@QEAA@XZ` would carry our
table while objects built through the **copy constructor** — and every object
whose **destructor** restamps the base table on the way out — would carry the
DLL's. That is two-owners-one-object, the shape
`reference_a_ported_row_can_be_permanently_unpromotable.md` and the
`_singleton@AchievementManager` tag are both about.

**COMPLETE IS NOT A PROMOTION.** That is the same conclusion Wave 27's FRONTIER,
GFXPART and GFXMESH each reached and declined on, and it is now backed by a
number for these three tables specifically instead of by an unrun tool.

> **RESIDUAL 1 — ADDRESSEE: the Wave 29 lane that owns `Graphics::Geometry`
> lifecycle, or whoever scopes the promotion backlog.** The blocker on
> `??_7ParticleSystemInstance@` is NOT its ten slots, which are done. It is two
> named exports: `??0ParticleSystemInstance@Graphics@@QEAA@AEBV01@@Z`
> (0x180020bd0-ish, the copy ctor) and `??1ParticleSystemInstance@Graphics@@UEAA@XZ`
> (0x1800a5520-ish). Port and register those two and the table becomes
> promotable with no further slot work. Re-derive with
> `tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_stamper_census.py` from
> the MAIN checkout's venv (see §8 defect 1) and look for
> `STAMPERS n ; ours-capable n ; retail 0`.

---

## 5. WHAT THE BODY ACTUALLY DOES, AND THE READINGS THAT ARE NOT OBVIOUS

The full transcription lives in `runtime/src/graphics/wave28_lane_partsys.cpp`'s
own comments. This section records only what a careful reader would otherwise
get wrong.

### 5.1 Ghidra DROPPED AN ARGUMENT, and it was the interesting one (MEASURED)

Ghidra's C prints

```c
pfVar12 = (float *)Math::Transform::FromDirection(local_108, (Vector3 *)&local_118);
```

— two arguments. The symbol says otherwise:
`?FromDirection@Transform@Math@@SA?AV12@AEBVVector3@2@0@Z` is
`static Transform FromDirection(const Vector3&, const Vector3&)`, i.e. a hidden
return slot plus **two** `Vector3`s. Retail loads all three at 0x1800a5fe5:

```
lea 0x20(%rsp),%r8        <- the argument Ghidra dropped
lea 0x50(%rsp),%rdx
lea 0x60(%rsp),%rcx
mov %r15,0x20(%rsp)       <- and it is a ZEROED Vector3, built right there
mov %r15d,0x28(%rsp)
```

The dropped argument is `FromDirection`'s `offset`, and it is the zero vector.
`runtime/src/math/transform.cpp:776` shows `offset` becoming the returned
transform's translation, so passing it matters even though the rotation half is
what this body reads back. **Exactly
`reference_ghidra_c_output_vs_disassembly.md`, on the one call in the body where
a dropped argument would have been invisible in play.**

### 5.2 A NAMING CORRECTION TO WAVE 27'S RECORD MAP — repair the LICENCE (MEASURED)

`wave27_lane_gfxpart.cpp` names the 0x58-byte particle record's fields from
`ParticleInstance::Draw`, which only **reads** them. Two names are wrong:

| Wave 27 name | offset | what the WRITER says it is |
|---|---|---|
| `kRec_PosB` / `kRec_PosBPrev` | +0x28 / +0x34 | the particle **colour** (rgb) |
| `kRec_ScrollCur` / `kRec_ScrollPrv` | +0x40 / +0x44 | the particle **alpha** |

The evidence is unambiguous and it is a WRITE: `Update` fills +0x28..+0x30 with
`ParticleSystem[+0xdc..+0xde] / 255.0f` and lerps it toward `[+0xe0..+0xe2]`.
A byte divided by 255 is not a coordinate. And +0x40/+0x44 are written **with
the same value in one 8-byte store** (`MOV [r8+r9+0x98],RAX` at 0x1800a67f0
carrying a duplicated float), so the cur/prev lerp Draw performs on that pair is
a no-op by construction — which is what an alpha that has no interpolation
history looks like and is not what a scroll offset looks like.

The thing Wave 27 could not see at all is the rotation RATE: it is not in the
record, it is at +0x0c of a **parallel 0x14-stride array** hanging off
`ParticleSystemInstance::_emitter` (+0xd8). Wave 27's degrees reading on
+0x48/+0x4c stands and is corroborated here from the writer's side.

**REPAIR THE LICENCE, NOT THE SENTENCE** (kickoff warning 13). The defect is not
"two names were wrong". It is that **a field was named from a reader that can
only see its SHAPE**: a 3-float pair lerped by an alpha is identical whether it
is a position or a colour, and Draw cannot distinguish them even in principle.
The rule, one level down from
`reference_a_writer_beats_readers_agreeing_on_a_sign.md`: **a reader can name a
field's TYPE and never its MEANING; only a writer can.** Wave 27's own
+0x30-is-a-MaterialInstance reading is sound precisely because it had a writer
(`Particle::Serialize`) as one of its two witnesses.

Wave 27's `kRec_*` constants are **deliberately NOT renamed** — they are
load-bearing for that file's bodies, renaming is a merge hazard in a seven-lane
wave, and the correction changes no behaviour. It is recorded in
`wave28_lane_partsys.cpp`'s banner, which is a file the next particle lane opens.

### 5.3 The emitter's parallel array — a structure nothing had read (MEASURED)

`ParticleSystemInstance::_f_d8` was `UNVERIFIED beyond "zeroed by the ctor"` in
`runtime/types/graphics/Particle.h`, and Wave 27 identified it as the emitter
allocated by `PSI::SetParticle`. This body is the first reader of its contents:

```
+0x00  uint8_t*  the per-particle EXTRA array, stride 0x14
+0x08  int32     that array's size      (+0x0c its capacity)  -- a Collection::Array header
+0x10  float     the spawn-rate accumulator, seeded from ParticleSystem[+0x48]
```

and each 0x14-byte extra record is

```
+0x00..+0x08  velocity (3 floats)
+0x0c         rotation rate, degrees/sec, with a RANDOM SIGN
+0x10         size rate
```

This reading is **corroborated from the other end**: Wave 27 transcribed
`PSI::SetParticle` as `*(u32*)(m+0x10) = *(u32*)(sys+0x48)` followed by
`0x180020e90(m, *(u32*)(sys+0x28))` — an Array `SetSize` to `sys[+0x28]`
elements. Two unrelated bodies, one conclusion.

### 5.4 Two pool fields nothing had read (MEASURED)

`Particle.h` records the 0x40-byte pool record's ctor as writing `+0x30 = 0,
+0x38 = 0` with no identified reader. They are two **int64 tick accumulators**:

* **+0x30** the spawn accumulator — incremented by the frame delta and
  DECREMENTED by one spawn interval per particle emitted;
* **+0x38** the system's total elapsed ticks — compared against
  `ParticleSystem[+0x58]` to switch the emitter off when the system expires.

### 5.5 Float readings a rewrite would silently change (MEASURED)

* The bounding box is rebuilt with retail's `COMISS`/`JA` idiom, which **keeps
  the existing value when the comparison is unordered**. `a < b ? a : b` does
  not reproduce that on a NaN, so each of the six is written as retail's
  `if (existing <= candidate) existing = candidate`.
* The colour parameter `t` clamps **0 first, then 1**; the alpha fade clamps
  **1 first, then 0**. Retail really does use opposite orders in the two places
  and the difference is observable on a NaN.
* Gravity is `[0xb4] * [0xb8|bc|c0] * [0x8c] * dt²`, multiplied in that sequence
  and hoisted out of the loop exactly where retail hoists it.
* Velocity damping is `(accel + v) * damping` — ADDSS then MULSS, not
  `accel*damping + v*damping`.
* The quaternion rotation's `w` term is a **real** `q.w * 0.0f` MULSS against the
  0.0f at 0x180161658, and the three `q.{x,y,z} * zero` products are real
  multiplies against a XORPS-zeroed register. They vanish for finite inputs and
  do not for NaN/inf, and retail emits them.

### 5.6 The spawn draws do NOT map to axes in draw order (MEASURED)

Draws 2, 3 and 4 each produce `r * ParticleSystem[+0x78] * 0.5f`, and the axis
each feeds is **not** the order they were drawn in: draw 2 → Z (`[+0x74]`),
draw 3 → Y (`[+0x70]`), draw 4 → X (`[+0x6c]`). The compiler interleaved them;
the register trace (`xmm10`, `xmm9`, `xmm1`) is the only thing that says which is
which, and the decompiled C's variable soup does not. Getting this backwards
gives a plausible-looking spawn volume that is transposed forever.

### 5.7 OWNERSHIP, and the direction that is dangerous (MEASURED)

`ParticleInstance::RemoveState` frees the pool **only** when
`_resource->_active == 0`:

```
if (res && res->_active == 0) {
    pool = this->_boundsPtr;
    if (pool) { Array::~Array(&pool[0x18]); operator delete(pool); _boundsPtr = 0; return; }
}
_boundsPtr = 0;
```

That test reads like a safety check and is an **ownership** test. It pairs with
`PSI::SetParticle`, whose two arms Wave 27 transcribed: when `_active` is set the
pool is **borrowed** from `_resource->_boxes[_index]` and belongs to the resource;
when it is clear the pool was `operator new`'d per instance and belongs here.

A teardown "simplified" to free unconditionally would free a live shared object,
and kickoff warning 6 is right that **the directions are not symmetric** — under-
release leaks, over-release frees something still in use. Ownership was
established by reading the WRITER, not by the shape of the field.

---

## 6. VERIFICATION STATE — STATED HONESTLY

### MEASURED

* **BUILD GATE, with the artefact stated rather than the exit code.** The
  integrator relayed that lane GFXBODY measured a backgrounded `mingw32-make`
  writing a **ZERO-BYTE** output and still returning exit 0 — indistinguishable
  from a clean build. So, for the final tree:

  ```
  $ mingw32-make standalone-debug          # FOREGROUND, not backgrounded
  Built standalone: standalone/build/reimpl-x64.exe
  cutover manifest gate: OK (60 rows; row set and every disposition match a fresh derivation)
  exit 0

  $ ls -la standalone/build/reimpl-x64.exe
  -rwxr-xr-x 33415616 Sep 12 13:18 standalone/build/reimpl-x64.exe
  mtime 2026-09-12 13:18:26  md5 625017315272d0a308a09eaafc652008
  ```

  **33,415,616 bytes, timestamp AFTER the last edit.** The deployed copy's md5
  was compared against the build's and matched, so the run below measured THIS
  binary and not a sibling lane's (the game directory is shared; the lock was
  held as `PARTSYS` throughout and released immediately after the last
  measurement — a sibling lane reported queueing behind it).

  **TWO md5s appear in this doc and they are the same source.** The measurement
  run (§6.1) used the 13:18 build, md5 `625017315272d0a308a09eaafc652008`. The
  tree was then rebuilt once more with `-Wall -Wextra` after a `touch`, purely to
  confirm **zero compiler warnings** from the new translation unit; that relink
  produced md5 `8e79c662fe32e71fb356779da730c86d` at 13:23, same 33,415,616
  bytes, from byte-identical sources. Stated rather than glossed, because "the
  md5 in the doc is not the md5 on disk" is exactly the kind of quiet mismatch
  that makes a later reader distrust a sound measurement.

  **And size plus exit code still cannot tell "linked my work" from "relinked
  without it"**, so, on the integrator's instruction after lane FACTUI1 measured
  a backgrounded build producing NO output file at exit 0 — four more lines,
  proving the symbols reached the **linked image** and not merely a `.o`:

  ```
  $ find runtime/src/graphics/wave28_lane_partsys.cpp \
         runtime/src/graphics/rt_graphics_port.cpp \
         standalone/main.cpp -newer standalone/build/reimpl-x64.exe
  (empty)                                   <- no source is newer than the exe

  $ nm  reimpl-x64.exe | grep -c RtGraphicsWave28PartsysRegister            1
  $ nm  reimpl-x64.exe | grep -c RtGraphics_ParticleSystemInstance_Update   7
  $ strings reimpl-x64.exe | grep -c 'Update@ParticleSystemInstance@…Time'  2
  $ strings reimpl-x64.exe | grep -c 'RemoveState@ParticleInstance@…IEAAXXZ' 2
  $ strings reimpl-x64.exe | grep -c 'GetSunCamera@SunLight@Graphics@@'     1
  ```

  `GetSunCamera` counting **1** rather than 2 is the discriminator that makes
  this screen worth running: it is a name this lane only *calls* (one
  `RT_EXPORT_ONCE` literal), while the two it *registers* appear twice — once at
  the registration site and once at the resolve/mint site. A count of 0 anywhere
  would have meant the string never reached the image.
* **The `*Summary()` function IS CALLED — verified empirically, not assumed.**
  The integrator relayed that several lanes' summaries are written and never
  invoked, which would make a zero mean "never printed" rather than "never
  called". `[PARTSYS28]` lines appear in `standalone_debug.log` from a real run
  (§6.1), so the zeros below are **printed zeros**, and the distinction that
  decides UNVERIFIED is settled in the direction that lets the zeros be read.
* **Every byte, extent and constant came from `objdump`/`pefile` against
  `C:\GOG Games\Banished\Runtime-x64.dll` on disk**, not from Ghidra, and the
  extent and every constant were **re-derived a second time in a lane-private
  scratch directory** after the integrator's clobber warning (§3, §8).
* **Every mangled name this lane registers or resolves was read out of the
  shipped export directory with `pefile` BEFORE the body was written**, by VA —
  the guard against a phantom name that resolves NULL in silence. All six of
  `SunLight::AddDrawItems`' callees are real exports; `?RemoveState@…IEAAXXZ` is
  a real export (IEAA = PROTECTED, and this DLL exports its protected members).
* **The census delta was re-derived on both sides with the same tool in the same
  worktree and diffed as a SET** (§1).
* **`tools/mint_stamper_census.py` was RUN** and its output is quoted in §4.
* **The two bare-RET SunLight slots are MEASURED, not assumed**: `0x180106790`
  and `0x1801067a0` are each `C2 00 00` followed by INT3 padding, and NEITHER has
  a `.pdata` entry. They are not stubs this lane chose to leave empty.

### NOT MEASURED / UNVERIFIED

**Every body this lane ported is UNVERIFIED by execution.** In the word:
**UNVERIFIED.**

The attendance line `[PARTSYS28] …` is wired **unconditionally** into
`standalone/main.cpp` so the expected zeros are PRINTED rather than inferred
from silence. The run result is in §6.1.

Why this is the expected outcome and not a defect, stated rather than implied:

* Four of the five registered symbols are **vtable slots of tables that are not
  promoted** (§4, and the decision NOT to promote them is this lane's). With the
  DLL present, the DLL's own vtable dispatches to the DLL's bodies. Nothing in
  our tree resolves `?Update@ParticleSystemInstance@` by name.
* The fifth, `?RemoveState@ParticleInstance@`, **does** have a live by-name
  caller — `wave27_lane_gfxpart.cpp`'s `ParticleInstance::SetParticle` — but
  that caller is itself an unpromoted vtable slot, so the chain bottoms out at
  the same place.
* This is kickoff §2.2 caveat 3 arriving in a non-factory lane: **a lane that
  registers rows whose attendance is zero has measured nothing**, and saying so
  is the deliverable.

**No oracle was built and none was run.** The kickoff forbids building one, and
none of the existing ones fits: `save_trace.h` replays Serialize write paths and
this body is not a Serialize; the diff harness's `calls=` column would report
`calls=0` on every site, which is not verification
(`reference_uncalled_port_is_not_verified.md`).

**The RNG measurement side of the MISC dependency is UNMEASURED either way.**
Lane MISC ports and registers `?NextGroup@Random@System@@`; this body reaches it
through `ResolveExport`, which is registry-aware, so after that merge the call
lands in our body rather than the DLL's. MISC verified the two are the same
algorithm. Since this lane's attendance is zero on both sides of that merge,
there is no number here that changes — **but the fact is recorded because a
future lane that DOES get attendance must say which side of the merge it
measured on.**

### 6.1 THE RUN (MEASURED)

`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=1800 BANISHED_PAUSEMENU_QUIT=120`, on
the final binary (md5 `625017315272d0a308a09eaafc652008`), under
`tools/game_dir_lock.sh` held as `PARTSYS` for the whole window and released
after. `standalone_crash.log` is **0 bytes**; the run reached the built world
(31 `[WildAnimal…]` / citizen lines in `standalone_error.log`) and self-quit.

```
[RtPort] Graphics: 117 exports + 6 grid addresses registered
[RtPort] Wave 28 lane PARTSYS: 5 particle/sunlight exports registered
[PARTSYS28] PSI::Update entries=0 earlyOut=0 spawned=0 aged=0 killed=0
[PARTSYS28] PI::RemoveState calls=0 freedPool=0 | SunLight Update=0 Draw(MHHH)=0
            AddDrawItems=0 emitted=0
```

**Read the `calls=` column, not just a lack of divergences** — there are no
divergences because there are no calls. All five bodies: **UNVERIFIED.**

The identity line proves the registration ran (5 of 5) on the exact binary that
produced the zeros, so the zeros are about dispatch and not about registration.

**CORROBORATION that this is the backlog and not a defect in my registration:**
Wave 27 lane GFXPART's own attendance line, printed in the SAME run from the
SAME binary, is also all zeros:

```
[GFXPART27] ParticleSer=0 (records=0) PIDraw=0 (quads=0) AddDrawItems=0
            (emitted=0) GetInstXform=0 PISetParticle=0 (rngDraws=0) PSISetParticle=0
```

Six bodies from a different lane, registered nine days earlier, in the same
family, on the same unpromoted tables, with the same result. **The whole
Graphics particle family is attendance-zero today**, and §4 explains why and what
it would take to change (two named exports, not more slots).

---

## 7. WHAT WAS NOT DONE, WITH ADDRESSEES

### 7.1 The two large SunLight bodies

| slot | symbol | retail | extent (full .pdata chain) |
|---|---|---|---|
| 9 | `?DrawShadowMap@SunLight@Graphics@@UEBAX…HH@Z` | 0x180106bc0 | 0x180106bc0..0x180106f44 = **0x384** (4 chained fragments) |
| 10 | `?Draw@SunLight@Graphics@@UEBAX…HHHH@Z` | 0x1801067b0 | 0x1801067b0..0x180106bb5 = **0x405** (single entry) |

Not attempted. **This was a budget choice, not a technical blocker** — the lane
spent itself on the 0x147a-byte headline body and taking
`??_7ParticleSystemInstance@` to COMPLETE. Nothing was found that makes these
two hard beyond their size. Both are already partly read elsewhere in the tree:
`code/Game/Game/gameupdateloop.cpp:2171` and
`gameupdateloop_load.cpp:1747` record that `DrawShadowMap` is gated by
`renderer+0x3c != 0` while `Draw` is not — and this lane independently
re-derived that same `+0x3c` gate from `AddDrawItems`' own bytes, which is a
second witness on it.

> **RESIDUAL 2 — ADDRESSEE: the Wave 29 Graphics lane (GFXBODY's successor).**
> Two bodies, 0x384 and 0x405 bytes, both slots of
> `??_7SunLight@Graphics@@6B@` (DLL 0x180158e10), neither tagged UNPROMOTABLE.
> They take the table 9/11 → 11/11. Re-derive their extents by following the
> `.pdata` CHAIN, not one entry — §2 is why. Note §4: COMPLETE will still not
> make that table promotable, because `??0SunLight@Graphics@@QEAA@AEBV01@@Z`
> (the copy ctor) is a RETAIL stamper.

### 7.2 `0x180062320` is an unregistered `Array<T>::~Array`

`RemoveState` reaches it BY ADDRESS. Disassembled here: it is byte-identical to
the eleven bodies `runtime/src/collection/array.cpp` already serves with
`ArrayDtorImpl` — `if (data) MemFree(data); data = 0; *(u64*)(this+8) = 0` — and
it is NOT in that file's `J1_ENTRY` list. Wave 27 left an identical residual for
`0x180062110` (`SetSize`) for the same reason: `array.cpp` is a shared file that
several lanes touch in one wave and an append there is a merge hazard.

> **RESIDUAL 3 — ADDRESSEE: the Wave 29 lane that owns
> `runtime/src/collection/array.cpp` (or the Wave 28 integrator, if he is
> consolidating that file anyway).** Add `J1_ENTRY(180062320)` and its
> `RegisterRuntimeAddr`. Verify first with
> `objdump -d --start-address=0x180062320 --stop-address=0x180062393` against the
> retail DLL and compare to any existing J1 row — the shape is the same eleven
> instructions. Doing so also converts this lane's one address door into a
> registered one. Wave 27's `0x180062110` residual is still open and is the same
> file.

### 7.3 The RNG draw count in three documents says twelve

§3.4 measured eleven.

> **RESIDUAL 4 — ADDRESSEE: the Wave 28 integrator, when writing WAVE28_CLOSED.**
> `WAVE28_KICKOFF.md` (your scope line and §5 warning 4),
> `WAVE27_CLOSED.md` §5 and `WAVE27_LANE_GFXPART.md` §6.1 all say twelve inlined
> RNG draws in `?Update@ParticleSystemInstance@`. Re-derive with
> `<scratchpad>/partsys/partsys_rederive.py` or by counting
> `and $0xff3a58ad` in
> `objdump -d --start-address=0x1800a5600 --stop-address=0x1800a6a7a`. The count
> is 11 and it agrees across five independent instruction signatures.

---

## 8. INSTRUMENT DEFECT OBSERVED, NOT FIXED

Per kickoff warning 1, nothing below was fixed.

### 8.1 `docs/runtime_port/WAVE28_KICKOFF.md` — the census command cannot run in a worktree

**File:line:** `docs/runtime_port/WAVE28_KICKOFF.md` §2 (the
`tools/ghidra_mcp/.venv/Scripts/python.exe tools/mint_factory_census.py` block)
and every other place in the kickoff and in Wave 27's lane docs that spells the
interpreter that way.

`tools/ghidra_mcp/.venv/` is gitignored and **does not exist inside a lane
worktree** — only `requirements.txt` is checked in. Every command written that
way fails with a "command not found" that an unwary lane can read as "the tool
printed nothing". The working spelling is the MAIN checkout's absolute path,
`C:\code\GIT\untitled_rev_eng_project\tools\ghidra_mcp\.venv\Scripts\python.exe`.
Relayed by the integrator mid-lane and independently true here — this lane used
the absolute path from its first call, so nothing in it is affected. Not fixed.

### 8.2 The session scratchpad is SHARED across all seven lanes

**File:line:** not a file — the harness's `scratchpad` directory, one per
session and not one per agent.

Sibling lanes silently overwrote each other's `pdata.py` / `exp.py` /
`exports.py` analysis scripts by basename collision. **A clobbered `.pdata`
reader or export dumper produces a confident wrong number with no tell**, which
is this project's recurring defect shape arriving through the harness rather than
through a tool. On the integrator's instruction this lane re-derived its extent
AND all nine float constants AND both RNG masks in a lane-private subdirectory
(`<scratchpad>/partsys/`), with a negative control; everything agreed with the
first derivation (§2, §3). Not fixed — the mitigation is per-lane subdirectories,
which is a convention, not a change.

### 8.3 `tools/runtime_census.py --csv` takes the path as a VALUE, not a redirect

**File:line:** `tools/runtime_census.py`, the `--csv CSV` argument.

`--csv` is `[--csv CSV]` — it requires a filename. `runtime_census.py --csv >
out.csv` exits 2 with argparse's usage text. Wave 27 lane GFXPART's §6.3 writes
"read out of `runtime_census.py --csv`", which reads as the redirect form. Minor,
but it costs a lane a failed run and the failure looks like a tool error rather
than a usage error. Not fixed.

### 8.4 `tools/runtime_census.py` prints 7 STALE VERDICTS unrelated to any lane

**File:line:** `tools/runtime_census.py`, the no-caller triage table's stale
check.

Every run in this worktree — base commit and branch alike — ends with
`*** 7 STALE VERDICT(S)` naming seven `Application@1@` symbols. It is identical
on both sides of this lane's diff, so it is pre-existing and not caused here, but
a red block at the end of the tool's output is the surface the next person
reaches and it currently says nothing about which wave owns it. Not fixed.

---

## 9. FILES CHANGED

| file | change |
|---|---|
| `runtime/src/graphics/wave28_lane_partsys.cpp` | NEW — five bodies + registration + attendance |
| `runtime/src/graphics/rt_graphics_port.cpp` | call `RtGraphicsWave28PartsysRegister()` behind `BANISHED_RT_PARTSYS` (**default ON**; the env var is a kill switch), plus its own identity line |
| `standalone/main.cpp` | unconditional `RtGraphicsWave28PartsysSummary()` |
| `docs/runtime_port/WAVE28_LANE_PARTSYS.md` | this file |
| `docs/runtime_port/wave28_partsys_rederive.py` | NEW — EVIDENCE, not tooling: re-derives the extent chain, the nine float constants and both RNG masks from the shipped DLL, with the negative control. Committed so §3's table is **cheaply re-derivable by the next lane**, which is the integrator's clobber rule. Precedent: `docs/runtime_port/wave9_laneV2_wrapscan.py`. |
| `docs/runtime_port/wave28_partsys_draw_core_diff.py` | NEW — EVIDENCE: the eleven-draw core set-difference of §3.3, with both controls |

**No tool was built, fixed or extended** (kickoff warning 1). The two `.py` files
above are single-purpose evidence scripts for this lane's own claims, in the
directory that already holds two of the same kind; nothing in `tools/` was
touched.

**`deleting_dtor.cpp` was NOT touched and NO index was claimed.** Kickoff
warning 10 does not apply to this lane.
