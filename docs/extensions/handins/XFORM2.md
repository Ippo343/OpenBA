# XFORM2 -- the world-matrix memo, retried at the two sites that repeat

Block XFORM2, 2026-10-13. A scoped-down retry of XFORM (`XFORM.md`: exact, but its A/B was
neutral-to-worse, and CLEANUP row 1 removed it). This is faithful perf work. It adds no registry row,
no flag, no `Opt()` field, no `getenv` and no door. The kill switch is
`static constexpr bool XformMemo::kEnabled = true` in `src/engine/graphics/xform_memo.h`.
**This block did not run the game.** The coordinator runs the shadow, the row and the A/B.
**This is a probe with a low prior.** Read the Doubts (section 9) before you spend a clean-machine
slot on it.

## Short version

- Only two sites are memoised:
  - `WorldGeometry::Draw` (`world.draw`) uses **one 64-byte slot** and no table.
  - `MeshInstance::Draw`'s pass == 1 pose block (`mesh.draw`) uses a **1024 x 64-byte
    direct-mapped table** (64 KB).
- Every other XFORM site is back on its retail arm: mesh.git, edge.git, edge.draw, geom.git,
  part.git and part.draw.
- A slot is one cache line. It holds a 16-byte key and the 12 matrix cells that AsMatrix varies.
  The key is the pair address, the t bits and a 32-bit epoch. The other 4 cells are AsMatrix's
  constant column, rebuilt on a hit.
- The proof carries over from XFORM.md and gets simpler: the two sites no longer share storage.
- The shadow `ItemTransform` and the `/xform` row are restored.
- With `kEnabled = false`, the three touched TUs compile to **exactly** main `dffc78154`'s objects:
  0 `objdump -dr` lines and no memo symbol. Section 7 has the proof.
- **What it keeps:** 1,750 of XFORM's 55,546 requests/frame in the heavy town (3.2%) and 3,759 of
  53,936 in compact (7.0%). The upside is small: at most ~0.03 ms/frame heavy and ~0.06 ms/frame
  compact, and probably much less (section 9).

## 1. What differs from XFORM

| | XFORM | XFORM2 |
|---|---|---|
| sites | 8 (all `GetInstanceTransform` and `Draw` pose spellings) | 2: `world.draw`, `mesh.draw` |
| storage | one shared 16,384-slot table of 88-byte slots (1.4 MB, 2 lines per slot, heap) | `g_world`: 1 slot (64 B). `g_table`: 1024 slots x 64 B (64 KB, static, 64-aligned). The two are separate. |
| slot | key 16 B + epoch 8 B + Matrix 64 B | key 16 B (pair, t bits, epoch32) + 12 cells (48 B) = **one line** |
| hash | splitmix64 finaliser (2 multiplies) | Fibonacci: 1 multiply, top 10 bits. `world.draw` has no hash. |
| hit test | key memcmp + separate epoch compare | one 16-byte compare (the epoch is in the key) |
| switch off | sites still went through `World`'s lambda, and the table's vector was linked (static init + `__tcf`) | sites take retail's text under `if constexpr` / constant selects. Objects identical to main. |
| cross-site sharing | yes: mesh.git and mesh.draw shared keys, so claim A had to cover 5 spellings | none: each storage is filled and read by one spelling |

## 2. The sites: kept and dropped, with the numbers

These numbers come from XFORM's `/xform` row (heavy faithful `perf/results/20261013_162747_sh2_heavy_faithful`,
compact `..._163214_sh2_compact_faithful`), per frame:

