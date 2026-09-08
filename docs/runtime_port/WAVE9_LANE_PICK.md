# WAVE 9 — LANE PICK: the `Picking::Scene` spatial-hash internals

**Status: CLOSED.** 5 of 5 census rows ported, plus their full 14-body
transitive closure. 19 addresses registered, 0 closed with a justification,
0 held back.

**Oracle: synthetic-subject replay against the shipped DLL.** 0 mismatches over
a 17-step lockstep script and a 57-case bitwise float sweep, with every
effectiveness counter non-zero and both negative controls firing.

---

## 1. Where this lane came from, and why no other instrument could see it

Not from a name-based instrument. From the **address-door census**
(`docs/runtime_port/wave9_unregistered_addr_doors.csv`, commits `ce1d8e9` /
`4ef55b6`): every `ResolveAddr(Module::Runtime, 0x…)` in the tree minus every
address anything registers. 172 distinct addresses, 62 unregistered — and five
of the 62 were the Picking grid, all forwarders in
`runtime/src/picking/scene.cpp`:

| Address | Site | What it is |
|---|---|---|
| `0x18007af70` | `scene.cpp:286` | `RtGridAdd` — adds an entry, returns its index |
| `0x1800724d0` | `scene.cpp:295` | `RtGridReindex` — re-threads an entry if its cell rect moved |
| `0x180072950` | `scene.cpp:305` | `RtGridUnthread` — unlinks an entry from each of its cells |
| `0x18007b390` | `scene.cpp:315` | `RtGridFreeEntry` — destroys the entry, pushes the slot |
| `0x18007b6d0` | `scene.cpp:336` | `RtCellIterAdvance` — the cell iterator |

With `Runtime-x64.dll` absent all five resolve to null and any call executes
address 0 — the exact failure shape of the cutover wall. **No name-based
instrument can see them, because the call sites never mention a name.** That is
why Wave 5 lane D could close the nine exported `Picking::Scene` bodies above
them and leave these open, and why they survived eight waves of census work.

Wave 5's stated reason for leaving them was correct at the time: they allocate
through `System::MemoryAllocator` under that allocator's own critical section,
and PLAN §4.1 decision 1 keeps `runtime/src/` free of `<windows.h>`. That
premise is gone. Wave 9 lane Q's `rtcoll::MemAlloc` / `rtcoll::MemFree` **are**
the `EnterCriticalSection / Allocate / LeaveCriticalSection` triples retail
inlines, reached through the port-aware address door, and lane Q disassembled
`0x1800c7ad0` to prove it instruction for instruction. The platform boundary
that blocked this cluster is now a function call.

---

## 2. Verdict per address

### The five census rows — ALL PORTED

| Address | Ported as | Verified by |
|---|---|---|
| `0x18007af70` | `rtpickgrid::GridAdd` | lockstep script, 10 adds |
| `0x1800724d0` | `rtpickgrid::GridReindex` | 3 moving + 1 early-out reindexes |
| `0x180072950` | `rtpickgrid::GridUnthread` | 3 removals, multi-cell rects |
| `0x18007b390` | `rtpickgrid::GridFreeEntry` | 3 removals + free-list reuse |
| `0x18007b6d0` | `rtpickgrid::CellIterAdvance` | 43 lockstep iterator steps, 40 hits |

### The fourteen-body closure — ALL PORTED

`0x18007b210` `EntryAlloc` · `0x180072730` `AddToCells` · `0x180072b30`
`FindOrCreateCell` · `0x180072d10` `UnlinkEntryFromCell` · `0x180072e70`
`FreeEmptyCell` · `0x1800738c0` `CellArraySetSize` · `0x18007bc30`
`EntryArraySetSize` · `0x1800735c0` `EntryCopyConstruct` · `0x18007b590`
`EntryAssign` · `0x180074100` `DestroyEntryTransforms` · `0x18001b360`
`TransformArrayAssign` · `0x18001c100` `ArrayDtorTransform` · `0x180125bc0`
`MeshBoundsXY` · `0x18001f470` `BoxTransform`

All in `runtime/src/picking/scene_grid.cpp`.

### Why the closure and not just the five

**Porting the five census rows alone would have traded 5 unregistered address
doors for 14 new ones.** The five call fourteen more unexported internals and
none of those fourteen was ported anywhere in the tree, so a five-body lane
would have produced a census that gets *worse* while every line of it reads like
progress. That is the trap already recorded on this project — *a faithful port
that calls DLL internals by address GROWS the cutover surface* — and closing the
closure rather than the visible rows is the response to it.

