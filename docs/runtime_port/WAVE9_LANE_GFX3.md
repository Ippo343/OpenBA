# WAVE 9 LANE GFX3 — the five `Graphics::Scene` by-address doors, CLOSED

**Base:** `ba7bc60` (integration tip, "Merge lane PKG3 and record attempt 17").
**Branch:** `wave9/lane-gfx3`.
**Kill switch:** `BANISHED_RT_GFXGRID=0` — with it off, `scene.cpp`'s five
forwarders go back to the DLL exactly as they did before this lane.
**Probe:** `BANISHED_RT_GFXGRID_PROBE=1` / `=neg`.
Date: 2026-09-08.

---

## 0. The stale-worktree check — EIGHT for eight now

This worktree was created at **`965361d`**, which predates the entire runtime
port: `ls docs/runtime_port/` failed on the first command and `CLAUDE.md` still
described the proxy-DLL mode. Reset onto `wave/upgrade-path-complete`
(`ba7bc60`) before a line was read. **Nothing below was concluded from the stale
tree.** Lane FS counted six lanes hitting this, lane GFX2 was the seventh; this
is the eighth. It is a harness defect, not eight independent mistakes.

---

## 1. Result

| # | Address | Body | Verdict |
|---|---|---|---|
| 1 | `0x18005e4a0` | `GridInit` (`Scene::Scene`) | **PORTED — row closed** |
| 2 | `0x18005e5e0` | `GridAdd` (`Scene::AddObject`) | **PORTED — row closed** |
| 3 | `0x18005f090` | `GridUnthread` (`Scene::RemoveObject`) | **PORTED — row closed** |
| 4 | `0x18005ee30` | `GridReindex` (`Scene::Clean`, per frame) | **PORTED — row closed** |
| 5 | `0x18005f1f0` | `FreeGlobals` (`Scene::~Scene`) | **PORTED — row closed** |

Plus their closure:

| Address | Body | Where |
|---|---|---|
| `0x18005f4b0` | `GridThread`, the thread-in half of the re-index | `runtime/src/graphics/scene_grid.cpp` |
| `0x18005fb00` | `Array<Scene::Node>::SetSize(n, &fill)` | `runtime/src/collection/array.cpp`, as `kSpec_Node18` in lane Q's family |

**5 doors in, 7 bodies written, 0 new doors out.**
`tools/addr_door_audit.py` set-diff: **5 CLOSED, 0 NEW**, `UNREGISTERED 42 → 37`.

Nothing declined.

---

## 2. The closure, counted BEFORE any code was written

Lane PICK's five census rows called fourteen more unexported internals; lane
VID2 counted 21 bodies up front and added zero doors. This lane counted first.
`docs/runtime_port/callgraph/retail_edges.csv` gives the closure exactly, and
each edge was confirmed against the disassembly rather than trusted:

```
GridInit      -> 0x18002fea0
GridAdd       -> 0x18002fea0, 0x18005fb00, 0x18005ee30
GridReindex   -> 0x18005f090, 0x18005f4b0
GridUnthread  -> (leaf)
GridThread    -> (leaf)
FreeGlobals   -> Enter / Free / Leave
```

Three names outside the five, and their disposition:

* **`0x18002fea0`** — already ported and registered by lane Q (`kSpec_I32`).
* **`0x18005fb00`** — not ported. Added to lane Q's `SetSizeCore` family rather
  than reimplemented here, so there is ONE body for the family instead of a
  private copy in the graphics tree that can drift. It also inherits lane Q's
  synthetic-subject replay for free (§5.3).
* **`0x18005f4b0`** — not ported, and **on no census list at all**, because
  nothing outside the DLL calls it. Porting `GridReindex` without it would have
  swapped one unregistered door for another.

`0x1800c7a50` / `0x1800c7ad0`, the allocator entries the SetSize bodies and
`FreeGlobals` reach, are already armed by `RegisterAllocatorAddresses()`.

### The grep that saved a duplicate registration

`0x180009c30` (`RtFreeLocked`, `Scene::RemoveGlobalObject`) was on my list as
"trivially the `MemFree` static with an ignored first argument" — which is
exactly what it is, and which is why **lane COLL had already ported and
registered it** in `runtime/src/collection/linkedlist.cpp`. A second
`RegisterRuntimeAddr` is last-binding-wins and would have silently made one of
the two bodies dead code.