| site | heavy req | heavy hits | req/key | compact req | compact hits | XFORM2 |
|---|---|---|---|---|---|---|
| mesh.git | 52,358 | 26,949 | 2.06 | 48,478 | 26,180 | **dropped** (retail arm) |
| part.git | 975 | 0 | 1.00 | 1,180 | 0 | dropped |
| part.draw | 463 | 463 (on part.git's keys) | -- | 519 | 519 | dropped |
| edge.git, edge.draw, geom.git | 0 | | | 0 | | dropped |
| **mesh.draw** | **334** | 277 | 5.84 | **1,596** | 1,206 | **kept** (table) |
| **world.draw** | **1,415** | 1,406 | 158 | **2,163** | 2,150 | **kept** (single slot) |

- **Removed from the memo:** 53,796 requests/frame heavy (96.8%) and 50,177 compact (93.0%). They
  now pay nothing: no hash, no lookup, no counter.
- **Kept:** 1,750 requests/frame heavy and 3,759 compact.
- **Hit ceiling:** heavy ~1,683 (world 1,406 + mesh.draw <= 277), compact ~3,356 (2,150 + <= 1,206).
- mesh.draw's XFORM hits included hits on keys that mesh.git claimed first, because the two shared a
  MeshInstance `_transform`. On its own its hits are at most that number. Its requests repeat per
  shadow map and per light and list, so most of them should remain.
- XFORM's world.draw showed 9 keys/frame over 6 scopes. Those were re-claims after mesh.git keys
  evicted its slot in the shared table. There is one game WorldGeometry
  (`gameupdateloop_load.cpp:932`; `menuscene.cpp:220` is the menu's). With a private slot, expect at
  most one key per scope that draws terrain, and ~0 `world.draw` evictions.

## 3. Slot layout and size

```
struct alignas(64) Slot {            // 64 bytes, one cache line
    Key   key;                       // 16: const void* pair; uint32 t bits; uint32 epoch
    float cells[12];                 // 48: m[0..2], m[4..6], m[8..10], m[12..14]
};
```

**The constant column.** `Transform::AsMatrix` (`transform.cpp:211`) always writes these cells:
- `out[3] = out[7] = out[11] = 0.0f` (bits 0x00000000), before `float out[16] = {0}`;
- `out[15] = RtMathConst::kOne`.

The other 12 cells depend on the input. `Load` rebuilds the 4 constant cells from `Matrix()` (it zeroes
all 16) and `m[15] = kOne`. The unit test checks those bits on 2,000 AsMatrix outputs, and the shadow
compares all 16 cells live.

**Epoch in the key.** `g_epoch` is 32 bits and is never 0. 0 marks an empty slot, since the table is
zero-initialised. `BeginScope` and `EndScope` each bump it. If it wraps to 0, every slot is cleared
once and the epoch restarts at 1. That costs one 64 KB clear per 2^32 epochs, roughly 4 days at 12
epochs/frame and 1000 fps. So a slot from a previous scope can never compare equal. A scope change
empties both stores without a clear.

**Size: 1024 slots (64 KB), not the 4096 (256 KB) in the brief.** The key count per scope is far
below XFORM's ~4.4k. That number was mesh.git.
- **Heavy:** mesh.draw makes 334 requests/frame over 6.03 scopes, which is <= 55 keys per scope.
  At ~5.8 req/key that is ~10.
- **Compact:** 1,596 requests over 10.15 scopes, which is <= 157 keys per scope, ~39 at 4.1 req/key.
- **Expected collisions per scope** (n^2 / 2m with m = 1024): ~0.05 heavy and ~0.7 compact. Even at
  the worst-case n = 157 it is ~12.
- **Evictions should therefore stay free of hit loss, as they were in XFORM.** The row shows
  per-site `evictions` next to `keys` to check that. In the unit test (300 keys per scope, well above
  the live ~10-40), the Fibonacci hash spreads 1024 pointers at the 0x38 pair stride over 830 slots.
  A random map would use ~647.
- **The cache footprint is the lines touched, not the table size.** That is <= ~160 lines (10 KB)
  per scope plus the world line. 64 KB is 16 pages, which fits the L1 dTLB. 4096 slots would cut
  collisions from ~0.7 to ~0.2 per scope compact, at 4x the page span. `kSlotBits` is one constant if
  the row disagrees.
- The table is a zero-initialised inline variable. It has no dynamic init and no teardown, and it is
  not emitted when the switch is off. MinGW places it in a COMDAT `.data$` section, so it adds 64 KB
  of zeros to the image.

## 4. The proof (carried over from XFORM.md, with what changed)

- **Section 1.1, claim A (one function): no longer needed across sites.** Each storage has one
  spelling:
  - `g_world` is filled and read only by `WorldGeometry::Draw`, which uses `_transform.Lerp` then
    `AsMatrix`.
  - `g_table` is filled and read only by `MeshInstance::Draw`, which uses `LerpTransform` then
    `AsMatrix`. Both arms of its pose block use the same spelling.

  What remains of claim A is that one spelling on bit-identical inputs gives bit-identical output.
  That holds: there is no FMA or fast-math (`CMakeLists.txt`, x86-64 baseline), and MXCSR is fixed.
  The two spellings still agree (`xform_memo_site_spellings_agree`, 40,000 hostile cases), but that is
  kept as a record, not as a premise.
- **Claim B (a hit is the retail bytes): verbatim from XFORM.md, with the epoch moved into the key.**
  A hit is a stored 16-byte compare that includes the pair, the t bits and the current epoch. The
  epoch moves at pass-2 entry and at body exit (`XformMemo::Scope`, `wave27_lane_gfxbig.cpp`, after
  the `PASS 2` banner). So the claim and the hit fall inside one emit pass, where nothing writes or
  frees the pair.
- **Section 1.2, the census: carried over verbatim.** Only `Geometry::_transform` and the pose arrays
  matter here, and that is a subset of XFORM's table. The diff `9a85e586a..dffc78154` adds no writer.
  It contains CULLROUTE, SORTCMP, GATHERPF and CLEANUP, and its only new hit for `_nodes` /
  `_transform` / `SetTransform` / `Clean(` / `const_cast` is GATHERPF's prefetch of the scene grid's
  `_nodes` in pass 1, which is a read.
- **Section 1.3, pointer keys: verbatim.** Nothing frees a pair inside the scope. Across scopes the
  epoch empties both stores.
- **Section 1.4, threads: verbatim.** Every `Renderer::Draw` runs on the Context thread, and the state
  is plain globals.
- **Section 1.5, what stays exactly as before:**
  - Outside a scope, `Pose` and `World` count a decline and run the retail lambda.
  - The only reorder is MeshInstance::Draw's skinned arm: with the memo on, the lerp runs (on a miss)
    after the bones, together with the AsMatrix. Both are pure. EdgeMeshInstance is no longer touched.
  - **No guard was added.** The pair pointer is formed by the same unguarded reads as retail. On a hit
    the memo dereferences nothing of the pair.
  - `Context::SetTransform` / `SetTransformScale` get the same bytes in the same calls.

## 5. Shadow and row

- **Shadow `ItemTransform`** (`BANISHED_STRESS_COMPPROF=4`): on every hit, the site's own retail
  lambda runs on the live pair and all 16 cells are compared bit for bit. That covers the rebuilt
  constant column. The game is handed the REFERENCE. The detail is
  `(site << 16) | first differing cell`, with 0xffff for none, site 0 = mesh.draw and
  1 = world.draw. **Expected: 0 mismatches.**
- **Row `/xform`** (`BANISHED_STRESS_COMPPROF>=1`, after `/skin`):
  ```
  [COMPPROF]   /xform world-matrix memo (XFORM2) requests/frame=R hits=H (h%) keys/frame=K (R/K req/key)
    scopes/frame=S declines/frame=D evictions/frame=E; miss X ms/frame Y ns/key; hit Z ms/frame W ns/call;
    est. saved M ms/frame (upper bound)
  [COMPPROF]   /xform per site: mesh.draw R req H hits K keys (req/key) E evictions; world.draw ...
  ```
  - The counters are bumped once per request: calls, hits or misses, and an eviction on a miss.
  - `keys` counts computations, so a re-miss after an eviction is counted again.
  - With no eviction loss, `hits / requests` = `1 - keys / requests`.
  - `world.draw evictions` should be ~0, and `keys` should be about scopes that draw terrain.
  - `est. saved` is the misses' RDTSC price per key times the hits, less the hits' time. It is an
    **upper bound** (section 9).

## 6. Unit tests (`tests/unit/xform_memo_test.cpp`, 6 tests)

- **`xform_memo_table_slots_and_epochs`**:
  - Keys that differ only in t's sign bit, only in the pair, or only in the epoch do not compare equal.
  - `Store` / `Load` round-trips bit-exactly on 2,000 AsMatrix outputs, including the constant column.
  - `Lookup`: a miss fills, a hit answers, a live different key evicts (counted), and a stale epoch
    misses without counting an eviction.
  - The Fibonacci spread is 830 of 1024 slots.
  - A decline outside a scope runs the faithful body, for both entries.
  - The epoch wrap clears every slot and restarts at 1.
- **`xform_memo_site_spellings_agree`**: `LerpTransform` vs the real `InterpolatedState::Lerp` on
  40,000 hostile cases (signed zeros, denormals, 1e30, t = 0 / 1). Result: 0 differences.
- **`xform_memo_two_renderers_bit_identical`**:
  - Setup: the real `Pose` / `World`, table, slot, epoch and scope, over 30,000 pairs (more than
    `kSlots`). Each scope has 300 visible mesh pairs and 2,000 mesh requests, plus terrain runs in
    which a second or third WorldGeometry sometimes evicts the single slot. Between scopes, 401 pairs
    are rewritten in place. 3 seeds x 40 scopes.
  - Result: **300,137 requests, 256,901 memo hits, 7,177 evictions, 0 mismatches.**
- **`xform_memo_shadow_clean_on_faithful_run`**: every hit is checked, and 0 are reported bad.
- **`xform_memo_write_inside_scope_diverges`**: a pair rewritten inside a scope (a broken census)
  diverges. With the shadow on, the shadow reports it and the game still gets the reference
  (0 mismatches).
- **`xform_memo_world_single_slot`**:
  - 160 chunk items in one scope give 1 computation and 159 hits.
  - The next scope, after an in-place rewrite, gives a miss and then hits.
  - Two geometries alternating give 40 computations and 39 evictions, still bit-exact.

## 7. Build results and the switch-off object proof

- `tools/build.sh build debug --jobs 2`: exit 0. `build perf --jobs 2`: exit 0. There are no new
  warnings in the touched files. The one in `wave27_lane_gfxbig.cpp:146`, the log-stripped `what`
  parameter, is older.
- `tools/build.sh unit perf --jobs 2`: exit 0, **1153 tests, 0 failures** (1147 + 6).
- `python tools/abi_audit.py`: OK. `tools/acceptance/acceptance.sh`: 0 lines off target.
  `python tools/castmetric.py src`: **total 2**.
- Setup: the worktree needed `git submodule update --init` (`third_party/SDL`) before CMake would
  configure.

**Object proof (perf flavour), method as in CLEANUP.md section 2.**
1. With `kEnabled = false`, I built the perf exe and kept the objects of `meshinstance_draw.cpp`,
   `wave27_lane_gfxbig.cpp`, `geometry_draw.cpp` and `mesh.cpp`.
2. I wrote main `dffc78154`'s text of the three touched files over them (`git show`), rebuilt the
   same build directory with the same command, kept those objects, and then restored this branch's
   files.
3. I compared the objects with `objdump -d` and `objdump -dr`, the section sizes and `nm`.

| TU | `-d` lines | `-dr` lines | section sizes | `XformMemo` symbols |
|---|---|---|---|---|
| `meshinstance_draw.cpp` | 0 | 0 | identical | 0 |
| `wave27_lane_gfxbig.cpp` | 0 | 0 | identical | 0 |
| `geometry_draw.cpp` | 0 | 0 | identical | 0 |
| `mesh.cpp` (untouched) | 0 | 0 | identical | 0 |

- **Verdict:** with the switch off, the three sites compile to main's objects exactly, and no table,
  slot or counter is emitted. Only the diagnostics TU `stress_harness.cpp` differs: it has the
  `/xform` row and hook, reading the counters.
- **One fix was needed to get there.** A first version put `geometry_draw.cpp`'s retail lines inside
  an `if constexpr` block. That narrowed the two locals' lifetime and shrank the frame (98 lines
  differed: `sub $0xc8` vs `$0xb8` and shifted stack offsets). The site now uses two constant-condition
  selects at function scope, which fold to retail's two lines.
- **With the switch on**, by contrast: `meshinstance_draw` .text grows 2496 -> 3328 bytes and
  `geometry_draw` 4656 -> 5568. `world.draw`'s hit path is a 16-byte xor/or compare against `g_world`
  followed by the 12-cell copy.

## 8. How to measure (coordinator)

1. **Shadow:** heavy town, `BANISHED_STRESS_COMPPROF=4`, faithful, mods parked. Expect
   `[VERIFY] ItemTransform checks=N mismatches=0`, with N around 1,700 x frames. Every other shadow
   (FrustumCull, SkinBones, DrawItemSort) should be at 0.
2. **Row:** `BANISHED_STRESS_COMPPROF=1`, heavy and compact. Check:
   - `/xform` requests ~1,750 heavy and ~3,760 compact;
   - world.draw req/key in the hundreds with ~0 evictions;
   - mesh.draw evictions much smaller than keys;
   - declines ~0;
   - `hit ns/call` against `miss ns/key`.
3. **A/B:** `XformMemo::kEnabled` false vs true (`src/engine/graphics/xform_memo.h`).
   - Use the unprofiled perf exe, the **faithful profile at speed 10**, heavy **and compact**, in
     interleaved pairs (at least 2 per town).
   - Stop RTSS, Afterburner and RTSSHooksLoader64 first, and run on a CLEAN machine.
   - **Should move: draw ms/frame, compact first.** Compact keeps 2x the requests of heavy on a 40%
     smaller draw, so if the memo shows anywhere it shows there.
   - **Must not move:** sim ms/iteration and the resave golden (no state is written).
4. Then the usual gate run: new game and the resave gate.

## 9. Doubts, and when to stop

- **The upside is small even on paper.** The ceiling is ~1,683 hits/frame heavy and ~3,356 compact.
  - At XFORM's measured miss price (26.4 / 22.7 ns per key, which includes RDTSC overhead and a
    table miss) minus a cheap hit, that is at most ~0.03 ms/frame heavy (~2% of 1.70 ms draw) and
    ~0.06 ms/frame compact (~6% of 1.04 ms).
  - Those are upper bounds, and XFORM's ab2 pairs (XFORM.md Results) had ±1-2% spread per leg.