Stated both ways, as the integrator asked:

|  | doors before | doors after |
|---|---|---|
| **Five-body lane (rejected)** | 5 unregistered | 14 unregistered (net **+9**) |
| **Nineteen-body lane (shipped)** | 5 unregistered | 0 unregistered (net **−5**, and **−6** counting the shared graphics row) |

### The call graph, in full

```
GridAdd 0x18007af70
  -> MeshBoundsXY 0x180125bc0 -> BoxTransform 0x18001f470 -> AsTransform3 (lane P's port)
  -> EntryAlloc 0x18007b210
       -> TransformArrayAssign 0x18001b360 -> ArrayDtorTransform 0x18001c100
       -> EntryArraySetSize 0x18007bc30
            -> EntryCopyConstruct 0x1800735c0 -> TransformArrayAssign
            -> DestroyEntryTransforms 0x180074100
       -> EntryAssign 0x18007b590 -> TransformArrayAssign
  -> AddToCells 0x180072730
       -> FindOrCreateCell 0x180072b30 -> CellArraySetSize 0x1800738c0
GridReindex 0x1800724d0 -> MeshBoundsXY, GridUnthread, AddToCells
GridUnthread 0x180072950 -> UnlinkEntryFromCell 0x180072d10 -> FreeEmptyCell 0x180072e70
GridFreeEntry 0x18007b390 -> EntryAssign, ArrayDtorTransform
CellIterAdvance 0x18007b6d0 -> (leaf, no callees)
```

`Math::Transform::AsTransform3` (`0x18001fcd0`) is the single external
dependency. It is Wave 9 lane P's port, registered under **both** doors, so
reaching it by address here reaches the port — not the DLL.

---

## 3. Both doors? No — there is only one, and that is checked not assumed

The normal rule is that a port reached by address needs `RegisterRuntimeAddr`
**as well as** `RegisterRuntimeExport`. **It does not apply to any of these
nineteen: not one of them is exported.** No symbol in the retail export
directory (~4,800 ordinals) or in `Runtime-x64-profile.dll`'s 5,182 names
resolves to any of these addresses, and `docs/runtime_port/wave9_internals.csv`
records the same for the five census rows (`profile_build_name: none`). The
address registration is therefore the **only** door and `RegisterRuntimeExport`
has nothing to register under. Said explicitly rather than left implied.

**Nor is this the allocator lane's situation.** `RegisterAllocatorAddresses()`
(`runtime/src/system/allocator.cpp`, `309c015`) is cutover-only because
`standalone/allocator_lock.cpp` *patches* the same addresses in the loaded DLL,
and `ResolveAddr` consults the port registry FIRST — registering early would
have handed the patcher our own prologue to rewrite. **Nothing patches any of
these nineteen.** Grepped across `standalone/`, `code/`, `engine/` and `dx11/`:
the only textual hits for these addresses are `scene.cpp`'s five forwarders and
`scene_grid.cpp` itself. So they register unconditionally at init, like every
other wave, behind one kill switch (`BANISHED_RT_PICKGRID=0`).

**`scene.cpp` was not edited except for comments.** `ResolveAddr` consults the
port registry first, so the five forwarders reach the new bodies with no code
change, and the addresses drop out of the census because they are now
registered. Leaving the forwarders alone preserves their analysis, which is
where most of this lane's groundwork came from.

---

## 4. The shared row: `0x18001f470`

`0x18001f470` (`Volume::Box` transform) was `runtime/src/graphics/scene.cpp:254`'s
unregistered door and `wave9_internals.csv` assigns it to the **graphics** lane.
This lane needs it — `MeshBoundsXY` calls it once per mesh node — so it was
flagged to the integrator before landing rather than registered quietly.

**Ruling (integrator, 2026-09-08): lane PICK owns the body and the
registration.** Lane GFX2 confirms it did not write a body and is not
registering it; it only changed that call site's cached resolve to
cache-a-hit / retry-a-miss, so the site picks up this port instead of freezing a
null. **There is exactly one registration.**

`graphics/scene.cpp`'s comment said it was *"left in the DLL because it is a
float-exact routine whose rounding this lane has no independent way to verify."*
That verification now exists — §6 below — and the comment is corrected in the
same commit.

---

