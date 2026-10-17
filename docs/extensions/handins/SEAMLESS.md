# SEAMLESS -- `ext.seamless_loops`: the looping ambiences without their codec-tail dropout

Block SEAMLESS, 2026-10-16. Worktree branch `worktree-agent-a9739c38307497eb5` (main `cf16b65ff` merged in).
Diagnosis: `handins/CHATTER-DIAG.md` (the town chatter's "harsh start / stop" = the 24.7 ms near-silent
codec tail of `Crowd.wav` at every 15.38 s wrap; retail's XAudio2 plays the same PCM and loops it whole).
Class A, default OFF, settings key `ext.seamless_loops`, display "Seamless ambient loops", description
"Blends the end of each looping ambient sound into its start, removing the dropout at the loop point.
Takes effect at the next start." Option B (fade-in / fade-out at voice start / stop) is DEFERRED until the
user has listened to this.

## What was built

| file | what |
|---|---|
| `src/ext/seamless_loops/seamless_loops.{h,cpp}` | the pure transform `Transform(pcm, frames, channels, rate)` and the seam body `ApplyToLoopingBuffer` (one `[SeamlessLoops]` stderr line per buffer) |
| `src/ext/seamless_loops/enhanced_entry.cpp` | the Enhanced tab's combo "Seamless ambient loops" Off / On (next start) |
| `src/engine/audio/SoundBuffer.h`, `vtbatch7_soundbuffer_soundeffect.cpp` | the engine seam `Audio::SoundBuffer::_extLoopSeam` (null by default) and its one call in `DecodeAtLoad` |
| `src/engine/audio/audio_pcm.h` | `Audio::LoopSeamApplies` -- the decode's gate as a pure inline (unit-tested) |
| `src/ext/registry.{h,cpp}`, `registry_boot.cpp` | the row (class A, OFF), the boot install of the seam |
| `src/ext/enhanced_tab/enhanced_tab.{h,cpp}`, `src/ext/ui/Dialog/EnhancedOptions.rsc` | the tab's fifth row (our own resource: a string, a two-item table, a text and a combo) |
| `tests/unit/ext_seamless_loops_test.cpp`, `tests/unit/sources.cmake` | 7 tests |

## The coordinator's five constraints, resolved

### 1. Looping sounds only -- at DecodeAtLoad, because the loop flag is provably known there

The loop is a property of the **buffer**, not of the SoundEffect: `SoundBuffer::_loopForever` (+0x20) is the
first field `SoundBuffer::Serialize` (0x1800e0480) reads, before `PlatformSerialize` and before
`DecodeAtLoad` runs at the end of the read path. The only player of a buffer, `Device::PlayPlatform`
(0x1800de7a0), takes the loop count from that flag alone (`MOVZX EAX,byte [RCX+0x20] ; NEG AL ; SBB` at
0x1800de90f: 255 or 0; `audio_play_platform.cpp`), and `SoundEffect::Compile` derives the effect's
`_maxLength` (0 = looping) from the same buffer flag (`vtbatch7...cpp:532`). So a looping buffer is never
played once and a one-shot buffer is never looped: "a buffer shared by a looping and a non-looping
SoundEffect" cannot occur. The transform therefore runs in `DecodeAtLoad`, gated by
`LoopSeamApplies(seam installed, _loopForever, pcm, frames)`; the tail trim is inside the same transform,
so it is loop-only too. A lazily built looped copy at the first Play was rejected: it would add an
allocation and a copy on the main thread at play time and a second PCM per buffer, and an in-place rewrite
at play time could race a voice already reading the buffer -- none of which buys anything when the flag is
known at load. Streamed buffers (the music) are not decoded here and are never touched.

### 2. Faithful path untouched

- Engine side: one static pointer `Audio::SoundBuffer::_extLoopSeam`, null by default (DESIGN.md hook rule 5;
  the same single-function-pointer shape as `Save::SaveLoad::_extExtraMods`). `src/engine` does not include
  `src/ext`. With the row off the decode pays `LoopSeamApplies`' first operand, one null test per buffer
  load (not a hot path), and the PCM is the plain decode.
- `Ext::InitFromSettings` sets it ONCE at boot: `Ext().seamless_loops ? &SeamlessLoops::ApplyToLoopingBuffer
  : nullptr`. The faithful profile turns every row off, so the gate (`/profile:faithful`) leaves it null.
- Evidence (debug exe, the `[SoundDecode]` exit line in `standalone_debug.log`, same stress town):
  main's exe (`4477f5e8`, faithful) `decoded=56 refused=0 short=0 xwmaBytes=2000696 pcmBytes=45594624
  pcmHash=02c8071bd4ca4cea`; this branch, row off (`/profile:faithful`): the SAME line, byte for byte, on
  both builds of the branch (`6db8a34e`, and `ae89a9e3` after the last edit); 0 `[SeamlessLoops]` lines.

### 3. The bounds

| constant | value | why |
|---|---|---|
| `kCrossfadeMs` | 50 | long enough to hide the join in noise-like ambience, short against loops of 15-56 s |
| `kTailWindowFrames` | 64 | the tail is measured in 64-frame windows (1.45 ms at 44.1 kHz) |
| `kMaxTrimFrames` | 2048 | at most one WMA frame (`XwmaDecoder::FrameLength` at 44.1 and 48 kHz): the padding is never longer |
| `kTailRatio`, `kQuietPercentile` | 0.2, 1 | a tail window's RMS is under 20 % of the sound's own quiet level, the 1st percentile of its 10 ms window RMS over the body (everything before the last 2048 + 441 frames). Crowd: quiet 377, limit 75, its tail windows 2..58, its body never under 312 |

- **Equal-power** (`sin` / `cos`), not linear: the ambiences are noise-like, so the end and the start are
  uncorrelated, and for uncorrelated signals `sin^2 + cos^2 = 1` keeps the power constant through the fade;
  a linear fade dips 3 dB at its middle (the unit test checks the middle of the fold stays within 15 % of
  the body level).
- Mono and stereo, any rate (the crossfade is `rate * 50 / 1000`: 2205 at 44.1 kHz, 2400 at 48 kHz).
- Never applied to a buffer under 2 x the crossfade, before or after the trim; channels outside 1..2 or a
  zero rate leave the PCM as decoded. A sound whose quiet level is 0 (>= 1 % digitally silent windows)
  trims nothing and is only folded.
- In place, no allocation: frame `j < X` is read before it is written, frame `m + j >= X` is never written;
  the buffer's `_length` shrinks by trim + X (the allocation is freed by `~SoundBuffer` as before).
- Side effect, recorded: `Device::PlayPlatform`'s resume point (`time % _durationSeconds` into the ORIGINAL
  seek table, then `XwmaPcmPlayBegin` clamped to the PCM length) can land up to trim + 50 ms (<= 96 ms) past
  the new end; it is clamped to the end, so such a resume starts at the wrap. `_durationSeconds` is not
  changed, so the scene's timing is unchanged.
- Unit tests (`tests/unit/ext_seamless_loops_test.cpp`, all pass; unit 1236 / 0 on the perf flavour):
  a seam with a 1000-frame codec tail (960 trimmed: whole windows only; both sides of the wrap within
  20 % of the body; the fold's middle within 15 %), a steady 440 Hz tone (nothing trimmed, never above
  sqrt 2 x the amplitude), too short before / after the trim (byte-identical), the trim bound (3000
  silent frames -> 2048 trimmed), mono 48 kHz (640 trimmed, crossfade 2400; bad channels / rate
  untouched), all silent, and the gate (`LoopSeamApplies`: a one-shot, the row off, a refused decode and an
  empty decode are never handed over).

### 4. Evidence

**Offline, the shipped transform on the game's own decode** (scratch harness `seam_check.cpp` linking
`src/ext/seamless_loops/seamless_loops.cpp`; the PCM from `tools/audio_tail` over the user's packages, in
the scratchpad only). 10 ms RMS across the wrap, last 50 ms | first 50 ms:

| loop | trimmed | before | after | seam step |
|---|---|---|---|---|
| Crowd 44.1k | 1088 (24.7 ms) | 517 499 412 **45 19** \| 476 543 660 592 545 | 630 536 626 647 565 \| 542 413 674 609 542 | 302 -> 138 |
| River 44.1k | 960 (21.8 ms) | 696 638 137 **39 0** \| 760 851 826 853 699 | 624 771 725 638 915 \| 841 817 761 824 689 | 834 -> 109 |
| Env field 44.1k | 1920 (43.5 ms) | 229 **1 1 1 1** \| 333 279 465 337 369 | 396 342 431 319 288 \| 235 285 408 318 349 | 640 -> 29 |
| Fire | 960 (21.8 ms) | 1159 824 305 **44 18** \| 9268 5582 ... | 784 608 992 1022 1271 \| 3719 2030 ... | 23 -> 44 |
| Rain | 832 (18.9 ms) | 930 881 320 **97 39** \| 1015 2860 ... | 1143 1324 1395 1218 1126 \| 920 1406 ... | 329 -> 142 |
| Wind | 448 (10.2 ms) | 1820 2906 2012 1248 **75** \| 821 1064 ... | 641 566 866 899 1034 \| 1634 1406 ... | 147 -> 44 |
| Tornado 48k | 1024 (21.3 ms) | 3043 2930 2425 **250 114** \| 3953 6693 ... | 2959 3919 3616 3177 2974 \| 2334 4459 ... | 155 -> 334 |
| Env forest 48k | 704 (14.7 ms) | 264 218 230 **39 17** \| **53** 209 ... | 391 342 267 271 258 \| 218 230 ... | 3 -> 34 |
| Env wetland | 512 (11.6 ms) | 429 408 551 **36 17** \| 195 231 ... | 257 279 365 398 356 \| 343 423 ... | 1 -> 92 |
| Env winter 48k | 0 | 739 824 569 547 430 \| 156 279 ... | 637 592 337 534 427 \| 744 812 ... | 91 -> 48 |

Every dropout (bold) is gone; the remaining steps are inside each sound's normal sample-to-sample range
(Crowd's body steps reach 901). Fire keeps a louder start (its first 10 ms is a crackle burst at 9268,
part of the recording); the fold brings the end up to meet it.

**In game** (debug exe of this branch `ae89a9e3`, the `BANISHED_STRESS` town via `tools/stress_run.sh
--no-build --debug --speed 1 --frames 300`, game-dir lock `seamless`, one run at a time):

| leg | profile | exit | crash log | `[SeamlessLoops]` | `[SoundDecode]` |
|---|---|---|---|---|---|
| main's exe `4477f5e8` | faithful | 0 | 0 B | 0 | pcmBytes 45594624, pcmHash `02c8071bd4ca4cea` |
| row off | faithful | 0 | 0 B | 0 | identical to main |
| row on (`STRESS_EXT=seamless_loops`: per-entry, every other row off) | per-entry | 0 | 0 B | **10** | pcmBytes 45470292 (-124,332 B = -31,083 frames = the ten trims + folds), pcmHash `80b5d431850d2e9d` |

The ten lines (`standalone_error.log`; the `[Ext] profile=` line is line 17, the first `[SeamlessLoops]`
line 53: the seam was installed before any looping buffer loaded), identical to the offline numbers:

```
[SeamlessLoops] Audio\Effects\Fire.wav: applied frames=1351680 ch=2 rate=44100 trimmed=960 (21.8 ms) crossfade=2205 quiet=489 -> 1348515 frames
[SeamlessLoops] Audio\Effects\Crowd.wav: applied frames=678400 ch=2 rate=44100 trimmed=1088 (24.7 ms) crossfade=2205 quiet=377 -> 675107 frames
[SeamlessLoops] Audio\Effects\River.wav: applied frames=716800 ch=2 rate=44100 trimmed=960 (21.8 ms) crossfade=2205 quiet=576 -> 713635 frames
[SeamlessLoops] Audio\Effects\Forest.wav: applied frames=877056 ch=2 rate=48000 trimmed=704 (14.7 ms) crossfade=2400 quiet=209 -> 873952 frames
[SeamlessLoops] Audio\Effects\Winter.wav: applied frames=983040 ch=2 rate=48000 trimmed=0 (0.0 ms) crossfade=2400 quiet=145 -> 980640 frames
[SeamlessLoops] Audio\Effects\Field.wav: applied frames=665600 ch=2 rate=44100 trimmed=1920 (43.5 ms) crossfade=2205 quiet=178 -> 661475 frames
[SeamlessLoops] Audio\Effects\Wind.wav: applied frames=663552 ch=2 rate=44100 trimmed=448 (10.2 ms) crossfade=2205 quiet=562 -> 660899 frames
[SeamlessLoops] Audio\Effects\Wetland.wav: applied frames=664064 ch=2 rate=44100 trimmed=512 (11.6 ms) crossfade=2205 quiet=156 -> 661347 frames
[SeamlessLoops] Audio\Effects\Tornado.wav: applied frames=1212928 ch=2 rate=48000 trimmed=1024 (21.3 ms) crossfade=2400 quiet=1847 -> 1209504 frames
[SeamlessLoops] Audio\Effects\Rain.wav: applied frames=2478080 ch=2 rate=44100 trimmed=832 (18.9 ms) crossfade=2205 quiet=733 -> 2475043 frames
```

Ten of the 56 decoded buffers = exactly the ten looping ambiences of the census; the 46 one-shots (tools,
animals, UI, tree fall) were not handed over. The listen is the user's (row on, near an occupied house,
camera still: the dropout every ~15 s should be gone).

### 5. Registry, settings, menu

- `ext.seamless_loops` row in `src/ext/registry.cpp` (class A, OFF, formatVersion 0), field in
  `Ext::Extensions`; it appears in the Extensions dialog from the table like every row.
- Enhanced tab: `src/ext/seamless_loops/enhanced_entry.cpp` (the FAA-MENU pattern: a Contribution, the
  combo found by name; Off / "On (next start)", shows the REQUESTED value; Apply calls `SetAndRecord`, the
  dialog's OK / Apply flushes `settings.cfg`). `EnhancedOptions.rsc` grows to five rows (string
  `SeamlessLoops`, table `seamlessLoopsItems`, `textSeamlessLoops`, `comboSeamlessLoops`).
- Takes effect at the next start (by design: the buffers decode once, at load, and stay resident).

## Checks

- unit 1236 / 0 (perf flavour; 7 new tests), `tools/acceptance/acceptance.sh` 0 off target, `tools/abi_audit.py`
  OK (a first draft's `long` in `ToS16` was a GROWTH row; replaced by an `int32_t` round with the same
  results: the row-on `pcmHash` is the same before and after the edit).
- `tools/ext_resources.sh` compiles the grown `EnhancedOptions.rsc` (0 problem lines;
  `EnhancedOptions_comboSeamlessLoops.crs`, `_seamlessLoopsItems.crs`, `_textSeamlessLoops.crs` built). NOT
  deployed and NOT looked at in game: the Enhanced tab's fifth row is owed a by-hand look (does the page fit
  five rows; Off / On (next start); OK writes `ext.seamless_loops`). Until the .crs are deployed the entry
  finds no combo and does nothing (its `[SeamlessLoopsMenu] combo=0...` line says so).
- Stress numbers (perf): not run -- the game directory is shared with another agent's runs (machine busy),
  and the extension does no per-frame work (one transform per looping buffer at load, ten buffers).
- `git diff --stat main...`: our code, our `.rsc`, the hand-ins and the unit test only. Nothing retail
  entered the tree (the decoded PCM and the harnesses stayed in the scratchpad).
- The game dir's `OpenBA.exe` was restored to main's debug build (`4477f5e8`) after the runs.

## Follow-up

- **Option B** (`ext.audio_declick`: short ramps at voice start / stop and a gain glide in the SDL backend)
  is DEFERRED until the user has listened to A (CHATTER-DIAG §5 B).
