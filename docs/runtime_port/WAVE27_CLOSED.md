# WAVE 27 CLOSED — 2026-09-12

Seven lanes, all merged. Base `wave27/base`. Every lane was a **decomposition-only
lane**: no lane was permitted to build, fix or extend a tool. Board re-derived on
the final merged tip, **after** the `cutover_manifest.inc` regeneration.

## 1. THE BOARD — AND IT WENT DOWN

```
              called   ported        gap
  opened        1372     1237  90.2%  135
  closed        1430     1277  89.3%  153
```

**THE PERCENTAGE FELL AND THE GAP GREW BY 18. Both are true, and the wave still
did more body work than any wave before it.** Read §1.1 before quoting any figure
here; the percentage is the single most misleading number available.

### 1.1 WHY THE BOARD MOVED BACKWARDS

`called` grew by **58** while `ported` grew by **40**. A faithful port of a body
that reaches DLL internals by address **ENLARGES the cutover surface** — the
callees it pulls in are dependencies the tree did not previously have, and they
are counted honestly the moment they exist. Five separate lanes reported this
independently, each in its own numbers:

| lane | its own board move | why |
|---|---|---|
| FRONTIER | 90.2 -> 90.1 | +6 called, +4 ported; one door deliberately left unported |
| GFXPART | 90.2 -> 89.9 | `ported` **cannot** move — every row is promotion-backlog |
| GFXBIG | Graphics BODY gap **4 -> 11** | 7 newly *visible* named exports, previously unknown |
| IOTEXT | 90.2, `DTOR 2 -> 0` | net flat, column emptied |
| LOOPS | **90.2 -> 90.7** | the one lane that went UP — its rows were already-`called` slots |

**LOOPS is the only lane whose percentage rose, and it is not the lane that did
the most work.** The sign tracks *what kind of row* a lane touched, not how much
it achieved. That is the argument for scoring identities, and this wave is its
cleanest demonstration.

### 1.2 THE NUMBERS THAT SHOULD BE QUOTED INSTEAD

```
  PROMOTION BACKLOG   156 bodies / 44 tables / 23 COMPLETE
                  ->  175 bodies / 41 tables / 34 COMPLETE
  REAL remaining vtable work   22 -> 18
  gap 153 = 26 BODY + 1 DTOR + 1 OPNEWDEL + 38 VTABLE + 49 DATA + 38 UNKNOWN
            of the 38 VTABLE rows, 20 are UNPROMOTABLE
```

**ELEVEN more vtables are COMPLETE than when the wave opened (23 -> 34).** That is
the wave's headline and it is a number the percentage actively hides.

### 1.3 IDENTITIES — what is now finished that was not

- **`Engine` 77.8% -> 94.1%, AT CEILING.** Its entire remaining gap is 3 DATA
  rows. **No bodies left in the namespace.**
- **The `DTOR` gap column went 2 -> 0**, then to 1 from a newly-visible row.
- **Nine tables reached COMPLETE:** `EdgeMesh` 0/9 -> 9/9, `Light` 5/6 -> 6/6,
  `Particle` 5/6 -> 6/6, `ParticleInstance` 5/9 -> 9/9, `Achievement` 4/9 -> 9/9,
  `AchievementInstance` 1/4 -> 4/4, `Goal` 4/7 -> 7/7, `GoalInstance` 3/7 -> 7/7,
  `ElementDescription` 3/6 -> 6/6, `PackageFile` 6/9 -> 9/9.
- **`StringTable` 7/9 -> 9/9**, and the cutover boot now clears it entirely.
- **`Achievement` and `Engine` BODY gaps are both 0.**
- Roughly **70 bodies** ported across the seven lanes.

## 2. CUTOVER ATTEMPT 30 — THE WALL MOVED

| attempt | wall | cleared by |
|---|---|---|
| 29 | `Collection::Array<ExternalArrayElement>::SetSize` (`0x18002c6a0`), EXEC-of-0 | **FRONTIER** |
| 30 | **`?CreateNewObject@SpriteSheet@UI@@`** — the mint's own per-slot FATAL | open |