## 5. What the disassembly said that the decompiler did not

Every body was transliterated from `disassemble_function`, never from the
decompiled C (`reference_ghidra_c_output_vs_disassembly.md`: three Wave 6
defects including a crash). The differences that mattered:

* **`0x180072d10` (the link unlink).** Ghidra's C rotates three loop variables
  through each other and is unreadable. The disassembly is a plain
  singly-linked-list unlink whose loop tests `_next >= 0` **before** the entry
  match — so the last link in a chain is unlinked whether or not it matches.
  That is not a bug to fix: `entryId` is always present when this is called, so
  the not-found path is unreachable, and a "helpful" guard would change the
  shape and prove nothing.

* **`0x18007b390` (`GridFreeEntry`) does not fully initialise its temporary.**
  It writes eleven fields of its stack `Entry` and leaves **two windows
  untouched**: `+0x14..+0x17` (alignment slack) and **`+0x3c`
  (`_userValue2`)**. The assignment then copies both into the live entry, so
  after any `RemoveMesh` the entry's `_userValue2` holds a dword of the DLL's
  own stack frame. This is the same finding Wave 5 lane D's RemoveMesh shadow
  surfaced, and it is **reproduced, not repaired** — an oracle comparing those
  bytes fails 100% of calls
  (`reference_oracle_must_not_compare_unwritten_bytes.md`), and a port that
  zeroed them would differ from retail in a way nothing here would ever report.

* **`0x1800724d0` / `0x18007af70` use 16-BIT rectangle extents.** `SUB SI,…` on
  the low words, not a 32-bit subtract, so a rectangle wider than 65535 cells
  wraps. Reproduced as a `uint16_t` subtraction.

* **The cell index is `CVTTSS2SI` — TRUNCATION toward zero, not floor.** For a
  negative coordinate the cell boundary at 0 is shared by `[-1,0]` and `[0,1]`,
  which makes the grid slightly wrong near the origin. Retail's arithmetic; the
  point of a byte-faithful port is that our grid is wrong in the same places.
  The probe's population deliberately includes negative coordinates, because
  that is exactly where a floor-vs-truncate slip would show.

* **`0x180125bc0` uses `COMISS` + `JA`, not `MINSS`/`MAXSS`.** The two differ on
  NaN: `COMISS cur,v / JA skip` assigns whenever `cur > v` is false, which
  includes unordered. Written as `if (!(cur > v))`, so a NaN box behaves the way
  retail's does rather than the way the obvious `if (v > cur)` would.

* **`0x180125bc0` computes a Z min/max and discards it.** Kept. Deleting a
  computed-then-unused value is how a side-effect carrier gets removed as dead
  code (lane FIND's `FindFiles` finding); here it is provably inert, and it is
  cheaper to keep it than to argue.

* **Re-reads are preserved.** `GridUnthread` and `AddToCells` re-read `xy[0]` at
  the top of every row; `UnlinkEntryFromCell` re-reads `_links._data` three
  times; `EntryAlloc` re-reads `_entries._data` after the transform copy.
  Nothing can move those pointers, but "nothing can move it" is a claim about
  code this lane did not port, so the re-reads stand.

---

## 6. The oracle

### Why a probe and not a live diff

Every function here **mutates shared state** and four of the five destroy
something. Running ours then the shipped body at a live call site frees the same
`Entry` twice, pushes the same slot on the free list twice, and starts the
second run from the state the first one left — not the same experiment, and the
first thing it produces is a corrupted allocator.

So: **synthetic subjects.** Two private `Picking::Scene`s with byte-identical
starting state, the same script applied to both, ours through the ported bodies
and theirs through the shipped ones, compared field by field.
(`reference_synthetic_subject_beats_a_fake.md`.)

The shipped bodies are reached through **`ResolveRuntimeRaw`, never
`ResolveAddr`** — `ResolveAddr` consults the port registry first, and this lane
registers all nineteen addresses, so `ResolveAddr` would have handed back our
own function and the probe would have compared ours to ours and always agreed.

The `PickingMesh` is a **synthetic subject, not a fake of the class**: every
body in this lane reads exactly two things through the mesh pointer — `_nodes`
at `+0x38` and each `SubMesh`'s AABB at `+0x28` (stride `0x80`). Supplying those
is enough; no factory, vtable or type id is involved. *Before building a fake
for an unreachable dependency, ask what the guard that reaches it READS.*

### The population is the point

A ported inline `strcmp` survived **four waves** of green oracles because 13 of
the 14 values in the population were the same value
(`reference_uniform_population_is_not_a_verification.md`). An all-single-cell,
empty-free-list run of this probe would pass while proving nothing, so the
script deliberately builds:

* entries whose rectangle spans **many** cells (1×1 up to ~5×4) — a 1×1
  population never enters `GridUnthread`'s row loop at all;
* cells **shared** by several entries, so a cell's link list has length > 1 and
  the unlink has a middle node to remove, not only a head;
* **non-empty free lists before every reuse arm** — three entries are removed
  mid-script, in a deliberately non-LIFO order (`{1, 4, 2}`), so `EntryAlloc`,
  `AddToCells` and `FindOrCreateCell` all take their reuse branches;
* reindexes that **both move and do not move** the rectangle, because
  `GridReindex`'s early-out means `calls` is not coverage (Wave 9 lane M's
  Finding 1: `calls=12896` on a site where ten calls did any work);
