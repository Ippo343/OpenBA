# WAVE 29 — LANE AVBODY

**Subject:** Audio's 3 remaining BODY rows and Video's 2.
**Branch:** `wave29/avbody`, cut from `wave29/base` (`c11511a1`).

**Headline:** 5 bodies ported and registered, **49 sweep comparisons against the
shipped exports with zero divergences and 6/6 negative controls caught**. Video's
body surface is **closed** (the census now prints `AT CEILING`); Audio's goes 3 → 1.
One body (`PlayPlatform`) declined, decoded in full, with an oracle design and an
addressee. One of the five was not on the work list and could not have been.

---

## 0. WHAT THIS DOES NOT CLAIM

Read this before any figure below.

1. **It does not claim `?PlayPlatform@Device@Audio@@` is ported.** It is not. It
   is decoded in full (§6) and declined, with the obstacle class named and an
   addressee. Four of the five named bodies landed, not five.
2. **It does not claim the two VIDEO bodies have ever run in play.** They have
   not, on any run I made. Their attendance of 8 each is **my own sweep's 8
   cases** and nothing else; the live route through `Material::GetConstantName`
   exists but was not exercised. The three AUDIO bodies *are* live (over a
   million calls, §4) — that asymmetry is real and is not smoothed over.
   Separately: **correctness here comes from the sweep, never from attendance.**
   A body running a million times says it RAN, not that it is RIGHT.
3. **It does not claim the sweep covers everything the bodies do.** It compares
   a named list of terms (§3.1). A term not on that list is not checked, and the
   list is short precisely so that it can be read.
4. **It does not claim the two new `SoundEffect` field names are right.** `+0x20`
   and `+0x24` are named `maxDistanceBase` / `minDistance` from their ROLE in one
   body's arithmetic. **No writer was found.** Only a writer names a field; these
   are reader-only and are flagged as such at the site.
5. **It does not claim retail's `OnSoundEnded` out-of-array write (§5.2) is a bug
   in any sense that licenses changing it.** It is transcribed. Whether retail
   intends it is *unknown* — I found no writer of `platformChannel[7]`'s top byte
   that would settle it.
6. **It does not claim the +0.2 points of TOTAL are the result.** See §7: my lane
   *adds* a row to `called` as well as to `ported`, and the figure is not
   comparable to lane INSTR's re-based board anyway. The identity is Video's
   body surface closing, not the percentage.
7. **It does not claim `AttenutedEffect` and `EffectSlot` are the same type on
   the strength of the name.** They agree at five offsets (§5.1). That is strong
   but it is still an inference, and I did not rename the type.
8. **The IOLANE cross-check (§8) validated the EXISTENCE of a hidden return slot,
   not its POSITION.** The position claim rests on my own reading of the bytes,
   plus agreement with an already-landed Wave 28 body. IOLANE has since added a
   third mechanism that reproduces the position independently, but it did so
   *after* I named the defect, so it corroborates rather than co-discovers.
9. **It does not claim the sweep would catch a defect in a case it does not
   run.** Two controls caught 2/8 and 1/2 respectively — i.e. most cases are
   insensitive to most defects. The sweep's power comes from specific cases
   (poisoned slot, negative index, negative baseVolume, not-found channel), and
   a defect outside those shapes would pass.

---

## 1. WHAT LANDED

Six exports registered across two new files.

| symbol | VA | ord | extent | file |
|---|---|---|---|---|
| `?GetConstantName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z` | `0x18004e4b0` | 2507 | 0x44 | `runtime/src/video/shader_program_names.cpp` |
| `?GetTextureName@ShaderProgram@Video@@QEBA?AVString@System@@H@Z` | `0x18004e420` | 2971 | 0x44 | same |
| `?GetAttenuatedVolume@Scene@Audio@@AEBAM...` | `0x1800dfde0` | 2453 | 0xea | `runtime/src/audio/wave29_lane_avbody.cpp` |
| `?OnSoundEnded@Scene@Audio@@SAX...` | `0x1800dffe0` | 3283 | 0x5e | same |
| **`?GetVolumeForPosition@Scene@Audio@@AEBAM...`** | `0x1800dfed0` | 3027 | 0x94 | same — **not on the work list, see §2** |

Kill switches: `BANISHED_RT_VIDSHADERNAME`, `BANISHED_RT_AVBODY29` (both default
ON). Sweeps: `BANISHED_RT_SHADERNAME_SWEEP=1`, `BANISHED_RT_AVBODY29_SWEEP=1`
(both opt-in — they are verification, not boot work).

**Every extent above was derived from the byte stream** (first `CC CC`
inter-function padding), never from `.pdata`. All four agreed with Ghidra's
bounds. Lane SLOTS measured a 70.3x `.pdata` understatement this wave; I did not
rely on it anywhere.

---

