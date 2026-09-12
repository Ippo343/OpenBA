# WAVE 23 — LANE GFXBODY

Branch `wave23/gfxbody`, cut from `wave23/base` at `a60351e3`. Private worktree.
Subject: the **10 Graphics function rows** of the 25-row Graphics census gap
(kickoff §2). The 8 VTABLE rows are lane GFXVT's and the 7 DATA rows are lane
GFXDATA's; nothing here ports one of those.

**Headline: 5 of 10 ported, ALL FIVE EXERCISED on a live in-world run, and the
largest of them has a differential oracle that needs no game run.** `tools/gfxbody23_probe/` compiles the SHIPPING
body, detours the callee, and runs the shipped
`?Update@AnimationManager@Graphics@@` and ours over one arena — 14 input shapes
AGREED, 5 of 6 scoreable negative controls CAUGHT. **The control set's first
version scored 3/4 and was wrong to:** all three of its firing controls fired on
the same one of the probe's two screens, leaving the other screen with no
control at all.

---

## 1. RE-DERIVATION (rule 1) — THE KICKOFF IS EXACT

```
tools/runtime_census.py            # run 2026-09-11 in this worktree, tip a60351e3
```
```
Graphics  599  100 called  75 ported  75.0%  ceiling 95.0%
Graphics gap 25 | BODY 8  CTOR 1  DTOR 1  OPNEWDEL 0  VTABLE 8  DATA 7  UNKNOWN 0
TOTAL     4632 1314 called 1137 ported 86.5%   => 42 of 177 gap rows (24%)
```

**Every figure in kickoff §0 reproduces exactly**, and the 8 BODY + 1 CTOR +
1 DTOR decomposition is this lane's ten rows symbol for symbol. Nothing in the
kickoff was stale for this lane. *(Rule 13: all census figures in this document
are BRANCH-LOCAL. The integrator must re-measure on the merged tip; Wave 21
published from lane reports and had to correct itself, and Wave 22's merged
System figure matched neither lane that moved it.)*

---

## 2. TASK 0 — THE PRE-PORT GREP (rule 2)

Denominator: **10 of 10 rows**, checked across `code/ engine/ runtime/
standalone/ dx11/`, plus the census `ported` column, plus the export directory.

| row | census `ported` | source hits | verdict |
|---|---|---|---|
| `??0FrameCopyImageBuffer@` | 0 | 5, all consumers/comments | not ported |
| `??1FrameCopyImageBuffer@` | 0 | 2, both consumers/comments | not ported |
| `?Create@FrameCopyImageBuffer@` | 0 | 4, all consumers/comments | not ported |
| `?Generate@FrameCopyImageBuffer@` | 0 | 5, all consumers/comments | not ported |
| `?Serialize@AnimationManager@` | 0 | 2 | not ported |
| `?Serialize@MeshInstance@` | 0 | 2 | not ported |
| `?Update@AnimationManager@` | 0 | 3 | not ported |
| `?AddDrawItems@MeshInstance@` | 0 | 4 | not ported |
| `?BuildParameters@MaterialInstance@` | 0 | 3 | not ported |
| `?Draw@Renderer@` | 0 | 2 | not ported |

**Zero hits are a body.** Every hit is a `ResolveExport` / `RT_EXPORT_ONCE`
door, a `RegisterDispatchSlot` line, an `RT_MINT_NAMED` table row, or prose.
This is the check `reference_half_registered_port_reads_as_absent.md` asks for —
`?StrCompare@String@System@@` read as unported for twenty waves because nobody
ran it.