* **off-axis and non-normalised rotations.** With identity rotations the
  absolute-valued basis is the identity too, every `fabs` returns its argument,
  and each extent sum has two zero terms — a sign error, a transposed index and
  a dropped `fabs` would *all* compare equal.

### Lockstep, not compare-at-the-end

The first version ran each subject to completion and compared once. That is a
weaker experiment in two ways: a defect that **cancels out** (push a link on the
free list in the wrong order, pop it back in the wrong order) compares equal at
the end while every query in between saw a different grid; and a failure at step
3 of 17 is reported as "the final states differ", the unlocalised verdict that
cost Wave 5's RemoveMesh shadow an extra integration run
(`reference_shadow_predicate_must_name_its_failing_term.md`).

The script is now **17 indexed steps**, each applied to both subjects and the
whole scene compared immediately, named by step.

### The float bodies need their own oracle — the grid comparison is NOT one

`BoxTransform` is the only float-exact body here, and its output reaches the
grid comparison **only after `(int)(f * _invCellSize)`**. That truncation
swallows anything smaller than a whole cell, so a 1-ULP AABB error would sail
through the entire scene comparison green. Comparing grid state and calling the
float body verified would have been "64 green comparisons where zero did any
work", one level down.

So `BoxTransform` and `MeshBoundsXY` are compared **directly and bitwise**, on
their own inputs, tolerance **zero**: 8 transforms × 6 boxes for the former
(including a degenerate box, a 1-ULP-wide box, and one whose extent terms nearly
cancel), 8 rotations for the latter, plus the empty-mesh case where retail
returns the inverted `±1e8` sentinel rectangle rather than anything sane.
Anything less than bit-exact means a term was moved, and a moved term is
algebraically identical and bit-different exactly where the terms nearly cancel
(`reference_moved_term_hides_where_terms_cancel.md`).

This sweep also covers, for free, the fact that our `BoxTransform` calls lane
P's ported `AsTransform3` while the DLL's calls the DLL's. If that ever stopped
being bit-exact it is reported here rather than as an unattributable picking
divergence.

### Excluded from the comparison, and why each

`reference_oracle_must_not_compare_unwritten_bytes.md` — an oracle that memcmps
bytes neither side writes fails 100% of calls and reads like a catastrophic port
defect. Four windows, every one read out of a disassembly:

1. **`_data` of every `Array`.** Two independent allocations, two addresses. The
   *contents* are compared instead.
2. **`Entry +0x14..+0x17`.** Neither `0x1800735c0` nor `0x18007b590` writes those
   four slack bytes.
3. **`Entry +0x3c` (`_userValue2`) — OF A FREED ENTRY ONLY.** Excluded when
   `_nextFree != -2`, compared on a live entry where `AddMesh` did write it.
   *"Never written" is meaningless without "by WHOM"*
   (`reference_never_written_needs_by_whom.md`) — Wave 7 got this wrong in both
   directions in one hour, so the exclusion is keyed per entry, not blanket.
4. **`[size, capacity)` of every `Array`.** `_capacity` never shrinks, so that
   extent holds allocator fill, which differs after a realloc.

### Controls

**Effectiveness** — every path reports how many times it ran, and a zero prints
a `GAP:` line saying in as many words that the run verified nothing about that
path. A green line over a path that never ran is the failure this exists to
prevent.

