# FRUSTUMCULL -- the exact prepared frustum cull of the scene walk

Block FRUSTUMCULL, 2026-10-12. Found by IBS_20261012 (sections 2.3 and 3.3): at uncapped fps on the
main thread, `RtVolume_Frustum_Contains` is 19.2% of op samples and `SelectVertices` 14.6%. Both
have zero cache misses, so the cost is ALU work and branches. This is faithful perf work in the
STILLPATH / PREFETCH style: no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()`
field, no `getenv`. The kill switch is `inline constexpr bool FrustumCull::kFastCull = true`.
**The game was not run by this block.** The coordinator runs the COMPPROF=4 shadow and the A/B.

**Short version.** `Scene::AddCulledDrawItems` builds its two world-space frusta once per draw
group. It now also builds a 240-byte side table for each one (`FrustumCull_Prepare`). The table
holds the planes as structure-of-arrays, plus the three `plane.<axis> < 0` sign masks per plane
that `SelectVertices` used to recompute for every box. Each box test is then an inlined,
branch-free SSE2 body. Each plane is tested with the same products, in the same association,
and with the same compares as the reference body, four planes per vector. The reference body
(`RtVolume_Frustum_Contains`, retail 0x1800f8e80) is unchanged. It is what `kFastCull = false`
runs, and it is the shadow's reference.

## 1. The proof: same bits for every (frustum, box, mask)

The reference body, frustum.cpp. For i = 0..5 with bit i set in `mask`:
`SelectVertices` picks n = (x<0 ? max.x : min.x, ...) and p = the other corner.
`dN = py*n.y + px*n.x + pz*n.z - pw`. If `dN <= 0`: `dP` is computed the same way, and if `dP < 0`
the body returns 0x40. Otherwise (the plane straddles the box) the bit is kept. If `!(dN <= 0)`
the bit is cleared. Bits 6 and up are never touched. Its only effects are the return value and the
sticky MXCSR status flags.

The fast body (`FrustumCull_ContainsFast`, frustum_cull.h). Lane i holds plane i, and lanes 6 and
7 are zero.

| step | reference | fast | why the bits are equal |
|---|---|---|---|
| sign test | `plane.<axis> < 0.0f` per box | the same C++ compare, once, in `Prepare`, stored as an all-ones / all-zeros lane mask | Same compare on the same bits. -0.0 and NaN give false, so the min corner is picked. The table copies the plane bits, so later writes to the frustum cannot make it stale (section 2). |
| vertex pick | `?:` select of min or max | `t = (min ^ max) & neg`, `n = min ^ t`, `p = max ^ t` | XOR / AND are bitwise, so n and p are exactly min's or max's bit pattern, NaN payloads included. |
| dN, dP | `((py*ny + px*nx) + pz*nz) - pw`, MULSS / ADDSS / SUBSS | the same tree with MULPS / ADDPS / SUBPS | Each lane of a packed op is the scalar op, rounded under the same MXCSR (RN, the same DAZ/FTZ). The products and the association are the same. GCC may swap the two operands of an add in either body (it does in the fast one); IEEE addition commutes, and the only thing a swap can change is the payload of a NaN result, which only ever reaches a compare. No FMA: the tree has no `-mfma` / `-march`, the objects contain no `vfmadd`, and a contracted reference would fail the unit test. |
| decisions | `dN <= 0` (COMISS, ordered), `dP < 0` | CMPLEPS / CMPLTPS against +0 | Both are the ordered predicates, false on NaN, true at exactly 0 for `<=`. |
| early-out | returns 0x40 at the first tested plane with `dN <= 0 && dP < 0`; planes after it are skipped | `outside = movemask(le & lt)`; `if (outside & mask & 0x3F) return 0x40` | The return value is the constant 0x40 whichever outside plane comes first, so the order cannot be seen. The skipped planes have no other effect. |
| result | `mask` with the bits of the tested planes where `!(dN <= 0)` cleared | `mask & ~(~le & 0x3F)` | Bits that were not tested stay as they were in `mask`, in both bodies. Bit 6 and up pass through unchanged (unit-checked: `0xFF` in, `0xC0` out). |

What the fast body does that the reference skips: the dot products of untested planes, the dP of
inside planes, and the planes after an early-out. These are pure ALU work on locals. FP
exceptions are masked (no code in `src/` unmasks them), so they cannot trap. Their only trace is
the sticky MXCSR status bits, which nothing reads. No memory is written. The reads are the table
(the caller's local) and the 24-byte box the reference reads too.

**Claim.** For every frustum f, box b and mask m:
`FrustumCull_ContainsFast(Prepare(f), b, m) == RtVolume_Frustum_Contains(f, b, m)`, provided that
f's planes are not written between `Prepare` and the test. ∎

The unit tests check this on 2.26M triples (section 5). The mutation in section 5 checks that the
tests can fail.

## 2. Writers of the planes (why the tables cannot be stale)

The table copies the planes, so the only question is whether the caller's frustum changes between
`Prepare` and its last test. The caller that uses the table is `Graphics::Scene::AddCulledDrawItems`
(src/engine/graphics/cull.cpp):

| frustum | declared | writers in the function | readers |
|---|---|---|---|
| `cullFrustum` | a local `::Frustum` | **one**: `FrustumTimes(&g->_cullCamera._frustum, &cullFrustum, ...)` (= `RtVolume_Frustum_MulTransform3`, 0x1800f9310) on the line before `Prepare` | `planes[0].x/y/z` (the shadow-caster extension, read before the walk), and since this block `Prepare` only |
| `shadowFrustum` | a local `::Frustum` | **one**: `FrustumTimes(&g->_camera._frustum, &shadowFrustum, ...)` on the line before `Prepare` | `Prepare` only |

Neither local's address is passed anywhere except as `const Frustum*` to `FrustumTimes`
(as `ret`, before `Prepare`) and to `Prepare`. The walk's other calls (`NodeBoxCull`,
`NodeBoxShadow`, `PushVisible`, `CullStackPushChildren`, `ReserveVisibleList`) take neither, and
the virtual `AddDrawItems` calls run after the walk. So both frusta are constant from `Prepare` to
the end of the walk.

The other writers of `Frustum::planes` in the tree are not reached by a table: the two ctors
(frustum.cpp), `MulTransform3`'s `ret`, `sun_camera.cpp:325`, `saveload_draw.cpp:630`,
`userinterface_draw.cpp:235`, `wave28_lane_gfxbody.cpp:599` (Camera copy), and
`wave9_lane_gfxd.cpp:249`. A table is a value snapshot built by the caller, never a cache keyed on
a Frustum's address, so these writers have nothing to invalidate. This is stronger than a memo
keyed on the plane bits: nothing is looked up, so nothing can match by accident. The shadow
(section 3) still reads the **live** frustum through `table->src`. A table built too early would
therefore show up as mismatches, and the unit test `frustum_cull_shadow_reports` checks that it does.

Layout: `Volume::Frustum` (0x64 bytes, pinned) is unchanged. The table is a separate stack object
(`FrustumCullPlanes`, 16-aligned, 7x8 floats + one pointer). Retail's 3 tail pad bytes
(+0x61..+0x63) are not used.

Callers left on the reference body (not hot):
- `geometry_draw.cpp` (the terrain chunks, chunkCount^2 per frame, plus decals and roads, mask 0x3F).
- `weather_particle_draw.cpp` (125 tiles, mask 0x3f).

They could use the table the same way (prepare `localFrustum` / `worldFrustum` once before the
loop), but IBS shows no samples from them.

## 3. The shadow `FrustumCull` (BANISHED_STRESS_COMPPROF=4)

The engine does not include the stress harness. `frustum_cull.h` declares two engine globals:

- `g_frustumCullVerify` (int)
- `g_frustumCullReport` (a function pointer)

`StressHarness`'s `Resolve` sets both when `cp == 4`, and nothing else ever does. The hook calls
`StressHarness_Verify("FrustumCull", ok, detail)`.

`AddCulledDrawItems` reads `g_frustumCullVerify` **once per call**, into the local
`const bool verify` before the walk. Each box test is `verify ? FrustumCull_ContainsVerified(...)
: FrustumCull_Contains(...)`, a branch on a loop-invariant local.

`ContainsVerified` runs the reference body on `table->src` (the live frustum) and the fast body
on the table. It reports `ok = (fast == ref)` with `detail = (ref << 8) | fast`, then **returns the
reference answer**. So a shadow run draws the faithful scene, and a mismatch cannot propagate
down the quadtree. A shadow run does both tests per box, so its draw ms are not a perf number.

Read: `[VERIFY] FrustumCull checks=N mismatches=M` in the [COMPPROF] report and in
standalone_error.log. **Expected M = 0.** N = every box tested over the run. The first five
mismatches print `[VERIFY] MISMATCH FrustumCull #k detail=D`: `D >> 8` is the reference mask and
`D & 0xFF` the fast one.

