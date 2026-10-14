# AUDIODEC -- the xWMA decode at load (hand-in)

Block AUDIODEC of the loading-speed session (`LOADPLAN.md` §3). Branch `worktree-agent-af7dfa5a1d8c2d424`,
base main `b55d48a65`, main `ecfef5a07` merged before the final build. Static gate only: the game was NOT run.

## 1. As built

| commit | what |
|---|---|
| `e62f642d0` diag | `[SoundDecode]` exit line gains `pcmHash=<16 hex>`: the sum over decoded buffers of an FNV-1a over each buffer's PCM (64-bit words, seeded with its length; the sum makes the two loading threads' order irrelevant). Under `#ifndef BANISHED_STRIP_LOGS`: debug / og only, perf and release skip the hash with the line. **First on purpose**: it builds alone on the OLD conversion |
| `27297d269` perf | `src/engine/audio/xwma_decode.{h,cpp}`: `ToS16` (`std::nearbyint` per sample) -> `Audio::FloatToS16` / `Audio::PlanesToS16` (SSE2: mulps, cmpord NaN mask, max/min clamp, cvtps2dq, packssdw, unpacklo/hi_epi16 for stereo; 8 frames per step; scalar `cvtss2si` tail; `lrintf` without SSE2). `Append` writes straight into the caller's `out` while it has room when a `DecodeRange` starts with nothing pending; only the overflow goes to `_pending` |
| `c0b480439`, `e434f6f58` test | `tests/unit/xwma_tos16_test.cpp`, 3 tests (below) |

No flag, no `getenv`, no guard, no door, no change to `External::Load` or to the order of anything. The decoder is
port-only (retail handed WMA to XAudio2), and its output is byte-identical.

## 2. The exactness proof

Old body, per sample: `v = nearbyintf(x * 32768.0f); v >= 32767 -> 32767; v <= -32768 -> -32768; NaN -> 0; else (int16_t)v`.

1. `y = x * 32768.0f` is the same single IEEE multiply (mulss / one mulps lane). DAZ / FTZ, if ever set, would act on
   the same instruction in both bodies.
2. NaN: the old comparisons are all false on NaN, so it reached `return 0`; the new body returns 0 (scalar `y != y`,
   vector `cmpord` mask -> +0.0 -> 0).
3. Clamp-then-round == round-then-clamp: rounding R is monotone and leaves integers fixed, and both bounds are
   integers. `y > 32767` -> `R(y) >= 32767` (old 32767) and `R(32767) = 32767`; symmetric below; inside, the same R.
   +-inf saturate in both.
4. R itself: `nearbyintf` (MinGW: x87, rounds by the x87 control word) vs `cvtss2si` / `cvtps2dq` (MXCSR). Both are
   round-to-nearest-even at thread start (Win64 and SysV ABIs) and nothing in `src/` changes either (grep: no
   `fesetround`, `_controlfp`, `_mm_setcsr`, `ldmxcsr`, `_MM_SET_ROUNDING_MODE`).
5. The clamped value is in [-32768, 32767]: the int32 convert never hits its 0x80000000 overflow value, and
   packssdw's saturation is then a no-op.
6. Direct write: Append converts the same frames in the same order; the first `outSamples` go straight to `out`,
   the rest to `_pending` (which is empty when direct mode starts, and only receives samples once `out` is full);
   the return is `direct + n` where the old code returned `n` copied from `_pending`. Same samples, same count.

The unit test (bit-exact, the old body kept in the test only):

* `xwma_tos16_float_classes`: specials (+-0, denormals, FLT_MIN/MAX, +-inf, NaN payloads), every integer and
  half-integer y in [-33100, 33100] with 8 ulps either side, a 1/401 stride over all 2^32 bit patterns, 2M
  pseudo-random floats in [-1.01, 1.01]; scalar and the SSE2 mono path. **14,961,521 floats, 0 mismatches.**
  Pinned: 32767.5 -> 32767, 32766.5 -> 32766, -32767.5 -> -32768, -32768.5 -> -32768, 1.5 -> 2, -2.5 -> -2,
  NaN -> 0, +-inf -> 32767 / -32768.