All ten were then confirmed **present in the export directory** of the shipped
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile`
(`tools/gfxbody23_probe/` was written afterwards; the query is in §8).

**The dtor-mangling trap the brief flagged is real and was measured, not
assumed.** `??1FrameCopyImageBuffer@Graphics@@UEAA@XZ` is at `0x1800a3c80`,
ordinal 597. `??1FrameCopyImageBuffer@Graphics@@QEAA@XZ` — the non-virtual
spelling — is **ABSENT from the directory**. A registration under it would have
resolved NULL and registered nothing, silently
(`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`).

### 2.1 Wave 21's holdbacks, RE-CHECKED (rule 3)

Wave 21 lane GFXBODY triaged these same ten rows and ported none of them. Its
four recorded blockers, re-checked here:

| Wave 21 blocker | state on 2026-09-11 |
|---|---|
| **FCIB-BLOCK (a)** — RenderBuffer/Context symbols unported, so a wrong field corrupts a shared object | **Already stale when Wave 21 wrote it, and Wave 21 said so.** Re-confirmed: all seven named dependencies read `ported=1` AND `called` today. §4.1. |
| **FCIB-BLOCK (b)** — no GPU oracle for "did the texture copy correctly" | **STILL STANDS, and this lane did not discharge it.** §6.1. |
| **ANIMSTATE-LAYOUT** — cannot pin the 9-argument call | **FALSE BY MECHANISM.** The callee is exported (`0x1800fdb20`, ord 4474) and its mangled name spells all eight parameters. Wave 21 found this; this lane went further and *verified the argument slots against the shipped body* (§5). |
| **STREAM-ORACLE** (both Serializes) | **NOT discharged. §6.2 — and its premise is now sharper, not weaker.** |
| **AddDrawItems UNPROMOTABLE/HARMFUL** | **HOLDS, and is already tagged** in `standalone/reg_mint_promoted.cpp:1519`. §6.4. |

---

## 3. WHAT WAS PORTED — 5 OF 10

All in `runtime/src/graphics/wave23_lane_gfxbody.cpp`, registered by
`RtGraphicsWave23GfxbodyRegister()` behind its own kill switch
`BANISHED_RT_GFXBODY23` (wired into `rt_graphics_port.cpp`; unset = ON).
A new type header, `runtime/types/graphics/FrameCopyImageBuffer.h`.

| symbol | retail | ord | bytes | verification tier |
|---|---|---|---|---|
| `??0FrameCopyImageBuffer@Graphics@@QEAA@XZ` | `0x1800a3d10` | 198 | 304 | **EXERCISED** (§7.3, `ctor=1`) |
| `??1FrameCopyImageBuffer@Graphics@@UEAA@XZ` | `0x1800a3c80` | 597 | 138 | **EXERCISED** (§7.3, `dtor=1`) |
| `?Create@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b70` | 1877 | 197 | **EXERCISED** (§7.3, `Create=1`) |
| `?Generate@FrameCopyImageBuffer@Graphics@@…` | `0x1800a3b20` | 2395 | 60 | **EXERCISED** (§7.3, `blits=1` — the BRANCH, not the door) |
| `?Update@AnimationManager@Graphics@@…` | `0x1800fc240` | 4473 | 430 | **ORACLE-VERIFIED** (§5) + EXERCISED (601 calls, 19,108 inner iterations) |

**EXERCISED means the body ran on a real device without crashing. It does NOT
mean it is right** — nothing compared any of the four FCIB bodies against the
DLL's on the same inputs, and §7.5 explains why the pixel comparison I proposed
for that job does not work. Read §7.6 before quoting this column.

Registered **by name only**, with every address checked first — the check exists
because row 8 of this lane's own board (`?AddDrawItems@MeshInstance@`) is a case
where a `RegisterRuntimeAddr` would be *active harm*. None of the five addresses
occurs anywhere in `code/ engine/ runtime/ standalone/ dx11/` except in
comments, and no consumer reaches any of them by address.

### 3.1 Ghidra's C would have produced a wrong ctor

The ctor's decompile renders the inlined `operator new` as
`pRVar1 = extraout_RAX;` and reads as "store the constructor's return value".
The bytes at `0x1800a3d9f`-`0x1800a3dae` are `JZ / MOV RAX,RBP / MOV [RSI+0x70],RAX`
with `RBP == 0` throughout — RAX is the ALLOCATION on both paths. The port
stores the pointer it already holds, which is byte-faithful AND independent of
whether any given ported constructor in this tree returns `this` (one,
`Video_ImageBuffer_Ctor`, is declared `void`, and two earlier lanes crashed on
exactly that: `saveload_draw.cpp:427`, `sunlight.cpp:230`).

### 3.2 `Create`'s eleven parameters do not land in field order

`Create` writes `+0x28/+0x2c/+0x30` from the three register arguments, then
`+0x20` and `+0x24` from the **fifth and sixth** stack arguments, then `+0x34`
from the **fourth**. A transcription that assumed ascending order would have put
the flags word into `width`. The mapping is instruction-by-instruction in the
source, with the retail address on each line.

### 3.3 `Generate`'s companion call takes THREE arguments

`MOV R9B,0x1` at `0x1800a3b41` before `CALL [RAX+0x18]`. This is the same defect
class `runtime/src/save/saveload_draw.cpp:75` recorded on the call *into* this
function, found the same way.

### 3.4 The unguarded derefs are deliberate

`Create`, `Generate` and the ctor all dereference an allocation or a companion
pointer without checking it, exactly as retail does. Per CLAUDE.md these are
reproduced, not guarded: retail relies on the value being there, and a guard
would skip the work rather than restore it.

---

## 4. THE CENSUS DELTA, MEASURED — AND THE ONE ROW THIS LANE ADDED

```
                      before      after     (BRANCH-LOCAL, this worktree)
