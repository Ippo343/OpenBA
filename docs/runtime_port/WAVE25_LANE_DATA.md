# WAVE 25 LANE DATA — the 46 DATA gap rows, split into move-now and cutover-only

Branch `wave25/data`, cut from `wave25/base` at `fc56353c`. Private worktree
(`.claude/worktrees/agent-a05eb1540d52487bc`). **No game-directory lock** — per
kickoff §1d this lane gets no run, and every measurement below is static on this
tip or a committed run whose tip is named.

---

## 0. HEADLINE

**Rows promoted: 0. Rows adjudicated and tagged: 14. Rows handed to MANIFEST: 39.
Rows left READY-and-blocked-on-one-run: 7.**

Zero promotions is the answer the evidence gives, and two of the three things that
produced it were **corrections to my own analysis found by controls**, not by
review:

| # | what I was about to conclude | what stopped it | §
|---|---|---|---|
| 1 | "the seven `?_typeInfo@` rows have ZERO in-DLL writers" | an **independent hand-rolled opcode scan** run while the capstone tool was agreeing with me. The tool scans only the row's **base byte**; each node has **two** in-DLL writes, at `+0x10` and `+0x30`. | §2.3 |
| 2 | "all three `Error`/`Debug` statics have only PORTED consumers — promote them" | a **rel32 caller scan**. `?SetText@Error@` is `ported` in the census and has **18 in-DLL rel32 call sites** that never touch a door. | §3.1 |
| 3 | "`?_systemTicksPerSecond@` is a read-modify-write hot spot" | reading the **detail** rather than the totals: `idiv m64` reads memory and writes registers. Twelve readers, one writer. | §3.3 |

---

## 1. DENOMINATOR, AND THE ONE THE BRIEF GOT WRONG

```
python tools/runtime_census.py --unported --csv c.csv
```
At kickoff on this tip: `TOTAL 4632 1323 called 1171 ported 9 detour 88.5%`,
**152 gap rows, 46 of them DATA** — System 11, Graphics 7, Object 6, Video 6,
Achievement 4, IO 4, Engine 3, Audio 2, Input 2, Application 1. That reproduces
`WAVE25_KICKOFF.md` §0 exactly.

**The brief's "41 tagged / 107 portable" is wrong and I did not use it.** I parsed
the `RT_UNPROMOTABLE:` lines **out of `standalone/reg_mint_promoted.cpp` directly**
with my own predicate (`'HARMFUL' in <rest-of-line>`), which does not require the
class field to be one token, and got **25 of my 46 already tagged**. All five of
the rows the coordinator later warned about —
`?_singleton@Application@1@`, `?_singleton@{Configuration,Device}@Audio@`,
`?_singleton@{Configuration,Device}@Input@` — were **in my tagged list from the
start and were never candidates here.**

**Figures re-derived after the coordinator's message** (stated per its request):
the tagged/untagged split (unchanged, 25/21), the per-row verdicts in §3
(unchanged), and the board totals in §5 (changed — see below). Nothing in §2 or
§3 rests on the census's tag output.

**A cross-check neither instrument printed before, and it is exactly five:**

```
python tools/unpromotable_slot_check.py   ->  CHECK 3: 60 tags with an expiry
python tools/runtime_census.py --unported ->  55 of the 152 above cannot be promoted
```

`60 − 55 = 5`, the five malformed tags. **Two instruments over the same file, on
the same tip, disagreeing by exactly the defect's size.** Print both numbers side
by side in any future wave doc; that is the detector this defect never had.

---

## 2. THE INSTRUMENT, AND THE BLIND SPOT ITS CONTROL FOUND

### 2.1 `tools/rt_data_row_refs.py` (new)

`tools/video_data_refs.py`'s decoder aimed at a **caller-supplied** population
instead of a hard-wired one, with the candidate scan hoisted into a single index
(46 subjects, not 18).

