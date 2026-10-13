# DRAWSORT -- SetSizeCore's empty tail and the exact keyed draw-item sort

Block DRAWSORT, 2026-10-13. These are the two remaining main-thread targets from IBS_20261012
(section 3.3): `rtcoll::SetSizeCore` (8.8% of ops) and the per-frame draw-item sort (8.6%). Both are
faithful perf work. Neither adds a registry entry, a settings key, a `BANISHED_*` flag or an `Opt()`
field. Each target has a `static constexpr` kill switch. **This block ran the static gate only.** The
coordinator runs the game, the shadow and the A/B.

## 1. Target 1: `SetSizeCore`

### 1.1 Attribution

`Scene::AddCulledDrawItems` (cull.cpp:384-385) starts every call with a fresh local
`GeomList list = {nullptr, 0, 0}`. It then calls `ReserveVisibleList` (cull.cpp:161), which calls
`rtcoll::RtColl_ArrayReserve_18005f980` (array.cpp, `Reserve(16384)`, retail 0x18005f980). That body
is `SetSizeCore(h, 0x4000, nullptr, &kSpec_Fill8)` followed by a restore of `_size`. With
`_size == 0` the core reallocates (MemAlloc of 128 KB), then runs its element loop over
`[0, 16384)`. Each iteration computes `dst` and tests it against null. With `fill` null and
`kSpec_Fill8.init` null, it does nothing else. There are 17-21 cull passes per frame (the FRUSTUMCULL
counter), so this is ~300k empty iterations per frame. That explains IBS's profile: cheap ops, many of
them, 0% misses, and no callee row. The coordinator's sampled heavy run confirms it: 270 of 317
`SetSizeCore` samples come from this reserve. The remaining 31 come from `UI::SpriteContext::AddSprite`
(appends). The same samples put SetSizeCore at only 0.44% of all samples (~0.03 ms/frame), so the
8.8% IBS figure was an op count, not time.

Retail's own instantiation of that reserve has no tail loop (array.cpp's banner: "the copy loop is
bounded by the OLD size"). The loop came from routing the reserve through the generic core.

### 1.2 The fix and why it is exact

`SetSizeCore` computes the loop end once per call. When `fill == nullptr && spec->init == nullptr`
and `oldSize < newSize`, the loop stops at `oldSize`.

- The iterations it skips are the ones with `i >= oldSize`. In each of them the original body
  computes `dst = data + i*stride`, tests `dst == nullptr`, and then takes three false branches
  (`i < oldSize`, `fill`, `spec->init`).
- So the skipped iterations perform no load from the buffer, no store and no call. Reading
  `spec->init` from a const table has no effect.
- The realloc prefix copy `[start, oldSize)` still runs. MemAlloc, MemFree and destroy are untouched,
  and the header stores come before the loop.

The result is the same bytes, the same header and the same allocator calls. The kill switch is
`rtcoll::SetSizeFast::kSkipEmptyTail` in `src/engine/collection/setsize_prof.h`. The coordinator said
no shadow is needed here. The unit tests drive the real core and the real reserve body.

**The visible-list buffer is not reused.** It is allocated (128 KB) and freed on every pass
(cull.cpp:385 and :505). Reusing it across passes would keep the list contents exact: `PushVisible`
writes `[0, size)` before the dispatch loop reads it. But it would remove one MemAlloc / MemFree pair
per pass, and that changes the allocator's state and so the addresses of later allocations. The
draw-item sort compares pointers as keys. Allocation addresses are not reproducible run to run anyway,
so this is arguably fine, but it is not provable from the code, so this block leaves it alone.
`ext.draw_gather` already reuses the buffer (class A).

### 1.3 The counter (any COMPPROF >= 1)

Only main-thread calls are counted. The thread is marked by `StressHarness_DrawBegin`, because the
loader and pathfinding threads also call `SetSizeCore` and the table is not thread-safe. The faithful
path pays one load and one predictable branch per call.

```
[COMPPROF]   setsize: calls/frame=N  loop iters/frame=I  empty iters/frame=E (P%)
[COMPPROF]     setsize <spec name> calls/f iters/f empty/f realloc/f maxN  iters/f by class append clear grow0 grow shrink
```

The detail rows are the top six specs by iteration count. `empty` counts the iterations of the
original loop that the fast arm skips. This counts what the original loop would do, so the figure is
the same whichever way the kill switch is set. **Expected:** an `Array<ptr>` row with
`grow0 ~= 16384 x passes/frame`, `empty ~= iters`, `maxN = 16384`.

## 2. Target 2: the draw-item sort

### 2.1 The consumer