## 4. The counter: `[COMPPROF]   frustum cull: ...` (any COMPPROF level >= 1)

`AddCulledDrawItems` counts its box tests in a local and adds the total to `g_frustumCullBoxes`
once per call, and increments `g_frustumCullPasses` once per call. Nothing is counted per box in
memory. The COMPPROF report prints one row per window, just before the per-component rows:

```
[COMPPROF]   frustum cull: boxes tested/frame=49123  cull passes/frame=2.00  (24561 boxes/pass)
```

`boxes tested/frame` counts every `Contains` call of the walk: node boxes for the cull and shadow
frusta, plus the per-object cull and shadow-caster boxes. IBS's figure was "~49k scene nodes,
6.9M box tests/s at ~140 fps". The row gives the real number for the heavy town. `cull passes/frame`
is the number of draw groups that reached the walk.

## 5. Unit tests (tests/unit/frustum_cull_test.cpp; frustum.cpp added to tests/unit/sources.cmake)

`tools/build.sh unit perf`: **1106 tests, 0 failures** (6 new).

- `frustum_cull_perspective_bit_identical`: 700,000 cases. The frusta come from the
  4-Scalar and 6-Scalar ctors, moved by random transforms (1 in 8 with the identity basis, 1 in 6
  left in camera space). The boxes are ordinary, zero-size, inverted, at the origin (dN exactly 0
  on the apex planes), or with a face exactly on a plane's `w`. The masks are 0x3F, random 6-bit
  values, 0, and full 32-bit values (bit 6 and up set). The last 100k cases also have specials in
  the boxes: +-0, +-inf, NaN with random payloads, denormals, random bit patterns, +-1e8. Results:
  culled 437k, inside 234k, partial 28k; **0 mismatches**.
