# CLICK hand-in (Phase D.4, branch `d4-click`, base `e9f7a978f`)

Brief: the user heard ONE click at the END of a "birdsong one-shot" in about five minutes of Windows
play on the SDL3 backend; everything else was clean. Diagnose with evidence; fix only what is ours,
with XAudio2 as the reference. Static gate only: the game was never run from this worktree.

**Final gate** (tip, worktree `d4-click`):

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0 |
| unit | 1002 / 0 (1000 + 2 `audio_feed_*`) |
| warnings | 0 new (debug, perf) against main's `runs/warn_baseline_{debug,perf}.txt`, full + incremental logs |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK, exit 0 |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

## Commits

| commit | title |
|---|---|
| (1) | refactor: audio_feed end-of-buffer tests; the offline sound-tail and SDL pull-check tools |
| (2) | behaviour: the SDL voice's first pull hands over the resampler padding SDL leaves out of its first request (known_port_issues row 1289) |

## The four candidates, in the brief's order

### 1. The voice END in `audio_sdl.cpp` / `audio_feed` — CLEAN

Read: `AudioFeed::Pull` hands over `min(left, want)`; the pass in which a one-shot runs out gets exactly
the bytes left, `BufferEnd` + `EndOfStreamPut` fire in that pull (the backend flushes the stream), and
every later pull hands over nothing. SDL's device thread fills the rest of a short period with silence
(`SDL_audio.c`, `SDL_PlaybackAudioThreadIterate`: `br < buffer_size` → memset to the silence value; in
the mixing path a stream that returns less simply contributes less). "OnBufferEnd up to one device period
early" is the hand-over point, not a cut: nothing is removed from the stream. `OnStreamEnd` fires only when
`SDL_GetAudioStreamAvailable == 0` (the converted output is consumed), one pass after the last data, and
only then does the main thread destroy the voice. Voices are never reused: one `SDL_AudioStream` per
`PlayPlatform`, destroyed at the reap, so candidate 3 (reuse) does not arise.

Pinned by `tests/unit/audio_feed_test.cpp`:
- `audio_feed_one_shot_end_mid_period_hands_over_only_its_tail`: 11 mono frames pulled 4 a pass; the third
  pull gets 6 bytes not 8, byte for byte the submitted data, `SEF` in that pull, nothing on three more
  pulls, and a second one-shot on the same feed starts at its own first byte.
- `audio_feed_pull_larger_than_the_buffer_takes_the_whole_buffer_once`: a pull larger than a one-shot takes
  it whole, once; a looping one wraps and keeps going (three `L`s: a pass ending exactly on the pull's end
  wraps in that pull).