Graphics called          100  ->    101     (+1)
Graphics ported           75  ->     80     (+5)
Graphics gap              25  ->     21     BODY 8->6, CTOR 1->0, DTOR 1->0
TOTAL called            1314  ->   1315     (+1)
TOTAL ported            1137  ->   1142     (+5)
TOTAL                  86.5%  ->  86.8%
```

**`called` moved, and this lane moved it.** `?Update@AnimationState@Graphics@@…`
(`0x1800fdb20`, ord 4474) was in NEITHER column before — nothing in our tree
asked for it. `AnimationManager::Update` calls it, so porting that body put it
in `called`, unported (`reference_porting_can_grow_the_cutover_surface.md`). It
is named at the call site so the next census reader can attribute it.

**The BODY arithmetic is worth reading because it is not 8 − 3.** It is
8 − 3 + 1 = 6: three BODY rows ported (`Create`, `Generate`, `Update`) and one
BODY row ADDED to the gap by the same work. A reader tracking "Graphics BODY
went 8 to 6, so two bodies got done" would be wrong in both directions.

### 4.1 The FrameCopyImageBuffer cluster grew the surface by ZERO — measured

All seven named dependencies of the four FCIB bodies read `ported=1` **and**
appear in `called` today:

```
??0ImageBuffer@Video@@QEAA@XZ                          ported=1
??1ImageBuffer@Video@@UEAA@XZ                          ported=1
??2RenderBuffer@Video@@SAPEAX_K@Z                      ported=1
??3RenderBuffer@Video@@SAXPEAX@Z                       ported=1
?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z ported=1
?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ         ported=1
?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ          ported=1
```

That is the hazard Wave 21 named, **measured to zero** rather than asserted away.

---

## 5. VERIFICATION — `tools/gfxbody23_probe/`

A differential oracle for `?Update@AnimationManager@Graphics@@` **that needs no
game directory and no GPU.** Build and run:

```
g++ -std=c++17 -O1 -g -DGFXBODY23_PROBE -o tools/gfxbody23_probe/probe.exe \
    tools/gfxbody23_probe/probe.cpp runtime/src/graphics/wave23_lane_gfxbody.cpp
tools/gfxbody23_probe/probe.exe [--groups G] [--states S] [--corrupt N]
```

It `LoadLibrary`s the shipped DLL (a READ of the game directory; nothing written,
no game launched), writes a 13-byte absolute JMP over
`?Update@AnimationState@Graphics@@` in the loaded image so **both** sides call
one recording stub, builds a synthetic manager in its own memory, runs the
shipped body then ours **over the same arena address** with a snapshot between,
and compares two things: the nine-argument tuple at every callee entry, and
every byte of the arena.

**It compiles the SHIPPING body, not a retyping of it.**
`wave23_lane_gfxbody.cpp` is linked straight in and a ~20-line seam is stubbed.
Wave 21 lane GFXBODY closed with "the binary measured in §7 is not the one
described in §4"; this is the fix for that, and the corruption seam is behind
`#ifdef GFXBODY23_PROBE`, which the shipping build never defines.

### 5.1 Result

```
14 input shapes (groups x states, 1..12 x 1..9), all AGREED
  e.g. g=1 s=17  -> 17 calls, 573 arena bytes changed by the SHIPPED body, AGREED
       g=12 s=2  -> 20 calls, AGREED
       g=4 s=3   -> 6 calls, 201 bytes changed, AGREED
```

`calls=` is read, not just `divergences=` (rule 8): every green cell made a
non-zero number of calls and changed a non-zero number of bytes. The
**effectiveness line** prints how many arena bytes the SHIPPED body changed, so
a cell that compared nothing would say so
(`reference_probe_needs_an_effectiveness_control.md`).

The arena deliberately contains a **null group** (skip arm) and a
**zero-count group** (empty-inner-loop arm), so the two early-exit branches are
covered and not merely present.

### 5.2 The negative controls — AND THE MISTAKE THEY CAUGHT IN THEMSELVES

| N | what it corrupts (OURS only) | expected | measured |
|---|---|---|---|
| 1 | swap args 5 and 6 (the two `InterpolatedState<Transform>*`) | tuple | **CAUGHT** 6/6 tuples |
| 2 | pass `false` for arg 8 instead of the caller's own flag | tuple | **CAUGHT** 6/6 |
| 3 | zero the identity quaternion's `w` passed to the callee | tuple | **CAUGHT** 6/6 |
| 4 | walk the state array with stride 0x40 instead of 0x48 | — | **SEGFAULT (exit 139)**, before any compare. Excluded from the score. |
| 5 | hoist the inner loop bound | **NOT CAUGHT** | **NOT CAUGHT** — correct |
| 6 | write `0.0f` as the copied-back quaternion's `w` | bytes | **CAUGHT** 12 bytes |
| 7 | copy the Vector3 back from `state+0x1c` | bytes | **CAUGHT** 28 bytes |

**Score: 5 of 6 scoreable controls CAUGHT, 1 correctly NOT CAUGHT, 1 excluded.**

**6 and 7 exist because the first control set was 1–5, it scored 3/4, and that
was a false pass.** Controls 1, 2 *and* 3 all fire on the **argument-tuple**
screen; 4 faulted and 5 abstained. So the **byte-compare screen — half the
oracle — had not one control that fired**, and could have been entirely dead
while the probe still read 3/4. That is
`reference_union_control_hides_a_blind_screen.md` in the wild. Controls 6 and 7
were added to corrupt the copy-back *only*, leaving every argument identical, so
they can be caught by the byte screen and by nothing else. Both are.

Control 5 is the **effectiveness control** and is *expected* to report NOT
CAUGHT: nothing reachable here grows an array, so hoisting the bound is
genuinely unobservable. A control set in which every control fires proves only
that the controls were picked to fire
(`reference_control_must_pick_a_compared_subject.md`).

Control 4 is **kept, reported, and excluded.** Deleting a control that behaved
unexpectedly is how a control set quietly becomes a set of controls that pass.

