# WGPAR -- data-parallel worldgen, the exact brush memo, the start town's diag lines (hand-in)

Block 2 of the loading-speed session (`LOADPLAN.md` §3). Branch `worktree-agent-a17f9b76698ff5102`,
base main `b55d48a65`, main merged at `ecfef5a07` (the `[WorldHash]` lines kept intact). Static gate
only: the game was NOT run. Ruling 2 (byte-identical worlds) is the design constraint of every change
below; the coordinator's `[WorldHash]` kit (3 seeds x 3 sizes) is the proof in the game.

## 1. As built

| commit | what |
|---|---|
| `perf:` `49d985baf` | `System::ParallelFor` (the pool); the e5d0 / cf440 cell loops moved to `worldgen_cells.cpp` and split by rows; the exact eee0 falloff memo in both brushes |
| `test:` `4b16d61df` | `tests/unit/wg_parallel_test.cpp` (4 tests) + the roster row |
| `perf:` `01792b672` | the start town's per-entity `fprintf` lines folded into one `[startTown::summary]` line |

### 1.1 The helper: `src/engine/system/parallel_for.{h,cpp}`

```cpp
namespace System {
namespace Parallel {
constexpr int kMaxWorkers = 7;                    // == Cpu::kMaxParallelWorkers
using RangeFn = void (*)(void* ctx, int begin, int end);
int  Workers();                                   // builds the pool on first use
void SetWorkers(int workers);                     // tests only: rebuild with n workers
void Shutdown();                                  // join (also at static destruction)
}
void ParallelForRaw(int begin, int end, int grain, Parallel::RangeFn fn, void* ctx);
template <class F> void ParallelFor(int begin, int end, int grain, F&& f);   // f(chunkBegin, chunkEnd)
}
```

* Pool size `Cpu::ParallelWorkers(Cpu::Get().physical)` = `clamp(physical - 1, 0, 7)` (new roster rule,
  `src/ext/cpu/roster.h`; the caller is the n-th thread). 0 workers (one core): `f(begin, end)` inline.
  Built lazily by the first call (the loader thread during `wg.base`); no settings key, no flag.
* Fork-join: the caller publishes the job under the pool mutex (epoch bump), wakes the workers, claims
  chunks from an atomic counter like a worker, then waits on a condvar until EVERY worker checked in for
  the epoch (so no worker can still be inside a job when the next is published). `grain <= 0` = ~4
  chunks per thread. No allocation per call, no `std::function` (a captureless trampoline + `void*`).
* Workers park on a condvar between calls (no CPU), register with `DiagThreads` as role `'J'`
  (`parallel`; `tools/sample_report.py` knows it) with a busy word set during a job, and copy the
  caller's MXCSR at every job (same rounding / FTZ state as the loader, whatever it is).
* Calls are serialised by a call lock; a nested call (from inside `f`, any thread) runs inline.
* Built on `Platform::ThreadCreateSuspended` / `ThreadJoin` (Windows and POSIX twins) + std mutex /
  condvar. For EROSION: keep calls coarse (one per pass; a call wakes every worker, ~10-50 us).

### 1.2 Where it is used, and why each split is exact

| kernel | file | exactness argument |
|---|---|---|
| `wg.base` e5d0 cell loop | `worldgen_cells.cpp` `WgCells_BaseHeightmap` (rows of the (dim+1)^2 grid) | The 4 `RandTol` draws and both Perlin ctors stay in `worldgen.cpp` before the call, in retail order. Per cell: sqrt/pow of its own distance, two `PerlinNoise_Generate` (const object, no state, no RNG), a blend; written to its own cell; no running value crosses cells. |
| cf440 scratch + cf150 normals | `WgCells_BlendScratch` | Each record is a pure function of (row, col) and the read-only heightmap. |
| cf440 R / G | `WgCells_BlendRockGrass` | Per texel: its own scratch record + `Generate` on the const Perlin (seeds drawn by the caller before). Writes its own 4 bytes. |
| cf440 AO | `WgCells_BlendOcclusion` | Reads only the scratch array (the previous call joined before returning); its 21x21 sums are its OWN, accumulated in the unchanged dz/dx order; writes only its own byte 2. |