- **The RDTSC estimate counts ALU that out-of-order execution may already hide.** XFORM's "est. saved
  0.47 ms/frame" was the miss price per key times the hits. That counts the Lerp + AsMatrix work it
  skipped as if it sat on the critical path. In the draw loop, that ALU work, a sqrt + div normalise
  and ~25 flops, is independent of the Context's stores and the next item's loads. A wide out-of-order
  core likely overlapped most of it with the gather's and the Context's memory stalls. **So the true
  saving per hit may be a few ns, not ~20.** Realistically that is ~0.005-0.015 ms/frame heavy (under
  1%, below noise) and ~0.01-0.03 ms/frame compact (1-3%).
- **The memo's own loads must hit L1/L2, or it loses again.**
  - XFORM lost because each lookup touched two lines of a 1.4 MB table while the retail arm's inputs
    (the 56-byte pair, just read by the caller) were hot.
  - Here, `world.draw`'s slot is one line touched ~200 times per scope, so it stays in L1.
  - `mesh.draw`'s lines are first touched cold once per scope (one L2/L3 miss per key, the same as
    XFORM per key) and then reused 4-6 times within the pass. Whether they are still in L1/L2 at the
    repeat depends on how much draw work sits between a key's shadow-map request and its light
    requests: the whole shadow pass. If the row's `hit ns/call` for the table is not well under
    `miss ns/key`, the table half is not paying.
