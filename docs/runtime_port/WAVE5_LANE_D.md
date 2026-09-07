# Wave 5, lane D — `Picking::Scene` / `Picking::PickingMesh`

Branch `wave/upgrade-path-complete`, 2026-09-06.

Every claim below is labelled **MEASURED** (read out of the retail binary, its export
directory, a tool's output, or one of the integration runs) or **INFERRED** (read off a
decompile).

**This lane wrote the port without running anything** — five lanes were working Wave 5
concurrently and the orchestrator owns the build machine, the game directory and the logs.
Sections 3–8 were written from static analysis only and are unchanged from that state, so
their hedges are the hedges that were honest at the time.

**The orchestrator then ran it (commit `6b6ec01`), and the ports execute.** Four runs on the
-O0 debug exe with `BANISHED_RT_DIFF=1`, 24M–30M diffed calls each, **no new divergent
site** — every divergence in all four is the pre-existing Wave 1 1-ULP sin/cos family. Live
call counts for this lane, MEASURED:

| Site | Calls |
|---|---|
| `Picking::Scene::SetTransform` | **74,783** |
| `Picking::Scene::Intersect` | **1,182** |
| `Picking::Scene::AddMesh` | **119** |

`Picking::Scene::Scene()`, `AddMesh` and `SetTransform` all appear in the run's site list
(`standalone_error.log` sites 53, 72, 80). See §12 for what that does and does **not**
retire — three of the eight still have no evidence of ever having been called, and
"registered but never executed" is not a port.

---

## 0. The one-line answer to "split or whole?"

**`Picking::Scene` ends up WHOLLY OURS. It is not split.** MEASURED, two ways:

* Our tree calls **nine** of `Picking::Scene`'s 21 exported symbols, and all nine are now
  ported and registered. A grep across `code/`, `engine/`, `standalone/`, `runtime/` and
  `dx11/` finds **no call site for any of the other twelve** — not `Update`, not `Clip`, not
  `GetMesh` / `GetAABB` / `GetLocalAABB` / `GetTransform` / `GetUserValue`, not the copy ctor
  or `operator=`.
* `get_xrefs_to` on `Update`, on `Clip` and on all nine ported addresses returns **only
  `DATA`** references — the export address and name tables, plus `.pdata` unwind records.
  **Runtime-x64.dll never calls any of them internally either.**

So there is no path by which the DLL and this port both mutate one `Scene`. The twelve
unported exports are dormant code in a DLL nothing dispatches into. Every one of the nine is
registered **twice**, under its mangled name *and* under its retail address, which is what
makes that true in practice rather than only on paper — see §2.1.

---

## 1. Result table

All **nine** are ported and registered — the eight assigned plus `RemoveMesh`, which the
brief's `ResolveExport` grep could not see (§2.1). Nothing is held back.