Worth recording HOW it was nearly missed: my first grep for the seven addresses
across the tree returned only `docs/` rows — because the result was **truncated
at 60 lines by a `head_limit`**, and a truncated grep is a lower bound that does
not say so. It was the second, narrower grep that found the port. Same rule as
WAVE9.md's "A GREP FOR A LITERAL IS A LOWER BOUND", one level down: *so is a
grep whose output was cut.*

`0x18001f470` is lane PICK's and is not touched here either.

---

## 3. THE FACT THAT DECIDES EVERY ONE OF THESE BODIES

### A cell's chain is CIRCULAR, not null-terminated

Read out of `0x18005f4b0`'s empty-cell arm at `0x18005f51c`: when a node is the
first in its cell, retail writes `_next = id` and `_prev = id` — the node points
at **itself**. It does not write `-1`.

**The first draft of `GridUnthread` here got this wrong, and the wrong reading is
seductive.** `GridUnthread`'s two splice stores are UNCONDITIONAL:

```
18005f0cf  nodes[nodes[id]._prev]._next = nodes[id]._next
18005f0e5  nodes[nodes[id]._next]._prev = nodes[id]._prev
```

with no `if (prev >= 0)` anywhere. Against a null-terminated list that says
retail writes to `nodes[-1]` — sixteen bytes in FRONT of the array buffer, i.e.
into the allocator's boundary tag. This project has already closed a wild write
of exactly that shape (`reference_load_wild_write_high_dword_7fc.md`), so
"retail has a wild write here, transcribe it faithfully" was a plausible and
completely wrong conclusion, and it would have produced a port that corrupts the
heap on every removal.

