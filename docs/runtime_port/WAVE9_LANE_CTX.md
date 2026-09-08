# Wave 9 lane CTX — the `Video::Context` methods the frame loop calls

Tasking: lane VID2 closed the Context *construction* chain and predicted the next
wall as "`?PlatformStartup@Context@Video@@` or the first `Context` method the
frame loop calls". Find it, then port the Context methods on the frame path.

**Result: 2 ported, 0 blocked, and the tasking's premise was two-thirds already
done.** `?BindResources@` and `?BindTextures@` land with 0 mismatches over 37
oracle cases, three negative controls all CAUGHT, and 598,619 live calls in one
gameplay run. The transitive closure was 2 and the unsatisfied-door count did not
move. Along the way three of `Context.h`'s unnamed dirty bits are named, one of
its named bits is shown to be misnamed, and **seven of Wave 6 lane A's fourteen
replay-verified Context ports are measured to have ZERO live callers** — with the
mechanism, not just the observation.

Branch `wave9/lane-ctx`, on `ba7bc60`.

---

## 1. The prediction was half wrong and half right, and the half that was right
   does not point where it sounds like it points

**`?PlatformStartup@Context@Video@@` does not exist.** The shipped export
directory of `C:\GOG Games\Banished\Runtime-x64.dll` has exactly two
`Platform*` members on `Video::Context` — `?PlatformConstruct@` (0x1800f21f0)
and `?PlatformDestruct@` (0x1800f2240) — and nothing with that name. Half a
prediction spent on a phantom symbol.

The live half is "the first `Context` method the frame loop calls", and the
answer is **not in the frame loop's own code at all**.
`?Draw@MainLoop@Engine@@QEAAXM@Z` (0x1800dcd40, disassembled in full) reaches
exactly two `Context` methods:

| where | what | status |
|---|---|---|
| `1800dcd82` tail `JMP` | `?Reset@Context@Video@@` (0x1800f1e50) | ported, Wave 6 lane A |
| `1800dcd9f` `CALL` | `?ClearResources@Context@Video@@` (0x1800f2090) | ported, Wave 9 lane VID2 |

Everything else `MainLoop::Draw` does is a virtual call through
`Context+0x2160`, the backend companion — **which is our own `dx11/`**. So the
frame loop was already clear of unported Context code before this lane started.

**The Context traffic in a frame comes from the other direction.** Our DX11
backend calls back *into* `Video::Context`, by mangled name, on the hot path:

```
dx11/src/contextdata.cpp:776    CtxBindTextures   — slot 5, ContextData_BindRenderBuffer
dx11/src/contextdata.cpp:953    CtxBindResources  — the indexed/instanced draw
dx11/src/contextdata.cpp:1092   CtxBindResources  — the dynamic-vertex draw
dx11/src/contextdata.cpp:1253   CtxBindResources  — the dynamic indexed draw
```

`BindResources` runs **once per draw call**. It is the most-executed
`Video::Context` method in the process, and before this lane every one of those
calls left our own renderer and entered the shipped DLL. Both names were also
sitting in `docs/runtime_port/wave9_residue_union.txt` — a real run had asked
for both — which is the second, independent confirmation that this is the row.

This is the shape lane VID2's own §1 warned about from the other side: a verdict
reached by looking at the ported caller cannot see a dependency that arrives
from a module nobody was reading.

### An unusual and favourable fact

`get_xrefs_to(0x1800ef060)` returns four DATA references (export tables) and
nothing else. `get_xrefs_to(0x1800ef330)` adds exactly one CALL — from
`BindResources` itself at `0x1800ef093`. **Nothing inside Runtime-x64.dll calls
either function.** Their only callers in the whole process live in the video
plugin, which is us. So the "two implementations on one live object" hazard
Wave 6 lane A had to argue its way around **does not arise here at all**: after
registration our backend is the only caller and it reaches only our body.

---

## 2. Transitive closure, counted before a line was written

`BindResources` contains exactly one direct `CALL` — `CALL 0x1800ef330`, which is
`BindTextures`. Every other call it makes is `CALL qword ptr [reg+imm]`: a
virtual dispatch through a pointer the Context already holds. `BindTextures`
contains no direct call at all.

**Closure = 2 functions, 0 new resolve doors**, predicted up front and confirmed
afterwards by the audit (§6). Lane PICK's five-rows-cost-fourteen note is what
this check exists for.

---

## 3. The oracle, chosen before the first body, and the two rejections