## 2. THE FINDING: A SIXTH BODY THAT NO CENSUS CAN SEE

`GetAttenuatedVolume` calls a helper at `0x1800dfed0` twice. **I was one
keystroke from writing "an unexported internal, inlined here as a file-local
static."** That would have been wrong in a way this project has already paid for
three times: **`AEBA` means PRIVATE, not unexported, and this DLL exports its
private members** (`reference_aeaa_is_private_not_unexported.md`).

I checked instead of asserting — a `pefile` lookup of every callee VA in my
subject against the export directory:

```
0x1800dfed0  EXPORTED  ord 3027  ?GetVolumeForPosition@Scene@Audio@@AEBAMAEBVVector3@Math@@MM0@Z
0x1800a0d10  EXPORTED  ord 3417  ?Play@Stream@Audio@@QEAAXHH@Z
0x1801fe728  EXPORTED  ord 4797  ?_secondsPerTick@Timer@System@@0NA
```

**So this lane's entire subject has ZERO address doors.** Every callee is a name
door and nothing here opens a new cutover door. Had I inlined the helper, I would
have (a) hidden a real exported body from the board forever, and (b) written a
second copy of arithmetic that already had a canonical home.

**Why no census will ever surface it:** the census's population is "what our tree
resolves." Nothing resolved `GetVolumeForPosition`, so it was never in the
denominator. It is not a census defect — it is the population definition working
as designed. The generalisable form: **the callees of an unported body are
invisible until that body is ported, so every body lane should expect to find
work that was not on its list.**

The mangled signature then confirms the decode from the **writer's** side rather
than the reader's: `AEBA M` = private const member returning float;
`AEBVVector3@Math@@` = `const Vector3&`; `M M` = two floats; `0` = MSVC's
back-reference to argument type 1, i.e. a second `const Vector3&`. Exactly
`(srcPos, maxDist, minDist, listenerPos) -> float`, which is what the instruction
stream says.

---

## 3. VERIFICATION

### 3.1 The method, and why no gameplay route was needed

`GetVolumeForPosition` and `GetAttenuatedVolume` are **pure**: the `this` they
receive is never read (RCX is dead in the helper; the outer body only forwards
it). `OnSoundEnded` is not pure but is fully synthesisable. The two Video getters
are pure apart from a refcount increment.

So all five verify by **input sweep against the shipped export**
(`project_pure_functions_verify_by_sweep.md` — the method that verified
`GetAlignedPosition` after it reported `calls=0` in a 16.9M-call run).
**"No gameplay route" is not "no oracle."**

Every retail side goes through **`ResolveExportRaw`**, never `ResolveExport`. The
latter consults the port registry first and would hand back *my own* function
once registration lands, turning the oracle into a self-check that agrees with
itself forever (`reference_shadow_marker_is_not_an_oracle.md`).

**The compared terms, stated so a reader knows what is NOT checked:**

| body | compared terms |
|---|---|
| ShaderProgram getters | `ret.data`; `returned == &ret`; 4 string refcounts; all 128 record bytes |
| `GetVolumeForPosition` | the returned float's **bit pattern** (not `==`, so NaN compares equal) |
| `GetAttenuatedVolume` | same |
| `OnSoundEnded` | the **entire** 4 × 0x70 channel table, `memcmp` |

The `OnSoundEnded` screen compares the whole table on purpose: that is what makes
retail's out-of-array `+0x5f` write (§5.2) a *compared* term rather than
something the screen would have to be told to look for in advance.

Float comparison is **bit-exact**, deliberately. The defect worth catching is a
reassociated float sum, which differs in the last bit or two — exactly what an
epsilon comparison waves through.

### 3.2 Results

Measured on a clean `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120` run, sweeps armed with
`BANISHED_RT_SHADERNAME_SWEEP=1 BANISHED_RT_AVBODY29_SWEEP=1`:

```
[AVBODY29] RESULT: compared=16 divergences: GetConstantName=0 GetTextureName=0 ; controls caught 3/3
[AVBODY29] RESULT: compared=33 divergences: GetVolumeForPosition=0 GetAttenuatedVolume=0 OnSoundEnded=0
```

**49 comparisons, ZERO divergences, all five bodies.** Read that with §0.3: it
means the five bodies agree with the shipped exports on the terms listed above,
over the cases listed in the source. It does not mean more than that.

### 3.3 Negative controls, scored separately

Five controls, each targeting **one** compared term, each scored on its own — a
positive control scored on the union of two screens reads 3/3 and hides that one
screen caught 1/3 (`reference_union_control_hides_a_blind_screen.md`).