With a circular chain a lone node has `prev == next == id` and both stores land
on the node itself. It is also the only reading under which
`GridUnthread`'s **second** head fix (`0x18005f11e`: `if (cellHeads[cell] == id)
cellHeads[cell] = -1`) is live at all: the first fix sets `head = _next`, and for
a lone node `_next` IS `id`, so the head still names the node being removed and
only the second fix empties the cell.

A null-terminated reading makes that second fix dead code — which is **the
missing-erase shape** that cost this project a teardown 100%-CPU hang and an
`IsA` execute crash (`reference_workarea_bst_bad_entry.md`), with `CheckPtr` a
bare `RET` in every shipped build so nothing downstream would have caught it.
The lane's third negative control (§5.2, N3) is that exact defect, and the probe
flags it.

The `-1` that does exist is written into the node being removed AFTER the
splice. It marks a node unthreaded; it is never a chain terminator.

### Two smaller facts, both from the disassembly

* **`GridReindex` uses only X and Y.** Retail loads all six floats of
  `_worldBounds` but the z pair is spilled to `[RBP-0x18]` and then clobbered by
  the `{max.x, max.y}` qword at `18005eeb4` before any read. That is a dead load
  in the shipped build, not a term this port dropped.
* **The level is an exponent field, not a logarithm call**:
  `level = (int)(bits(cellSize / (halfExtent + 0.001f)) >> 23) - 127`. `SHR` is
  LOGICAL, so a negative ratio would read its sign bit as part of the exponent.
  Reproduced as an unsigned shift over the raw bits.

Constants lifted as raw bits with `read_memory`, never retyped from a decimal
(`reference_crt_coefficients_must_be_lifted_not_retyped.md`):
`0x180161694 = 0x3f000000` (0.5f), `0x18016166c = 0x3a83126f` (0.001f),
`0x18016169c = 0x3f800000` (1.0f).

The Morton interleave at `18005efca..18005f043` is transcribed operation for
operation rather than replaced by a canonical `part1by1`. The two agree on the
inputs that reach them, but the compiler's form shares sub-expressions in a way
that is easy to "simplify" into something that differs somewhere — and this
container family has already produced four near-misses that all passed green
runs, differing only in an alignment immediate, a shift, and a trailing call.

### `kSpec_Node18` is NOT `kSpec_Window`, and the difference is one field

`0x18005fb00` and `0x180024c10` agree on stride (0x18), on alignment (8) and on
the element assign (three plain qword moves). They disagree on the tail:
`0x180024c10` has an `else` arm that builds an empty `Window`; **`0x18005fb00`'s
loop has no else arm at all** — `CMP R8,RSI; CMOVL RCX,R10` picks the old element
or the fill and there is no third case. Reusing the lookalike would have run
`InitWindow` over the new tail on any two-argument call and written three qwords
retail never writes. Fifth time this family has offered a wrong answer by
resemblance.

---

## 4. What is deliberately NOT compared

`reference_oracle_must_not_compare_unwritten_bytes.md`. Both windows derived
from the disassembly, not discovered from a failure:

* **`SceneNode::_pad14` (+0x14).** BOTH of `GridAdd`'s arms build the
  `{_cell, ?}` pair as ONE qword out of a stack slot whose high dword is never
  written (`MOV dword [RSP+0x30],0xffffffff` then `MOV RAX,qword [RSP+0x30]`,
  nothing writing `[RSP+0x34]`). Retail copies uninitialised stack into it on
  every insert. The port's append arm uses a genuinely partially-initialised
  local so the SHAPE is reproduced; the reuse arm writes only `_cell`.
* **Every Array's `_data` pointer.** Two independent allocations. `_size`,
  `_capacity` and the full CONTENTS out to `_size` are compared instead — only
  to `_size`, because `_capacity` never shrinks and the slack holds allocator
  fill that differs after a realloc.

---

## 5. The oracle

### 5.1 Lockstep synthetic subjects, not a live diff and not an end-state compare

`runtime/src/graphics/scene_grid_probe.cpp`. Two private `Graphics::Scene`s,
ours through the ported bodies and theirs through the shipped ones reached via
**`ResolveRuntimeRaw`** — never `ResolveAddr`, which consults the port registry
first and would have compared ours to ours
(`reference_resolveaddr_consults_port_registry_first.md`).

Each of **18 indexed steps** is applied to both subjects and the WHOLE scene is
compared immediately, per lane PICK's rule: *"a defect that cancels out — push a
link on the free list in the wrong order, pop it back in the wrong order —
compares equal at the end while every query in between saw a different grid."*
The lane's N2 control is that exact defect and it is caught at step 14.

Both subjects point at the SAME geometry blobs, so `SceneNode::_object` is
directly comparable rather than excluded. The blobs are synthetic — nothing here
reads a `Geometry` through anything but `_worldBounds` at +0x4c, so no vtable,
factory or type id is involved (`reference_synthetic_subject_beats_a_fake.md`).

The population is built to separate the arms: four objects in ONE level-7 cell
(so the ring has a genuine middle node), objects on four different quadtree
levels, a re-index that does not move and one that does, and a non-empty free
list before three of the adds.

**Measured (run `gfx3P2`, §6):**

```
[GfxGridProbe] comparisons=19 mismatches=0
[GfxGridProbe] work: addAppend=7 addReuse=3 threadEmpty=7 threadRing=4
[GfxGridProbe] work: unthreadHead=1 unthreadMiddle=2 unthreadEmptied=1
[GfxGridProbe] work: reindexFirst=10 reindexMoved=1 reindexNoop=8 levels=4 maxRing=4
[GfxGridProbe] freeGlobals: post-state clean, reclamation agrees (blocks reclaimed ours=3 theirs=3)
[GfxGridProbe] PASS -- every ported grid body agreed with the shipped one
```

No `GAP:` line was printed, which is the effectiveness control saying every arm
it names ran at least once. `maxRing=4` is the one that matters most: without a
cell holding more than two nodes the MIDDLE-of-ring unthread is never exercised,
and the probe says so in as many words when it is not reached.

### 5.2 SIX negative controls — one per ported body

Lane GFX2's own finding was that a control perturbing one input, going red on one
site and leaving two others green, *"is a partial control that reads like a
complete one."* So there is one defect per body, and each is a defect this
project has shipped or nearly shipped rather than a random corruption:

| # | Body | Injected defect |
|---|---|---|
| N1 | `GridInit` | one wrong level offset (a transcribed constant table) |
| N2 | `GridAdd` | free list popped from the FRONT — the "cancels out" shape |
| N3 | `GridUnthread` | lone-node head clear removed — **THE MISSING ERASE** |
| N4 | `GridThread` | new node made the head instead of the ring tail |
| N5 | `GridReindex` | x and y swapped in the Morton interleave |
| N6 | `FreeGlobals` | only the first node of the list freed |

**Measured (run `gfx3N`): `negative controls: 6/6 flagged`**, each on the right
step and naming the right term:

```
MISMATCH step=neg:00:init            term=levelOffsets[i] ours=5460 theirs=5461
MISMATCH step=neg:14:add-reuse       term=returned node id ours=3 theirs=2
MISMATCH step=neg:13:remove-lone     term=cellHeads[i]  ours=2 theirs=-1
MISMATCH step=neg:10:unthread+thread term=cellHeads[i]  ours=2 theirs=0
MISMATCH step=neg:17:reindex-all     term=node._cell    ours=12698 theirs=16863
```

N3's line is the dangling head itself: `cellHeads[i] ours=2 theirs=-1` — the cell
still names a node that has left.

**N5 is the finding inside the controls.** It fired at step **17**, not at step 9
where the re-index actually moves an object. Step 9 moves `g1` to `(500, 500)`
and step 8 re-indexes it in place — both **diagonal**, so `gx == gy` and swapping
them changes nothing. Only step 17, which re-indexes an asymmetric object at
`(300, -200)`, distinguishes them. A population of diagonal positions would have
left the x/y swap invisible while every counter read healthy — the same shape as
`reference_uniform_population_is_not_a_verification.md`, in a dimension
("symmetric coordinates") that no coverage counter measures.

### 5.3 `0x18005fb00` gets an independent second oracle for free

Because it went into lane Q's family it inherits that lane's synthetic-subject
replay against the shipped body:

```
[RtArrayProbe] 0x18005fb00 Array<Graphics::Scene::Node> arity=3 stride=0x18 align=8
               cases=9 did-work=8 mismatches=0