### 5.3 A defect in the FIRST cut of the probe, and what it would have looked like

The probe originally used **two arenas side by side**. At `--corrupt 0` it
reported **53 byte divergences** — every one an interior pointer that the
builder had written, which necessarily differs when the arenas sit at different
addresses. A correct port read as a broken one. This is
`reference_position_only_diffs_hide_one_body.md` in miniature (same cause,
opposite direction: there position-only diffs *hid* an identity, here they
*manufactured* a difference). The fix is one arena run twice with a snapshot
between, which removes the whole class rather than special-casing fields.

**Nothing was concluded from the two-arena run.** It is recorded because a lane
that only reports its final instrument is not reporting its instrument.

### 5.4 A CORRECTION TO `runtime/types/graphics/Animation.h` THAT FELL OUT OF THIS

`AnimationManager` `+0x00/+0x08/+0x0c` were documented as a
"CRITICAL_SECTION-shaped, never initialised" triple, and the dtor's helper call
as "a defensive no-op call against a lock this class never actually
initialises". **Both are wrong.** They are a
`Collection::Array<AnimationGroup>` — `_data / _size / _capacity` — and the
proof is in the body this lane ported:

```
1800fc268:  CMP dword ptr [RCX + 0x8],R14D   ; +0x08 is a LOOP BOUND
1800fc26c:  JLE <return>
1800fc2a0:  MOV RBP,qword ptr [R15]          ; +0x00 is an ARRAY BASE, stride 0x30
```

A LockCount is not a loop bound. The header's own dtor note supplies the other
half once read the right way round: `FUN_18005bf60` destroys each element, frees
the block at `+0x00`, then zeroes all three — that is
`Collection::Array<T>::~Array`, and Ghidra typed its parameter
`LPCRITICAL_SECTION` only because the free happens under the *allocator's* own
critical section. So the dtor's call is load-bearing whenever a group has been
registered, not a no-op.

**No behaviour changes.** The ported ctor writes the same twelve zero bytes (a
zeroed Array *is* an empty Array) and the ported dtor still delegates to the real
helper. What changes is the names and the explanation — which is what the next
lane would have built on. Header and `animation.cpp` are updated in this branch.

This is also the check Wave 21 §4.3 explicitly said it had NOT done: it
enumerated the same group/state offsets from Ghidra's C and recorded that they
were unverified against the disassembly. They now are, and they hold — but
running the check was worth it anyway, because an agreement's *sign* is not
informative until somebody runs it.

---

## 6. THE FIVE NOT PORTED — DISPOSITION WITH EVIDENCE

### 6.1 The FCIB cluster's GPU half still has NO ORACLE — this is the one Wave 6 blocker that survived

All four FCIB bodies are ported and **EXERCISED** (§7.3) — they ran, on a real
device, and produced a real 1.9 MB screenshot. What they do not have is an
ORACLE. There is no memcmp for "did the back buffer actually get copied"; only a
pixel comparison would say, and **§7.5 measured that route's run-to-run variance
at 98% and retired it.** So FCIB-BLOCK (b) — the half Wave 8.5 already called
the load-bearing one — is the single Wave 6 holdback on this cluster that this
lane did not discharge, and it is now blocked on harness determinism (R6) rather
than on Graphics work.

What this lane did do: removed the other half (§4.1, measured to zero), wrote
the bodies, and moved the rows from "held back" to "ported and running".

### 6.2 The two `Serialize` rows — REFUSED, with the premise sharpened

`?Serialize@MeshInstance@` (114 bytes) and `?Serialize@AnimationManager@`
(165 bytes) are small. They are refused anyway, for a reason that is *not* the
one in the tree:

* Both dispatch through the Encoding vtable at `+0xa0` — the retail spelling.
  `reference_encoding_vtable_offsets_differ_between_builds.md` says the profile
  build puts it at `+0xb8`, so a port written from the profile decompile desyncs
  every save while looking right. I confirmed `+0xa0` in both bodies. **This is
  a transcription hazard, not a blocker.**
* The real cost is the four **unexported internals** they call —
  `0x18005cc60`, `0x18005d4a0` (MeshInstance) and `0x18005c3d0`, `0x18005c660`
  (AnimationManager) — which are `Collection::Array<T>` serialize helpers. Each
  is an address door, and an address door into an unported internal is a cutover
  liability, not a free call.
* The inherited "no oracle possible" half **is contestable** —
  `reference_write_path_serialize_is_replayable.md`. I am not repeating it as a
  blocker. I am declining on SIZE-OF-SURFACE, and saying so, because
  "no oracle" would have been the easy and wrong reason.

### 6.3 `?Draw@Renderer@` and `?BuildParameters@MaterialInstance@` — REFUSED, sized

