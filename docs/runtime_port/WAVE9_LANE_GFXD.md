# WAVE 9 LANE GFXD — the bottom half of the `Graphics::Renderer` class lane

**Branch** `wave9/lane-gfxd`, base `085782d`. **Port commit** `fc884df`.
**Built and measured in** `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/gfxd-clean`
(a detached worktree created at `fc884df` *specifically* so the build and the audits
contain this lane's change and nothing else — see §7, which is not a formality).

---

## 1. What this lane was handed, and what it did with it

Lane GFXR corrected the record on `?Draw@Renderer@Graphics@@`: nobody had run its five
callees through the DLL's **export table**, and four of the five are ordinary named
exports. Only `0x180062d60` is an unexported internal. That turns "a wall of five
anonymous bodies" into "one unnamed body behind four portable exports", i.e. a
specifiable `Graphics::Renderer` class lane.

This lane takes **the two of those four whose closures are finite and readable** and
declines the other two with blockers it measured itself (§4).

**PORTED — four exports and seven unexported internals:**

| Address | What | Exported? |
|---|---|---|
| `0x180105110` | `?Reset@Renderer@Graphics@@QEAAXXZ` | yes |
| `0x180105700` | `?AddScene@Renderer@Graphics@@QEAAXPEBVScene@2@PEBVRenderBuffer@Video@@AEBVCamera@2@HHHI@Z` | yes |
| `0x180017a70` | `??0Camera@Graphics@@QEAA@AEBV01@@Z` (copy ctor) | yes |
| `0x1800fe7f0` | `??0Camera@Graphics@@QEAA@XZ` (default ctor) | yes |
| `0x1800626a0` | DrawGroup pass-clear (3 arrays per call) | no |
| `0x180062ae0` | `Array<Renderer::SceneEntry>::SetSize(0)` | no |
| `0x180062c40` | `Array<Renderer::Drawable>::SetSize(0)` | no |
| `0x1800637b0` | `Array<Renderer::SceneEntry>::SetSize(n, fill)` | no |
| `0x18001bdc0` | `Renderer::SceneEntry` copy-assign | no |
| `0x180062860` | `Renderer::SceneEntry` default construct | no |
| `0x18001ce90` | `Renderer::SceneEntry` range destroy | no |

The five array bodies are **lane Q's `SetSizeCore`** driven by three new `ArraySpec`
rows, not a second copy of that loop; every one of the five matched it instruction for
instruction (the `newSize == _size` early return, the `newSize > _capacity` grow with
the `* 3` bump on a by-one append from non-empty, the `realloced ? 0 : oldSize` loop
start, the realloc-destroy arm, the shrink-tail arm). `0x18001ce90` is **lane
DESTROY's `FreeInnerArraysCore`** with `kD_InnerArr_110` — the spec row already written
for that exact address (`array_destroy.cpp:255`) and already driven against the shipped
body by `array_destroy_probe.cpp:729`. Lane DESTROY registered only `0x18001c7e0`, the
`~Array` that calls it; this lane registers the callee and does not re-prove it.

---

## 2. FOUR CORRECTIONS, all from one export-table lookup

The lesson lane GFXR handed over is that **a source-tree grep proving a callee is
"absent" cannot distinguish an unexported internal from an export nobody has ported
yet**, and only the export table can. This lane applied it to its own callees before
writing any of them down, and it paid four times.

**2.1 — three addresses that a grep would have recorded as unnamed internals are
exports.** All three are `Graphics::Camera` members reached from `AddScene` and
`BeginDrawGroup`:

| Address | Actually |
|---|---|
| `0x180017a70` | `??0Camera@Graphics@@QEAA@AEBV01@@Z` — **ported here** |
| `0x1800fe7f0` | `??0Camera@Graphics@@QEAA@XZ` — **ported here** |
| `0x180017c40` | `??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z` — not ported (only `BeginDrawGroup` needs it) |

**2.2 — `renderer.cpp` says `Graphics::Camera` has five exports. It has THIRTEEN.**
The registration block at `runtime/src/graphics/renderer.cpp:678` is headed
"Graphics::Camera (all five of the class's exports)". Read out of the shipped export
table:

```
0x180017a70  ??0Camera@Graphics@@QEAA@AEBV01@@Z                    [lane GFXD]
0x180017c40  ??4Camera@Graphics@@QEAAAEAV01@AEBV01@@Z
0x180017e10  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@M@Z
0x180017e70  ?GetCameraToWorld@Camera@Graphics@@QEBA?AVTransform3@Math@@XZ
0x180017ed0  ?GetProjection@Camera@Graphics@@QEBA?AVMatrix@Math@@XZ
0x1800fe3e0  ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ  [lane F]
0x1800fe480  ?IsMirrored@Camera@Graphics@@QEBA_NXZ
0x1800fe550  ?SetZUp@Camera@Graphics@@QEAAX_N@Z
0x1800fe560  ?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z  [lane F]
0x1800fe5c0  ?SetFrustum@Camera@Graphics@@QEAAXAEBVFrustum@Volume@@@Z          [lane F]
0x1800fe7f0  ??0Camera@Graphics@@QEAA@XZ                            [lane GFXD]
0x1800fe960  ??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z  [lane F]
0x1800feb00  ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z   [lane F]
```

Five were lane F's, two are this lane's, **six remain**. Nothing depended on the wrong
count, but a lane reading that comment would have believed the class was closed. The
comment is left alone rather than edited under lane F; it is corrected here.

**2.3 — a fact neither record carried: `Reset`'s own closure is three unexported bodies
deep.** GFXR's table says `Draw` is blocked by ONE unnamed body. That is true of
`Draw`'s own call list and it understates the lane. `Reset` — one of the four "portable
exports" — calls `0x1800626a0`, `0x180062ae0` and `0x180062c40`, none exported and none
in the tree; `AddScene` calls `0x1800637b0` and `0x18001bdc0`, which drag in
`0x180062860` and `0x18001ce90`. "Portable" was the right word — all seven turned out
to be small and all seven are ported here — but **"one unnamed body" is the count for
`Draw`, not for the lane**, and the next lane should size `BeginDrawGroup` and
`AddCulledDrawItems` from their own closures rather than from that table.

**2.4 — two of `AddCulledDrawItems`' stated blockers have DISSOLVED.** See §4.2; this
is the standing "re-check a holdback's blocker before believing it" rule paying out
again.

---

## 3. Verification

### 3.1 Why there is a probe rather than a diff-oracle run

**Nothing in our tree calls any of the eleven addresses or four names.** The class's
only live door is `?Draw@Renderer@Graphics@@` (`code/Game/Game/gameupdateloop.cpp:627`
and `engine/funcs/engine_funcs.cpp:3599`), `Draw` is still the DLL's, and the DLL's own
internal `CALL 0x180105110` is a direct rel32 that no registration can redirect. So
every site's `calls=` is **0 by construction, not by accident**
(`reference_uncalled_port_is_not_verified.md`), and the diff oracle has nothing to
compare. That is lane X's situation exactly and it is stated the same way.

The oracle is therefore this lane's own probe, in `wave9_lane_gfxd.cpp`, behind
`BANISHED_RT_GFXD_PROBE` (**off by default — a shipping run executes none of it**). It
drives OURS and the SHIPPED body on two independently seeded copies of the same subject
and compares the post-state. The shipped bodies come through `ResolveRuntimeRaw`, never
`ResolveAddr`, because `ResolveAddr` consults the port registry first and this lane
registers all eleven — the probe would otherwise compare ours to ours and agree by
construction.

### 3.2 Two phases, because of where each arm can run

**PHASE 1** runs at registration and every arm is **allocation-free by construction**:
each subject array gets a static buffer and a capacity that already covers its size, so
`newSize > _capacity` is never true and neither side reaches `MemAlloc`/`MemFree`. That
matters — this init runs right after `ResolveInit()`'s `LoadLibrary`, where the DLL's
`MemoryAllocator` is constructed but **not started**, and allocating there faults on
null+0x640 (lane PKG2 paid for this once).

Phase 1 is **not a reduced test of `Reset`**: `SetSize(0)` is the only arm `Reset` can
ever take, because shrinking to zero never grows a capacity. The arm phase 1 skips is
one `Reset` cannot reach.

**PHASE 2** needs the allocator (`AddScene` grows the scene array from empty; the
SceneEntry assign's inner-array copy allocates), so it is armed at init and fired by the
first live `ProbeGate()`, placed in `Graphics::Scene::Clean` — per frame, and
unreachable before `Engine::Startup` has built a scene. Same self-scheduling contract as
`scene_grid.cpp` and `picking/scene_grid.cpp`.

### 3.3 Controls

* **Effectiveness, and it cuts BOTH ways.** Every comparison counts the comparable
  bytes the body actually changed relative to the seed. An arm declared `Changes` that
  changed nothing is reported **VACUOUS**, not as a pass. An arm declared `Untouched`
  that changed anything is reported as an **UNEXPECTED WRITE**. The first draft of the
  probe had only the first rule, which would have flagged the four deliberate
  untouched-buffer checks as vacuous and buried the two that matter.
* **Negative controls, four of them, all deliberately SMALL** — a control a memcmp would
  catch even with the port deleted says nothing about the comparison's resolution:
  1. Camera copy ctor **one dword short** (`+0xd0..+0xd3` never copied).
  2. SceneEntry init with `+0xf8` written as `0x41` rather than retail's `0x40`.
  3. A `Reset` that **also empties `_drawGroups`** — the single most plausible
     misreading of that body, and four bytes.
  4. `AddScene` with **arg6/arg7 swapped** — see §3.5; the most valuable one here.
* **Seeding is a varying non-zero pattern.** All-zero subjects are the one seeding under
  which an unwritten-byte defect is invisible, because both sides read zero and agree by
  accident (`reference_uniform_population_is_not_a_verification.md`).
* **A shipped body that does not resolve SKIPS with a message**, rather than letting the
  run report clean over zero comparisons.

### 3.4 Unwritten bytes — four holes, all real

`reference_oracle_must_not_compare_unwritten_bytes.md`. The probe excludes exactly these
and no others, and the lists are **per ARM, not per type**
(`reference_never_written_needs_by_whom.md`):

| Arm | Never written |
|---|---|
| Camera copy ctor `0x180017a70` | `+0xd5..+0xd7` (53 dwords + one byte at `+0xd4`; `sizeof` is `0xd8`) |
| Camera default ctor `0x1800fe7f0` | `+0x61..+0x63` and `+0xd5..+0xd7` |
| SceneEntry assign `0x18001bdc0` | `+0xe5..+0xe7` (the Camera hole) and `+0xfc..+0xff` |
| SceneEntry init `0x180062860` | `+0x71..+0x73`, `+0xe5..+0xe7`, `+0xfc..+0xff` |

`AddScene`'s stack record is **deliberately uninitialised** for the same reason: retail
never writes `+0xfc` nor the Camera tail, and the assign copies those uninitialised
bytes into the array element. A port that zeroed them would be "helpful" and wrong in
exactly the way that shows up as a byte divergence against a DLL-built `Renderer`.

### 3.5 The one thing in `AddScene` a reader gets wrong

**The four stack arguments are not stored in source order.** Read off the four
`[RSP+0x1xx]` loads at `0x18010572c..0x18010576f`:

```
arg5 (H, [RSP+0x170])  ->  entry +0xe8
arg6 (H, [RSP+0x178])  ->  entry +0xf8      <-- out of order
arg7 (H, [RSP+0x180])  ->  entry +0xec      <-- out of order
arg8 (I, [RSP+0x188])  ->  entry +0xf0
```

and `+0xf4` is the immediate `-1`. Negative control 4 exists for precisely this: a port
that assumed source order writes two live fields swapped and nothing else in the
function looks wrong.

### 3.6 Results — BOTH PHASES RAN, BOTH PASS

Read out of that run's own log, gated on a line only this build emits
(`[RtPort] Wave 9 lane GFXD: 4 exports + 11 addresses registered`), not on a timestamp
and not on "I rebuilt". Run under `tools/game_dir_lock.sh` held as `lane-GFXD`; the
deployed exe's md5 (`ccca0e6cae2a54f7a6c49ac9f4dc2c6e`) was checked identical to the
build output before the run. `BANISHED_RT_GFXD_PROBE=1 BANISHED_AUTOSTART=1
BANISHED_PAUSEMENU=400 BANISHED_PAUSEMENU_QUIT=60`, clean self-exit, zero-byte
`standalone_crash.log`. Log: `docs/runtime_port/wave9_lane_gfxd/gfxd_probe_run.txt`.

```
[GfxdProbe] phase1: comparisons=17 mismatches=0 vacuous=0 changedBytes=786  negControls=3 fired=3 skipped=0
[GfxdProbe] phase1: PASS
[RtPort] Wave 9 lane GFXD: 4 exports + 11 addresses registered
[GfxdProbe] phase2: comparisons=23 mismatches=0 vacuous=0 changedBytes=1039 negControls=4 fired=4 skipped=0
[GfxdProbe] phase2: PASS
```

**THE COUNTERS ARE CUMULATIVE, NOT PER-PHASE, and the label does not say so.** The
`phase2` line is the running total across both phases. Read correctly, phase 2
contributed **6 comparisons, 253 changed bytes and 1 negative control** on top of phase
1's 17 / 786 / 3. This is a defect in the probe's *reporting*, found while reading its
own output, and it is **left in the tree exactly as measured rather than fixed**: a lane
that keeps improving after it reports invalidates the build the integrator measured
(`reference_lane_edits_after_report_invalidate_the_measurement.md`). It is filed as
§6 limit 10.

**That phase 2 printed at all is itself the finding that matters most here**: it means
`ProbeGate()` fired from `Graphics::Scene::Clean`, which means the run reached live
gameplay with a built scene, which is the only place the allocating arms are safe. The
realloc arm of `Array<SceneEntry>::SetSize`, the SceneEntry assign's allocating
inner-array copy, and the `AddScene` arg6/arg7 negative control were all exercised
against the shipped bodies.

**`vacuous=0` and `fired=N/N` are the two numbers that make `mismatches=0` mean
anything.** Zero vacuous says no arm was compared while doing nothing; 7 of 7 negative
controls firing says the comparison resolves defects as small as one dword, one byte of
a scalar, one four-byte field, and a pair of swapped arguments. 1,039 comparable bytes
were actually changed by the bodies under test.

**THE FIRST RUN FAILED, AND THE FAILURE WAS THE INSTRUMENT, NOT THE PORT.** It reported
five mismatches — and all five were at the `+0x00` of an `Array` header, all five a
pointer whose low bytes differed (`ours=0xa0 theirs=0xf0`). Ours and theirs are handed
**different buffers on purpose**, so their `_data` fields differ by construction. Phase
2's `AddScene` arm had that exclusion from the start; phase 1's header arms did not.

The repair was **not** to exclude the eight bytes and move on — that would leave a blind
spot exactly where a real defect could hide, since a port that FREED and reallocated the
buffer would then pass silently. `PointersHeld` replaces the cross-comparison with a
**stronger** assertion: each side's post-state `_data` is compared against **its own**
pre-run value, which is the actual invariant of `SetSize(0)` on a non-negative capacity
— retail reuses the buffer and never reaches a `Free` on that path. Comparisons went
13 -> 17 and 19 -> 23 because that check is counted, and the run went `REVIEW` -> `PASS`
with no change to any ported body.

---

## 4. THE DECLINES, with blockers this lane MEASURED

Every address below was run through the export table **and** grepped across `runtime/`,
`standalone/`, `code/`, `engine/` and `dx11/`.

### 4.1 `?BeginDrawGroup@Renderer@Graphics@@` (`0x180104fd0`) — DECLINED, and it is the CLOSEST row

Two of its seven callees are now ours (`0x180017a70`, ported here; `0x18001b960` is a
5-byte JMP thunk to a twelfth copy of lane D's `~Array`, already characterised at
`array_destroy.cpp:666`). What remains:

| Callee | Exported? | In the tree? |
|---|---|---|
| `0x180017c40` `??4Camera@Graphics@@` | **yes** | no — same shape as the copy ctor ported here |
| `0x180062950` Array\<DrawItem\> default ctor | no | no |
| `0x1800638e0` `Array<DrawGroup>::SetSize(n, fill)` | no | no |
| `0x1800801e8` `__ehvec_ctor` | no | no (CRT) |
| `0x18007ec98` `__ehvec_dtor` | no | no (CRT) |

**The real blocker is one function, and it is not the count.** `0x1800638e0` is
`SetSizeCore` again, so it needs only an `ArraySpec` — but that spec's `assign` is a
**`DrawGroup` deep copy**: a `RenderBuffer*`, TWO `Camera`s (`+0x08` and `+0xe0`), two
scalars, and **three `Array<DrawItem>` members at `+0x1c0`** whose `operator=` allocates.
That is a genuinely non-trivial element type, in a family (`kSpec_StringExt`,
`kSpec_PkgEntry`) that has produced a near-miss every time it was extended. It wants its
own lane with a save/replay-shaped oracle, not a tail-end of this one.

Worth carrying: `BeginDrawGroup` pins the `RendererDrawGroup` head that `Camera.h`
currently records as an opaque `_head[0xe0]` — `+0x00` is the `RenderBuffer*` and
`+0x08` is a `Camera` (0xd8, ending exactly at `+0xe0`). That is read out of
`0x1801050b7..0x1801050d0` and is free for the next lane.

### 4.2 `?AddCulledDrawItems@Scene@Graphics@@` (`0x1801017a0`) — DECLINED, but TWO of its blockers have DISSOLVED

~500 instructions, a `0x26f8`-byte stack frame through `__chkstk`, and the heaviest
float body in this lane's reach (frustum culling, a `sqrtf` on a sum of squares, six
`COMISS` min/max ladders). **Re-checked rather than re-read**, and the news is good:

| Callee | Exported? | Status |
|---|---|---|
| `0x1800f9310` `??DFrustum@Volume@@` | yes | **PORTED** — `runtime/src/volume/frustum.cpp:187` |
| `0x1800f8e80` `?Contains@Frustum@Volume@@` | yes | **PORTED** — `runtime/src/volume/frustum.cpp:203` |
| `0x180128110` `sqrtf` | no | **PORTED bit-exact** — lane L, 66.8M comparisons |
| `0x1800c7a50` `?MemAlloc@MemoryAllocator@System@@` | yes | ours |
| `0x1800c6c00` `?Free@MemoryAllocator@System@@` | yes | ours |
| `0x18005f980` | **no** | **absent** |
| `0x18005f2f0` | **no** | **absent** |
| `0x18005ead0` | **no** | **absent** |
| `0x180095060` `__chkstk` | no | a CRT concern, not a port row |

So the blocker is **three unexported scene-grid query internals**, not the float work
and not the frustum maths. Those three belong with `runtime/src/graphics/scene_grid.cpp`
(lane GFX3's file), which is where a lane taking this row should start.

**The float discipline warning stands and is the reason this is not a quick pickup.** A
moved term in a float sum is bit-different only where terms nearly cancel — 1 call in
23,654 — so a clean re-run never exonerates it. This body has six near-cancellation
ladders (`SUBSS` of two nearly-equal box extents, then `ADDSS` back) and is exactly the
shape lane N lost a session to.

### 4.3 `?Draw@Renderer@Graphics@@` (`0x180104bb0`) — DECLINED

`Draw` itself is only ~180 instructions and calls nothing but its four exports,
`0x180062d60`, and four virtuals. **Two of the four exports are now ours.** What is
left is §4.1, §4.2, and `0x180062d60`: **0x2306 bytes of unexported per-pass sort, no
name, no PDB, absent from the tree.** Porting `Draw` before that would convert a *named*
residue row into an unexported one, which is a worse row because no name-based
instrument can report it — GFX2's reasoning, unchanged and still correct.

### 4.4 `?BuildParameters@MaterialInstance@Graphics@@` (`0x1801033e0`) — DECLINED, unchanged

Re-checked, not re-read: there is still **no `struct Material` anywhere** under
`runtime/types/` or `engine/types/`. `runtime/types/graphics/Material.h` pins
`MaterialInstance` (0x60) and its four entry structs and nothing else. The blocker
stands and it still belongs to a lane paired with `Graphics::Material`.

### 4.5 `?Serialize@MeshInstance@Graphics@@` (`0x1800ff680`) — DECLINED, and the closure is one row WIDER than the record, not narrower

**RE-TAKEN AT THE INTEGRATION TIP `be75284`, not inherited.** The integrator asked for
this specifically, on the grounds that lane ENC replaced two of the five traps on vtable
`0x1801554a8` and that this bears on this row. The re-take says the decline stands, and
it corrects the reason in two directions at once.

The body, read again rather than copied:

```
this+0xd8  ->  CALL 0x1800fe250              ; AnimationState::Serialize
int n = *(int*)(this + 0xd0);
rw = serializer->[+0x08];  rw->vtable[+0xa0](rw, &n, nullptr)
if (serializer->[+0x00]->[+0x08] == 0)       ; stream->direction == 0, i.e. READING
    CALL 0x18005cc60(this+0xc8, n)           ; Array::SetCount
CALL 0x18005d4a0(serializer, n, this->[+0xc8])
```

**CORRECTION 1 — `0x1801554a8` IS NOT THIS ROW'S VTABLE.** That vtable has **five
slots** (`+0x00..+0x20`), and lane ENC's `CreateReader`/`CreateWriter` are slots 3 and 4
of it. This row's virtual is at **`+0xa0`**, which does not exist in a five-slot table.
The object at `serializer+0x08` is an **`IO::ReaderWriter`**, not an `IO::Encoding` —
`runtime/types/io/Serializer.h:17` already flags exactly this confusion, noting that
`engine/types/io_serializer.h` calls both `serializer+0x08` and `stream+0x10` an
`Encoding*` and that **they are different classes**. So lane ENC's two new bodies are
not on the vtable this row dispatches through.

**Lane ENC's work IS relevant, but INDIRECTLY, and that is the more useful reading:**
`Encoding::CreateReader`/`CreateWriter` are precisely what *produce* the `ReaderWriter`
stored at `serializer+0x08`. Before lane ENC, that object could not be constructed at
cutover at all. So ENC makes this row **reachable**; it does not make it **smaller**.

**CORRECTION 2 — there is a FOURTH dependency nobody had counted.** The old record lists
three callees. The virtual through `+0xa0` is a fourth, and it is unported:
`+0xa0` is `rw_vtbl::kScalarInt`, `(int&, const char*)`, which the call site matches
exactly (`LEA RDX,[RSP+0x30]` and `XOR R8D,R8D` — the value and a null name). Its two
concrete bodies are `?Serialize@BinaryReader@IO@@UEAAXAEAHPEBD@Z` (`0x1800a88d0`, slot
index 20 = `20 * 8` = `0xa0`) and the `BinaryWriter` twin (`0x1800a8fd0`). Both are
**minted BY NAME only** in `standalone/rt_data_mint_tables.inc` and **neither body is
ported** — grepped across `runtime/` and `standalone/` at `be75284`, zero hits outside
the mint table. A name-minted slot resolves out of the DLL's export table, which is the
one thing the cutover boot does not have.

**The closure at `be75284`, measured:**

| Dependency | Exported? | Ported at `be75284`? |
|---|---|---|
| `0x1800fe250` `?Serialize@AnimationState@Graphics@@` | yes | no — drags in the §4.6 `AnimationState` cluster |
| `+0xa0` -> `0x1800a88d0` / `0x1800a8fd0` (`kScalarInt`) | yes | **no — minted by name, body absent** |
| `0x18005cc60` `Array::SetCount` | no | no |
| `0x18005d4a0` element-serialise loop | no | no |

All three original addresses were re-grepped against `be75284` and returned **zero
hits**, so nothing in the old record dissolved either. **Verdict unchanged, closure
+1 row.**

Carrying forward for whoever takes it: the branch is `stream->direction == 0` meaning
**reading** — `SetCount` runs on the READ path only, and the element loop runs on both.
On the WRITE path this is a pure read and replays into two independent sinks, so
`runtime/src/io/save_trace.{h,cpp}`'s emission-sequence oracle is the right instrument
(`reference_write_path_serialize_is_replayable.md`). Compare the emission SEQUENCE, not
bytes, and use **retail's** `rw_vtbl` offsets — the profile build puts the same three
slots at `+0x90`/`+0xb0`/`+0xb8`
(`reference_encoding_vtable_offsets_differ_between_builds.md`).

### 4.6 `?Update@AnimationManager@Graphics@@` (`0x1800fc240`) — DECLINED, explicitly, as GFXR asked

GFXR's widening confirmed here by the same export-table run: `AnimationState::Update`
calls six more (`CopyInstancedToUnique`, `UpdateRootOnly`, `UpdateNodes`, `ApplyEvents`
— all exports — plus `0x18005aa30` and `0x18005b1e0`, both unexported), and none of the
seven is in the tree. Taking it as one function trades one named residue row for seven,
two unnamed. **This lane declines it explicitly rather than taking it**: it is a
`Graphics::AnimationState` class lane.

---

## 5. Audit numbers, matched denominators, set-diffed

**The "before" is this same clean tree with this lane's four files reverse-applied**, and
both halves were taken back-to-back in `gfxd-clean` — not from a neighbouring worktree,
which is how lane GFXR nearly quoted another lane's untracked files as its own delta.

**Predictions were written before measuring. Three of six were wrong, all in the same
direction, and they are recorded rather than quietly corrected** (§6 limit 5).

| Audit | Before | After | Delta | Predicted? |
|---|---|---|---|---|
| `unsatisfied_export_scan` | 854 names, 117 unsatisfied | 854, 117 | **0** — output byte-identical, set-diff run not just the count | **yes** |
| `runtime_census` TOTAL | 4632 / 1022 called / 798 ported / 78.1% | 4632 / **1032** / **808** / **78.3%** | called **+10**, ported **+10** | **no** (predicted +4) |
| … Graphics | 599 / 90 / 62 / 68.9% | 599 / 94 / 66 / 70.2% | +4 / +4 | yes |
| … internal | 115 / 102 / 88.7% | 121 / 108 / 89.3% | +6 / +6 | no |
| … ports with no caller | 88 exports, 194 addresses | 88, **195** | **+1: `addr 0x18001ce90`** — triaged below | no |
| `port_bypass_audit` ADDRESS | 377 sites; ADDR-REG 83, BYPASS **3**, NO-EXPORT 178, UNPORTED 97 | **390** sites; **87**, BYPASS **3**, **187**, 97 | **+13 sites**, ADDR-REG +4, NO-EXPORT +9, **BYPASS unchanged** | no (predicted +2) |
| `port_bypass_audit` NAME | 494 sites, OK-ORACLE 453 | 494, 453 | **0** | no (predicted growth) |
| `rawdoor_audit` | 642 sites, **0 UNGUARDED-CALL**, 71 DEFERRED | 654, **0**, 72 | +12 / +1, still **0 UNGUARDED** | yes |
| `addr_door_audit` | 299 by-address sites, 1226 registered (687 literal), **UNREGISTERED 15** | 301, **1237 (698 literal)**, **UNREGISTERED 15** | +2 sites, **+11 literal registrations**, UNREGISTERED **unchanged** | yes |

**`called` GREW by 10, and this lane says so with numbers rather than hiding it**
(`reference_porting_can_grow_the_cutover_surface.md`). The cause is specific and worth
being precise about: **the growth is the PROBE's oracle doors, not production
reachability.** Ten of the eleven addresses acquire a `ResolveRuntimeRaw(0x…)` literal in
the probe, and that literal is what makes the census call them "called". Every one of
the ten is also newly *ported*, so `called` and `ported` moved by the same +10 and
coverage went **up** 0.2pp. No production call site changed; §3.1's "inert" claim is
unaffected.

The +13 ADDRESS-door sites decompose exactly: **2** `ResolveAddr` literals in the ported
bodies (`0x18001cdf0`, `0x18001c750` — both already lane Q's ports, so both land in our
own code) and **11** `ResolveRuntimeRaw` oracle literals in the probe. Of those 13, the
**4** on exports bucket as ADDR-REGISTERED and the **9** on non-exported addresses as
NO-EXPORT — which is the whole of the +4/+9 split above, with nothing unaccounted for.
**`BYPASS` did not move**: the 3 reported are pre-existing and all in
`runtime/src/ui/spritecontext_probe.cpp`.

**TRIAGE of the one new "port with no caller" row, because the census says a new name
there has NOT been triaged.** `addr 0x18001ce90` is registered so that a future `Draw`
port — and the DLL-less boot — reaches our body rather than the DLL's. It has no door in
our tree because this lane reaches it through `ArraySpec::destroy`, a function pointer,
not a literal. It is **not unverified**: `array_destroy_probe.cpp:729` already drives
`FreeInnerArraysCore` + `kD_InnerArr_110` — the exact pairing registered here — against
the shipped `0x18001ce90`.

**Every one of the eleven addresses was checked for an existing registration before
being written**, because a duplicate is last-binding-wins and silently makes one body
dead code (`reference_grep_for_the_port_before_writing_it.md`). All eleven came back
with **zero** `RegisterRuntime*` hits across `runtime/`, `standalone/`, `code/`,
`engine/` and `dx11/`; the only pre-existing mentions are five comments
(`standalone/scene_watch.h:75`, `code/Game/Game/gameupdateloop_load.cpp:1748`,
`code/Game/Game/gameupdateloop.cpp:2162`, `runtime/src/graphics/renderer.cpp:204`,
`runtime/src/collection/array_destroy.cpp:592`).

---

## 6. HONEST LIMITS

1. **`calls=` IS ZERO ON EVERY SITE AND WILL STAY ZERO UNTIL SOMEBODY PORTS `Draw`.**
   This is the lane's central limitation and it is structural, not an oversight: the
   DLL's `Draw` calls `Reset` and `AddScene` with a direct rel32 that no registration
   redirects. These eleven registrations change nothing in a live run. They are what a
   `Draw` lane needs to exist, and they are "cheap insurance" in exactly the sense lane
   GFXR's limit 3 uses — **not evidence of anything, and not a closed residue row.**

2. **The probe is a well-powered test of a SMALL, HAND-BUILT population, not a proof.**
   Phase 1 runs each arm on one seeded subject with `kNs=3` scene entries, `kNg=2` draw
   groups, `kNd=5` drawables and `kNi=4` items per pass. That is enough to reach every
   branch these bodies have on the `Reset` path, and the negative controls say the
   comparison is discriminating — but it is nothing like lane GFXR's 2.24M cases, and it
   must not be read as if it were. A larger, randomised population per arm is the
   obvious strengthening and this lane did not do it.

3. **`kSpec_SceneEntry` and `kSpec_DrawItem` carry `unwritten` windows of 0/0, which is
   a LIE THE STRUCT FORCES.** `ArraySpec` has one assign window and one init window;
   these two rows need two and three. The real hole lists live in this file's probe,
   which is sound because a probe is the only consumer of those fields — **but if a
   future caller drives `kSpec_SceneEntry` through `array_probe.cpp`, that probe will
   compare bytes neither side writes and report a divergence that is not one.** The same
   one-window limit is already filed against `kSpec_Sprite38` and `kSpec_SpriteDraw`;
   this lane adds two more rows to it rather than widening a shared struct under lane
   COLL mid-wave.

4. **Phase 2 DID run (§3.6), but on ONE subject each.** The realloc arm was exercised
   with a single growth from empty to one element, and the SceneEntry assign with a
   single four-element inner array. The `* 3` capacity bump on a by-one append from
   NON-empty — the branch `SetSizeCore` takes on the second and later `AddScene` of a
   frame, which is the common case in a real `Draw` — was **not** reached by this probe,
   because it only ever adds one scene. That branch is lane Q's code and lane Q's
   probe covers it for other specs, but it has not been driven against the shipped
   `0x1800637b0` specifically.

5. **Three of six audit predictions were wrong, all by under-counting the probe's own
   doors.** I predicted `ported +4` (it was +10), `ADDRESS door +2` (it was +13) and
   growth in the NAME door (there was none — `ResolveRuntimeRaw(addr)` is an ADDRESS
   door; only `ResolveExportRaw(name)` is a NAME door). The direction of the error is
   consistent: an oracle door is a door, and a probe that names eleven addresses adds
   eleven of them. Recorded because the next lane will make the same estimate.

6. **The `ArraySpec` rows live in `wave9_lane_gfxd.cpp`, not in `array.cpp`, which is a
   deliberate departure from lane GFX3's and lane UIF's precedent.** That precedent's
   stated reason is "a private second copy of `SetSizeCore` in the graphics tree is
   exactly the drift this parameterisation exists to prevent" — and that reason is
   satisfied, because this file calls `rtcoll::SetSizeCore` and writes no loop of its
   own. What it avoids is a third concurrent lane editing `array.cpp` in the same wave.
   **The cost is that `array_internal.h`'s "Only two files include this" is now stale by
   one.** Not edited, because it is a neighbour's header; recorded here instead.

7. **Nothing here was run with the DLL absent.** That is the integrator's cutover boot,
   and this lane was told not to run it.

8. **`Renderer::Reset` deliberately does NOT empty `_drawGroups`**, and if that reading
   is wrong, negative control 3 is the thing that would have caught it and everything
   below `Reset` in this lane is wrong with it. It is read straight off
   `0x180105110..0x180105173`: the group *objects* are cleared in place via
   `0x1800626a0` and the array keeps its size, its capacity and its buffer, so the next
   frame's `BeginDrawGroup` reuses them. `_scenes` and `_drawables` are emptied and even
   they keep their capacity.

9. **PHASE 1's allocation-free guarantee has an ORDERING DEPENDENCY that is currently
   satisfied and is not enforced by anything.** Phase 1 drives `rtcoll::SetSizeCore`,
   whose first act is lane Q's own `ProbeGate()` — and lane Q's probe **allocates**. It
   is inert here only because `rtcoll::g_probePending` is armed by
   `RtCollectionPortInit()`, which `standalone/resolve_standalone.cpp:345` runs **after**
   `RtGraphicsPortInit()` at line 332. That ordering is documented in `rt_port.h`
   ("it must run LAST of the wave inits") for an unrelated reason, and if it were ever
   reversed, this lane's phase 1 would fire lane Q's allocating probe at the one moment
   in the boot where allocating faults. Checked, not assumed; recorded because the
   dependency is invisible at both call sites.

10. **The probe's summary counters are CUMULATIVE and the `phase2` label does not say
   so** (§3.6). Left as measured rather than fixed, because editing after reporting
   invalidates the build the integrator measured. A one-line reset of the counters
   between phases is the fix, and it should be taken together with whatever change
   next touches this file.

---

## 7. Worktree churn — the environmental hazard, and what it actually cost

The brief warned about this and it happened, worse than expected. **This session's
isolated worktree was re-pointed SIX times mid-task**, each time onto a different lane's
checkout on a different branch:

```
flush-089e6eb4 -> lane-type-089e6eb4 -> lane-gfxd -> lane-plat
               -> vidl-089e6eb4 -> rres-089e6eb4 -> flush-089e6eb4
```

Two concrete pieces of damage, both recovered:

1. **`git hash-object` silently hashed the WRONG worktree's files.** A single command
   that hashed all four blobs was re-pointed between its first and third sub-command, so
   two of the four blob hashes came from `rres-089e6eb4`'s copies of
   `rt_graphics_port.cpp` and `scene.cpp`. Caught because the two new files failed to
   open at all (they do not exist there). **A lane whose four files all existed in the
   neighbouring worktree would have committed the neighbour's content with its own
   message and nothing would have said so.**

2. **This lane's two new files were DELETED from `lane-gfxd` and the two modified files
   reverted**, between the build and the commit — the "an edit was reverted on disk
   before its author could commit it" damage. Recovered because authoritative copies
   were kept in the scratchpad, outside the repo, exactly as the brief instructs.

Separately, and reported to the integrator at the time rather than cleaned up: **another
lane's uncommitted work was sitting STAGED in `lane-gfxd`** (`array_internal.h`,
`array_probe.cpp`, `engine.cpp`, `rt_engine_port.cpp`, `External.h`,
`rt_data_mint_tables.inc`, and a half-applied move of `mainloop_video.cpp` — deleted in
the index, present untracked). None of it was touched, reverted or committed.

**The repair, and it is the reusable part:**

* **Commit with git plumbing.** `hash-object -w --path` / `read-tree` / `update-index
  --cacheinfo` / `write-tree` / `commit-tree` / `update-ref` with compare-and-swap,
  against a temporary index. Plumbing **never reads the working tree**, so no amount of
  foreign content in the directory can leak into the commit. `git diff --stat 085782d
  wave9/lane-gfxd` is exactly the four files, 1253 insertions, nothing foreign.
* **`--path` matters.** This repo checks out CRLF; files written by a tool are LF.
  `git hash-object -w --path <repo-path> <scratch-file>` applies the repo's filters, so
  the blob matches what a normal `git add` would have produced.
* **Build and measure in a worktree created at the COMMIT, not in the working
  directory.** `gfxd-clean` is detached at `fc884df`, so its build contains this lane's
  change and nothing else. The first build taken in `lane-gfxd` had a different md5
  (`0713fac` vs `7ff3ac4`) purely because a neighbour's uncommitted files were compiled
  into it — a contaminated exe that would have carried this lane's identity line and
  been reported as this lane's measurement.

---

## 8. Files

| Path | What |
|---|---|
| `runtime/src/graphics/wave9_lane_gfxd.cpp` | the eleven bodies, three `ArraySpec` rows, the two-phase probe, `RtGraphicsWave9GfxdRegister()` |
| `runtime/src/graphics/wave9_lane_gfxd.h` | `ProbeGate()` and the wave init |
| `runtime/src/graphics/rt_graphics_port.cpp` | +14 lines: the extern, the `BANISHED_RT_GFXD` gate, the identity line |
| `runtime/src/graphics/scene.cpp` | +15 lines: the phase-2 `ProbeGate()` in `Scene::Clean` |
| `docs/runtime_port/WAVE9_LANE_GFXD.md` | this document |

**Kill switch:** `BANISHED_RT_GFXD=0` leaves the whole lane unregistered.
**Probe:** `BANISHED_RT_GFXD_PROBE=1`. Off by default; a shipping run executes none of it.