The loop bodies moved verbatim (only the row bounds come from the chunk); `worldgen.cpp` keeps the
RNG draws, allocations, the cf440 fallbacks and the `[WorldHash]` lines. x86-64 SSE floats carry no
excess precision and the build has no FMA, so the same expression rounds the same in any TU / thread.

### 1.3 The brushes: an exact memo instead of threads (`src/game/Game/worldgen_brush_curve.h`)

Why pow dominated: the eee0 weight is `w(s) = 1 / (pow(B, -(sqrt(s) - C)) + 1)` with `s = dx^2 + dy^2`
and the scan half-extent comes from a radius-growth loop over `pow(B, -(r - C))`. Both depend ONLY on
the brush's (falloff base B, cutoff C) and the integer s. Each brush recomputed every w(s) for up to
8 symmetric cells, and every point of a path carries the SAME (B, C) (cc850 / ranges TryPlacePath stamp
identical attributes into all ~dim+1 points): ring inner (1.25, 10) / outer (4, 4), secondary paths
inner (1.25, 8) / outer (16, 1.5).

`WgBrushCurveCache` keys on the BIT patterns of (B, C) (4 entries, round robin; the e910 / ec50 path
pass alternates inner and outer) and fills w(s) lazily, once per (brush, s), with the brush file's own
`wg_powf` / `wg_sqrtf`, the same sign-bit negate and the same float expression. pow / sqrt are
deterministic, so each stored weight IS the value the old body computed; the brush's min / max blend,
the mask, the brush order and the stamp / carve combine loops are untouched. River points and
satellites have random radii (a new key per brush) and still save ~8x (each distinct s once).

Edits in the two files (for the EROSION merge):
* `worldgen_f390_erosion.cpp`: one `#include "Game/worldgen_brush_curve.h"` after `system/Value.h`, and
  inside `wg_eee0_radial_brush` only: the step-1 radius loop -> `s_curves.Get(falloffBase, cutoff)`,
  `span = curve.span`, the 3 per-cell lines (sqrt / pow / 1/(f+1)) -> `s_curves.Weight(...)`.
  `wg_f390_erosion`, `wg_f390_carve`, `wg_f390_smooth` are unchanged.
* `worldgen_f390_rivers.cpp`: the same in `wg_StampBrush_eee0`.

The brushes were NOT threaded: after the memo a brush is a few thousand compares; a fork-join per
brush costs more than it saves, and threading a whole pass needs the row-band scheme in §4.

### 1.4 Start town (`starttown.cpp`, `founding_spawn.cpp`)

The coordinator's sampler: ~85 ms of `gul.starttown` in our own unbuffered `fprintf` (callers
SpawnColonistRetail, the placeStructure lambda, SeedHouseInventory, AssignFoundingHousing,
SpawnEntities / Resources, CarryInit). Folded into a print-only `StartTownDiagTally` and one line at the
end of `StartTown_SpawnEntities`:

```
[startTown::summary] houses captured=N | structures placed=N model=N instantOk=N | families=N kids=N housed=N full=N | house food sufficient=N foodClamped=N fuelClamped=N
```