The attempt-29 wall appears **nowhere** in the attempt-30 logs. `RtMintShadow`
printed `DLL=ABSENT (post-cutover; oracle gone)`, so this was a genuine cutover
run. Boot now reaches **t=583.58ms**, past input bindings and all three string
tables, where attempt 26 died at ~t=569ms on `StringTable::CreateNewObject`.

**FRONTIER predicted this exactly**: *"It will clear `ArraySetSize+0x4d` — and it
will very likely report a new name."*

**The new wall is the SAME SHAPE as attempt 26's, one class along.** That is Wave
26 lane FACTORY's finding arriving on schedule: the **206-slot factory family**.
`StringTable` and `SpriteSheet` are two members of one family, and clearing them
one cutover attempt at a time is the expensive way. FACTORY's §6 item 2 prescribed
the cheap way — make `rt_mint_gen.py` mint the factory so the vtable is harvested
for free — and **nothing in Wave 27 acted on it.** It is the strongest Wave 28
input.

**RECORDED LIMITATION, and it is structural:** the `BANISHED_RT_STRICT` residue is
**NOT available** for attempt 30. The mint's fail-fast kills the process before the
exit summary that prints it. So attempt 30 named **one symbol and nothing else**.
*A cutover boot is a GATE, not a PLANNER* — for the fourth consecutive wave.

## 3. WHAT THIS WAVE ACTUALLY WAS

**A porting wave that stayed a porting wave** — the first in three. The no-tooling
rule held in all seven lanes. Every lane hit instrument defects; every lane
recorded them and kept porting. **Twelve defects** are in
`WAVE27_INSTRUMENT_LEDGER.md`, none fixed, each a Wave 28 input.

