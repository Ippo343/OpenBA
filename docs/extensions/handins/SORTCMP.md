# SORTCMP -- the draw-item sort with fewer compares, exact for the consumer

Block SORTCMP, 2026-10-13. Idea 3 of `IBS_20261013.md`. DRAWSORT made each compare of the per-pass draw-item
sort cheaper (a packed 320-bit key, retail's skeleton on the keys). This block cuts the number of compares. It is
faithful perf work: no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()` field. It has a
`static constexpr` kill switch, `DrawSort::kPlainSort` in `src/engine/graphics/draw_sort.h`. **This block ran the
static gate only.** The coordinator runs the game, the shadow and the A/B.

## 1. The exactness argument

### 1.1 The claim

Take any correct sort of a list of items whose keys are all valid (no NaN `_sortKey`, every pointer below 2^48).
Items with equal full keys then sit in contiguous tie groups. Two cases follow:

- **Every tie group agrees on `_arg8`.** Then every record in a group agrees on every field `DrawItemList` reads.
  So the sequence of values the consumer reads is the same whatever order a group is in. Any correct sort is then
  observationally identical to retail's skeleton.
- **Some tie group carries differing `_arg8`.** Then the order inside that group can change an instancing batch,
  and only retail's swap sequence is exact. The list falls back to DRAWSORT's keyed skeleton.

### 1.2 Why equal keys plus equal `_arg8` means equal consumer reads

- **The key covers six of the seven consumer fields.** DRAWSORT section 2.2 proves that two keys are equal
  exactly when `DrawItemLess` finds neither item first. Each term sits in a fixed-width field whose equality is the
  term's equality. So equal keys mean bit-equal `_flags`, `_material`, `_instance`, `_arg5`, `_arg10`,
  `_userData` and `_geometry`. The pointers are checked below 2^48 per list, so 48 bits carry the whole pointer.
  `_sortKey` is only equal up to the comparator: +0, -0 and DAZ denormals tie.
- **The consumer reads seven fields and nothing else.** `DrawItemList` (`batchc_gfx_leaves.cpp` 712-820) reads
  `_material`, `_instance`, `_arg8`, `_geometry`, `_arg5`, `_userData` and `_arg10`. It never reads `_flags`,
  `_sortKey`, `_pad24` or `_pad34`. Every callee gets field values: `GenerateTextures` and `Setup` on the instance,
  then the geometry's `SetSharedInstanceData`, `GetInstanceTransform` and `Draw`. The batch loop compares the
  next item's `_instance`, `_arg5`, `_userData` and `_arg10`. No pointer to an item escapes. So with `_arg8` also
  equal, the records in a tie group can differ only in bytes nobody reads.
- **Nothing else reads the item contents** (checked by grep of `_items[`, `._items` and `->_items` over `src/`):
  - `Renderer::AddDrawItem` (`wave9_lane_gfx.cpp:396`) appends.
  - `RtGfxd_DrawGroupClearPasses` (`wave9_lane_gfxd.cpp:446`, from `Renderer::Reset` every frame) sets the size
    to 0.
  - The draw-group copy constructor (`closure_batch2b.cpp:256`) copies whole arrays when the group array grows.
  - The group array's destructor (`array.cpp:2010`) frees.
  - `scene_watch.cpp:819` reads sizes.
  - The two sorts (`Renderer::Draw` pass 1, `Renderer::EndDrawGroup`) both enter through `DrawSort::Sort`.

### 1.3 The fallback starts from the original order

`SortPlain` takes `const Key*`. It reads the item array only for the `_arg8` test in the tie scan. It writes the
item array only in its last step, after the scan has passed. When the scan finds a differing-`_arg8` tie,
`SortPlain` returns false before that step. The keys and the items are then exactly what `BuildKeys` saw, and
`SortKeyed` runs DRAWSORT's keyed skeleton on them. That path is unchanged and byte-exact. The unit test
`draw_sort_plain_refusal_leaves_data_and_keys` checks both arrays after a refusal.

### 1.4 Across sorts

One more case needs an argument: a list whose earlier sort permuted a harmless tie group, then is sorted again
without a clear, and that second sort falls back. Call two records equivalent when they have the same key and the
same `_arg8`. Retail's skeleton decides only through the comparator, which sees only the key. So if two inputs
hold equivalent records position by position, the skeleton makes the same swaps on both. Its outputs then hold
equivalent records position by position. By induction over every sort and append, our lists and retail's agree
up to equivalence at every point, and equivalent records are identical to the consumer. Today no list is sorted
twice anyway: `Reset` clears every group each frame, and each group's lists are sorted once.

### 1.5 Lists that are not keyed

NaN keys, pointers at or above 2^48, and lists under 32 items take the inlined skeleton on the items, as in
DRAWSORT. A list over 65,535 items, where the plain path's 16-bit index field would not fit, takes the keyed
skeleton. Both are unchanged and byte-exact.

### 1.6 What is not exact, and why it does not matter

Inside a harmless tie group, the pad dwords and the raw `_sortKey` bits (+0 against -0) can land in a different
order than retail's. Nothing reads them (section 1.2). This is why the shadow compares the consumer view and not
the bytes (section 4).

## 2. The census (any COMPPROF >= 1)

There is a second row under DRAWSORT's `draw sort:` row. It is per frame unless the field says otherwise:

```
[COMPPROF]   draw sort/cmp: plain=P fallback=F /frame  groups/list=G  runs/list=R  same-as-last=H% (C checked/frame)  max tie=T  arg8>1 items/frame=I  shadow byte-only=B /frame
```

| field | definition |
|---|---|
| `plain` | keyed lists that took `SortPlain`, fallbacks included |
| `fallback` | of those, lists with a full-key tie whose `_arg8` differs; the keyed skeleton reran them from the original order |
| `groups/list` | distinct 240-bit prefixes (every key term above the geometry) per plain list; the number that decides the algorithm's cost |
| `runs/list` | ascending runs in the key array before sorting, per keyed list (1 = presorted, n = strictly descending); measures how presorted the gather's order is |
| `same-as-last` | keyed lists whose key array hashes (64-bit) equal to the same buffer's previous sort, over the lists whose buffer was sorted before; option (c)'s hit rate, measured only, no skip built |
| `max tie` | the longest tie group in any sorted output of the window (1 = no ties) |
| `arg8>1 items/frame` | items with `_arg8 > 1`, each of which can open an instancing batch in `DrawItemList`; shows whether the town has instanced batches at all |
| `shadow byte-only` | COMPPROF=4 only: lists that were consumer-equal to retail's output but not byte-equal (a harmless tie permutation) |

DRAWSORT's row keeps its fields. `ties/frame` and `ties arg8!=/frame` are the full-key tie censuses. `compares`
now includes the plain path's hash probes (each one a 4-word equality test), the prefix sort's compares and the
segment sorts' one-word compares. On a fallback it also includes the keyed skeleton's compares. So C/I per item is
an honest before / after figure.

The list's identity in `same-as-last` is its buffer pointer. `Reset` keeps each group's capacity, so a buffer
lives across frames unless it grows. A buffer that grows reads as a new list.

## 3. The algorithm and why

**What the data says.** The gather emits items object by object (`MeshInstance::AddDrawItems` adds all triangle
lists of one instance in a row). Objects come in scene and cell order, which says nothing about material
addresses, so long ascending key runs are not expected, and a natural merge sort would gain little. `runs/list`
measures this. For meshes `_sortKey` is the constant `0.0f`, so the leading 64 bits hold `_flags` and the
material's top 16 bits, which are nearly constant. A radix on the top 48 bits would therefore find a handful of
buckets and save almost nothing.

The structure that does exist: thousands of items fall into a few hundred "prefix groups" that agree on every
term except the last. Those are the same mesh, material instance, detail level, list index and animation handle,
and they differ only in the geometry (the MeshInstance). Plain `std::sort` over the 40-byte key would cost about
n log2 n, about 12 compares per item, barely below 13.6.

**Chosen: hash-group by prefix, then sort within groups** (`SortPlainImpl` in `draw_sort.cpp`):

1. Hash each item's 240-bit prefix (`w0`, `w1`, `w2`, `w3 >> 16`) into an open-addressed table of int32 group
   ids, with power-of-two capacity at least 2n. Each probe compares 4 words with the group's stored prefix.
2. `std::sort` the distinct prefixes. They are all distinct, so the order is strict.
3. Compute segment offsets in prefix order. Place each item's `(geometry48 << 16) | index` word in its group's
   segment, keeping the original order within the group.
4. Sort each segment as plain `uint64_t`: insertion sort up to 16 items, `std::sort` above. The geometry decides.
   The index only orders equal geometries, which are full-key ties.
5. Tie scan: in each sorted segment, adjacent words with equal geometry bits are a full-key tie. If their items'
   `_arg8` differ, return false.
6. Gather the items by index into the scratch buffer and copy back. This is the same final permutation as
   `SortKeyed`, with the same 70% miss, which no algorithm removes.

Cost with G groups and mean group size g = n / G:

- **Before:** about 13.6 five-word compares per item.
- **After:** about 1.1 hash probes (4 words) per item, plus G log2 G prefix compares, plus about g/4 (insertion)
  or log2 g one-word compares per item.

All scratch is grow-only, function-static and main-thread only: the table, group ids, groups, offsets and
segments, beside DRAWSORT's three buffers. The table costs one `std::fill` of 4 x cap bytes per list (32 KB at
4,000 items).

**What I expect the census to show:** `groups/list` of 100-400 on 2,700-item lists, `runs/list` close to n/2
(no useful presortedness), `same-as-last` near 0% at speed 10, and `max tie` = 1 or small. If `groups/list` comes
in near `items/list`, mostly particles or terrain batches with unique prefixes, the plain path costs more than the
keyed skeleton and the switch should go off.

## 4. The shadow `DrawItemSort` (BANISHED_STRESS_COMPPROF=4)

As before, for every sort the shadow copies the list, runs `SortReference` on the copy and runs the fast sort on
the list. What changed is the verdict:

- **Byte-equal:** a match.
- **Not byte-equal:** the arrays are compared item by item with `DrawSort::ConsumerEqual`, which checks the
  seven fields `DrawItemList` reads, `_flags` exactly, and `_sortKey` up to the comparator's equivalence. All
  equal: a match, counted in `shadow byte-only`. Any item differing: a mismatch, `detail = (count << 32) | first`
  differing item.
- **Either way:** whenever the bytes differ, retail's order is copied back. A shadow run therefore draws, and
  later sorts from, the faithful bytes.

Read `[VERIFY] DrawItemSort checks=N mismatches=M`. **Expected M = 0.**

What `shadow byte-only` should read depends on the switch:

- **`kPlainSort = true`:** the share of lists where the plain path permuted a harmless tie group. Expected about
  0, because DRAWSORT read 0 ties in the heavy town.
- **`kPlainSort = false`:** it must read exactly 0. The keyed path is byte-exact, and a non-zero count there is a
  DRAWSORT defect that the consumer-view verdict would otherwise hide.

## 5. Unit tests (`tests/unit/draw_sort_test.cpp`, all pass)

- `draw_sort_fast_equals_reference_bit_for_bit` now drives `SortFastExact`, the DRAWSORT path. It stays byte-exact
  on 1,500 lists, NaN and high-pointer lists included.
- **`draw_sort_plain_is_consumer_exact`:** 1,500 lists of 2-4,000 items (1,397,314 items). A third are
  gather-shaped (prefix groups, one geometry each, sort key 0, `_arg8` 1 or 10). The rest are DRAWSORT's random
  items with NaN keys and high pointers mixed in. Each list has planted full ties: copies with fresh pads,
  sometimes a fresh `_arg8`, sometimes -0 for +0. Checks:
  - `SortFast` is `ConsumerEqual` to `SortReference` on every item of every list.
  - The arrays are byte-equal whenever the reference output has no tie or has a differing-`_arg8` tie.
  - 1,393 lists had ties, and 53 were harmlessly permuted (the case is exercised).
- **`draw_sort_plain_falls_back_on_differing_arg8`:**
  - A planted five-item tie group with equal `_arg8`: plain path, no fallback, `ties = 4`, `max tie = 5`,
    consumer-equal.
  - The same group with one `_arg8` changed: fallback, byte-equal.
  - A NaN key: inline path, byte-equal, never plain.
- **`draw_sort_plain_refusal_leaves_data_and_keys`:** a refusal leaves the items and the keys byte-identical.
  Without the differing tie, the result is in order and consumer-equal.
- **`draw_sort_census_runs_and_instancing`:**
  - A presorted list gives `runs = 1` and a reversed one `runs = n`.
  - `instItems` matches a direct count.
  - The same buffer re-sorted with the same keys is a repeat hit, and with two keys swapped a miss.
- `draw_sort_shadow_reports_and_counts` additionally checks that its planted differing-`_arg8` tie took the plain
  path, fell back and left `shadowBytes = 0`.

Two mutations were checked and both reverted:

| mutation | tests that fail |
|---|---|
| tie scan without the `_arg8` test | 4 |
| the prefix `std::sort` skipped | 3 |

## 6. How to measure (coordinator)

**The towns.**

- Heavy: `tools/stress_run.sh --map large --citizens 1600 --sets 3 --spread --speed 10`.
- Compact: the default town (`tools/stress_run.sh` with no town arguments, the medium map, the camera parked over
  the town).

**Where instanced batches come from.** `MeshInstance::AddDrawItems` (`mesh.cpp:2198`) sets `_arg8` to
`_detailLevels[level]._maxInstances`. That is the mesh resource's `_instanceCount`, serialized per detail level
(`graphicsmesh_vtable.cpp:381`). Particles set `_arg8 = 0x34` while their resource is active
(`wave27_lane_gfxpart.cpp:454`). The game's `.rsc` files (`ToolKit/resource`) set `_instanceCount` as follows:

| `_instanceCount` | meshes |
|---|---|
| 10 | WoodHouse1-6, StoneHouse1-6, every stone and wood fence piece, the cemetery tombstones |
| 15 | Well, Hostel |
| 4 | Chapel, Tavern, Cemetery, the wood bridges and the tunnels |
| -1 | trees, fruit trees, crops, herbs, ore, rocks, resources, citizens, animals |
| 1 | the other buildings |

The value -1 compiles to whatever `Tools-x64.exe` derives; this block did not decode it. **The stress town
already places them.** `kStressSet` (`starttown.cpp:992`) holds the Well, Chapel, Cemetery, Tavern and Hostel, and
`kStressHouses` places a WoodHouse or StoneHouse per family. Chimney smoke is a particle with `_arg8 = 0x34`. The
DRAWSORT census of 0 ties was therefore probably taken with instanced items on screen; the new
`arg8>1 items/frame` field settles that. Take the census in both towns. In the heavy spread town the houses stand
a quarter map east of the centre. The compact town keeps them around the centre, under the parked camera, so it
is the one to read for batches. If both read `arg8>1 = 0`, pin the camera over the houses (skill `camera-pin`)
and repeat.

**Steps:**

1. **Shadow:** `BANISHED_STRESS_COMPPROF=4`, heavy and compact. Expect `DrawItemSort ... mismatches=0`. Read
   `shadow byte-only`: about 0 expected with the switch ON, and it must be 0 with the switch OFF.
2. **Census:** any COMPPROF >= 1 run, both towns. Read `fallback`, `groups/list`, `runs/list`, `same-as-last`,
   `max tie`, `arg8>1 items/frame`, and DRAWSORT's `compares (C/I per item)`, `ties` and `ties arg8!=`. Expect
   `nan = ptr = 0` as before.
3. **A/B:** flip `DrawSort::kPlainSort` (`draw_sort.h`) and rebuild perf. Run COMPPROF=0 on a CLEAN machine with
   Afterburner, RTSS and RTSSHooksLoader64 stopped, interleaved pairs, heavy and compact.
   - **The A/B column:** `draw_ms_mean`, plus fps.
   - **Must not move:** sim ms / iteration, the resave golden. The sort is render-only and the save never sees
     draw items.

## 7. Prediction

| | compares per item | sort share of main-thread ops |
|---|---|---|
| before (DRAWSORT keyed, heavy) | 13.6 (all 5-word) | ~15% (KeyLess 7.2, Introsort 2.8, BuildKeys 2.4, SortKeyed 0.9, selection 0.7, DrawItemLess 1.2) |
| after, if `groups/list` is 100-400 | **5-6** (about 2.5 four-word probe / prefix compares + about 3 one-word segment compares) | ~8-9% (the 10.7% compare block becomes about 4-5%; the key build, the gather and the under-32 lists stay; hashing and placement add about 1.5-2%) |
| after, if `groups/list` is close to `items/list` | 13-15 | no gain, slightly worse: switch off |

In wall time, DRAWSORT's -40..-60% op prediction became -0.1..-2.5% of draw ms. Scaled the same way, expect draw
ms **-1 to -3%** in the heavy town and fps +1-3% uncapped. Nothing moves at 60 Hz vsync. `fallback` should read
0. Any fallback costs that list one extra keyed sort.

## 8. Doubts

- **The argument rests on the consumer's read set.** If a future port adds a reader of `_sortKey`, `_flags` or
  the pads to the sorted lists, the plain path stops being exact. The shadow's consumer view would also have to
  grow. Section 1.2's grep is the list to recheck.
- **The algorithm choice is a guess.** `groups/list` decides whether it pays. The hashing adds a 4-word compare
  per item and a 32 KB table fill per list, so on lists with few repeated prefixes it is slower than the keyed
  skeleton. The census will say.
- **The hash is not adversarially safe.** Linear probing with a multiplicative mix over pointer bits is fine for
  heap addresses. A pathological collision pattern would make step 1 quadratic. It cannot make the result wrong.
- **The `-1` instance counts are not decoded.** I do not know what `_maxInstances` the trees, citizens and crops
  get. The `arg8>1 items/frame` field measures the effect directly.
- **`same-as-last` is a hash of the key array, not of the items.** A skip (option (c)) would also need `_arg8`
  and the input bytes to repeat. The figure is an upper bound on the hit rate.
- **The shadow's verdict changed from bytes to the consumer view.** That is weaker for DRAWSORT's keyed path. The
  `shadow byte-only` count with `kPlainSort = false` restores the byte check: it must be 0.

## 9. Files

| file | change |
|---|---|
| `src/engine/graphics/draw_sort.h` | `kPlainSort`, `kPlainMaxCount`, `SortPlain`, `SortFastExact`, `ConsumerEqual`, the census counters, the banner's SORTCMP paragraph |
| `src/engine/graphics/draw_sort.cpp` | `SortPlainImpl`, the census (runs, repeat hash, max tie, `_arg8 > 1`, plain / fallback / groups), the consumer-view shadow |
| `src/diag/stress_harness.cpp` | the `draw sort/cmp:` row |
| `tests/unit/draw_sort_test.cpp` | the bit-exact test on `SortFastExact`; four new tests; the shadow test's plain / fallback checks |

## 10. Static gate (after `git merge main`, merge `00ff0b5fb`)

The merge brought CULLROUTE and the S50 IBS docs. It auto-merged `stress_harness.cpp` with no conflict. The
reader grep of section 1.2 was rerun on the merged tree, with the same result.

| check | result |
|---|---|
| `tools/build.sh build perf --jobs 2` | OK |
| `tools/build.sh build debug --jobs 2` | OK |
| `tools/build.sh unit perf --jobs 2` | 1148 tests, 0 failures |
| `python tools/abi_audit.py` | OK |
| `tools/acceptance/acceptance.sh` | 0 lines off target |
| `python tools/castmetric.py src` | total 2 (no new cast) |
| warnings in the touched files | none |

Nothing else was run: no game, no stress town, no shadow, no A/B.

## Results (coordinator, 2026-10-13 evening) -- SHIPPED ON (`DrawSort::kPlainSort = true`)

Gate SORTCMP 0 problems (merge `724231814`, unit 1153 / 0, resave 5/5 at the golden). Shadow `DrawItemSort`
(COMPPROF=4, consumer view, mods parked): heavy faithful 148,921 lists / 0 mismatches, heavy with draw_gather +
render_culling 116,385 / 0, compact 262,077 / 0; **`shadow byte-only` 0.00 per frame in every run**: the plain
path's output was byte-identical to retail's skeleton on every list, not merely consumer-equal. The census rows:

| town | sorts/frame | items/frame | compares/item (was 13.6) | plain | fallback | groups/list | runs/list | same-as-last | max tie | arg8>1 items/frame |
|---|---|---|---|---|---|---|---|---|---|---|
| heavy faithful | 24.1 | 54,977 (max list 3,300) | 9.5 | 24.1 | 0 | 149 | 856 | 86.8% | 0 | 53,333 |
| heavy + draw_gather | 17.5 | 36,610 | 9.0 | 17.5 | 0 | 158 | 842 | 81.4% | 0 | 35,350 |
| compact | 40.6 | 52,862 (max 1,985) | 7.6 | 40.6 | 0 | 160 | 532 | 91.0% | 0 | 49,658 |

`groups/list` 149-160 on 2,000-3,300-item lists (the prediction's 100-400). Ties 0 even with 50k instanceable items
per frame (the instanced batches are of distinct geometries, so the geometry term separates them). The compares
count (hash probes included) fell to 7.6-9.5 per item.

A/B series ab2 (CLEAN, overlays stopped, interleaved x2, faithful, speed 10; leg "xform" = the merged tree with
kPlainSort off (and XformMemo on, which is neutral, see XFORM.md), leg "sort" = the same plus kPlainSort):

| town | draw ms off | on | delta | fps off | on |
|---|---|---|---|---|---|
| heavy | 1.72 / 1.71 | 1.37 / 1.38 | **-20.3% / -19.3%** | 474 / 484 | 608 / 602 |
| compact | 1.04 / 1.05 | 0.82 / 0.84 | **-21.2% / -20.0%** | 861 / 855 | 1079 / 1058 |

Far above the predicted 1-3%: the saving is the whole cost of the keyed introsort (its 40-byte key swaps and
mispredicted compares, ~0.35 ms/frame at 55k items), not the compare count alone. Sim columns unchanged. ON.

**Follow-up on record, not taken (option (c)):** `same-as-last` reads 81-91%: in 4 of 5 sorts the key array is
byte-identical to the same list's previous frame (pinned camera, static objects, mesh `_sortKey` = 0). A memo of
the previous frame's permutation keyed on the key-array hash would skip 80-90% of the remaining sort work; exact
iff the hash is replaced by a full key-array compare (O(n) words, cheaper than any sort). Worth a block.