Neither function can be **replayed** the way Wave 6 lane A replayed the setters.
A setter is a pure state transformation, so running it twice on two copies is
free. These two **issue commands**: every arm ends in a virtual call into the
backend, and running them twice would bind every resource twice and — because
each clears the dirty bits it consumed — make the second pass see a different
input from the first.

**Rejected: a live `BANISHED_RT_VIDCTXBIND` A/B.** It can only report "did it
boot and did the picture look right" — precisely the executed-but-undiffed
state. It cannot tell me that arm `0x1000` uploaded 0x20 bytes from `+0x3f8`
rather than 0x40 bytes from `+0x208`.

**Rejected: comparing the Context bytes afterwards.** Between them the two
bodies write four fields (`+0x10`, `+0x18`, `+0x1c`, and the qword at `+0x20`).
A byte-only comparison is blind to the entire point of the function: which
backend entry points were called, in which order, with which arguments. That is
the lossy-transform trap — the comparison point cannot see the work.

**Chosen: a synthetic subject plus a CALL LOG.** Three `.bss` Contexts (master,
ours, theirs), one set of fake sub-objects **shared by both sides**, and a
recorder on every vtable slot. Ours runs on one copy, the shipped body on the
other, and the probe compares

* **the full 0x2168 bytes**, with **no exclusions at all** — both subjects are
  `memcpy`'d from one master that writes every byte, so there is no region
  neither side wrote and therefore no tolerance to state; and
* **the call sequence, element by element** — tag, vtable byte offset, arity and
  arguments, with pointer arguments that land inside the subject normalised to
  an offset from *that side's* base.

That is lockstep, not end-state-only. Negative control 1 below is the proof: a
reordering defect leaves the Context byte-identical and is caught only by the
log.

### The one exclusion, and it is a derivation

Argument slots past a call shape's **arity** are excluded. Retail's call sites
load only as many argument registers as the callee takes — `?BindViewport@`'s
dispatch loads RCX alone, the image bind loads RCX/RDX/R8, the vertex-stream
bind loads RCX/RDX/R8/R9 plus one stack slot. A trampoline with one wide
signature therefore reads registers **the caller never set**, whose contents are
whatever each implementation happened to leave there. Comparing those is
comparing bytes neither side wrote. The arity per shape is fixed in
`ArityOf()` from the call site, and the record zeroes everything past it.

What that cannot see, stated rather than buried: a port passing a **sixth**
argument where retail passes five. No shape in either body has more than five.

### The fake tables are wide, and the probe prints the observed set

Eight tables, **64 slots each, every entry implemented and recording its own
index**. Lane VID2's probe guessed a slot set three times and was wrong three
times (an `EXEC-of-0`, then a read *past* the array). The derivation in
`context_bind.cpp` predicts slots 1, 2, 4, 5, 6, 7, 8, 9, 10 and 11 — and the
probe reports what was actually reached, on both sides, so prediction and
measurement can disagree out loud. They did not:

```
[CtxBind] vtable slot INDEXES observed: ours=0x0000000000000ff6 theirs=0x0000000000000ff6 MATCH
```

`0xff6` is exactly `{1,2,4,5,6,7,8,9,10,11}`.