```

That is separate evidence from the grid probe, which reaches the same body only
through `GridAdd`'s append arm.

### 5.4 THE HARNESS MANUFACTURED ITS OWN DIVERGENCE, AND THE RUN CAUGHT IT

`FreeGlobals` has no replay: it frees. Comparing `{head, tail, count} == {0,0,0}`
alone **cannot distinguish "freed every node" from "freed the first node and then
cleared the header"** — both leave the same post-state, which is a control that
says PASS while measuring nothing. So a second observation was added: after the
call, ask the allocator for three blocks of the node's size and count how many of
the three ORIGINAL node addresses come back.

The first version reported **`ours=0 theirs=3`** on a correct port. Cause: it ran
ours, then theirs, then measured both counts. The allocator's free list is LIFO,
so by the time ours' blocks were asked for, **theirs' three frees were in front
of them** and satisfied the request. The two halves are now serialised —
build, free and re-allocate consecutively per subject — and the run reads
`ours=3 theirs=3`.

Two things this cost and bought:

* the first version's summary line printed **"post-state MISMATCH"** when it was
  the reclamation count that differed, because one verdict covered two
  observations. They are printed separately now
  (`reference_shadow_predicate_must_name_its_failing_term.md`).
* N6 exists precisely because the reclamation check's ABILITY to fail is not
  self-evident. It flagged, so the check is real rather than decorative. Had it
  not, the report says in as many words that the `FreeGlobals` result is
  worthless.

---

## 6. Live coverage — and why `calls=0` here is NOT the alarm

DLL-present run under `tools/game_dir_lock.sh acquire lane-GFX3`, logs copied out
BEFORE the release. Both runs gated on an identity line **only this build
emits**, read out of the run's own log — `[RtPort] Graphics: 67 exports + 6 grid
addresses registered` — because the on-disk md5 says which exe is on disk, not
which one ran (lane GFX2 got a complete, plausible, wrong summary from another
lane's binary).

| Run | Config | Whole run | This lane |
|---|---|---|---|
| `gfx3P2` | `BANISHED_RT_DIFF=1 BANISHED_RT_GFXGRID_PROBE=1 BANISHED_RT_ARRAY_PROBE=1` | 497 sites, **17,058,326 calls**, 1 divergence in 1 site | probe **PASS**, every live site **divergences=0** |
| `gfx3N`  | `… _PROBE=neg _ARRAY_PROBE=neg` | 501 sites, **17,509,389 calls**, 1 divergence in 1 site | **6/6 negative controls flagged** |

**The one divergence is not this lane's.**
`Object::Factory::Startup [probe, not diffable] divergences=1` — another lane's
probe, the same row lane GFX2 reported as pre-existing.

```
Graphics::Scene grid init (0x18005e4a0)                  calls=0   worked=4
Graphics::Scene grid add (0x18005e5e0)                   calls=0   worked=24584
Graphics::Scene grid reindex (0x18005ee30)               calls=0   worked=24623
Graphics::Scene grid thread (0x18005f4b0)                calls=0   worked=24624
Graphics::Scene grid unthread (0x18005f090)              calls=0   worked=11818
Graphics::Scene free globals (0x18005f1f0)               calls=0   worked=1
Collection::Array::SetSize @0x18005fb00                  calls=24496 worked=?

  grid reindex[branch:first threading]      calls=24584
  grid reindex[branch:same cell, early out] calls=87493
  grid reindex[branch:moved cell]           calls=39
  grid thread[branch:cell was empty]        calls=633
  grid thread[branch:appended to ring]      calls=23991
  grid unthread[branch:was cell head]       calls=11561
  grid unthread[branch:cell emptied]        calls=211
