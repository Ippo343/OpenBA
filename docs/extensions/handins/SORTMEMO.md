# SORTMEMO -- skip the draw-item sort when a list's input repeats

Block SORTMEMO, 2026-10-13. The follow-up SORTCMP left on record (its option (c)): `same-as-last` read 81-91%,
so in 4 of 5 sorts the key array was byte-identical to the same buffer's previous frame. This block builds the
memo. It is faithful perf work: no registry row, no settings key, no `BANISHED_*` flag, no `Opt()` field, no
getenv, no door. It has one `static constexpr` kill switch, `DrawSort::kSortMemo` in
`src/engine/graphics/draw_sort.h`. **This block ran the static gate only.** The coordinator runs the census, the
shadow and the A/B.

## Short version

- Per item buffer, the memo keeps the previous sort's **input** bytes, **output** bytes and **permutation**.
- The next sort of that buffer compares the fresh input with the stored input, front to back, and stops at the
  first differing byte outside the pads.
  - **Byte-equal:** the stored output is copied over the list. Nothing else runs (no `BuildKeys`).
  - **Equal except the two pad dwords:** the stored permutation is applied to the fresh records, carrying the
    fresh pads.
  - **Otherwise:** a miss. The list is sorted as today and the entry is rewritten.
- The memo is exact byte for byte, pads included, not just consumer-exact. The output is what a fresh sort of the
  same input would leave.
- **Choice: (i), the raw-input memo, with the pads masked out of the hit test.** (ii), the key memo, is not
  built. The pads are stack garbage (section 1.4), so their stability cannot be argued from the code. A pad-masked
  compare needs neither the key build nor (ii)'s tie / `_arg8` argument.
- Prediction: draw ms **-2 to -8%** in both towns. It could also be neutral, because the memo's hit moves about 224
  bytes per item and the plain sort it skips is not much more (section 4).

## 1. Exactness

### 1.1 The sort is a pure function of the non-pad bytes (and DAZ)

I read every path that `SortFastPath` can take:

- **`BuildKeys`** reads `_flags`, `_sortKey`, `_material`, `_instance`, `_arg5`, `_arg10`, `_userData` and
  `_geometry`. It reads no pad. `SortKeyBits` uses `x == 0.0f`, which depends on MXCSR.DAZ for a denormal.
- **`SortPlainImpl`** reads the keys, then `_arg8` in the tie scan. It reads its scratch only after writing it:
  - The table is `std::fill`ed per call.
  - `grp[i]` is written for every `i < count`.
  - `gks[g]` is written for every `g < groupCount`.
  - `off` is written for every group.
  - `seg[i]` is written for every `i`.
  - `ItemScratch` is written before it is copied back.

  The hash only chooses group ids. The groups are then put in order by `std::sort` of distinct prefixes, a
  strict order, so the result does not depend on the ids. The segment sort orders on the `(geometry, index)`
  words, which are all distinct. The fallback decision is a function of the keys and `_arg8`.
- **`SortKeyed`** runs retail's skeleton on the keys (`KeyLess`), then gathers by the index each key carries.
- **`SortItemsInline`** runs retail's skeleton with `DrawItemLess`, which reads the eight compared terms, and with
  `DrawItemSwap`.
- **Path selection** depends on the count, NaN, pointers at or above 2^48 and `kPlainMaxCount`. All are functions
  of the bytes.

The only state outside the list that any of these reads is MXCSR.DAZ. It decides whether a denormal `_sortKey`
equals zero in `SortKeyBits` and in `DrawItemLess`'s `>`. FTZ only affects arithmetic results, and the sort makes
none. The entry stores DAZ, and a different DAZ is a miss.

**Every path moves whole records.** `DrawItemSwap` copies 56 bytes, and both gathers copy `data[idx]` whole. So
the output is the input permuted, and the permutation is a function of the non-pad bytes.

### 1.2 The hit conditions

The stored triple (in, out, perm) always comes from one real sort: `out[i] == in[perm[i]]`.

- **Byte-equal hit.** The fresh input equals `in` in every byte, and the count and DAZ are equal. A fresh sort
  then makes the same decisions on the same bytes and leaves exactly `out`. The memo copies `out`.