**Negative (structural)** — `BANISHED_RT_PICKGRID_PROBE=neg` runs the script a
third time with a deliberately wrong `UnlinkEntryFromCell`: the link is
unlinked from the cell but **never returned to `_freeLink` and never stamped
`_entry = -1`**. That is precisely the *missing erase* defect class that
produced this project's teardown 100%-CPU hang and `IsA` EXEC crash
(`reference_workarea_bst_bad_entry.md`), and the one a blind rewrite of that
function is most likely to introduce. The comparator must flag it; if it does
not, the comparator cannot fail and the green result is worthless.

**Negative (float)** — one **moved term** in the extent sum, `(a + b) + c` →
`a + (b + c)`, run against the shipped body over the cancelling inputs. If it is
not flagged, the sweep contains no cancelling case and the zero-tolerance float
result is weaker than it looks — and the probe says so.

---

## 7. Results

DLL-PRESENT run, `BANISHED_AUTOSTART=1 BANISHED_RT_PICKGRID_PROBE=neg`,
`reimpl-x64.exe` md5 `7f29abe8…`, identity confirmed from the run's own log
(`[RtPort] Picking: 9 exports + 19 grid addresses registered` — a line only this
build emits).

```
[PickGridProbe] comparisons=45 mismatches=0  floatCases=57 floatMismatches=0
[PickGridProbe] work: adds=10 multiCellAdds=9 removes=3 unthreads=3
                      reindexMoved=3 reindexNoop=1 cellsFreed=3
[PickGridProbe] work: entryFreeReuse=3 linkFreeReuse=4 cellFreeReuse=4
                      maxLinkChain=3 iterSteps=43 iterHits=40
[PickGridProbe] negative control: 1/1 flagged  (comparator proven able to fail)
[PickGridProbe] float negative control: 1/9 cases differ from the shipped body
                      (a moved term IS visible in this input set)
[PickGridProbe] PASS -- every ported grid body agreed with the shipped one
```

**No `GAP:` line printed** — every instrumented path ran at least once. That
absence is the measurement, not the silence:

* `multiCellAdds=9` of 10 — the multi-cell row loop is the whole risk in
  `GridUnthread`, and 9 of 10 entries exercised it.
* `maxLinkChain=3` — a cell held three links, so the **middle-of-list** unlink
  ran, not only the head case.
* `entryFreeReuse=3`, `linkFreeReuse=4`, `cellFreeReuse=4` — every free-list
  reuse arm ran on a **non-empty** list.
* `cellsFreed=3` — `FreeEmptyCell`, the cell-level erase, ran.
* `reindexNoop=1` alongside `reindexMoved=3` — the early-out and the work path
  are separately accounted, so `calls` is not being mistaken for coverage.
* `iterHits=40` of 43 steps — the iterator's stamp-skip chain walk was
  exercised, not just its termination.

*(Numbers above are from the FIRST clean run, before the lockstep and
float-oracle changes. The final numbers are §7.1; the first run is kept because
the story of finding it inadequate matters more than the number.)*

### 7.1 Final run — lockstep script + direct float oracle

Same recipe, `reimpl-x64.exe` md5 `c8e7cc58…`, identity gate passed
(`run_pick.sh` derives the expected `+ N grid addresses` from the source's own
`RegisterRuntimeAddr` count each run rather than carrying a literal forward — a
carried literal would have fired falsely the moment the count grew).

```
[PickGridProbe] comparisons=61 mismatches=0  floatCases=57 floatMismatches=0
[PickGridProbe] work: adds=10 multiCellAdds=9 removes=3 unthreads=3
                      reindexMoved=3 reindexNoop=1 cellsFreed=3
[PickGridProbe] work: entryFreeReuse=3 linkFreeReuse=4 cellFreeReuse=4
                      maxLinkChain=3 iterSteps=43 iterHits=40
[PickGridProbe] negative control: 1/1 flagged  (comparator proven able to fail)
[PickGridProbe] float negative control: 1/9 cases differ from the shipped body
                      (a moved term IS visible in this input set)
[PickGridProbe] PASS -- every ported grid body agreed with the shipped one
```

`comparisons` rose 45 → 61: 17 full-scene comparisons (one per lockstep step,
was one at the end) + 43 iterator steps + 1 negative control. Still **0
mismatches**, still **no `GAP:` line**, and the counters are byte-identical
between runs — the fixture is deterministic, so a future regression is a
signal and not noise.

### 7.2 Live regression check