**SUBJECT EXAMINED**, stated because a verdict without a named subject is how six
Wave 23 instruments went wrong: `.text` of the **retail** `Runtime-x64.dll`,
`0x180001000..0x18014a600`, 1,349,120 bytes, **one section**. `.rdata` and `.data`
are not scanned, so a `0` here means *no code reference*, never *no reference*.
The profile DLL is not consulted.

Both Wave 23 decoder traps are structurally absent: it never parses disassembly
**text** (so AT&T-vs-Intel cannot arise — classification is from capstone's decoded
operand list), and it is prefix-agnostic (so the non-REX `MOV EAX,[rip]; RET` form
classifies like any other). **The unclassified bucket is printed on every run.**

### 2.2 Controls, each with an effectiveness counter

| control | result | comparisons it actually made |
|---|---|---|
| 1 positive — 5 known `?_singleton@SaveLoad@Save@@` refs, **2 of them WRITEs** | **GREEN** 5/5 addresses, **5/5 kinds** | 5 kind comparisons, **2 on the WRITE arm** |
| 2 negative — same scan restricted to trailing-imm=0 | **GREEN**, lost 1 of 5 | 5 addresses |
| 3 negative — **WRITE arm disabled**, i.e. the exact shape trap 1 produces | **GREEN**, 2/2 known WRITEs fell through to READ | 2 WRITE sites |
| 4 blind-spot — an arbitrary `.rdata` address | **GREEN**, 0 refs | — |

Control 1 in `video_data_refs.py` checked only that the five **addresses** were
found. **That control cannot detect trap 1**, because trap 1 changes the *kind*
and not the address. Control 3 is new for the same reason: it corrupts a subject
the probe compares, and the corruption is the one the trap produces.

**The tool also agrees with four independently-recorded ground truths it was never
shown**: `?_firstTypeInfo@` 106 R / 105 W, `?_singleton@ExternalFactory@` 149 R /
2 W, `?_singleton@Factory@` 18 R / 2 W, `?_singleton@InternalFactory@` 5 R / 1 W —
the exact figures in the Wave 23 OBJ tags. **9 of 9 checkable tag figures
reproduce**; the one soft edge is `?_singleton@Configuration@Input@`, whose tag
says 16 reads and this tool says 17.

### 2.3 THE HAND-CHECK DISSENTED — a whole-node blind spot

Instrument rule 1: hand-check a row **while the tool is agreeing with you**. It was
agreeing (`W=0` on all seven `?_typeInfo@` rows, which is what I wanted), so I
wrote a deliberately independent scanner — a hand-rolled opcode walk for
RIP-relative **store** forms (`(modrm & 0xC7) == 0x05`), no capstone — and handed
it a known WRITE shape first (the two SaveLoad VAs; **GREEN, both found**).

It found **28 store sites into the seven nodes.** The capstone tool scans the row's
**base address only**; these are at **`+0x10` (`parentName`) and `+0x30` (`next`)**,
in the CRT static-initialiser region `0x180147xxx`. Re-run at `--width 0x38`, the
tool reports exactly **2 writes per row**, matching.

**Width 1 is COMPLETE for a scalar row and INCOMPLETE for every aggregate**, and
the failure direction is FALSE-SAFE. The tool now takes `--width` and **prints the
blind spot with the measured specimen** when width is 1, so it is not discoverable
only by reading this document. Two rows in §3 are aggregates and would have been
mis-measured without it: `?_errorString@Error@` (a `char[0x2000]`) and
`?_loader@LoaderLoop@Engine@` (a `Loader` **object**).

The conclusion survives the correction: two writes, both CRT static-init, one-shot
at DLL load, into the DLL's own node. There is no mutable state and no chain to
split.

### 2.4 `tools/rt_indll_callers.py` (new) — the half a reader census cannot supply

> **THE CENSUS `ported` COLUMN DOES NOT MEAN "EVERY READER GOES THROUGH OUR BODY."**
> It means a door is registered. Retail's copy still runs for every in-DLL `rel32`
> call site and still touches retail's statics.