Measured without a device, with the real `AudioFeed` under SDL 3.4.18 (`tools/audio_tail/sdl_pull_check.cpp`):
every one-shot scenario outputs exactly the expected frame count (`out 83395 frames (expect ~83395)`), the
last-data pass is short by design, `OnStreamEnd` is the next pass, and the continuity check over the output
(a seamless sine, the device's silence fill modelled) finds no step above the sine's slope at the end.

### 2. The decoder tail — RETAIL-IDENTICAL, AND NOTHING ENDS ON A CLICK

`tools/audio_tail/pkg_sounds.py` reads the two `.pkg` directories (the FastLZ-1 window format of
`IO::CompressedStream`, `package_file_read.cpp`'s metadata and entry layout) and cuts the
`PlatformSerialize` run out of every SoundBuffer resource **after decompressing the `.crs`** — 56 buffers,
all of them, by name. `tools/audio_tail/audio_tail.cpp` decodes each with the game's `Audio::XwmaDecoder`
and prints the tail. All 56 decode to the seek table's total with `Failed()` false. Final samples (ch 0,
S16): the largest in magnitude are Rain 36, Saw0 -27, Wind -26, Winter -21, Cow3 -17; 34 of 56 end
within ±3. The largest sample step in any tail (last 64 frames) is 81 (Rain) against body steps in the
thousands. A step of 36 is -59 dBFS: not a click, and retail (XAudio2 within 1 LSB of this decoder) plays
the same tail.

**There is no bird one-shot.** The 56 shipped SoundBuffers are Animal (Chicken, Cow, Sheep), Tool (Anvil,
AxeChop, Hammer, Hoe, PickAxe, Saw), UI, and the Effects loops. The birds are in `Environment_field`,
`Environment_forest`, `Environment_wetland`, whose serialized `loopForever` byte is 1 (the Animal / Tool /
UI buffers carry 0). So the user heard a LOOPING ambience, which never reaches the END path at all; its
loop seams are 640 (Field), 3 (Forest), 1 (Wetland) S16 steps, present in retail too and periodic, not
once in five minutes.

### 3. Voice reuse — does not arise (see 1)

### 4. Resampling — ONE DEFECT, AT THE START, FIXED

SDL sizes `additional` as the input frames that convert to the output it needs minus what is available
(`SDL_audiocvt.c`, `SDL_GetAudioStreamDataAdjustGain`). While a track is not flushed, the resampler holds
back `RESAMPLER_MAX_PADDING_FRAMES` (7 with SSE: `SDL_audioresample.c`) of input as its right-hand filter
context. From the second pull on, that hold-back is inside "available" and so inside the request. On a
fresh stream nothing is queued, the first request leaves it out, and handing over exactly `additional`
converts 7-8 frames fewer than the period: the device fills the end of the FIRST period with silence, a
0.15 ms notch 21 ms into every resampled voice. Every voice is resampled (the device runs at its own
rate; the pitch ratio resamples the rest). XAudio2's SRC primes itself and has no such gap.

The fix (`kFirstPullMarginFrames = 8` in `audio_sdl.cpp`): a voice's first pull hands over the padding
too; the surplus sits in SDL's queue and shrinks the next request, so nothing else moves. A buffer may
finish up to 8 frames earlier than before, the same class as "one period early".

Measured (`sdl_pull_check.exe`, 3 source formats × 2 device rates × 5 period sizes × 8 pitch ratios,
loop and one-shot, 20000 passes each):

| | short pulls on pass 0 | short pulls elsewhere | continuity problems |
|---|---|---|---|
| `--prime=0` (before) | 375 of 375 resampled scenarios | 0 | 342 (steps up to 0.42 full scale at the notch) |
| `--prime=8` (the fix) | 0 | 0 | 0 |

**This is not the reported END click.** It is the only defect the pull path has, it is systematic, and
it is fixed; the END finding stays open with no our-side cause. The remaining candidate is a scheduling
underrun of SDL's device thread under a -O0 exe at full load, which only a run can show.

## Also found: row 1276 (the tornado "noise burst") rests on a mis-cut

`TornadoEffect_version0.crs` sits in `data0.pkg` as 7 CompressedStream blocks; the last block (the final
5,966 bytes, packets 194-199) is FastLZ-compressed to 5,256 bytes. The XWMA block's raw-package scan read
those compressed bytes as packets, so "FFmpeg rejects packets 194-199" and XAudio2's "5,000 frames of
full-scale noise" were both measured on bytes the game never sees (the loader decompresses). Decoded from
the decompressed resource: 200 of 200 packets, 1,212,928 frames (the table's total), `Failed()` false,
ending at -1 0. `SoundBuffer::DecodeAtLoad`'s "Failed() is not fatal" handling is harmless but its
comment and row 1276 describe a sound that does not exist. Hoe2 is stored whole, so EXTRACT.md's 12-step
tail difference stands (and is -58 dBFS). Withdrawing 1276 is the coordinator's edit.

## Files

- `src/platform/sdl/audio_sdl.cpp` — `primed`, `frameBytes`, `kFirstPullMarginFrames`, the first-pull margin in `GetCallback`; banner line.
- `tests/unit/audio_feed_test.cpp` — two tests (above).
- `tools/audio_tail/pkg_sounds.py` — the package reader / SoundBuffer extractor (`python -I pkg_sounds.py <pkg>... --out <dir>`).
- `tools/audio_tail/audio_tail.cpp` — decode and print the tails (build line in its banner).
- `tools/audio_tail/sdl_pull_check.cpp` — the pull-path model (build line and the measurement in its banner).
- `docs/known_port_issues.md` — row 1289.