| row | bytes | instrs | why not this lane |
|---|---|---|---|
| `?Draw@Renderer@` | 921 | 237 | Reads ~14 distinct `Video::Context` offsets, 2 Scene/Renderer array layouts, and calls **`BeginDrawGroup`, `AddCulledDrawItems`** (exported, unported, NOT in `called` — porting `Draw` adds 2 rows to the cutover surface) plus **`0x180062d60`, an unexported internal** (a sort). It is the every-frame renderer and there is **no oracle for it short of a pixel comparison**, which §7 says I could not run. |
| `?BuildParameters@MaterialInstance@` | 1422 | 383 | The **largest row in the Graphics gap by every measure** (Wave 21 measured it; I did not re-measure the byte count, see §9). 12 indirect calls. |

Neither carries an oracle-shaped blocker. They are **sized out, not argued out**.

### 6.4 `?AddDrawItems@MeshInstance@` — UNPROMOTABLE (HARMFUL), and already tagged

`standalone/scene_watch.cpp:400` uses the retail address `0x180100f20` as an
**identity token** against vtable slot 3 of live objects, and
`ResolveAddr(Module::Runtime, …)` consults the port registry first — so a
`RegisterRuntimeAddr` makes that compare fail for every object and
`BANISHED_LOD_CENSUS` silently sees an empty scene. Registering by NAME instead
is harmless but a **no-op**: the only caller is the DLL's own `Renderer`
dispatching through the DLL's own `??_7MeshInstance@`, and an in-DLL vtable
dispatch is not a door.

**Re-checked, not inherited (rule 3):** the `scene_watch.cpp` code is still there
and still reads that address as a token; and the row already carries an
`RT_UNPROMOTABLE … | HARMFUL` tag at `standalone/reg_mint_promoted.cpp:1519`
with a named addressee. **Nothing to do. This row is correctly parked and this
lane changed nothing about it.**

Scope note per rule 5: HARMFUL is the **DLL-present** verdict. At cutover, with
no DLL supplying vtable slot 3, the identity token is meaningless and the row
becomes ordinary; the existing tag says this.

---

## 7. THE RUN — I GOT ONE, AND ITS PIXEL ORACLE FAILED INFORMATIVELY

**This section was first written as "the run I did not get", specifying a route
I had found in the harness and could not take. Then the lock came free, I took
it, and the route worked. What follows is what actually happened — including the
part where the check I had proposed turned out to be worthless.**

### 7.1 Finding the route — the fourth instance of one lesson

Before writing "unverifiable" I grepped the harness, as
`reference_grep_the_harness_before_writing_unverifiable.md` requires.
`code/Game/Game/gameupdateloop.cpp:1812` already reads
**`BANISHED_SCREENSHOT_AT=<frame>`**, a harness-only hook that raises the same
request bit the quicksave key does and drives `GameUpdateLoop_TakeScreenshot`.
That function (`code/Game/Game/w2_takescreenshot.cpp:148-154`) resolves **all
four** FrameCopyImageBuffer symbols by `ResolveExport` NAME — so it is a door,
and my name registrations reach it. **Fourth instance on this project, and the
second wave running in which the driver was written by a lane that had no idea
it would become somebody else's only route.**

### 7.2 The lock

`tools/game_dir_lock.sh acquire GFXBODY 420` — **bare, never piped**
(`reference_a_piped_acquire_discards_the_refusal.md`). It queued behind lane
VIDVT for about 350s and then printed `ACQUIRED by GFXBODY` / `GRANT GFXBODY`,
`rc=0`. Five runs, then `release GFXBODY`, `rc=0`. Held about nine minutes.

*Correction to a relayed claim:* the coordinator told me the directory had been
held by Wave 22 lane ALLOC "throughout". It was held by lane **ENGINE** when I
checked and by lane **VIDVT** when I queued. Small, but it is the coordinator's
own instruction — do not treat a relayed cross-lane claim as established —
applied to the coordinator.

Deployed exe md5 `26387ebdfd9a3dcff00778dd1d688f44`, **compared against this
worktree's own build before the first run and equal**. And the stronger check,
made after the commit: `mingw32-make standalone-debug` **from the committed
tree reproduces that exact md5**, so the binary these runs measured is bit-for-bit
the binary this branch ships. That is the hazard Wave 21 lane GFXBODY closed
with ("the binary measured in §7 is not the one described in §4") shut by
measurement rather than by argument — the `#ifdef GFXBODY23_PROBE` corruption
seam added for §5 provably contributes nothing to it.

### 7.3 ATTENDANCE — all five bodies ran

```
BANISHED_AUTOSTART=1 BANISHED_SCREENSHOT_AT=300 \
BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120   reimpl-x64.exe
```

-> `rc=0`, **0-byte crash log**, a 1,966,134-byte `Screenshot0.bmp` written, and:

```
[GFXBODY23] ctor=1 dtor=1 Create=1 Generate=1 (blits=1) AnimUpdate=601 (states=19108)
```

Every one of the five ported bodies entered. **`Generate`'s BLIT counter is the
one that matters**: its whole body sits behind one equality test, so an entry
count of 1 would have been compatible with the copy never happening
(`reference_calls_zero_can_mean_always_took_the_uncovered_branch.md`). It reads
`blits=1`, so the branch fired and the indirect four-argument call into the
companion was made. `AnimationManager::Update` ran 601 times and its inner loop
ran **19,108 times** — this is not a body that ran and did nothing.