This is the finding I most want to outlive the wave, because it is a **false-safe**
error in a lane whose one way to do damage is a false-safe promotion. A
reader/writer census over `.text` is **necessary and not sufficient**; the rel32
caller count is the other half. Positive control: a target with known in-DLL
callers must return > 0 before any `0` is believed.

---

## 3. THE 46 ROWS

`R`/`W`/`LEA` are in-DLL references in `.text` at `--width 1` unless the row is
marked ⊞ (aggregate, measured at its full width). **`ported?` is the census column
and §2.4 says what it does and does not mean.**

### 3.0 The whole table

| # | row | R | W | LEA | move now? | verdict |
|---|---|---|---|---|---|---|
| 1 | `?_typeInfo@Achievement@1@` ⊞ | 9 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 2 | `?_typeInfo@AchievementInstance@Achievement@` ⊞ | 4 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 3 | `?_typeInfo@GoalInstance@Achievement@` ⊞ | 7 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 4 | `?_typeInfo@AnimationEvent@Graphics@` ⊞ | 6 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 5 | `?_typeInfo@MaterialInstance@Graphics@` ⊞ | 8 | 2 | 1 | **READY, blocked on a run** | not tagged; §4, §4.2 |
| 6 | `?_typeInfo@Particle@Graphics@` ⊞ | 2 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 7 | `?_typeInfo@PackageFile@IO@` ⊞ | 2 | 2 | 1 | **READY, blocked on a run** | not tagged; §4 |
| 8 | `?_errorString@Error@System@` ⊞ | 2 | 0† | 3 | cutover-only | **TAGGED this lane** §3.1 |
| 9 | `?_stopApplication@Error@System@` | 1 | 2 | 0 | cutover-only | **TAGGED this lane** §3.1 |
| 10 | `?_hideDisplay@Error@System@` | 1 | 2 | 0 | cutover-only | **TAGGED this lane** §3.1 |
| 11 | `?_disableHandler@Debug@System@` | 1 | 5 | 0 | cutover-only | **TAGGED this lane** §3.2 |
| 12 | `?_secondsPerTick@Timer@System@` | 96 | 2 | 0 | cutover-only | **TAGGED this lane** §3.3 |
| 13 | `?_ticksPerSecond@Timer@System@` | 12 | 2 | 0 | cutover-only | **TAGGED this lane** §3.3 |
| 14 | `?_systemInitialTick@Timer@System@` | 12 | 0† | 1 | cutover-only | **TAGGED this lane** §3.3 |
| 15 | `?_systemTicksPerSecond@Timer@System@` | 12 | 1 | 0 | cutover-only | **TAGGED this lane** §3.3 |
| 16 | `?_systemSecondsPerTick@Timer@System@` | 0 | 1 | 0 | cutover-only | **TAGGED this lane** §3.3 |
| 17 | `?_singleton@MainLoop@Engine@` | 19 | 1 | 0 | cutover-only | **TAGGED this lane** §3.4 |
| 18 | `?_singleton@Engine@1@` | 4 | 2 | 0 | cutover-only | **TAGGED this lane** §3.4 |
| 19 | `?_loader@LoaderLoop@Engine@` ⊞ | 0† | 2 | 5 | cutover-only | **TAGGED this lane** §3.4 |
| 20 | `?_singleton@Configuration@System@` | 4 | 2 | 0 | cutover-only | **TAGGED this lane** §3.4 |
| 21 | `?_singleton@AchievementManager@Achievement@` | 1 | 5 | 0 | cutover-only | **TAGGED this lane** §3.5 |
| 22 | `?_singleton@MemoryAllocator@System@` | 2185 | 3 | 0 | cutover-only | tagged Wave 22 ALLOC |
| 23 | `?_firstTypeInfo@BaseTypeInfo@Object@` | 106 | 105 | 0 | cutover-only | tagged Wave 23 OBJ |
| 24 | `?_singleton@ExternalFactory@Object@` | 149 | 2 | 0 | cutover-only | tagged Wave 23 OBJ |
| 25 | `?_singleton@Factory@Object@` | 18 | 2 | 0 | cutover-only | tagged Wave 23 OBJ |
| 26 | `?_singleton@InternalFactory@Object@` | 5 | 1 | 0 | cutover-only | tagged Wave 23 OBJ (harm is the WRITER) |
| 27 | `?_typeInfo@CompiledExternal@Object@` | 2 | 0 | 1 | cutover-only | tagged Wave 23 OBJ, NO-OP |
| 28 | `?_typeInfo@Internal@Object@` | 5 | 0 | 1 | cutover-only | tagged Wave 23 OBJ, NO-OP |
| 29 | `?_singleton@Application@1@` | 24 | 2 | 0 | cutover-only | tagged; **census-invisible**, §1 |
| 30 | `?_singleton@Configuration@Audio@` | 2 | 7 | 0 | cutover-only | tagged; **census-invisible**, §1 |
| 31 | `?_singleton@Device@Audio@` | 7 | 1 | 0 | cutover-only | tagged; **census-invisible**, §1 |
| 32 | `?_singleton@Configuration@Input@` | 17 | 7 | 0 | cutover-only | tagged; **census-invisible**, §1 |
| 33 | `?_singleton@Device@Input@` | 16 | 1 | 0 | cutover-only | tagged; **census-invisible**, §1 |
| 34 | `?_singleton@FileSystem@IO@` | 89 | 2 | 0 | cutover-only | tagged Wave 17 CEIL |
| 35 | `?_singleton@Modification@IO@` | 6 | 5 | 0 | cutover-only | tagged Wave 17 CEIL |
| 36 | `?_singleton@Persistence@IO@` | 23 | 1 | 0 | cutover-only | tagged Wave 17 CEIL |
| 37 | `?_singleton@Configuration@Video@` | 56 | 7 | 0 | cutover-only | tagged Wave 17 CEIL |
| 38 | `?_singleton@Device@Video@` | 163 | 2 | 0 | cutover-only | tagged Wave 17 CEIL |
| 39 | `?_firstSurface@ImageBuffer@Video@` | 8 | 3 | 0 | cutover-only | tagged Wave 17 CEIL |
| 40 | `?_firstBuffer@IndexBuffer@Video@` | 8 | 3 | 0 | cutover-only | tagged Wave 17 CEIL |
| 41 | `?_firstBuffer@VertexBuffer@Video@` | 8 | 3 | 0 | cutover-only | tagged Wave 17 CEIL |
| 42 | `?_firstRenderBuffer@RenderBuffer@Video@` | 8 | 3 | 0 | cutover-only | tagged Wave 17 CEIL |
| 43 | `?_frameIndex@ParticleInstance@Graphics@` | 2 | 0 | 0 | cutover-only | tagged Wave 17 CEIL |
| 44 | `?_rng@Particle@Graphics@` | 20 | 12 | 14 | cutover-only | tagged Wave 17 CEIL |
| 45 | `?_rootOnlyUpdates@MeshInstance@Graphics@` | 2 | 1 | 0 | cutover-only | tagged Wave 17 CEIL |
| 46 | `?_singleton@AnimationManager@Graphics@` | 17 | 2 | 0 | cutover-only | tagged Wave 17/23 GFXDATA |