* `xwma_tos16_layouts`: mono / stereo / 3 channels x frames 0..40 x plane offsets 0..9; no write past the end.
* `xwma_tos16_every_shipped_sound` (the content proof): scans `WinData/*.pkg` for the plain PlatformSerialize runs
  (the scan that cut the fixture), decodes each with `XwmaDecoder` -- whole, and one packet per call into a 1500-frame
  buffer (direct + carry + pending-first continuation) -- against a reference pass over the raw WMA v2 API with the
  old frame rules and `OldToS16`. **23 sounds, 19,707,392 int16 values (mono + stereo, 44.1 + 48 kHz), 0 mismatches
  both ways, pcmHash `cedd9c0b9063a28d`.** Prints SKIPPED without a game install
  (`C:/GOG Games/Banished/WinData` or `/mnt/c/...`).

The other sounds sit inside compressed package blocks the scan cannot read: that is what the `[SoundDecode]
pcmHash` is for (§4).

## 3. Cost, measured (-O2 scratch bench of the same test sources, 9800X3D, the 23 plain sounds)

| | ms |
|---|---|
| old conversion (`OldToS16` over 19.7M values) | **880** |
| new conversion (`PlanesToS16`, same values) | **2.0** |
| whole decode through `XwmaDecoder` (new) | 91 (of which buffer allocation 8) |
| the WMA v2 decoder alone (raw API, no conversion) | 85 |

So after AUDIODEC the decode is the WMA v2 decoder itself. Scaling the census: `ToS16` was 79% of 1,063 ms = ~840 ms,
i.e. ~19M values per route at the measured 45 ns each; those same values now cost ~2 ms of conversion and ~80 ms of
WMA decoding. **Expected `loader.resources` ~1,063 -> ~200 ms** (both routes); the boot's global resources (581 ms)
lose their decode share too (size unknown until the census).

Note: `unit` always compiles at `-O0`, so its printed `decodeMs` (~125-150) is not the game's number.

## 4. What the coordinator checks

1. **Content (debug exe, one new game each)**: build `e62f642d0` alone (the diag on the OLD conversion) and this
   branch's tip; same new game; compare the `[SoundDecode] decoded=... pcmBytes=... pcmHash=...` line in
   `standalone_debug.log` (written at exit): **identical** (decoded / pcmBytes / pcmHash). Both runs must reach the
   same loads (the hash sums over every buffer the process decoded, boot + game).
2. The usual gate: `[ExtLoad] calls=5694 / 0` unchanged (no load moved), resave 5/5 == golden, unit 0 failures
   (now 1175 tests: +3), acceptance 0 off target.
3. Census: `loader.resources` on the perf exe, both routes; the boot's global-resource stage.

## 5. Static gate (worktree)

`tools/build.sh build debug --jobs 2` OK, `build perf` OK, `unit debug` 1175 / 0; no warning in the changed files
(the named `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt` do not exist in the tree -- only the
archived `docs/history/endgame_b/*/runs/` copies; I checked my files produce none). `abi_audit.py` OK.
`acceptance.sh` 0 off target. **castmetric: my files 0; the tree total reads 4 after merging main** -- the two
extra casts come from main, not this branch: `src/diag/load_time.cpp` (static_void) and
`src/game/Game/gameupdateloop_load.cpp` (static_cross), both from the LOADSTAMP / loadkit commits.

## 6. Open items

* **Parallel decode not done.** The decode runs inside `SoundBuffer::Serialize` (inside `External::Load`); moving it
  to workers means deferring `DecodeAtLoad` past Serialize with a join before anything reads `_data` / `_length` /
  the rewritten format -- not simple, and after this block the whole decode is ~80-90 ms per route. Worth it only if
  the census still shows `loader.resources` well above target; WGPAR's pool would be the tool.
* The WMA v2 decoder (`third_party/wmadec`, fixed `-O2 -w`) is now the cost. Raising its flags could change float
  results (no `-ffast-math` / `-march` would be safe to assume): not touched.
* Linux build not run here (no WSL session in this block); the SSE2 path is `__SSE2__`-guarded and the fallback is
  portable `lrintf`.
