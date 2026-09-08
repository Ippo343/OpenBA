# Wave 9 — lane VIDL: the device-lost pair

Branch **`wave9/lane-vidl`**, worktree
`C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\vidl-089e6eb4`, based on
`085782d` *Attempt 20: the failure becomes OURS*.

**Landed: 2 exported members, 2 unexported Collection instantiations, 7
BaseTypeInfo mint rows, 0 new unregistered address doors, and three stale
sentences in `engine.cpp` corrected in place. VERIFIED LIVE: the forced
device-lost round trip reached both members (`calls=1`, `divergences=0`)
and `SetSize` passed a byte-exact replay against the shipped body (9 cases,
0 mismatches, negative control CAUGHT 17 of 162).**
**Declined: nothing in the assignment. Three of `OnVideoRestart`'s callees are
taken as NAMED doors rather than ported, with the cost measured and stated.**

---

## 1. The assignment's premise, re-derived rather than inherited

The brief said lane VID's blocker was stale and that lane ENGIO had measured it,
and asked me to re-verify rather than trust it. Re-verified off the
disassembly. **ENGIO's count holds and lane VID's does not** — and lane VID was
wrong in three separate ways, each of which had been load-bearing for two waves:

| lane VID's claim | truth |
|---|---|
| "not one of its seven internal callees is ported" | `OnVideoError` has **eight** distinct direct callees and **seven were already ours** |
| "ten unexported `.data` globals" | all ten are **exported DATA** (`?_typeInfo@…`, ordinals 4853–4926), and there are **eleven** if you count the one `FindNext` reads |
| "porting either would relocate 17 nulls rather than remove any" | the real residue was **one** Array instantiation for `OnVideoError` and **three named exports** for `OnVideoRestart` |

The eight callees of `OnVideoError` (0x1800dc5e0), counted off the listing —
`get_function_callees` silently ignores `address=`, so it was not used:

| callee | name | ours? |
|---|---|---|
| 0x1800c7a50 | `?MemAlloc@MemoryAllocator@System@@` | yes |
| 0x1800c6c00 | `?Free@MemoryAllocator@System@@` | yes |
| 0x1800c6970 | `?AdjustDelimiters@Filename@System@@` | yes |
| 0x180006250 | `?GetPath@External@Object@@` | yes |
| 0x1800be9d0 | `?FindNext@ExternalFactory@Object@@` | yes |
| 0x1800f16e0 | `??1Context@Video@@` | yes |
| 0x180033880 | `~Array<VideoResource>` | yes (`array_destroy.cpp`) |
| **0x18004f620** | `Array<VideoResource>::SetSize(n, fill)` | **the only one left** |

`OnVideoRestart` (0x1800dc3b0) also has eight, and here ENGIO's list is right in
substance but its arithmetic is not: **five** are ours, not four
(`CreateVideoContext`, `AdjustDelimiters`, `MemFree`, `Free`, and 0x180034440
once §3 registers it). The three that are not are exactly the three ENGIO named.

---

## 2. The door count — a set-diff, and a prediction this lane got WRONG

Both sides measured with the same tools. The "before" is not a memory: the base
tree was extracted with `git archive 085782d | tar -x` into a scratch directory
and the audits were run **there**, against `tools/` from that same extraction.

| instrument | BEFORE (085782d) | AFTER |
|---|---|---|
| `addr_door_audit` by-address call sites | 299 | 302 (+3) |
| `addr_door_audit` distinct addresses | 212 | 213 (+1) |
| `addr_door_audit` registered — literal | 687 | 691 (+4) |
| `addr_door_audit` registered — mint | 539 | 560 (+21) |
| `addr_door_audit` **UNREGISTERED** | 15 | **15 — UNMOVED** |
| `rawdoor_audit` | 642 sites, 0 UNGUARDED-CALL | **642, 0 UNGUARDED-CALL** |
| `port_bypass_audit` | 494 name-door sites, 0 bypasses | **494, 0 bypasses** |
| `detour_target_audit` | clean | **clean** |
| `unsatisfied_export_scan` names through a door | 854 | 866 |
| `unsatisfied_export_scan` unsatisfied | 117 | **118** |
| `runtime_census` called | 1022 | 1034 |
| `runtime_census` ported | 798 | 800 |
| `runtime_census` Engine | 21/33 (63.6%) | **23/33 (69.7%)** |
| `runtime_census` TOTAL | 78.1% | 77.4% |