- **The census rests on pass 2 reaching only const geometry members** (XFORM.md doubt 1). A future
  draw-time writer would show as an `ItemTransform` mismatch, and the unit test shows such a writer
  does diverge.
- **The flags assumption:** XFORM.md doubt 2 is weaker now. The two stores no longer share keys
  across spellings, so an FMA build could not create a cross-site mismatch. Each store would still be
  self-consistent.
- **STOP CONDITION.** Remove XFORM2 per the user's policy (a neutral A/B means the code is removed:
  the hand-in keeps the record and history keeps the code) if any of these holds:
  - the compact A/B does not show draw ms/frame down by more than the spread of its interleaved pairs;
  - heavy is worse;
  - the shadow reports any mismatch.

  If it is neutral, do not try a third variant of this memo. The remaining requests are mesh.git's
  2.06 req/key, and XFORM already measured those.

## Results (coordinator, 2026-10-13 night) -- NOT MERGED (neutral; code at tag `legacy/perf-20261013-xform2-unmerged`)

Measured from the block's worktree exe, deployed by hand under the lock (the identity verified per run). Shadow
`ItemTransform` (COMPPROF=4, heavy, mods parked): 10,128,045 checks / 0 mismatches, every other shadow 0. The `/xform`
row (heavy): 1,827 requests/frame, 89.7% hits, 188 keys/frame (9.73 req/key: mesh.draw 2.18, world.draw 234.9),
6.09 scopes/frame, 0 declines, 2.6 evictions/frame; miss 34.6 ns/key, hit 8.2 ns/call; est. saved 0.043 ms/frame
(upper bound).

A/B series ab3 (CLEAN, overlays stopped, interleaved x2, faithful, speed 10, COMPPROF=0; `perf/results/*_ab3_*_xform2*`):

| town | draw ms off | on | delta |
|---|---|---|---|
| heavy | 1.11 / 1.13 | 1.11 / 1.12 | 0.0% / -0.9% |
| compact | 0.70 / 0.68 | 0.70 / 0.70 | 0.0% / +2.9% |

Neutral in the heavy town and inside the pair spread in the compact one, as the Doubts predicted: the ALU the memo
skips was hidden behind the gather's memory stalls, and with only the two repeating sites kept the ceiling was 0.043
ms/frame. Per the user's policy the body does not enter main; this hand-in is the record, the code stays at the tag.
The world-matrix memo idea is closed: mesh.git (2.06 req/key) was measured by XFORM, the repeating sites by XFORM2.