### 7.4 THE KILL SWITCH IS A REAL CONTROL

`BANISHED_RT_GFXBODY23=0`, same route, same seed:

```
[GFXBODY23] ctor=0 dtor=0 Create=0 Generate=0 (blits=0) AnimUpdate=0 (states=0)
```

Zero in every counter in the arm where the registrations did not happen, and
non-zero in the arm where they did — two runs each way. That is the check
`reference_the_gate_was_never_a_control.md` asks for, and this gate passes it.
Both arms exit `rc=0` with a 0-byte crash log and both write a screenshot.

### 7.5 THE PIXEL ORACLE I PROPOSED **DOES NOT WORK**, AND THE VARIANCE MEASUREMENT IS WHY

The first draft of this section proposed comparing `Screenshot0.bmp` from a
flag-on and a flag-off run byte for byte. I ran it. **The two differed — and
that means nothing, because I then measured the variance.** Five runs
(A, A2, A3 armed; B, B2 unarmed), all with `BANISHED_SEED=12345
BANISHED_CAMERA_PIN=0,0,60` and an otherwise identical command line:

```
armA   vs armA2  [same-arm ]:  97.98% of bytes differ
armA   vs armA3  [same-arm ]:  97.98%
armA2  vs armA3  [same-arm ]:   0.86%
armB   vs armB2  [same-arm ]:  97.98%
armA   vs armB   [CROSS-ARM]:   0.92%
armA3  vs armB2  [CROSS-ARM]:   0.79%
armA2  vs armB2  [CROSS-ARM]:   0.86%
armA   vs armB2  [CROSS-ARM]:  97.98%
armA2  vs armB   [CROSS-ARM]:  97.98%
armA3  vs armB   [CROSS-ARM]:  97.98%
```

The pairs fall into two tight clusters — about 0.8% and about 98% — and **the
split does not follow the flag at any point.** Two ARMED runs are 97.98% apart;
two UNARMED runs are 97.98% apart; an armed and an unarmed run are 0.79% apart.
Something bimodal in the run (most likely which side of a day/night or weather
transition frame 300 lands on) dominates the image completely.

**Consequences, stated rather than glossed:**

* **The screenshot byte-diff is NOT an oracle for these four bodies, and my own
  proposal was wrong.** The proposal and its refutation are recorded together,
  because a lane that reports only the instruments that worked is not reporting
  its instruments.
* **The cross-arm minimum (0.79%) is not distinguishable from the same-arm
  minimum (0.86%).** So there is no *detectable* flag effect — which is
  consistent with the port being correct and is **a negative result of unknown
  power**, not a verification. It rules out a gross defect that would blank or
  corrupt the frame. It rules out nothing subtler.
* **An md5 comparison would have been worse than useless.** It reports "differ"
  for two identical-configuration runs, so a correct port reads as broken and a
  broken one reads as broken — the same verdict either way.

**What a working pixel oracle would need** is frame determinism this harness
does not currently provide: the same in-world frame, not merely the same seed
and camera pin. That is residual R6.

### 7.6 WHAT THESE RUNS COULD NOT HAVE DETECTED — read this before quoting §7.3

* **No oracle ran on the four FCIB bodies.** Nothing compared ours against the
  DLL's on the same inputs. EXERCISED means they ran without crashing and
  produced a plausible image, not that they are right.
* **NO PIXEL WAS SUCCESSFULLY COMPARED** (§7.5). A wrong format enum, a wrong
  address mode or a wrong filter index that still yields a plausible frame
  passes every arm silently.
* **`Create` and `Generate` each ran EXACTLY ONCE per run.** One call cannot
  distinguish a body that is right from one that is right the first time. In
  particular `Generate`'s latch at `+0x68` only becomes interesting on a SECOND
  call with the same argument, and nothing on this route makes one.
* **`ctor=1 dtor=1` is balanced, but nothing checked for a leak** beyond that
  pairing. The two `RenderBuffer`s and the inner `ImageBuffer` are released
  through three different idioms; I verified the idioms against the bytes, not
  against an allocator census.
* **`AnimUpdate=601 states=19108` counts ENTRIES, not branches.** The probe (§5)
  is what covers the branches, and it covers them against a stubbed callee.
* **The attendance instrumentation is in the committed body**, so the binary
  measured here is the one that ships — but the `#ifdef GFXBODY23_PROBE`
  corruption seam is NOT compiled into it (§5).

## 8. COMMANDS, SO EVERY NUMBER CAN BE RE-DERIVED

```bash
# census (branch-local)
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py

# export directory: names, addresses, ordinals, and ABSENCE
tools/ghidra_mcp/.venv/Scripts/python.exe - <<'PY'
import pefile
pe = pefile.PE(r"C:\GOG Games\Banished\Runtime-x64.dll", fast_load=True)
pe.parse_data_directories([pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
    if e.name and b'FrameCopyImageBuffer' in e.name:
        print(hex(0x180000000+e.address), e.ordinal, e.name.decode())
PY

# the oracle
g++ -std=c++17 -O1 -g -DGFXBODY23_PROBE -o tools/gfxbody23_probe/probe.exe \
    tools/gfxbody23_probe/probe.cpp runtime/src/graphics/wave23_lane_gfxbody.cpp
for c in 0 1 2 3 5 6 7; do tools/gfxbody23_probe/probe.exe --corrupt $c; done
for g in 1 2 5 12; do for s in 1 2 4 9; do tools/gfxbody23_probe/probe.exe --groups $g --states $s; done; done

# build
mingw32-make standalone-debug        # clean, exit 0, zero errors, 2026-09-11
nm standalone/build/reimpl-x64.exe | grep RtGraphics_FrameCopy    # all 4 present
```