The fake also had to be the right *size* rather than the right shape: the
sub-objects carry their companion pointer at offsets up to `+0x78` (the render
state's), so a two-qword fake would have been a silent overrun. They are 0x80
bytes.

---

## 4. Measured numbers

Gated on this build's own identity line, read out of the run's own log:

```
[LaneCTX] identity: context_bind.cpp built Sep  8 2026 13:11:07, cases=33, constant-buffer uploads=5, fake slots=64
[CtxBind] BindResources: cases=33 mismatches=0
[CtxBind] effectiveness: shipped-body backend calls=197 across 29/33 cases that did work
[CtxBind] vtable slot INDEXES observed: ours=0x0000000000000ff6 theirs=0x0000000000000ff6 MATCH
[CtxBind] BindTextures: cases=4 mismatches=0 (shipped-body calls=40 across 3/4 cases that did work)
[CtxBind] negative-control 1 (bind the vertex LAYOUT before the streams (order only)): CAUGHT
[CtxBind] negative-control 2 (clear only the dword at +0x20, not the qword): CAUGHT
[CtxBind] negative-control 3 (upload 0x20 bytes of global constants instead of 0xd0): CAUGHT
[CtxBind] TOTAL mismatches=0
```

**Effectiveness is counted inside the function**, not by a proxy outside it:
`shipped-body backend calls` is how many virtual dispatches the *shipped* body
actually made across the case matrix, and `29/33` / `3/4` is how many cases made
any at all. The four `BindResources` cases and the one `BindTextures` case that
did no work are deliberate — `dirty=0`, `dirty=0 tc>0`, `textures mask 0`,
`vertexBuffers mask 0` and `tex mask 0` exist to prove the early-out and the
unconditional trailing clears, and a probe that reported "33/33 did work" would
be lying about them.

The three negative controls are aimed at **different halves of the oracle**:

| # | defect | invisible to |
|---|---|---|
| 1 | bind the vertex layout *before* the streams | the byte comparison (state is identical) |
| 2 | clear only the dword at `+0x20`, not the qword | the call log (no call changes) |
| 3 | upload 0x20 bytes of global constants, not 0xd0 | both, unless arguments are compared |

All three CAUGHT.

### Live execution, and the two questions kept apart

A correct port with no caller reports clean forever. Both bodies carry a
liveness site placed **before** the early-out, so `calls=` means *was entered*,
with `RT_DIFF_WORK` carrying *did work* separately — a shadow after the return
merges the two:

```
[RtDiff]   Video::Context::BindResources[live]   calls=470973   worked=311345   divergences=0
[RtDiff]   Video::Context::BindTextures[live]    calls=127646   worked=127644   divergences=0
```

`BANISHED_AUTOSTART=1 BANISHED_AUTOQUIT=400 BANISHED_RT_DIFF=1`, exit 0, empty
`standalone_crash.log`. **598,619 live calls in one 400-frame run**, and the
split matters: 34% of `BindResources` calls take the `dirty == 0` early-out, so
that guard is load-bearing rather than decorative, and 2 of 127,646
`BindTextures` calls arrive with a zero mask and still clear `+0x18`.

Logs: `docs/runtime_port/wave9_lane_ctx/run_debug.txt` and `run_rtdiff.txt`,
copied out under `tools/game_dir_lock.sh acquire lane-CTX` before release.

### The measurement predates the last two commits, and here is the exact gap

`reference_lane_edits_after_report_invalidate_the_measurement.md` is about
precisely this: Wave 8's closing numbers were measured on a build whose sources
had already moved. The run above is from the build stamped `Sep 8 2026 13:11:07`
in its own identity line, and two commits landed after it. **Every line they
changed in a compiled source is a comment**, which is checkable rather than
asserted:

```
$ git diff a9b5cbc HEAD -- runtime/src/video/context_bind.cpp \
    | grep -E "^[+-]" | grep -vE "^[+-][+-]" | grep -vE "^[+-]\s*//" | grep -vE "^[+-]\s*$"
(no output)
```

`git diff a9b5cbc HEAD --stat` names only `WAVE9_LANE_CTX.md` and
`context_bind.cpp`, and the filter above shows the second contains no
non-comment change. `Context.h` and `rt_video_port.cpp` were also comment-only
and were already inside the measured build. Re-run the probe
(`BANISHED_RT_CTXBIND_PROBE=1`) on any later build before quoting these numbers
against a tree that has moved further than that.

---

## 5. Facts recovered — `Context.h`'s dirty mask is now complete

`Context.h` said "0x100 and 0x2000 and 0x8000 are inside `Reset`'s 0x1fe00 but
no lane-A function names them; they are NOT guessed here." They could not be
named from the setters, because **a setter only ORs a bit — the meaning lives in
the one function that consumes the whole mask.** Reading `BindResources` closes
all three and corrects a fourth:

| bit | is | evidence |
|---|---|---|
| `0x00100` | **video program** | arm at `0x1800ef0d4` dereferences `[RBX+0x200]`; `?SetVideoProgram@` (0x18001a040) is four instructions that compare, OR `0x100`, and store there |
| `0x02000` | **light constants** | uploads `+0x1dd8` len `0x1a0` — `?SetLight@`'s block — into the buffer at `+0x2098` |
| `0x08000` | **material constants** | uploads `+0x208` len `0x40` into the buffer at `+0x298` |
| `0x01000` | **instance**, not material | uploads `+0x3f8` len `0x20` (`_inverseScale`, `_color`) into `+0x1fc0`, which the header itself calls `_instanceConstants`. The enum spelling `kDirtyMaterialConstants` is **misleading**; it is left alone because three lanes include the header, and `context_bind.cpp` does not use it. |

Two more, both load-bearing:

* **`Context+0x200` is a POINTER, and the header models it as staging.**
  `_pixelConstantStaging[0x98]` starts at `0x200`; the first eight bytes of that
  array are `_videoProgram`. The real staging is `0x208..0x297`, which is
  exactly what `?Reset@` memsets (two calls, `0x208` len `0x40` then `0x248` len
  `0x50`) and exactly what arms `0x8000` and `0x10000` upload. The member is left
  undivided so no other lane's offsets move, with a `*** ***` note at the field.
* **`_unknown0024` is a live counter.** The transform arm fires on
  `(dirty & 0x4000) || (int32)+0x20 > 0 || (int32)+0x24 > 0` — a bit OR two
  *signed* comparisons — and the function's final clear is `MOV qword ptr
  [RBX+0x20],RBP`, one store over both. Clearing only the dword leaves a live
  transform binding that never rebinds; that is negative control 2.

And one ordering fact a normalising rewrite would destroy: **the arms are not in
bit order.** The sixteen vertex streams (`0x8`) are bound *before* the vertex
layout (`0x4`), and the transform check sits between the shader programs and the
streams. Negative control 1 exists because nothing in the Context bytes can tell
the two orders apart.

---

## 6. The audits, before and after

| instrument | before | after |
|---|---|---|
| `unsatisfied_export_scan.py` doors | 844 | 846 |
| ... unsatisfied | **130** | **130** |
| ... outside a diff guard | **127** | **127** |
| `port_bypass_audit.py` name door | 0 bypasses | **0 bypasses** |
| `port_bypass_audit.py` address door | 0 bypasses | **0 bypasses** |
| `export_name_audit.py` | 1 known placeholder | 1 known placeholder |
| `runtime_census.py` Video ported | 105 (derived) | **107 measured** (of 142 called) |

The 105 is a *derivation*, not a second measurement, and is marked as such: this
lane's three commits add exactly two `RegisterRuntimeExport` literals and touch
no other registration, so `107 − 2` is arithmetic on the one number that was
measured. Do not quote it as an independent reading.


The census is a third, independent confirmation that these were the right two
rows rather than a lane's own choice: `docs/runtime_port/census_20260907_prewave9.csv`
already carried them, unported, with `dx11/src/contextdata.cpp` named as the
caller —

```
export,?BindResources@Context@Video@@AEAAXXZ,Video,0,0,dx11/src/contextdata.cpp
export,?BindTextures@Context@Video@@AEAAXXZ,Video,0,0,dx11/src/contextdata.cpp
```

— which is the same finding as §1 arrived at from the disassembly, sitting in a
CSV since before Wave 9 opened.


The two added doors are the probe's own `ResolveExportRaw` lookups, and both are
**satisfied** (by this port), which is why the unsatisfied count did not move.
That is the closure claim of §2, measured rather than asserted.

### "The `Video::Context` cluster is closed" — measured, not asserted

Grepping the post-change `unsatisfied_export_scan.py` output for `Video@@`
returns **no `@Context@Video@@` method at all**. Fourteen rows survive and every
one of them belongs to a *different* class that merely takes a `Context&` in its
signature — `Graphics::Renderer::Draw`, `UI::UserInterface::Draw`,
`Save::SaveLoad::Draw`, `Graphics::MaterialInstance::Setup`,
`Graphics::FrameCopyImageBuffer::{Create,Generate}`,
`UI::SpriteContext::SetScissor`, plus four `Video::Device` / `RenderBuffer` /
`Configuration` lifecycle rows that belong to other lanes. The class itself is
fully satisfied.

The one remaining unsatisfied door *reached from inside* a `Video::Context` body
is `?Inverse@Matrix@Math@@QEBA?AV12@XZ` at `runtime/src/video/context.cpp:352`
— see §7 and §8.

Both names were checked against the shipped export directory with `pefile`
**before** any body was written: `?BindResources@Context@Video@@AEAAXXZ` at
0x1800ef060 ordinal 1664, `?BindTextures@Context@Video@@AEAAXXZ` at 0x1800ef330
ordinal 1667. Note the **`AEAA`** — both are *private* members, and a `QEAA`
guess resolves NULL in silence forever. `dx11/` already spells them `AEAA`.

**Both doors are registered for both symbols**, which is lane VID2 §R5's lesson
applied rather than repeated: `??3Context@Video@@` sat address-bound-only for
three phases because nothing had yet asked for it by name. Grepping
`0x1800ef060` and `0x1800ef330` across `code/`, `engine/`, `runtime/`,
`standalone/` and `dx11/` returned nothing before this file, so the address
registration adds a door rather than shadowing one.

---

## 7. Declined, with reasons

**`?Inverse@Matrix@Math@@QEBA?AV12@XZ` — declined, and it is the most important
thing in this section.** It is an **unsatisfied door inside an already-ported
`Video::Context` body**: `runtime/src/video/context.cpp:352` resolves it for
`?SetCamera@`, and `unsatisfied_export_scan.py` lists it with neither a port nor
a mint entry. At cutover it is a null call.

Two reasons for declining, one of them a correction to my own first reading:

1. It is a ~200-instruction float schedule (0x1800bb310) — a 4x4 cofactor
   inverse whose term order is heavily interleaved. Bit-exactness there is the
   moved-term hazard in its purest form, and it is a Math lane with a *perfect*
   oracle (a pure function: sweep random matrices, compare bit patterns), not a
   side errand in a Video lane.
2. **It is not live today**, which I established rather than assumed —
   see §8. So it is a latent cutover null, not a present one, and the lane that
   makes `?SetCamera@` live is the lane that must land it.

**The other ~90 exported `Context` methods** (`?SetLight@`, `?SetBones@`,
`?BeginCommands@`, the rest of the `?Bind*@` family) stay in the DLL: nothing in
our tree calls them, they have no Runtime-internal caller our code can reach, and
a registered port on a path nothing executes reports clean and proves nothing.

---

## 8. A negative result worth more than the port: seven Wave 6 Context ports
   have no live caller

Every one of Wave 6 lane A's fourteen replay-verified setters carries an
`RT_DIFF_BEGIN` at the top of its body, so a site that never appears in
`RtDiffSummary()` was never *entered*. In a full `BANISHED_AUTOSTART=1`,
400-frame gameplay run with `BANISHED_RT_DIFF=1`:

| entered | not entered |
|---|---|
| `SetColor` 239,046 · `SetTransform` 239,046 · `SetVertexBuffer` 263,237 · `SetImageBuffer` 167,495 · `SetViewport` 882 · `SetRenderBuffer` 882 · `SetFog` 787 | **`SetCamera` · `Reset` · `AddVertex` · `AddVertices` · `SetInverseScale` · `SetTime` · `SetDefaultRenderBuffer`** |

The mechanism is structural, not a gap in the harness. `?Reset@` is called from
`Engine::MainLoop::Draw` **every frame** — at `0x1800dcd82`, by direct near-JMP
inside the DLL, which the port registry cannot redirect. Same for `?SetCamera@`
from `Graphics::Renderer::Draw` and the `AddVertex` pair from
`UI::SpriteContext`. Those three callers are all still DLL bodies.

Two consequences, both actionable:

* Seven ports are in the state `reference_uncalled_port_is_not_verified.md`
  describes — registered, counted by the census, and never executed. Their
  *correctness* rests on Wave 6's replay oracle, which is real; their
  *integration* is untested and will be exercised for the first time on the run
  that ports `Graphics::Renderer::Draw`.
* The doors those bodies open — `?Inverse@Matrix@Math@@` above being the one
  that is unsatisfied — are latent, and go live all at once.

This is also why lane VID2's prediction pointed at a wall that was not there.
The Context is built by us and *used* by the DLL; the frame path only reaches
our Context code through the one module that is already ours, `dx11/`.

---

## 9. Predictions

1. **Predicted next fault site: `?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z`
   (0x180104bb0), with `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z`
   (0x180121890) immediately behind it.** Both are unsatisfied, both are outside
   a diff guard, both were in attempt 17's `[RtUnresolved]` list, and they are the
   two calls `UpdateLoop::Draw` makes on every frame. The `Video::Context`
   cluster itself is now closed — a DLL-less boot that reaches a frame will not
   stop inside it.
2. Porting `?Draw@Renderer@Graphics@@` makes seven currently-dead Context ports
   live in one step (§8) and turns `?Inverse@Matrix@Math@@` from a latent null
   into an executing one. **Land the Math port first**, or that lane's first run
   dies in `SetCamera` for a reason that has nothing to do with the renderer.
3. `?BindResources@` and `?BindTextures@` will not appear in
   `BANISHED_RT_STRICT`'s residue again.
4. The observed slot mask `0xff6` is a claim about the backend interface, not
   only about this port. If a later change to `dx11/`'s ContextData vtable makes
   the two sides' masks differ, the port is not what changed.
5. `[CtxBind]` will SKIP, not fail, at cutover — it says so, because a
   comparison with no shipped body to compare against is not a pass.