The probe is a synthetic instrument, but it fires from a REAL grid call
(`ProbeGate()` sits at the top of the five entry points and self-schedules on
first use, because `RtPickingPortInit` runs before the DLL's `MemoryAllocator`
is STARTED and allocating there is the "works on my boot order" hazard lane Q
documents). **So the probe running at all proves `scene.cpp`'s forwarders now
reach these bodies.** It fires during the menu scene, at the first
`Picking::Scene::AddMesh`.

A second run drove the harness all the way through: boot → main menu → New Game
→ in-world → in-game pause menu (`buttonSave` / `buttonMenu` / `buttonMap`
resolved, which only exist in the in-game menu) → clean quit, `rc=0`,
`standalone_crash.log` **empty**, no `[RtDiff]` divergences anywhere in the run.
That is the whole picking path — every mesh in a generated world added,
transformed and removed through these nineteen bodies — with no regression.

Logs were copied out of the shared game directory **before** the lock was
released, but `*.log` is gitignored so they are not in the tree — the numbers
quoted above are the whole of the evidence, and the run is reproducible from
§7.4 in one command.

### 7.4 Reproducing the run

```sh
tools/game_dir_lock.sh acquire lane-PICK 900 || exit 1
G="C:/GOG Games/Banished"

# IDENTITY IS DERIVED, NOT CARRIED FORWARD. The on-disk md5 says which exe was
# on disk when you looked, not which one RAN. The gate is a line emitted by
# THIS build, read out of THIS run's own log -- and the expected value is
# computed from the source every time, because the literal changed the moment
# the registration count grew and a carried-forward literal would then fire
# falsely on our own success.
WANT=$(grep -c 'RegisterRuntimeAddr(0x' runtime/src/picking/scene_grid.cpp)

cp standalone/build/reimpl-x64.exe "$G/reimpl-x64.exe" || exit 2
rm -f "$G"/standalone_{debug,error,crash}.log
( cd "$G" && env BANISHED_AUTOSTART=1 BANISHED_RT_PICKGRID_PROBE=neg \
      BANISHED_PAUSEMENU=2400 BANISHED_PAUSEMENU_QUIT=120 ./reimpl-x64.exe )

mkdir -p evidence && cp "$G"/standalone_*.log evidence/    # BEFORE releasing
LINE=$(grep -a 'RtPort\] Picking:' evidence/standalone_debug.log | head -1)
case "$LINE" in
  *"+ $WANT grid addresses registered") echo "IDENTITY OK" ;;
  *) echo "*** another lane's exe ran -- discard this run ***"; exit 3 ;;
esac
grep -a PickGridProbe evidence/standalone_debug.log
tools/game_dir_lock.sh release lane-PICK
```

### 7.3 Two runs lost to a clobbered exe — and how it was caught

Two runs in this lane measured **another lane's binary** despite holding
`tools/game_dir_lock.sh`, which is advisory and was not yet known to every lane.
The first produced a complete, plausible log — from the pre-lane build. The
on-disk md5 matched at deploy time and again afterwards only because the other
lane's copy landed and was replaced in between.

**The md5 says which exe was on disk when you looked, not which one ran.** What
caught it was an identity line emitted by this build alone, read out of the
run's own log: `[RtPort] Picking: 9 exports + 19 grid addresses registered`. The
stale run said `9 exports registered`, the pre-lane format, and was discarded on
the spot. `run_pick.sh` now gates on that line and derives the expected count
from `scene_grid.cpp` itself.

---

## 8. Instruments

`tools/addr_door_audit.py` is **new in this lane**. The address-door census
behind `ce1d8e9` / `4ef55b6` was produced ad hoc and never committed, so the
next lane would have had to re-derive it. It is committed here so it can be
re-run — *a shared instrument is fixed by ONE owner in ONE commit*
(`reference_census_cannot_see_the_macro_doors.md`).

It states its own blind spot rather than hiding it: the address must be a
**literal** in both the call and the registration, exactly the hole
`runtime_census.py` had for twelve live ports for a full wave
(`reference_census_cannot_see_nonliteral_registrations.md`).
`--nonliteral` reports the sites it cannot see, so the blind spot is at least
**measured**: today 63 non-literal call sites and 10 non-literal registrations.

**Its counts do not match the committed CSV exactly, and that is expected.**
`4ef55b6` removed 32 rows from `ce1d8e9`'s 94 because those addresses are
supplied by the DATA MINT rather than by `RegisterRuntimeAddr`, and this script
does not model the mint. So it reports 88 unregistered where the curated list
has 56. Use it for *deltas* (what did my change close?) and the curated CSV for
the absolute cutover watch list.