- **Pad-only hit.** The fresh input equals `in` everywhere except bytes `+0x24..+0x27` and `+0x34..+0x37`. No path
  reads those bytes, so a fresh sort makes the same decisions and applies the same permutation, now to the fresh
  records: `fresh[perm[i]]`. The memo does exactly that. This needs a stored permutation, which exists after the
  keyed and plain paths:
  - keyed: the key indices left in `KeyScratch`;
  - plain: `SegScratch & 0xFFFF`.

  The inlined skeleton swaps the items themselves and leaves no permutation. An inline-path entry is therefore
  served only on a byte-equal hit; a pad-only difference is a miss. NaN or pointer lists never occur in the
  census (`nan = ptr = 0`).
- **Everything else is a miss**, sorted exactly as without the memo.

The shadow's `shadow byte-only` must therefore stay what it is without the memo: 0 in SORTCMP's runs.

### 1.3 The identity of a list

The memo looks up its entry by the list's item buffer pointer, the same identity the census uses.

- `Renderer::Reset` sets each group's sizes to 0 and keeps the capacity, so a group's buffer lives across frames.
- `DrawItemArrayResize` moves the list to a new buffer only when it outgrows its capacity. The new buffer is a
  new entry, and the old one is swept.

**The pointer is only where to look, never part of the proof.** A hit requires the full compare of every
non-pad byte, the count and DAZ. Suppose a freed buffer is handed to another list, or two groups trade
addresses. A hit then means the new list's input really equals the stored input outside the pads. Section 1.2's
argument does not care where the input came from, so the stored output is the right output for that input. A
buffer that another list reuses cannot produce a false hit.

### 1.4 The pads, and why the memo masks them