---

## 9. WHAT I DID **NOT** CHECK (rule 10)

* **No pixel was compared.** There WAS a run (§7) and all five bodies have
  attendance, so the "no attendance" caveat an earlier draft of this section
  carried is gone. What remains missing is any comparison of the RESULT: §7.5
  measured the screenshot route's run-to-run variance at 98% and retired the
  pixel check I had proposed.
* **The probe does not verify the FCIB four at all.** It verifies exactly one
  row. Four of my five ports have no ORACLE of any kind — they have attendance
  and a clean exit, which is a strictly weaker thing.
* **`AnimationState::Update` was never entered.** The probe detours it away. So
  the probe proves the CALLER is right about what it asks for and what it does
  with the answer; it proves nothing about the animation result, and it cannot
  detect a defect that is only visible when the real callee's side effects feed
  back (e.g. if the callee resized the state array — which is why control 5 is
  correctly NOT CAUGHT and why the loop bounds are transcribed unhoisted).
* **The probe's arena bound message is approximate.** `ArenaFits()` is exact;
  the printed hint "(groups<=21, groups*states<=54)" is a rounded description of
  three separate inequalities. A user who believes the hint rather than the
  refusal will mis-plan a sweep cell, not get a wrong verdict.
* **Byte sizes for `?Draw@Renderer@` (921) and `?BuildParameters@` (1422) are
  Wave 21's numbers, not re-measured here.** Wave 21 derived them by summing
  `.pdata` unwind-chain fragments after catching its own single-record error
  (`reference_pdata_is_a_lower_bound_on_function_extent.md`), and confirmed four
  of thirteen against Ghidra. `Draw` was one of the four confirmed;
  `BuildParameters` was **not**. My refusal of `BuildParameters` in §6.3 rests
  partly on an unconfirmed size — it would have to be much smaller than 1422 to
  change the verdict, but I did not check.
* **I did not read `?BeginDrawGroup@Renderer@`, `?AddCulledDrawItems@Scene@`, or
  `0x180062d60`.** The claim that porting `Draw` grows the surface by two rows
  is from the census `called` column; the claim that `0x180062d60` is genuinely
  an unexported internal rather than an export I failed to match is the weaker
  kind (`reference_grep_cannot_tell_internal_from_unported_export.md`) — it is
  absent from the directory *by address*, which is not the same question.
* **The `??_7FrameCopyImageBuffer@` vtable's slot contents beyond slots 0–7.**
  I read `0x180154f00` and corroborated the table's identity through
  `saveload_draw.cpp:83`'s independent walk of slots `+0x48`/`+0x50`. I did not
  enumerate all nine or twelve slots myself.
* **The `ANIM_BATCH_ALL` path** (`standalone/perf_experimental.cpp`) — the
  second half of the Wave 6 `AnimationManager::Update` holdback. I argued it is
  not a blocker on this body (it changes the function's INPUT, not its
  contract), but I did not read the file.
* **Any of the 8 VTABLE or 7 DATA Graphics rows.** Out of scope by assignment.
* **Whether the four FCIB bodies behave correctly when the allocation fails.**
  The failure paths are transcribed faithfully (they fault, as retail does) and
  are not exercised by anything.

---

## 10. RESIDUALS — each with a NAMED addressee (rule 11)

**R1 — DISCHARGED BY THIS LANE.** The run happened (§7): all five bodies
exercised on a live device, the kill switch proved a real control, clean exits
and 0-byte crash logs throughout. Left in the list so a reader who saw the
earlier draft does not go looking for an open item. **No addressee needed.**

**R6 — THE SCREENSHOT ROUTE IS NOT FRAME-DETERMINISTIC, AND THAT COSTS EVERY
FUTURE RENDERING LANE ITS ORACLE.** §7.5: five runs with identical seed, camera
pin and command line split into two clusters 98% apart, and the split is
independent of the flag under test. `BANISHED_SEED` + `BANISHED_CAMERA_PIN` are
NOT sufficient for a reproducible frame, while CLAUDE.md's "Camera Pinning
(deterministic screenshots)" section reads as though they are — it says a wrong
pin "costs one probe run", which assumes the framing is otherwise stable. Either
a third knob is needed (pin the world clock / weather at the screenshot frame)
or that section needs a warning. **This is the blocker on every pixel-level
verification, including the FCIB four above and the one lane SHOT exists to
build.**
**ADDRESSEE: the Wave 24 kickoff writer.**