- `frustum_cull_orthographic_bit_identical`: the same with the orthographic ctor. 700,000
  cases (culled 474k, inside 212k, partial 14k), **0 mismatches**.
- `frustum_cull_random_planes_bit_identical`: 700,000 cases with fully random planes, 500k of
  them with specials in the planes as well (NaN / inf / -0.0 / denormal normals and `w`).
  **0 mismatches**.
- `frustum_cull_edges`: 11^3 plane combinations over {+-0, +-1, 0.5, -2, +-inf, NaN, +-denormal}
  x 40 boxes x 3 masks (159,720 cases: NaN dN, 0 * inf, inf - inf), plus hand-checked cases:
  - a box face exactly on a plane (dN = dP = 0): the plane straddles and its bit is kept;
  - fully outside: 0x40;
  - the same box with that plane's bit clear: 0;
  - `0xFF` in, `0xC0` out.
- `frustum_cull_shadow_reports`:
  - `ContainsVerified` returns the reference, reports every call, and gives the detail
    `(ref << 8) | fast`;
  - after the live frustum is moved, the shadow reports mismatches (a stale table is caught).
- `frustum_cull_prepare_layout`: the lane masks are all-ones / all-zeros, -0.0 and NaN give 0,
  lanes 6 and 7 are zero, and `src` is set.

Mutation checks (run once, then reverted):
- `cmple` -> `cmplt` for dN fails four tests.
- Changing the n-vertex select to retail's `!(y >= 0)` (NaN -> max) stays **green**. This is
  correct: a NaN normal component makes dN and dP NaN whichever vertex is picked, so the select
  difference cannot be observed (section 8).

Gates:
- `tools/build.sh build perf --jobs 2`: OK. The fast body is inlined at all four call sites of
  `AddCulledDrawItems`, with no call and no xmm6-15 save per box.
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2**. No casts were added: the masks are floats filled
  by `memcpy`.

## 6. How to measure (coordinator)

1. **Shadow:** `BANISHED_STRESS_COMPPROF=4`, heavy town. Expect `[VERIFY] FrustumCull ...
   mismatches=0` with N in the hundreds of millions.
2. **Row:** any COMPPROF >= 1 run prints the `frustum cull` row, which gives boxes/frame.
3. **A/B:** `kFastCull` true vs false (frustum_cull.h, then rebuild perf; `--no-build` copy in).
   Use COMPPROF=0, RTSS / Afterburner stopped (every earlier fps column was the 140 cap), a CLEAN
   machine, and interleaved pairs (off, on, off, on), heavy and compact towns. Compare the stress
   CSV's `draw_ms_mean` and the fps column. `update_ms_per_iter` must not move: the sim does not
   use this code. Neither does the resave golden, since the cull is presentation only and the
   answers are identical.

## 7. Prediction