The sorted lists are read only by `Renderer::DrawItemList` (batchc_gfx_leaves.cpp:684-820), through
`DrawLight`. Per item it reads `_material`, `_instance`, `_arg5`, `_geometry`, `_userData`, `_arg10`
and **`_arg8`**. `_arg8 > 1` opens an instancing batch of at most `_arg8` consecutive items that agree
on instance / arg5 / userData / arg10 (lines 754-787). So `_arg8` is observable after the sort, and it
is not one of the compared terms. The two pad dwords (+0x24, +0x34) are never read.

The writers of `_arg8` (all `AddDrawItem` callers) are:

| caller | `_arg8` |
|---|---|
| `MeshInstance::AddDrawItems` (mesh.cpp:2198) | `_detailLevels[level]._maxInstances`, or 1 |
| particles (wave27_lane_gfxpart.cpp:454) | `0x34` or 0 |
| edge meshes, terrain / water / decals / roads, weather, status icons | 1 |

For two items to tie on all eight compared terms, they must come from the same geometry with the same
arg5 (`level | list index` for meshes), which in practice means the same arguments. So ties with a
different `_arg8` look impossible today. That is a reading of every caller, not a proof the code
enforces. Separately, a NaN `_sortKey` makes the comparator a non-strict-weak order (the NaN term
falls through to the next one), and then the result depends on the algorithm. **Option (a) (std::sort
or radix) is therefore not exact by construction. This block builds option (b).** Option (c) was not pursued. `DrawItemList`'s
drop tests use the shader index of the draw call, which the light's `DrawLight` passes in at emit
time. `AddDrawItem` already drops an item on the group's own shader index, so this block cannot tell
statically whether any appended item goes undrawn.

### 2.2 The design and the proof

The design is retail's skeleton (`introsort_msvc.h`) with the same decisions and the same swap
sequence, run on a parallel key array.

