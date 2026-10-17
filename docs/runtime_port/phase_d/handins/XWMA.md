# XWMA hand-in (Phase D.4 wave 3, branch `d4-xwma`, base `0b11a0f85`)

Plan: `plans/SDL.md` Block 4, step A3 only, re-shaped by D4_RULES §3 Q5 (the FFmpeg WMAv2 extract,
decoded at load, the music chunk-ahead on the Stream worker). A4 / A5 (the SDL backend) come later.

Static gate only. The game, `sa-run` and `sa-deploy` were never run. Main moved twice during the
block. AUDIO-A (`a1599c13a`) merged in cleanly as `b46bd26a1`. SDL-C (`c2ad9ea93`) merged in as
`c78f63dbf`, keep-both in `tests/unit/sources.mk` with `sources.cmake` regenerated. The final gate
ran after that merge.

**Final gate** (tip `4c103380b`, main `c2ad9ea93` merged):

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf` (`-j2`) | EXIT 0 |
| `unit` (`-j2`) | 990 / 0, of which 6 are `xwma_*` |
| warnings | 0 new vs the main checkout's `warn_baseline_{debug,perf}` |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK; it scans `src/` and `tests/`, so `third_party` is outside its scope |

## Commits

| commit | title | gate |
|---|---|---|
| `27b4c637b` | build: vendor a WMAv2 decoder extracted from FFmpeg (LGPL-2.1) | gated ALONE (the next commit's files moved out): debug / perf EXIT 0, unit 971 / 0, library built from an empty build dir; 0 warnings; castmetric 2; abi_audit OK |
| `058581f91` | refactor: Audio::XwmaDecoder (xWMA packets -> S16) + an XAudio2 2.7 fixture | debug / perf EXIT 0, unit 976 / 0; 0 new warnings (debug, perf); castmetric 2 (the new files 0); abi_audit OK; B11 below |
| `b46bd26a1` | Merge main (AUDIO-A) | debug / perf EXIT 0, unit 976 / 0, 0 new warnings, castmetric 2, abi_audit OK |
| `fccddf48a` | refactor: XwmaDecoder::DecodeChunk for the Stream worker's packet ring | debug / perf EXIT 0, unit 977 / 0, 0 warnings, castmetric 2, abi_audit OK |
| `1bb8adcff` | docs: XWMA hand-in; EXTRACT.md's two shipped-sound findings; one header comment | docs and a comment only |
| `c78f63dbf` | Merge main (SDL-C) | keep-both `tests/unit/sources.mk`, `sources.cmake` regenerated |
| `4c103380b` | build: wrap the WMADEC_SRCS / WMADEC_CFLAGS lines of the Makefile | the final gate above; `make -pn` prints the same values |
| (this commit) | docs: XWMA hand-in, final gate | docs only |

**B11** (`b11cmp.py`, `runs/b11base_d4c/perf/obj` vs this branch's `build/perf/obj` before the merge):
950 objects IDENTICAL (17101 functions). Three objects contain no code in either build
(`system/debug`, `ui/ui_operator_delete`, `Player/tool_base_edit2`). One object is ONLY-TIP:
`src__engine__audio__xwma_decode.o`, which is new. The new `-I third_party/wmadec/include` on every
compile line changes no object.

**CMake:** configure (`-DBANISHED_FLAVOUR=debug`, Ninja) and `-t unit` built. `unit.exe` gave 976 / 0
from the repo root and from another directory. The `OpenBA` CMake target was not built: its only
change is one `target_link_libraries` line.

## The extract (`third_party/wmadec/`)

- **Source:** FFmpeg tag `n8.1.3` (commit `1041abdc962f`, 2026-09-20), from a shallow sparse clone of
  https://github.com/FFmpeg/FFmpeg in a scratch directory, copied by hand.
- **50 verbatim upstream files** (580,085 B), exported from the git blobs so no line ending was
  converted, with the sha256 of each in `EXTRACT.md`. Every one is in the compiler's include closure.
  The set is `wmadec.c`, `wma.c`, `wma_common`, `wma_freqs`, `wmadata.h`, `vlc`, `get_bits.h`,
  `sinewin`, `tx.c` + `tx_float.c` + `tx_template.c` (the MDCT), `float_dsp`, `reverse` and 24
  libavutil utility headers.
- **12 files written here** (26,769 B): `shim/` (config headers; the replaced `avcodec.h`,
  `codec_internal.h`, `decode.h`, `internal.h` and `thread.h`; two AAC tables copied from
  `aactab.c`; the runtime stubs) and `include/wmav2_decoder.h`, the C API.
- `.gitattributes` `* -text` keeps the files byte-exact under the repo's `core.autocrlf=true`.
- **The built library:** `libwmadec.a` 153 KB; 108 KB text + data and 328 KB bss (the sine-window and
  FFT tables, built lazily on first use).
- **Build:** one static C library, `-std=gnu11 -O2 -DNDEBUG -w` in every flavour, like SDL3.
  - Makefile: `WMADEC_LIB`, a file rule over its objects.
  - CMake: `third_party/wmadec/CMakeLists.txt`, target `wmadec`.
  - Both link it into the exe and into `unit`. Under the game's `-Wall -Wextra` it would raise 79
    warnings, all in verbatim upstream files and 0 in `shim/`.
- **Licence:** LGPL-2.1-or-later, linked statically. `EXTRACT.md` "Licence" spells out what a
  distributor owes: notice, licence text, library source and the relink offer.

## The API

`src/engine/audio/xwma_decode.h`, `Audio::XwmaDecoder`. It has no engine type, no OS header and no
allocator but `std::vector`. A "sample" is one frame (all channels), XAudio2's SamplesPlayed unit.

| call | does |
|---|---|
| `Open(format, packets, bytes, seekTable, packetCount)` | Takes the four pieces a SoundBuffer holds and keeps the pointers. FFmpeg's own init checks the format. `packets` / `bytes` may be null / 0 for a stream. |
| `DecodePackets(first, count, int16_t* out, outSamples)` | Returns the samples written, S16 interleaved at the stream's channels and rate. `first == NextPacket()` continues the pass; any other value starts a fresh pass. The pass's last packet drains the decoder. Output that does not fit is carried to the next call. |
| `DecodeChunk(first, count, chunk, out, outSamples)` | The same, with that call's packet bytes (the Stream worker's `Block::packets`), read only during the call. |
| `TotalSamples()`, `SeekSamples(k)` | The seek table in samples: `table[last] / (2 * channels)`, and the entry before packet k. |
| `FrameLength()`, `NextPacket()`, `PendingSamples()`, `MaxSamplesFor(n)`, `Failed()`, `Close()` | |

Decode at load is one call: `DecodePackets(0, PacketCount(), pcm, TotalSamples())`. A pass from
packet 0 yields `TotalSamples()` samples. A pass from packet k yields
`TotalSamples() - SeekSamples(k) - FrameLength()` samples, which are the tail of the packet-0 pass.

## The fixture and the measured error

(2026-10-17: both fixture files were removed from the tree as game content; the test now reads
the run from the user's install and pins a fingerprint of the proven decode -- `tests/unit/data/README.md`.)

- `tests/unit/data/xwma_fixture.bin` (13,452 B) is `Audio\Animal\Sheep1.wav` exactly as stored: the
  `SoundBuffer::PlatformSerialize` byte run cut from `WinData/data0.pkg` at 0x2d802. SoundBuffers
  sit uncompressed in the package, so a scan for a 0x0161 WAVEFORMATEX plus the two counts in front
  of the table finds them; no package reader was needed.
- `xwma_fixture.pcm` (131,072 B) is the same packets decoded once by **XAudio2 2.7**, the engine
  retail uses. The scratch program used one source voice and one buffer with its
  `XAUDIO2_BUFFER_WMA` table. A pass-through XAPO on the source voice captured the decoded float, and
  the mastering voice ran at 44.1 kHz mono, so there was no resampling. The floats were converted
  with `rint(x * 32768)`. `tests/unit/data/README.md` has the layout and the sha256s.
- **Measured** on S16 against the fixture: max |diff| 1 LSB, RMS 0.025 LSB, 99.94 % identical.
  In float the max is 1.53e-5, half an S16 step. The test pins max 2, RMS 0.05 and >= 99.5 %
  identical, a 2x margin. A one-sample slip fails all three by orders of magnitude.
- **Measured in scratch, not pinned** (the references are megabytes), with the same 1.53e-5 bound:
  - `Crowd.wav` (stereo 44.1 kHz) and `Forest.wav` (stereo 48 kHz, 1008-byte packets), over every
    sample. With AUDIO-A's census these cover all three format classes the game loads.
  - Passes started at packets 3, 10, 31 and 50.
  - XAudio2 decoding the Stream worker's 16-packet buffers on one voice: bit-identical to one buffer.
- **Two shipped sounds end differently** (EXTRACT.md): `Tornado.wav` and `Hoe2.wav`. Open item A4.4
  below.

## Deviations from the brief

1. **"Samples" means frames** throughout the API: `outSamples` is a frame count and `out` holds
   `outSamples * Channels()` int16 values. That is XAudio2's unit and the seek table's.
2. **`DecodeChunk` was added** (its own commit). The Stream worker never holds a whole track: it
   reads 16 packets into a reused `Block`. So `Open` also accepts the table and format alone.
3. **The reference PCM is 128 KB**, not "a few KB". The .bin is 13 KB; a reference has to be full
   length to pin the start and end rules. There is one fixture, the mono one; the stereo and 48 kHz
   checks are scratch measurements recorded in EXTRACT.md.
4. **The glue decodes the frame left in the bit reservoir at end of stream** (`wmav2_drain`'s
   terminator superframe), which FFmpeg's own EOF path does not do. Without it the last 2048
   samples differ from XAudio2 (up to 0.039). No upstream line is changed.
5. **The library is `-O2` in the debug exe too**, as SDL3 is.
6. **S16 output, as specified.** The decoder's native output is float, and SDL3 takes F32 as easily
   as S16. A float variant of `Append` is a few lines if A4 prefers it.

## Open items for A4 / A5

**A4, one-shot voices (decode at load)**

1. **Where to decode.** `SoundBuffer::Serialize` (`src/engine/audio/vtbatch7_soundbuffer_soundeffect.cpp`),
   on the read path, after `PlatformSerialize` fills `_data` / `_length`, `_wmaData` / `_wmaLength`
   and `_format`. That is where AUDIO-A's census hook sits (`if (IsReading(ser._stream))`). Only
   non-streamed buffers (`_streamed == 0`) have packets; a streamed one holds the table and
   `_streamPath`. SoundBuffers load on the loader thread as well as the main thread; one
   `XwmaDecoder` per call is safe, and the shared tables are built under a real once-init.
2. **Storage.** Q5 says the PCM replaces the payload. If it goes into `_data`, allocate it with
   `rtcoll::MemAlloc`, because `~SoundBuffer` frees `_data` and `_wmaData` with `MemFree`
   (`batchc_audio_leaves.cpp:136-139`). Then rewrite `_format` to PCM: tag 1, `blockAlign`
   2 x channels, `avgBytesPerSec` to match.
   - The XAudio2 backend consumes xWMA until Block 5 deletes it. So either switch both together, or
     keep the PCM beside the xWMA until then.
   - **Memory:** the 25 SoundBuffers found in the clear are 1.87 MB of xWMA and 44.4 MB of PCM
     (23.7x). Rain.wav alone is 9.9 MB.
3. **PlayBegin.** `ComputePlayPosition` (`src/engine/audio/audio_play_platform.cpp:266-306`) sets
   `playBegin = nBlockAlign * idx`, in samples, where idx is the first packet whose cumulative
   decoded-byte entry exceeds `bytePos`. Retail mixes the units. Measured against XAudio2 2.7:
   - Let k be the packet whose table range holds `playBegin`, in samples:
     `SeekSamples(k) <= playBegin < SeekSamples(k + 1)`.
   - XAudio2 decodes from packet k fresh and starts at sample `playBegin` of the packet-0 PCM when
     k = 0, or `playBegin + FrameLength()` when k > 0.
   - It plays to the end, so the length is `TotalSamples() - start`.
   - Exact (0.0 difference) for Sheep1 at playBegin 6690 (k = 0) and Crowd at 22300 (k = 1).
   - With the PCM in hand, A4 starts the SDL stream at that offset.
4. **Two shipped sounds end differently:**
   - `Tornado.wav`: FFmpeg rejects packets 194-199, so the PCM is 1,177,600 samples, not the
     table's 1,212,928. XAudio2 turns those packets into about 5,000 frames of full-scale noise
     (peak 1.0) and plays 1,183,232. Retail plays that burst at the end of every tornado loop; ours
     will not. Size buffers by `TotalSamples()`, use the returned count, and do not treat
     `Failed()` as fatal. **Suggest a `docs/known_port_issues.md` row when A4 lands** (coordinator).
   - `Hoe2.wav`: the last 2048 samples differ by up to 12 S16 steps in a near-silent tail.
5. **Looping one-shots** (LoopCount 255 since `586ea9f5e`). A single buffer looped by XAudio2
   replays the whole buffer exactly: LoopCount 1 gave 2 x TotalSamples, and pass 2 was bit-identical
   to pass 1 for Sheep1 and Crowd. So a PCM loop restarts at sample 0. A loop combined with a
   non-zero PlayBegin was not measured.

**A5, the music stream**

6. **Where to decode.** `Audio::Stream::Run` (`src/platform/win/vtbatch7b_stream_run_win.cpp`), on
   the Stream worker:
   - `curSeg` is the first packet whose entry exceeds `_bytePosition` (the loop at about
     `:247-255`).
   - Each pass reads `nPackets * 0x8b6` bytes into `_buffers._data[slotIndex].packets`, rebases the
     table into `cumulativeBytes` (`:328-331`), and submits with `Platform::AudioVoiceSubmit`
     (`:372`).
   - A5 replaces the submit with `DecodeChunk(curSeg, nPackets, block->packets, pcm, cap)` and
     pushes the PCM. `cap = MaxSamplesFor(16)`; one decoder per Stream; `Open(format, nullptr, 0,
     buf->_wmaData, segCount)` per track.
   - `kPacketBytes` is a hard 0x8b6, which every streamed format in the census matches.
7. **Start at `curSeg > 0`** (a resumed track) is a fresh pass. It yields
   `TotalSamples() - SeekSamples(curSeg) - FrameLength()` samples, which is what a fresh XAudio2
   voice plays from there (measured at four start packets).
8. **The loop seam.** Run wraps `curSeg` to 0 and keeps submitting on the same voice.
   - XAudio2 restarts its decoder at the wrap: pass 2 equals a fresh decode. But it **drops the
     first 512 samples of pass 2 for Crowd and Forest** (exact match from sample 512 on), and none
     for Sheep1.
   - The three fit "drop FrameLength minus the decoder's slack past the table total", but that is
     three data points, not a rule. Measure on the music tracks before copying it.
   - With this API a wrap is `DecodeChunk(0, ...)`, which starts a fresh pass. It does not drop
     those 512.
9. **What was not measured:** XAudio2's behaviour on a buffer whose PlayBegin is not packet-aligned
   in the table's sense (the A4 rule above covers retail's values), and the music files themselves
   (they are read from `_streamPath`, not from the package scan).

## Shared files (merge with care)

- `Makefile`: the WMADEC block after the SDL3 `third_party` rule. `$(WMADEC_LIB)` was added as a
  prerequisite of `standalone`, `standalone-debug`, `standalone-og`, `standalone-perf`,
  `$(SA_TARGET)` and `unit.exe`, and to `SA_LIBS` / `SA_INCLUDES`. The unit.exe link line gained
  `$(WMADEC_LIB)`.
- `CMakeLists.txt`: `add_subdirectory(third_party/wmadec ...)` after SDL's. `target_link_libraries`
  gained `wmadec` for `OpenBA` and `unit`. The "This project compiles no C" comment was reworded.
- `tests/unit/sources.mk` (one family appended at the end) and the regenerated `sources.cmake`.
- New only: `third_party/wmadec/**`, `src/engine/audio/xwma_decode.{h,cpp}`,
  `tests/unit/xwma_decode_test.cpp`, `tests/unit/data/**`.