Counted from the removed lines of the LOADSTAMP `newgame_perf` log, that run would read `captured=6 |
placed=8 model=7 instantOk=8 | families=6 kids=8 housed=6 full=1 | sufficient=6 foodClamped=0
fuelClamped=6` (a cross-check for the coordinator's run of the same seed).

Gone: residence OnCreate, struct complete-model / instant-path ok / placed, citizen family, housing
family / seed budget / clamps / per-house food, `[startTown::res] begin` + `variants` (folded into the
one per-pool line), `[FoundingSpawn] colonist`, `[CarryInit] VERIFY` (prints only when capacity is 0).
Anomaly lines still print in place. No spawn order, RNG draw or call changed. Not touched (sim-tick
components, coordinator's ruling): `[AnimalUpd]`, `[WPCCensus]`, `[WPCCtrl]`, `[DestroyWorkers]`,
`[Herd]`, `[Pasture]`. Log readers that grep the removed lines must read the summary instead (none in
`tools/` matched).

## 2. Proof

* **Unit** (`tests/unit/wg_parallel_test.cpp`, 1176 / 0 debug; the unit target compiles -O0 in every
  flavour): `ParallelFor` covers every index exactly once with 0 / 1 / 2 / 4 / 7 workers, grains 0..5000,
  nested calls inline; base + the three blend passes vs the OLD bodies (copied into the test only) on dims
  16 / 66 / 130 / 256 x 3 seeds x workers 1 / 2 / 4 / 7, `memcmp` (heightmap, scratch, RGBA); the memo vs
  the old per-cell expression: span + every w(s) of the window for the game's 4 pairs, 40 river /
  satellite keys and 20 random ones, with `powf` AND `std::pow`, three interleaved passes (evictions);
  400 whole brush applications old vs memo on one random 193^2 mask (both modes, off-edge centres), `memcmp`.
* **World identity** (coordinator): the `[WorldHash]` height f32 / i16 and blend lines must equal
  `docs/extensions/handins/loadkit/hash_base_4bdffa55a.txt` for every seed x size, debug and perf. The
  tiles hash too (the start town change is print-only).

## 3. Expected saving (census on 9800X3D, large map; numbers from the unit bench, -O0, machine NOT clean)

Unit bench at dim 1024 (-O0, so absolute ms are ~7x the perf exe's): base 3,981 ms sequential -> 1,073
(3 workers + caller) -> 624 (7 + caller), i.e. x3.7 on 4 threads, x6.4 on 8; blend 5,402 -> 1,588 -> 916
(x3.4 / x5.9); one secondary path (1,025 points, inner + outer brush) 286 ms -> 15 ms (x18).

| stage (perf census ms) | 8 physical (expected) | 4 physical (expected) |
|---|---|---|
| wg.base 559 | ~90 | ~150 |
| wg.blend 1,447 | ~250 | ~430 |
| wg.stamp 1,191 | ~60-120 (memo, single thread) | same |
| wg.carve 1,130 | ~60-120 | same |
| gul.starttown 459 | ~400 (-60..-85) | same |
| **base + blend + stamp + carve 4,327** | **~0.5 s** (target <= 1.0 s) | **~0.8 s** |

Those are estimates; the coordinator's `[LoadTime]` census on the perf exe is the measurement.

## 4. Open items

* If stamp / carve stay above ~100 ms after the memo, the exact parallel form is ROW BANDS per pass:
  each thread applies ALL brushes of the pass in order, clipped to its own rows of the mask. A cell's
  update sequence (brush order) is unchanged and bands share no cell, so it is byte-identical; it
  restructures `wg_f390_stamp` / `wg_f390_carve` (the carve is in EROSION's file), so not done here.
  The stamp / carve combine loops (`h = mB*h*mA - mC`) are trivially row-parallel too (cheap, left).
* cf440's AO pass is the largest remaining kernel (21x21 per texel, sqrt + div per tap); a scratch-
  locality rewrite would have to keep the per-texel tap order exactly.
* `castmetric` reads 4 on the merged tree: the two extra casts are the coordinator's `[WorldHash]`
  commit (`load_time.cpp:65`, `gameupdateloop_load.cpp:1227`), not this block's (mine are 2 CAST-OK
  `user_data` lines).
* The `wg.base` falloff `powf(1.1, distBias - dist)` could also be memoised by s (exact); not needed
  once the loop is threaded (Perlin dominates).