† **`W=0` is not "never written."** Rows 8, 14 and 19 are written through a
register after a `LEA`, a form **no RIP-relative scan can see**. This is the
`reference_never_written_needs_by_whom` shape, and the tags say so in line.

### 3.1 The three `Error` statics — the near-miss

`?_errorString@`, `?_stopApplication@`, `?_hideDisplay@` have, between them,
**twelve in-DLL references and every single one sits in `?SetText@Error@` or
`?Display@Error@`, both of which the census calls PORTED.** On the reader rule
alone, all three read "promote". Wave 22 lane ERRCFG ported `?Display@` **for
exactly this purpose** — "while it is unported those three rows cannot be reasoned
about at all" — so the tree itself invited the conclusion.

```
python tools/rt_indll_callers.py CONTROL=0x1800c9020 SetText=0x1800c5fa0 Display=0x1800c6060
  CONTROL   0x1800c9020 : 1 in-DLL rel32 site   <- positive control, GREEN
  SetText   0x1800c5fa0 : 18 in-DLL rel32 sites
  Display   0x1800c6060 : 2  in-DLL rel32 sites
```

**Twenty in-DLL bodies still call retail's SetText/Display by `rel32`** — from
Application, FileSystem, `Timer::PlatformStartup`, `Engine::Startup`, Input — and
each writes and reads **retail's** copy of all three statics. Promoting splits the
error-reporting state, and the path it splits is the out-of-memory path
(`FatalOutOfMemory → Display`), which `WAVE22_LANE_ALLOC.md` §5.2 already names as
a live unattended-boot hazard. **Tagged HARMFUL, DLL-present; REQUIRED at cutover.**

