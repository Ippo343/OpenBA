# tests/unit/data — binary fixtures

## xwma_fixture.bin / xwma_fixture.pcm (tests/unit/xwma_decode_test.cpp)

`xwma_fixture.bin` (13,452 bytes, sha256 `30da65356d18a1e0c9c0c92a208da2bfd03fc60c61946003de3d8980496fea4c`)
is the game's `Audio\Animal\Sheep1.wav` SoundBuffer exactly as stored: the byte run
`Audio::SoundBuffer::PlatformSerialize` reads (src/engine/audio/vtbatch7_soundbuffer_soundeffect.cpp),
cut from `WinData/data0.pkg` at file offset 0x2d802 (the shipped GOG package). The resource
stores it uncompressed, so the run was found by scanning the package for a `WAVEFORMATEX`
with wFormatTag 0x0161 and checking the two counts in front of the table:

| offset | bytes | field |
|---|---|---|
| 0 | 4 | int32 `_length` = 13380 (6 packets x 2230) |
| 4 | 4 | int32 `_wmaLength` = 24 (the seek table's byte length) |
| 8 | 24 | uint32 x 6 cumulative decoded bytes: 0x6000 0xb000 0xf000 0x15000 0x1a000 0x20000 |
| 32 | 0x28 | the format window: 0x0161, 1 ch, 44100 Hz, 6000 B/s, nBlockAlign 2230, 16 bit, cbSize 0 |
| 72 | 13380 | the xWMA packets |

`xwma_fixture.pcm` (131,072 bytes, sha256 `2ea387237471837b473ebb36a61c1356ff56d76be34cafbb29cd05c15ef48519`)
is the same packets decoded once by XAudio2 2.7 on Windows 10 (2026-10-10): one source voice
created with the fixture's format, one `XAUDIO2_BUFFER` with its `XAUDIO2_BUFFER_WMA` table, a
pass-through in-place XAPO on the source voice capturing the decoded float, the mastering voice
created at 44.1 kHz mono (no sample-rate conversion). SamplesPlayed was 65536, the table's
total. The 65536 floats were converted to little-endian S16 by `rint(x * 32768)`, saturated.
The capture program was a scratch tool (not kept); EXTRACT.md in third_party/wmadec records
the method and what it measured on two more shipped sounds.

Both are binary (`.gitattributes` here): never line-ending converted.