| # | control | targets | why it is the right shape |
|---|---|---|---|
| 1 | omit the unconditional `*ret = nullptr` | `ret.data` | invisible to any case whose slot starts zeroed, so the sweep pre-fills the slot with **poison** |
| 2 | read the wrong element (`index^1`) | `ret.data` | bounded stand-in for index arithmetic |
| 3 | omit the refcount `INC` | refcounts | the refcount is a side effect; a return-value-only screen is blind to it |
| 4 | multiply by `baseVolume` **inside** the loop | returned float | the moved-term defect |
| 5a | guard away retail's `+0x5f` write | channel table | a screen looking for the PRESENCE of a write must be handed a case where it is ABSENT |
| 5b | set bit 2 instead of bit 1 in `channelFlags` | channel table | reaches `channelFlags[index]`, which 5a never does — see below |

**Control 4 is the interesting one and its expected result is a SPLIT, not a
pass.** Moving `* baseVolume` into the loop is invisible when `baseVolume > 0`
and visible when `baseVolume < 0`, because `max(a,b)·v ≠ max(a·v, b·v)` for
`v < 0`. A control that fired on every case would mean I had built the *wrong*
control. The sweep therefore includes negative-`baseVolume` cases specifically.

**All six controls CAUGHT, scored separately:**

```
control "omit the unconditional *ret=nullptr"        (ret.data):      CAUGHT (2/8)
control "read the wrong element (index^1)"           (ret.data):      CAUGHT (6/8)
control "omit the refcount INC"                      (refcounts):     CAUGHT (5/8)
control "baseVolume multiplied INSIDE the loop"      (float):         CAUGHT (2/4)
control "guard away retail's +0x5f write"            (channel table): CAUGHT (1/2)
control "set bit 2 instead of bit 1 in channelFlags" (channel table): CAUGHT (2/2)
```

**Control 4 came back 2/4 — exactly the predicted split**, and the split is the
result, not the count. The two caught cases are the negative-`baseVolume` ones;
the two positive-`baseVolume` cases are genuinely identical. A control that fired
on all four would have meant I had built the wrong control.

**Control 5a came back 1/2, and 1/2 is correct**: the guard only changes the
not-found path, so the found case is identical by construction.

### 3.4 THE CONTROL THAT FAILED, AND WHY IT INDICTED ITSELF

**5b was originally "write the flag as a DWORD instead of a BYTE" — the
`Camera::operator=` width defect — and it reported `*** NOT CAUGHT — SCREEN IS
BLIND ***`.** That looks exactly like a blind screen. It was not.

```
byte-OR :  a5 a5 a7 a5 a5 a5 a5 a5
dword-OR:  a5 a5 a7 a5 a5 a5 a5 a5      <- identical
```

`OR` with the constant `2` touches **only bit 1 of the lowest byte, at any
width**, because the constant's upper bytes are zero and `OR` with 0 is the
identity. The two spellings produce byte-identical memory. **My control corrupted
nothing at all**, so "NOT CAUGHT" indicted the control, not the screen — the
purest possible form of *a negative control that changes nothing reports NOT
CAUGHT and indicts a sound instrument*.

**And the real conclusion is about the BODY, not the control: the write-width
trap does NOT apply to `OnSoundEnded`.** It bites where retail **stores** — the
Video pair's 8-byte `MOV [RDX],R9` beside its 4-byte `INC dword [RCX-4]` — not
where retail does a read-modify-write with a small constant. I had asserted the
opposite in the source banner; **that comment was wrong and has been corrected in
place, with the reasoning, rather than deleted.** Applying kickoff §2.2's
"compare every write width" as a blanket rule would have recorded a hazard here
that cannot exist.

The replacement (bit 2 instead of bit 1) is observable and reaches
`channelFlags[index]` — which 5a never does, since 5a only ever differs on the
not-found path. With both, the screen is demonstrably sensitive to **both**
halves of the flag region.

**One control I deliberately did NOT build, and the honest reason.** The defect
most worth modelling on the Video pair is `MOVSXD` read as a zero extension. With
a negative index that computes `base + 0xFFFFFFFF*16` — a ~64 GB wild read. It
would **fault, not diverge**, so the screen would never report and a "CAUGHT"
would be a lie about the mechanism. The sign-extension property is instead
checked the honest way: cases 6 and 7 use negative indices and compare against
**retail**, which is the oracle for that behaviour.

---

## 4. ATTENDANCE, AND A REPRODUCTION OF LEDGER DEFECT #7

**Defect #7 ("every lane's `*Summary()` is written and nothing calls it")
reproduced on my own subject, and it would have cost me a wrong conclusion.**

Wave 28 lane GFXBODY ported `?GetConstantName@Material@Graphics@@`, whose whole
body forwards to *my* `ShaderProgram::GetConstantName`. So a live route already
exists and GFXBODY already counts it in `g_getConstNameCalls`. But:

```
$ grep -rn 'RtGraphicsWave28GfxbodySummary' --include=*.cpp --include=*.h .
runtime/src/graphics/wave28_lane_gfxbody.cpp:899   (the definition)
runtime/src/graphics/wave28_lane_gfxbody.cpp:912   (a thunk to it)
```