**Five of the twelve were live GhidraMCP defects, and all five are one shape:**
*the tool answers about a subject it has not verified is the one you named* —
wrong program (#1), wrong section bounds (#2), wrong function (#4, #10), wrong
bytes (#11). The operating rule the wave converged on, from two lanes
independently:

> **Ghidra for names, comments, types and navigation. `objdump` and `pefile`
> against the retail file on disk for any BYTE, ADDRESS, EXTENT or BODY a port
> depends on.** Two sources sharing no mechanism are two sources; two Ghidra calls
> are one.

## 4. THE FINDINGS THAT OUTLIVE THE WAVE

### 4.1 A returned Matrix carried a COLOUR column

`MeshInstance::GetInstanceTransform` performs 16 `MOVSS` stores; only 12 come from
`AsMatrix`. The **w column** is `_color[0..3]`. The obvious port passes **every
geometric check** and renders everything untinted. The sibling
`EdgeMeshInstance` override does not do this, so port-by-analogy produces the wrong
body and "I checked the sibling" reads as diligence.

**Count the STORES, not the fields.** A mismatch between the writes a body
performs and the writes its named helper accounts for is the signal — visible in
disassembly, invisible in decompiled C.

### 4.2 A registration that does not move the census is a PHANTOM NAME

GFXMESH registered a body as `UEBA` by analogy with the virtual slots beside it;
it is `QEBA`. `ResolveExport` returns NULL silently and the `ResolveAddr` fallback
hides it. **The detector is free and universal**, requires no tool, and caught what
reading the mangled name did not. Its stronger half: the lane only noticed because
it was *attributing its census delta row by row*. **An unexplained board movement
is a finding, not noise.** "The board moved by about the right amount" is no check.

### 4.3 OWNERSHIP, not element type, decides teardown

One Wave 26 warning was handed to three lanes and **each found a different truth**:

| lane | what was true |
|---|---|
| FRONTIER | Not a separate template — same engine, 7 rows since Wave 9. Add ONE `ArraySpec` row. |
| ACHIEVE | The **POD teardown is CORRECT.** `GoalList::Initialize` **MOVES** each `Goal*` out, so the array owns nothing by teardown. The refcounted recipe would have **OVER-RELEASED every Goal.** |

`Array<ExternalPtr<Goal>>` looks unmistakably refcounted and is not. **The
directions are not symmetric:** under-release leaks, over-release frees a live
object — so the *safe-looking reflex* is the more dangerous error.
**Relay the METHOD, never the VERDICT.**

### 4.4 The shipped RNG masks are not the textbook ones

`ParticleInstance::SetParticle` inlines a Mersenne Twister with masks
`0xff3a58ad` / `0xffffdf8c`. A port written from the published algorithm compiles,
produces plausible randomness, and diverges forever — **RNG output is supposed to
look arbitrary, so nothing catches it.** Recognising the algorithm is exactly what
licenses you to stop reading the bytes. Lift every magic constant.

### 4.5 REPAIR THE LICENCE, NOT THE SENTENCE

Needed two lanes to see. ACHIEVE and LOOPS drew **the same unlicensed inference**
from the same evidence. **LOOPS' conclusion was false; ACHIEVE's was true.**

> A true conclusion reached by an unlicensed inference is not a success, and
> correcting only the false one leaves the mechanism intact. **The repair belongs
> to the reasoning step, not to the sentence that happened to be wrong.**

This is why this project keeps rediscovering the same families: the false instance
is corrected, the true instance is left standing as evidence the method works, and
**the method is what produced both.**

### 4.6 Two lanes hit ONE bare `else` with DIFFERENT shapes

`rt_deleting_dtor_verify.py`'s scalar-branch check knows two shapes; its third arm
is a bare `else`. GFXMESH hit it (inlined `~Class` with no restamp) and GFXPART hit
it (devirtualised to `~Geometry`). **Both called their finding "a third shape". They
are a third and a fourth**, and each lane, reasoning correctly from what it could
see, **under-reported the defect by one.**

**So the Wave 28 fix is NOT to add a third arm** — a closed enumeration is the
wrong model. Verify the property, or print UNRECOGNISED instead of `BAD`.

**Neither lane could see this. It is the finding the integrator's ledger existed to
catch**, and the argument for keeping one *during* a wave rather than assembling it
at merge.

### 4.7 A DEFLATING screen hid a real door for 21 waves

`addr_door_audit.py` is blind to an address passed to a *wrapper*. GFXMESH noticed
only because respelling its own door changed the answer — exposing that
**`mesh.cpp:201` has held an unregistered door at `0x1800136f0` since Wave 6.**

**Direction decides urgency.** An INFLATING screen is self-limiting: someone chases
a false positive and finds the truth. A DEFLATING one reports that work does not
exist, and **nothing detects that** — here, for 21 waves, until an accident.

## 5. WHAT IS EXPLICITLY NOT CLOSED

- **~70 bodies were ported and a MINORITY were verified by execution.** Eleven of
  LOOPS' twelve have no oracle; most of GFXMESH's 14, GFXPART's 8 and ACHIEVE's 16
  likewise. LOOPS states it plainly: *"ported + registered should not be read as
  verified."* **This is the single most misleading thing available to omit.**
- **Exactly ONE promotion was claimed in the whole wave** —
  `??_7UpdateLoop@Engine@@6B@`, by LOOPS, settling the wave's one explicitly open
  question. **FRONTIER, GFXPART and GFXMESH each reached COMPLETE tables and each
  DECLINED**, none having established ownership of the stamping function. That is
  correct, and **the pile of 34 COMPLETE tables is not a pile of cheap promotions.**
- **ACHIEVE's oracles are wired but UNEXERCISED** — LOOPS held the game-dir lock
  for its whole 600s window. Two probes need watching on a future run; in
  `GoalList::Initialize`, `firstWrongGoal >= 0` means the ownership move is
  transposed.
- **`?_singleton@AchievementManager@Achievement@@` — HALF retired, tag STANDS.**
  ACHIEVE ported and registered the ctor; the other writer is a store inside
  `??0DialogLoopT@Engine@@`, LOOPS' table. Two-owners-one-object is still live.
- **`?_loader@LoaderLoop@Engine@@` was NOT retired — and LOOPS satisfied the
  written expiry condition.** The condition **named the wrong fact.** Fourth
  well-formed-and-wrong expiry clause this project has found.
- **Audio: nothing ported.** Both items re-checked as *out of budget, NOT newly
  blocked*. `?Update@Scene@Audio@@`'s stated blocker was already re-checked as
  "NOT A WALL" by Wave 24. Clean scope for Wave 28.
- **`?SetupFileSystem@Modification@IO@@` — writable but NOT VERIFIABLE:** the only
  unattended mod route is the broken `ModDialog_ToggleMod_Impl+0x43` one.
- **`?Update@ParticleSystemInstance@`** (`0x1800a5600`, 0x1479 bytes, 12 inlined
  RNG draws) — the whole particle sim. Deserves its own lane.
- **`AchievementInstance::Persist`'s prefix oracle uses `stopAfter=1`, a FLOOR the
  lane CHOSE**, not the index `save_trace.h` asks for. It declined to guess.
- Every one of the twelve ledger entries.

## 6. A MERGE HAZARD THIS WAVE PAID FOR IN FULL

`deleting_dtor.cpp`'s table is **POSITIONAL**: `DeletingDtorCore(self, flags, N)`
indexes `kSpecs[N]`. **Three lanes each appended starting at index 44 on their own
branch.** A keep-both merge silently gives two of them the first lane's rows —
**dispatching the wrong destructor, with no diagnostic.**

The `static_assert` beside the table checks the **COUNT ONLY** and cannot see an
index collision. Resolved at integration by renumbering `Light` 44->47, `SunLight`
45->48, `ElementDescription` 44->49 (count 44 -> 50), each verified for duplicates
and out-of-range. **The hazard is now recorded in
`deleting_dtor_internal.h` beside the constant**, because the next appender will
read that file and not this one.

## 7. MY OWN ERRORS, RECORDED

1. **I propagated a STALE fact into a lane brief.** `WAVE26_CLOSED.md` §2 item 6
   says `file_stream_probe.cpp` has two structurally-false vptr checks; PROMOIO had
   **already fixed them inside Wave 26**. I copied it into IOTEXT's brief with an
   instruction not to repair the probe. What limited the damage was a **hedge in my
   phrasing**, not a control. The control was one `git log` on the file.
2. **I relayed the `Array<T>` warning twice and its application was wrong both
   times** (§4.3).
3. **I ran cutover attempt 30 with no arm at all** — no `BANISHED_RT_NO_DLL`, and
   `AUTOQUIT` instead of `AUTOSTART`. It failed at `LoadLibrary`, never reached the
   cutover, and burned 120s against a blocked *Init Failed* dialog. The script's own
   header says the caller passes the arm, and CLAUDE.md says to poll with
   `boot_state.sh` rather than wait out a timeout. Both were written down; neither
   was consulted.

**All three are the same failure as §4.5 and as Wave 26's close: knowing a rule and
writing it down does not execute it.**

## 8. GATES, ON THE FINAL MERGED TIP

```
mingw32-make standalone-debug            exit 0
cutover_manifest.py --gate               OK (60 rows), regenerated ON THE MERGED TIP
rt_mint_gen.py --check                   UP TO DATE (64 vtables, 104 BaseTypeInfo, 39 storage)
runtime_census.py --selftest-all         ALL GREEN (16/16 on the last subject)
cutover attempt 30                       attempt-29 wall CLEARED, new wall named
deleting-dtor index audit                50 rows, 50 helpers, 0 duplicates, 0 out of range
```

## 9. WAVE 28 INPUTS

1. **`?CreateNewObject@SpriteSheet@UI@@` and the 206-slot FACTORY FAMILY.**
   Clearing them one cutover attempt at a time is the expensive way. FACTORY's
   prescription — make `rt_mint_gen.py` emit the factory so the vtable is harvested
   free — turns a second workstream into a property of how the first is written, and
   **Wave 27 did not act on it.** Strongest input.
2. **Ledger #8 first (the only DEFLATING one), then #11** (wrong bytes,
   self-corroborating). Then the rest of the twelve.
3. **A VERIFICATION lane.** ~70 bodies are registered and unexecuted. That is the
   wave's real debt, and no porting lane will pay it.
4. **A PROMOTION lane with its own measurement** for the 34 COMPLETE tables. Not an
   integration step.
5. `?Update@ParticleSystemInstance@` as a lane of its own; Audio's two bodies;
   `SunLight`'s five; `TextFileStream` slots 2 and 8.
6. **Make the residue survive a fail-fast**, so a cutover attempt can name more than
   one symbol. Four consecutive waves have been gated one symbol at a time.