| # | Export | Retail | Ord | Status | Oracle |
|---|---|---|---|---|---|
| 1 | `??0Scene@Picking@@QEAA@XZ` | `0x1801258e0` | 376 | **PORTED** | **FULL** — construct theirs into a scratch `Scene`, compare every field plus the 10,000 bucket heads by content |
| 2 | `??1Scene@Picking@@QEAA@XZ` | `0x180125890` | 672 | **PORTED** | POST-STATE only — a second run would double-free |
| 3 | `?AddMesh@Scene@Picking@@…` | `0x1801252e0` | 1571 | **PORTED** | **FULL on the computed part** — theirs runs into a private scratch `Scene`, the two flattened transform arrays are compared bit-for-bit, then the scratch entry is removed |
| 4 | `?GetUserData@Scene@Picking@@QEBAPEAXI@Z` | `0x1800722d0` | 2998 | **PORTED** | **FULL** — pure, call both |
| 5 | `?Intersect@Scene@Picking@@…` | `0x1801246c0` | 3081 | **PORTED** | **FULL, tolerance 0** — call both, compare the int return and both out-`Vector3`s bit-for-bit |
| 5b | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` | `0x180125290` | 3560 | **PORTED** | POST-STATE only — a replay would release an already-free entry |
| 6 | `?SetMeshUserData@Scene@Picking@@…` | `0x180125240` | 3917 | **PORTED** | **FULL by idempotent replay** — snapshot the 0x48-byte entry after ours, run theirs, `memcmp` |
| 7 | `?SetTransform@Scene@Picking@@…` | `0x1800721b0` | 3993 | **PORTED** | **FULL by idempotent replay** — entry + root transform |
| 8 | `?Vertices@PickingMesh@Picking@@…` | `0x180071e10` | 4536 | **PORTED** | **FULL** — pure, call both |

`tools/runtime_census.py --namespace Picking` after the lane: **`Picking 49 8 8 0 100.0%`**
(total / called / ported / detour / covered). MEASURED.

Files owned and touched by this lane, and no others:

* `runtime/src/picking/scene.cpp` — the eight bodies plus `RtPickingSceneRegister()` (was a
  stub `return 0;`)
* `runtime/types/picking/Scene.h` — **new**; `Scene`, `Entry`, `Cell`, `Link`, `AddDesc`,
  `PickingMesh`, `SubMesh`, `Triangle`, `HitCallback`
* `docs/runtime_port/WAVE5_LANE_D.md` — this file

`runtime/src/picking/rt_picking_port.cpp` already declared `RtPickingSceneRegister()` and was
not edited. The kill switches `BANISHED_RT_PICKING=0` (namespace) and `BANISHED_RT_PICKSCENE=0`
(this lane) were already wired by the orchestrator and work unchanged.

Build: `mingw32-make standalone-debug` is clean, and the file is also clean under a
standalone `-Wall -Wextra -fsyntax-only`. MEASURED.

---

## 2. Export-name verification — MEASURED

`reference_resolveexport_phantom_names.md`: a `ResolveExport` name the DLL does not contain
returns NULL in silence, the `ResolveAddr` fallback hides it, the census counts the port and
the oracle reports `calls=0`. So every name was checked against the export directory of
`C:\GOG Games\Banished\Runtime-x64.dll` with `pefile` **before any body was written**. The
query returned 53 exports containing "Picking"; all eight assigned names are present, and the
addresses and ordinals in §1 are that query's output, not Ghidra's.

Also present and used by this lane: `?RemoveMesh@Scene@Picking@@QEAAXI@Z` at `0x180125290`,
ordinal 3560 (the AddMesh oracle calls it to keep its scratch scene bounded).

**NOT present, and worth stating because it shapes the design: there is no
`??_7Scene@Picking@@6B@`.** The only `??_7` in the namespace is
`??_7PickingMesh@Picking@@6B@` (`0x180158fb0`, ordinal 1478). See §4.

### 2.1 The scope was 9, not 8 — and the census still says 8

The brief's eight exports came from grepping `ResolveExport`. That misses every call site
that resolves by RAW ADDRESS. Lane E's sweep of the 246 `ResolveAddr(Module::Runtime, ...)`
sites found the ninth, and I confirmed each site by hand:

```
code/Game/Component/pickingcomponent.cpp:171   AddMesh     by NAME (the two-line literal)
code/Game/Component/citizenai_executor.cpp:406 AddMesh     0x1801252e0  ADDRESS ONLY
code/Game/Component/floorcomponent.cpp:303     AddMesh     0x1801252e0  ADDRESS ONLY
code/Game/Component/floorcomponent.cpp:308     RemoveMesh  0x180125290  ADDRESS ONLY
code/Game/Component/pickingcomponent.cpp:344   RemoveMesh  0x180125290  ADDRESS ONLY
```

`?RemoveMesh@Scene@Picking@@QEAAXI@Z` (`0x180125290`, ordinal 3560) is now ported. It had to
be: `AddMesh` and `RemoveMesh` are the two halves of one free list — `AddMesh` pops
`Scene::_freeEntry`, `RemoveMesh` pushes onto it — so porting one and leaving the other would
have two implementations taking turns on one object's mesh table. It would not fail at the
seam; it would hand out a colliding or stale mesh id and surface later as a wrong pick or a
`_userData` pointing at a freed entity. `pickingcomponent.cpp:336-342` records that this
class has already produced exactly that crash once (2026-07-21, foundation click, dead in
`EditTool_SelectEntity_Impl`).

**Every one of the nine is registered under BOTH its mangled name and its retail address**,
and that was true of the eight before this correction too — which is the only reason `AddMesh`
recorded 119 live calls, since two of its three call sites are address-only.

#### A THIRD census blind spot — MEASURED

`tools/runtime_census.py` still reports **`Picking 49 8 8 0 100.0%`** after the ninth port
landed, and separately lists `?RemoveMesh@Scene@Picking@@QEAAXI@Z` under *"Ports with no
caller in our tree"*. Both are the same artifact, and it is distinct from the two blind spots
this project already knows about (the named-constant one, and the multi-line literal one lane
E just fixed):

> **A function our tree reaches only by raw address is attributed to the `internal` bucket,
> not to its namespace.** So its namespace row under-counts the real scope, and its *name*
> registration looks caller-less because the census never connects `0x180125290` to
> `?RemoveMesh@Scene@Picking@@…`.

This is not cosmetic — it is the mechanism that made the brief's scope list miss `RemoveMesh`
in the first place, and any future lane whose scope is built the same way will miss the same
class of function. A census that resolved registered addresses back to their export names
(the mapping is in the shipped export directory and the tool already opens it) would close it.

The mirror image also shows: `0x1801246c0` (`Intersect`) appears in the "ports with no
caller" **address** list, because our tree reaches `Intersect` by name and through the exe
dispatch slot but never by `ResolveAddr`. That registration is deliberate belt-and-braces,
not a vestigial resolve.

#### The call-site comment's neighbours, checked — MEASURED

`pickingcomponent.cpp:340-342` claims `0x1801252e0` is the paired `AddMesh` and that
`0x18010c3c0` is "Collision::Scene::RemoveMesh, wrong one". Verified against the export
directory rather than taken on trust:

* `0x1801252e0` **is** `AddMesh`, ordinal 1571. Correct.
* `0x18010c3c0` is `?RemoveMesh@Scene@`**`Collision2D`**`@@QEAAXI@Z`, ordinal **3559** — the
  namespace in the comment is slightly off (`Collision2D`, not `Collision`), and the ordinal
  sits *immediately* before Picking's 3560, which is precisely why the two are easy to
  confuse.
* The comment understates the danger. `0x18010c3c0` is a two-instruction adjustor thunk,
  `ADD RCX,0x60 ; JMP 0x180068620` — `Collision2D::Scene` embeds the same generic grid at
  its offset 0x60 and shares the implementation. Calling it on a `Picking::Scene` would not
  be a no-op: it would run the grid code against `scene + 0x60`, which is **past the end** of
  a 0x60-byte `Picking::Scene`. That is a wild write into whatever follows the object, not a
  wrong answer.

### The brief's claim about `export_name_audit.py` — TRUE WHEN WRITTEN, FIXED MID-WAVE

The lane brief said the two-line `AddMesh` literal at
`code/Game/Component/pickingcomponent.cpp:171` makes `tools/export_name_audit.py` report a
truncated version of the name as a phantom, and asked me to verify that reading rather than
take it on trust. When I checked, it did not: 11 phantoms reported, **none of them a Picking
name**, and both `tools/export_name_audit.py` and `tools/runtime_census.py` carried a
`concat_literal()` helper that joins adjacent literals exactly as the preprocessor does.

I first wrote this section up as "the brief is FALSE". **That was wrong, and the correction
is worth recording because it is a lesson about parallel lanes rather than about the tools:**
the brief was accurate when it was written, and **lane E fixed both tools during this same
wave** (commit `6b6ec01`, `tools/*.py` +78 lines) — it reports that the old behaviour
produced five false phantom reports and cost Wave 3's Object port one correctly-attributed
function (27 → 28). I was reading a tool another lane had repaired an hour earlier and
concluding the brief had never been right. Checking a claim against a moving tree tells you
the state of the tree, not the state of the claim.

The standing consequence is real either way: the advice that a registration name must be a
single literal on one line is now **stale for both tools**. The blind spot that actually cost
Wave 3 six ports is a name passed as a *named constant*, and the census emits an explicit
`*** WARNING: N RegisterRuntimeExport call(s) pass a NON-LITERAL name` for that. This lane
still writes each registration name as its own literal (a constant would be invisible), but
no longer for the line-length reason, and the correction is written into
`runtime/src/picking/scene.cpp` at the point where the old claim would otherwise have been
repeated (PLAN §6 step 8).

The 11 live phantoms are all outside this lane (9 in `engine/funcs/engine_funcs.cpp`, 2 in
`code/Game/Action/action_base_complete.cpp`). That is up from the 6 recorded in
`reference_resolveexport_phantom_names.md`; not this lane's to fix, but the memory note is
out of date.

---

## 3. Layouts, and what pins each offset

Full commentary, with the pinning function named next to every field, is in
`runtime/types/picking/Scene.h`. Every offset there carries a `static_assert`. Summary of
the four structures that matter:

### `Picking::Scene` — 0x60 — MEASURED

A uniform 2D spatial hash. Whole-object witness: the ctor's only callee, `FUN_18007aef0`,
was **disassembled** (`0x18007af06`–`0x18007af55`) and writes exactly +0x00, +0x04, +0x08,
+0x10, +0x18, +0x20, +0x28 = −1, +0x30, +0x38, +0x40 = −1, +0x48, +0x50, +0x58 = −1,
+0x5c = 0. The dtor (`0x180125890`, also disassembled) frees exactly +0x48, +0x30, +0x18,
+0x08 in that order, the last as a tail `JMP`.

| Off | Field | Pinned by |
|---|---|---|
| 0x00 | `_cellSize` | ctor stores `DAT_1801616fc`; the bytes at that address are `00 00 80 41` = **16.0f** (read out of the DLL, not from Ghidra's rendering) |
| 0x04 | `_invCellSize` | ctor stores `1.0f / _cellSize`. Every cell index in the namespace is `(int)(coord * *(float*)(scene+4))`, compiled as `CVTTSS2SI` — **truncation toward zero, not floor** |
| 0x08 | `Array<int> _cellHeads` | `FUN_18007b960` is `SetSize(10000)` filling with −1; the cell iterator takes its modulus against this Array's `_size` at +0x10 |
| 0x18 | `Array<Cell> _cells` | the cell iterator walks it at stride 0x10 |
| 0x28 | `_freeCell` | ctor −1 |
| 0x30 | `Array<Link> _links` | `FUN_180072730` grows it and threads it |
| 0x40 | `_freeLink` | ctor −1; `FUN_180072730` pops and pushes it |
| 0x48 | `Array<Entry> _entries` | every exported accessor indexes `*(scene+0x48)` at stride 0x48 |
| 0x58 | `_freeEntry` | ctor −1; `FUN_18007b210` pops it |
| 0x5c | `_queryStamp` | `INC dword ptr [RCX+0x5c]` at `0x1801247bf` |

**+0x2c and +0x44 are never written, by the ctor or by anything else this lane read.** A
live `Scene` therefore contains two dwords of garbage, and any byte-wise comparison of one
must skip them — the ctor's oracle does, field by field, rather than `memcmp`ing the object.

### `Picking::Scene::Entry` — 0x48 — MEASURED

The single strongest witness is `FUN_18007b210`, the entry allocator, which builds a whole
entry on the stack; every one of the fields below is one store there, and every exported
accessor confirms it independently.

`+0x00 _queryStamp`, `+0x04 _nextFree` (−2 while allocated), `+0x08/+0x0c` cell x/y,
`+0x10/+0x12` cell w/h **as uint16**, `+0x18` `ExternalPtr<PickingMesh>`,
`+0x20 Array<Transform> _transforms`, `+0x30 _userData`, `+0x38 _userValue`,
`+0x3c _userValue2`, `+0x40` callback.

Two cross-checks fell out of this and are worth recording:

* `Intersect` bounds its sub-mesh loop on `*(int*)(entry+0x28)`, which is the transform
  Array's `_size`. That the loop bound and the Array `_size` are the same dword is an
  independent confirmation of `Collection::Array`'s `{_data, _size, _capacity}` layout from
  a class Wave 2 never looked at.
* `Entry+0x18 .. +0x47` is byte-for-byte the descriptor `AddMesh` builds on its stack, which
  is exactly why `FUN_18007b210` can copy it field for field. `AddDesc` in the header is
  that 0x30-byte prefix.

**`Object::ExternalPtr<T>` is a bare pointer in this slot** — `Intersect` dereferences
`*(entry+0x18)` directly to reach `PickingMesh+0x38`, with no handle-table indirection.
MEASURED, from `Intersect`'s own decompile. That is one data point about `ExternalPtr`, not
a general claim about the template; Wave 3 owns that question.

### `Picking::Scene::Cell` (0x10) and `Link` (8) — MEASURED

`Cell = {x, y, nextInBucket, firstLink}`, `Link = {entry, next}`, both entirely pinned by
the cell iterator `FUN_18007b6d0` and by `FUN_180072730`. The hash is
`(y * -0x27e9c7bf + x * -0x72594cbd) % _cellHeads._size`, wrapped up by `+= _size` when
negative.

### `Picking::PickingMesh::SubMesh` — 0x80 — MEASURED

`+0x08 _parent`, `+0x0c Math::Transform _local`, `+0x28 Volume::Box _aabb`,
`+0x40 Array<Vector3> _vertices`, `+0x50 Array<Vector3> _normals`,
`+0x60 Array<Triangle> _triangles`. Stride 0x80 pinned three ways (`GetNode`, the four
per-field accessors, and `AddMesh`'s `lVar14 += 0x80` walk).

`Triangle` is 12 bytes: three uint16 vertex indices at +0x00/+0x02/+0x04 and **six bytes
this lane could not pin** — nothing it read touches them. They are an explicit blob in the
header rather than a guess; the obvious reading (normal or UV indices) is UNVERIFIED.

`PickingMesh` itself is pinned only at `+0x38 _nodes` and `+0x40 _nodeCount`, with the
`Object::External` base left as an opaque 0x38 blob and `sizeof` deliberately unpinned. Two
of this lane's eight functions need those two fields and nothing else; inventing the rest
would put an unverified layout in a header, which is the failure mode Collection.h's banner
is about.

---

## 4. Why a half-ported `Scene` is safe — and why that needed an argument

PLAN §4.1 decision 5 says stateful subsystems switch **atomically as a class**: ctor, vtable
and every method together, because a class ported half-way with two vtables in play is the
swap-remove UAF family again. This lane ports eight of `Picking::Scene`'s methods and leaves
the rest — including `RemoveMesh`, which our tree calls — in the DLL. That is allowed here,
and the reason is specific rather than general:

**`Picking::Scene` has no vtable.** Three independent witnesses, all MEASURED:

1. Both the ctor and the dtor are `QEAA` in the export table — non-virtual. A virtual dtor
   would be `UEAA` (compare `??1CompressedMemoryStream@IO@@UEAA@XZ`, which is).
2. The ctor's first store is `MOVSS dword ptr [RCX],XMM1` — a float at +0x00, not a vptr.
3. `??_7Scene@Picking@@6B@` does not exist in the export directory, while
   `??_7PickingMesh@Picking@@6B@` does.

So there is no ownership to split: a `Scene` is plain data, and a method of ours and a method
of the DLL's operating on the same object agree as long as the layout in §3 is exact. The
layout is the whole of the risk in this lane, which is why every field in the header names
the instruction that pins it.

**No detour is needed, and this is checkable rather than assumed.** `get_xrefs_to` on all
eight retail addresses returns only `DATA` references — the export address table, the
export name/ordinal tables, and the `.pdata` unwind records — with no `CALL` from anywhere
in the DLL. `Picking::Scene::Intersect` in particular has zero callers inside
Runtime-x64.dll. So the registry seam alone redirects 100% of the callers that exist, and
the census `detour` column staying at 0 for this lane is correct, not an omission. (PLAN
§4.2's note that "the first wave that ports something the DLL calls internally will be using
that mechanism for the first time, in anger" is still true — this is not that wave.)

---

## 5. What stays in the DLL, and why

`Picking::Scene` is a spatial hash whose growth, cell threading and frees all go through
`System::MemoryAllocator` **under that allocator's own critical section**. `EnterCriticalSection`
is Win32, and PLAN §4.1 decision 1 keeps everything under `runtime/src/` platform-neutral
outside an explicit `_win` twin. So this lane ports the eight **exported bodies** faithfully
and calls six unexported internals at their retail addresses, exactly as retail's own code
does — the shape Wave 4 lane C used for `Map_Find`/`Map_Insert`.

| Retail | What it is | Called by |
|---|---|---|
| `0x18007b960` | `Array<int>::SetSize(10000)`, filling new slots with −1 | the ctor |
| `0x1800737f0` | `Array<int>::~Array` | the dtor (+0x08) |
| `0x1800739e0` | `Array<Cell>::~Array` | the dtor (+0x18), and AddMesh's scratch free |
| `0x180073bb0` | `Array<Link>::~Array` | the dtor (+0x30) |
| `0x180073c90` | `Array<Entry>::~Array` — also destroys each Entry's inner `Array<Transform>` via `FUN_180074100` | the dtor (+0x48) |
| `0x180059140` | `Array<Transform>::SetSize(n)`, stride 0x1c, new slots = identity | AddMesh |
| `0x18007af70` | the grid insert: allocate an Entry, deep-copy the descriptor, compute the cell rectangle, thread it onto every covered cell; returns the entry index | AddMesh |
| `0x1800724d0` | the grid re-index: recompute the cell rectangle and rethread only if it changed | SetTransform |
| `0x180072950` | unthread an entry from every cell of its rectangle | RemoveMesh |
| `0x18007b390` | release an entry: assign an all-zero `Entry` over it (freeing the inner `Array<Transform>`), then push the slot onto the free list | RemoveMesh |
| `0x18007b6d0` | the cell-rectangle iterator, skipping entries already stamped this query | Intersect |

One deliberate substitution, flagged because it is a deviation and not a transcription:
retail's `AddMesh` frees its scratch transform buffer with a bare
`MemoryAllocator::Free(_singleton, buf)` between an `EnterCriticalSection`/`LeaveCriticalSection`
pair. This port calls `0x1800739e0` instead, which is the same `Free` under the same lock
followed by a clear of the `{data,size,capacity}` triple — and the triple being cleared
belongs to a dying local, so the observable effect is identical. It keeps `<windows.h>` out
of the file. Every `Array<T>::~Array` for a trivially-destructible `T` is byte-identical and
`Math::Transform` is one.

`?RemoveMesh@Scene@Picking@@QEAAXI@Z` (`0x180125290`) was **added to this lane after the
brief** and is ported — see §2.1 for how it was missed and why leaving it behind would have
been the wave's sharpest hazard rather than a bookkeeping gap.

---

## 6. The two findings that changed how this lane was built

### 6.1 `Intersect` is declared `const` and it MUTATES the `Scene` — MEASURED

`?Intersect@Scene@Picking@@QEBAH…` is a `QEBA` (const) member. The first thing it does after
reading its arguments is `INC dword ptr [RCX+0x5c]` (`0x1801247bf`): it allocates a fresh
**query stamp**, and then writes that stamp into `entry->_queryStamp` (+0x00) for every entry
it visits, so an entry that spans several grid cells is tested once per query and not once
per cell. The port takes a `const Scene*` and casts it away; the constness is retail's, and
it is a lie.

This is the whole oracle design for the function. The brief's default assumption — "read-only,
so full RT_DIFF oracle" — happens to survive, but **only because of what the mutation is**,
and the argument has to be made rather than assumed:

* Every call allocates a *new* stamp before using it, so the second run of a back-to-back
  pair sees a state indistinguishable from a virgin one. Entries carrying the first run's
  stamp are not "already visited" as far as the second run is concerned.
* The only lasting difference between one call and two is that `_queryStamp` advanced by two
  instead of one. It is a monotonic `int` and nothing reads it except the next query.

So the port runs both implementations and demands bit equality on the return value and both
out-`Vector3`s, with **tolerance 0** — this is a transliteration of retail's own float
expressions in retail's association order, and anything short of bit-exact means a term
moved. No tolerance is passed anywhere in this lane.

One trap the oracle has to dodge: retail writes `outPos`/`outNormal` **only on a hit**. Handing
the original a zeroed scratch would make every miss report a false divergence. The port
snapshots the caller's two buffers *before* ours runs and gives the original that snapshot,
so a miss compares equal because neither implementation touched anything.

### 6.2 The grid is over **XY**, not XZ — MEASURED

`Intersect` builds its query rectangle from `origin[0]`/`origin[1]` and `dir[0]`/`dir[1]`,
and `FUN_180125bc0` projects a `Volume::Box` by taking `min[0], min[1], max[0], max[1]`
(Box+0x00, +0x04, +0x0c, +0x10). Components 0 and 1, i.e. X and Y. Anyone reading this code
expecting a ground-plane XZ hash will mis-derive the cell arithmetic. Consistent with
`mapquery.cpp`, whose ray is `{inX, inZ, kRayStartZ}` with direction `{0,0,kRayDirZ}` — the
world's horizontal pair goes into the first two lanes and the vertical into the third.

---

## 7. Oracle shapes, stated honestly

Three of the eight are pure and get an unqualified full oracle: `GetUserData`, `Vertices`,
`Intersect`. The other five mutate, and PLAN's guidance is shadow mode or a written reason
for holding back. Rather than hold any back, this lane used three constructions; each is
described here with what it does **not** prove.

**(a) Scratch-object oracle — the ctor.** The ctor mutates only the object it is handed, so
the original can be run for real into a stack `Scene` and the two compared. The two bucket
buffers are different heap allocations, so the comparison is every field except the Array
`_data` pointers, plus the 10,000 bucket heads by content. Theirs is then destroyed with the
DLL's own dtor so the scratch does not leak 40 KB per construction. This is a genuine full
oracle. It does not prove the *allocation* is right, only that the resulting state is.

**(b) Scratch-scene oracle — `AddMesh`.** The insert mutates the live scene, so theirs cannot
be run on it. Instead theirs runs into a private scratch `Scene` — constructed once, lazily,
by the **DLL's** ctor, so the oracle never depends on the port under test — and the two
entries' flattened transform arrays plus the five scalar fields are compared bit-for-bit,
after which the scratch entry is removed again with `RemoveMesh` so the scratch stays
bounded. This covers the only thing `AddMesh` actually computes: the sub-mesh hierarchy
flatten (`world[i] = world[parent(i)] * local[i]`, with the quaternion product and an inlined
quaternion-rotate of the offset). It does **not** cover the grid insert, which is the DLL's
`FUN_18007af70` in both implementations. Cost under `BANISHED_RT_DIFF`: one extra
insert+remove per `AddMesh` call, and one ~40 KB scratch `Scene` leaked for the life of the
process.

**(c) Idempotent replay — `SetTransform`, `SetMeshUserData`.** Both bodies are pure stores
whose values depend on nothing but their arguments, so running the original immediately
afterwards with the same arguments **must** be a no-op. The port snapshots the whole
0x48-byte entry (plus, for `SetTransform`, the 0x1c-byte root transform) after ours, runs
theirs, and `memcmp`s. If ours wrote the right values to the wrong offsets, theirs writes
them to the right ones and the bytes differ. For `SetTransform` this is doubly informative:
theirs re-runs the grid re-index, which is a no-op only if ours left the entry's cell
rectangle where retail would have.

What (c) cannot catch: ours writing to the wrong **entry** — a wrong 0x48 stride would send
both implementations to the same wrong place. `GetUserData`'s independent full oracle and the
`static_assert`s cover that, but it is a gap in the replay construction itself and is stated
rather than papered over.

**(d) Post-state only — the dtor.** A second run would double-free. What is checked is what
retail's body must leave behind: all four Arrays cleared. That catches a wrong offset or a
missed call; it does **not** prove the frees themselves were correct. Only the allocator
watchdog (`BANISHED_REDZONE=watch`) can say that, and it has not been run.

---

## 8. What this lane did NOT verify

* **No run of any kind.** Zero divergences observed, because zero calls have been diffed.
  Every oracle above is code that has compiled and never executed.
* **Most of `Intersect`'s float association order is from the Ghidra decompile, not from
  instructions.** Four regions were checked against the disassembly and agree exactly: the
  ray-extent clamps (`COMISS`+branch, so ordinary `?:` semantics), the face-normal cross
  product and its length sum, the `tHit` numerator's `((a*b + c*d) + e*f) * (1/det)`
  grouping, and `AddMesh`'s translation compose. The remaining ~40 float expressions were
  transcribed from the decompile. The tolerance-0 oracle exists precisely to settle them,
  and until it runs this is the largest unproven thing in the lane.
* **The six unpinned bytes of `Triangle`**, `SubMesh+0x00`, `SubMesh+0x70`, and
  `PickingMesh`'s `Object::External` base. Marked UNVERIFIED in the header.
* **`Math::Quaternion::operator*` and `Math::Transform::Inverse` are duplicated, not
  reused.** Retail calls both; this port transliterates both into file-local helpers. The
  reason is attribution: routing this lane's arithmetic through Wave 1's registration would
  mean a divergence here could not be told apart from a Wave 1 defect, and
  `Quaternion::operator*` is this project's recorded example of a wrong port that nothing
  called (`reference_uncalled_port_is_not_verified.md`). Both helpers were re-derived from
  the retail decompile and *then* checked against `runtime/src/math/transform.cpp`; they
  agree term for term, which is an independent confirmation of Wave 1's fix rather than a
  dependency on it. The duplication is a deliberate cost and should be collapsed at cutover.
* **Whether `AddMesh`'s scratch-scene oracle is safe at volume.** It is bounded by design
  (insert then remove), but `PickingComponent_RegisterMesh` is default-on, so a stress town
  will drive it thousands of times. Untested.
* `abi_retslot_audit.py`: **0 suspicious of 49 by-value-returning exports.** MEASURED, but
  note what that tool covers — none of this lane's nine returns a class by value.
  `Vertices` returns a *reference* (`AEBV…`), so it has **no** hidden return slot and the
  whole body is one `LEA` into RAX. Getting that backwards would be a wild write on every
  call, with RDX — the caller's buffer — read as the sub-mesh index. It is written the right
  way round; the audit is not what proves it, the mangled name and the disassembly are.

---

## 9. What would exercise these ports, and what a PASS looks like

The orchestrator owns the runs. Call volumes below are INFERRED from the call sites, not
measured.

**Run 1 — `make sa-run` with `BANISHED_RT_DIFF=1`.** Exercises, in order of confidence:

* **ctor** — `gameupdateloop_load.cpp:391` constructs the map's `Picking::Scene` on every new
  game. Expect `calls=1` per game. `BANISHED_AUTOSTART_GAMES=2` doubles it and also exercises
  the **dtor** (`gameupdateloop.cpp:650`, `keystone_a_gamedtor.cpp:97`) on the quit-to-menu.
* **AddMesh** — `PickingComponent_RegisterMesh` (`pickingcomponent.cpp:171`) is **default-on**
  (`BANISHED_LIVE_PICKMESH` is a kill switch, not an opt-in), so every placed building
  registers a pick mesh. Expect hundreds of calls on any autostart. `citizenai_executor.cpp`
  also calls it per citizen but is **opt-in** behind `BANISHED_LIVE_PICKING`; adding that env
  var multiplies the volume by the population and is the cheapest way to stress this body.
* **Intersect** — the largest and riskiest port, and the one a plain autostart may **not**
  reach. `mapquery.cpp:239` calls it only when the terrain cell under a citizen has
  `floorCount > 0`, i.e. only where a dock or bridge deck exists (`BANISHED_LIVE_FLOOR`,
  default-on). `edittool.cpp:282` calls it on a mouse pick, and PathTool reads the same
  dispatch slot. **If Intersect reports `calls=0`, that is not a pass** — read the
  `calls=` column, not just `divergences=` (`reference_uncalled_port_is_not_verified.md`).
* **GetUserData** — `edittool.cpp:285`, on entity clicks. Same caveat: unattended runs do not
  click, so expect 0 unless a harness drives a pick.
* **SetTransform** — exe dispatch slot `0x1401529a8`.
* **SetMeshUserData** — exe dispatch slot `0x1401529a0`; the two exe callers are
  `BridgeTool::CalcPath` and `FencedTool_helper_90d0`, so it needs a bridge or a fenced area
  to be placed.
* **Vertices** — exe dispatch slot `0x140151478`; `TerraformComponent`'s build-complete mesh
  pass, so it needs a tunnel to finish.

**Run 2 — `make sa-stress` with `BANISHED_RT_DIFF=1`.** The stress town places three sets of
every placeable building, which is the only harness that reliably reaches **all** of
Intersect, SetMeshUserData and Vertices in one go: docks give the floor refine real work,
bridges drive `SetMeshUserData`, and the building count drives `AddMesh` hard. This is the
run this lane most wants.

**Run 3 — the free A/B.** `BANISHED_RT_PICKSCENE=0` on the same scenario. Everything must
behave identically; any behavioural difference between the two runs is this lane's defect
regardless of what the diff counters say.

**PASS** = for each of the eight sites, `divergences=0` **and** `calls>0`, plus a clean exit
and no new `standalone_crash.log`. A site with `calls=0` is an unproven port, not a passing
one, and should be reported as such rather than counted.

Things to watch for that would be this lane's fault:

* `EXEC-of-0` in `RtGridAdd` / `RtCellIterAdvance` / one of the four array frees — a
  `ResolveAddr(Module::Runtime, …)` that came back null. All nine internal addresses are
  hard-coded in `scene.cpp`.
* A fault reading `0x3F800000` — the signature of a float landing where a pointer belonged
  (`reference_hidden_retslot_wild_write.md`). If it appears, `Vertices` is the first suspect.
* A divergence on `Intersect` with a tiny numeric delta is a float-association bug in the
  transliteration, not a logic bug; the site's log prints both hit positions to 9 digits.

---

## 10. Comments to correct (PLAN §6 step 8)

None of these are in files this lane owns, so none were edited. Reported for the orchestrator.

| Where | What it says | What is true |
|---|---|---|
| `runtime/src/picking/README.md` (whole file) | "## Original files (1 cpp, 1 h)", listing only `PickingMesh.h` and `PickingMesh_compile.cpp`; and "## Ported here — _nothing yet_" | Retail ships **49** Picking functions across **two** classes, and `Picking::Scene` is about two thirds of them. `docs/runtime_port/original_source_tree.txt` is string-anchored, so it names only files that left a path string in the profile DLL — absence is not evidence a file did not exist. The heading should read "recovered", and the "nothing yet" line is now stale. The same caveat applies to PLAN §3.4's "207 files, 116 .cpp" for every directory, not just this one. |
| `code/Game/Component/floorcomponent.cpp:299–303` | An `⚠ OPEN` flag: "the call below passes 0 for the OWNER argument, so a floor's pick mesh has no owner entity. That is a separate, untested question." | This lane can now say what that argument *becomes*: `AddMesh`'s 4th argument is stored verbatim at `Entry+0x30` and is exactly what `Scene::GetUserData` returns. So a dock or bridge deck registered with owner `0` is pickable by `Intersect` (it returns the entry id) but `GetUserData` on that id returns **null**, and `EditTool` — which pairs the two (`edittool.cpp:282` and `:285`) — selects nothing when the player clicks a deck. That is a concrete, falsifiable prediction, not a proof; nobody has clicked a deck to check. The OPEN should be re-worded around it. |
| `docs/runtime_port/PLAN.md` §4.2, wave 5 row | "Collision2D, Picking, Pathfinding, Achievement, Save — M each, self-contained, each ~13–18 KB, only System/Object below them" | For Picking specifically the row is right about size and wrong about the dependency: `Picking` depends on **Math** (Quaternion, Transform) and **Volume** (Box) as well, which PLAN §3.2 already says ("Picking adds Math/Volume") and the §4.2 row does not. Harmless here because Waves 1 and 2 are done, but the two rows disagree. |
| `reference_resolveexport_phantom_names.md` (memory) | "6 found" | `tools/export_name_audit.py` reports **11** today, none of them in this lane. |
| Standing advice repeated in several lane briefs | "spell every `RegisterRuntimeExport` name as a SINGLE string literal on ONE line: the census cannot see through a named constant OR a multi-line literal" | The multi-line half is **stale as of this wave**. Lane E added `concat_literal()` to both `tools/runtime_census.py` (line 53) and `tools/export_name_audit.py` (line 41). Only the named-constant half still holds, and the census now warns on it explicitly. See §2. |

## 11. Claims from the brief, checked

| Claim | Verdict |
|---|---|
| The `AddMesh` mangled name reassembled from `pickingcomponent.cpp:171` is the real export | **TRUE.** Present at `0x1801252e0`, ordinal 1571, character for character. |
| `tools/export_name_audit.py` reports a truncated version of it as a phantom | **TRUE WHEN THE BRIEF WAS WRITTEN, FALSE NOW.** Lane E fixed both census tools mid-wave. See §2 — including the fact that I got this verdict wrong on the first pass by checking a claim against a tree another lane was repairing. |
| `edittool.cpp` and `mapquery.cpp` are live consumers of `Intersect` and `GetUserData`, "hit on essentially every click" | **TRUE for `edittool.cpp`** (`:282`, `:285`). **Overstated for `mapquery.cpp`**: its `Intersect` call is behind a terrain-cell floor counter (`cellRec+0x2d > 0`), so it fires only over a dock or bridge deck, not on every click and not at all in a town without one. |
| `PickingComponent::OnAdded` (exe `0x140128f50`) was a real gap closed in the exe closeout | **CONSISTENT** with PLAN §1.1 and `citizenai_executor.cpp:413`. Read, not re-derived — this lane did not open the exe. |
| The dock/bridge deck walk height depended on a Picking refine at exe `…f3e0` | **CONSISTENT.** `address_registry.cpp` names `CharacterComponent_impl_f3e0 (0x14009f534)` as a reader of dispatch slot `0x140152428` (`Intersect`), and `mapquery.cpp` is that refine. Read in-tree, not re-derived from the exe. |
| Picking is "only 49 functions and ~14 KB, so the internals `Scene` calls are a bounded, readable set" | **TRUE, with a caveat.** The set was bounded (nine internals, §5) and readable. But `Intersect` alone is **2,936 bytes** of the namespace's 14,039 — a fifth of it in one function — so "14 KB" understates the shape of the work. |
| The `Set` node-offset trap (`reference_one_bst_find_two_node_layouts.md`) is a live risk here | **NOT APPLICABLE.** `Picking::Scene` uses no `Collection::Set`. Its containers are four `Array`s plus hand-rolled free lists and an open-chained hash. |
| `SetMeshUserData` / `AddMesh` have more than five arguments, so the "pointer typed `int`" trap is live | **TRUE and load-bearing.** `SetMeshUserData` takes five arguments after `this` and `AddMesh` six, so in both the callback is the last register/stack argument. The header types it (`HitCallback`) rather than leaving it `void*` so a signature cannot lose it silently. Retail never *invokes* the callback in any of the eight bodies — it only stores and returns it. |

---

## 12. What the integration runs settled, and what they did not

MEASURED, from commit `6b6ec01`: four runs on the -O0 debug exe (autostart,
`AUTOLOAD=Save9`, `SAVE_TEST=700`, and a save/load round-trip), each with
`BANISHED_RT_DIFF=1` and a pause-menu self-quit so `RtDiffSummary()` prints. 24M–30M diffed
calls per run across 91–111 sites. **No new divergent site in any of the four**; every
divergence is the pre-existing Wave 1 1-ULP sin/cos family
(`Transform::FromYawPitchOffset`, `Vector3::FromYawPitch`). Every run exited cleanly with an
empty `standalone_crash.log`.

### Retired

* **`SetTransform` — 74,783 calls, 0 divergences.** The idempotent-replay oracle (§7c) ran
  seventy-five thousand times: our seven stores plus the DLL's grid re-index, then the
  original replayed on top and the whole entry `memcmp`'d. The `Entry` stride, the `+0x20`
  transform pointer and the cell fields are settled.
* **`Intersect` — 1,182 calls, 0 divergences, tolerance 0.** This is the result the lane
  most wanted and least expected to get for free. §8 called the float association order "the
  largest unproven thing in the lane" because only four of ~40 expressions had been checked
  against instructions rather than against the decompile. Bit-exact agreement over 1,182 ray
  casts settles them, and settles the `const`-that-mutates double-run argument in §6.1 with
  it: had the query stamp not been self-initialising, the second run would have skipped
  entries and the returns would have diverged immediately.
* **`AddMesh` — 119 calls, 0 divergences.** The scratch-scene oracle (§7b) works, stayed
  bounded, and the sub-mesh hierarchy flatten — the quaternion product and the inlined
  quaternion-rotate — is bit-exact against the DLL. It also means the file-local copies of
  `Quaternion::operator*` and `Transform::Inverse` (§8) agree with retail, which was assumed
  and is now measured.
* **The `Scene` ctor ran** — it is site 53 in the run's site list, and the world builds, so
  the layout and the 10,000-bucket initialisation are exercised. The autostart run's
  `[GUL::Load] resolves: … Picking=00007ff6965e9a70` shows the resolve landing in the exe's
  own image (`0x7ff69…`), i.e. on our port, not in the loaded DLL.
* **No detour was needed.** §4 predicted this from `get_xrefs_to` returning only `DATA`
  references; four runs with the registry seam alone and no behavioural difference is the
  confirmation.

### NOT retired — three of the eight still have no evidence of a single call

`GetUserData`, `SetMeshUserData` and `Vertices` do not appear in the reported call counts.
That is consistent with what §9 predicted: `GetUserData` needs a mouse pick (`edittool.cpp`)
and none of the four runs clicks anything; `SetMeshUserData` needs a bridge or fenced area
placed; `Vertices` needs a tunnel to finish. The dtor likewise only runs on quit-to-menu.

**These are registered, not proven** (`reference_uncalled_port_is_not_verified.md`: read the
`calls=` column, not just `divergences=`; a wrong `Quaternion::operator*` survived all of
Wave 1 exactly this way). All four are small — one load, four stores, one `LEA`, four calls —
so the risk is low, but low is not zero and `Vertices` is the one function in the lane where
a mistake would be a wild write rather than a wrong answer (§8, last bullet).

What would close them, cheapest first:

1. `BANISHED_PAUSEMENU_*` / `BANISHED_AUTOMENU_*` already drive real button clicks; a harness
   click on a placed building would exercise `Intersect` **and** `GetUserData` together,
   which is the pair `EditTool` uses.
2. `make sa-stress` places three sets of every placeable building, including bridges and
   tunnels — the one harness that should reach `SetMeshUserData` and `Vertices` without new
   code. **`sa-stress` has not been run with `BANISHED_RT_DIFF=1` for this lane.**
3. `BANISHED_AUTOSTART_GAMES=2` exercises the dtor on the quit-to-menu.

### Still open, and not this lane's to close

* **`RemoveMesh` has never been diffed.** It was added after the four integration runs, so
  its post-state oracle has compiled and not executed — and both of its call sites are
  address-only, so nothing incidental would have reached it. It runs whenever a building or
  a floor is removed; one demolition in any run closes it.
* **The census still reports `Picking 8/8`** and lists `RemoveMesh` as a port with no caller.
  That is the tool, not the port — see §2.1. Do not read the 8 as the scope.
* **Whether `Picking::Scene` is split: it is NOT.** See §0. That question is answered rather
  than open.
