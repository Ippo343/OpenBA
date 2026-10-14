# LOADPLAN -- the loading-speed session (opened 2026-10-14)

The coordinator's plan for the phase the user opened at the close of the multithreading session: make the game load
faster -- boot to the main menu, a new game (new map), a save load. Research: `LOADRESEARCH.md` (the route map, read-only);
instrument: `LOADSTAMP.md` (`[LoadTime]` stamps + the sampler on every game thread, merged at gate 0, `df4c80d8a`; the
sampler's exit-probe fix `f166b68ad`). Progress goes to `docs/extensions/STATUS.md`; each block's facts to its hand-in.

## 1. The user's rulings (2026-10-14)

| # | question | ruling |
|---|---|---|
| 1 | retail's splash floors (2.0 s + 0.5 s + fades, ~3.6 s of the boot) | a **class A ext toggle** that ends them as soon as the load is done; faithful keeps retail's floors |
| 2 | world-build changes | **byte-identical only**: the same seed gives the identical world and the identical save; no class B world generation |
| 4 | boot after SPLASH (3.06 s with ext.quick_screens: device ~0.6-0.8 s in the NVIDIA driver, fades ~0.75 s, menu scene ~0.66 s, global resources ~0.36 s) | **left as is** (2026-10-14): BOOTDEV and a menu-scene profile are NOT queued |
| 3 | the other presentation floors | the SPLASH toggle **also ends the save dialog's minimum operation time** (1.46 s) once the work is done; the ~0.25 s loading-screen fades **stay retail** |

## 2. The census (perf exe `f166b68ad`, warm file cache, mods parked, machine CLEAN, 9800X3D / 8 physical)

`[LoadTime]` stamps from `standalone_error.log`; attribution from the loader-thread sampler (1000 Hz).

### 2.1 New game: click -> first frame **9.86 s**

| stage | ms | what the loader thread does |
|---|---|---|
| click -> STAGE C (menu fade) | 263 | presentation |
| `loader.resources` (GameUpdateLoop's resource list) | **1,063** | **88% `Audio::SoundBuffer::DecodeAtLoad`** -> `XwmaDecoder::DecodeRange` -> **`ToS16` 79%** (`src/engine/audio/xwma_decode.cpp:32`, `std::nearbyint` per sample: MinGW's libm call). Port-only work: retail handed WMA to XAudio2 |
| `wg.base` | 559 | Perlin noise per cell (`RtUtility_PerlinNoise_Generate`, CornerValue, pow, cosf) |
| `wg.coastline` .. `wg.rivers` | 153 | |
| `wg.stamp` | **1,191** | `wg_StampBrush_eee0`: **pow 88%** (falloff curve per cell) |
| `wg.erosion` | **3,582** | `wg_f390_erosion` self time 97% (droplets, sequential `System::Random`) |
| `wg.carve` | **1,130** | `wg_eee0_radial_brush`: **pow 91%** |
| `wg.smooth1/2`, `quantize` | 6 | |
| `wg.blend` | **1,447** | `wg_BlendTexture_cf440` self 88% + Perlin 12% (per texel) |
| `wg.upload` .. `teardown` | 124 | mesh, water contour |
| tile grid (classify, FillZones, PfSeed, RegionSeed, shore) | **12** | LOADRESEARCH's suspects are not a cost |
| `gul.starttown` | 459 | Perlin 59%, **our own `fprintf` 18%** (diag lines), allocator |
| finalize -> first frame (fade 253) | ~265 | presentation |

### 2.2 Load Gate.sav: click -> first frame **4.07 s**

| stage | ms | |
|---|---|---|
| `save.read` I/O | 14 | |
| `save.read.io-done` -> `save.read.done` | **1,465** | SaveLoad's **minimum operation time** (a presentation floor, no work) |
| menu fade | 286 | |
| `loader.resources` | **1,031** | the same audio decode as 2.1 |
| `ser.entities` | **785** | `InternalFactory::PersistObject` -> `ExternalFactory::FindOrLoad` by name 73%: `Filename::AdjustDelimiters` 37%, `ExpandRelativePath` 31%, `String::GetLength`, `MemAlloc/MemFree` (the path is rebuilt and normalised for every entity's Description / MaterialInstance reference) |
| rest of Serialize | 57 | |
| fade -> first frame | ~275 | |

### 2.3 Boot -> main menu **4.74 s**

| stage | ms | |
|---|---|---|
| process -> `engine.videomodes` | 95 | |
| `engine.device` (D3D11CreateDevice) | **546** | NVIDIA driver: WINTRUST / CRYPT32 signature checks (`nvldumdx`), not ours |
| audio, rest of Startup | 110 | |
| global resources (loader) | **581** | ImageBuffer decompress + SoundBuffer decode; few samples = mostly waiting (I/O or the main-thread sync) -- the block finds which |
| menu scene a1 -> done | 716 | hidden behind the splash floor |
| idle until `splash.fadeout` | **~1,380** | **the splash floor** (ruling 1) |
| fades + menu loop load | ~540 | |

## 3. Blocks

All four code blocks are **faithful-path speedups with byte-identical output** (ruling 2): no toggle, no flag, always on
(memory `feedback_no_env_flags_for_working_features`); SPLASH is the one class A row. Thread counts come from
`src/ext/cpu/roster.h` (physical cores inside `sys.cpu_limit`); with one core every parallel loop runs inline.

| block | what | target | depends on | agent |
|---|---|---|---|---|
| **AUDIODEC** | the WMA decode at load: `ToS16` exact and fast (round-half-even via `cvtss2si` under the default MXCSR / `lrintf`, the same saturation and NaN rule; a unit test over every sample value class + a hash of every decoded SoundBuffer of the game's packages before / after); then, if still > 150 ms, decode the SoundEffects of one resource list on a worker pool (below `External::Load`, results identical, `[ExtLoad]` unchanged) | `loader.resources` 1,063 -> <= 200 ms (both routes, and the boot's share) | -- | opus, worktree |
| **WGPAR** | the data-parallel worldgen passes, each split by rows with the per-cell operation order unchanged: `wg.base` (Perlin), `wg.blend` (cf440), the stamp and carve brushes (`wg_StampBrush_eee0`, `wg_eee0_radial_brush`: each brush application split across rows; brushes stay in order); plus the start town's diag `fprintf` lines removed or folded (port-only noise). Owns the one parallel-for helper (`src/engine/system/parallel_for.h`, a fixed pool built once from the roster, fork-join, no allocation per call) | stamp + carve + blend + base 4.33 s -> <= 1.0 s on 8 physical, report P4 | -- | opus, worktree |
| **EROSION** | `wg_f390_erosion` 3.58 s single-threaded: first the exact single-thread work (read the inner loop's code generation; the profile shows `powf` running through the double `pow` / `PowCore` -- keep the result bits, find the cheaper exact path; bounds / bilinear reads; allocation per droplet), then a research note on exact parallelism (droplets whose footprints cannot touch a cell another in-flight droplet reads, run speculatively and committed in order -- only if provably identical). Coordinates with WGPAR through the helper header (uses it after WGPAR merges; never writes its own pool) | 3.58 s -> as low as exact allows; report the floor | WGPAR's helper for the parallel part | opus, worktree |
| **SAVELOAD** | the save load's name lookups: `ExpandRelativePath` / `AdjustDelimiters` / `Filename` copies per reference -- a memo or a cheaper exact normalisation below `FindOrLoad` (the found External is identical; `[ExtLoad]` and the RefTable counts unchanged); the third package mount on a save load (`UpdateLoad` sets `mods->_dirty=1`; skip an unchanged remount only after reading its side effects, LOADRESEARCH §1.3) | `ser.entities` 785 -> <= 300 ms | -- | opus, worktree |
| **SPLASH** | class A row (default OFF in faithful, ON in the ext profile): the two boot splash floors end when the load is done; and the save dialog's minimum operation time (ruling 3); the fades stay | boot 4.74 -> ~2.5 s; save load -1.5 s if extended | -- | opus, after the four above |
| **BOOTDEV** (not scheduled) | the 546 ms device creation is the NVIDIA driver; overlapping the global resources' file reads / decompression with it is the only lever. Recorded; taken only if the user asks after SPLASH | | | |

Expected (8 physical, warm): new game 9.9 -> ~4 s (erosion is the unknown), save load 4.1 -> ~2.0 s (~0.6 s with the
floor in SPLASH), boot 4.7 -> ~2.5 s with SPLASH on.

## 4. Proof of identity (every code block)

- **Unit**: each changed kernel against the old body on random and edge inputs, bit-compared (the old body kept in the
  test only, never in `src/` -- `feedback_no_off_switched_perf_bodies_in_main`).
- **World identity (coordinator, before merge)**: new game with `BANISHED_SEED` fixed, saved at frame 1, for 3 seeds x
  3 map sizes, old exe vs new exe -> the save files `cmp` equal; a debug-exe kit script in the scratchpad, folded into the
  gate table row.
- **The gate**: `mergegate.sh` at 0 problems: `[ExtLoad] calls=5694 / 0`, tutorial 5773 / 12, typetable `965b527a`,
  resave 5/5 == golden, unit 0 failures, castmetric 2, abi_audit OK, acceptance at target, 0 new warnings.
- **Timing**: the census routes (`[LoadTime]`) x3 per route on the perf exe, machine CLEAN, P4 and unrestricted masks;
  numbers recorded, not chased.

## 5. Rules every block follows (in the brief: agents do not see CLAUDE.md)

- No null guard on a faithful crash; no `getenv` (options through `Opt()`, no new `BANISHED_*` flag); no doors
  (`tools/acceptance/acceptance.sh` at target); includes, never a local `extern`.
- Static gate only in the worktree: `tools/build.sh build debug|perf --jobs 2`, `tools/build.sh unit debug --jobs 2`,
  0 new warnings against `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt`, `tools/castmetric.py` at 2,
  `tools/abi_audit.py` OK. Never run the game; never `taskkill`; never `git stash` / amend / rebase / push; no
  sub-agents; never `find /`; merge main before the final build. At most four builders at `--jobs 2`.
- Commit titles `perf:` (faithful exact speedup) / `ext:` (SPLASH) / `diag:` / `test:`; the body names the files and
  the exactness proof. Hand-in at `docs/extensions/handins/<BLOCK>.md`: as built, the proof, open items.
- The coordinator alone runs the game, the identity kit, the gates and the census.

## 6. Results (coordinator, 2026-10-14 afternoon; perf exe `3a9e17c0f`, warm cache, mods parked, machine CLEAN, medians)

| route | before (`f166b68ad`) | faithful now | P4 mask (4 physical) | `ext.quick_screens` ON |
|---|---|---|---|---|
| new game, click -> first frame | 9.86 s | **3.23 s** (x3: 3.24 / 3.14 / 3.23) | 3.72 s | 3.12 s |
| of it `wg.erosion` | 3.58 s | 1.20 s | 1.59 s | 1.18 s |
| save load (Gate.sav), click -> first frame | 4.07 s | 3.10 s | 3.10 s | **1.19 s** |
| boot -> main menu | 4.74 s | 4.46 s | -- | **2.89 s** |

Merged, every one at gate 0 with `[ExtLoad] 5694 / 0`, tutorial 5773 / 12, typetable `965b527a`, resave 5/5 == golden:
LOADSTAMP `df4c80d8a` (+ sampler fix `f166b68ad`, `[WorldHash]` `4bdffa55a`), AUDIODEC `c6d0d499f` (pcmHash identical),
WGPAR `7c5d3174b`, SAVELOAD `7f3f5f86a` (`[ExpandMemo] MISMATCH` 0), SPLASH `da5242e07` (`ext.quick_screens`, class A, OFF in
faithful), EROSION `0377eed07`, EROSIONPAR `3a9e17c0f`. `[WorldHash]` 9 / 9 identical to the `4bdffa55a` baseline after every
world-generation block, and the three large maps also under P4. Boot left as is after SPLASH (ruling 4). The gate's loadgame
route count is timing-dependent (3420 / 3436 back to back), not an invariant.
