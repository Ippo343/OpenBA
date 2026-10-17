# tests/unit/data — fixtures

No game bytes are stored here (CLAUDE.md: no game content in the tree). What the tests
need from the game they read from the user's install at test time, and skip, visibly,
when there is none.

## resource_names.inc (tests/unit/filename_adjust_test.cpp)

A generated list of every `<path>.rsc[:id]` / `.crs` string literal our own sources
mention: resource NAMES, from `src/`, not resource contents.

## The xWMA fingerprint (tests/unit/xwma_decode_test.cpp)

Until 2026-10-17 this directory held `xwma_fixture.bin` (the game's `Audio\Animal\Sheep1.wav`
SoundBuffer as stored, 13,452 bytes cut from `WinData/data0.pkg`) and `xwma_fixture.pcm`
(its XAudio2 2.7 decode, 131,072 bytes). Both were game content and were removed from the
tree (they remain in git history before that date; history was not rewritten). What
replaced them:

- **The run is read from the install.** `xwma_decode_test.cpp` opens `WinData/data0.pkg`
  under `$BANISHED_DIR`, `C:/GOG Games/Banished` or `/mnt/c/GOG Games/Banished`, reads
  13,452 bytes at file offset 0x2d802 and accepts them when their sha256 is
  `30da65356d18a1e0c9c0c92a208da2bfd03fc60c61946003de3d8980496fea4c` (the shipped GOG
  package). Otherwise it scans the package for the run (a `WAVEFORMATEX` with wFormatTag
  0x0161 preceded by the two counts, the shape below). Without an install, or without the
  run, every xwma_decode test is a `UNIT_SKIP`.
- **The reference is a fingerprint of our decoder, `kFingerprint` in the test.** The
  decoder was proven against the XAudio2 capture on 2026-10-10 (max |diff| 1 LSB, RMS
  0.025 LSB, 99.94 % of samples identical; the capture: one source voice, one
  `XAUDIO2_BUFFER` with its `XAUDIO2_BUFFER_WMA` table, a pass-through in-place XAPO on
  the source voice capturing the decoded float, the mastering voice at 44.1 kHz mono, no
  sample-rate conversion, floats converted by `rint(x * 32768)` saturated; the method and
  two more shipped sounds are in third_party/wmadec/EXTRACT.md). On 2026-10-17, with that
  comparison passing, the decoder's output was reduced to 64 per-1024-sample blocks of
  (RMS rounded to int, peak |sample|), plus the sample count (65536) and the global peak
  (12966 at sample 16369), and those numbers were committed as the pin. The live decode
  must match every block within +-2 (LSB-level decoder differences); a timing slip or a
  wrong frame drop moves whole blocks by orders of magnitude. The fingerprint cannot
  reconstruct the sound.

### The run's layout (a format, not content)

The byte run `Audio::SoundBuffer::PlatformSerialize` reads
(src/engine/audio/vtbatch7_soundbuffer_soundeffect.cpp); the resource stores it
uncompressed:

| offset | bytes | field |
|---|---|---|
| 0 | 4 | int32 `_length` = 13380 (6 packets x 2230) |
| 4 | 4 | int32 `_wmaLength` = 24 (the seek table's byte length) |
| 8 | 24 | uint32 x 6 cumulative decoded bytes: 0x6000 0xb000 0xf000 0x15000 0x1a000 0x20000 |
| 32 | 0x28 | the format window: 0x0161, 1 ch, 44100 Hz, 6000 B/s, nBlockAlign 2230, 16 bit, cbSize 0 |
| 72 | 13380 | the xWMA packets |