One dependency-shaped blocker on this row set **is** discharged, per kickoff §1a,
and it is recorded so nobody re-derives it: `?HideErrorDisplay@Error@` is the only
unported writer of `?_hideDisplay@`, and Wave 22 ERRCFG proved it **DEAD** — no
call site in any of the four shipped binaries, four screens each with a live
positive control, and `rt_indll_callers.py` independently reads **0** here. The row
still fails, on the rel32 sites. *A discharged blocker is not a green light; it
just stops being the reason.*

### 3.2 `?_disableHandler@Debug@System@`

One in-DLL reader, `?ExceptionHandler@Debug@System@` — **unported and live**, it is
the DLL's own top-level exception filter. Five writers, of which only
`?EnumerateVideoModes@Device@Video@` is ours. Promoting makes our
`EnumerateVideoModes` raise a suppression flag retail's filter never sees, so
retail swallows the probe exception it is meant to re-raise. **HARMFUL,
DLL-present; REQUIRED at cutover.**

### 3.3 The five `Timer` statics — three distinct harms, one namespace

* `?_secondsPerTick@` — **96 reads / 2 writes**, readers spread engine-wide
  (`ParticleSystemInstance::Update`, four UI element `Update` bodies,
  `Audio::Scene::Update`, `Device::Input::UpdateState`, `MainLoop::Frame`/`Draw`,
  SaveLoad's five `Update` bodies), a majority unported. The `?_firstTypeInfo@`
  shape at 96:2.
* `?_ticksPerSecond@`, `?_systemInitialTick@` — 12 reads each, the same six
  unported readers (`MainLoop::MarkFrameUpdateTime`, `Timer::CurrentTime`,
  `Timer::CurrentTimeTicks`, `MainLoop::Draw`, `MainLoop::Frame`,
  `Context::Finish`).
* `?_systemTicksPerSecond@` — **harmful in both directions at once.** Its one
  writer `?PlatformStartup@Timer@` is UNPORTED, so a promoted slot is never
  written and stays 0, and its twelve readers reach it as **`idiv qword ptr
  [rip]`** — a zero divisor is `#DE`, not a wrong number. The
  `?_singleton@InternalFactory@` shape (harm is the writer) stacked on the
  `?_firstTypeInfo@` shape (harm is the readers). **This is the concrete form of
  the "TIMER's IDIV-by-zero" that `WAVE22_LANE_ALLOC.md` §5.3 refers to.**
* `?_systemSecondsPerTick@` — **one reference in the whole of `.text`**, and it is
  a WRITE by that same unported `PlatformStartup`. Zero in-DLL readers, so there is
  no split-brain to fear and the harm is purely the writer: promoted, our
  `timer.cpp` reads `0.0`. **A one-reference row is the easiest kind to misread as
  trivially safe.**

All five: **HARMFUL, DLL-present; REQUIRED at cutover.** `?_systemTicksPerSecond@`
and `?_systemSecondsPerTick@` have an ordering constraint —
`?PlatformStartup@Timer@` must be ported and registered *first*.

### 3.4 The three `Engine` rows and `?_singleton@Configuration@System@`

* `?_singleton@MainLoop@Engine@` — **19 reads / 1 write**, and the one writer (the
  `MainLoop` ctor) **is ours**. This is the textbook Wave 23 GFXDATA statement: *a
  ported writer is not progress toward promotability while readers remain.*
  Unported readers include `?Draw@MainLoop@Engine@` and `?Finish@Context@Video@`.
* `?_singleton@Engine@1@` — 4 reads / 2 writes, both writers ours, at least one
  reader (`?Run@Loader@Engine@`) not. Same shape as
  `?_singleton@AnimationManager@Graphics@`, which Wave 23 GFXDATA left HARMFUL on
  identical grounds.
* `?_loader@LoaderLoop@Engine@` ⊞ — **an aggregate, not a pointer.** Consumers take
  its **address**: 5 `LEA` and 2 stores, and `?Startup@LoaderLoop@Engine@`
  (UNPORTED) does `lea rcx,[0x1801fe7b8]` then works through the register. A
  RIP-relative read/write census reports `R=0` here and **that 0 is an artefact of
  the access pattern, not evidence of disuse** — recorded in the tag.
* `?_singleton@Configuration@System@` — split on **both** halves: reader
  `?Get@Configuration@System@` unported **and** writer `??0Configuration@System@`
  unported.

### 3.5 `?_singleton@AchievementManager@` — the inverted shape

**1 read against 5 writes.** Two of the five writers are unported
(`??0AchievementManager@Achievement@`, and a store inside
`??0DialogLoopT@Engine@`'s address range — an over-attribution, see §6). Retail
constructs the manager into retail's slot; a promoted slot would hold whatever our
ported `Engine::Startup`/`Shutdown` last wrote. Two owners, one object. **A low
read count is not a safe row** — the reader rule is a sufficient condition for
harm, not a necessary one.

---

## 4. THE SEVEN `?_typeInfo@` ROWS — READY, and blocked on one thing I was not allocated

They are **deliberately not tagged.** They are not unpromotable; they are blocked
on **criterion 2 at the top of `reg_mint_promoted.cpp`: a real boot.**

**Safety, established three ways:**
1. **No mutable state.** At `--width 0x38`, exactly two in-DLL writes per node,
   both CRT static-init (`parentName` +0x10, `next` +0x30), one-shot at DLL load
   into the DLL's own node (§2.3). Every other reference is `MOV r32, dword ptr
   [rip]` of `+0x00 typeId`.
2. **Content equal.** `MintTypeInfos()` (`rt_data_mint.cpp:317`) seeds all six
   fields unconditionally inside `RtDataMintInit()`, before promotion registers;
   `RtMintShadowReport("exit")` measures **624 field comparisons, 0 MISMATCH, 0
   typeId not-yet-comparable** across all 104 rows. The "promoted but zeroed /
   inert" reading is false and is not revived here.
3. **Every consumer reads `+0x00` or `+0x08` only** — hand-read, all nine sites:
   `package_file.cpp:327`, `goal.cpp:169`, `achievement_manager.cpp:293`,
   `graphics_typeinfo_accessors.cpp:132/136/140`, `gameupdateloop_load.cpp:75`,
   `gameupdateloop_serialize.cpp:195`, `reg_unreg_lane3.cpp:164/195` (exe dispatch
   slots `0x140151350`/`0x140151420`, whose exe consumers do `MOV RCX,[slot]; MOV
   EDX,[RCX]`). Nothing walks the node.

**Why not promoted:** the committed attendance evidence
(`docs/runtime_port/wave19_phase0c/run2_attend/`) reads `never` for three of them —
`PackageFile init=0 post=0`, `MaterialInstance init=1 post=0`, `Particle init=1
post=0` — and says **nothing at all** about the other four, because three of the
four were not in the watch table. Promoting on readiness alone is precisely what
Wave 21 and Wave 23 GFXDATA each declined to do, and I have no lock.

**What I did instead, so the next armed run decides all seven:** added
`?_typeInfo@Achievement@1@`, `@AchievementInstance@` and `@GoalInstance@` to
`standalone/wave17_door_attend.cpp`'s `g_watch[]`. These three are the only ones of
the seven whose name-door consumer is **ordinary code rather than a vtable-slot
body**, so they are the three most likely to come back ATTENDED.

### 4.1 ONE THING THAT MUST BE ANNOTATED *WITH* THE PROMOTION, NOT AFTER IT

Promoting `AnimationEvent` / `MaterialInstance` / `Particle` / `PackageFile` makes
their `?TypeName@` bodies hand out **our** node's `name` **pointer**, and all four
bodies compare that pointer against the DLL's with `RT_DIFF_SCALAR`
(`graphics_typeinfo_accessors.cpp:171-176`, `package_file.cpp:555-561`). They will
read `divergences=N` **forever after**. This is not a prediction: the top of
`reg_mint_promoted.cpp` records `?TypeName@SpriteSheet@UI@@` already doing exactly
this, `calls=1 divergences=1`, going to 0 under `BANISHED_RT_MINT_PROMOTED=0`. It
is the string twin of the vtable-identity divergence and it is **not a
regression** — but an unannotated red line in a sweep is how a wave loses a
session. Annotate beside each body in the same commit as the promotion.

### 4.2 `?_typeInfo@MaterialInstance@` — lane VTGFX's residual, re-derived

VTGFX reports `??_7MaterialInstance@Graphics@@6B@` promoted (9/9, kill switch
`BANISHED_RT_PROMOTE_MATINST=0`). **On my branch it is not promoted** — that work
is in VTGFX's worktree and unmerged, so I could not take it as a live fact and did
not. What it changes post-merge is the **route**: `RtGraphics_MaterialInstance_Type`
will actually dispatch, which is what Wave 23 GFXDATA named as the missing door.
It does **not** change the attendance requirement, and it makes §4.1 **imminent
rather than hypothetical** for this row: the `TypeName` body it unblocks is one of
the four that compares pointers.

---

## 5. DECLARED SURFACE GROWTH, AND THE BOARD

**Surface growth: +0.** No new door, no new by-address call, no registration. The
only compiled change is three entries in an attendance watch table.

**Board effect (branch-local; the integrator re-derives on the merged tip):**
`TOTAL 4632 1323 called 1171 ported 9 detour 88.5%` — **unchanged, and that is
correct**: tagging is not porting and moves no count. What moves is the
adjudicated fraction:

```
before: 41 of 152 tagged (census's own parse)   portable surface 107  <- both wrong
after : 55 of 152 tagged (census's own parse)   portable surface  97
truth : 60 of 152 tagged (unpromotable_slot_check)  portable surface 92
```

**The 5-row gap is the malformed-class defect, not a disagreement (§1).** My
fourteen tags are spelled `HARMFUL-DLL-PRESENT-REQUIRED-AT-CUTOVER` — one
hyphenated token, deliberately uglier — so that **both** instruments parse them. I
did not touch the census regex or re-spell the five existing tags: that is
MANIFEST's finding and another lane's verdict text.

Checkers, on this branch after the change:
```
tools/unpromotable_slot_check.py  CHECK 1: 20/20 fit, 0 STALE
                                  CHECK 2: 0 UNDECLARED, 0 malformed
                                  CHECK 3: 60 tags, 51 scoped, 51 addressed, 0 MISSING
tools/dispatch_slot_map.py --check  110 registrations, 0 mismatches, 0 duplicates
mingw32-make standalone-debug       clean
```

---

## 6. WHAT I DID NOT ESTABLISH

1. **I ran nothing.** No lock (§1d). Every attendance statement is a *committed*
   run at a *named* tip, and the three tips are not mine. Nothing here is verified
   in a live process.
2. **I did not promote anything, so none of §4's safety reasoning has been tested
   by a run.** It is three static arguments that agree; that is a bound, not a
   proof.
3. **The `unported readers` column my tool prints is NOISY and no verdict here
   rests on it.** `owner` is the nearest preceding export, which for an unexported
   internal over-attributes — visibly so: it credits `??0String@System@@` with
   reading `Achievement`'s typeInfo and `??0Scene@Picking@@` with 46 CRT-init
   `LEA`s. The bias is toward calling a reader PORTED, i.e. **false-safe**. Every
   §3 verdict was made by reading the named callers in the detail listing.
4. **I did not measure indirect reachability.** `rt_indll_callers.py` sees `E8`/`E9`
   rel32 only. A `0` from it means *no direct in-DLL call*, never *unreachable* —
   a vtable or function-pointer call is invisible to it. Where I used it (§3.1) the
   answer was 18 and 2, so the direction did not matter; a future lane using it to
   justify a *promotion* must close that gap first.
5. **I did not re-derive the 25 pre-existing tags' verdicts, only their numbers.**
   Nine of nine checkable figures reproduce (§2.2), with
   `?_singleton@Configuration@Input@` off by one read (17 vs 16). I did not chase
   that one down; it does not move the verdict, and it may be a convention
   difference rather than an error.
6. **I did not touch the Object namespace's two NO-OP `?_typeInfo@` rows**
   (`CompiledExternal`, `Internal`). They are tagged, they are MANIFEST's, and
   Wave 23 OBJ's re-derived counts reproduce exactly here.
7. **A pre-existing staleness I found and did not fix:**
   `tools/rt_mint_gen.py --check` reports `standalone/rt_data_mint_tables.inc` is
   **STALE** on `wave25/base` — `ENTERED (1): ?_typeInfo@Base@Object@@`, pulled in
   by a minted vtable. `git diff HEAD -- standalone/rt_data_mint_tables.inc` is
   empty, so this predates my branch and is not mine. Regenerating mid-wave would
   collide with every lane touching mint rows. **Addressee: the Wave 25 integrator,
   before attempt 26** — this is the
   `reference_a_generated_file_goes_stale_with_no_detector` shape and the checker
   is the detector, so it only needs running.

---

## 7. RESIDUALS, WITH ADDRESSEES

* **lane MANIFEST** — take all 39 cutover-only rows (§3 rows 8-46) into the publish
  list. Fourteen of them are newly tagged by this lane and carry the registration
  condition in the tag's expiry field. **Every one of the fourteen is
  `REQUIRED (cutover)`, not merely safe** — each is resolved by name somewhere in
  our tree and `ResolveExport` returns NULL for an unpublished row with no DLL.
* **lane SWEEP** (holds the lock second) — one armed run with
  `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=1200 BANISHED_PAUSEMENU_QUIT=60
  BANISHED_W17_ATTEND=1` decides all seven §4 rows. Read the `[W17Attend]` lines
  for the seven `?_typeInfo@` symbols. **Any that reports ATTENDED is a two-line
  promotion** on the `reg_mint_promoted.cpp:950-958` template — plus §4.1's
  annotation, in the same commit.
* **whoever owns `tools/runtime_census.py` this wave** — the class-field regex
  `(?:\|\s*(\S+)\s*)?$` drops the *whole* tag, not just the class, for a class
  containing a space. Five tags are affected. If it is widened, the five may be
  re-spelled and so may my fourteen — **but not before**, or the count moves
  silently again. Until then, **print `unpromotable_slot_check.py`'s CHECK 3 count
  beside the census's**; their difference is the defect's exact size.
* **nobody** — rows 22-46. Do not reopen them looking for a promotion without first
  porting the readers named in each tag. That is namespace-scale body work, not a
  DATA-lane task.