Every one of the +3 / +4 / +21 was predicted before it was measured and each
came out exact:

* **+3 call sites** = the three `ResolveAddr(Module::Runtime, …)` literals this
  port adds: 0x180033880, 0x1800f16e0, 0x1800dd500. No others.
* **+4 literal registrations** = the four `RegisterRuntimeAddr` calls.
* **+21 mint registrations** = 7 new BaseTypeInfo rows × 3 registrations each
  (the type info plus its two factory entry points).
* **UNREGISTERED unmoved at 15** is the check that matters: this lane opened no
  address door it did not also close.

### The set-diff of the unsatisfied names

```
removed: ?OnVideoError@MainLoop@Engine@@QEAAXXZ
         ?OnVideoRestart@MainLoop@Engine@@QEAAXXZ
added  : ?GetCompiledPath@ExternalFactory@Object@@SA?AVFilename@System@@AEBV34@@Z
         ?Load@External@Object@@SAPEAV12@AEBVFilename@System@@PEAV12@@Z
         ?OnVideoRestart@SaveLoad@Save@@QEAAXXZ
```

### ⚠ THE PREDICTION WAS 119. THE TRUTH IS 118.

The port's banner was written before the measurement and predicted
`117 -> 119`. It is **wrong**, by double-counting the baseline: it computed
`OnVideoError`'s effect (117→116) and `OnVideoRestart`'s effect (117−1+3) from
the *same* starting number. The real shape is **−2 / +3**, because
`OnVideoError`'s own name closes as well.

Recorded rather than quietly fixed, because this is precisely the class of error
the wave's "predict your delta before measuring" rule exists to surface — and it
surfaced. A lane that had only reported "118" would have looked more correct and
learned nothing.

### The TOTAL percentage falls, and that is not a regression

78.1% → 77.4%. The denominator grew by 12 names and the numerator by 2. This is
`reference_porting_can_grow_the_cutover_surface.md` measured a fourth time in
Wave 9: a faithful port that reaches its dependencies by name makes the cutover
surface **visible**, and a visible surface is bigger than an invisible one.

---

## 3. The mint: ENGIO's unchecked flag was real, and the count was seven

ENGIO flagged "10 `.data` type-id globals in this area have not been checked
against the mint", unchecked. Checked:

| global | symbol | in the mint at 085782d? |
|---|---|---|
| 0x1801fa548 | `?_typeInfo@ImageBuffer@Video@@…` | yes |
| 0x1801fa698 | `?_typeInfo@VertexProgram@Video@@…` | **no** |
| 0x1801fa580 | `?_typeInfo@PixelProgram@Video@@…` | **no** |
| 0x1801fa5f0 | `?_typeInfo@RenderState@Video@@…` | **no** |
| 0x1801fa660 | `?_typeInfo@VertexLayout@Video@@…` | **no** |
| 0x1801fa5b8 | `?_typeInfo@VideoProgram@Video@@…` | **no** |
| 0x1801f99b0 | `?_typeInfo@EdgeMesh@Graphics@@…` | yes |
| 0x1801fa708 | `?_typeInfo@GraphicsMesh@Graphics@@…` | yes |
| 0x1801f9a90 | `?_typeInfo@ReflectionImageBuffer@Graphics@@…` | **no** |
| 0x1801f99e8 | `?_typeInfo@FrameCopyImageBuffer@Graphics@@…` | **no** |
| 0x1801f9b38 | `?_typeInfo@External@Object@@…` (FindNext's) | yes |

**Eleven globals, four minted, seven not** — ENGIO said ten because it counted
the `OnVideoError` push list and not the one `FindNext` reads.

**The cause is not a defect in the mint.** `tools/rt_mint_gen.py` does not read
a frozen list; it shells out to `tools/runtime_census.py` and mints the DLL data
*our tree references*. Nothing referenced these seven, so nothing minted them.
This port references all eleven through `RT_DATA_SLOT`, which the census does
see, so regenerating picks them up: **BaseTypeInfo rows 46 → 53**.

Had the port shipped without the regeneration it would have moved `unsatisfied`
by **+7** and closed nothing — a strictly worse tree than not porting at all.

**A second number this lane got wrong and corrected:** the port banner said
*six* until the generator said *seven*. Six was counted off the table by eye.
The correction is left visible in the source.

---

## 4. Verification

Replay is **invalid** for both members and not marginally so: `OnVideoError`
destroys the process's `Video::Context` and releases every GPU resource, and
`OnVideoRestart` recreates them. Running ours then theirs would double-free the
context on the first call and crash before a byte could be compared. So the
verification is three different kinds, and only one of them is strong.

### 4.1 The strong one: `Array<VideoResource>::SetSize`, byte-exact vs the shipped body

`0x18004f620` is added as **one row** in `array_probe.cpp`'s `kJ3NonTrivial`
table. That probe already builds two identical synthetic arrays, runs OUR body
on one and the **shipped DLL body** on the other, and compares byte for byte
across nine start/target-size cases — including the realloc, append-at-capacity,
shrink, shrink-to-zero and bulk-from-empty arms. Nothing was re-derived; the
probe drives the same `SetSizeCore` and the same `ArraySpec` the registered
entry point drives.

The spec is where the reading is, and three of its fields are the ones a wrong
port would get wrong:

* **`assign unwritten = [0x04, 0x08)`** — the type id is one dword and the
  `External*` starts at +0x08, so four bytes of every element are written by
  nobody. Comparing them would fail 100% of cases and read like a catastrophic
  defect (`reference_oracle_must_not_compare_unwritten_bytes.md`).
* **`seed zero = [0x10, 0x18)`** — the assign dereferences the *source's*
  String (the copy ctor's `INC [RAX-4]`) and the destroy dereferences and frees
  the *destination's*. Everything outside that range is seeded non-zero, which
  is what makes the +0x04 window visible at all; an all-zero element is the one
  seed under which both sides agree by accident.
* **`init = nullptr`** — the only caller passes a fill, so retail's fill-less
  arm is unreachable for this instantiation.

The probe's `WrongSetSize` negative control (retail's rule with the
`oldSize != 0` guard dropped) applies to this row like every other.

### 4.2 The live drive: `BANISHED_FORCE_DEVICE_LOST` — RUN, and it fired

One run, gated on an identity line **only this build emits**, read out of that
run's own log:

```
[RtPort]   MainLoop device-lost pair (lane VIDL): 2 exports registered
           (OnVideoError + OnVideoRestart, plus Array<VideoResource>::SetSize
           0x18004f620 and elem-destroy 0x180034440 by ADDRESS; …)
```

exe md5 `99155c623fea84a26493308653fe8ac8` (built and deployed compared, equal),
`rt_run` exit 0, **`standalone_crash.log` 0 bytes**. Env: `BANISHED_AUTOSTART=1
BANISHED_RT_DIFF=1 BANISHED_FORCE_DEVICE_LOST=400 BANISHED_RT_ARRAY_PROBE=neg
BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120`.

The device really was lost and rebuilt:

```
[DeviceLost] ARMED: a synthetic DXGI_ERROR_DEVICE_RESET will be injected on Present #400
[DeviceLost] INJECTING DXGI_ERROR_DEVICE_RESET (real Present returned 0x00000000)
[DeviceLost] hr=0x887a0007 onLost=00007ff6c7e64fd9 -> calling
[DeviceLost] recreate OK; onRestored=00007ff6c7e6503f -> calling
[DeviceLost] device-lost round trip COMPLETE
```

and it reached **our** bodies:

```
[RtDiff]   Engine::MainLoop::OnVideoError            calls=1    worked=?  divergences=0
[RtDiff]   Engine::MainLoop::OnVideoRestart          calls=1    worked=?  divergences=0
[RtDiff]   Collection::Array::SetSize @0x18004f620   calls=995  worked=?  divergences=0
[RtDiff] ==== 537 sites, 18,216,161 calls, 1 divergences in 1 sites ====
[RtResolve] 0 misses — every cached resolve in this run found its symbol.
```

**`calls=1` is the expected number, not a disappointment**: the pair runs once
per device-lost event and one was injected. `SetSize`'s 995 is the resource
sweep — every `Video`/`Graphics` external alive at frame 400.

The tree-wide divergence is the **pre-existing** `Object::Factory::Startup`
self-check (WAVE9.md instrument #6, lane SYS2's open item 2), not this lane's;
all three of this lane's sites report `divergences=0`.

`[RtResolve] 0 misses` is the check that the eleven `_typeInfo` names, the four
singletons and the three new function names all resolved — a phantom mangled
name resolves NULL *in silence* while the census still counts the port
(`reference_resolveexport_phantom_names.md`), and this is what rules that out.

**READ `worked=?`, NOT `worked=0`.** All three sites are among the 415 that do
not declare did-work, so their coverage is **UNKNOWN, not zero** — the summary
says so itself. What is known is `calls=`, and `calls=1`/`calls=995` with the
`[DeviceLost] … COMPLETE` line above it is what says the bodies ran.

### 4.2.1 The probe, which is the result that actually verifies something

```
[RtArrayProbe] 0x18004f620 Array<MainLoop::VideoResource:0x18>
               arity=3 stride=0x18 align=8 cases=9 did-work=8 mismatches=0
[RtArrayProbe] ==== 195 comparisons, 0 mismatches ====
[RtArrayProbe] EFFECTIVENESS: 178 of 195 comparisons changed something
               (realloc 94, copy-loop 57, fill-loop 111, free 79;
                17 were the n==size early return)
[RtArrayProbe] UNWRITTEN WINDOWS: 12 excluded windows verified as genuinely
               unwritten, 0 turned out to be written
[RtArrayProbe] NEGATIVE CONTROL: the wrong body was flagged in 17 of 162 cases
```

Nine cases, **eight of which did work**, compared **byte for byte against the
shipped 0x18004f620**, zero mismatches. The stride and align the probe reports
are the ones read off the listing (`0x18` and `MOV EDX,8`), so the row also
checks that the spec describes the function it is registered for.

### 4.2.2 It was measured TWICE, by accident, and the two agree exactly

A stray lock contender this lane had left running won the lock one second after
the first run released it and ran the whole thing again, against the same exe
(md5 equal on both deploys) with the same environment, overwriting the copied
logs.

That was a mistake in the harness driving, not a designed replicate — but the
measurement it produced is real and is worth having, so it is reported rather
than hidden. **Every number quoted above is reproduced byte-identically by the
second run**: `calls=1 / 1 / 995`, `divergences=0` on all three, the same 195
comparisons and 0 mismatches, the same 178/195 effectiveness split, the same
17-of-162 negative control, `[RtResolve] 0 misses`, one
`device-lost round trip COMPLETE`, and `standalone_crash.log` 0 bytes both
times. The logs in the evidence directory are the SECOND run's.

The lesson is the harness one, and it is the same shape as the failure this lane
already hit twice: **a lock acquired in one step and used in another is not
held in between.** Two of this lane's `game_dir_lock.sh acquire` calls won the
lock and then EXITED, leaving it held by a lane that was not using it, while a
separate contender spun against it. Acquire and use in ONE script.

Three controls, all of which fired:

* **EFFECTIVENESS** — 178 of 195 comparisons changed something. A green run
  where nothing changed is a green run that proves nothing
  (`reference_probe_needs_an_effectiveness_control.md`).
* **NEGATIVE CONTROL** — the deliberately wrong body was **caught in 17 of 162
  cases**. A control that cannot fire is worse than none.
* **UNWRITTEN WINDOWS** — 12 excluded windows were verified as genuinely
  unwritten and **0 turned out to be written**. This is the check on this lane's
  own `[0x04, 0x08)` claim: had those four bytes actually been written by
  somebody, the probe would have said so instead of quietly excluding them
  (`reference_oracle_must_not_compare_unwritten_bytes.md`).

### 4.3 The shadow oracles

Both members carry `RT_DIFF_SHADOW` on post-conditions their callees — not our
arithmetic — write. `OnVideoRestart`'s `capKept` term is the one with teeth: it
is the only assertion in this lane whose expected value is a *stale* number
rather than a zero, so a port that "tidied up" by clearing `_capacity` (which
retail deliberately leaves stale — 0x1800dc58d writes two qwords covering +0x00
and +0x08 only) fails there and nowhere else.

---

## 5. Faithfulness decisions, each with the address it was read from

* **`AdjustDelimiters` is called twice in a row** on the record's path
  (0x1800dcad6, 0x1800dcadf) and **the type id is fetched twice** (0x1800dca2a,
  0x1800dca4a). Both kept. They are idempotent, so dropping one would not change
  today's result — which is exactly why they are kept: "it happens to be
  idempotent" is not a licence to edit a listing.
* **Two unguarded dereferences kept**: `entry.object` in `OnVideoError`'s
  release loop and `_currentLoop` in both members. Retail relies on both, and a
  guard would *skip* the notification rather than restore it (CLAUDE.md).
* **`OnVideoRestart`'s not-found AVL path faults on purpose.** When the walk
  falls off the tree retail zeroes RAX at 0x1800dc439 and loads `[RAX+0x10]`
  three bytes later. That makes "every recorded resource's class is registered
  with `Object::Factory`" a load-bearing invariant; if it ever faults, the null
  came from `OnVideoError` recording a class the Factory does not know.
* **The AVL comparison is UNSIGNED** (`JC`/`JNC` at 0x1800dc424/0x1800dc428).
  Reading it as signed would put every type id above 0x7fffffff on the wrong
  side of the tree and find nothing, *silently*.
* **`OnVideoRestart` calls `CreateVideoContext` on the SINGLETON, not `this`**
  (0x1800dc3d3 loads 0x1801fe750 into RCX). They are the same object in every
  run we have; the indirection is kept because "they are always equal" is the
  assumption that stops being true at cutover.
* **`OnVideoError` releases `_context` with the inlined allocator**
  (Enter/Free/Leave at 0x1800dcc7e..0x1800dccb2), while `~MainLoop` releases the
  same field with `Video::Context::operator delete` (0x180018500). Two members
  of one class, two different releases. Both transcribed as written.
* **Steps 4-5 and step 6 of `OnVideoRestart` are two separate loops** over the
  same array (0x1800dc410 and 0x1800dc4b0 are distinct loop heads with distinct
  counters). Not merged — merging would re-bind and re-`Load` in a different
  interleaving, and `External::Load` is not ours, so nothing in this tree could
  tell us whether that mattered.
* **The SaveLoad loop bound is re-read every iteration**, not hoisted
  (0x1800dcc62 is inside the loop, and so is the base load at 0x1800dcc30).
  The body calls a scalar-deleting destructor through a vtable — arbitrary code
  that can shrink that array or move its buffer. The port hoisted it in its
  first draft and was corrected; a hoisted bound is a port that is right only
  while that destructor happens to be well-behaved.
* **ONE DELIBERATE REORDERING, declared:** retail interleaves the ten global
  reads with the ten pushes; the port reads all ten first. Safe because the ten
  are static `BaseTypeInfo` objects and the only thing running in between is the
  allocator, which cannot reach them — but it is a reordering, and it is in the
  source as such rather than passed off as a transcription.
* **`0x180034440`'s first parameter is not a parameter.** RCX is stored to its
  home slot at 0x180034449 and never read; all three retail call sites leave RCX
  holding whatever the previous instruction put there.

---

## 6. HONEST LIMITS

1. **The `unsatisfied` count goes UP, 117 → 118.** Deliberate, and §2 gives the
   set-diff. It buys three loud named doors in exchange for one silent exported
   member, but a reader scanning for the headline number will see a regression
   and they are not wrong about the number.

2. **`OnVideoRestart` depends on three functions this lane did not port.**
   `?Load@External@Object@@`, `?GetCompiledPath@ExternalFactory@Object@@` and
   `?OnVideoRestart@SaveLoad@Save@@`. `External::Load` in particular is the
   resource-loading entry point and is a lane of its own. **A DLL-less boot that
   reaches a device reset will die in `OnVideoRestart` until those three land.**
   It will die *loudly and by name*, which it would not have before.

3. **The two members' oracles are SHADOWS, not replays.** They can catch a body
   that left the object inconsistent. They cannot catch a body that released the
   *wrong* resources, recorded them in the *wrong order*, or called a vtable
   slot adjacent to the right one — the shadow would be green for all three.
   The only byte-exact comparison in this lane is `SetSize` (§4.1).

4. **The ten type ids' ORDER is asserted by nothing.** It is transcribed from
   the ten `MOV EAX,[…]` loads in address order through the body, and it decides
   the order resources are recorded and therefore reloaded. A transposition of
   two of the ten would produce a correct-looking run with a different reload
   sequence and no oracle in this tree would notice.

5. **`Object::Factory`'s AVL node layout is transcribed from ONE listing.**
   Root at +0x00, key at +0x00, left +0x38, right +0x40, value +0x10, read only
   from `OnVideoRestart` itself. Unlike the `VideoResource` element (read
   independently from two bodies) and the `SaveLoad` entry (corroborated by
   `Save::SaveLoad::OnVideoRestart`), this one has a **single** reading.

6. **`0x180034440` is registered from `mainloop_video.cpp`, not from
   `array_destroy.cpp`.** That was a merge-safety choice, not an architectural
   one. If another lane registers the same address this wave the two
   registrations SHADOW and the later binding silently wins. The integrator must
   grep for `0x180034440` across the merged tree.

7. **The mint regeneration rewrites a GENERATED file that other lanes also
   regenerate.** `standalone/rt_data_mint_tables.inc` will conflict on merge.
   It is generated: the resolution is to re-run `tools/rt_mint_gen.py` on the
   merged tree, not to hand-merge the hunks.

8. **This lane did NOT run the DLL-less cutover boot.** That is the
   integrator's, per the brief. What §4.2 exercised is the device-lost path
   with the DLL still present.

8b. **ONE run, on one machine, on one GPU, with ONE injected reset.** The same
   limit lane ENGIO recorded for the driver itself. `calls=1` means the pair
   ran once; it does not mean it is re-entrant, or that a SECOND device loss
   after a restart behaves the same — and the second one is the case where
   `_capacity` is stale from the first, which is the one field retail
   deliberately does not clear.

8c. **All three sites report `worked=?`, not a number.** They do not declare
   did-work to the diff harness, so their branch coverage is UNKNOWN rather
   than measured. `calls=` plus the `[DeviceLost] … COMPLETE` line is what
   establishes that the bodies ran; nothing establishes which arms inside them
   were taken.

9. **The census's own blind spots apply here as everywhere.** The
   `unsatisfied_export_scan` output names two resolves that go through a wrapper
   with a non-literal argument and are invisible to it, and `addr_door_audit`
   reports 15 non-literal call sites and 8 non-literal registrations invisible
   to both halves. None of this lane's doors are among them — every registration
   is one string literal on one line and every `ResolveAddr` takes a hex literal
   — but the denominators above are not the whole truth.

---

## 7. Files

| file | what |
|---|---|
| `runtime/src/engine/mainloop_video.cpp` | NEW — both members, `kSpec_VideoResource`, the two Collection entry points, registration |
| `runtime/src/engine/rt_engine_port.cpp` | wires `RtEngineMainLoopVideoRegister()`; new switch `BANISHED_RT_MAINLOOPVIDEO`; the "HELD BACK" note replaced with why the hold is discharged |
| `runtime/src/engine/engine.cpp` | three stale sentences corrected IN PLACE (PLAN §6 step 8), not deleted — each was load-bearing for a decline that stood two waves |
| `runtime/src/collection/array_internal.h` | `extern const ArraySpec kSpec_VideoResource` |
| `runtime/src/collection/array_probe.cpp` | one row in `kJ3NonTrivial` |
| `standalone/rt_data_mint_tables.inc` | REGENERATED — BaseTypeInfo 46 → 53 |
| `docs/runtime_port/WAVE9_LANE_VIDL.md` | this file |