**Zero callers.** `RtGraphicsWave28GfxbodyRegister` *is* called from
`rt_graphics_port.cpp:187`; the summary beside it is not called from anywhere.
Had I read that counter I would have seen 0 and concluded "never called."

**What I did instead.** Both my files print attendance from a **static
destructor** — the mechanism that actually produced Wave 28's single measured
figure (`audio_scene_update.cpp`'s `UpdateReport`, 799 calls) — and print
**unconditionally**, even when every counter is zero. `audio_scene_update.cpp`
guards its report with `if (g_updates)`, which makes "never ran" and "ran and
counted zero" the *same silence*. That silence is how defect #7 stayed invisible.

Measured, same run:

```
[AVBODY29] Audio ATTENDANCE: GetVolumeForPosition=1052286 (far=774381 near=261373) ;
                             GetAttenuatedVolume=2896 (single=2 loopIters=1052249) ;
                             OnSoundEnded=16 (found=9 notFound=4 rejected=3)
[AVBODY29] ShaderProgram ATTENDANCE: GetConstantName calls=8 (nameNonNull=5) ;
                                     GetTextureName calls=8 (nameNonNull=5)
```

**Read these two lines very differently.**

* **Audio is genuinely live.** `GetVolumeForPosition` ran **1,052,286** times on
  an ordinary autostart run, and both of its branches were taken in quantity
  (774,381 beyond max distance, 261,373 inside min distance). `GetAttenuatedVolume`
  ran 2,896 times and its loop body 1,052,249 times. These are not registered-and-
  unexecuted bodies; they are in the per-frame audio path.
* **The two Video counts are MY OWN SWEEP, not gameplay.** The sweep runs 8 cases
  per body, and 8 is exactly what the counter shows. **The live route through
  `Material::GetConstantName` was not exercised on this run at all.** I am
  stating that rather than quoting "8 calls" as attendance, because the number is
  real and the inference "therefore it runs in play" would be false. The two
  Video bodies remain **verified but not exercised in play** — they join the
  ~280-body verification debt, with an oracle attached, which is strictly better
  than joining it without one.

**`OnSoundEnded` notFound=4 is the finding.** Retail's out-of-array `+0x5f` write
(§5.2) **fired four times on an ordinary gameplay run.** It is not a theoretical
path reachable only by a synthesised fixture; normal play takes it. Whatever it
is, it happens.

---

## 5. WHAT THE BYTES SAID THAT THE C DID NOT

### 5.1 `AttenutedEffect` is the struct `Audio.h` calls `EffectSlot`

Retail's own name, typo and all, is readable in the export directory:
`AEBUAttenutedEffect@12@`, where `12` back-references `Scene@Audio`. So the type
is `Audio::Scene::AttenutedEffect`. Five independent offsets agree with
`Audio.h`'s `EffectSlot`:

```
+0x00  Vector3 pos      <- helper reads [RDX+0], [RDX+4], [RDX+8]
+0x10  SoundEffect ref  <- MOV RAX,[RDX+0x10]
+0x28  baseVolume       <- MULSS XMM6,[RBP+0x28]
+0x30  positions._data  <- MOV RBX,[RDX+0x30]
+0x38  positions._size  <- MOV EAX,[RDX+0x38]
```

I did **not** rename the type: six files depend on it and that is not this lane's
change to make.

> **HOLDBACK — ADDRESSEE: the next lane that edits
> `runtime/types/audio/Audio.h` for any reason.** Rename `EffectSlot` to
> `AttenutedEffect`, citing this section. Not "whoever next counts Audio types" —
> that names a condition and no one, and waits forever.

### 5.2 `OnSoundEnded` writes one byte OUTSIDE the array it indexes

```
1800e0008: OR  R8D,0xffffffff      ; R8D = -1
   ...                             ; the 8-slot search runs and finds nothing
1800e0028: MOVSXD RAX,R8D          ; RAX = -1, SIGN-extended
1800e002b: OR  byte [RAX + R11*1 + 0x60],0x2
```

The not-found path ORs `2` into `channel + 0x60 - 1` = **`channel + 0x5f`**.
`Audio.h` (derived in an earlier wave, from different functions) puts
`platformChannel[8]` at `+0x20..+0x5f` and `channelFlags[8]` at `+0x60..+0x67`.
So `+0x5f` is **not padding**: it is the most significant byte of
`platformChannel[7]`. Retail sets bit 1 of the top byte of an interface pointer.

**Transcribed, not fixed** (CLAUDE.md: a guard skips the behaviour rather than
restoring it). Flagged loudly at the site so a later reader does not diagnose it
as an off-by-one in the *port*. Control 5a exists to prove the screen can see it.

The write is a **BYTE** (`OR byte ptr`) on both paths — the `Camera::operator=`
width class, live here. Control 5b covers it.

### 5.3 Two hoisted terms that must stay hoisted

In `GetAttenuatedVolume`, retail computes `maxDist = radiusBias + base` **once**
before the loop, and applies `* baseVolume` **once after** the max-reduction.
The second is not merely a rounding question: it reorders the result whenever
`baseVolume` is negative. Both are transcribed in retail's positions and control
4 measures the difference.

Also: `count < 0` skips the loop and leaves `best = 0.0f`, which is **still**
multiplied by `baseVolume`. An early `return 0.0f` would be wrong, because
`0.0f * baseVolume` is not `0.0f` when `baseVolume` is NaN or infinite.

### 5.4 Constants, lifted and not retyped

| VA | bits | value | role |
|---|---|---|---|
| `0x18016169c` | `0x3f800000` | `1.0f` | the falloff ceiling |
| `0x1801616d8` | `0x40000000` | `2.0f` | `MaxFrequencyRatio` (PlayPlatform) |
| `0x1801616a0` | `0x3f879c7d` | `2^(1/12)` | the semitone ratio (PlayPlatform) |
| `0x1801fe728` | — | **not a constant** | `?_secondsPerTick@Timer@System@@0NA`, **zero in the file**: `.data`, initialised at runtime |

The last one matters: it reads like a constant in the disassembly and is not one.
Baking its file value would bake **zero** and divide by it.

`2^(1/12)` also identifies `PlayPlatform`'s 6th argument as a **pitch in
semitones** — recovered from the constant, not from a name.

The CRT-coefficient rule does not bite here: the only transcendental reached is
`sqrtf`, and IEEE-754 specifies `sqrt` as correctly rounded, so our libm and the
DLL's agree bit-for-bit. That is *why* this is the one case the sweep can compare
without `BANISHED_RT_LIBM=dll`.

---

## 6. WHAT DID NOT LAND: `?PlayPlatform@Device@Audio@@`

**Obstacle class: INTRINSIC.** Not a missing capability, so it does not expire.

`PlayPlatform` creates a real XAudio2 source voice, submits a real audio buffer,
sets volume and frequency ratio, and starts playback. Its effects are audible and
live in the driver. `WAVE9_LANE_ENGIO.md` §5 reached the same verdict by a
different route ("the device call sequence is UNCOMPARABLE ... in OURS it would
be a resolved call the detour catches, and in THEIRS a NEAR CALL inside the DLL
that the detour misses").

I re-checked that blocker rather than inheriting it, per warning 3 — and **one
half of it has expired.** ENGIO's stated next step was:

> "It is a lane that takes `PlayPlatform` and `OnSoundEnded` first, after which
> both sides call OUR `PlayPlatform` and the sequence becomes comparable for real."

**`OnSoundEnded` is now ported and registered (§1), so half of that prerequisite
is met.** The remaining half is `PlayPlatform` itself.

**I declined it as a size-and-evidence judgement, stated as such rather than
dressed as a blocker:** 632 bytes, six indirect XAudio2 calls, no oracle, and it
mutates audio hardware. Writing an unverifiable body of that size default-OFF
would add risk without adding evidence, and this wave already records that
*retail ships empty bodies* and that porting on momentum is how defects land.

### 6.1 The decode, which is the expensive part and is done

Register layout: `RCX` = `Device*`, `EDX` = slot index (sign-extended),
`R8` = `ExternalPtr<SoundBuffer>` **by value, 8 bytes, and its payload is the
`SoundBuffer*`**, `R9` = `const Time&`, then three floats at `[RSP+0x28/0x30/0x38]`
(volume, **semitones**, and a third float that is only forwarded).

```
[this+0xa88]  the XAudio2 engine object; NULL here => return false immediately
[this+0xa98]  voice-callback records, stride 0x20; +0x08 = the source voice
              (agrees exactly with Wave 28 lane SLOTS, which found this from a
              different subject: SLOTS' Device+0xa98 +0x08 == the +0xaa0 that
              audio_scene_update.cpp reads)
[this+0xa00]  an Audio::Stream  (proved: the tail call is ?Play@Stream@Audio@@)
```

The engine call is `[engine_vtbl+0x40]` with **seven** arguments whose shape is
uniquely `IXAudio2::CreateSourceVoice`: `(ppSourceVoice, pSourceFormat, Flags=0,
MaxFrequencyRatio=2.0f, pCallback=&record, pSendList=0, pEffectChain=0)`.
`pSourceFormat` is `buffer+0x54`, i.e. the `WAVEFORMATEX` — which independently
confirms `gameupdateloop.cpp:1699`'s existing note.

**An unresolved fact I am not going to paper over.** The returned voice is a
*standard* `IXAudio2SourceVoice`: five vtable offsets match the documented layout
exactly — `+0x60` SetVolume, `+0x90` DestroyVoice, `+0x98` Start, `+0xa8`
SubmitSourceBuffer, `+0xd0` SetFrequencyRatio. (`+0x60` = SetVolume was reached
independently by `audio_scene_update.cpp` from a different body.) But
`CreateSourceVoice` at `+0x40` matches **neither** XAudio2 2.7 (`+0x38`) nor
2.8+ (`+0x20`). So the object at `Device+0xa88` is **not a bare `IXAudio2` of
either documented version**, and I am not naming it. The port does not need the
name — offset and argument shape are enough — but a later lane should not read
"IXAudio2" into it on my authority.

The arithmetic half: elapsed ticks are divided by the buffer's duration
(`[buf+0x24]` seconds ÷ `_secondsPerTick`); the remainder becomes a byte offset
via `nChannels · wBitsPerSample · nSamplesPerSec / 8`; a table at `[buf+0x48]`
with `[buf+0x50]>>2` entries is searched for the first entry **strictly greater**
than that offset, giving `PlayBegin = nBlockAlign · index`. Note `MOVSXD` then a
**logical** `SHR` on the sign-extended value — a genuine sign/zero mix that
Ghidra's C will not show.

`XAUDIO2_BUFFER.pContext` (`+0x78` of the local) is **never written** by retail:
it is passed as uninitialised stack. Any port must decide whether to reproduce
that, and silently zeroing it is a divergence, not a tidy-up.

### 6.2 The oracle that would retire the INTRINSIC verdict

I did not build this, but it is feasible and it is the real handover:

**Synthesise the backend.** Build a fake `Device` whose `+0xa88` points at a
fake engine object with a fake vtable whose `+0x40` returns a fake voice with a
fake vtable. Then call **retail's** `PlayPlatform`. It will call *my*
`SubmitSourceBuffer` and hand me the exact `XAUDIO2_BUFFER` and
`XAUDIO2_BUFFER_WMA` it built, plus the volume and frequency-ratio arguments — no
hardware, no audio, no driver. Compare against mine on an identical fixture.

That covers everything except the actual XAudio2 effects, which are the part
nothing can compare. It would convert "UNCOMPARABLE" from a property of the
function into a property of one *residual* call, which is a much smaller claim.

> **HOLDBACK — ADDRESSEE: the Wave 30 lane that takes `?PlayPlatform@Device@Audio@@`.**
> Start from §6.1 (the decode is done) and build §6.2's fake-backend oracle
> *before* writing the body, not after. Do not inherit "no oracle is possible"
> from `WAVE9_LANE_ENGIO.md` — that verdict was reached before `OnSoundEnded`
> existed as a port and before anyone costed a synthesised backend.

---

## 7. THE BOARD, AND WHY ITS MOVEMENT IS NOT MY SCORE

`ceiling = (called − unpromotable) / called` has `called` in **both** halves.
`GetVolumeForPosition` was outside the population entirely; porting it adds **+1
called and +1 ported**, which moves Audio's percentage by less than porting
nothing would suggest, and my lane's honest identity is not a percentage.

**The identity I am scoring this lane on:**

> Video's remaining BODY surface goes 2 → 0, Audio's goes 3 → 1, and the one that
> remains is the one with an INTRINSIC blocker and a written oracle design.

**Measured with the SAME tool on both sides — my branch and `wave29/base` — so
the delta is valid even though the absolute figure is not comparable to lane
INSTR's.** (INSTR fixed `mint_slot_sweep.py` mid-wave and re-based the board to
1460/1295 88.7%. My numbers below come from the census as it stands on *my*
branch, which does not carry that fix. Quoting my 89.3% against INSTR's 88.7%
would be comparing two different instruments — the "same tool on both sides"
rule cuts both ways.)

```
                 total  called  ported          ceiling
wave29/base   Audio  137     35      29   82.9%   94.3%
              Video  573    162     142   87.7%   88.9%
              TOTAL 4632   1453    1294   89.1%

wave29/avbody Audio  137     36      32   88.9%   94.4%
              Video  573    162     144   88.9%   88.9%  AT CEILING
              TOTAL 4632   1454    1299   89.3%
```

Delta: **+1 called, +5 ported.** The +1 is `GetVolumeForPosition` entering the
population for the first time; the other four were already in `called`. +5 ported
is exactly the five registrations, and the arithmetic reconciles per namespace
(Audio +1 called / +3 ported, Video +0 / +2).

**`Video` now prints `AT CEILING`.** Its ported% and its ceiling are both 88.9%:
the entire remaining Video gap is the 18 rows already proved unpromotable, and
**Video's body surface is closed.** That is the identity this lane should be
scored on, not the 0.2 points of TOTAL.

---

## 8. PAIRING WITH IOLANE

### 8.1 What IOLANE's instrument found on my subject

IOLANE self-controlled (I had not reported) and ran its return-slot screen
against my two Video rows, with two independent mechanisms (M1 = the mangled
return spelling; M2 = does the body's instruction stream reference RDX at all)
and a known-positive / known-negative pair scored separately.

**Result: `BYVAL` on both of my rows, both mechanisms agreeing, extent 0x44 for
both.** That **reproduced** my own two findings by independent means: I derived
0x44 from `CC CC` padding, IOLANE from disassembling to the first `int3` after a
`ret`; neither used `.pdata`.

IOLANE also **corrected my brief and its own**: the kickoff paired us on a
"shared hidden-return-slot problem," and IOLANE measured that its four bodies are
two `AEBV` references and one `H` (an int). **The hidden-slot trap was mine
alone.** The pairing was still worth it — just not for the stated reason.

### 8.2 The defect I found in IOLANE's instrument — DEFLATING, in the wild-write direction

IOLANE's screen is sound on the question it asks. Its **prescription** is wrong:

> prescribed: `String* (*)(String* retSlot, const ShaderProgram* self, int index)`
> — "the hidden slot FIRST and `this` SECOND"

The bytes say the opposite:

```
18004e4d0: ADD RAX,qword ptr [RCX + 0x30]   ; RCX is DEREFERENCED at +0x30 -> RCX is `this`
18004e4d4: MOV qword ptr [RDX],R9           ; RDX is WRITTEN THROUGH      -> RDX is the slot
18004e4ec: MOV RAX,RDX                      ; the slot comes back in RAX
```

An object is dereferenced through `RCX`; a slot is written through `RDX`. The
correct typedef is `String* (*)(const void* self, String* ret, int32_t index)` —
**`this` first, slot second**. That is MSVC x64's rule for a *member* function
returning a class by value; the "return pointer first, everything shifts right"
rule IOLANE applied is the *free-function* case. Independent agreement: Wave 28
lane GFXBODY's already-landed `Material::GetConstantName` forwarder uses exactly
this order and has been running since last wave.

**Why IOLANE's own controls could not catch it, which is the transferable part:**
both mechanisms answer *"IS there a hidden slot?"*. Neither answers *"WHERE does
it sit?"*, and the known-positive/negative pair only exercises the first
question. **A control that scores a binary present/absent verdict cannot validate
an ORDERING claim derived beside it** — the ordering rode along on the screen's
credibility, which is the "a measured number sitting beside an asserted one lends
it credibility" failure in a new costume.

Direction: **DEFLATING for the caller.** Following it puts `self` in RDX and the
caller's `ret` in RCX — the body then dereferences the caller's String slot as a
ShaderProgram and writes 8 bytes through `self`. That is precisely the wild write
the screen exists to prevent.

IOLANE separately self-reported a **DEFLATING** defect in its own M1 (a
`sym.rfind("QEBA")` fallback returning `-1` for a `UEBA` symbol, misreporting a
by-value return as a reference), caught **only** by the known positive. Recorded
here because it is the same lesson from the other side: hand an absence-screen a
positive it would have to work to pass.

### 8.3 What I could not corroborate

IOLANE asked whether any of my bodies contains an inlined `System::String`
**release**. **None does** — so I cannot corroborate its `MemFree` idiom, and I
said so rather than manufacturing agreement. I have the **adopt** half only, and
it matches IOLANE's exactly (refcount at `[ptr-4]`, DWORD, incremented after a
null test), which is a second derivation from a different namespace.

---

## 9. MY OWN ERRORS

1. **My worktree was provisioned at a stale commit.** It was at `1afa558e`
   (2026-09-09, Wave-10 era), not `wave29/base` — 19 waves behind, while every
   sibling lane sat at `c11511a1`. Had I worked there, "grep for the port before
   writing it" would have read **absent** for everything landed in Waves 11–28 —
   a deflating failure that manufactures duplicate work. Caught by checking
   `git worktree list` before starting. **This is worth a standing check:
   verify your worktree's HEAD against the wave base as step zero.**
2. **I nearly inlined an exported function** as "an unexported internal" (§2).
   Caught by checking the export directory instead of trusting `AEBA`.
3. **My registration check failed its own control on the first run, and it was my
   fault.** I wrote the `Scene::Update` known-positive symbol *from memory* as
   `?Update@Scene@Audio@@QEAAXAEBVTime@System@@@Z`; the real spelling is
   `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z`.
   The screen refused to report and exited 2. **A guessed known-positive is not a
   known positive** — it can only fail closed by luck. Ledger-worthy: hand a
   control a symbol you have *read*, not one you have *reconstructed*.
4. **My first negative control would have crashed rather than reported.** I wrote
   a zero-extension control that, on a negative index, computes a ~64 GB wild
   read. It would have faulted, not diverged — and a control that faults teaches
   nothing. Replaced with a bounded one, and the limitation stated (§3.3) rather
   than hidden.
5. **I asserted a write-width hazard that cannot exist, and built a control that
   could not fail.** I wrote in the source banner that `OnSoundEnded`'s BYTE flag
   write was the `Camera::operator=` trap "live here". It is not: `OR` with a
   small constant is width-invariant. My control for it changed nothing and
   reported NOT CAUGHT, which I could easily have written up as a blind screen.
   **Applying a kickoff warning as a blanket rule, rather than checking whether
   its mechanism is present, manufactured a hazard.** Both the comment and the
   control are corrected (§3.4).
6. **I let a 900s lock acquire run unattended.** The harness backgrounded it at
   600s; had it been granted I would have held the shared game directory without
   knowing and blocked the wave. I killed it and verified with `assert-held` that
   I did not hold it. Short acquires from then on.

---

## 10. INSTRUMENT DEFECTS FOUND, WITH DIRECTION

| # | defect | direction | owner |
|---|---|---|---|
| 1 | `RtGraphicsWave28GfxbodySummary()` has **zero callers** — ledger defect #7, reproduced concretely. The counter for a live route into my two Video bodies never prints. | **DEFLATING** (reads as "never called") | VERIFY |
| 2 | `audio_scene_update.cpp`'s report is guarded by `if (g_updates)`, so "never ran" and "ran zero times" are the same silence. | **DEFLATING** | VERIFY |
| 3 | IOLANE's return-slot prescription has `this` and the return slot **swapped** (§8.2). | **DEFLATING** for the caller — produces a wild write | IOLANE |
| 4 | The census cannot see a body no one resolves, so an unported body's callees are invisible until it is ported (§2). **Not a bug** — the population definition working as designed — but it means a body lane should expect off-list work. | DEFLATING by construction | INSTR (for the record) |
| 5 | **Mine.** My own 5b control was a NO-OP (`OR` width is unobservable) and reported NOT CAUGHT, which reads as a blind screen (§3.4). | reads as DEFLATING; was neither | fixed in this lane |

**The Bash tool auto-backgrounds any foreground command at 600s**, which silently
breaks kickoff §0.3's "never background a blocking lock acquire." My 900s acquire
was backgrounded at 600s with no way to prevent it. No lane can; the cap is the
tool's. The mitigation that works is **short acquires (≤300s) in a retry loop you
control**, plus `assert-held` immediately before every launch. Recorded for §0.3
of the next kickoff.

Direction matters: all four are deflating, and **a deflating blind spot reports
that work does not exist and nothing detects that.**

---

## 10a. THE GAME-DIRECTORY LOCK WAIT

Required by §0.3. WALL, MINT and VERIFY had priority and I queued behind them.

| attempt | timeout | outcome |
|---|---|---|
| 1 | 900s | **killed by me at ~690s.** Auto-backgrounded by the harness at 600s; MINT held it throughout. Killed rather than left running, then `assert-held` confirmed I did **not** hold it. |
| 2 | 240s | DENIED — timed out, VERIFY held it |
| 3 | 280s | **GRANTED** at ~150s |

**Total wait ≈ 19 minutes across three attempts.** Held for ~7 minutes: deploy,
run, rebuild with the corrected control, re-run, release. Released and verified
released with `assert-held` (exit 1).

I verified ownership with an independent `assert-held` rather than reading the
acquire's exit status, because I had piped that invocation through `grep` and **a
pipeline exits with the last command's status, so a TIMEOUT would have read as a
GRANT.** The lock script itself refuses to be piped for exactly this reason; the
independent assert is the safe way to ask.

---

## 11. WHAT I RE-DERIVED, AND WHAT REPRODUCED

| claim | source | verdict |
|---|---|---|
| Audio = 3 unported bodies; Video = 2 | kickoff §1.2 | **REPRODUCED** exactly, `runtime_census.py --unported --namespace {Audio,Video}` |
| Video's other 18 rows are not bodies | kickoff | **REPRODUCED**: 12 NO-OP vtables + 6 HARMFUL data slots |
| Audio's 4th gap row `??_7Stream@Audio@@6B@` is not mine | kickoff | **REPRODUCED**, left to SLOTS |
| Both Video rows return `String` by value | kickoff / brief | **REPRODUCED**, three ways (mangling, RDX use, IOLANE's screen) |
| all five targets unported | coordinator's request | **CONFIRMED** — each registered only by my own new files; controls incl. three split-across-lines positives all passed |
| `.pdata` extents | — | **NOT USED**. Every extent derived from the byte stream; all agreed with Ghidra |
| `?StopEffect@Scene@Audio@@` is a stale-verdict row | kickoff §1 | **NOT MINE** (lane INSTR). Noted: it became stale because Wave 28's `Scene::Update` port gave it a caller, moving it out of "ports with no caller" in the same run |