```

**`calls=0` on the six bodies means "no self-comparison", not "never ran"** —
these are destructive mutators with no live replay, so the oracle is the probe
and this column is structurally zero, exactly as WAVE9.md records for lane PKG2's
`Read`. `worked=` is the liveness signal, and the `[branch:…]` sites are the
coverage: the branch counters are taken INSIDE the function under test, not from
a proxy observed outside it, which is the rule the effectiveness control was
wrong twice for violating.

Two things the live numbers say that the probe cannot:

* `reindex` is entered **111,905** times a session and does work **39** of them.
  A `calls`-only dashboard would have called this the best-covered site in the
  lane; it is the early-out arm 78% of the time.
* `unthread[branch:cell emptied]` runs **211** times against `was cell head`'s
  **11,561** — i.e. the lone-node arm, the one the circular-chain reading makes
  live, is ~1.8% of real traffic. A live population would have exercised it, but
  the probe is what makes it *comparable*.

---

## 7. Audits, before and after, with a set-diff and an unmoved denominator

"Before" measured on a clean `git archive ba7bc60` into a scratch tree, so the
delta is this lane's and not another lane's.

| Audit | Before (`ba7bc60`) | After |
|---|---|---|
| `tools/addr_door_audit.py` | 284 sites, 201 addresses, **42 UNREGISTERED** (5 of them `graphics/scene.cpp`) | 287 sites, 202 addresses, **37** |
| … set-diff on the unregistered set | — | **CLOSED 5:** `0x18005e4a0`, `0x18005e5e0`, `0x18005ee30`, `0x18005f090`, `0x18005f1f0`. **NEW 0.** |
| `tools/rawdoor_audit.py` | 569 sites, **0 UNGUARDED-CALL**, 46 DEFERRED-CALL | 577 sites, **0**, 46 |
| `tools/port_bypass_audit.py` ADDRESS door | 298 sites, **0 BYPASS**; NO-EXPORT 128, UNPORTED 92, GUARDED-FALLBACK 4, ADDR-REGISTERED 61, DETOURED 3, SYMBOLIC-UNRESOLVED 10 | 308 sites, **0**; NO-EXPORT 138, UNPORTED 93, GUARDED-FALLBACK 3, rest unchanged |
| … NAME door | 469 raw-door sites, **0 BYPASS** on a live path | 469, **0** |
| `tools/unsatisfied_export_scan.py` | 844 names through a door; **130** with neither a port nor a mint entry | 844; **130** — unmoved, correctly: nothing here is an exported NAME |
| `tools/export_name_audit.py` | 666 names, 1 not in the DLL (the known prose ellipsis) | 666, the **same 1** |
| `tools/abi_retslot_audit.py` | 0 suspicious of 48 | 0 of 48 |

The +10 ADDRESS-door sites are fully accounted for: 3 in `scene_grid.cpp`
(`0x18002fea0`, `0x18005fb00`, `0x1800c7ad0`) and 7 `ResolveRuntimeRaw` sites in
the probe, which `ADDR_SITE` matches too. Nine are unexported (NO-EXPORT) and one
is `0x1800c7ad0`, the ported `MemFree` reached by address exactly as
`linkedlist.cpp` already reaches it (UNPORTED, +1).

### The GUARDED-FALLBACK row moved for a reason that is a TOOL FINDING, not a change

`GUARDED-FALLBACK 4 → 3` is **not** a behaviour change. All three of this file's
"guarded fallbacks" were FALSE POSITIVES, and one of them fell out of the set
because **I added nine lines of comment above it**.

`resolve_fallback_audit.find_sites()` pairs a `ResolveAddr` fallback with a
primary `ResolveExport` by looking back **40 RAW LINES** for an assignment to the
same *bare identifier*. Every forwarder in `runtime/src/graphics/scene.cpp` names
its local static `fn`, and `RtTransformAsMatrix` — a different function — assigns
`fn` from `ResolveExport`. So the checker was pairing `RtSceneGridUnthread`'s
`ResolveAddr` with a `ResolveExport` in a function two bodies above it. My
comment pushed that primary out of the 40-line window and the row vanished.

That function's own docstring already carries the fix for the *other* window it
has (`TRAP 1 — PAIR BY ASSIGNMENT TARGET, NOT BY PROXIMITY … the difference
between a checker and a rumour`, and the 7-line guard window that deliberately
skips comment lines). The 40-line lookback is the same trap with neither
correction applied: it counts comment lines, and it matches a bare identifier
across function boundaries.

**Consequence for the record:** WAVE9_LANE_GFX2.md §8 reported
`GUARDED-FALLBACK 1 -> 4` and attributed it to its `if (!fn) fn = …` change.
That attribution is right about the trigger and wrong about the meaning — those
three rows were never fallbacks. Reported here, not fixed: a shared instrument
should be fixed by one owner in one commit, and reaching for the ALLOWLIST is how
a checker gets talked out of a true finding.

**RESOLVED 2026-09-08 in `2794eaf` by the integrator.** The lookback now
breaks on a `}` in column 0 and skips comment-only lines. This lane's diagnosis
held tree-wide and the extent was larger than it could see from one file:
**all 13 of the tool's pairings were cross-function bleed**, not just
`scene.cpp`'s three. The tell was in the output all along and had been read past
for weeks -- one name paired against several DIFFERENT addresses in a single
file (`?Update@UserInterface@UI@@` x3 in `loops.cpp`,
`?SetText@EditElement@UI@@` x3 in `widgets_input.cpp` including `MemFree`'s
address, `?MemFree@` x2 in `renderer.cpp`).

Because the fixed count is **zero**, it was proved rather than argued: a
synthetic control file with a genuine same-function pair, the same shape with a
deliberately mismatched address, and a cross-function split. Fixed lookback
2 found / 1 WRONG-ADDRESS; OLD lookback on the same file 3 found /
2 WRONG-ADDRESS, reproducing this lane's artefact verbatim. Reporting rather
than allowlisting was the right call and is why the count is honest now.

---

## 8. What this lane could NOT verify — stated, not buried

* **`FreeGlobals` has no replay and its oracle is two indirect observations**
  (post-state + allocator reclamation), not a comparison of the walk. N6 shows
  the pair CAN fail, which is the most that can be said. A node freed with the
  wrong size class, or a double free, is invisible to it; only
  `BANISHED_REDZONE=watch` speaks to that.
* **`GridInit`'s `_pad14` and the `+0x14` of every node are never compared**, by
  construction (§4). A port that wrote a defined value there would pass.
* **The float half of `GridReindex` is verified only through the CELL INDEX it
  produces**, which is a lossy transform: `(int)(inv * x)` truncates, so an error
  smaller than one cell of the chosen level is invisible. That is lane PICK's
  own finding about its grid comparison, and it applies here unchanged. The
  mitigation is that the population spans four levels (cell widths 2048, 1024,
  64 and 16 world units), so the same arithmetic is sampled at four
  granularities — but **a sub-cell ULP error would still sail through green**.
  A bitwise sweep of the level/centre arithmetic against the shipped body, on
  its own inputs, is the missing piece and is left for whoever needs it.
* **`GridThread`'s live path is never separately measured**: it is reached only
  from `GridReindex`, so its `worked=24624` is that caller's traffic.

---

## 9. Files

| File | Change |
|---|---|
| `runtime/src/graphics/scene_grid.h` | NEW — the six signatures, the `GlobalNode`/`GlobalList` layout, the probe-scheduling contract |
| `runtime/src/graphics/scene_grid.cpp` | NEW — the six bodies, branch counters, registration |
| `runtime/src/graphics/scene_grid_probe.cpp` | NEW — the lockstep oracle and its six negative controls |
| `runtime/src/graphics/rt_graphics_port.cpp` | `+ BANISHED_RT_GFXGRID`, `RtGraphicsGridRegister()`, probe arming, the lane's identity line |
| `runtime/src/collection/array.cpp` | `+ kSpec_Node18`, `J3_ENTRY3(18005fb00, …)`, registration |
| `runtime/src/collection/array_internal.h` | `+ extern kSpec_Node18` |
| `runtime/src/collection/array_probe.cpp` | `+ 0x18005fb00` replay row |
| `runtime/src/graphics/scene.cpp` | the §7 verdict block updated to say all seven doors are closed and by whom; the circular-chain fact recorded at the unthread forwarder; two stale "stays in the DLL" claims corrected |