Per box, the reference runs up to six iterations of: a loop test, 3 sign compares and 6 selects,
a 3-term dot product, a data-dependent branch, and on `dN <= 0` a second dot product and a second
branch. `SelectVertices` writes n and p through memory (the IBS row is it, inlined, attributed by
line), and the per-plane branches mispredict on straddling boxes. The fast body is about 70
branch-free SSE ops for all six planes, plus one predictable branch at the end, inlined in the
walk. Expected: **-50 to -65% of the cull's cost**. Most of the 14.6% `SelectVertices` row goes
away, and a good part of the 19.2% `Contains` row.

In frame terms, the cull is ~34% of main-thread ops at uncapped fps. Ops are not cycles, but the
cull's ~70 cycles per op sit close to the main-thread mean. So expect the uncapped heavy-town
`draw_ms_mean` to drop by roughly 15-25% (on the order of 0.3-0.5 ms of the ~2.1 ms Draw CPU)
and uncapped fps to rise ~5-8%.

**At 60 Hz** the frame is vsync-bound, not CPU-bound, so fps will not change. The ~0.3-0.5 ms per
frame becomes idle time on the main thread (out of 16.7 ms), which means less CPU and power. At
high sim speeds, Update and Draw share the main thread, so that time goes to sim iterations
instead (at speed 50, ~0.4 ms a frame is ~1-2 more iterations per frame in the heavy town). The
gain shows only when the main thread is the bottleneck: uncapped, or at high speed.

## 8. Doubts and findings

- **Port vs retail on NaN (a finding, not changed):** docs/known_port_issues.md row 1327. The
  retail disassembly (objdump of the shipped Runtime-x64.dll; Ghidra was not up):
  - `SelectVertices` 0x1800f8fb0 tests `plane >= 0` (COMISS + SETAE). For a NaN component this
    picks max for the n-vertex, where the port picks min. This cannot be observed (section 5's
    mutation).
  - Contains 0x1800f8f35 branches to the dP test on `dN <= 0` **or unordered** (COMISS + JBE). The
    port tests only the ordered `<=`, so a NaN dN clears the bit here, while retail keeps it (NaN
    dP) or culls (dP < 0).

  Every input whose dN is not NaN agrees, and no known frustum or box carries NaN or inf. A
  `behaviour:` fix would change the reference and the fast body together (fast:
  `inside = CMPGTPS(dN, 0)`, i.e. `le = !(dN > 0)`), plus the unit tests' expectations. This block
  matches today's port, as asked.
- **The kill switch is in the header, not in frustum.cpp.** Out of line, GCC's Win64 code saved
  and restored xmm6..xmm15 (20 stack moves) on every call, about as much work as the body.
  Inlining the body into the walk needs it, and the switch, in the header.
  `FrustumCull_ContainsVerified`, `Prepare` and the globals stay in frustum.cpp.
- **MXCSR:** both bodies run under the same MXCSR. No code in `src/` changes it. A third-party DLL
  that did would affect both bodies the same way, so they would still agree with each other.
- **Off x86** (the Linux ARM64 build): no SSE2, so `FrustumCull_ContainsFast` is the reference
  body and the shadow compares the reference with itself. On ARM64, GCC's default
  `-ffp-contract=fast` may fuse the reference's products into FMAs, which is a retail-fidelity
  question for the reference body and not something this block changed.
- **The shadow returns the reference.** A COMPPROF=4 run therefore draws exactly the faithful
  scene, even if the fast body were wrong.
- Not run in the game by this block. First verification is the coordinator's COMPPROF=4 heavy run.

## 9. Files

| file | change |
|---|---|
| `src/engine/volume/frustum_cull.h` (new) | `FrustumCull::kFastCull`, `FrustumCullPlanes`, `FrustumCull_Prepare`, inline `FrustumCull_ContainsFast` / `FrustumCull_Contains`, `FrustumCull_ContainsVerified`, the verify hook and the two counters |
| `src/engine/volume/frustum.cpp` | `Prepare`, `ContainsVerified`, the globals; a comment on the reference body recording the retail NaN difference. The reference body itself is unchanged. |
| `src/engine/graphics/cull.cpp` | two tables after the two `FrustumTimes`, the hoisted `verify`, a local `contains` lambda at the four test sites, the counters once per call; the unused `FrustumContains` wrapper deleted |
| `src/diag/stress_harness.cpp` | COMPPROF=4 sets the hook; the `frustum cull` row; window reset of the counters |
| `tests/unit/frustum_cull_test.cpp` (new), `tests/unit/sources.cmake` | the 6 tests; frustum.cpp linked into `unit` |
| `docs/known_port_issues.md` | row 1327 (the NaN difference) |