1. **Key** (`DrawSort::Key`, five words). The bit string, most significant first:
   `flags(16) | desc(sortKey)(32) | material(48) | instance(48) | arg5(16) | arg10(32) | userData(48) | geometry(48)`
   (288 bits), then the item index in the low 32 bits of w4. `KeyLess` compares w0..w3, then
   `w4 >> 32`.
   - Claim: for every pair of items without a NaN key and with all pointers below 2^48,
     `KeyLess(k(a), k(b)) == DrawItemLess(a, b)`.
   - Proof: `DrawItemLess` is lexicographic over the eight terms, in this order. Each term is
     encoded in a fixed-width field whose unsigned order is the term's order and whose equality is
     the term's equality. Lexicographic comparison of a concatenation of fixed-width fields equals
     lexicographic comparison of the fields, so the two orders agree. Field by field:
     - `flags`, `arg5` and `arg10` are already the unsigned compares retail makes.
     - The pointers are `std::less`, which is the unsigned address compare. Their high 16 bits are
       zero (checked per list), so 48 bits carry the whole order.
     - The float uses `desc(x) = ~asc(x)`, where `asc` is the standard sign-flip map. It is
       monotone, so `x > y <=> desc(x) < desc(y)`. Before mapping, `x == 0.0f` (UCOMISS, under the
       same MXCSR the comparator's COMISS uses) replaces x with +0's bits. That covers +0, -0, and
       every denormal when DAZ is set. These are exactly the values for which both `>` tests are
       false against 0, so "neither greater" <=> equal bits.
   - The index sits below the compared bits and never decides anything.
2. **The sort.** `rtsort::Introsort` and `SelectionSortMaxToHigh` run on the key array, with an
   inlined key compare and a 40-byte key swap.
   - Every branch of the skeleton depends only on comparator results and on position compares
     (`PtrLt` etc. within one array). Positions map one-to-one between the key array and the item
     array.
   - By induction over the steps, key slot p always holds the key of the item that retail's run has
     at slot p, so the swap sequence is the same.
   - Afterwards the items are gathered once by the carried index into a scratch buffer and copied
     back. Retail moves whole 0x38-byte records, so the final item array is byte-identical to
     retail's, including the pads.
3. **Lists that are not keyed** take retail's skeleton on the items, with `DrawItemLess` and
   `DrawItemSwap` as inlined lambdas: the same code as the reference minus the indirect calls. These
   are lists under 32 items, lists with a NaN key, and lists with a pointer at or above 2^48 (never
   expected on Win64 user space; possible on a 52/56-bit VA). The kill switch
   `DrawSort::kFastSort = false` runs the reference itself: function pointers, exactly as before.

Both callers of 0x180062d60 now reach the fast sort through `RtGraphics_Renderer_DrawItemSort`:
`Renderer::Draw` (wave27_lane_gfxbig.cpp, pass 1) and `Renderer::EndDrawGroup`
(wave29_lane_gfxbody.cpp:670, the light / shadow groups). `DrawItemLess`, `DrawItemSwap` and the two
reference bodies moved unchanged into `draw_sort.h` / `draw_sort.cpp`. The retail reading stays in
wave27's banner.

The scratch buffers are three function-static `std::vector`s (keys, items, shadow copy). They are
main-thread only, grow-only, and the sort never re-enters. They allocate only when a list is longer
than any before it.

### 2.3 The shadow `DrawItemSort` (BANISHED_STRESS_COMPPROF=4)

For every sort, the shadow copies the list, runs `DrawSort::SortReference` on the copy, runs the fast
sort on the list, and `memcmp`s the whole arrays (n x 0x38 bytes). `detail = (count << 32) | first
differing index`. On a mismatch it copies the reference order into the list, so a shadow run draws the
faithful frame. Read: `[VERIFY] DrawItemSort checks=N mismatches=M`. **Expected M = 0**, with N equal
to the number of lists sorted over the run. A shadow run sorts every list twice, so its draw ms are
not a perf number.

### 2.4 The counter (any COMPPROF >= 1)

```
[COMPPROF]   draw sort: sorts/frame=S items/frame=I (max list L) compares/frame=C (C/I per item)
             keyed=K inline: small=a nan=b ptr=c /frame  ties/frame=T  ties arg8!=/frame=D
```

- `keyed` is the number of lists that took the key path.
- `small`, `nan` and `ptr` are the lists that took the inlined item path, by reason.
- `ties` counts adjacent pairs in the sorted output that compare equal.
- `ties arg8!=` counts the ties whose `_arg8` differs. These are the cases where only retail's swap
  sequence decides what `DrawItemList` batches.

Expected: `nan = ptr = 0`. `D` is what section 2.1 predicts to be 0. If it is 0 over a long run, the
reading is confirmed, and an option (a) sort (std::sort or radix over the same key) would also be
exact whenever there are no NaN keys. That is the next step if this one is not enough.

### 2.5 Unit tests (`tests/unit/draw_sort_test.cpp`, all pass)

- `draw_sort_key_order_is_the_comparator`: 2,000,000 pairs drawn from small value pools (often with
  their leading terms copied across), with floats including +-0, +-inf, denormals, +-FLT_MAX and
  random bits. `KeyLess` matches `DrawItemLess` both ways, and `KeyEqual` holds exactly when neither
  sorts first (196k full ties).
- `draw_sort_sort_key_bits_reverse_the_float_order`: 2,000,000 float pairs.
- `draw_sort_build_keys_refuses_nan_and_high_pointers`.
- `draw_sort_fast_equals_reference_bit_for_bit`: 1,500 lists, 1,315,997 items, sizes 2-4000. Pads
  and bytes are random, and the lists include duplicates that differ only in `_arg8` and the pads,
  presorted and reversed inputs, 186 NaN lists and 191 lists with high pointers. `memcmp(SortFast,
  SortReference) == 0` for every list.
- `draw_sort_shadow_reports_and_counts`: the shadow reports a match, leaves the reference order, and
  the counters see a tie that differs in `_arg8`.

Two mutations were checked (both reverted):
- With the index included in the compare (`a.w[4] < b.w[4]`), the bit-identity test and the key test
  both fail.
- Dropping the `x == 0.0f` merge fails four tests.

## 3. Prediction

- **Target 1:** most of IBS's 8.8% `SetSizeCore` op share goes away. In wall time that is ~0.03
  ms/frame (the coordinator's samples, 0.44%), which is noise.
- **Target 2:** the sort is ~12% of the frame's real CPU work (coordinator's samples: DrawItemLess
  4.3 + Introsort 1.8 + Swap 0.7 + std::less 0.4). Expected changes:
  - The out-of-line comparator call per compare goes away.
  - A deep compare (instanced trees tie on seven terms, so the geometry pointer decides) becomes 5
    word compares instead of 8 field compares plus a call.
  - Swaps move 40 bytes instead of 56.
  - Added cost per list: one key build pass and one gather-and-copy pass.
  - Expect **-40 to -60% of the sort**: 8.6% of ops down to ~3-5%, draw ms/frame -0.08 to -0.15 of
    ~2.1 ms CPU in the heavy town, uncapped fps +3-6%.
  - At 60 Hz vsync nothing visible changes. The time becomes main-thread slack, or more sim
    iterations at high speed.

## 4. How to measure (coordinator)

1. Shadow: `BANISHED_STRESS_COMPPROF=4`, heavy town. Expect `DrawItemSort ... mismatches=0`.
2. Rows: any COMPPROF >= 1 run prints `setsize:` and `draw sort:`. Check `nan = ptr = 0` and read
   `ties arg8!=`.
3. A/B: flip `DrawSort::kFastSort` (draw_sort.h) and `SetSizeFast::kSkipEmptyTail`
   (setsize_prof.h), rebuild perf, run COMPPROF=0 with RTSS / Afterburner stopped on a CLEAN machine,
   interleaved, heavy and compact towns. Compare `draw_ms_mean` and fps. The sim columns must not
   move.

## 5. Doubts

- `x == 0.0f` and COMISS must run under the same MXCSR. They do: same thread, same frame, and
  nothing in `src/` changes MXCSR. A DLL that flipped DAZ between the key build and the sort would
  be needed to break this, and the sort calls nothing.
- The pointer-width check makes a list with any pointer at or above 2^48 fall back to the inline item
  path. That is exact, just slower. The counter (`ptr`) shows whether it ever happens.
- The memcpy-back costs n x 0x38 bytes per list on top of the sort. For short lists the threshold of
  32 avoids it. The threshold is a guess, because this block did not run the game.
- `ties arg8!=` is the only data on whether option (a) would be safe. This block does not rely on it.

## 6. Files

| file | change |
|---|---|
| `src/engine/collection/setsize_prof.h` (new) | `SetSizeFast::kSkipEmptyTail`, the counter's API |
| `src/engine/collection/array.cpp` | the loop end in `SetSizeCore`, the counter table |
| `src/engine/graphics/draw_sort.h` / `.cpp` (new) | `kFastSort`, the comparator and swap (moved), the key, the reference and the fast sorts, the shadow and the counters |
| `src/engine/graphics/wave27_lane_gfxbig.cpp` | the two extern "C" sort entries forward to `DrawSort` |
| `src/diag/stress_harness.cpp` | COMPPROF >= 1 turns on both counters (the SetSize thread mark in DrawBegin); COMPPROF=4 turns on the `DrawItemSort` shadow; the two report rows |
| `tests/unit/setsize_fast_test.cpp`, `draw_sort_test.cpp` (new), `tests/unit/sources.cmake` | 4 + 5 tests; draw_sort.cpp linked into `unit` |

## 7. Static gate (after `git merge main`)

See the final commit message for the post-merge numbers:
- `tools/build.sh build perf --jobs 2`: OK. The keyed skeleton is fully inlined (no call in it).
- `tools/build.sh build debug --jobs 2`: OK.
- `tools/build.sh unit perf --jobs 2`: 1139 tests, 0 failures.
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: total 2. The one new reinterpretation is marked
  `// CAST-OK: ptr_int`.
- No new warnings in the touched files.

## Results (coordinator, 2026-10-13 midday) -- SHIPPED ON (both targets)

Attribution confirmed by the sampler (BANISHED_SAMPLE=1000, heavy, `perf/results/20261013_105425_sample_setsize_heavy`):
270 of 317 SetSizeCore samples under `RtColl_ArrayReserve_18005f980` from `RtGraphics_Scene_CullStackInit`;
in wall time 0.44% of samples, so the op share IBS reported was cheap ops. Pre-merge on the branch perf exe
(heavy): shadow `DrawItemSort` 27,869 / 0; `setsize` row: 554 calls / 115,196 loop iterations per frame,
114,445 of them empty (99.3%), all in `Array<ptr>` grow-from-0 at maxN 16384, 7 reallocs per frame; `draw sort`
row: 9.3 sorts / 25,215 items per frame (largest list 3,915), 13.6 compares per item, every list keyed
(nan = ptr = small = 0), **ties 0.0 and ties with a differing `_arg8` 0.00 per frame**. Gate DRAWSORT 0
problems (merge `cbc3c6848`, a keep-both resolution in stress_harness.cpp against SKINMEMO; unit 1143 / 0).
A/B (series ab1, CLEAN, 2 interleaved pairs; `sortoff` = both switches off):

| town | draw ms off | on | delta |
|---|---|---|---|
| heavy | 1.932 / 1.949 | 1.931 / 1.914 | -0.1% / -1.8% |
| compact | 1.312 / 1.291 | 1.279 / 1.267 | -2.5% / -1.9% |

Never worse; kept ON (`SetSizeFast::kSkipEmptyTail`, `DrawSort::kFastSort`). Follow-up on record, not
taken: the row's 0 ties means a plain `std::sort` / radix over the same key would also be exact in this town;
it would need the tie census in a town with instanced batches before it could ship. The visible list's
128 KB alloc / free per cull pass stays (the "Not done" item above).