`Renderer::AddDrawItem` (`wave9_lane_gfx.cpp:376`) builds the record as `RendererDrawItem rec;`, which is
deliberately not value-initialised (RULE B5-13: retail's record is a raw stack frame). It stores nine fields.
**`+0x24` (`_pad24`) and `+0x34` (`_pad34`) are never stored.** `DrawItemArrayResize` then `memcpy`s all 56 bytes
into the list (`wave9_lane_gfx.cpp:136`).

So the pads hold whatever the caller's stack slot last held. That is the leftovers of earlier deeper calls from
the same call path:

- the previous `AddDrawItem`'s own frame, whose pads were never written either;
- `MaterialInstance::RenderTextures` on the rendered-texture branch;
- a growing `DrawItemArrayResize`;
- whatever `MeshInstance::AddDrawItems` and the cull walk called before.

Whether those bytes repeat from frame to frame depends on GCC's frame layout and on the per-frame values that
pass through those slots: camera-relative distances, LOD selection, animation time. They cannot be argued from
the source. A raw memo whose hit test included the pads could therefore collapse to a 0% hit rate for no
semantic reason. Masking them costs one AND per item and keeps the exactness argument trivial (section 1.2).
(ii), the key memo, would also survive varying pads, but it pays `BuildKeys` (56 bytes read and 40 written per
item) on every sort and needs the tie / `_arg8` argument on a hit. The masked raw compare needs neither.

## 2. The census and what to expect

There is a new third row under SORTCMP's. It is per frame unless the field says otherwise:

```
[COMPPROF]   draw sort/memo: census raw=R% masked=M%  memo hit=H pad-hit=P miss=X new=N stores=S full=F /frame  cmp=C MB/frame  stored=W MB/frame  entries=E held=B MB
```

| field | definition |
|---|---|
| `census raw` | over SORTCMP's `checked` lists: the share whose 56 bytes per item hash equal to the same buffer's previous sort. The ceiling of the byte-equal hit |
| `census masked` | the same, hashing everything except the two pad dwords. The ceiling of hit + pad-hit |
| `memo hit` | lists served by copying the stored output |
| `pad-hit` | lists served by applying the stored permutation |
| `miss` | an entry existed but the input differed (or the count or DAZ did): sorted, entry rewritten |
| `new` | no entry for the buffer: sorted, stored |
| `stores` | entries written after a sort (= miss + new - full) |
| `full` | sorts not stored because the table was full of recent entries (expected 0) |
| `cmp` | MB of fresh input compared per frame (each side), hits and misses |
| `stored` | MB written into entries per frame (input + output + permutation) |
| `entries` / `held` | the table at the end of the window |

The census is taken **before** the memo, whatever `kSortMemo` says. With the switch off it still gives `raw` and
`masked`. That is the census the brief asked for: build the counters, then take the row. So that a memo hit does
not hide a list from the census, SORTCMP's `runs/list` and `same-as-last` moved there too. They now average over
`keyable` lists (all keyed lists with the switch off, so the figures read as before). The tie / instancing census
(`ties`, `max tie`, `arg8>1`) runs on every output, hits included. `keyed`, `plain`, `fallback`, `groups` and
`compares` count only the lists that were actually sorted. With the memo on, `compares/frame` falls by the hit
share. That fall is the point, not a regression.

**What I expect (I cannot run the game):**

- **`masked`:** about SORTCMP's key `same-as-last`, 81-91%, slightly below it at most. A masked repeat is a key
  repeat plus equal `_arg8` (per mesh, constant) and equal raw `_sortKey` bits (mesh keys are 0).
- **`raw`:** unknown. It is the one thing this block cannot predict.
  - If the pad slots carry the same garbage every frame, `raw` is close to `masked` and most hits are memcpy hits.
  - If they vary, `raw` is close to 0, and `pad-hit` carries the memo at a higher cost per hit (section 4).
- **The memo row:** `hit + pad-hit` about `masked` x sorts of 32 items or more. `new` about 0 in steady state.
  `full` 0. `entries` about the lists per frame (24 heavy, 41 compact) plus a few in the sweep's window.
  `held` about 116 B x items/frame: about 6.4 MB heavy and 6.1 MB compact.

## 3. Design

- **Where:** `SortFastSwitch` (`draw_sort.cpp`), the fast path's single entry, which both `Sort` (the renderer's
  two callers, through `RtGraphics_Renderer_DrawItemSort`) and `SortFast` use.
  - `if constexpr (kSortMemo)` routes lists of `kMemoMinCount` (32) items or more to `SortMemoized`. Below that,
    the lookup costs more than the sort.
  - `SortFastExact` (the DRAWSORT oracle) and the new `SortFastNoMemo` (the memo tests' oracle) never touch the
    memo.
- **Entry:** `count`, `daz`, `havePerm`, `lastUse`, and three grow-only vectors:
  - `in` (56 B per item);
  - `out` (56 B per item);
  - `perm` (`uint32_t`, 4 B per item).

  Entries live in an `unordered_map<const void*, MemoEntry>`, looked up once per sort (17-41 sorts per frame).
  Main thread only, like every scratch buffer in the file.
- **The compare** (`CompareInput`) loads seven qwords per side per item. It ORs the XORs of qwords 0-3 and 5 with
  the low halves of qwords 4 and 6, and collects the high halves (the pads) separately. It returns at the first
  item with a non-pad difference.
  - `static_assert`s pin the layout this relies on: little-endian, 56 bytes, `_arg8` / `_pad24` at
    `+0x20` / `+0x24`, `_arg10` / `_pad34` at `+0x30` / `+0x34`.
- **Miss:** the input is copied into the entry, then `SortFastPath` sorts, unchanged. `SortFastImpl` now returns
  which path ran, so the memo knows where the permutation is. Then the output is copied and the permutation
  stored.
- **Bounds.**
  - At most `kMemoMaxEntries` = 256 entries.
  - Every `kMemoSweepEvery` = 1024 memoised sorts, entries unused for `kMemoMaxAge` = 1024 sorts are freed,
    vectors included. At 17-41 sorts per frame that is about 25-60 frames.
  - When the table is full, entries unused for `kMemoFullAge` = 64 sorts go first. If none is that old, the list
    is sorted and not stored (`full`).

  Memory is therefore bounded by 256 times the largest list a live buffer holds, and in practice by the lists
  per frame times their lengths (about 6 MB). A list that stops appearing is freed within about 1024 sorts.

## 4. Cost model

Bytes per item, by case:

| case | memory traffic per item | access pattern |
|---|---|---|
| byte-equal hit | compare 56 (list) + 56 (stored in), copy 56 read (stored out) + 56 write (list) = **224 B** | four sequential streams |
| pad-only hit | compare 112, perm 4, gather 56 random read + 56 write (scratch), copy back 112 = **~340 B** | gather random within the list (one list, at most 185 KB, L2) |
| miss | compare up to the first difference (usually a few items), store in 112, out 112, perm 4 + scratch read 8-40 = **~240-270 B on top of the sort** | sequential |
| the plain sort it skips | `BuildKeys` 56 read + 40 write; hash pass 40 read + a 4 x 2n-byte table fill and probes; placement 40 read + 8 write; segment sorts 8 B words; gather 56 random read + 56 write + 112 copy = **~420 B, plus 7.6-9.5 compares per item, the hash multiplies and the segment sorts' mispredicts** | partly random |

Per frame, at a hit rate h (SORTCMP's 81-91%):

- **Heavy:** 55.0k items/frame (max list 3,300). Hits move 0.87 x 55k x 224 B = **10.7 MB**. Misses add about
  0.13 x 55k x 250 B = 1.8 MB of stores on top of their sorts. The memo holds 6.4 MB.
- **Compact:** 52.9k items/frame (max list 1,985). Hits move 0.91 x 52.9k x 224 B = **10.8 MB**. Misses add
  about 1.2 MB. The memo holds 6.1 MB.
- **Without the memo:** the sort touches about 55k x 420 B = 23 MB per frame, with its compute.

**The compare is sequential, unlike XFORM's table.** XFORM's memo hashed 55k requests per frame into a 1.4 MB
direct-mapped table. Each lookup was a dependent L2 / L3 miss at a random address, so it paid latency per
request, and that latency ate the ALU it saved (XFORM.md Results). Here every list's memo is three contiguous
arrays, read front to back in step with the list. The hardware prefetchers stream them, so the cost is
bandwidth, not latency: about 32 B/cycle from L2 and 16 B/cycle or more from L3 on this core.

- The fresh list is hot: the gather just wrote it.
- The stored copies are one frame old: 6.4 MB, in L3 on the 96 MB X3D part.
- On a 32 MB L3 part they still fit, but they compete with the frame's other traffic. A miss to DRAM would make
  the hit about memory-bound: 11 MB per frame at about 20 GB/s is about 0.5 ms. That is worse than the sort, and
  the A/B on this box cannot see it (see Doubts).

**What it replaces, in time.** SORTCMP cut draw ms by 0.35 ms per frame (heavy, 1.72 to 1.37 ms) by replacing
the keyed introsort, about 11.6 of DRAWSORT's 15 IBS op points. Scaling by those points:

- `BuildKeys` (2.4 points) is about 0.07-0.09 ms.
- The plain sort plus its gather is about 0.1-0.15 ms.

So I estimate the whole remaining sort at **0.2-0.3 ms per frame heavy**, about 15-20% of draw, and about 0.15-0.2
ms compact. A byte-equal hit at 10.7 MB per frame streaming from L2 / L3 is about 0.07-0.15 ms. A pad-only hit
costs about 1.5 times that.

## 5. Kill switch, shadow and row

- **`DrawSort::kSortMemo`** (`draw_sort.h`, default `true`).
  - `false` removes the memo's call site under `if constexpr`. `SortFastSwitch<false>` is then exactly
    `SortFastPath<false>`, the code that ran before this block.
  - With COMPPROF on, the census moved (section 2), so `SortFastSwitch<true>` differs in where it counts, not in
    how it sorts.
  - The unit suite passes with the switch off: 1151 / 0, the memo tests print `kSortMemo off: skipped`.
- **Shadow (`BANISHED_STRESS_COMPPROF=4`), unchanged code.** `Sort` copies the list, runs `SortReference` on the
  copy, runs the fast path (memo included) on the list and compares on the consumer view. A memo hit is checked
  exactly like a sort. A corrupted entry is caught: unit test (d) reports `count << 32 | 0`, and the list ends in
  retail's order.
  - Read `[VERIFY] DrawItemSort checks=N mismatches=M`, **expected M = 0**.
  - `shadow byte-only` must stay 0: the memo is byte-exact, so it adds no byte-only case.
- **The row:** COMPPROF >= 1, the `draw sort/memo:` line above.
  - The counters are bumped once per sort call, never per item (the GATHERPF lesson). `memoCompared` and
    `memoStored` add one product per call.
  - The census hashes (raw, masked) run only under COMPPROF, in `CensusInput`, outside the memo's code.

## 6. Unit tests (`tests/unit/draw_sort_test.cpp`, all pass)

- **(a) `draw_sort_memo_hit_is_a_fresh_sort`:** 400 lists of 32-4,000 items, half gather-shaped, half DRAWSORT's
  random items with NaN keys and high pointers mixed in. Planted full ties with fresh pads, and in a quarter of
  the lists a differing-`_arg8` tie (the fallback case). The run saw 395 lists with ties, 53 fallbacks and 187
  inline-path lists. For each list:
  1. The first sort stores.
  2. The same input in the same buffer is a byte-equal hit: 400 / 400.
  3. Changed pads give a permutation hit on keyable lists (213) and a miss on inline-path lists (187).

  Every output is byte-equal to `SortFastNoMemo` on a copy.
- **(b) `draw_sort_memo_one_byte_forces_a_miss`:** two 300-item lists, gather-shaped and random. For each of the
  56 byte offsets, four trials flip one bit in a random item.
  - Each of the 48 non-pad offsets is a miss: 384 / 384.
  - Each of the 8 pad offsets is a permutation hit: 64 / 64.

  Every output is byte-equal to a fresh sort. A shorter count in the same buffer is a miss.
- **(c) `draw_sort_memo_buffer_reuse_cannot_hit`:**
  - Another list in the same buffer is a miss, sorted correctly.
  - So is that list with the first list's first record planted.
  - The first list in a new buffer is `new`, then a hit there.
  - The bound: 400 distinct buffers never hold more than `kMemoMaxEntries` entries. After
    `kMemoSweepEvery + kMemoMaxAge + 1` sorts of one buffer, exactly one entry is left.
- **(d) `draw_sort_memo_shadow_catches_a_corrupt_entry`:** under `g_verify`:
  - A sound hit matches.
  - `MemoCorruptForTest` swaps the first and last stored records and permutation slots. The next byte-equal hit
    is reported as a mismatch at item 0, and the list ends byte-equal to retail.
  - A pad-only hit through the corrupted permutation is also reported, and the list is also restored.

Two mutations were checked and both reverted:

| mutation | tests that fail |
|---|---|
| a pad-only difference served from the stored output (pads ignored) | 3 (a, b, d) |
| the compare skips qword 5 (`_userData`) | 1 (b) |

## 7. Build results

`git submodule update --init` was needed first: the worktree had an empty `third_party/SDL`.

| check | result |
|---|---|
| `tools/build.sh build debug --jobs 2` | exit 0 |
| `tools/build.sh build perf --jobs 2` | exit 0 |
| `tools/build.sh unit perf --jobs 2` | exit 0, **1151 tests, 0 failures** (1147 + 4) |
| the same with `kSortMemo = false` | exit 0, 1151 / 0 (memo tests skipped) |
| warnings in the touched files (`draw_sort.*`, `stress_harness.cpp`, `draw_sort_test.cpp`) | none |
| `python tools/abi_audit.py` | OK |
| `tools/acceptance/acceptance.sh` | 0 lines off target |
| `python tools/castmetric.py src` | total 2 (no new cast) |

Nothing else was run: no game, no stress town, no shadow, no resave gate, no A/B.

## 8. How to measure (coordinator)

The towns are SORTCMP's: heavy `tools/stress_run.sh --map large --citizens 1600 --sets 3 --spread --speed 10`,
compact `tools/stress_run.sh`. Park the mods.

1. **Census and row, COMPPROF=1, both towns, switch ON.**
   - Read the `draw sort/memo:` line: `census raw` and `masked`, then `hit`, `pad-hit`, `miss`, `new`, `full`,
     `cmp`, `stored`, `entries` and `held`.
   - Also read `draw sort:` `compares/frame`. It should fall by about the hit share.
   - If `raw` is about 0 while `masked` is high, the pads vary, and every hit is a permutation hit.
   - For the census alone, take one run with the switch OFF as well. `raw` and `masked` are the same census
     either way.
2. **Shadow, COMPPROF=4, both towns, switch ON.** Expect `[VERIFY] DrawItemSort ... mismatches=0` and
   `shadow byte-only` 0.00. Every other shadow must stay at 0.
3. **A/B:** `DrawSort::kSortMemo` false against true, perf exe, faithful profile, speed 10, heavy and compact.
   COMPPROF=0, on a CLEAN machine with Afterburner, RTSS and RTSSHooksLoader64 stopped, in interleaved pairs.
   - **The A/B column:** `draw_ms_mean`, which should move down, plus fps.
   - **Must not move:** sim ms per iteration and the resave golden. The sort is render-only, and the save never
     sees draw items.

## 9. Doubts

- **The gain is small and could be zero.** After SORTCMP the remaining sort is, by my estimate, 0.2-0.3 ms of
  the heavy town's 1.37 ms draw (15-20%) and about 0.15-0.2 ms of the compact town's 0.82 ms. At a 0.87 hit rate,
  with a hit costing about 0.3-0.5 of the sort it replaces, I expect **-0.05 to -0.15 ms per frame: draw ms -2 to
  -8%** in the heavy town and similar or slightly more in the compact town. If the remaining sort is cheaper than
  I think (the plain path is mostly sequential too), the memo's 224 B per item eats the gain: neutral. Per the
  house rule, a neutral or worse A/B means the body is removed in a cleanup block, not left off.
- **The pads may make every hit a permutation hit.** That case costs about 340 B per item with a random gather.
  It is still cheaper than `BuildKeys` + hash + segments + the same gather, but by less. `census raw` against
  `masked` says which case this is.
- **L3 size.** The memo holds about 6 MB that must survive one frame. On the 96 MB X3D box that is free. On a
  16-32 MB part, if the frame's other traffic evicts it, the hit compare runs from DRAM. 11 MB per frame at about
  20 GB/s is about 0.5 ms, which is worse than the sort. This box cannot measure that. The working set is 116 B x
  items per frame (6.4 MB heavy), and it stops fitting comfortably below about 32 MB of L3.
- **DAZ.** I treated MXCSR.DAZ as the sort's only external state and stored it per entry. If something else ever
  changes the compare semantics, for example a future `/fp` model change, the purity argument must be redone.
- **The census moved.** `runs/list` and `same-as-last` are now counted over keyable lists before the memo, not
  inside the keyed path. With the switch off the figures are the same lists. With it on, `keyed`, `plain`,
  `groups` and `compares` count sorted lists only, so do not read their fall as a change in the town.
- **The pointer identity is a heuristic for the hit rate only.** A town whose lists change buffers often (growth
  past capacity) reads `new` instead of `hit` until the capacity settles. `Reset` keeps capacity, so that should
  be the first frames only.

## 10. Files

| file | change |
|---|---|
| `src/engine/graphics/draw_sort.h` | `kSortMemo`, the memo bounds, `SortFastNoMemo`, `MemoClear` / `MemoEntries` / `MemoBytesHeld` / `MemoCorruptForTest`, the census and memo counters, the banner's SORTMEMO paragraph |
| `src/engine/graphics/draw_sort.cpp` | `CensusInput` (runs, key / raw / masked repeat hashes, moved out of the keyed path), `CountOutput`, `SortFastImpl` returns its `Path`, `SortFastPath`, the memo (`MemoEntry`, `CompareInput`, `SortMemoized`, the sweep), `SortFastSwitch` routing |
| `src/diag/stress_harness.cpp` | the `draw sort/memo:` row; `runs/list` over keyable lists |
| `tests/unit/draw_sort_test.cpp` | four tests (a)-(d) |