**R2 — `?Draw@Renderer@` + `?BuildParameters@MaterialInstance@` are a lane, not
an add-on.** 2343 bytes, 620 instructions, 16 calls between them, and `Draw`
grows the cutover surface by 2 exported rows plus 1 unexported address door.
**ADDRESSEE: the Wave 24 kickoff writer** — scope them as one lane named for
those two rows, and do not put them on a "cheap wins" list
(`reference_cheap_wins_list_is_the_stalest_section.md`).

**R3 — The two `Serialize` rows need an IO/Serializer lane, not a Graphics
one.** §6.2. The four unexported `Collection::Array<T>` serialize helpers are
the work; the Encoding `+0xa0`/`+0xb8` offset is a documented transcription trap;
and the "no oracle" half should be re-argued against
`reference_write_path_serialize_is_replayable.md` rather than repeated.
**ADDRESSEE: the Wave 24 kickoff writer**, to be scoped alongside lane IOPORT's
subject rather than under Graphics.

**R4 — `runtime/types/graphics/Animation.h` `+0x10` is still UNVERIFIED.** No
body this project has read touches it: not the ctor, not the dtor, not `Update`.
`?Serialize@AnimationManager@` (`0x1800fc190`) probably does and is unported.
**ADDRESSEE: whoever takes R3** — answer it from that body, and delete the
UNVERIFIED note.

**R5 — `tools/gfxbody23_probe/` is a shape worth reusing and nothing points at
it.** It is a general recipe: compile the shipping body, detour the callee,
one arena run twice. Any pure-ish CPU-side Runtime body with an exported callee
can be verified this way with no game directory — which matters because the
directory is a single resource and Wave 23 had ten lanes.
**ADDRESSEE: the Wave 24 kickoff writer**, to name it in the kickoff's tools
section so the next ten lanes do not each conclude "no run, therefore
STRUCTURAL".

---

## 11. CROSS-LANE ITEMS HANDED TO ME BY THE COORDINATOR

### 11.1 The stale `scene.cpp` bullet — DONE

`runtime/src/graphics/scene.cpp`'s **VTABLE OWNERSHIP** block, bullet 3, pointed
at `code/Game/Component/statusicon_geometry.cpp:335` and described a loop Wave 22
lane GEOM has since rewritten. **Verified independently before editing**: the
builder is `StatusIconGeometry_BuildVtable` at :336, and the copy/null split is
at :393 reading `if (i < 9)`. The bullet now carries the corrected line, the
corrected extent (nine slots, not twelve), and an explicit statement that it is a
**standing blocker on minting `??_7Geometry@Graphics@@6B@`**, not a note.
**Bullets 1 and 2 are untouched** — they are the load-bearing ones and I agree
with GFXVT that the whole block must not be tidied away.

### 11.2 `??_7MaterialInstance@Graphics@@` — HANDED BACK, with a measurement that
changes its priority

GFXVT is right that this is the only Graphics vtable whose promotion would change
a live dispatch, and right that it is 4 slot bodies. I measured them, and
**"four slot bodies" is right in count and wrong in cost**:

| slot | symbol | retail | shape |
|---|---|---|---|
| 6 | `?Reconstruct@MaterialInstance@` | `0x180002a50` | trivial — one virtual call then ~14 zero stores |
| 7 | `?Destruct@MaterialInstance@` | `0x180002b50` | **one instruction of work**: tail-call slot 0 with flag 0 |
| 8 | `?Construct@MaterialInstance@` | `0x180002ad0` | trivial — the same ~14 zero stores |
| 5 | `?Serialize@MaterialInstance@…Stream@IO@@` | `0x180103a70` | **NOT trivial** |

Slot 5 dispatches through the Encoding vtable at `+0xa0` (the retail-vs-profile
offset trap), calls **four unexported internals** (`FUN_180061100`,
`FUN_180061880`, `FUN_1800616c0`, `FUN_18005d0f0`), and calls
**`?BuildParameters@MaterialInstance@`** — which is the largest unported row on
my own board and which I refused in §6.3.

So the shape of the job is three trivial bodies plus one that is the same
Serializer problem as R3. **And three of four unblocks nothing**: the mint needs
all nine slots, so a partial is worth zero.

**I am handing it back rather than taking it.** Not because it is out of my
namespace — it is squarely body work in `runtime/src/graphics/` — but because
taking four unscoped rows late in a lane, with no oracle and no run, on the
strength of a "highest-leverage" framing, is precisely
`reference_cheap_wins_list_is_the_stalest_section.md`. The measurement above is
what makes it schedulable: it is **a small lane**, and it should be scoped with
R3, because slot 5 and R3 are the same Serializer/Array-helper problem in two
places.

**ADDRESSEE: the Wave 24 kickoff writer**, as one named lane —
"`??_7MaterialInstance@` completion": 3 trivial slot bodies + `Serialize`, with
the `Collection::Array<T>` serialize helpers shared with R3.

### 11.3 The GFXDATA→GFXVT claim about abstract tables

I did not re-derive GFXVT's refutation and I am not endorsing it — I have no
independent evidence either way. I note only that it does not bear on any row of
mine, and that I have applied its lesson in §7 by checking the coordinator's own
lock claim rather than repeating it.