Delta for this lane, measured:

```
CLOSED: 0x18001f470 0x1800724d0 0x180072950 0x18007af70 0x18007b390 0x18007b6d0
registered addresses  508 -> 527   (+19, exactly this lane's registrations)
curated CSV rows       62 -> 56
```

Other instruments, all clean:

* `tools/port_bypass_audit.py` — `0` address-door sites reach a function ported
  by name; `0` raw-door sites are a ported function on a live path.
* `tools/abi_retslot_audit.py` — `0` suspicious of 48 by-value-returning
  exports.
* `tools/runtime_census.py` — `called 958, ported 671, 70.0%`, up from
  `665 / 69.4%` at the post-`29cfaa2` baseline. **`called` did not grow**: all
  six addresses that moved were already in the denominator, and the other
  thirteen registrations are reached by direct C++ call rather than through a
  resolver, so they add coverage without adding surface.

### The census's "ports with no caller" list — a written verdict

Eleven of this lane's addresses appear in `runtime_census.py`'s orphan table
(`0x180072730`, `0x180072b30`, `0x180072d10`, `0x180072e70`, `0x1800735c0`,
`0x1800738c0`, `0x180074100`, `0x18007b210`, `0x18007b590`, `0x18007bc30`,
`0x180125bc0`). The census warns that *"a NEW name appearing here has NOT been
triaged"*, so here is the triage:

**All eleven are called, by a direct C++ call from a sibling port in the same
file.** They are the closure of the five entry points and are invoked as
`rtpickgrid::` functions, not through any resolver — which is precisely what the
census cannot see. This is the same category the census already accepts for five
existing rows ("exercised by a direct C++ call from a sibling port"). Every one
of them is executed by the probe on every run: the counters in §7 are the proof
(`cellsFreed` is `FreeEmptyCell`, `linkFreeReuse` is `AddToCells`' free-list
arm, `entryFreeReuse` is `EntryAlloc`'s reuse arm, `floatCases` is
`BoxTransform` and `MeshBoundsXY`).

`0x18001b360` and `0x18001c100` do not appear in the orphan table because they
are also reached from elsewhere.

---

## 9. Kill switch and blast radius

`BANISHED_RT_PICKGRID=0` leaves all nineteen unregistered, and
`scene.cpp`'s five forwarders plus `graphics/scene.cpp:254` go back to the DLL
exactly as before this lane. It is a **separate** switch from
`BANISHED_RT_PICKSCENE` on purpose: this is the only part of the Picking
namespace that allocates, so an integration run that sees a new failure can
halve the search space with one env var.

Registering these changes behaviour at **six existing call sites** —
`runtime/src/picking/scene.cpp` ×5 and `runtime/src/graphics/scene.cpp:254` —
because `ResolveAddr` consults the port registry first. Both files carried
comments asserting they reached the DLL; both are corrected in the same commit.

`runtime/src/picking/scene.cpp` now has **no** address door left reaching
Runtime-x64.dll: its twelve `ResolveAddr(Module::Runtime, …)` sites are five
from lane Q (`0x18007b960`, `0x1800737f0`, `0x1800739e0`, `0x180073bb0`,
`0x180059140`), one from lane D2 (`0x180073c90`), one duplicate of lane Q's
`0x1800739e0`, and this lane's five.

---

## 10. Open items — none, and one caution

Nothing is held back and nothing is closed with a justification: 5 of 5 census
rows are ported and verified against the shipped bodies.

**The caution is about the probe's reach, not the port's.** The probe runs on
synthetic scenes, so it verifies the bodies against the DLL over the inputs it
constructs. It does **not** measure the live game's picking grid, and no
instrument in this lane does — a live diff is impossible for destructive
mutators, which is the whole reason the probe exists. The mitigation is that the
probe's population was built to be *harder* than the live one on the axes that
matter (multi-cell rectangles, shared cells, non-LIFO free-list order, negative
coordinates, off-axis rotations), and that every one of those axes reports a
non-zero counter. What the probe cannot cover is a live-only *input* shape — a
mesh with a node count or an AABB the fixture does not contain. Nothing in the
five bodies branches on either, but that is an argument from reading the
disassembly, not a measurement, and it is recorded as such.
